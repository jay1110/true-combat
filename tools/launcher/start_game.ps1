# Optional weather-aware launcher; forwards arguments without shell-built code.
& python (Join-Path $PSScriptRoot 'start_game.py') @args
exit $LASTEXITCODE
