# Troubleshooting

## Omni-bot not loaded or build mismatch

Run `bot load` from the server console/RCON and read the preceding loader error.
Check the engine process architecture, QGame/core names, runtime path and
Game/Editor edition. Install the matching QGame, core and scripts together.
Changing Cvars cannot repair an incompatible native function table.

## No navigation or unreachable objectives

Check the actual map name, `.way` and mode-specific GM pair. A graph alone does
not supply bomb/capture goals or every ladder/door recipe. Use the Editor edition
for authoring; the Game edition deliberately omits those tools.

## Old menus, missing images or mixed interface

Look for competing TCE2 update PK3s and old loose UI/CGame libraries in both
`fs_basepath` and `fs_homepath`. Restart after installing a matched package.
Retain original TC:E packages; remove superseded TCE2 updates from active lookup.
Do not install only a menu file without its shaders/artwork and client modules.

## Small or clipped text

Increase Menu Size/HUD Size as appropriate. Compass, Fireteam and Commandmap
have separate controls. Use the included fonts and matching CGame/UI pair.
Classic ET 2.60b and ETLegacy do not share every renderer/font implementation.
The classic OpenGL test path uses `r_primitives 2`.

## Gametype changes do not take effect

Select `+set g_gametype 2`, `5` or `7` before startup and use a map with the
corresponding TCE script. Do not substitute ordinary ET mode descriptions.
For custom scripts, check `g_mapscriptdirectory` and the exact `_gtN.script` name.

## Background settings or weather

Background changes take effect at the next process start. Season uses configured
calendar rules; live weather requires the optional launcher with an explicit
location. Missing fresh weather falls back to local time rather than claiming snow.

## Reporting a bug

Include version, edition, OS/process architecture, engine/version, map/gametype,
steps, expected/actual result and relevant console error. Remove credentials,
private player data and personal filesystem paths before attaching logs.
