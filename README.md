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
| **GrooveSix Studios and Team Terminator** | Original True Combat: Elite game, design and assets. Credits include Coroner, Tweety, Airon, Bruce, BS, Diane, HellOnStage, Liquid, LordOfKao$, Dender, ISM, Law, Nazgul, Race, Raptor, Traumahawk and Tier. |
| **jay1110 and TCE2 contributors** | TC:E-specific reconstruction, gameplay fixes, bot integration and current development. |
| **Fabian Grutschus** | Original fabiang/true-combat distribution repository and tooling. |
| **id Software, Splash Damage, Activision** | Enemy Territory/Quake technology, SDK and original development/publishing. Gray Matter Studios, Nerve Software and Orange Smoothie Productions are credited in the original game. |
| **ETLegacy contributors** | Compatibility/build references and specifically recorded imported components. |
| **TrueCombat: Close Quarters Battle contributors** | Behavioral reference for selected name-display and spawn-protection behavior; no official affiliation. |
| **Omni-bot contributors** | Compatible bot core, bridge, mission scripts and navigation runtime. |
| **Timo “Timothy” Smit** | WolfAdmin 1.2.1, adapted for this server runtime. |
| **Lua, SQLite and LuaSQL authors** | Server scripting and database components. |
| **GameMonkey / Auran, Wild Magic / David Eberly, iprof, PhysicsFS / Ryan C. Gordon, zlib/LZMA contributors** | Components used by the compatible Omni-bot import. |
| **Boost contributors; RecastNavigation / Mikko Mononen and contributors** | Bot development dependency; optional navigation geometry tools. |
| **Barlow Project Authors / Jeremy Tribby** | Barlow and Barlow Condensed fonts, SIL OFL 1.1. |
| **Bud Byrd and Mike Miscevic / Xmod** | Flag artwork and documented platform conventions, with retained notices. |
| **TCE2 contributors and supplied map-image authors** | UI/HUD artwork and map backgrounds; individual origins accompany the assets. |
| **Open-Meteo and GeoNames** | Optional weather and location data. |
| **pywebview, PyInstaller, FreeType, Sphinx, MyST and Read the Docs theme contributors** | Optional authoring/documentation tools. |

[Full credits and provenance](Docs/credits.md) include source locations, original
in-game credits, component licenses and known provenance gaps. Original notices
remain authoritative; the root [LICENSE](LICENSE) does not relicense every
included component or the original TC:E assets.

## Contributing and reporting issues

Report problems in the [issue tracker](https://github.com/jay1110/true-combat/issues)
with the TCE2 version, engine/version, OS, architecture, map, gametype and
reproducible steps. Include relevant console output with passwords and personal
information removed. Distinguish Game and Editor editions.

Gameplay changes should preserve the ET module ABI and include suitable regression
coverage. Keep original source notices and asset attribution intact.
