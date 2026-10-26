# Dead and unwired code audit

Audit date: 2026-10-26. Scope: the TC:E mod, its configured module targets,
optional tooling and local build output. This is not an engine audit.

## Method and evidence

`tools/analysis/inventory.py` compares the actual configured `qagame`, `cgame`
and `ui` source lists with the mod source tree. The initial observation covered
12 Game, NavStudio and x64 module projects. It also measures logical file sizes
without following symlinks or Windows reparse points. It never deletes inputs.

`tools/analysis/clang_unused.py` uses the installed Zig package's embedded
Clang 18.1.6 with each Game module's Release definitions and include paths. It
executes `clang -fsyntax-only -Wunused-function -Wunused-variable
-Wunreachable-code`, generating diagnostics rather than objects or binaries.
The existing Visual Studio installation also contains clang-tidy 19.1.5.

The initial compiler pass examined 250 C translation-unit/module combinations:
246 parsed successfully. The four parse failures are explicit toolchain limits:
the existing MSVC naked `BoxOnPlaneSide_fast` implementation in `q_math.c`
(three module contexts), and the MSVC `_div64` intrinsic in `cg_effects.c`.
Those sources were not rewritten to make the audit appear successful.

There were 24 unused-function diagnostics representing 21 distinct private
functions. These are observations of one preprocessor profile, not a list of
safe removals. No function is classified dead merely because a text search
finds no callers. VM exports, callback tables, script registrations, other
platforms and optional build flags remain separate reachability roots.

## Removed after review

| Source | Evidence and bounded change |
| --- | --- |
| `src/cgame/cg_panelhandling.c` | Nine old `CG_Panel*` definitions and their private focus pointer were absent from every observed module project, current CMake lists, legacy `cgame.vcproj` and `SConscript.cgame`. Current callers use the active `BG_Panel*` implementation. The module export file contains only `vmMain` and `dllEntry`. The unused file was removed; the external original `ET_SDK` copy remains intact. |
| `src/cgame/cg_limbopanel.c` | Clang confirmed the private `TCE_LimboWidget` wrapper was unreferenced. An all-source/target-branch review showed its five small private adapters and one pointer belonged exclusively to that unused wrapper. Only that six-function cluster and pointer were removed. Active Limbo rendering functions and public declarations remain unchanged. |
| `src/assets/omni-bot/et/scripts/goals/goal_checkstuck.gm` | Obsolete, uninstalled script copy with a five-second stuck timeout. Current runtime staging explicitly uses `vendor/omni-bot/runtime` plus the `vendor/omni-bot/tce2-scripts` overlay; the active overlay has a thirty-second timeout and the tested collision/traffic corrections. No current build, release or desktop resource input refers to this assets copy. It was archived and removed; the active vendor script was not changed. |

The removed panel file was 7,158 bytes, SHA-256
`a7b8b3224336eaea6f8bdce653584279d491ff3af829e493f98eb435dd929ab4`.
The Limbo source before removal was SHA-256
`58bebb249d0e470a94d3b09e8247135bb5a295e03d4af174b8e32f3d174b57cc`;
after removal it is
`9befa2f7c5f372c7053ce9b5cbc37c797cebba26bdb25033a36b52b12072206b`.
The removed 6,860-byte stuck-script copy was SHA-256
`3ab3bf0173f6a2a2ed5e827f5e5a5614bc9daf6758ac78ce51c1f432783fa2ae`;
the retained active script was SHA-256
`75925eff23fcfc73697d5483bb19d16a6a33c019735b39567b1c8c1084913ee8`.
The modified translation unit parses with zero warnings in the same Clang
profile. The post-removal inventory reports one unlisted source, `bg_lib.c`,
and no configured missing input caused by these removals.

Byte-preserving source archives, removal hashes and the complete Limbo diff
are retained locally under
`src/reconstruction/dead-code-audit/20261026/`. They are local evidence, not
additional production sources.

## Deliberately retained

| Category | Examples and reason |
| --- | --- |
| Target support | `src/game/bg_lib.c` supplies the historical QVM C-library replacement. Its absence from native DLL builds does not establish that its compatibility purpose is obsolete. |
| Platform implementations | `dot`, `clampSway` and `posture` in `tce_weapon_position.c`, `interpolate` in `tce_trajectory.c`, and `PM_TCECheckProne` are unused in the observed Windows profile but called from other preprocessor branches. |
| Optional features | Lua, the Omni-bot gameplay bridge/core, NavStudio and editor components have distinct build gates. A default build excluding one does not make its sources dead. |
| ABI and dispatch roots | `dllEntry`, `vmMain`, engine import wrappers, function-pointer tables, script actions, Lua bindings and Omni-bot registrations can be entered without a direct C call. |
| Unwired legacy UI | Clang identified unused static command, HUD, scoreboard, breath and older weapon-file helpers. These remain review candidates; no broader removal was justified in this pass. |
| Tests and reference data | Original DLL comparisons, platform fixtures, authenticated binaries, source snapshots and reference SDKs are validation inputs, not unused gameplay code. |

## Disk inventory and cleanup boundaries

At the initial snapshot, `build/` contained about 20.99 GiB of logical bytes.
The largest inspected children included:

| Directory | Logical GiB | Classification |
| --- | ---: | --- |
| `build/navgen-overkill` | 3.898 | Sole authorized game installation; preserve. |
| `build/sdk-win32` | 1.443 | Mixed output; about 0.980 GiB in object/debug/static-archive extensions. Preserve final modules, symbol maps and final PDBs; inspect individual intermediates. |
| `build/source-snapshots` | 1.378 | Unique historical source/evidence; preserve or archive with approval. |
| `build/navgen-railhouse` | 0.810 | Old game/asset copy containing PK3s, not a pure compiler cache. |
| `build/parity-win32` | 0.779 | Generated comparison build; about 0.416 GiB in object/debug/static-archive extensions. Preserve comparison reports and provenance before cleanup. |
| `build/modernization-validation` | 0.690 | Generated matrix output; about 0.515 GiB in those intermediate/debug/archive extensions. Preserve reports and final outputs. |
| `build/corona-runtime` | 0.653 | Runtime/asset copy containing PK3s, not an automatic deletion candidate. |
| `build/sdk-binary-baseline` | 0.293 | Historical binary baseline; preserve. |
| `build/linux-toolchain` | 0.288 | Required existing compiler; preserve. |

These are logical sizes, not guaranteed reclaimable space. Hard links can be
counted more than once. A directory containing a CMake cache may also contain
unique logs, saved source or runtime assets. This audit did not delete build
directories or game data. Cleanup is a separate explicit operation with path
checks and provenance preservation; important originals or unique archives
require approval before deletion.

## Reproduce

```powershell
python tools/analysis/inventory.py --scan-disk --out build/llvm-audit-inventory/inventory.json
python tools/analysis/clang_unused.py --project-dir build/sdk-win32 --out build/llvm-audit-inventory/clang-game --jobs 2
```

Full local commands, source/project hashes and compiler diagnostics are in
`build/llvm-audit-inventory/`. Sizes and configured source lists are snapshots;
rerun after changing feature flags or cleaning generated projects. The audit
does not claim complete interprocedural reachability or whole-game parity.

The [official LLVM Getting Started guide](https://llvm.org/docs/GettingStarted.html)
describes the toolchain and recommends selecting tools/targets when disk space
is constrained. Existing tools were reused; no LLVM source checkout or large
LLVM build was needed for this pass.
