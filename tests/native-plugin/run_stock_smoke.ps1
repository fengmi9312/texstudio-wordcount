# SPDX-License-Identifier: GPL-2.0-or-later
param(
    [Parameter(Mandatory)]
    [string] $StockCopy,
    [Parameter(Mandatory)]
    [string] $PluginDll,
    [Parameter(Mandatory)]
    [string] $ObserverDll,
    [ValidatePattern('^[A-Za-z0-9][A-Za-z0-9_-]*$')]
    [string] $RunName = 'native-plugin-smoke-run',
    [switch] $DebugPlugins,
    [switch] $InstalledLayout
)
$ErrorActionPreference = 'Stop'
$project = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '../..')).Path.TrimEnd('\')
$build = Join-Path $project 'build'
$run = Join-Path $build $RunName

function Assert-LocalPath([string] $Path) {
    $full = (Resolve-Path -LiteralPath $Path).Path
    if (!$full.StartsWith("$project\", [StringComparison]::OrdinalIgnoreCase)) {
        throw "Outside the project: $full"
    }
    $item = Get-Item -LiteralPath $full -Force
    while ($null -ne $item) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Unexpected filesystem link: $($item.FullName)"
        }
        if ($item.PSIsContainer) { $item = $item.Parent } else { $item = $item.Directory }
    }
    return $full
}

$null = Assert-LocalPath $build
$stock = Assert-LocalPath $StockCopy
if (!(Test-Path -LiteralPath $stock -PathType Container)) {
    throw "StockCopy must be a project-local TeXstudio directory: $stock"
}
$exe = Assert-LocalPath (Join-Path $stock 'texstudio.exe')
$plugin = Assert-LocalPath $PluginDll
$observer = Assert-LocalPath $ObserverDll
if ((Get-Item -LiteralPath $exe).VersionInfo.ProductVersion -notmatch '^4\.9\.8') {
    throw 'The isolated stock executable must be TeXstudio 4.9.8'
}
if (Test-Path -LiteralPath $run) { throw "Smoke output already exists: $run" }

foreach ($name in @('temp', 'profile', 'plugins/generic', 'appdata', 'local-appdata',
                     'xdg-cache', 'xdg-config', 'xdg-data', 'font-cache')) {
    [void][IO.Directory]::CreateDirectory((Join-Path $run $name))
}
Copy-Item -LiteralPath $plugin -Destination (Join-Path $run 'plugins/generic/texstudio-wordcount.dll')
Copy-Item -LiteralPath $observer -Destination (Join-Path $run 'plugins/generic/texstudio-wordcount-smoke.dll')

$utf8 = [Text.UTF8Encoding]::new($false)
$profile = Join-Path $run 'profile'
$ini = @(
    '[texmaker]'
    'Editor\ScanInstalledLatexPackages=false'
    'Startup\CheckLatexConfiguration=false'
    'Update\AutoCheck=false'
    'Grammar\Language%20Tool%20Autorun=false'
) -join [Environment]::NewLine
[IO.File]::WriteAllText((Join-Path $profile 'texstudio.ini'), $ini + [Environment]::NewLine, $utf8)
$first = Join-Path $run 'first.tex'
$second = Join-Path $run 'second.tex'
Copy-Item -LiteralPath (Join-Path $project 'tests/selection-smoke.tex') -Destination $first
$secondContents = 'other ' + [char]0x4e2d + [char]0x6587 + [Environment]::NewLine
[IO.File]::WriteAllText($second, $secondContents, $utf8)

$env:TEMP = Join-Path $run 'temp'
$env:TMP = $env:TEMP
$env:TMPDIR = $env:TEMP
$env:APPDATA = Join-Path $run 'appdata'
$env:LOCALAPPDATA = Join-Path $run 'local-appdata'
$env:XDG_CACHE_HOME = Join-Path $run 'xdg-cache'
$env:XDG_CONFIG_HOME = Join-Path $run 'xdg-config'
$env:XDG_DATA_HOME = Join-Path $run 'xdg-data'
$env:PATH = "$stock;$env:SystemRoot\System32;$env:SystemRoot"
$env:QT_QPA_PLATFORM = 'windows'
$env:QT_QPA_PLATFORM_PLUGIN_PATH = Join-Path $stock 'platforms'
$env:QT_QPA_GENERIC_PLUGINS = ''
if ($InstalledLayout) {
    $env:QT_PLUGIN_PATH = ''
    $env:QT_QPA_PLATFORM_PLUGIN_PATH = ''
} else {
    $env:QT_PLUGIN_PATH = "$(Join-Path $run 'plugins');$stock"
}
$env:QT_FORCE_STDERR_LOGGING = '1'
$env:QML_DISABLE_DISK_CACHE = '1'
$env:QT_QUICK_BACKEND = 'software'
$env:FONTCONFIG_PATH = $run
$env:FONTCONFIG_FILE = Join-Path $run 'fonts.conf'
$env:TEXSTUDIO_WORDCOUNT_GUI_SMOKE = $first
$env:TEXSTUDIO_WORDCOUNT_GUI_SMOKE_SECOND = $second
if ($DebugPlugins) { $env:QT_DEBUG_PLUGINS = '1' }
$fontCache = [Security.SecurityElement]::Escape((Join-Path $run 'font-cache').Replace('\','/'))
$fontConfig = '<?xml version="1.0"?><fontconfig><dir>WINDOWSFONTDIR</dir><cachedir>{0}</cachedir></fontconfig>' -f $fontCache
[IO.File]::WriteAllText($env:FONTCONFIG_FILE, $fontConfig, $utf8)

$stderr = Join-Path $run 'stderr.log'
$start = @{
    FilePath = $exe
    WorkingDirectory = $run
    WindowStyle = 'Hidden'
    ArgumentList = ('-plugin texstudio-wordcount -plugin texstudio-wordcount-smoke --start-always --no-session --config "{0}" "{1}" "{2}"' -f $profile, $first, $second)
    RedirectStandardOutput = Join-Path $run 'stdout.log'
    RedirectStandardError = $stderr
    PassThru = $true
}
$added = [Collections.Generic.List[string]]::new()
$proc = $null
try {
    if ($InstalledLayout) {
        $generic = Assert-LocalPath (Join-Path $stock 'generic')
        foreach ($name in @('texstudio-wordcount.dll', 'texstudio-wordcount-smoke.dll')) {
            $destination = Join-Path $generic $name
            if (Test-Path -LiteralPath $destination) { throw "Stock copy already contains $name." }
            [IO.File]::Move((Join-Path $run "plugins/generic/$name"), $destination)
            $added.Add($destination)
        }
    }
    $proc = Start-Process @start
    if (!$proc.WaitForExit(45000)) { throw 'Native smoke test did not finish within 45 seconds.' }
} finally {
    if ($null -ne $proc -and !$proc.HasExited) { $proc.Kill(); [void]$proc.WaitForExit(3000) }
    foreach ($file in $added) {
        Remove-Item -LiteralPath (Assert-LocalPath $file)
    }
}
Get-Content -LiteralPath $stderr -Tail 30
if ($proc.ExitCode -ne 0 -or !(Select-String -LiteralPath $stderr -SimpleMatch 'native word-count GUI smoke passed' -Quiet)) {
    throw "Native smoke test failed ($($proc.ExitCode)); see $stderr"
}
