# Manual release builds

All workflows use **only `workflow_dispatch`**. Pushes, pull requests, tags and
schedules do not start builds. Select branch `final` under Actions → **Manual
TCE2 release binaries** → **Run workflow**, or use:

```sh
gh workflow run release.yml --ref final -f expected_version=1.0.2 -f include_editor=true -f include_desktop=true -f publish_release=false
```

The workflow must exist on the repository's default branch before GitHub exposes
manual dispatch. See GitHub's [manual-run documentation](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow).
Publishing is disabled by default. Selecting `publish_release=true` explicitly
creates `v<version>` from the dispatched commit only after every selected build,
archive validation and aggregate job succeeds. It does not change repository
settings or publish on push.

`targets.json` enumerates Windows x86/x64, Linux i386/x86_64/ARMv7/AArch64,
Android ARMv7/ARM64, WASM32 and macOS universal. Every target attempts both mod
modules and a matching Omni-bot core. The default builds public gameplay and
separate authoring variants. No target is silently excluded: a concrete compiler
failure keeps its logs and, where available, a clearly named **Modules-Only-Client-Inputs** ZIP,
then fails that job. Such an archive is not a complete bot runtime. Successful
compilation does not establish cross-target engine compatibility or parity.

Matrix jobs produce **Client-Inputs** ZIPs for aggregation, without a provisional
download PK3. Only the full collection of all ten target ABIs is promoted to
installable target ZIPs. Aggregation creates one deterministic `tce2-1.0.2.pk3`
per variant containing all twenty native cgame/UI modules and matched resources.
The two user-provided Delta overview/blueprint images and their source hashes are
included in both variants; maps without a supplied overview keep the map fallback.
Every target ZIP of that variant receives the exact same PK3 bytes, so a Linux
server serves the current Windows client pair as well. Version, variant, Release
configuration, native ABI headers, resource equality and module editor boundaries
are checked before promotion. Shared client download ZIPs are included separately
in the aggregate. Game and Editor PK3s remain in separate variant packages.

The final public ZIP contains three target mod libraries, `tce2-1.0.2.pk3`, its client version
manifest, gameplay Omni-bot core/scripts/config, six existing standard-map WAY
files with GT2/GT5 goals and mapscripts, Lua examples/WolfAdmin and notices.
Execute `tce2/omnibot-release.cfg` before starting a map; automatic core discovery
uses `fs_homepath`/`fs_basepath`. Configure bot limits in
`tce2/omni-bot/config/omni-bot.cfg`. No automatic bot count is forced. Navigation
is an existing audited data set; full-map/mission completion is explicitly not
claimed. No engine, BSP maps, local test installation, user profile or cache is
bundled.

Authoring DLLs/core and `zz_tce2_navstudio_editor.pk3` are separate. Public script
staging classifies every global and local command registration, including aliases,
and rejects unreviewed additions. It removes authoring/test registrations;
a lexer preserves every retained gameplay/admin statement byte-for-byte. Exact
Studio diagnostic calls are disabled separately, including navigation spam and
the Studio-only map-loaded handshake. Recovery, routes and mission logic remain.
Raw
imports and per-file provenance stay under `vendor/omni-bot/runtime`. CMake's
`TARGET_FILE` manifests locate every module and bot core. Compiled function sets
are checked through MSVC/LLD link maps or native `nm` output, separately for each
universal-binary slice. Symbol records are bound to the core's SHA256 and the
reviewed source registry. Optimized short command strings can be split into code
immediates, so their literal presence is recorded separately. The Game build
also rejects editor lookup literals and navigation diagnostic formats.

The standalone Windows x64 NavStudio app is frozen separately with pinned Python
dependencies and pinned Recast source. `package_preflight.py` checks its complete
resource closure. Other target ZIPs contain mod/core libraries, not an untested
desktop application for those operating systems. No desktop app is started in CI.

Each target produces versioned input ZIPs, SHA256 files, a file manifest and build logs.
The final aggregate includes every promoted target archive, selected desktop package and
tracked build-source ZIP. Partial runs are labeled **INCOMPLETE-Build-Artifacts**
and cannot reach the publish job. Artifacts remain downloadable from the manual
run; explicitly published GitHub releases are independent of artifact retention.

Build inputs come from the checkout. Boost1.85 headers use the
[official archive checksum](https://archives.boost.io/release/1.85.0/source/boost_1_85_0.tar.bz2.json).
Recast is pinned in its CMake declaration, NDK27d is selected through the Android
SDK, and the Emsdk checkout and SDK3.1.74 are pinned in the workflow. Original
TC:E DLLs, Ghidra data, external local paths and historical reference fixtures are
not required. `GeoIP.dat` is an optional explicit local packaging input, never a
client PK3 entry or implicit download. Flag atlas/license/provenance are included
unconditionally; absent country data falls back to unknown/local flags.

Local validation, without rebuilding or starting a game:

```sh
python tools/release/validate.py source --expect-version 1.0.2
python -m unittest discover -s tools/release -p 'test_*.py'
python tools/release/validate.py archives path/to/packaged-zips
python tools/release/validate.py bot-help path/to/actual-console-capture.txt --variant game
```

The last check reads actual `/bot help` output. Game help accepts only reviewed
gameplay/administration names and requires the normal bot controls; aliases are
checked as individual commands. Use `--variant navstudio` for the separate editor.

`build.py`, `package.py` and `source_archive.py --help` expose the same commands
used in CI. Source ZIPs include only tracked build/production inputs and relevant
documentation. Local reconstruction reports, AGENTS instructions, credentials,
original DLLs, engines, build caches and generated packages are excluded.

For an explicit single-target smoke package, `package.py --local-client-pk3`
preserves native client packaging with a **Local-Test** name and metadata; release
aggregation rejects it. Supply `--configuration Debug` for a Debug smoke build.
For independently packaging an installable target, `--shared-client-pk3` accepts
only a validated canonical PK3 spanning the full release matrix and matching the
target's exact cgame/UI bytes. Normal release inputs require CMake's generated
module manifest, including its actual configuration.
