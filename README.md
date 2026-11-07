# TCE2 mod source

TCE2 builds the `cgame`, `ui` and `qagame` modules for True Combat: Elite.
The current mod version is **1.0.3**. A compatible external Enemy Territory /
ETLegacy engine and an existing original True Combat: Elite installation are
required. This repository builds the mod, not an engine. `pak` contains only
new or changed project resources. `pak_original_tce` is a local-only reference
and is excluded from Git and source archives.

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
or release-event triggers. The public mod ZIP contains **one `tce2-1.0.3.pk3`**:
the new/changed scripts, menus, artwork and UI/CGame binaries are inside that PK3.
Unchanged original game resources stay in the installed original TCE packages.
The ZIP's `server` directory contains the loose QGame modules and their matching
server runtime for each architecture. The optional Editor ZIP is separate and
uses the same one-PK3 layout. Each architecture needs a matching external host.
No GitHub release is published by the workflow.

## Source layout

| Directory | Contents |
| --- | --- |
| `src/` | Game, client, UI, Lua modules and tests |
| `pak/` | Editable new/changed resources for the single mod PK3 |
| `compat/` | Reviewed legacy numeric and ABI compatibility |
| `platform/` | Native platform services |
| `vendor/` | Dependency source, bot scripts and attribution |
| `omnibot/` | Bot build integration |
| `navstudio/` | Separate editor, BSP/Recast tools and desktop packaging |
| `tools/` | Build, release and validation tools; retained UI design sources |
| `Docs/` | Maintained audit and release documentation |

Local original resources (`pak_original_tce`), game installations, reference binaries, reconstruction evidence,
credentials and generated build files are excluded from version control.
Source-file copyright and license notices and vendored dependency licenses
remain applicable; see also [LICENSE](LICENSE).
