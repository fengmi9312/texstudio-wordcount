# 安装 TeXstudio 字数统计插件

此插件已在官方 Windows x64 TeXstudio 4.9.8（`git_00f7c1c3d`，Qt 6.11.2）上验证。插件使用编辑器的内部接口，其他 TeXstudio 构建即使使用同一 Qt 版本，也需要单独验证。

1. 保存文档并完全退出 TeXstudio。
2. 将安装 ZIP 解压到一个文件夹，在解压目录中双击 `Install.cmd`。如果 Windows 弹出管理员权限提示，确认后继续。
3. 用原来的桌面、开始菜单或任务栏快捷方式启动 TeXstudio，打开文档并选中文字。计数会出现在 `Line / Column / Selected / INSERT` 同一行，例如 `English 3 · 汉字 4 · Total 7`。

安装器会从 Windows 卸载记录查找 TeXstudio 的安装目录，在该目录下写入 `generic\texstudio-wordcount.dll`，并在需要时为匹配的现有快捷方式追加 `-plugin texstudio-wordcount`。TeXstudio 继续使用原有设置和图标。

已安装本项目早期发布版本的用户可以直接运行新版 `Install.cmd`。安装器通过文件哈希识别已知旧版，并在升级成功后清理用于替换的临时文件和旧 DLL 备份。

如果自动查找不到唯一的安装目录，或找不到现有快捷方式，可以在解压目录中打开有相应写入权限的 PowerShell，手动指定这两个路径。将下面的示例路径替换为实际路径。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\setup.ps1 -Action Install -InstallPath 'C:\Path\To\TeXstudio' -ShortcutPath 'C:\Path\To\TeXstudio.lnk'
```

## 编辑器底部没有计数

完全退出 TeXstudio，再用原有快捷方式启动并打开文档。没有选区时显示 `English 0 · 汉字 0 · Total 0`。如果编辑区域较窄，拉宽窗口或编辑区域即可为标签留出空间。

直接双击 `.tex` 文件可能会启动一个没有加载插件的 TeXstudio 实例。可以先用快捷方式启动 TeXstudio，再从程序中打开文档。也可以在 Windows 的「运行」窗口中输入下面的命令，将路径替换为实际安装位置。

```text
"C:\Program Files\TeXstudio\texstudio.exe" -plugin texstudio-wordcount
```

## 卸载

关闭 TeXstudio，双击解压目录中的 `Uninstall.cmd`。卸载脚本会删除识别到的插件 DLL，并从匹配的快捷方式移除插件启动参数。

安装包附有 GPL-2.0-or-later 的 `COPYING` 和汉字码点数据的 `LICENSES/Unicode-3.0.txt`。
