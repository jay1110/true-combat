# Optional exact skin/ray validation

These tools build only `tce_pose_math.c` and the focused fixture. They never
build, load or deploy a game module. No engine source is imported. The exact
surface API remains isolated from the active damage/server path.

Generate local expectations from the existing installation and six test-only
renderer captures, then run the actual C helper:

```text
python tools/modernize/pose_skin_fixtures.py --out build/pose-skin-fixtures
python tools/modernize/pose_skin_regression.py --fixtures build/pose-skin-fixtures --out build/pose-skin-check --optimized
```

The generator accepts explicit `--mod-pk3`, `--base-pk3` and `--captures` paths.
Original assets and raw captures are external test data, not public release
inputs. It produces compact MDX, the original MDM/MD3, full expected vertex
coordinates, 180 pose tuples and 737 reference rays. Do not ship these files.
The independent Python bone parser/profile is in `pose_eval_fixtures.py`; head
parsing, skin weighting and finite triangle intersection are independent Python
implementations in the new generator. No ignored prototype is imported.

On Windows, the runner uses native MSVC x86; `--sanitize` uses x64 AddressSanitizer.
On native Linux use `--compiler gcc` or `--compiler clang`; `--sanitize` adds
AddressSanitizer and UndefinedBehaviorSanitizer. All optimized builds disable
floating-point contraction or use MSVC precise semantics. Compiler logs and
source/data hashes are retained in the selected output directory.

The fixture checks all 1,365 vertices for 176 actual captured poses and four
explicit frozen climbing frames; all 401 original MD3 face frames; geometry
regions;737 independent rays; conservative broadphase equivalence; empty leg
gaps and opaque-cover clamps; reversed bone indices; checked malformed assets;
cache/vertex/weight limits; and triangle endpoints, shared edges and coplanarity.
Timing uses200 evaluations and 1,000 pairs of cached real 1,990-triangle surfaces
(3,980 triangles per unfiltered pair). It is a bounded CPU microbenchmark, not
game/server performance acceptance.

API semantics are explicit: MDM `u_*` surfaces map BODY; `l_*` triangle weights
are summed by named thigh ancestry, with majority leg weight mapping LEGS and
ties mapping BODY. Dedicated MD3 surfaces map HEAD using supplied head origin,
axis and facial interpolation. These are anatomy labels, not established legacy
damage-weight parity. Full base topology is tested, without renderer LOD/shader
visibility filtering. Raycasting is two-sided, finite and inclusive at the
world-cover limit; coplanar triangles miss and a ray starting inside finds its
exit. It does not infer a closed solid or an automatic start-inside hit.

Optional `--bound-capture <json>` arguments append normal-CGame read-only inputs
after the original 180 cases. `pose_skin_bound_inputs.py` requires the coherent
typed selected CGame character, draw eligibility, matching body/head handles,
the installed character's MDM/MD3 paths and named animation-group mapping for
every used MDX handle. Missing/ambiguous old handles, special character overrides
and mismatched assets fail instead of borrowing server registry associations.
All supplied scaled axes, head transforms and facial frames are preserved.

Run `pose_skin_bound_inputs_test.py --capture <json> --out <report.json>` to
exercise real inputs and invalid binding controls. Appended cases each get an
independent head-surface ray and a synthetic cover limit before its nearest
surface, alongside the original ray/leg-gap controls. These current normal
captures supply actual frame/axis/head inputs; they are not measured skinned
vertices or tags. Their phase/ladder counts and file/module hashes are recorded
separately in the generated manifest. The six earlier measured-tag captures
remain an independent gate, and default generation retains its original matrix.

The server must still establish animation phase, body swing, head-relative
orientation, face phase and historical pose generation before using this API.
An active provider also needs explicit region/damage-weight and world-hit tie
policies plus actual matched client/server shot acceptance. This fixture does
not activate such a provider or claim H3 completion.
