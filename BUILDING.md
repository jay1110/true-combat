# TC:E mod builds

This repository builds the `cgame`, `ui` and `qagame` mod libraries. It does not
build an ET engine, renderer, browser host or Android application. ET 2.60b uses
32-bit native modules; other architectures require an external matching host.
WASM uses the separate ETLegacy syscall-vector ABI1 host contract.

## Public build

CMake 3.21+, C/C++17 compilers, Boost headers and a platform SDK are required. Ninja is recommended
for Clang/GCC; Visual Studio 2022 supports the Windows builds.

```powershell
cmake --preset windows-msvc-win32
cmake --build build/modern/windows-msvc-win32 --config Release --parallel 4
ctest --test-dir build/modern/windows-msvc-win32 -C Release --output-on-failure
```

```sh
cmake --preset linux-clang-release
cmake --build build/modern/linux-clang-release --parallel 4
ctest --test-dir build/modern/linux-clang-release --output-on-failure
```

The default includes the Omni-bot gameplay bridge and matching runtime core.
Navigation authoring and the desktop NavStudio application remain separate.
Server Lua is enabled; `-DFEATURE_LUA=OFF` disables it. Lua is not a
client dependency. See [the Lua API](src/lua/README.md) for script configuration.
`cmake -S src` remains supported, but new automation should use the repository
root. `tce_modules` builds all three mod libraries. `cmake --install` separates
the game/editor destinations.

## Explicit optional builds

| Option | Effect | Default |
|---|---|---|
| `FEATURE_OMNIBOT` | Server-side gameplay bot bridge | ON |
| `FEATURE_NAVSTUDIO` | In-game waypoint/goal authoring; requires bot bridge | OFF |
| `TCE_BUILD_OMNIBOT_CORE` | Separate `omnibot` core target | ON with gameplay bots |
| `TCE_BUILD_NAVSTUDIO` | Desktop `navstudio`, `navstudio-run`, Windows package targets | OFF |
| `TCE_BUILD_NAVMESH_TOOL` | Separate pinned Recast geometry tool | OFF |
| `TCE_BUILD_TCE_HELPERS` | Local legacy launcher helper target | OFF |
| `TCE_BUILD_REFERENCE_TESTS` | Historical Win32 original-DLL comparison suite | OFF |

```powershell
# Public gameplay package with explicitly requested bot runtime:
python src/build_windows_variants.py --variant game --with-omnibot --out dist/Game-With-Bots
# Separate editor DLLs, bot core and editor UI addon:
python src/build_windows_variants.py --variant navstudio --out dist/Editor
# Both variants, with bot runtime explicitly enabled for the game as well:
python src/build_windows_variants.py --variant both --with-omnibot
```

Each variant uses `tce2-<TCE_VERSION>.pk3`, currently `tce2-1.0.2.pk3`, inside
its own directory and a versioned ZIP. The PK3 has the versioned client manifest.
Only the editor variant includes `zz_tce2_navstudio_editor.pk3`. Public builds
exclude authoring source and commands at compile time. The normal client has
no waypoint debug renderer. Gameplay bot scripts and navigation remain separate
runtime data, included in the release packages; the DLL package alone is not a
complete game installation. See the [manual release workflow](tools/release/README.md)
for every target, source archives and the separate desktop application.

Country flags use `g_countryflags` on the server (latched) and `cg_countryflags`
on the client. Supply an existing legacy IPv4 country database explicitly:

```powershell
python src/build_windows_variants.py --variant game --geoip-database C:/data/GeoIP.dat --out dist/Game
```

The database is installed as loose server-side `tce2/GeoIP.dat`; it is never in
the client PK3. The PK3 includes the flag atlas and its provenance/license.
Missing/invalid database entries display unknown countries; localhost is local.
Use `r_primitives 2` for the tested classic ET OpenGL rendering path.

