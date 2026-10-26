@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0verify-package.ps1" -Package "%~dp0."
if errorlevel 1 goto failed
set "NAVSTUDIO_NAME=@PACKAGE_NAME@"
"%~dp0%NAVSTUDIO_NAME%.exe" %*
exit /b
:failed
echo NavStudio-Paket konnte nicht geprueft werden. README.md beachten.
pause
exit /b 1
