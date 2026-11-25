# Troubleshooting

## Custom player animations and replaced sounds

Animation groups and scripts determine the numeric animation IDs used by the
server and clients. A package that adds or reorders animation definitions must
be installed on the server and the connecting clients together. Merely replacing
an MDX file while preserving the shared animation definitions is a different case.

The supplied `z101_TCERemaster_Animations.pk3` changes the flattened player group
from 236 to 288 definitions and changes their order. Its animation group and
script therefore need a matching server installation. On a pure server,
client-only animation or sound replacement packages can also be excluded by the
engine's approved package list. Check the server's `sv_pure` policy and installed
packages before changing mod code.

Use `path` to inspect package precedence and `modellist` to inspect loaded
animation models. During local ET 2.60b testing with the package installed on both
sides, its custom `rtcw_deaths.mdx` and `rtcw_kick.mdx` were loaded. A different
server's package configuration must be checked separately.

Install replacement packages in the active mod directory (`fs_game tce2`), not
only in a separate original TC:E installation. Restart the game after changing
packages: registered sound/model handles are cached. The firemode selector uses
`sound/weapons/misc/firemode.wav` through the engine filesystem, so its effective
replacement depends on package precedence and the server's package policy.

Room reverberation also depends on the map's sound environments and
`cg_snd_reverb`; suppressed/subsonic weapons deliberately have different sound
behavior. Distant gunshot variants are separate from room responses. Version
1.0.5 fixes the independent distant-variant selection when custom weapons define
fewer echo sounds than close sounds.

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

Look for competing TCE2 update PK3s and old loose UI/CGame/QGame libraries in both
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

### Manifest version differs from the installed package

If a local start reports `Missing/invalid tce2-1.0.4.pk3 client manifest` with a 1.0.5 package installed, an older QGame module may be loaded from `fs_homepath`. The version in this message is compiled into QGame. Close the game and install matching QGame, UI, CGame and the update PK3; check both base and home directories. The local test launcher backs up and synchronizes existing cached Windows modules with its installed copies before startup.

### Wall lowering and corpse transitions (1.0.5)

The server controls `g_weaponWallAvoidance` (`1` by default, `0` to disable for everyone). Lowering and return use command-time smoothing over at least 250 ms for full travel. With this rule enabled, wall-impact effects come from the server; local predicted marks must not draw an obsolete straight shot while the weapon is lowered. Muzzle flashes and recoil remain locally predicted. The same server-controlled lowering is transmitted to third-person player models, including other players. A low-ready arm pose moves the grip back and down rather than bending the torso toward the wall. The barrel follows the server aim direction. The pose is selected by animation name, so compatible animation replacements retain their declared model and frame mapping. Crouching uses an upward high-ready arm pose. While prone, the weapon turns left or right toward a clear direction before reaching the wall. Crawling and lack of side clearance put it visibly on the back. Small heading changes retain the selected side. Switching requires at least 20 degrees of heading change and better clearance; the bounded transition temporarily stows the weapon. A visible side-deflected weapon can fire, with authoritative shots following its displayed yaw. Only the stowed/crawling state blocks firing in prediction and on the server without spending ammunition. Moving away restores the ordinary prone weapon pose and firing. Shared movement also checks head clearance against solid world geometry. Update both server QGame and client CGame together.

When Space moves a dead player to reinforcement, the corpse keeps the chosen death sequence rather than an idle/run state or a fixed stock animation number. Some custom MDX clips contain a standing reset frame after their resting pose. Corpse-only terminal selection checks the adjacent head-tag positions and avoids that abrupt reset. It does not modify the animation package or normal death playback.
