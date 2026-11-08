# Credits, origins and licenses

## TCE2 and the original game

**TCE2's TC:E-specific mod implementation is reconstructed code, not the
original proprietary True Combat: Elite source release.** Binary analysis,
behavioral comparison and new implementation were used to recreate it.
SDK-derived source and third-party components are distinct, credited inputs.
This project is not endorsed by the original developers and does not claim
ownership of their game, art, names or trademarks.

- **jay1110 and TCE2 contributors**: reconstruction, integration, gameplay fixes,
  modern UI/HUD, tooling and continued maintenance.
- **Fabian Grutschus**: the original
  [fabiang/true-combat repository](https://github.com/fabiang/true-combat),
  distribution setup and tooling; the root BSD notice credits that work.
- **GrooveSix Studios and Team Terminator**: True Combat: Elite design,
  implementation and original assets, including the work of Coroner, Tweety,
  Airon, Bruce, BS, Diane, HellOnStage, Liquid, LordOfKao$, Dender, ISM, Law,
  Nazgul, Race, Raptor, Traumahawk, Tier and the original testing community.
- **TrueCombat: Close Quarters Battle contributors**: behavioral comparison for
  selected labels and spawn protection; not a claim of an official code release.
- **id Software, Splash Damage and Activision**: original Enemy Territory/Quake
  technology, SDK, development and publishing. Original credits also acknowledge
  Gray Matter Studios, Nerve Software, Orange Smoothie Productions and Even Balance.

[Original in-game credits](original-credits.md) preserve the credited names and
roles from the standard TC:E game menus, including additional developers,
voice artists and technology credits.

## Imported components and dependencies

Source-relative paths below identify the material in this repository. Existing
copyright and permission text in each component remains authoritative.

| Component / authors | Origin and repository location | Notice / licensing record |
| --- | --- | --- |
| Enemy Territory SDK / id Software and Splash Damage contributors | Retained SDK/game interfaces in `src`; external engine ABI. | Existing source notices and original game terms; not covered by a blanket TCE2 license. |
| ETLegacy contributors | [ETLegacy](https://github.com/etlegacy/etlegacy); selected compatible Omni-bot and LuaSQL imports and structural/compatibility reference. | Per-import notices and pinned LuaSQL revision; not a wholesale engine import. |
| Omni-bot / jswigart and contributors | [Omni-bot](https://github.com/jswigart/omni-bot), compatible 0.93-era core/bridge under `vendor/omni-bot` and `vendor/omnibot`. | `vendor/omni-bot/IMPORT-NOTICE.txt`; exact original import revision and some standalone license texts are unresolved. |
| GameMonkey / Matthew Riek, Greg Douglas, Auran Development Ltd. and extension contributors | `vendor/omni-bot/source/Omnibot/dependencies/gmscriptex`. | GameMonkey permission notice in `gmsrc_ex/src/gm/gmMachine.h`; retained extension notices. |
| Wild Magic / David Eberly | Compatible Wild Magic 3 headers/source in the bot dependencies. | Headers refer to `WildMagic3License.pdf`; that standalone historical text was not supplied. |
| iprof contributors | Bot profiling dependency. | Retained source; a separate license text was not supplied with this import. |
| PhysicsFS / Ryan C. Gordon and contributors | Bot virtual-filesystem dependency. | Included `dependencies/physfs/LICENSE.txt`, zlib-style notice and `CREDITS.txt`. |
| zlib / Jean-loup Gailly and Mark Adler; LZMA / Igor Pavlov and contributors | Compression code in the PhysicsFS/bot dependency tree. | Retained source/README permission notices; consult the exact imported files. |
| Lua / Lua.org, PUC-Rio and contributors | Lua 5.4.9 in `vendor/lua`; [Lua](https://www.lua.org/). | MIT notice in `lua.h` and import notice. |
| SQLite / D. Richard Hipp and contributors | SQLite amalgamation in `vendor/sqlite`; [SQLite](https://www.sqlite.org/). | Public-domain source declaration and import record. |
| LuaSQL / Kepler Project and contributors | ETLegacy-pinned SQLite driver in `vendor/luasql`; [LuaSQL](https://github.com/lunarmodules/luasql). | Included MIT-style `license.html`; documented TCE host-policy adaptation. |
| WolfAdmin / Timo “Timothy” Smit | WolfAdmin 1.2.1 adaptation in `src/lua/wolfadmin`; [WolfAdmin](https://github.com/timosmit/wolfadmin). | Retained GPLv3 license/copyright and original configuration/schema. |
| Boost contributors | External header dependency for the bot core; [Boost](https://www.boost.org/). | [Boost Software License 1.0](https://www.boost.org/LICENSE_1_0.txt). |
| RecastNavigation / Mikko Mononen and contributors | Optional geometry builder, pinned v1.6.0 revision in `navstudio/navmesh/CMakeLists.txt`. | Upstream zlib license; optional authoring dependency. |

Dependency versions, source URLs, content fingerprints and deliberate changes
are recorded in `vendor/dependencies.lock.json` and each component's import record.
Do not assign the license of a different/newer upstream tree to an older import.

## Art, fonts and data

| Material | Credit and origin |
| --- | --- |
| Original TC:E models, sounds, maps, textures and publisher marks | Their original authors; game terms are retained in `EULA_TrueCombat_Elite.txt`. Publisher-mark shaders use marks already present in the original installation. |
| Barlow and Barlow Condensed | Jeremy Tribby and the Barlow Project Authors. SIL Open Font License 1.1; full texts in `pak/fonts/tce2_ui/LICENSE-Barlow*.txt`. Font source revisions and generated atlas metadata accompany the fonts. |
| Country flags | Bud Byrd and Mike Miscevic / Xmod; Apache 2.0 notice retained in `pak/gfx/flags/LICENSE.xmod.txt`. |
| Navigation and Fireteam icons | Original geometric TCE2 artwork, recorded as CC0-1.0 in the corresponding `pak/gfx/tce2/*/SOURCE.json`. |
| Modern UI artwork | TCE2 geometric/interface work; the industrial scene was created with OpenAI image generation. Its source/method and asset fingerprints are recorded in the UI provenance. |
| Map backgrounds and Delta command-map pictures | Supplied project/user map imagery. Per-image records identify source filenames and hashes; no authorship or general license for the original maps is inferred. |
| Optional live weather | [Open-Meteo](https://open-meteo.com/) weather data; [GeoNames](https://www.geonames.org/) location data, used by the explicitly configured optional launcher. |

## Development and documentation tools

Optional tools credit **pywebview**, **PyInstaller**, **FreeType/freetype-py**,
**RecastNavigation**, **CMake**, **Clang/LLVM**, **GCC**, **Microsoft toolchains**,
**Zig**, **Emscripten**, **Sphinx**, **MyST-Parser** and the **Read the Docs theme**
and their respective contributors. Ghidra/GhidraMCP and REA were used during
analysis; OpenAI/Codex assisted development and documentation. Tool use does not
transfer ownership of the original game or imply endorsement by those projects.
Individual tool distributions retain their own licenses.

## License boundaries and outstanding provenance

The root `LICENSE` is the inherited Fabian Grutschus BSD notice. It is **not** a
license grant for every SDK file, imported library, reconstructed behavior or
original TC:E asset. This document does not replace component licenses or
relicense material whose author/license is unknown.

The compatible Omni-bot import did not include a top-level Omni-bot license,
a standalone iprof license or the referenced Wild Magic 3 license PDF. Their
absence is explicitly recorded rather than filled with an unrelated license.
Before redistributing those components, resolve the missing permission texts
and review the original game terms. Reconstruction is a provenance statement,
not a claim that the original game's terms cease to apply.

If a credit or origin is incomplete, report the component/file and an
identifiable source record so it can be corrected without inventing authorship.

```{toctree}
:maxdepth: 1

original-credits
```
