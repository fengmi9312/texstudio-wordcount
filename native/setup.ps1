# SPDX-License-Identifier: GPL-2.0-or-later
[CmdletBinding()]
param(
    [ValidateSet('Install', 'Uninstall')][string]$Action = 'Install',
    [string]$InstallPath,
    [string[]]$ShortcutPath,
    [switch]$Interactive
)

$ErrorActionPreference = 'Stop'
$pluginFile = 'texstudio-wordcount.dll'
$pluginArgument = '-plugin texstudio-wordcount'
$pluginSuffix = ' ' + $pluginArgument
$sourceDll = Join-Path $PSScriptRoot $pluginFile
$previousReleaseHashes = @(
    '8084519751C05210AD2B0FC12FD843505251B5C66B41049DCEF9BD1A25CDEFDF'
    '9BC7C94CFC77501AB75D877F117FFEB264F1F44CD721CF24049ED18120D14947'
)

function Assert-RegularPath([string]$Path) {
    $item = Get-Item -LiteralPath $Path -Force
    while ($null -ne $item) {
        if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "链接路径需要先手动处理：$($item.FullName)"
        }
        if ($item -is [IO.DirectoryInfo]) { $item = $item.Parent }
        else { $item = $item.Directory }
    }
}

function Find-InstallPath {
    $uninstallKey = 'Microsoft\Windows\CurrentVersion\Uninstall\TeXstudio'
    $keys = @("HKLM:\SOFTWARE\$uninstallKey", "HKLM:\SOFTWARE\WOW6432Node\$uninstallKey", "HKCU:\SOFTWARE\$uninstallKey")
    $paths = foreach ($key in $keys) {
        $entry = Get-ItemProperty -LiteralPath $key -Name InstallLocation -ErrorAction SilentlyContinue
        if (!$entry) { continue }
        $directory = $entry.InstallLocation.Trim().Trim('"')
        if ($directory -and (Test-Path -LiteralPath (Join-Path $directory 'texstudio.exe') -PathType Leaf)) {
            (Get-Item -LiteralPath $directory).FullName
        }
    }
    $paths = @($paths | Sort-Object -Unique)
    if ($paths.Count -ne 1) {
        throw '未找到唯一的 TeXstudio 安装。请用 -InstallPath 指定安装目录。'
    }
    return $paths[0]
}

function Find-Shortcuts {
    $directories = @(
        [Environment]::GetFolderPath('DesktopDirectory'),
        [Environment]::GetFolderPath('CommonDesktopDirectory'),
        [Environment]::GetFolderPath('Programs'),
        [Environment]::GetFolderPath('CommonPrograms'),
        (Join-Path $env:APPDATA 'Microsoft\Internet Explorer\Quick Launch\User Pinned\TaskBar')
    )
    foreach ($directory in $directories) {
        if ($directory -and (Test-Path -LiteralPath $directory -PathType Container)) {
            Get-ChildItem -LiteralPath $directory -Filter '*TeXstudio*.lnk' -File -Recurse -ErrorAction SilentlyContinue |
                Select-Object -ExpandProperty FullName
        }
    }
}

