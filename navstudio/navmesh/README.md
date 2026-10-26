# BSP to Omni-bot navigation

The offline tools import collision geometry, build standing/crouching Recast
surfaces and sample distributed positions over their free space. Local links
offer several useful directions; narrow doors and bends get supplementary
anchors. The loaded
TCE2 server then checks its actual player hull, floors and every directed walk
before writing Omni-bot WAY v9. No WPS or waypoint-per-voxel export is used.

First load the intended map in the existing test instance with current qagame
and run `tce_navgen_entities` in the server console. This writes
`navgen/<map>_entities.json` under the mod homepath, containing actual BSP model
origins, angles, contents and linkage for the current gametype/script state.

Use the current test directory; do not copy or install another game instance:

```powershell
python navstudio/workers/tce_bsp_navmesh.py build/navgen-overkill/etmain/obj_overkill.pk3 --map obj_overkill --entities build/navgen-overkill/tce2/navgen/obj_overkill_entities.json --out-dir build/navgen-overkill/navmesh-preview
cmake -S navstudio/navmesh -B build/navgen-overkill/navmesh-build -A x64
cmake --build build/navgen-overkill/navmesh-build --config Release --target tce_navmesh_preview
build/navgen-overkill/navmesh-build/Release/tce_navmesh_preview.exe build/navgen-overkill/navmesh-preview/obj_overkill.collision.obj build/navgen-overkill/navmesh-preview/obj_overkill.stand.json 87 4
build/navgen-overkill/navmesh-build/Release/tce_navmesh_preview.exe build/navgen-overkill/navmesh-preview/obj_overkill.collision.obj build/navgen-overkill/navmesh-preview/obj_overkill.duck.json 61 4
python navstudio/navmesh/waygraph.py --stand build/navgen-overkill/navmesh-preview/obj_overkill.stand.json --duck build/navgen-overkill/navmesh-preview/obj_overkill.duck.json --geometry build/navgen-overkill/navmesh-preview/obj_overkill.geometry.json --out build/navgen-overkill/navmesh-preview/obj_overkill_candidate.txt
Copy-Item build/navgen-overkill/navmesh-preview/obj_overkill_candidate.txt build/navgen-overkill/tce2/navgen/
```

On the same loaded server, with bots idle or absent, run
`tce_navgen_import navgen/obj_overkill_candidate.txt`. Collision checks may
stall simulation. Wait for completion and a freshly written
`navgen/<map>_navmesh.json`/`.way`; failed imports leave older output untouched.

If native checks reject shortcuts, recover extra mesh bends with:

```powershell
python navstudio/navmesh/waygraph.py --stand build/navgen-overkill/navmesh-preview/obj_overkill.stand.json --duck build/navgen-overkill/navmesh-preview/obj_overkill.duck.json --geometry build/navgen-overkill/navmesh-preview/obj_overkill.geometry.json --base-candidate build/navgen-overkill/navmesh-preview/obj_overkill_candidate.json --native-feedback build/navgen-overkill/tce2/navgen/obj_overkill_navmesh.json --out build/navgen-overkill/navmesh-preview/obj_overkill_candidate_v2.txt
Copy-Item build/navgen-overkill/navmesh-preview/obj_overkill_candidate_v2.txt build/navgen-overkill/tce2/navgen/
```

Import the new candidate using `tce_navgen_import navgen/obj_overkill_candidate_v2.txt`.
The base candidate and its native report must belong together. Then audit:

```powershell
python navstudio/workers/tce_navgen.py inspect build/navgen-overkill/tce2/navgen/obj_overkill_navmesh.json
python navstudio/navmesh/report.py build/navgen-overkill/navmesh-preview/obj_overkill.stand.json --way-report build/navgen-overkill/tce2/navgen/obj_overkill_navmesh.json --geometry build/navgen-overkill/navmesh-preview/obj_overkill.geometry.json --out build/navgen-overkill/navmesh-preview/obj_overkill.preview.html
```

