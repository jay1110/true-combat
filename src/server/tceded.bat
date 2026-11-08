@echo off
setlocal
if "%~1"=="" (
  echo Usage: %~nx0 game-directory [engine-name]
  exit /b 2
)
set "TCE_GAME_DIR=%~f1"
set "TCE_ENGINE=%~2"
if not defined TCE_ENGINE set "TCE_ENGINE=etded.exe"
if not exist "%TCE_GAME_DIR%\etmain\" (
  echo An existing Enemy Territory installation is required.
  exit /b 2
)
if not exist "%TCE_GAME_DIR%\tce2\serverconfig.cfg" (
  echo Copy the server configuration into the installed tce2 directory first.
  exit /b 2
)
pushd "%TCE_GAME_DIR%" || exit /b 2
"%TCE_ENGINE%" +set fs_game tce2 +set dedicated 2 +set net_port 27960 +set g_gametype 5 +set sv_official 0 +exec serverconfig.cfg
set "TCE_RESULT=%ERRORLEVEL%"
popd
exit /b %TCE_RESULT%
