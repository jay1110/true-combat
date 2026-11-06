# Manual mod builds and single-package delivery

All workflows are `workflow_dispatch` only. They never publish a GitHub release.
This repository contains a mod, not an ET/ETLegacy engine. Local agents do not
start Actions runs; the maintainer chooses when to dispatch a prepared workflow.

The current mod version comes from `src/game/tce_version.h` (1.0.3).
`release.yml` builds the ten ABI families declared in `targets.json`, for Game
and optionally the separate Editor variant. Source and optional NavStudio desktop
have their own workflow artifacts.

The complete Game ZIP uses this target matrix (the optional Editor ZIP uses the
same matrix, with its authoring features compiled separately):

| Operating system | Architectures |
| --- | --- |
| Windows | x86, x64 |
| Linux | i386, x86_64, ARMv7, AArch64 |
| Android | armeabi-v7a, arm64-v8a |
| macOS | universal x86_64 + arm64 |
| WebAssembly | wasm32 |

Each target contributes UI, CGame, loose QGame and its matched Omni-bot runtime.
These are mod binaries for a compatible engine, not bundled engine executables.

## What to download

- `TCE2-1.0.3-Game.zip`: normal gameplay, with authoring compiled out.
- `TCE2-1.0.3-Editor.zip`: explicitly enabled local authoring build, never a public server.

Each ZIP contains exactly one `tce2/tce2-1.0.3.pk3`. It is an update for an
**already installed original TCE 0.49b base**, not a standalone resource bundle.
It contains every new/changed resource from `pak/`, the generated dependency and
version manifests, and UI/CGame for every advertised ABI. Editor additionally
overlays `navstudio/editor-pak/` into the same PK3. It has no second theme or
editor addon PK3.

`tools/assets/base-game-reference.json` records hashes of the actual stock
`pak0.pk3` through `pak4.pk3` and their VFS files. The reader rejects unchanged
stock bytes in either mod resource tree. New project/user assets remain included,
including all twelve custom stock-map compass images; comparison is against real
stock packages, not the historically contaminated local reference folder.

`pak_original_tce/` stays local and untracked. It is excluded from source ZIPs even
if a stale Git index or `--include-untracked` lists it. Neither the runtime reader
nor CI falls back to it. The source artifact includes the mod delta, `map_pics/`,
small version-bound stock/UI signature manifests and build inputs, never the
original resource tree. Editor menu closure can reference stock menu signatures
without publishing or loading their original text during a CI build.

QGame remains loose in `server/<ABI>/`, with each matching Omni-bot core. Shared
bot scripts, Lua and server configuration live in `server/common/`. Client DLLs
are shipped only inside the single PK3, never as duplicate loose ZIP entries.
The bundled launcher can select the local background and launch an existing game;
it does not install or download an engine.

Unpack the selected ZIP, stop the game, then use:

```text
python install.py --target windows-x86 --mod-dir C:/Games/ET/tce2
```

Choose the matching target listed in `manifest.json`. Before changing files, the
installer verifies the required original `pak0..4` package hashes in the existing
mod directory. It then copies the one update PK3, common server files and selected
server ABI. It never downloads or imports a base installation from another folder.
Overwritten files are backed up in a sibling directory outside the mod search path.
Keep the original `pak0..4` packages installed. Move obsolete TCE2-update/theme
PK3s outside the search path after making an external backup.
Game and Editor must never be mixed.

ET/ETLegacy may load an existing loose UI/CGame before reading a PK3 at main-menu
startup. If either loose client file of the selected ABI already exists, the
installer automatically updates **both** from the same PK3 and backs up changed
files. Other ABIs remain untouched. If neither file exists, installation keeps
the clients in the PK3; use `--classic-client` to request first extraction.
This adds no second package or duplicate loose files to the ZIP. Restore backed-up
loose modules as well as the old PK3 when rolling back.

## How packaging is validated

Per-target `*-Client-Inputs.zip` files are internal build transfers, not downloads
for installation. They contain native inputs plus a delta-resource/base-reference fingerprint;
resource payloads are not duplicated in every job.
Aggregation requires every planned target exactly once, validates native ABI and
public/editor boundaries, then reads the `pak/` delta tree into one canonical,
deterministically ordered PK3 per variant. Same-variant downloads therefore use
identical PK3 bytes across all server operating systems.

The aggregate checks its actual ZIP contents, loose QGame ABIs, matched bot cores,
script-command policy, full delta-resource closure, original-base dependencies,
SHA256s and CRCs. Missing/failed jobs
remain explicitly incomplete and never produce a partial installable PK3.
The final Mod-Packages artifact is uploaded only after both assembly and the
source/build closure check succeed. A failed aggregate uploads JSON/Markdown
diagnostics separately, without exposing an intermediate ZIP as an installable
download.
`closure_report.py` binds the final archives and actual build invocations to the
clean workflow checkout. Compiler success and package closure do not establish
behavioral parity or Linux/Android/macOS runtime testing.

```text
python tools/release/validate.py source --expect-version 1.0.3
python -m unittest discover -s tools/release -p "test_*.py"
python tools/release/resource_assets.py --variant game
```

`build.py`, `package.py`, `source_archive.py` and `aggregate.py --help` document the
local entry points. `loose-binaries.yml` remains a separate, explicitly manual
server/developer artifact path; it does not replace the complete mod ZIPs.

For an already compiled local verification subset, `local_bundle.py` writes the
same single-PK3/server layout without persisting a separate PK3 or target ZIPs:

```text
python tools/release/local_bundle.py --inputs build/local-game-inputs.json --variant game --allow-subset --out dist/TCE2-1.0.3-Game.zip --receipt build/local-game-bundle.json
```

The input JSON is a list of objects with `target`, `module_manifest` (the generated
`tce-modules-Release.json`) and `core_manifest` (the matching
`omnibot-runtime-Release.json`). Partial coverage is explicitly recorded as
`local-diagnostic`; it is not a claim that all ten ABIs were built. Without
`--allow-subset`, every target in `targets.json` is required. The helper performs
no build, download or game launch and checks that its native/resource inputs stay
unchanged while packaging.
