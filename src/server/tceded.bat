@echo off
cd /d "%~dp0..\.."
etded.exe +set fs_game tce2 +set com_hunkMegs 256 +set com_zoneMegs 128 +set com_soundMegs 64 +set dedicated 1 +set sv_fps 30 +set match_latejoin 1 +set sv_pure 1 +set net_port 27960 +set logfile 2 +set g_gametype 5 +exec serverconfig.cfg
pause
