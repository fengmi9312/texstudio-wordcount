# TeXstudio Word Count · TeXstudio 字数统计插件

[简体中文](#简体中文) · [English](#english)

## 简体中文

在 TeXstudio 中用鼠标或键盘选中 LaTeX 文本后，编辑器下方的行号、列号和选区长度那一行会实时显示 `English 3 · 汉字 4 · Total 7`。扩展以独立的 Qt 通用插件 DLL 加载，继续使用原版 TeXstudio 和已有配置。

### 兼容性

已验证的宿主是官方 Windows x64 版 **TeXstudio 4.9.8**（`git_00f7c1c3d`，Qt **6.11.2**）。插件使用该版本编辑器的内部接口。其他 TeXstudio 或 Qt 版本、第三方构建、Windows ARM64、Linux 和 macOS 尚未验证。 [TeXstudio 4.9.8 官方发行页](https://github.com/texstudio-org/texstudio/releases/tag/4.9.8)

### 安装、升级与卸载

1. 从本仓库的 **Releases** 页面下载 Windows x64 安装 ZIP。对应版本的源码也可在同一发布页下载。
2. 保存文档并完全退出 TeXstudio。解压 ZIP，双击 `Install.cmd`。
3. 用原有的 TeXstudio 快捷方式启动，打开文档并选中文字。

安装器将插件 DLL 写入 TeXstudio 安装目录下的 `generic` 文件夹，并在需要时为现有快捷方式追加启动参数。TeXstudio 会继续使用原有设置。安装器能识别的旧版本可直接升级。卸载时先退出 TeXstudio，再运行包内的 `Uninstall.cmd`。 [详细安装说明](native/README.md#中文)

### 统计范围

统计针对活动编辑器的主选区。英文按词计，简体、繁体和扩展区汉字逐字计，总数为二者相加。解析器从 LaTeX 源码识别公式、注释、引用键、标签和常见元数据，保留常见格式命令、标题和脚注中的正文。

自定义宏不会执行展开，`\input` 和 `\include` 指向的文件不会并入当前选区。选区变化会查询当前文档的缓存索引，编辑后在后台重建索引。独立解析器通过 `WordCountRules` 提供扩展接口，插件目前使用内置规则。

### 从源码构建与测试

已验证的构建组合为 WinLibs GCC **16.2.0**（x86_64、POSIX、SEH、UCRT）、CMake **4.4.2**、Ninja **1.13.2**，以及 MSYS2 UCRT64 的 `mingw-w64-ucrt-x86_64-qt6-base` **6.11.2-2**。从单独的归档包准备 Qt SDK 时，同一 UCRT64 前缀还需要 PCRE2 和 Vulkan headers。插件与宿主需使用兼容的 Qt 版本、架构和编译器 ABI。 [WinLibs 工具链](https://github.com/brechtsanders/winlibs_mingw/releases/tag/16.2.0posix-14.0.0-ucrt-r1) · [MSYS2 UCRT64 包归档](https://repo.msys2.org/mingw/ucrt64/)

在项目根目录运行构建命令，将示例工具路径换成本机路径，并确认 CMake 和 Ninja 在 `PATH` 中。生成的 `build/native-plugin/texstudio-wordcount.dll` 与 `native/` 下的 `Install.cmd`、`Uninstall.cmd` 和 `setup.ps1` 放入同一个文件夹后，即可按安装说明操作。 [构建命令](#build-commands)

独立解析器测试需要 C++17 编译器和 CMake，可沿用构建时的 PowerShell 环境。界面测试需要将官方 TeXstudio 4.9.8 的完整副本放在项目内，例如 `build/stock-texstudio`。使用 `-InstalledLayout` 时，先确认副本中有 `generic` 目录，缺少时创建空目录。测试会临时向副本放入插件并使用项目内的独立配置，验证标签位置、键盘选区、简繁体和扩展区汉字、标签页切换、编辑后重算，以及系统无障碍文字接口不可用时的统计。再次运行时，可用 `-RunName` 指定新的结果目录名。 [测试命令](#test-commands)

插件源码使用 GPL-2.0-or-later。汉字码点范围参考 Unicode 17.0 的 `PropList.txt`（© 2025 Unicode, Inc.）。 [插件许可证](COPYING) · [Unicode 数据](https://www.unicode.org/Public/17.0.0/ucd/PropList.txt) · [Unicode 许可声明](LICENSES/Unicode-3.0.txt)

## English

Select LaTeX text with the mouse or keyboard to see a live count beside the line, column, and selection indicators at the bottom of the TeXstudio editor. For example, the label can read `English 3 · 汉字 4 · Total 7`. The extension loads as a separate Qt plugin DLL and works with an existing TeXstudio installation and its settings.

### Compatibility

Tested with the official **Windows x64** build of **TeXstudio 4.9.8** (`git_00f7c1c3d`, Qt **6.11.2**). The plugin uses internal editor interfaces from that build. Other TeXstudio and Qt versions, third-party builds, Windows ARM64, Linux, and macOS have not been tested. [Official TeXstudio 4.9.8 release](https://github.com/texstudio-org/texstudio/releases/tag/4.9.8)

### Install, upgrade, and uninstall

1. Download the Windows x64 ZIP from this repository's **Releases** page. The matching source archive is available on the same release page.
2. Save your documents and close TeXstudio completely. Extract the ZIP and double-click `Install.cmd`.
3. Start TeXstudio from your existing shortcut, open a document, and select some text.

The installer places the plugin DLL in the TeXstudio installation's `generic` directory and adds a launch argument to existing shortcuts when needed. TeXstudio continues to use your existing settings. The installer can upgrade recognized earlier versions of the plugin in place. To uninstall, close TeXstudio and run `Uninstall.cmd` from the extracted package. [Detailed installation guide](native/README.md#english)

### Counting rules

The plugin counts the primary selection in the active editor. It counts English words and individual Han characters, including simplified and traditional characters and those in CJK extension blocks. The total is the sum of both counts. The parser skips common LaTeX math, comments, citation keys, labels, and metadata, while counting text in common formatting commands, headings, and footnotes.

Counting operates on the selected LaTeX source. Custom macros are not expanded, and files referenced by `\input` or `\include` are not included. Selection changes use a cached index of the current document, which is rebuilt in the background after edits. The standalone parser exposes `WordCountRules` for extensions, and the plugin uses the built-in rules.

### Build and test from source

The tested toolchain uses WinLibs GCC **16.2.0** (x86_64, POSIX, SEH, UCRT), CMake **4.4.2**, Ninja **1.13.2**, and MSYS2 UCRT64's `mingw-w64-ucrt-x86_64-qt6-base` **6.11.2-2**. When assembling the Qt SDK from individual archives, include PCRE2 and the Vulkan headers in the same UCRT64 prefix. The plugin and host need compatible Qt versions, architecture, and compiler ABI. [WinLibs toolchain](https://github.com/brechtsanders/winlibs_mingw/releases/tag/16.2.0posix-14.0.0-ucrt-r1) · [MSYS2 UCRT64 package archive](https://repo.msys2.org/mingw/ucrt64/)

Run the build commands in PowerShell from the project root, replace the example paths, and make sure CMake and Ninja are on `PATH`. To install from source, put the resulting `build/native-plugin/texstudio-wordcount.dll` in the same directory as `Install.cmd`, `Uninstall.cmd`, and `setup.ps1` from `native/`, then follow the installation guide. [Build commands](#build-commands)

The parser tests require a C++17 compiler and CMake. Run them in the same PowerShell environment used for the build. The UI test needs a complete copy of the official TeXstudio 4.9.8 installation inside the project, for example `build/stock-texstudio`. With `-InstalledLayout`, create an empty `generic` directory in that copy if needed. The test loads the plugins into the copy with an isolated project-local profile. It checks label placement, keyboard selection, simplified and traditional Han characters and CJK extensions, tab switching, recounting after edits, and counting without the system accessibility text interface. Use `-RunName` to specify a new results directory for subsequent runs. [Test commands](#test-commands)

The plugin source is licensed under GPL-2.0-or-later. Han code point ranges reference Unicode 17.0's `PropList.txt` (© 2025 Unicode, Inc.). [Plugin license](COPYING) · [Unicode data](https://www.unicode.org/Public/17.0.0/ucd/PropList.txt) · [Unicode license notice](LICENSES/Unicode-3.0.txt)

## Build commands

```powershell
$compiler = 'C:/path/to/mingw64/bin/g++.exe'
$qtSdk = 'C:/path/to/qt-6.11.2/ucrt64'
$env:Path = "$qtSdk/bin;$(Split-Path -Parent $compiler);$env:Path"
cmake -S native -B build/native-plugin -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_COMPILER=$compiler" "-DCMAKE_PREFIX_PATH=$qtSdk"
cmake --build build/native-plugin
```

## Test commands

### 解析器测试 · Parser tests

```powershell
cmake -S tests -B build/parser-tests -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_COMPILER=$compiler"
cmake --build build/parser-tests
ctest --test-dir build/parser-tests --output-on-failure
```

### 界面测试 · UI smoke test

```powershell
cmake -S tests/native-plugin -B build/native-plugin-smoke-build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_COMPILER=$compiler" "-DCMAKE_PREFIX_PATH=$qtSdk"
cmake --build build/native-plugin-smoke-build
.\tests\native-plugin\run_stock_smoke.ps1 -StockCopy build/stock-texstudio -PluginDll build/native-plugin/texstudio-wordcount.dll -ObserverDll build/native-plugin-smoke-build/texstudio_wordcount_smoke.dll -InstalledLayout
```
