# Building TCE2

## Requirements and defaults

CMake 3.21+, a C/C++17 compiler, Boost headers and the target platform SDK are
required. Normal builds include QGame, CGame, UI and the matching gameplay bot
core. An engine and TC:E assets are needed to run the result, not to compile
the public modules.

```powershell
cmake --preset windows-msvc-win32
cmake --build --preset windows-msvc-win32-release
ctest --test-dir build/modern/windows-msvc-win32 -C Release --output-on-failure
```

```sh
cmake --preset linux-clang-release
cmake --build --preset linux-clang-release
ctest --test-dir build/modern/linux-clang-release --output-on-failure
```

`cmake --list-presets` shows configuration entries. Use `OMNIBOT_BOOST_INCLUDEDIR`
when Boost headers are not found automatically. The std::filesystem bot build
avoids compiled Boost filesystem libraries.

## Edition and feature options

| Option | Default | Purpose |
| --- | --- | --- |
| `FEATURE_OMNIBOT` | ON | Gameplay server bridge. |
| `TCE_BUILD_OMNIBOT_CORE` | ON with bots | Matching compatible bot library. |
| `FEATURE_LUA` | ON | Optional server Lua runtime; no automatic scripts. |
| `FEATURE_NAVSTUDIO` | OFF | Native navigation/goal authoring. |
| `TCE_BUILD_NAVSTUDIO` | OFF | Desktop authoring application. |
| `TCE_BUILD_NAVMESH_TOOL` | OFF | Separate Recast geometry helper. |

Do not combine modules from different Game/Editor editions. The `tce_modules`
target builds the three game-facing modules; the bot core is a separate target.

## Targets

| Family | Required toolchain/host |
| --- | --- |
| Windows x86/x64 | MSVC/clang-cl and Windows SDK; architecture-matched engine. |
| Linux i386/x86_64 | Clang/GCC and matching development libraries, or configured Zig. |
| Linux ARMv7/AArch64 | Cross-toolchain/sysroot or configured Zig; matching engine. |
| Android ARMv7/ARM64/x86/x86_64 | Android NDK and matching external engine. |
| macOS universal x86_64 + arm64 | AppleClang/Xcode SDK and matching host. |
| WASM32 | Emscripten and compatible ETLegacy ABI1 side-module host. |

The targets build mod libraries, not an Android APK, browser engine or standalone
game. A configured target is not evidence of testing on every device.

## Packaging and checks

The packager collects `pak` assets and selected native modules into one mod PK3,
with server libraries and shared scripts in the ZIP's `server` tree. The Editor
overlay is explicit. See the
[packaging reference](https://github.com/jay1110/true-combat/blob/final/tools/release/README.md)
for complete matrices and installable archives.

```sh
python -m unittest discover -s tools/release -p "test_*.py"
python tools/assets/prepare_pak.py --check
```

The [full build guide](https://github.com/jay1110/true-combat/blob/final/BUILDING.md)
documents platform and validation options. Gameplay changes also need suitable
native regression and runtime checks; a successful documentation or package
check alone does not establish gameplay parity.
