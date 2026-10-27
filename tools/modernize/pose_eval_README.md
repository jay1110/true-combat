# Bounded attachment evaluator

`src/game/tce_pose_math.[ch]` is a mod-owned MDX/MDM parser and pure seven-anchor
evaluator. It is not currently connected to damage, animation state, or a game
target. It changes no SDK structure or syscall and contains no engine code.

Loading validates little-endian field ranges, versions, counts, finite values,
variable surface weights, tag references, and single-root acyclic skeletons.
Bone indices need not be in parent-first order. Attachment preparation resolves
seven tag names and derives their parent closure plus the torso pivot. The
observed assets require 13 of 53 bones. Evaluation works only on that closure;
32 closure bones is a hard limit with an explicit failure result.

The cache allows ten MDX clips, sixteen MDM meshes and eight MiB of borrowed
immutable bytes. The captured character's ten complete clips total 3,758,560
bytes; its body MDM is 62,468 bytes. Limits are conservative bounds for supplied
assets, not a promise to accept every custom asset. Caller-owned buffers and
loaded objects must remain at stable addresses. Loading callbacks run only at
registration. Evaluation performs no allocation, filesystem access or callback.
Any evaluation failure leaves the complete output zero, allowing an explicit
caller fallback. Failed loads never mark the object ready or consume a cache
slot. Callers serialize cache loading and own buffer lifetime/reset handling.

The `OBSERVED_WINDOWS` mode explicitly chooses the measured renderer arithmetic:
4096-entry angle table on interpolated paths, unstored angle intermediates,
one-step fast direction normalization, current-only partial-torso offsets and
the reference old-torso/old-legs same-frame test. Noninterpolated axes use precise
trigonometry. These choices are isolated here; they do not silently change
global math or floating-point state. `ANALYTIC` is a comparison mode, not a
renderer-compatibility default. Observed renderer captures do not establish
another engine/backend's arithmetic or every hypothetical split-frame branch.
Actual clips have differing bone lengths, so compatibility checks names,
parents, weights, flags and torso pivot; current legs own bone distances.
Shipped clips also contain reversed unused bounds. These remain finite-checked
without a new min/max ordering restriction.

## Reproduce the focused test

The generator needs original game assets and six recorded renderer captures.
It has a self-contained independent Python oracle, with no ignored-source
imports in its default path. Asset bytes are not redistributed here. Captures
are explicit inputs; absence of those inputs is not a passing test.

```text
python tools/modernize/pose_eval_fixtures.py --captures <capture-directory> --mod-pk3 <tce2/pak2.pk3> --base-pk3 <etmain/pak0.pk3> --out <fixture-directory>
python tools/modernize/pose_eval_regression.py --fixtures <fixture-directory> --out <debug-output>
python tools/modernize/pose_eval_regression.py --fixtures <fixture-directory> --out <release-output> --optimized
python tools/modernize/pose_eval_regression.py --fixtures <fixture-directory> --out <asan-output> --optimized --sanitize
```

Windows selects MSVC x86 for Debug/optimized and MSVC x64 for ASan. The runner
also supports native non-Windows GCC/Clang with `--compiler` and ASan/UBSan;
those configurations must be run before claiming results for them. Add repeated
`--asset <complete-original.mdx-or-mdm>` to validate full clips in addition to
the compact capture fixtures. The runner writes source hashes, exact compiler
arguments, input hashes, compiler output, exit status and numeric metrics.

Five anchors per frame retain raw captured model-space origins and axes; thighs
are clearly marked independent-oracle-only. The generator preserves complete
bone metadata and selected frame bytes, with original-to-compact frame maps.
The test uses the full production C functions, permutes all bone indices and
replays every pose again, probes each short/table boundary and predecessor
through an actual parsed head channel, rejects every truncated asset prefix,
and checks malformed topology, cache bounds and zero-output fallback.

## Integration gate

Registration and attachment preparation belong at asset/cache initialization.
The evaluator input must contain all four MDX model/frame slots, both
backlerps, body origin/basis and torso basis for the selected shot time.
`result.model` is relative to the body and `result.world` applies the supplied
body basis/origin. Head output is the body attachment tag, not a fitted MD3
head surface or an additional relative head rotation.

There is deliberately no damage-provider switch here. Before enabling one,
establish the authoritative animation phase/history contract and measure the
volume fit against visible poses, including ladder/freeclimb and slope cases.
Unknown phase, missing assets, unsupported geometry or parser failure must
select a deliberate fallback. The 176 captures establish supplied pose math;
they do not establish server/client clock parity, ladder/slope coverage,
head-surface fit, or final damage behavior.