try {
    if ($Interactive) {
        if ($InstallPath -or $ShortcutPath) { throw '交互模式会自动查找安装和快捷方式。' }
        $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
        $principal = [Security.Principal.WindowsPrincipal]::new($identity)
        if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
            $arguments = '-NoProfile -ExecutionPolicy Bypass -File "{0}" -Action {1} -Interactive' -f $PSCommandPath, $Action
            $elevated = Start-Process -FilePath (Join-Path $PSHOME 'powershell.exe') -ArgumentList $arguments -Verb RunAs -Wait -PassThru
            exit $elevated.ExitCode
        }
    }

    if (!$InstallPath) { $InstallPath = Find-InstallPath }
    $InstallPath = (Get-Item -LiteralPath $InstallPath -Force).FullName
    Assert-RegularPath $InstallPath
    $exe = Join-Path $InstallPath 'texstudio.exe'
    Assert-RegularPath $exe
    $appVersion = (Get-Item -LiteralPath $exe).VersionInfo
    if ($appVersion.ProductName -ne 'TeXstudio') { throw "目录中没有 TeXstudio：$InstallPath" }
    if ($Action -eq 'Install') {
        $qt = Join-Path $InstallPath 'Qt6Core.dll'
        Assert-RegularPath $qt
        $qtVersion = (Get-Item -LiteralPath $qt).VersionInfo.FileVersion
        if ($appVersion.FileVersion -notmatch '^4\.9\.8(?:\D|$)' -or $qtVersion -ne '6.11.2.0') {
            throw '此扩展包适用于 Windows x64 的 TeXstudio 4.9.8（Qt 6.11.2）。'
        }
        # IMAGE_FILE_MACHINE_AMD64 = 0x8664
        $reader = [IO.BinaryReader]::new([IO.File]::OpenRead($exe))
        try {
            $reader.BaseStream.Position = 0x3c
            $header = $reader.ReadUInt32()
            $reader.BaseStream.Position = $header
            if ($reader.ReadUInt32() -ne 0x4550 -or $reader.ReadUInt16() -ne 0x8664) {
                throw '此扩展包需要 x64 版本的 TeXstudio。'
            }
        } finally {
            $reader.Dispose()
        }
    }
    foreach ($process in (Get-Process -Name texstudio -ErrorAction SilentlyContinue)) {
        if (!$process.Path -or $process.Path -ieq $exe) { throw '请先关闭 TeXstudio，再重新运行。' }
    }

    Assert-RegularPath $sourceDll
    $hash = (Get-FileHash -LiteralPath $sourceDll -Algorithm SHA256).Hash
    $generic = Join-Path $InstallPath 'generic'
    if (Test-Path -LiteralPath $generic) {
        Assert-RegularPath $generic
        if (!(Test-Path -LiteralPath $generic -PathType Container)) { throw "无法使用插件目录：$generic" }
    }
    $installedDll = Join-Path $generic $pluginFile
    $alreadyInstalled = Test-Path -LiteralPath $installedDll
    $installedHash = $null
    if ($alreadyInstalled) {
        Assert-RegularPath $installedDll
        $installedHash = (Get-FileHash -LiteralPath $installedDll -Algorithm SHA256).Hash
        if ($installedHash -ne $hash -and $installedHash -notin $previousReleaseHashes) {
            throw "发现来源未知的同名插件：$installedDll"
        }
    }

    if (!$ShortcutPath) { $ShortcutPath = @(Find-Shortcuts) }
    $shell = New-Object -ComObject WScript.Shell
    $matched = 0
    $changes = foreach ($file in ($ShortcutPath | Sort-Object -Unique)) {
        Assert-RegularPath $file
        $path = (Get-Item -LiteralPath $file).FullName
        if ([IO.Path]::GetExtension($path) -ine '.lnk') { throw "无效的快捷方式：$path" }
        $shortcut = $shell.CreateShortcut($path)
        if ($shortcut.TargetPath -ine $exe) { continue }
        ++$matched
        $before = [string]$shortcut.Arguments
        # The installer owns only the exact argument it appends. A quoted
        # document path may itself contain the words "-plugin texstudio-wordcount".
        $matchesSuffix = $before.EndsWith($pluginSuffix, [StringComparison]::OrdinalIgnoreCase)
        $hasPluginArgument = $before -ieq $pluginArgument -or $matchesSuffix
        if ($Action -eq 'Install') {
            if ($hasPluginArgument) { continue }
            $after = if ($before) { $before + $pluginSuffix } else { $pluginArgument }
        } else {
            if (!$hasPluginArgument) { continue }
            $after = if ($matchesSuffix) { $before.Substring(0, $before.Length - $pluginSuffix.Length) } else { '' }
        }
        [pscustomobject]@{
            Path = $path
            Link = $shortcut
            Before = [IO.File]::ReadAllBytes($path)
            After = $after
            Icon = $shortcut.IconLocation
        }
    }
    if ($Action -eq 'Install' -and $matched -eq 0) {
        throw '没有找到指向这个 TeXstudio 的快捷方式。请用 -ShortcutPath 指定原快捷方式。'
    }

    $modified = [Collections.Generic.List[object]]::new()
    $addedDll = $false
    $replacedDll = $false
    $previousDll = $null
    $temporary = $null
    try {
        if ($Action -eq 'Install' -and $installedHash -ne $hash) {
            [void][IO.Directory]::CreateDirectory($generic)
            $temporary = Join-Path $generic ([Guid]::NewGuid().ToString() + '.tmp')
            [IO.File]::Copy($sourceDll, $temporary)
            if ((Get-FileHash -LiteralPath $temporary -Algorithm SHA256).Hash -ne $hash) {
                throw '复制后的插件校验失败。'
            }
            if ($alreadyInstalled) {
                $previousDll = Join-Path $generic ([Guid]::NewGuid().ToString() + '.bak')
                [IO.File]::Replace($temporary, $installedDll, $previousDll)
                $replacedDll = $true
            } else {
                [IO.File]::Move($temporary, $installedDll)
                $addedDll = $true
            }
        }
        foreach ($change in $changes) {
            $modified.Add($change)
            $change.Link.Arguments = $change.After
            $change.Link.Save()
            $saved = $shell.CreateShortcut($change.Path)
            if ($saved.TargetPath -ine $exe -or $saved.Arguments -cne $change.After -or
                $saved.IconLocation -cne $change.Icon) {
                throw "快捷方式验证失败：$($change.Path)"
            }
            Write-Host $change.Path
        }
        if ($Action -eq 'Uninstall' -and $alreadyInstalled) {
            Remove-Item -LiteralPath $installedDll
        }
    } catch {
        foreach ($change in $modified) {
            try { [IO.File]::WriteAllBytes($change.Path, $change.Before) }
            catch { Write-Warning "未能恢复快捷方式：$($change.Path)" }
        }
        if ($replacedDll) {
            try { [IO.File]::Replace($previousDll, $installedDll, $temporary) }
            catch { Write-Warning "未能恢复旧插件，备份保留在：$previousDll" }
        } elseif ($addedDll) {
            Remove-Item -LiteralPath $installedDll -ErrorAction SilentlyContinue
        }
        throw
    } finally {
        if ($temporary -and (Test-Path -LiteralPath $temporary)) {
            Remove-Item -LiteralPath $temporary -ErrorAction SilentlyContinue
        }
    }

    if ($replacedDll) {
        try { Remove-Item -LiteralPath $previousDll }
        catch { Write-Warning "无法删除旧插件备份：$previousDll" }
    }
    if ($Action -eq 'Install') { Write-Host '扩展安装完成。请用原来的 TeXstudio 图标启动。' }
    else { Write-Host '扩展已卸载。' }
    exit 0
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
