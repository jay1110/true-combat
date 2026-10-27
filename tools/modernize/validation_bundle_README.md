# Source-bound validation bundle

`validate_mod.py --strict-release` is the final acceptance entrypoint. The old
CLI and schema-1 reports remain available for historical callers; those reports
are not strict release acceptance. In particular, the old Windows recipe that
disabled Omni-bot does not meet the current Game policy.

Review the plan first (this executes no checks and writes no output):

```powershell
python tools/modernize/validate_mod.py --strict-release --plan --build-map C:/local/validation-map.json --baseline-dir C:/local/baseline --out C:/local/validation-final
```

Remove `--plan` to execute the ordered bundle. The runner never downloads a
toolchain, creates a game installation, discovers credentials, deploys a module,
or implicitly starts a game/VM. Explicit external commands are caller-reviewed;
do not put credentials in their arguments, environment, or map. Runtime drivers
should keep using their private credential reader.

Use `--resume` only for interrupted/failed steps with the same inputs. Passed
steps are reused only when their recorded artifacts and dependencies still
match. A changed source, map, tool, environment or passed artifact requires a new
output directory. `--verify` checks the frozen bundle without running commands;
it reports stale/missing evidence as failure. It cannot turn a stale report back
into a valid build merely by rerunning a binary string check.

The strict report retains `steps`, `command`, `returncode`, `log` and `passed`
where applicable, with schema 2, explicit category/status and source freeze.
`INCOMPLETE`, `FAIL` and `STALE` never count as successful required gates. Logs,
reports, generated fixtures, actual module/core bytes, matching linker maps,
CMake cache/compiler metadata, and CTest executables/fixtures are hashed. All
passed artifacts and source inputs are checked again at the end. These hashes
detect stale or changed evidence; they are not cryptographic authorship
signatures. Exported C signatures are checked separately by `symbol_diff.py`.

## Map contract

The JSON map has `schema: 1`. Paths are absolute or repository-relative;
`{root}`, `{out}`, and `{python}` expand in mapped paths and external commands.
Start with the deliberately incomplete `validation_map.example.json`; replace
the placeholders with existing owned trees and inputs. An incomplete map can be
planned, but cannot pass validation.

`baseline_dir` optionally supplies the existing baseline directory when
`--baseline-dir` is absent. `source_reviews` (or explicit `--reviews`) selects the separately root-reviewed approval input
for `source_diff.py --reviews`. Absence grants no exceptions. Its schema1
contains `baseline_inventory_sha256`, a `proofs` mapping (description plus
optional `{path,sha256}` input bindings), `functions`, and `removed_files`.
Each function decision requires `path`, `name`, zero-based `ordinal`,
`current_path`, exact `baseline_normalized_sha256`, explicit
`current_normalized_sha256` (null only for removal), `reason`, and nonempty
`proof_ids`. Each removed-file decision requires `path`, exact
`baseline_sha256`, `reason`, and `proof_ids`. Unknown/changed proof inputs,
stale hashes, duplicate or unmatched decisions and unreviewed deltas block the
gate. No source-name exception exists. The tool does not generate approvals or
replace the baseline. Newly added functions/files still require the separate
current risk/fixture/IR review; baseline identity alone does not cover them.
The selected manifest and every explicitly listed proof input are part of the
global freeze, including local ignored reports. A later proof edit invalidates
the bundle even if its source-diff gate already passed.

- `tool_inputs`: explicit compiler/linker executables and SDK/toolchain identity
  files. Include every actual tool version selected by the existing runners
  (for example both their MSVC 14.39 and 14.44 compilers), plus external `nm`
  tools. These bytes are frozen in addition to the executed command tools.
- `builds`: each row has a unique `name`, `role`, existing `directory`, complete
  `configure_args`, `configuration`, `abi`, and optional `env`, `linker_map`,
  `nm`, `core`, `sanitizers`, and `runnable`. Required roles are `debug`,
  `release`, `asan`, and at least one distinct `matrix` ABI. `abi` contains
  `system`, `architecture` (the generated runtime manifest's spelling, e.g.
  `i386`, `x86_64`, `armv7`, `aarch64`), and `pointer_bytes` (4 or 8).
  Debug/ASan require `Debug`; Release requires `Release`. Single-config trees
  must configure the matching `CMAKE_BUILD_TYPE`. Keep distinct configurations
  in distinct single-config trees. `env` is only the additional required build
  environment, never a credential store.
- `assets`: `mod_pk3`, `base_pk3`, `captures` directory containing the six
  original renderer JSONL captures, `bound_captures` list of explicitly bound
  normal-CGame JSON captures, `full_assets` containing the ten original MDX
  files and `body.mdm`, and `installation` pointing to the sole existing game
  installation for an offline PK3 script scan. No game process is touched.
- `gates`: explicit `critical-ir-review`, `native-compiler-matrix`,
  `package-closure`, and `runtime-smoke`
  commands, each with `inputs` and regenerated `outputs`; optional `env` and
  `evidence` bind additional artifacts produced by preceding build steps.
  These are required, not optional skip slots. The caller must review each
  command's semantic contract, not substitute a successful echo command.