`tce_navgen.py install --help` describes explicit installation with a backup.
Generation does not overwrite active navigation. Test regional bot routes and
combat with waypoint display in the existing instance before deployment.

CMake fetches pinned upstream Recast v1.6.0 (zlib license), without demos, tests,
or editor dependencies. Only the offline CPU geometry builder is linked.
The preview driver defaults to the TCE `g_newbbox 1` radius16 profile with
standing height87, crouched height61 and step18. `obj_delta` and `obj_stadtrand`
set `g_newbbox 0` in worldspawn: use heights70/49 and append `--radius 14` for
both Recast passes; the native player origin is24 rather than30 units above
its feet. Use the fresh runtime map profile, not a forced global override.
`--radius 16` is explicit but optional; either radius option can be combined
with `--tile-size`. Recast conservatively rounds radius up to whole cells:
at4-unit resolution both14 and16 occupy4 erosion cells. It uses a circular clearance
approximation; final game validation must use the actual TCE collision hull.
A 4-unit cell / 2-unit vertical resolution is an initial precision setting,
not a guarantee of narrow-passage parity. Maps up to 8M grid cells keep the
single-field build; larger maps automatically use 512-cell tiles at the same
resolution. Each tile has a `walkableRadius + 3` cell collision border. Its Y
bounds come from triangle portions clipped to that padded XY footprint and
are expanded onto the common 2-unit height grid. No arbitrary height clipping
or resolution reduction is used. Small disconnected regions are retained for
inspection instead of silently discarded. Only the native gate emits WAY files. Sky/slippery surface flags are retained in the OBJ material
markers and excluded from initial walkability classification.

