# TeXstudio 编辑器底部字数统计

在 TeXstudio 中用鼠标或键盘选中 LaTeX 文本后，编辑器下方的行号、列号和选区长度那一行会实时显示 `English 3 · 汉字 4 · Total 7`。扩展以独立的 Qt 通用插件 DLL 加载，继续使用原版 TeXstudio 和已有配置。

已验证的宿主是官方 Windows x64 版 **TeXstudio 4.9.8**（`git_00f7c1c3d`，Qt **6.11.2**）。插件使用该版本编辑器的内部接口。其他 TeXstudio 或 Qt 版本、第三方构建、Windows ARM64、Linux 和 macOS 尚未验证。 [TeXstudio 4.9.8 官方发行页](https://github.com/texstudio-org/texstudio/releases/tag/4.9.8)

## 安装

安装包发布后，可从本仓库的 **Releases** 页面下载 Windows x64 ZIP，并在同一发布页下载对应版本的源码。解压安装包，运行其中的 `Install.cmd`，然后用原来的 TeXstudio 快捷方式启动。也可以按下文从源码构建。 [安装、升级与卸载说明](native/README.md)

## 统计范围

统计针对活动编辑器的主选区。英文按词计，简体、繁体和扩展区汉字逐字计，总数为二者相加。解析器从 LaTeX 源码识别公式、注释、引用键、标签和常见元数据，保留常见格式命令、标题和脚注中的正文。

自定义宏不会执行展开，`\input` 和 `\include` 指向的文件不会并入当前选区。选区变化会查询当前文档的缓存索引，编辑后在后台重建索引。独立解析器通过 `WordCountRules` 提供扩展接口，插件目前使用内置规则。

## 构建

已验证的构建组合为 WinLibs GCC **16.2.0**（x86_64、POSIX、SEH、UCRT）、CMake **4.4.2**、Ninja **1.13.2**，以及 MSYS2 UCRT64 的 `mingw-w64-ucrt-x86_64-qt6-base` **6.11.2-2**。从单独的归档包准备 Qt SDK 时，同一 UCRT64 前缀还需要 PCRE2 和 Vulkan headers。插件与宿主需使用兼容的 Qt 版本、架构和编译器 ABI。 [WinLibs 工具链](https://github.com/brechtsanders/winlibs_mingw/releases/tag/16.2.0posix-14.0.0-ucrt-r1) · [MSYS2 UCRT64 包归档](https://repo.msys2.org/mingw/ucrt64/)

在项目根目录运行以下 PowerShell 命令，将工具路径换成本机路径。PATH 中也需有 CMake 和 Ninja。

```powershell
$compiler = 'C:/path/to/mingw64/bin/g++.exe'
$qtSdk = 'C:/path/to/qt-6.11.2/ucrt64'
$env:Path = "$qtSdk/bin;$(Split-Path -Parent $compiler);$env:Path"
cmake -S native -B build/native-plugin -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_COMPILER=$compiler" "-DCMAKE_PREFIX_PATH=$qtSdk"
cmake --build build/native-plugin
```

构建产物为 `build/native-plugin/texstudio-wordcount.dll`。从源码安装时，将 DLL 与 `native/` 下的 `Install.cmd`、`Uninstall.cmd` 和 `setup.ps1` 放在同一文件夹，再按安装说明操作。

## 测试

独立解析器测试需要 C++17 编译器和 CMake。沿用上面的 PowerShell 环境可运行以下命令。

```powershell
cmake -S tests -B build/parser-tests -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_COMPILER=$compiler"
cmake --build build/parser-tests
ctest --test-dir build/parser-tests --output-on-failure
```

界面测试需要将官方 TeXstudio 4.9.8 的完整副本放在项目内，例如 `build/stock-texstudio`。使用 `-InstalledLayout` 时，先确认副本中有 `generic` 目录，缺少时创建空目录。测试会临时向副本放入生产插件和测试插件，并使用项目内的独立配置。测试覆盖标签位置、键盘选区、简繁体和扩展区汉字、标签页切换、编辑后重算，以及系统无障碍文字接口不可用时的统计。

```powershell
cmake -S tests/native-plugin -B build/native-plugin-smoke-build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_CXX_COMPILER=$compiler" "-DCMAKE_PREFIX_PATH=$qtSdk"
cmake --build build/native-plugin-smoke-build
.\tests\native-plugin\run_stock_smoke.ps1 -StockCopy build/stock-texstudio -PluginDll build/native-plugin/texstudio-wordcount.dll -ObserverDll build/native-plugin-smoke-build/texstudio_wordcount_smoke.dll -InstalledLayout
```

再次运行界面测试时，使用 `-RunName` 指定新的结果目录名。

插件源码使用 GPL-2.0-or-later。 [COPYING](COPYING)

汉字码点范围参考 Unicode 17.0 的 `PropList.txt`（© 2025 Unicode, Inc.）。 [Unicode 数据](https://www.unicode.org/Public/17.0.0/ucd/PropList.txt) · [Unicode 许可声明](LICENSES/Unicode-3.0.txt)
