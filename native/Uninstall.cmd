@echo off
"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0setup.ps1" -Action Uninstall -Interactive
set "result=%ERRORLEVEL%"
pause
exit /b %result%