The builder never starts or deploys a game. It refuses stale optional files in
a public output; use a fresh output directory rather than mixing variants.
Installers check competing client-DLL PK3 files before copying, because a
later-sorting old archive can otherwise override the new client code.

The core can also be configured separately:

```sh
cmake -S omnibot -B build/bots -DOMNIBOT_STD_FILESYSTEM=ON
cmake --build build/bots --config Release --target omnibot-et
```

Boost headers are required for the core; `OMNIBOT_BOOST_INCLUDEDIR` selects them.
The STD_FILESYSTEM mode avoids compiled Boost libraries. NavStudio desktop
dependencies are pinned in `navstudio/requirements-desktop.txt` and are not
required for any public mod build.

## Target matrix

`cmake --list-presets` lists the supported configuration entries. Architecture
is derived from both CPU and pointer ABI, rather than pointer size alone.

| Target | Inputs / host requirement |
|---|---|
| Windows x86 / x64 | MSVC or clang-cl with Windows SDK; matching 32/64-bit engine |
| Linux i386 / x86_64 | Clang/GCC with libc development files, or supplied Zig toolchain |
| Linux ARMv7 / AArch64 | Supplied cross-toolchains, sysroot or Zig; matching engine |
| Android armeabi-v7a / arm64-v8a | Installed NDK, explicit ABI/API; external Android engine |
| WASM32 | Emscripten; matching external ABI1 side-module host |
| macOS universal | AppleClang/Xcode SDK; x86_64 + arm64 external host |

The toolchain files are under `cmake/toolchains/`. Set `TCE_ZIG_EXECUTABLE` for
Zig, `ANDROID_NDK_HOME` for Android presets, and `EMSDK` for the WASM preset.
No cross-target runtime parity follows from successful compilation. Existing
original numeric profiles remain explicitly documented under `compat/`.

## Bundled validation

```powershell
python tools/modernize/validate_mod.py --out build/validation --cross
```

Select an empty output directory; `--resume` explicitly reuses build caches and
archives previous reports/logs. The bundle performs configure, Debug/Release,
native/vector ABI golden tests, sanitizer builds and available cross-target
builds. On this Windows setup the sanitizer runner selects MSVC 14.44 through
a matching developer environment and Ninja, avoiding mixed compiler/CRT/ASAN
versions. Clang/GCC sanitizer jobs are supplied in CI. Unavailable SDKs are
reported rather than counted as passes.

Use `--baseline-dir <local-snapshot>` to add normalized function-body and named
export/source-signature comparisons. `risk_scan.py` is a conservative lexical
index, not a complete AST or a proof of behavioral parity. `compat_validate.py`
checks retained baseline hashes independently and reports reviewed runtime
corrections separately. Original hashes remain in the manifest.
`compat_word_probe.py` compares optimized integer helpers against frozen previous
implementations in Debug and Release.

Native game smoke uses only an already running local installation:

```powershell
python tools/modernize/game_smoke.py --instance <existing-game> --out build/game-smoke.json --password <local-rcon-password>
```

It creates no game copies, starts no server and generates no waypoints. Full
gameplay, objective and cross-target runtime validation remain separate from
deterministic helper tests.

## Layout and preserved contracts

`src/` contains mod/game/UI/cgame source. Third-party code is under `vendor/`,
with provenance and content pins in `vendor/dependencies.lock.json`.
`compat/legacy/` holds classified numeric/ABI helper bodies; old include paths
forward there. `platform/` isolates genuine native OS services. Engine
filesystem, timing and rendering services remain engine traps. `navstudio/`,
`omnibot/` and `tce/` are explicit optional tools, not public gameplay modules.
The `tce_` prefix on a gameplay source file does not make it optional tooling.

Existing SDK-facing structures, syscall numbers, `dllEntry`, and `vmMain`
signatures are preserved. Compatibility profiles and legacy precision branches
are retained until their behavior can be separately validated; no blanket
removal of Linux/Windows branches or new assembly was introduced.
