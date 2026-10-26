# TCE2 mod source

TCE2 builds the `cgame`, `ui` and `qagame` modules for True Combat: Elite.
The current mod version is **1.0.2**. A compatible external Enemy Territory /
ETLegacy engine and the original game data are required; this repository does
not build or redistribute an engine or the original maps, sounds and models.

Normal Game builds include **Omni-bot gameplay**, its runtime library and the
required behavior scripts. Waypoint/goal editing, editor UI and the desktop
NavStudio application are built and packaged separately.

## Build

Use CMake 3.21 or newer and the appropriate platform SDK. Clang/Ninja,
Visual Studio and GCC configurations are supplied in `CMakePresets.json`.
Vendored source dependencies and their provenance are under `vendor/`.

```powershell
cmake --preset windows-msvc-win32
cmake --build --preset windows-msvc-win32-release
```

```sh
cmake --preset linux-clang-release
cmake --build --preset linux-clang-release
```

See [BUILDING.md](BUILDING.md) for bot dependencies, target architectures,
separate Game/Editor builds, portable tests and package instructions.

## Manual releases

[The release workflow](.github/workflows/release.yml) is started explicitly with
GitHub Actions **Run workflow**. Workflows have no push, pull-request, schedule
or release-event triggers. The release build produces versioned archives with
native mod modules, `tce2-1.0.2.pk3`, the bot runtime and separate editor /
NavStudio deliverables. Each architecture needs a matching external host.

## Source layout

| Directory | Contents |
| --- | --- |
| `src/` | Game, client, UI, Lua modules and tests |
| `compat/` | Reviewed legacy numeric and ABI compatibility |
| `platform/` | Native platform services |
| `vendor/` | Dependency source, bot scripts and attribution |
| `omnibot/` | Bot build integration |
| `navstudio/` | Separate editor, BSP/Recast tools and desktop packaging |
| `tools/` | Build, release and validation tools |
| `Docs/` | Maintained audit and release documentation |

Local game installations, original reference binaries, reconstruction evidence,
credentials and generated build files are excluded from version control.
Source-file copyright and license notices and vendored dependency licenses
remain applicable; see also [LICENSE](LICENSE).
