# TCE2

**An unofficial reconstruction and continued development of the True Combat: Elite mod.**

TCE2 recreates the TC:E-specific client, UI and server implementation for
Wolfenstein: Enemy Territory. **This repository does not contain the original
proprietary True Combat: Elite source code.** Its mod-specific behavior has been
rebuilt through binary analysis, comparison with the original game, and new
source implementation. Retained Enemy Territory SDK code and third-party
libraries remain credited to their respective authors.

Current version: **1.0.4**. This community project is independent of GrooveSix
Studios, Team Terminator and the original TC:E/ET developers. It is not an
official source release, an engine port, or a claim of complete behavioral parity.

## Play and configure

TCE2 requires an installed **True Combat: Elite 0.49b asset set** and a compatible
external **ET 2.60b or ETLegacy engine**. The mod update does not replace the
original maps, models, sounds or engine.

- [Installation and upgrades](Docs/installation.md)
- [Game modes and controls](Docs/gameplay.md)
- [Menus, HUD and backgrounds](Docs/interface.md)
- [Server setup](Docs/server.md) and [Omni-bot](Docs/bots.md)
- [Cvar reference and change history](Docs/cvars.md)
- [Lua and WolfAdmin](Docs/scripting.md)
- [Changelog, including pre-1.0.0 history](CHANGELOG.md)
- [Troubleshooting](Docs/troubleshooting.md)

The normal **Game** edition includes Omni-bot gameplay. The optional **Editor**
edition adds navigation/goal authoring and is intended for map development.
Install matching game modules, bot core and scripts from the same edition.

## Features

- TCE-specific bot objectives, jumping, climbing, ladders and door/switch routes.
- Revised hit registration, regional bullet damage and moving-target rewind.
- GT7 custom spawn selection that favors distance from living opponents.
- Modern menus, deployment, scoreboard and loading screens.
- Configurable colors/transparency and independently scaled HUD widgets.
- CQB-style team labels, TCE weapon statistics and expanded Fireteam information.
- Optional Lua server scripting and WolfAdmin administration.

See the [categorized changelog](CHANGELOG.md) for Added, Removed, Fixed and
Modified entries and the individual Cvars involved.

## Installable package

The Game ZIP contains a single `tce2/tce2-1.0.4.pk3` with menus, project assets
and UI/CGame modules. `server/<target>/` contains matching QGame and bot
libraries; `server/common/` contains shared server scripts/data. Copy the
appropriate server contents into the game's `tce2` directory. The
[installation guide](Docs/installation.md) explains architecture selection,
classic ET client modules and safe upgrades.

Build targets cover Windows x86/x64, Linux x86/ARM, Android ARM/x86, macOS
universal and WASM32. Each requires a matching engine host; target support does
not imply a standalone application or testing on every device.

## Build from source

Use CMake 3.21+, a C/C++17 toolchain, Boost headers and the platform SDK.
The default builds the gameplay modules and matching bot core.

```powershell
cmake --preset windows-msvc-win32
cmake --build --preset windows-msvc-win32-release
```

```sh
cmake --preset linux-clang-release
cmake --build --preset linux-clang-release
```

[BUILDING.md](BUILDING.md) covers toolchains, target architectures, tests and the
separate Editor edition. The [manual packaging workflow](tools/release/README.md)
assembles native modules into the installable ZIP.

## Documentation

The [English manual](Docs/index.md) builds with Sphinx/MyST.
[Read the Docs configuration](.readthedocs.yaml) is included.

```sh
python -m pip install -r Docs/requirements.txt
python -m sphinx -W --keep-going -b html Docs build/docs/html
```

[Documentation maintenance and Read the Docs setup](Docs/documentation.md)
describe validation and publishing. A hosted address is assigned when the
repository is connected to a Read the Docs project.

## Credits and origins