Some BSPs contain real collision brushes that extend far beyond their playable
model bounds (for example, Stadtrand's narrow pipes reach height65536). For
those inputs the exporter option `--vertical-raster-domain` records an explicit
`vertical_raster_domain` from world-model bounds, transformed active inline
models and valid spawns, plus128 units of hull/step padding. It does not change
the collision OBJ. Pass that manifest's exact `minimum` and `maximum` to both
Recast passes using `--height-domain MIN MAX`. The values use BSP-Z / OBJ-Y.
Only the raster working interval is intersected: original triangle vertices,
normals and material flags remain unchanged, with no new horizontal caps.
The mesh `height_domain` ledger records source/requested/raster bounds and
excluded/crossing triangle counts. An empty interval, empty traversable result
or excessive remaining span fails explicitly. Without this opt-in, previous
build behavior remains unchanged; the domain does not certify native movement.

An exactly collinear, zero-area terminal polygon with at most one incident
neighbor is removed with an explicit `degenerate_cleanup` ledger and remapped
adjacency. Positive-area polygons are not removed by that pass. If contour
simplification collapses a corridor into a zero-area polygon with multiple
neighbors, the affected tile is rebuilt from the same compact heightfield with
bounded contour errors 1.1, 0.9, then 0.5. The `contour_refinement` ledger records
the original problem coordinates and every attempt's error and polygon count.
This changes contour precision, not raster resolution or collision clearance.
Remaining nonleaf degeneracies or non-collinear zero-area polygons fail before
writing a mesh; no connecting edge is invented across them.

Before surface sampling, `waygraph.py` applies two separate negative filters:
BSP visibility-leaf membership and static WORLD liquid contents. The latter
uses the hash-verified BSP's world-model brush range and plane equations,
with the same water/lava/slime contents mask used by native `NG_Ground`.
Native grounded wading is allowed when feet+18 units are dry. Accordingly,
water is excluded only if one brush contains the entire possible foot range
and dry-probe range (-20 through +38 units relative to the Recast sample).
Lava/slime retain the stricter exclusion of the foot range (-20 through +20).
Brush boundaries, shallow or uncertain samples, and dry ledges are retained
for native checks. Solid brushes and inline models are not filtered as liquid.
The JSON `world_liquid_filter` ledger records every exclusion with coordinates,
brush ID, contents, BSP SHA and reason; excluded liquid polygon centers are
listed separately from outside-leaf centers. Original ET waypoints can include
swimming routes that remain outside the current native generator's supported
movement. Retained shallow-water samples still require native hull and route
checks; this filter does not certify wading or swimming.
Spatial format4 candidates allow15,872 input nodes and253,952 directed links;
the matching current qagame reserves512 of its16,384 nodes plus8,192 links for
repairs/actions. Legacy input stays at4,096/65,536. Final text input is limited
to5MiB. Candidate JSON records these limits and whether older8k native storage
is insufficient. Do not import an enlarged candidate into an older qagame.

Tile adjacency is built only from opposite Recast boundary portals on adjacent
tiles. Overlapping intervals are split on both sides; endpoint heights must be
within the 18-unit step limit and every exported seam must be reciprocal.
If multiple surfaces compete for the same portal interval, all polygons are
retained but that interval is left unconnected in both directions. The JSON
`tiling.seam_rejections` array records its world coordinates, polygon IDs and
reason. This can occur where a floor meets an overlying sloped surface.
`unmatched_interior_segments` counts all open interior seam segments, including
`ambiguous_segments`; neither count is a certificate of physical obstruction.
The native stage still has to prove actual movement and any missing connection.

Only one padded tile is processed at a time. Recast allocations have a 256 MiB
cap; this measures Recast allocations, not total process memory. Other explicit
limits include 4,096 tiles, 50,000 output polygons, 500,000 output vertices,
2,000,000 portal-pair comparisons and Recast's 13-bit vertical span per tile.
Limits fail explicitly instead of dropping regions. The JSON `tiling` object
reports tile counts, the maximum padded cell count, peak Recast allocations
and seam diagnostics. The optional argument `--tile-size 32..512` forces
a tiled build and can also resolve high span-memory use below the8M cell
threshold (Railhouse needs512). It leaves collision resolution unchanged.
Output remains a geometry candidate and retains the existing
`vertices` / `polygons` / `neighbors` format consumed by `waygraph.py`.

## Implemented checks and remaining limits

The importer clips brushes, adaptively subdivides collision patch grids and
transforms colliding inline BSP models. Trigger-only/unlinked models are
deliberately excluded. Malformed bounded brushes use conservative axial boxes,
recorded separately in the manifest. These boxes and patch facet bevel/float
details are approximations, not exact collision parity. Native checks remain
the authority for accepted movement.

Automatic candidates use `TCE_NAVMESH 4`. Sampling depends on free surface area,
not the number of Recast polygons. Native validation preserves these spatial
samples instead of applying the older polygon-anchor contraction pipeline.
Crossing links and overlapping arrival radii are not rejected just for looking
that way: each directed movement must pass the actual hull/floor tests.

The native gate bounds input/trace work, independently checks spawn anchors and
uses bounded component bridges and fine doorway searches. Native feedback can
add true corner approaches while preserving all existing candidate IDs. The
report accounts separately for floor rejects, exported input positions and
valid-floor positions without a proved connection. Such positions remain
quarantined, not declared physically unreachable. Missing-region sizes,
bounds, rejected links and search attempts are available for follow-up.
`tce_navgen.py inspect` reconciles this ledger and rejects silent sample loss.

Runtime entities are a snapshot, not all mover/destruction states. Dynamic and
destructible floor supports are rejected. The native stage discovers ladder
mounts/exits and short ballistic jumps, validating their action-specific hull
paths before export. Water traversal, arbitrary deep drops and all scripted
door states are not covered. Complete map coverage and every gametype are
not certified. Test exported actions with real bots before deployment.

The HTML comparison is a diagnostic view, not a coverage certificate. Upper
roofs, brush interiors, incomplete collision imports and closed scripted doors
can produce geometry regions that are not playable. A connected graph alone
cannot prove that all rooms are covered, nor does fewer waypoints prove better
navigation. Source: https://github.com/recastnavigation/recastnavigation
