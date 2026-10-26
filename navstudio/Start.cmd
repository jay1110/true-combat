@echo off
setlocal
for %%I in ("%~dp0..") do set "NAVSTUDIO_WORKSPACE=%%~fI"
set "NAVSTUDIO_EXE=%NAVSTUDIO_WORKSPACE%\build\navgen-overkill\TCE2-NavStudio.exe"
if exist "%NAVSTUDIO_WORKSPACE%\build\navstudio-desktop-deps\webview" set "PYTHONPATH=%NAVSTUDIO_WORKSPACE%\build\navstudio-desktop-deps;%PYTHONPATH%"
rem Source entry is current by definition; never silently prefer an old EXE.
where py >nul 2>nul
if not errorlevel 1 (
  py -3 -c "import webview" >nul 2>nul
  if not errorlevel 1 goto source_py
)
where python >nul 2>nul
if not errorlevel 1 (
  python -c "import webview" >nul 2>nul
  if not errorlevel 1 goto source_python
)
if not exist "%NAVSTUDIO_EXE%" goto failed
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0verify-package.ps1" -Package "%NAVSTUDIO_WORKSPACE%\build\navgen-overkill" -Workspace "%NAVSTUDIO_WORKSPACE%"
if errorlevel 1 goto failed
"%NAVSTUDIO_EXE%" --workspace "%NAVSTUDIO_WORKSPACE%" %*
goto result
:source_py
py -3 "%~dp0entry.py" --workspace "%NAVSTUDIO_WORKSPACE%" %*
goto result
:source_python
python "%~dp0entry.py" --workspace "%NAVSTUDIO_WORKSPACE%" %*
:result
if errorlevel 1 goto failed
exit /b
:failed
echo.
echo NavStudio konnte nicht gestartet werden. Aktuelles Desktop-Paket oder Python mit requirements-desktop.txt benoetigt. README.md beachten.
pause
exit /b 1