Build rows run `cmake --fresh` in the explicitly mapped existing tree, force
normal Omni-bot/core ON and authoring/reference tests OFF, then run a clean
build of all default targets. Supply the complete generator/compiler/toolchain
arguments needed after a fresh cache; the runner does not guess them. ASan is
explicitly instrumented. Generated CMake metadata checks system/pointer width;
the fresh runtime manifest checks architecture/variant and selects the exact
core. A supplied `core` is an additional equality assertion, not an alternate
library to validate. Module paths come from the emitted configuration manifest.
Strict execution requires CMake 3.24 or newer for `--fresh`. Use separate trees
where core library/map outputs collide between Debug and Release, even with a
multi-config generator; overwritten evidence is correctly rejected as stale.

External `inputs` are immutable files that exist before the pass, such as the
driver, capture, or review policy. Files generated by preceding jobs belong in
`evidence`, not `inputs`; they are bound after execution. A missing frozen input
cannot become a passing gate by appearing partway through the pass.

Runnable native builds must discover and pass all seven portable CTests,
including both syscall/vector compatibility goldens and protocol/presentation
checks. An empty CTest run cannot pass. Cross `matrix` builds are build-only
unless explicitly `runnable`; no native execution is inferred from a Linux
preprocessor branch on Windows. Windows-only fixture runners remain required
but unavailable on other hosts; this entrypoint does not invent remote proof.
Caller-reviewed native runs can be orchestrated in the mapped workflow, but
must preserve these exact source/tool/input closures and identify the real OS.

## Included gates and remaining evidence

The catalog includes risk/classification, normalized compatibility hashes,
baseline function-body diff, six SDK entry signatures and module exports,
vendor locks, release-source/public-boundary policy, release unit tests, and
orchestration tests. Historical intentional body changes need their reviewed
baseline; a different hash is not silently accepted as parity.

It generates pose evaluator/skin/character-family inputs from explicit assets
and captures, then runs every current `*_regression.py`, lifecycle/UI/weather
probes, clock, compact codec, presentation/history, full skin/adapter, and
compatibility goldens. Runners supporting Debug/O2/ASan get all three. Runners
with a single fixed optimization profile use `default` plus ASan, never a
mislabelled duplicate Debug/Release run. The coverage test fails if a new
regression runner is omitted. Mover-origin uses `--verify-fixed` to check the
affected-path correction. Its separate `--require-canonical` mode documents an
intentional mismatch in the untouched ordinary path and is not the fixed gate.

`critical-ir-review` must preserve the reviewed high-risk function decisions
and normalized hashes, compare IR/disassembly where relevant, and explicitly
justify any not-applicable comparison. `package-closure` must validate the
final target archives, full shared client ABI closure, exact Game/Editor split,
and provenance against the newly built module bytes. Use the existing release
validators and packaging helpers; their synthetic tests alone are not proof of
final artifact freshness. The archive validator's input directory must contain
only target ZIPs, not source/shared-client/aggregate ZIPs. Bind final ZIPs and
matching build/provenance reports as evidence.

The generic `critical_ir_review.py` adapter accepts explicit `--baseline`,
`--risk`, `--reviews`, `--policy`, `--baseline-source`, `--bundle-out`, and `--out`.
Its local policy binds the reviewed114 differences, every newly indexed
function identity/file hash, and the explicitly supplemental active damage
include. Each decision declares comparison or a source-backed not-applicable
rationale, proof inputs and required fresh job IDs. It rechecks current indexing
and the same bundle's source-bound fixture receipts/artifact bytes before
compiling12 arithmetic pairs in real MSVC Win32 Debug/O2. It preserves actual
object disassembly, emitted helper listings, differences, compiler/include
hashes and small runtime controls. Compiler output differences are observations,
not opcode equality or full numerical parity. `--arithmetic-only` is explicitly
diagnostic and cannot supply the critical gate. Freeze the policy and all its
proof inputs through the mapped gate's `inputs`; no local policy ships by default.

`native-compiler-matrix` must validate the selected source-bound fixtures using
real native GCC and Clang, including optimized and sanitizer runs, and preserve
the exact transferred input/asset/tool hashes. Cross-compilation or old green
logs cannot fill this gate. The command is explicit because the local machine
does not supply every native ABI; no SSH transport is silently introduced here.

`runtime-smoke` must verify startup/map/game evidence against the actual built
module hashes, including public bot-help boundary evidence where required.
Old screenshots/captures are not automatically promoted to new-source live
acceptance. A fixture pass never establishes render, network, map or gameplay
parity. This entrypoint deliberately leaves these gates incomplete until their
real checked commands and inputs are supplied.

The executor stores hashes rather than copies of large inputs or build trees.
It reuses mapped trees and writes each tiny fixture into its own output
directory. The operator must budget existing-tree build outputs and available
space before launching the full pass; this tool performs no automatic cleanup.

The small orchestration selftest needs no compiler, SDK, network or game:

```powershell
python -m unittest discover -s tools/modernize -p test_validation_*.py -v
```
