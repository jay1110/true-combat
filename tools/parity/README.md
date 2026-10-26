# Runtime comparison

These tools keep original-binary comparisons, intentional extensions, portable
contracts and live-engine observations distinct. A successful CMake build is
not a parity result. Original DLL hashes are authenticated before address-based
tests. The historical comparison suite also requires local reference fixtures
and binaries; it is deliberately opt-in.

`inventory.py` lists actual registered reference tests; `run.py --help` describes
the bundled build/CTest runner. The runner records source/executable hashes and
refuses a current-source parity claim if inputs change during execution.

`--all-reference` excludes tests labeled `accepted-extension` by default. Use
`--include-accepted-extensions` to run those additional contracts in the same
bundle, or select their names explicitly with `--tests`. Reports keep accepted
extensions separate from strict original equality; passing a prefix/suffix
contract does not make the intentional extension byte-identical to the original.
`--include-portable-contracts` adds the portable GeoIP/score-packet checks to the
same bundle, with a third, separate result count.

Additional focused checks:

```powershell
python tools/modernize/compat_validate.py --msvc-version 14.39
python tools/modernize/compat_word_probe.py
python tools/modernize/animation_probe.py --help
python tools/runtime_parity/compare_pmove.py --help
```

The portable `tce_geoip_test` and `tce_score_packet_test` targets are part of
ordinary `BUILD_TESTING`. They test malformed input, atomic updates and unique
score rows for up to 64 clients, independently of original-format equality.

The Windows-only live tools are local test helpers for the **one existing**
`build/navgen-overkill` installation, with local RCON on port 27986. They never
create a game installation. `live_compare.py` explicitly swaps selected DLLs
after stopping the previous game; originals under `tcetest` remain untouched.
It backs up previously installed modules outside the engine search path.

`live_scenario.py` waits for a real match and healthy actor, then records fire,
reload and jump states plus engine screenshots. `live_fps.py` measures repeated
primitive-path trials in one stationary view and always restores `r_primitives
2`. `live_fpu.py` briefly suspends each selected game thread solely to read its
WOW64 floating-point control word and always resumes it in `finally`; it never
writes context or game memory. These helpers do not take keyboard/mouse control.

Live input and snapshots are asynchronous. Compare recorded states/velocities
and actual frame counts, rather than claiming equality from an FPS cap or a
single screenshot. VM software rendering is not a host GPU performance test.
Native 64-bit mod libraries require a matching external engine; the original
ET 2.60b executable is 32-bit, including when Windows itself is 64-bit.