| Contributor / project | Contribution or origin |
| --- | --- |
| **GrooveSix Studios and Team Terminator** | Original [True Combat: Elite](https://www.truecombatelite.com/) game, design and assets; [official TrueCombat community](https://steamcommunity.com/groups/truecombatmod). Credits include Coroner, Tweety, Airon, Bruce, BS, Diane, HellOnStage, Liquid, LordOfKao$, Dender, ISM, Law, Nazgul, Race, Raptor, Traumahawk and Tier. |
| **jay1110 and TCE2 contributors** | TC:E-specific reconstruction, gameplay fixes, bot integration and [current development](https://github.com/jay1110/true-combat). |
| **id Software, Splash Damage, Activision** | [Enemy Territory technology and source](https://github.com/id-Software/Enemy-Territory), [original ET development](https://www.splashdamage.com/games/wolfenstein-enemy-territory/) and [publishing](https://www.activision.com/). Gray Matter Studios, Nerve Software and Orange Smoothie Productions are credited in the original game. |
| **ETLegacy contributors** | [Compatibility/build references and recorded imports](https://github.com/etlegacy/etlegacy). |
| **TrueCombat: Close Quarters Battle contributors** | [CQB](http://cqb.truecombat.net/) behavioral reference for selected name-display and spawn-protection behavior; no official affiliation. |
| **Omni-bot contributors** | [Bot core, bridge, mission scripts and navigation runtime](https://github.com/jswigart/omni-bot). |
| **Timo “Timothy” Smit** | [WolfAdmin](https://github.com/timosmit/wolfadmin) 1.2.1, adapted for this server runtime. |
| **Lua, SQLite and LuaSQL authors** | [Lua](https://www.lua.org/), [SQLite](https://www.sqlite.org/) and [LuaSQL](https://github.com/lunarmodules/luasql) server scripting and database components. |
| **GameMonkey / Auran, Wild Magic / David Eberly, iprof / Sean Barrett, PhysicsFS / Ryan C. Gordon, zlib/LZMA contributors** | [GameMonkey](https://github.com/publicrepo/gmscript), [Wild Magic lineage](https://www.geometrictools.com/), [Sean Barrett](https://nothings.org/), [PhysicsFS](https://icculus.org/physfs/), [zlib](https://zlib.net/) and [LZMA SDK](https://www.7-zip.org/sdk.html), used by the compatible Omni-bot import. |
| **Boost contributors; RecastNavigation / Mikko Mononen and contributors** | [Boost](https://www.boost.org/) bot development dependency; [RecastNavigation](https://github.com/recastnavigation/recastnavigation) optional navigation geometry tools. |
| **Barlow Project Authors / Jeremy Tribby** | [Barlow and Barlow Condensed](https://github.com/jpt/barlow) fonts, SIL OFL 1.1. |
| **Bud Byrd and Mike Miscevic / Xmod** | [Xmod](https://github.com/jay1110/xmod) flag artwork and documented platform conventions, with retained notices. |
| **vi$ion** | [Original ServerListFix package](https://github.com/jay1110/true-combat/blob/52122ec81b8d615731a63a348f944a5a77f605c9/tcetest/zzzzzzzzzzzzzz_ServerListFix.pk3), used as the server-browser query reference. |
| **TCE2 contributors and supplied map-image authors** | UI/HUD artwork and map backgrounds; [individual asset origins](Docs/credits.md#art-fonts-and-data). |
| **Open-Meteo and GeoNames** | Optional [weather](https://open-meteo.com/) and [location](https://www.geonames.org/) data. |
| **pywebview, PyInstaller, FreeType, Sphinx, MyST and Read the Docs theme contributors** | Optional [authoring and documentation tools with upstream links](Docs/credits.md#development-and-documentation-tools). |

[Full credits and provenance](Docs/credits.md) include source locations, original
in-game credits, component licenses and known provenance gaps. Original notices
remain authoritative for each component and the original TC:E assets; see the
[license boundaries](Docs/credits.md#license-boundaries-and-outstanding-provenance).

## Contributing and reporting issues

Report problems in the [issue tracker](https://github.com/jay1110/true-combat/issues)
with the TCE2 version, engine/version, OS, architecture, map, gametype and
reproducible steps. Include relevant console output with passwords and personal
information removed. Distinguish Game and Editor editions.

Gameplay changes should preserve the ET module ABI and include suitable regression
coverage. Keep original source notices and asset attribution intact.
