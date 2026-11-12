# TeXstudio 字数统计插件 / TeXstudio Word Count Plugin

[中文](#中文) · [English](#english)

## 中文

### 兼容性

此插件已在官方 Windows x64 TeXstudio 4.9.8（`git_00f7c1c3d`，Qt 6.11.2）上验证。插件使用编辑器的内部接口，其他 TeXstudio 构建即使使用同一 Qt 版本，也需要单独验证。

### 安装与升级

1. 保存文档并完全退出 TeXstudio。
2. 将安装 ZIP 解压到一个文件夹，在解压目录中双击 `Install.cmd`。如果 Windows 弹出管理员权限提示，确认后继续。
3. 用原来的桌面、开始菜单或任务栏快捷方式启动 TeXstudio，打开文档并选中文字。计数会出现在 `Line / Column / Selected / INSERT` 同一行，例如 `English 3 · 汉字 4 · Total 7`。

安装器会从 Windows 卸载记录查找 TeXstudio 的安装目录，在该目录下写入 `generic\texstudio-wordcount.dll`，并在需要时为匹配的现有快捷方式追加 `-plugin texstudio-wordcount`。这些操作会写入解压目录之外的 Windows 安装目录和快捷方式。TeXstudio 继续使用原有设置和图标。

已安装本项目早期发布版本的用户可以直接运行新版 `Install.cmd`。安装器通过文件哈希识别已知旧版，发现来源未知的同名 DLL 时会停止。升级成功后会清理用于替换的临时文件和旧 DLL 备份。

如果自动查找不到唯一的安装目录，或找不到现有快捷方式，可以在解压目录中打开有相应写入权限的 PowerShell，手动指定这两个路径。将示例路径替换为实际路径。

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\setup.ps1 -Action Install -InstallPath 'C:\Path\To\TeXstudio' -ShortcutPath 'C:\Path\To\TeXstudio.lnk'
```

### 编辑器底部没有计数

完全退出 TeXstudio，再用原有快捷方式启动并打开文档。没有选区时显示 `English 0 · 汉字 0 · Total 0`。如果编辑区域较窄，拉宽窗口或编辑区域以留出显示空间。如果行号与列号整行都没有显示，请在 TeXstudio 设置中启用编辑器光标状态行。

直接双击 `.tex` 文件可能会启动一个没有加载插件的 TeXstudio 实例。可以先用快捷方式启动 TeXstudio，再从程序中打开文档。也可以在 Windows 的「运行」窗口输入下面的命令，将路径换成实际安装位置。

```text
"C:\Program Files\TeXstudio\texstudio.exe" -plugin texstudio-wordcount
```

### 卸载与许可证

关闭 TeXstudio，双击解压目录中的 `Uninstall.cmd`。卸载脚本会删除识别到的插件 DLL，并从匹配的快捷方式移除插件启动参数。如果安装时手动指定了路径，可用同一条 PowerShell 命令和相同路径，将 `-Action Install` 改为 `-Action Uninstall`。

安装包附有 GPL-2.0-or-later 的 `COPYING` 和汉字码点数据的 `LICENSES/Unicode-3.0.txt`。

## English

### Compatibility

The plugin was verified with the official Windows x64 build of TeXstudio 4.9.8 (`git_00f7c1c3d`, Qt 6.11.2). It uses internal editor interfaces. Other TeXstudio builds require separate validation, even with the same Qt version.

### Install and upgrade

1. Save your documents and exit TeXstudio completely.
2. Extract the installation ZIP to a folder and double-click `Install.cmd` in that folder. Approve the Windows administrator prompt if it appears.
3. Start TeXstudio from your existing desktop, Start menu, or taskbar shortcut. Open a document and select some text. The count appears on the same row as `Line / Column / Selected / INSERT`, for example `English 3 · 汉字 4 · Total 7`.

The installer finds TeXstudio through the Windows uninstall registry entries. It writes `generic\texstudio-wordcount.dll` under the TeXstudio installation directory and adds `-plugin texstudio-wordcount` to matching existing shortcuts when needed. These changes affect the Windows installation directory and shortcuts outside the extracted folder. TeXstudio keeps its existing settings and icons.

To upgrade an earlier release of this plugin, run the newer `Install.cmd`. The installer recognizes known previous versions by file hash and stops if it finds a DLL of the same name from an unknown source. After a successful upgrade, it removes the temporary file and backup of the replaced DLL.

If automatic discovery finds no unique installation or no matching shortcut, open PowerShell in the extracted folder with write access to both locations. Replace the example paths with your installation and shortcut paths.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\setup.ps1 -Action Install -InstallPath 'C:\Path\To\TeXstudio' -ShortcutPath 'C:\Path\To\TeXstudio.lnk'
```

### If the count does not appear

Exit TeXstudio completely, reopen it from an existing shortcut, and open a document. With no text selected, the row displays `English 0 · 汉字 0 · Total 0`. If the editor is narrow, widen the window or editor area. If the entire line and column row is missing, enable the editor cursor status row in TeXstudio's settings.

Double-clicking a `.tex` file may start an instance without the plugin. Start TeXstudio from a shortcut first, then open the document from within the editor. You can also use the following command in Windows Run. Replace the example path with your installation path.

```text
"C:\Program Files\TeXstudio\texstudio.exe" -plugin texstudio-wordcount
```

### Uninstall and licenses

Exit TeXstudio and double-click `Uninstall.cmd` in the extracted folder. The script removes the recognized plugin DLL and its launch argument from matching shortcuts. If you specified the paths manually during installation, run the same PowerShell command with the same paths and change `-Action Install` to `-Action Uninstall`.

The package includes `COPYING` for the GPL-2.0-or-later license and `LICENSES/Unicode-3.0.txt` for the Han code point data.
