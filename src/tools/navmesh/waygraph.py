"""Build sparse Omni-bot *candidates* from standing/crouching Recast meshes.

There is deliberately no WAY writer here. The text output is consumed by the
TCE server's full-hull movement validator before navigation can be installed.
The accessible surface is sampled independently of polygon tessellation, then
joined by local visible neighbours. Polygons remain coverage witnesses, not
mandatory runtime waypoints. Disconnected surfaces remain explicit frontiers.
"""

import argparse
from collections import Counter, defaultdict, deque
import hashlib
import heapq
import json
import math
from pathlib import Path
import re
import struct
import zipfile
try:
    from .objective_contact import capture_contact, plant_contact, plant_perimeter_axes
except ImportError:  # Direct worker script execution.
    from objective_contact import capture_contact, plant_contact, plant_perimeter_axes


EPS = 1e-6
SPATIAL_NODE_LIMIT = 15872  # Native storage16384, with512 slots reserved for actions/repairs.
SPATIAL_DIRECTED_LINK_LIMIT = 253952  # Native16 links/node minus8192 repair links.
CANDIDATE_BYTES_LIMIT = 5*1024*1024
TOPOLOGY_ROLES = {"turn_portal", "portal_approach", "portal_exit", "crouch_transition",
                  "spawn_entry", "spawn_approach", "native_repair_portal", "native_repair_center",
                  "room_junction"}


def ordinary_collision_rejections():
    """Completed ordinary collision decisions that may request a new corridor.

    This is a proposal policy shared by fresh generation and draft repair,
    never an acceptance rule. Budget, identity, dynamic-ground and hazard
    failures must not be reinterpreted as a checked static obstruction.
    """
    return frozenset({
        'blocked_clearance','blocked_sweep','blocked_step_up','blocked_step_down',
        'different_floor','gap_or_drop','step_or_drop','steep_ground',
        'capsule_support_gap_or_drop','capsule_support_step_or_drop','capsule_support_surface',
        'capsule_support_solid','capsule_support_skin_blocked','capsule_support_clearance',
        'capsule_walk_startsolid','capsule_walk_step_up','capsule_walk_sweep','capsule_walk_step_down',
    })


def navigation_spawns(geometry):
    """Team maps use team starts; deathmatch starts are only a fallback.

    Keep every surface sample. This changes reachability seeds, not geometry:
    a disconnected spectator/fallback room must prove a route from a team start.
    SelectCTFSpawnPoint falls back to SelectSpawnPoint when a team has no start,
    so retain deathmatch seeds when either team's initial active starts are absent.
    """
    spawns = geometry.get("spawns", [])
    active = {s.get("classname") for s in spawns
              if int(s.get("spawnflags", "0")) & 2}
    team_map = {"team_CTF_redspawn", "team_CTF_bluespawn"} <= active
    selected, excluded = [], []
    for spawn in spawns:
        name = spawn.get("classname", "")
        reason = None
        if "intermission" in name:
            reason = "spectator_camera_not_navigation_seed"
        elif team_map and name in {"info_player_start", "info_player_deathmatch"}:
            reason = "deathmatch_fallback_not_team_navigation_seed"
        if reason:
            excluded.append({**spawn, "reason": reason})
        else:
            selected.append(spawn)
    return selected, excluded


def movement_profile(stand, duck):
    """Require a matching pair of the two TC hulls or the original ET hull."""
    dp = duck.profile
    profiles = {(14, 49): (70, 24), (16, 61): (87, 30), (18,48): (72,24)}
    key = (dp.get("radius"), dp.get("height"))
    if key not in profiles or dp.get("step") != 18:
        raise ValueError("unsupported TCE crouching movement profile")
    standing, origin = profiles[key]
    if stand is not None:
        sp = stand.profile
        if (sp.get("radius") != key[0] or sp.get("height") != standing or
                sp.get("step") != dp.get("step") or
                any(sp.get(k) != dp.get(k) for k in ("cell", "cell_height"))):
            raise ValueError("standing/crouching movement profiles differ")
    return {"radius": key[0], "standing_height": standing,
            "crouching_height": key[1], "origin_above_feet": origin, "step": 18}


def validate_native_profile(profile, native):
    """Reject evidence from the other hull before interpreting coordinates."""
    bbox = native.get("bbox", {})
    fields = {"half_width": "radius", "stand_height": "standing_height",
              "crouch_height": "crouching_height", "origin_above_feet": "origin_above_feet"}
    if any(bbox.get(k) != profile[v] for k, v in fields.items()):
        raise ValueError("native feedback movement profile differs from Recast meshes")


class BspLeafFilter:
    """Reject only samples wholly outside the BSP's compiled visibility leaves.

    This is a negative geometry filter, never a movement or reachability proof.
    A point with any valid body-height leaf is retained, including crouch-only
    passages, disconnected rooms, and roofs within the compiled level. No PVS
    visibility between two points is required. Leaked/no-cluster BSPs disable
    this filter instead of discarding the entire map.
    """
    OFFSETS = (8., 24., 48.)  # All lie within both the 49/61-unit crouched bodies.

    def __init__(self, geometry):
        self.enabled = False
        self.disabled_reason = "no_bsp_provenance"
        self.sha256 = None
        self._raw = None
        self.cache, self.excluded = {}, {}
        source = geometry.get("input", {})
        if not source.get("path") or not source.get("bsp_sha256"):
            return
        path = Path(source["path"])
        if source.get("archive_member"):
            with zipfile.ZipFile(path) as archive:
                entry = archive.getinfo(source["archive_member"])
                if entry.file_size > 256 * 1024 * 1024:
                    raise ValueError("BSP visibility source exceeds 256 MiB")
                raw = archive.read(entry)
        else:
            if path.stat().st_size > 256 * 1024 * 1024:
                raise ValueError("BSP visibility source exceeds 256 MiB")
            raw = path.read_bytes()
        self.sha256 = hashlib.sha256(raw).hexdigest()
        if self.sha256 != source["bsp_sha256"]:
            raise ValueError("BSP visibility source hash differs from collision geometry")
        if len(raw) < 144 or raw[:4] != b"IBSP" or struct.unpack_from("<i", raw, 4)[0] != 47:
            raise ValueError("BSP leaf filter requires an intact ET IBSP47 header")
        self._raw = raw

        def lump(index, fmt):
            offset, length = struct.unpack_from("<ii", raw, 8 + index*8)
            size = struct.calcsize(fmt)
            if offset < 144 or length < 0 or offset+length > len(raw) or length % size:
                raise ValueError("BSP leaf filter encountered an invalid tree lump")
            return list(struct.iter_unpack(fmt, raw[offset:offset+length]))

        self.planes, self.nodes, self.leaves = lump(2, "<4f"), lump(3, "<9i"), lump(4, "<12i")
        if not self.nodes or not self.leaves or not self.planes:
            self.disabled_reason = "no_bsp_tree"
            return
        if any(not all(math.isfinite(v) for v in plane) for plane in self.planes):
            raise ValueError("BSP leaf filter encountered nonfinite planes")
        for node in self.nodes:
            if not 0 <= node[0] < len(self.planes):
                raise ValueError("BSP leaf filter plane index is out of range")
            for child in node[1:3]:
                if not (0 <= child < len(self.nodes) or -len(self.leaves) <= child < 0):
                    raise ValueError("BSP leaf filter child index is out of range")
        if not any(leaf[0] >= 0 for leaf in self.leaves):
            self.disabled_reason = "no_usable_visibility_clusters"
            return
        self.enabled = True
        self.disabled_reason = None

    def cluster(self, point):
        index = 0
        for _ in range(len(self.nodes)+1):
            if index < 0:
                return self.leaves[-1-index][0]
            node = self.nodes[index]
            plane = self.planes[node[0]]
            side = sum(point[k]*plane[k] for k in range(3)) - plane[3]
            index = node[1 if side >= 0 else 2]
        raise ValueError("BSP leaf filter encountered a cyclic tree")

    def allows(self, feet, polygon=None, kind="query"):
        if not self.enabled:
            return True
        key = tuple(feet)
        if key not in self.cache:
            self.cache[key] = any(self.cluster((feet[0], feet[1], feet[2]+height)) >= 0
                                  for height in self.OFFSETS)
        allowed = self.cache[key]
        if not allowed:
            entry = self.excluded.setdefault((polygon, key), {"point": list(feet), "polygon": polygon,
                                               "reason": "all_body_samples_outside_bsp_clusters", "kinds": []})
            if kind not in entry["kinds"]:
                entry["kinds"].append(kind)
        return allowed

    def report(self):
        return {"model": "negative_bsp_leaf_cluster_body_samples_v1", "enabled": self.enabled,
                "disabled_reason": self.disabled_reason, "bsp_sha256": self.sha256,
                "body_height_offsets": list(self.OFFSETS), "body_profile": "within_TCE_49_and_61_unit_crouch_heights",
                "tested_unique_positions": len(self.cache),
                "excluded_unique_positions": sum(not allowed for allowed in self.cache.values()),
                "excluded_samples": list(self.excluded.values()),
                "valid_cluster_positions_excluded": 0,
                "native_collision_and_reachability_still_required": True}


class BspWorldLiquidFilter:
    """Negative filter for static WORLD liquids rejected by native NG_Ground.

    A Recast sample is not yet the native projected foot position. Exclude
    only when one liquid brush contains the whole native projection range
    (NG_Ground uses +18/-18.5 and a .125 foot lift). Water must also contain
    every possible standing middle sample (up to41 above feet), since native grounding permits waterlevel1 wading.
    Boundary/shallow cases go through to the native authority. Solid brushes
    and inline entities are intentionally not classified by this filter.
    """
    CONTENTS_MASK = 0x38  # CONTENTS_LAVA | CONTENTS_SLIME | CONTENTS_WATER.
    PROJECTION = 20.
    WADE_DRY_PROBE = 41.
    BOUNDARY_GUARD = .125

    def __init__(self, tree):
        self.tree, self.sha256 = tree, tree.sha256
        self.enabled, self.disabled_reason = False, "no_verified_bsp_tree"
        self.brushes, self.cache, self.excluded = {}, {}, {}
        self.leafbrushes = []
        raw = tree._raw
        if raw is None or not tree.nodes or not tree.leaves:
            return

        def records(index, fmt):
            offset, length = struct.unpack_from("<ii", raw, 8+index*8)
            if length == 0:
                return []
            if offset < 144 or length < 0 or offset+length > len(raw) or length % struct.calcsize(fmt):
                raise ValueError("BSP liquid filter encountered an invalid brush lump")
            return list(struct.iter_unpack(fmt, raw[offset:offset+length]))

        shaders, models = records(1, "<64sii"), records(7, "<6f4i")
        brushes, sides = records(8, "<iii"), records(9, "<ii")
        self.leafbrushes = [v[0] for v in records(6, "<i")]
        if not shaders or not models:
            self.disabled_reason = "no_world_model_or_shaders"
            return
        first, count = models[0][8:10]
        if first < 0 or count < 0 or first+count > len(brushes):
            raise ValueError("BSP liquid filter world brush range is invalid")
        for leaf in tree.leaves:
            start, length = leaf[10:12]
            if start < 0 or length < 0 or start+length > len(self.leafbrushes):
                raise ValueError("BSP liquid filter leaf brush range is invalid")
        if any(i < 0 or i >= len(brushes) for i in self.leafbrushes):
            raise ValueError("BSP liquid filter leaf brush index is invalid")
        for i in range(first, first+count):
            start, length, shader = brushes[i]
            if not 0 <= shader < len(shaders) or start < 0 or length < 0 or start+length > len(sides):
                raise ValueError("BSP liquid filter brush reference is invalid")
            contents = shaders[shader][2]
            if not contents & self.CONTENTS_MASK:
                continue
            plane_ids = [sides[j][0] for j in range(start, start+length)]
            if len(plane_ids) < 4 or any(not 0 <= p < len(tree.planes) for p in plane_ids):
                raise ValueError("BSP liquid filter liquid brush planes are invalid")
            self.brushes[i] = (contents, [tree.planes[p] for p in plane_ids])
        self.enabled, self.disabled_reason = True, None

    def containing_brush(self, feet):
        index = 0
        for _ in range(len(self.tree.nodes)+1):
            if index < 0:
                break
            node = self.tree.nodes[index]; plane = self.tree.planes[node[0]]
            delta = sum(feet[k]*plane[k] for k in range(3))-plane[3]
            index = node[1 if delta >= 0 else 2]
        else:
            raise ValueError("BSP liquid filter encountered a cyclic tree")
        leaf = self.tree.leaves[-1-index]
        for j in range(leaf[10], leaf[10]+leaf[11]):
            brush = self.leafbrushes[j]
            if brush not in self.brushes:
                continue
            contents, planes = self.brushes[brush]
            # Convex endpoint support proves all projected feet are wet.
            # For water every possible41-unit standing middle sample must stay wet;
            # otherwise a higher native reprojection may permit wading.
            upper = self.PROJECTION
            if contents & 0x20 and not contents & 0x18:
                upper += self.WADE_DRY_PROBE
            if all(sum(feet[k]*plane[k] for k in range(3)) + max(-plane[2]*self.PROJECTION, plane[2]*upper)
                   < plane[3]-self.BOUNDARY_GUARD for plane in planes):
                return brush, contents
        return None

    def allows(self, feet, polygon=None, kind="query"):
        if not self.enabled or not self.brushes:
            return True
        key = tuple(feet)
        if key not in self.cache:
            self.cache[key] = self.containing_brush(feet)
        match = self.cache[key]
        if match is None:
            return True
        entry = self.excluded.setdefault((polygon, key), {"point": list(feet), "polygon": polygon,
                    "reason": "native_foot_and_wading_probe_wholly_in_world_liquid" if match[1] & 0x20 and not match[1] & 0x18 else "native_foot_projection_wholly_in_world_hazard", "brush": match[0],
                    "contents": match[1], "kinds": []})
        if kind not in entry["kinds"]:
            entry["kinds"].append(kind)
        return False

    def report(self):
        return {"model": "negative_world_liquid_posture_projection_v3", "enabled": self.enabled,
                "disabled_reason": self.disabled_reason, "bsp_sha256": self.sha256,
                "contents_mask": self.CONTENTS_MASK, "world_liquid_brushes": len(self.brushes),
                "foot_projection_half_range": self.PROJECTION, "boundary_guard": self.BOUNDARY_GUARD,
                "water_dry_probe_above_feet": self.WADE_DRY_PROBE,
                "water_exclusion_vertical_range": [-self.PROJECTION, self.PROJECTION+self.WADE_DRY_PROBE],
                "hazard_exclusion_vertical_range": [-self.PROJECTION, self.PROJECTION],
                "tested_unique_positions": len(self.cache),
                "excluded_unique_positions": sum(v is not None for v in self.cache.values()),
                "excluded_samples": list(self.excluded.values()), "inline_models_filtered": False,
                "boundary_or_uncertain_samples_retained": True, "native_validation_still_required": True}


class BspPointFilters:
    def __init__(self, geometry):
        self.leaf = BspLeafFilter(geometry)
        self.liquid = BspWorldLiquidFilter(self.leaf)

    def allows(self, feet, polygon=None, kind="query"):
        return self.leaf.allows(feet, polygon, kind) and self.liquid.allows(feet, polygon, kind)

    def report(self):
        return {"bsp_leaf_filter": self.leaf.report(), "world_liquid_filter": self.liquid.report()}


def validate_candidate_capacity(nodes, directed_links, spatial):
    node_limit = SPATIAL_NODE_LIMIT if spatial else 4096
    link_limit = SPATIAL_DIRECTED_LINK_LIMIT if spatial else 65536
    if nodes < 1 or nodes > node_limit or directed_links < 0 or directed_links > link_limit:
        raise ValueError(f"candidate exceeds native input capacity ({node_limit} nodes/{link_limit} directed links)")


def topology_protected(node):
    """Keep the actual door/bend witnesses through the native acceptance stage."""
    if node.get("straight_chain_witness") or node.get("native_contraction_candidate"):
        return False
    return bool(node.get("topology_protected") or node.get("kind") in TOPOLOGY_ROLES)


def crossing(a, b, c, d):
    """Proper XY crossing on the same floor; overpasses are not junctions."""
    if (max(a[0], b[0]) <= min(c[0], d[0]) or max(c[0], d[0]) <= min(a[0], b[0]) or
            max(a[1], b[1]) <= min(c[1], d[1]) or max(c[1], d[1]) <= min(a[1], b[1])):
        return False
    ux, uy, vx, vy = b[0]-a[0], b[1]-a[1], d[0]-c[0], d[1]-c[1]
    denominator = ux*vy-uy*vx
    if abs(denominator) < EPS:
        return False
    wx, wy = c[0]-a[0], c[1]-a[1]
    t, s = (wx*vy-wy*vx)/denominator, (wx*uy-wy*ux)/denominator
    return (EPS < t < 1-EPS and EPS < s < 1-EPS and
            abs(a[2]+t*(b[2]-a[2])-c[2]-s*(d[2]-c[2])) <= 16)


def fan_count(center, targets, nodes):
    vectors = []
    for node in targets:
        p = nodes[node]["point"]
        dx, dy = p[0]-center[0], p[1]-center[1]
        length = math.hypot(dx, dy)
        if length > 32 and abs(p[2]-center[2]) <= 18:
            vectors.append((dx/length, dy/length))
    return sum(a[0]*b[0]+a[1]*b[1] > .93969262
               for i, a in enumerate(vectors) for b in vectors[i+1:])


def contraction_shape_ok(a, b, others, adjacency, nodes):
    """A safe walk is not by itself a useful room-network simplification."""
    replacement = (adjacency[b] - {a}) | others
    if len(replacement) > max(4, len(adjacency[b])):
        return False
    if fan_count(nodes[b]["point"], replacement, nodes) > fan_count(
            nodes[b]["point"], adjacency[b], nodes):
        return False
    old_edges = {tuple(sorted((a, n))) for n in adjacency[a]}
    new_edges = {tuple(sorted((b, n))) for n in others if n not in adjacency[b]}
    if not new_edges:
        return True
    fixed = [(u, v) for u, neighbors in adjacency.items() for v in neighbors
             if u < v and a not in (u, v)]

    def crossings(edges):
        return sum(crossing(nodes[u]["point"], nodes[v]["point"],
                            nodes[c]["point"], nodes[d]["point"])
                   for u, v in edges for c, d in fixed
                   if not {u, v} & {c, d})
    return crossings(new_edges) <= crossings(old_edges)


def organize_room_edges(nodes, edges, stand, duck):
    """Make shared floor routes explicit, retaining every original endpoint.

    These are still candidate changes. Native hull validation must approve
    every new segment before a WAY is written. Short parallel forks receive a
    retained two-edge witness; different floors and obstructed doors cannot
    be joined merely because their projections cross in the viewer.
    """
    edges = set(edges)
    adjacency = defaultdict(set)
    for a, b in edges:
        adjacency[a].add(b)
        adjacency[b].add(a)
    protected_edges, forks = set(), []
    for center in sorted(adjacency, key=lambda n: (-len(adjacency[n]), n)):
        targets = sorted(adjacency[center], key=lambda n: distance(nodes[center]["point"], nodes[n]["point"]))
        for index, short in enumerate(targets):
            if short not in adjacency[center]:
                continue
            a, b = nodes[center], nodes[short]
            ab = distance(a["point"], b["point"])
            if ab < 32:
                continue
            for far in targets[index+1:]:
                old = tuple(sorted((center, far)))
                if far not in adjacency[center] or old in protected_edges:
                    continue
                c = nodes[far]
                ac, bc = distance(a["point"], c["point"]), distance(b["point"], c["point"])
                if (bc > 128 or ab+bc > ac*1.2 or far in adjacency[short] or
                        len({a["crouch"], b["crouch"], c["crouch"]}) != 1):
                    continue
                if fan_count(a["point"], [short, far], nodes) != 1:
                    continue
                if not duck.visible(b["point"], b["polygon"], c["point"], c["polygon"]):
                    continue
                if not b["crouch"] and not stand.visible(b["point"], stand.locate(b["point"]),
                                                        c["point"], stand.locate(c["point"])):
                    continue
                replacement = tuple(sorted((short, far)))
                edges.remove(old)
                edges.add(replacement)
                adjacency[center].remove(far)
                adjacency[far].remove(center)
                adjacency[short].add(far)
                adjacency[far].add(short)
                protected_edges.update((tuple(sorted((center, short))), replacement))
                forks.append({"edge": list(old), "via": short, "max_stretch": 1.2})

    ordered, splits, junctions, reused = sorted(edges), defaultdict(list), [], []
    for index, (a, b) in enumerate(ordered):
        pa, pb = nodes[a]["point"], nodes[b]["point"]
        for c, d in ordered[index+1:]:
            if {a, b} & {c, d} or nodes[a]["component"] != nodes[c]["component"]:
                continue
            pc, pd = nodes[c]["point"], nodes[d]["point"]
            if not crossing(pa, pb, pc, pd):
                continue
            ux, uy, vx, vy = pb[0]-pa[0], pb[1]-pa[1], pd[0]-pc[0], pd[1]-pc[1]
            den, wx, wy = ux*vy-uy*vx, pc[0]-pa[0], pc[1]-pa[1]
            t, s = (wx*vy-wy*vx)/den, (wx*uy-wy*ux)/den
            za, zb = pa[2]+t*(pb[2]-pa[2]), pc[2]+s*(pd[2]-pc[2])
            if abs(za-zb) > 2:
                continue
            point = [pa[0]+t*ux, pa[1]+t*uy, (za+zb)*.5]
            polygon = duck.locate(point, tolerance=2)
            if polygon is None or duck.component[polygon] != nodes[a]["component"]:
                continue
            point[2] = duck.height(polygon, point)
            if abs(point[2]-za) > 2 or abs(point[2]-zb) > 2:
                continue
            if any(not duck.visible(point, polygon, nodes[n]["point"], nodes[n]["polygon"])
                   for n in (a, b, c, d)):
                continue
            # A crossing with one low route must not turn the other, standing
            # route into a duck-only connection through the shared junction.
            standing_ends = [n for edge in ((a, b), (c, d))
                             if not any(nodes[v]["crouch"] for v in edge) for n in edge]
            sp = stand.locate(point)
            if any(not stand.visible(point, sp, nodes[n]["point"], stand.locate(nodes[n]["point"]))
                   for n in standing_ends):
                continue
            # Reuse an existing endpoint/junction within four units only if
            # every affected piece remains visible at the identical stance.
            # Moving a protected door point or merging across a thin obstacle
            # is forbidden. Check other splits too, not just the four ends.
            crouch = stand.locate(point) is None
            endpoints = {a, b, c, d} | {n for _, n in splits[(a, b)] + splits[(c, d)]}
            joint = None
            near = sorted((distance(node["point"], point), n) for n, node in enumerate(nodes)
                          if node["component"] == nodes[a]["component"] and node["crouch"] == crouch
                          and distance(node["point"], point) <= 4)
            for _, candidate in near:
                node = nodes[candidate]
                if abs(node["point"][2]-point[2]) > 2:
                    continue
                if any(not duck.visible(node["point"], node["polygon"], nodes[n]["point"], nodes[n]["polygon"])
                       for n in endpoints):
                    continue
                if any(not stand.visible(node["point"], stand.locate(node["point"]),
                                         nodes[n]["point"], stand.locate(nodes[n]["point"]))
                       for n in standing_ends):
                    continue
                joint = candidate
                nodes[joint]["topology_protected"] = True
                reused.append({"node": joint, "crossing": point, "distance": distance(node["point"], point)})
                break
            if joint is None:
                joint = len(nodes)
                nodes.append({"point": point, "polygon": polygon, "crouch": crouch,
                              "spawn": False, "kind": "room_junction", "component": nodes[a]["component"],
                              "topology_protected": True})
                junctions.append(joint)
            splits[(a, b)].append((t, joint))
            splits[(c, d)].append((s, joint))
    for edge, intersections in splits.items():
        chain = []
        for n in [edge[0]] + [n for _, n in sorted(set(intersections))] + [edge[1]]:
            if n in chain:
                chain = chain[:chain.index(n)+1]
            else:
                chain.append(n)
        edges.remove(edge)
        pieces = {tuple(sorted((a, b))) for a, b in zip(chain, chain[1:]) if a != b}
        edges.update(pieces)
        if edge in protected_edges:
            protected_edges.remove(edge)
            protected_edges.update(pieces)
    return edges, {"parallel_fork_witnesses": forks, "shared_floor_junctions": junctions,
                   "split_crossing_edges": len(splits), "native_revalidation_required": True,
                   "reused_floor_junctions": reused,
                   "retained_witness_segments": sorted(protected_edges)}


def distance(a, b):
    return math.sqrt(sum((a[i] - b[i]) ** 2 for i in range(3)))


def cross(a, b, c):
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


class Mesh:
    def __init__(self, path):
        self.path = Path(path)
        raw = self.path.read_bytes()
        self.sha256 = hashlib.sha256(raw).hexdigest()
        data = json.loads(raw)
        if data.get("coordinate_system") != "TCE_Z_up":
            raise ValueError("expected a TCE_Z_up polygon mesh")
        self.profile = data["profile"]
        self.vertices = data["vertices"]
        self.polygons = data["polygons"]
        self.detail = [poly.get("detail_triangles", []) for poly in self.polygons]
        if data.get("floor_height_model") == "recast_compact_field_detail_triangles_v1" and any(not triangles for triangles in self.detail):
            raise ValueError("Recast detail height data missing for a polygon")
        if sum(len(triangles) for triangles in self.detail) > 2000000:
            raise ValueError("detail triangle budget exceeded")
        for triangles in self.detail:
            for triangle in triangles:
                if len(triangle)!=3 or any(len(point)!=3 or any(type(v) not in (int,float) or not math.isfinite(v) for v in point) for point in triangle):
                    raise ValueError("invalid Recast detail triangle")
        if len(self.polygons) > 50000:
            raise ValueError("polygon budget exceeded")
        self.points, self.centers, self.areas, self.signs = [], [], [], []
        self.grid = defaultdict(list)
        for index, poly in enumerate(self.polygons):
            points = [self.vertices[v] for v in poly["vertices"]]
            if len(points) < 3 or any(not math.isfinite(x) for p in points for x in p):
                raise ValueError("invalid polygon")
            signed = sum(points[j][0] * points[(j + 1) % len(points)][1] -
                         points[(j + 1) % len(points)][0] * points[j][1]
                         for j in range(len(points)))
            if abs(signed) < EPS:
                raise ValueError("degenerate polygon")
            center = [sum(p[k] for p in points) / len(points) for k in range(3)]
            self.points.append(points)
            self.centers.append(center)
            self.areas.append(abs(signed) * .5)
            self.signs.append(1 if signed > 0 else -1)
            for x in range(math.floor(min(p[0] for p in points) / 128),
                           math.floor(max(p[0] for p in points) / 128) + 1):
                for y in range(math.floor(min(p[1] for p in points) / 128),
                               math.floor(max(p[1] for p in points) / 128) + 1):
                    self.grid[x, y].append(index)
            if len(poly["neighbors"]) != len(points) or any(
                    n < -1 or n >= len(self.polygons) for n in poly["neighbors"]):
                raise ValueError("invalid adjacency")
        self.component = [-1] * len(self.polygons)
        # A polygon can span a step or slope. Its arithmetic mean vertex Z is
        # not necessarily the floor height at its centre XY.
        for p, center in enumerate(self.centers):
            center[2] = self.height(p, center)
        self.components = []
        for initial in range(len(self.polygons)):
            if self.component[initial] >= 0:
                continue
            group, queue = [], [initial]
            self.component[initial] = len(self.components)
            while queue:
                p = queue.pop()
                group.append(p)
                for n in self.polygons[p]["neighbors"]:
                    if n >= 0 and self.component[n] < 0:
                        self.component[n] = self.component[initial]
                        queue.append(n)
            self.components.append(group)

    def height(self, p, point):
        points = self.points[p]
        for a,b,c in self.detail[p]:
            area=cross(a,b,c)
            if abs(area)<EPS:continue
            u,v,w=cross(point,b,c)/area,cross(a,point,c)/area,cross(a,b,point)/area
            if min(u,v,w)>=-EPS:
                return u*a[2]+v*b[2]+w*c[2]
        for j in range(1, len(points) - 1):
            a, b, c = points[0], points[j], points[j + 1]
            area = cross(a, b, c)
            if abs(area) < EPS:
                continue
            u, v, w = cross(point, b, c) / area, cross(a, point, c) / area, cross(a, b, point) / area
            if min(u, v, w) >= -EPS:
                return u * a[2] + v * b[2] + w * c[2]
        return self.centers[p][2]

    def contains(self, p, point):
        pts = self.points[p]
        return all(self.signs[p] * cross(pts[j], pts[(j + 1) % len(pts)], point) >= -EPS
                   for j in range(len(pts)))

    def locate(self, point, tolerance=12):
        matches = []
        for p in self.grid.get((math.floor(point[0] / 128), math.floor(point[1] / 128)), ()):
            if self.contains(p, point):
                z = self.height(p, point)
                if abs(z - point[2]) <= tolerance:
                    matches.append((abs(z - point[2]), p))
        return min(matches)[1] if matches else None

    def closest(self, point, horizontal=128, vertical=96):
        """Project a known spawn to a nearby floor, never choose a high roof."""
        options = []
        px, py = math.floor(point[0] / 128), math.floor(point[1] / 128)
        candidates = set()
        extent = math.ceil(horizontal / 128)
        for x in range(px - extent, px + extent + 1):
            for y in range(py - extent, py + extent + 1):
                candidates.update(self.grid.get((x, y), ()))
        for p in candidates:
            xy = point[:2]
            if not self.contains(p, xy):
                near = []
                pts = self.points[p]
                for a, b in zip(pts, pts[1:] + pts[:1]):
                    dx, dy = b[0] - a[0], b[1] - a[1]
                    den = dx * dx + dy * dy
                    t = max(0, min(1, ((point[0] - a[0]) * dx + (point[1] - a[1]) * dy) / den)) if den else 0
                    q = [a[0] + t * dx, a[1] + t * dy]
                    near.append(((q[0] - point[0]) ** 2 + (q[1] - point[1]) ** 2, q))
                xy = min(near)[1]
            result = [xy[0], xy[1], self.height(p, xy)]
            dxy = math.hypot(result[0] - point[0], result[1] - point[1])
            dz = abs(result[2] - point[2])
            if dxy <= horizontal and dz <= vertical and result[2] <= point[2] + 18:
                options.append((dxy * dxy + 4 * dz * dz, p, result))
        return min(options)[1:] if options else None

    def interval(self, p, a, b):
        lo, hi = 0., 1.
        points = self.points[p]
        for x, y in zip(points, points[1:] + points[:1]):
            ca, cb = self.signs[p] * cross(x, y, a), self.signs[p] * cross(x, y, b)
            change = cb - ca
            if abs(change) < EPS:
                if ca < -EPS:
                    return None
                continue
            boundary = -ca / change
            if change > 0:
                lo = max(lo, boundary)
            else:
                hi = min(hi, boundary)
            if lo > hi + EPS:
                return None
        return max(0., lo), min(1., hi)

    def visible(self, a, ap, b, bp):
        """Walk the straight segment only through neighboring floor polygons.

        Overlapping floors in different storeys cannot create a shortcut. This
        is a mesh certificate only; the native square hull remains authoritative.
        """
        if ap is None or bp is None or self.component[ap] != self.component[bp]:
            return False
        if ap == bp:
            return True
        intervals = {ap: self.interval(ap, a, b)}
        if intervals[ap] is None:
            return False
        queue, visited = [(0., ap)], set()
        while queue:
            _, p = heapq.heappop(queue)
            if p in visited:
                continue
            visited.add(p)
            lo, hi = intervals[p]
            if p == bp and hi >= 1 - EPS:
                return True
            for n in self.polygons[p]["neighbors"]:
                if n < 0 or n in visited:
                    continue
                if n not in intervals:
                    intervals[n] = self.interval(n, a, b)
                bounds = intervals[n]
                if bounds is None or bounds[0] > hi + EPS or bounds[1] < lo - EPS:
                    continue
                t = max(lo, bounds[0])
                q = [a[i] + t * (b[i] - a[i]) for i in range(3)]
                if abs(self.height(p, q) - self.height(n, q)) > 20:
                    continue
                heapq.heappush(queue, (bounds[0], n))
        return False


def build_visibility_legacy(stand, duck, geometry, radius):
    profile = movement_profile(stand, duck)
    polygon_stance = [stand.locate(center) is None for center in duck.centers]
    spawns, spawn_errors = [], []
    seed_spawns, excluded_spawns = navigation_spawns(geometry)
    for spawn in seed_spawns:
        point = spawn["origin_bsp"][:]
        point[2] -= profile["origin_above_feet"]
        found = duck.closest(point)
        if found:
            spawns.append({"entity": spawn["entity"], "polygon": found[0], "point": found[1]})
        else:
            spawn_errors.append(spawn)
    owners = [-1] * len(duck.polygons)
    nodes, clusters = [], []
    by_component = defaultdict(list)
    for spawn in spawns:
        by_component[duck.component[spawn["polygon"]]].append(spawn)
    for component, polygons in enumerate(duck.components):
        remaining = set(polygons)
        nearest = {p: float("inf") for p in polygons}
        seeds = by_component.get(component, [])
        first = seeds[0]["polygon"] if seeds else max(polygons, key=lambda p: duck.areas[p])
        while remaining:
            seed = first if len(remaining) == len(polygons) else max(
                remaining, key=lambda p: (nearest[p], duck.areas[p], -p))
            point = duck.centers[seed][:]
            owner = len(nodes)
            nodes.append({"point": point, "polygon": seed, "crouch": polygon_stance[seed],
                          "spawn": False, "kind": "visibility_region", "component": component})
            cluster, queue, visited = [], deque([seed]), set()
            while queue:
                p = queue.popleft()
                if p in visited:
                    continue
                visited.add(p)
                d = distance(point, duck.centers[p])
                if d > radius or polygon_stance[p] != polygon_stance[seed]:
                    continue
                if not duck.visible(point, seed, duck.centers[p], p):
                    continue
                if p in remaining:
                    owners[p] = owner
                    cluster.append(p)
                    remaining.remove(p)
                queue.extend(n for n in duck.polygons[p]["neighbors"] if n >= 0 and n not in visited)
            if not cluster:
                raise ValueError("a polygon failed its own visibility certificate")
            clusters.append({"node": owner, "polygons": cluster, "area": sum(duck.areas[p] for p in cluster)})
            for p in remaining:
                nearest[p] = min(nearest[p], distance(point, duck.centers[p]))

    edges, unresolved = set(), []
    portal_nodes = {}

    def add(point, p, kind):
        # Reuse close transition anchors only with a genuine mesh connection.
        for index in portal_nodes.get((duck.component[p], math.floor(point[0] / 48), math.floor(point[1] / 48)), []):
            old = nodes[index]
            if distance(old["point"], point) < .001 and duck.visible(old["point"], old["polygon"], point, p):
                return index
        index = len(nodes)
        nodes.append({"point": point[:], "polygon": p, "crouch": stand.locate(point) is None,
                      "spawn": False, "kind": kind, "component": duck.component[p]})
        portal_nodes.setdefault((duck.component[p], math.floor(point[0] / 48), math.floor(point[1] / 48)), []).append(index)
        return index

    def connect(a, b):
        if a == b:
            return True
        x, y = nodes[a], nodes[b]
        length = distance(x["point"], y["point"])
        if not duck.visible(x["point"], x["polygon"], y["point"], y["polygon"]):
            return False
        if length > 512:
            chain = [a]
            steps = math.ceil(length / 400)
            for j in range(1, steps):
                point = [x["point"][k] + (y["point"][k] - x["point"][k]) * j / steps for k in range(3)]
                p = duck.locate(point, tolerance=24)
                if p is None or duck.component[p] != x["component"]:
                    return False
                point[2] = duck.height(p, point)
                chain.append(add(point, p, "long_corridor_sample"))
            chain.append(b)
            if any(not duck.visible(nodes[u]["point"], nodes[u]["polygon"], nodes[v]["point"], nodes[v]["polygon"])
                   for u, v in zip(chain, chain[1:])):
                return False
            edges.update(tuple(sorted((u, v))) for u, v in zip(chain, chain[1:]) if u != v)
            return True
        edges.add(tuple(sorted((a, b))))
        return True

    # Each cluster boundary keeps at least one route. Portal nodes are created
    # only where a single anchor-to-anchor straight path cannot follow a bend.
    boundaries = defaultdict(list)
    for p, poly in enumerate(duck.polygons):
        for side, n in enumerate(poly["neighbors"]):
            if n > p and owners[p] != owners[n]:
                boundaries[tuple(sorted((owners[p], owners[n])))].append((p, n, side))
    boundary_routes = []
    for (a, b), choices in boundaries.items():
        if connect(a, b):
            continue
        # Two regions can meet through several separate doors or on opposite
        # sides of an obstacle. Keep each contiguous portal run; selecting just
        # one arbitrary shared edge would erase those alternate routes.
        touching = defaultdict(set)
        for i, (p, n, side) in enumerate(choices):
            pts = duck.points[p]
            for endpoint in (pts[side], pts[(side + 1) % len(pts)]):
                touching[tuple(round(c, 3) for c in endpoint)].add(i)
        todo = set(range(len(choices)))
        while todo:
            first = min(todo)
            queue, group = [first], []
            todo.remove(first)
            while queue:
                i = queue.pop()
                group.append(choices[i])
                p, n, side = choices[i]
                pts = duck.points[p]
                for endpoint in (pts[side], pts[(side + 1) % len(pts)]):
                    for other in touching[tuple(round(c, 3) for c in endpoint)] & todo:
                        todo.remove(other)
                        queue.append(other)
            boundary_routes.append(((a, b), group))
    for (a, b), choices in boundary_routes:
        connected = False
        for p, n, side in sorted(choices, key=lambda v: distance(duck.centers[v[0]], duck.centers[v[1]])):
            pts = duck.points[p]
            midpoint = [(pts[side][k] + pts[(side + 1) % len(pts)][k]) * .5 for k in range(3)]
            portal = add(midpoint, p, "turn_portal")
            chain = [owners[p]]
            if not connect(chain[-1], portal):
                center = add(duck.centers[p], p, "portal_approach")
                if not connect(chain[-1], center) or not connect(center, portal):
                    continue
            if not connect(portal, owners[n]):
                center = add(duck.centers[n], n, "portal_exit")
                if not connect(portal, center) or not connect(center, owners[n]):
                    continue
            connected = True
            break
        if not connected:
            unresolved.append({"clusters": [a, b], "polygon_boundaries": choices})

    # The region boundaries above already supply the room topology, including
    # separate doorway runs. Adding a visible neighbor in every compass sector
    # created crossing diagonals and parallel fans with no new room access.
    # Keep this boundary network; the native gate can repair a measured detour
    # after collision validation rather than guessing extra all-room routes.

    # Stance change belongs at the low section of a route, not arbitrarily at
    # both room anchors. Split every duck-only link through an explicit crouch
    # anchor. Native validation will still reject any unsupported transition.
    original_edges = sorted(edges)
    edges.clear()
    stance_splits = 0
    for a, b in original_edges:
        x, y = nodes[a], nodes[b]
        ax, bx = stand.locate(x["point"]), stand.locate(y["point"])
        if ax is not None and bx is not None and stand.visible(x["point"], ax, y["point"], bx):
            edges.add((a, b))
            continue
        if x["crouch"] or y["crouch"]:
            edges.add((a, b))
            continue
        low = None
        steps = max(2, math.ceil(distance(x["point"], y["point"]) / 12))
        for j in range(1, steps):
            q = [x["point"][k] + (y["point"][k] - x["point"][k]) * j / steps for k in range(3)]
            p = duck.locate(q, tolerance=24)
            if p is not None:
                q[2] = duck.height(p, q)
                if stand.locate(q) is None:
                    low = q, p
                    break
        if low is not None:
            middle = add(low[0], low[1], "crouch_transition")
            nodes[middle]["crouch"] = True
            if connect(a, middle) and connect(middle, b):
                stance_splits += 1
                continue
        # Conservative stance is required for a mesh link that cannot be
        # proven standing, even if the low subsegment missed the sample grid.
        x["crouch"] = y["crouch"] = True
        edges.add((a, b))

    for spawn in spawns:
        candidates = []
        for i, node in enumerate(nodes):
            if node["component"] == duck.component[spawn["polygon"]] and distance(node["point"], spawn["point"]) <= 224:
                if duck.visible(node["point"], node["polygon"], spawn["point"], spawn["polygon"]):
                    candidates.append((distance(node["point"], spawn["point"]), i))
        if candidates:
            spawn["node"] = min(candidates)[1]
            nodes[spawn["node"]]["spawn"] = True
        else:
            anchor = add(spawn["point"], spawn["polygon"], "spawn_entry")
            owner = owners[spawn["polygon"]]
            attached = connect(anchor, owner)
            if not attached:
                middle = add(duck.centers[spawn["polygon"]], spawn["polygon"], "spawn_approach")
                attached = connect(anchor, middle) and connect(middle, owner)
            if attached:
                spawn["node"] = anchor
                nodes[anchor]["spawn"] = True
            else:
                spawn_errors.append({"reason": "no_visible_region_anchor", **spawn})

    # Remove redundant transition anchors and coalesce adjacent visible regions.
    # Unlike a raw distance filter this keeps *all* assigned polygon witnesses
    # within the original coverage radius of the replacement anchor, and checks
    # every rewired segment against polygon connectivity. Narrow bends survive.
    adjacency = defaultdict(set)
    for a, b in edges:
        adjacency[a].add(b)
        adjacency[b].add(a)
    assigned = defaultdict(list)
    for p, owner in enumerate(owners):
        assigned[owner].append(p)
    alive = set(range(len(nodes)))
    contracted = []
    for _ in range(8):
        changed = False
        ordered = sorted(alive, key=lambda a: (not not assigned[a],
                         min((distance(nodes[a]["point"], nodes[b]["point"])
                              for b in adjacency[a]), default=float("inf"))))
        for a in ordered:
            if (a not in alive or len(adjacency[a]) < 2 or nodes[a]["spawn"] or
                    topology_protected(nodes[a])):
                continue
            candidates = sorted(adjacency[a], key=lambda b: distance(nodes[a]["point"], nodes[b]["point"]))
            for b in candidates:
                if nodes[a]["crouch"] != nodes[b]["crouch"]:
                    continue
                if distance(nodes[a]["point"], nodes[b]["point"]) > 192:
                    break
                rep = nodes[b]
                if any(distance(rep["point"], duck.centers[p]) > radius + EPS or
                       not duck.visible(rep["point"], rep["polygon"], duck.centers[p], p)
                       for p in assigned[a]):
                    continue
                others = adjacency[a] - {b}
                if any(distance(rep["point"], nodes[c]["point"]) > 256 or
                       not duck.visible(rep["point"], rep["polygon"], nodes[c]["point"], nodes[c]["polygon"])
                       for c in others):
                    continue
                if not contraction_shape_ok(a, b, others, adjacency, nodes):
                    continue
                if not rep["crouch"]:
                    rp = stand.locate(rep["point"])
                    if any(not nodes[c]["crouch"] and not stand.visible(
                            rep["point"], rp, nodes[c]["point"], stand.locate(nodes[c]["point"]))
                           for c in others):
                        continue
                for c in list(adjacency[a]):
                    adjacency[c].remove(a)
                for c in others:
                    adjacency[b].add(c)
                    adjacency[c].add(b)
                adjacency[a].clear()
                for p in assigned[a]:
                    owners[p] = b
                assigned[b].extend(assigned.pop(a, []))
                alive.remove(a)
                contracted.append({"removed": a, "replacement": b})
                changed = True
                break
        if not changed:
            break
    remap = {old: new for new, old in enumerate(sorted(alive))}
    nodes = [node for i, node in enumerate(nodes) if i in alive]
    edges = {(remap[a], remap[b]) for a in alive for b in adjacency[a] if a < b}
    owners = [remap[a] for a in owners]
    clusters = [{"node": remap[a], "polygons": polygons, "area": sum(duck.areas[p] for p in polygons)}
                for a, polygons in assigned.items() if a in alive and polygons]
    for spawn in spawns:
        if "node" in spawn:
            spawn["node"] = remap[spawn["node"]]

    edges, room_ordering = organize_room_edges(nodes, edges, stand, duck)

    # Shortest-first deletion leaves final, retained two-hop witnesses. Keep
    # cycles without drawing redundant diagonal fans in every open space.
    adjacency = defaultdict(set)
    for a, b in edges:
        adjacency[a].add(b)
        adjacency[b].add(a)
    removed_edges = []
    retained_witnesses = {tuple(edge) for edge in room_ordering["retained_witness_segments"]}
    for a, b in sorted(edges, key=lambda edge: distance(nodes[edge[0]]["point"], nodes[edge[1]]["point"])):
        if (a, b) in retained_witnesses:
            continue
        direct = distance(nodes[a]["point"], nodes[b]["point"])
        for c in sorted(adjacency[a] & adjacency[b]):
            ac, bc = distance(nodes[a]["point"], nodes[c]["point"]), distance(nodes[b]["point"], nodes[c]["point"])
            if ac < direct - EPS and bc < direct - EPS and ac + bc <= direct * 1.35:
                if nodes[c]["crouch"] and not (nodes[a]["crouch"] or nodes[b]["crouch"]):
                    continue
                adjacency[a].remove(b)
                adjacency[b].remove(a)
                removed_edges.append({"edge": [a, b], "via": c})
                break
    edges = sorted((a, b) for a, targets in adjacency.items() for b in targets if a < b)
    if len(nodes) > 4096 or len(edges) * 2 > 65536:
        raise ValueError("candidate exceeds native importer limits")
    return nodes, edges, {"movement_profile": profile,
                          "coverage": {"kind": "polygon_center_visibility_witness_not_runtime_coverage",
                                      "max_center_distance": radius, "polygon_owner": owners, "clusters": clusters},
                          "mesh_components": [{"id": i, "polygons": len(group),
                                               "area": sum(duck.areas[p] for p in group),
                                               "spawn_count": len(by_component.get(i, []))}
                                              for i, group in enumerate(duck.components)],
                          "spawn_matches": spawns, "spawn_failures": spawn_errors, "excluded_spawn_seeds": excluded_spawns,
                          "unresolved_cluster_boundaries": unresolved,
                          "separate_portal_runs": len(boundary_routes),
                          "crouch_transition_splits": stance_splits,
                          "contracted_nodes": len(contracted),
                          "room_ordering": room_ordering,
                          "removed_edge_witnesses": removed_edges,
                          "topology_policy": {"sector_neighbor_links": False,
                                              "max_new_contraction_link": 256,
                                              "protected_roles": sorted(TOPOLOGY_ROLES),
                                              "preserve_floor_witnesses": True,
                                              "prevent_new_crossings_and_fans": True}}


class SpatialGraph:
    """A floor-sampled roadmap; polygons certify space, they are not WAY nodes.

    The native importer remains responsible for player hulls and actual steps.
    This class only proposes mesh-visible connections and records every witness.
    """
    def __init__(self, stand, duck, spacing=160., nodes=None, edges=(), point_filter=None):
        self.movement_profile = movement_profile(stand, duck)
        self.stand, self.mesh, self.spacing = stand, duck, spacing
        self.point_filter = point_filter
        self.nodes = [] if nodes is None else nodes
        self.edges = {tuple(sorted(e)) for e in edges}
        self.grid, self.boundaries = defaultdict(list), defaultdict(list)
        self.visibility, self.standing = {}, {}
        self.adjacency = defaultdict(set)
        self.blocked = set()
        for i, node in enumerate(self.nodes):
            self.grid[self.cell(node["point"])].append(i)
        for a, b in self.edges:
            self.adjacency[a].add(b); self.adjacency[b].add(a)
        for p, poly in enumerate(duck.polygons):
            for side, neighbor in enumerate(poly["neighbors"]):
                if neighbor >= 0:
                    continue
                a, b = duck.points[p][side], duck.points[p][(side+1) % len(duck.points[p])]
                for x in range(math.floor(min(a[0], b[0])/128), math.floor(max(a[0], b[0])/128)+1):
                    for y in range(math.floor(min(a[1], b[1])/128), math.floor(max(a[1], b[1])/128)+1):
                        self.boundaries[x, y].append((p, a, b))

    def cell(self, point):
        return math.floor(point[0]/self.spacing), math.floor(point[1]/self.spacing)

    def near(self, point, radius):
        x, y = self.cell(point); extent = math.ceil(radius/self.spacing)
        return [i for gx in range(x-extent, x+extent+1) for gy in range(y-extent, y+extent+1)
                for i in self.grid.get((gx, gy), ()) if distance(point, self.nodes[i]["point"]) <= radius]

    def posture(self, point):
        key = tuple(round(v, 4) for v in point)
        if key not in self.standing:
            self.standing[key] = self.stand.locate(point)
        return self.standing[key]

    def sample(self, point, polygon, kind):
        return dict(point=list(point), polygon=polygon, crouch=self.posture(point) is None,
                    spawn=False, kind=kind, component=self.mesh.component[polygon])

    def visible(self, a, b):
        if a["component"] != b["component"]:
            return False
        key = (tuple(a["point"]), a["polygon"], tuple(b["point"]), b["polygon"],
               a["crouch"], b["crouch"])
        if key not in self.visibility:
            valid = self.mesh.visible(a["point"], a["polygon"], b["point"], b["polygon"])
            if valid and not a["crouch"] and not b["crouch"]:
                valid = self.stand.visible(a["point"], self.posture(a["point"]),
                                           b["point"], self.posture(b["point"]))
            self.visibility[key] = valid
        return self.visibility[key]

    def clearance(self, point, polygon, limit=None):
        """Distance to real exterior edges on this floor, after Recast erosion."""
        limit = self.spacing if limit is None else limit
        x, y = math.floor(point[0]/128), math.floor(point[1]/128)
        extent = math.ceil(limit/128); best = limit
        for gx in range(x-extent, x+extent+1):
            for gy in range(y-extent, y+extent+1):
                for p, a, b in self.boundaries.get((gx, gy), ()):
                    if self.mesh.component[p] != self.mesh.component[polygon]:
                        continue
                    dx, dy = b[0]-a[0], b[1]-a[1]; den = dx*dx+dy*dy
                    t = max(0., min(1., ((point[0]-a[0])*dx+(point[1]-a[1])*dy)/den)) if den else 0.
                    if abs(point[2] - (a[2]+t*(b[2]-a[2]))) > 20:
                        continue
                    best = min(best, math.hypot(point[0]-a[0]-t*dx, point[1]-a[1]-t*dy))
        return best

    def add(self, node):
        if len(self.nodes) >= SPATIAL_NODE_LIMIT:
            raise ValueError(f"spatial roadmap exceeds {SPATIAL_NODE_LIMIT} input nodes; no partial candidate is written")
        index = len(self.nodes)
        self.nodes.append(node); self.grid[self.cell(node["point"])].append(index)
        return index

    def link(self, a, b):
        if a == b:
            return True
        edge = tuple(sorted((a, b)))
        if edge in self.blocked or not self.visible(self.nodes[a], self.nodes[b]):
            return False
        self.edges.add(edge); self.adjacency[a].add(b); self.adjacency[b].add(a)
        return True

    def covered_by(self, sample, radius, same_stance=True):
        choices = sorted(self.near(sample["point"], radius),
                         key=lambda i: (distance(sample["point"], self.nodes[i]["point"]), i))
        return next((i for i in choices if (not same_stance or sample["crouch"] == self.nodes[i]["crouch"])
                     and self.visible(sample, self.nodes[i])), None)

    def local_network(self):
        """Keep the local surface neighbourhood, including alternative links.

        An undisturbed triangular lattice has six neighbours at one spacing;
        the next ring lies beyond this 1.5-spacing window. At boundaries the
        native validator receives every visible local alternative. Only a
        nearly collinear skip over another point is redundant: a point merely
        lying inside a diameter circle no longer deletes a useful side route.
        """
        potential = set()
        for a, node in enumerate(self.nodes):
            for b in self.near(node["point"], self.spacing*1.5):
                if b > a and self.visible(node, self.nodes[b]):
                    potential.add((a, b))
        near = defaultdict(set)
        for a, b in potential:
            near[a].add(b); near[b].add(a)
        for a, b in sorted(potential):
            pa, pb = self.nodes[a]["point"], self.nodes[b]["point"]
            delta = [pb[k]-pa[k] for k in range(3)]
            length_squared = sum(v*v for v in delta)
            redundant = False
            for c in near[a] & near[b]:
                if self.nodes[c]["crouch"] and not (self.nodes[a]["crouch"] or self.nodes[b]["crouch"]):
                    continue
                pc = self.nodes[c]["point"]
                t = sum((pc[k]-pa[k])*delta[k] for k in range(3))/max(EPS, length_squared)
                deviation = math.sqrt(sum((pc[k]-pa[k]-t*delta[k])**2 for k in range(3)))
                if .05 < t < .95 and deviation < 8:
                    redundant = True; break
            if not redundant:
                self.link(a, b)
        return len(potential)

    def has_route(self, a, b, budget):
        costs, queue = {a: 0.}, [(0., a)]
        while queue:
            cost, u = heapq.heappop(queue)
            if cost != costs[u] or cost > budget:
                continue
            if u == b:
                return True
            for v in self.adjacency[u]:
                total = cost + distance(self.nodes[u]["point"], self.nodes[v]["point"])
                if total <= budget and total < costs.get(v, float("inf")):
                    costs[v] = total; heapq.heappush(queue, (total, v))
        return False

    def polygon_route(self, start, finish):
        if self.mesh.component[start] != self.mesh.component[finish]:
            return None
        costs, parent, queue = {start: 0.}, {}, [(0., start)]
        while queue:
            cost, p = heapq.heappop(queue)
            if cost != costs[p]:
                continue
            if p == finish:
                route = [p]
                while p in parent:
                    p = parent[p]; route.append(p)
                return list(reversed(route))
            for n in self.mesh.polygons[p]["neighbors"]:
                if n < 0:
                    continue
                candidate = cost + distance(self.mesh.centers[p], self.mesh.centers[n])
                if candidate < costs.get(n, float("inf")):
                    costs[n] = candidate; parent[n] = p; heapq.heappush(queue, (candidate, n))
        return None

    def repair_margin(self, a, b):
        """Prefer an aligned approach after native rejection of a diagonal.

        The circular Recast erosion can admit a line that the native player
        hull clips at a corner. Test a small lateral reserve on that same
        floor. Narrow passages bound this reserve; this is still only a
        proposal, never a replacement for native collision validation.
        """
        pa, pb = a["point"], b["point"]
        length = math.hypot(pb[0]-pa[0], pb[1]-pa[1])
        if length < 2:
            return True
        margin = min(8., self.clearance(pa, a["polygon"], 8.), self.clearance(pb, b["polygon"], 8.))
        if margin < .5:
            return True
        nx, ny = (pb[1]-pa[1])/length, (pa[0]-pb[0])/length
        steps = max(2, math.ceil(length/8))
        for step in range(1, steps):
            t = step/steps
            q = [pa[k]+(pb[k]-pa[k])*t for k in range(3)]
            p = self.mesh.locate(q, tolerance=24)
            if p is None or self.mesh.component[p] != a["component"]:
                return False
            q[2] = self.mesh.height(p, q)
            for side in (-1, 1):
                r = [q[0]+nx*margin*side, q[1]+ny*margin*side, q[2]]
                rp = self.mesh.locate(r)
                if rp is None or not self.mesh.visible(q, p, r, rp):
                    return False
                if not a["crouch"] and not b["crouch"] and self.stand.locate(r) is None:
                    return False
        return True

    def corridor(self, a, b, force_bend=False):
        """String-pull a floor corridor, exporting only required steering turns."""
        polygons = self.polygon_route(self.nodes[a]["polygon"], self.nodes[b]["polygon"])
        if polygons is None:
            return None
        samples = [self.nodes[a]]
        for index, p in enumerate(polygons):
            samples.append(self.sample(self.mesh.centers[p], p, "corridor_turn"))
            if index+1 < len(polygons):
                side = self.mesh.polygons[p]["neighbors"].index(polygons[index+1])
                pts = self.mesh.points[p]
                midpoint = [(pts[side][k]+pts[(side+1) % len(pts)][k])*.5 for k in range(3)]
                midpoint[2] = self.mesh.height(p, midpoint)
                samples.append(self.sample(midpoint, p, "corridor_turn"))
        samples.append(self.nodes[b])
        pulled, current = [0], 0
        while current < len(samples)-1:
            chosen = None
            for following in range(len(samples)-1, current, -1):
                if force_bend and current == 0 and following == len(samples)-1:
                    continue
                if force_bend and current == 0 and min(distance(samples[following]["point"], self.nodes[i]["point"])
                                                       for i in (a, b)) < 2:
                    continue
                if force_bend and current == 0:
                    # A point just before the same failed endpoint would
                    # reproduce the clipped approach almost unchanged. The
                    # first retry turn must leave that corridor by at least
                    # one native eight-unit movement-sampling step.
                    pa, pb, q = self.nodes[a]["point"], self.nodes[b]["point"], samples[following]["point"]
                    span = math.hypot(pb[0]-pa[0], pb[1]-pa[1])
                    if span > 16 and abs(cross(pa, pb, q))/span < 8:
                        continue
                if distance(samples[current]["point"], samples[following]["point"]) > self.spacing*2:
                    continue
                # Never string-pull across an intervening crouch-only section
                # by merely giving one endpoint a different posture.
                if any(s["crouch"] != samples[current]["crouch"]
                       and s["crouch"] != samples[following]["crouch"] for s in samples[current+1:following]):
                    continue
                if self.visible(samples[current], samples[following]):
                    if force_bend and not self.repair_margin(samples[current], samples[following]):
                        continue
                    chosen = following; break
            if chosen is None:
                return None
            pulled.append(chosen); current = chosen
        if self.point_filter and any(not self.point_filter.allows(samples[i]["point"], samples[i]["polygon"],
                                                                  "corridor_turn") for i in pulled[1:-1]):
            return None
        chain, original_count = [a], len(self.nodes)
        for j, index in enumerate(pulled[1:-1], 1):
            sample = samples[index]
            after = samples[pulled[j+1]]
            reusable = [i for i in self.near(sample["point"], 16)
                        if (not force_bend or i not in (a, b))
                        and self.nodes[i]["crouch"] == sample["crouch"]
                        and abs(self.nodes[i]["point"][2]-sample["point"][2]) <= 2
                        and self.visible(self.nodes[chain[-1]], self.nodes[i])
                        and self.visible(self.nodes[i], after)
                        and (not force_bend or (self.repair_margin(self.nodes[chain[-1]], self.nodes[i])
                                               and self.repair_margin(self.nodes[i], after)))]
            if reusable:
                chosen = min(reusable, key=lambda i: (distance(self.nodes[i]["point"], sample["point"]), i))
            else:
                sample = dict(sample)
                if sample["crouch"] != samples[pulled[j-1]]["crouch"] or sample["crouch"] != after["crouch"]:
                    sample.update(kind="crouch_transition", topology_protected=True)
                chosen = self.add(sample)
            if chosen != chain[-1]:
                chain.append(chosen)
        chain.append(b)
        if any(tuple(sorted((u, v))) in self.blocked or not self.visible(self.nodes[u], self.nodes[v])
               for u, v in zip(chain, chain[1:]) if u != v):
            for i in range(original_count, len(self.nodes)):
                self.grid[self.cell(self.nodes[i]["point"])].remove(i)
            del self.nodes[original_count:]
            return None
        for u, v in zip(chain, chain[1:]):
            self.link(u, v)
        return dict(chain=chain, polygons=polygons, intermediate_polygon_samples=len(samples)-2,
                    exported_turns=max(0, len(chain)-2), native_revalidation_required=True)


def objective_anchors(graph, geometry, profile):
    """Propose actual floor targets inside objective contact volumes.

    Ordinary lattice coverage can pass twenty units outside a small bomb
    trigger. Visibility to that trigger is not a reachable touch/use target.
    The resulting anchors still require native hull and connection checks.
    """
    records=geometry.get("objective_volumes") or geometry.get("inline_models",[])+geometry.get("excluded_inline_models",[])
    objectives=[];seen=set()
    for source in records:
        if source.get("classname")!="trigger_objective_info" or source.get("entity") in seen:
            continue
        seen.add(source.get("entity"));lo,hi=source.get("mins"),source.get("maxs")
        if not isinstance(lo,list) or not isinstance(hi,list) or len(lo)!=3 or len(hi)!=3:
            continue
        if any(not math.isfinite(v) for v in lo+hi) or any(a>b for a,b in zip(lo,hi)):
            continue
        center=[(a+b)*.5 for a,b in zip(lo,hi)]
        try: flags=int(source.get("properties",{}).get("spawnflags","0"))
        except ValueError: flags=0
        plant=(profile["radius"] in (14,16) and
               (source.get("target_class")=="func_explosive" or flags&128))
        face_center=[(a+b)*.5 for a,b in zip(*source["target_bounds"])] if plant and source.get("target_bounds") else center
        capture=source.get("target_class") in ("trigger_flagonly","trigger_flagonly_multiple")
        if capture:
            contact=source.get("target_bounds")
            if (not isinstance(contact,list) or len(contact)!=2 or
                    any(not isinstance(row,list) or len(row)!=3 for row in contact) or
                    any(not math.isfinite(v) for row in contact for v in row) or
                    any(a>b for a,b in zip(*contact))):
                objectives.append(dict(entity=source["entity"],status="missing_flagonly_contact_bounds",
                                       native_revalidation_required=True))
                continue
            lo,hi=contact;center=[(a+b)*.5 for a,b in zip(lo,hi)]
        def inside(node):
            p=node["point"]
            if capture:
                return capture_contact(p,[lo,hi],profile["radius"],
                    profile["crouching_height"] if node.get("crouch") else profile["standing_height"])
            if plant:
                return plant_contact(p,profile["origin_above_feet"],[lo,hi],
                    profile["radius"],profile["crouching_height"] if node.get("crouch") else profile["standing_height"],
                    source.get("target_bounds"),source.get("contact_top"))
            return (lo[0]+.125<=p[0]<=hi[0]-.125 and lo[1]+.125<=p[1]<=hi[1]-.125 and
                    lo[2]-.125<=p[2]+profile["origin_above_feet"]<=hi[2]+.125)
        covered=[i for i,node in enumerate(graph.nodes) if inside(node)]
        if covered and not plant and not capture:
            best=min(covered,key=lambda i:distance(graph.nodes[i]["point"],center))
            graph.nodes[best]["topology_protected"]=True
            graph.nodes[best]["objective_entities"]=sorted(set(graph.nodes[best].get("objective_entities",[])+[source["entity"]]))
            objectives.append(dict(entity=source["entity"],node=best,status="existing_floor_inside_contact_volume"))
            continue
        axes=[]
        for k in range(2):
            if plant or capture:
                target=source.get("target_bounds") if plant else None
                axes.append(plant_perimeter_axes(lo[k],hi[k],
                    target[0][k] if target else None,target[1][k] if target else None,profile["radius"]));continue
            middle=center[k];width=hi[k]-lo[k]
            values={middle,lo[k]+min(16.,width*.25),hi[k]-min(16.,width*.25)}
            # Include 32-unit offsets so a crate at the centre cannot hide
            # the walkable perimeter of the trigger's floor rectangle.
            for delta in (-64,-32,32,64):
                if lo[k]+.125<middle+delta<hi[k]-.125:values.add(middle+delta)
            axes.append(sorted(values))
        # A lattice candidate is not yet native proof. Keep alternatives from
        # different crate faces instead of declaring coverage after one sample
        # that may later be rejected by the real capsule/connection gate.
        pool=[(i,graph.nodes[i]) for i in covered] if plant or capture else []
        for x in axes[0]:
            for y in axes[1]:
                polygons=graph.mesh.grid.get((math.floor(x/128),math.floor(y/128)),[])
                for p in polygons:
                    point=[x,y,0.]
                    if not graph.mesh.contains(p,point):continue
                    point[2]=graph.mesh.height(p,point)
                    sample=graph.sample(point,p,"objective_contact_anchor")
                    if plant and not inside(sample) and not sample["crouch"]:
                        duck_sample=dict(sample,crouch=True)
                        if inside(duck_sample):sample=duck_sample
                    if (sample["crouch"] and not plant and not capture) or not inside(sample):continue
                    if graph.point_filter and not graph.point_filter.allows(point,p,"objective_contact_anchor"):continue
                    pool.append((None,sample))
        if not pool:
            objectives.append(dict(entity=source["entity"],status="no_standing_mesh_floor_inside_contact_volume",
                                   bounds=[lo,hi],native_revalidation_required=True))
            continue
        def order(item):
            point=item[1]["point"]
            # A solid flagpost can occupy the trigger's whole inner floor.
            # Prefer touched perimeter floors before a centre proposal uses
            # up that face's native-validation alternative.
            inner=capture and lo[0]<=point[0]<=hi[0] and lo[1]<=point[1]<=hi[1]
            return (bool(inner),distance(point,face_center if plant else center),point[2])
        ordered=sorted(pool,key=order)
        selected=[];faces=set()
        for identity,sample in ordered:
            chosen_center=face_center if plant else center
            dx,dy=sample["point"][0]-chosen_center[0],sample["point"][1]-chosen_center[1]
            face=(0,dx>=0) if abs(dx)>=abs(dy) else (1,dy>=0)
            if (plant or capture) and (face in faces or any(distance(sample["point"],old[1]["point"])<24 for old in selected)):
                continue
            selected.append((identity,sample));faces.add(face)
            if len(selected)>=(4 if plant or capture else 1):break
        for identity,sample in selected:
            sample.update(topology_protected=True,
                          objective_entities=sorted(set(sample.get("objective_entities",[])+[source["entity"]])))
            if identity is None:identity=graph.add(sample)
            objectives.append(dict(entity=source["entity"],node=identity,point=sample["point"],
                                   status="mesh_floor_contact_proposal",contact_model="tce_actor_plant_box" if plant else "flagonly_actor_capsule" if capture else "trigger_origin",
                                   native_revalidation_required=True))
    return objectives


def build(stand, duck, geometry, radius, spacing=128.):
    profile = movement_profile(stand, duck)
    """Sample the accessible surface first, then construct its local roadmap."""
    point_filter = BspPointFilters(geometry)
    leaf_filter, liquid_filter = point_filter.leaf, point_filter.liquid
    graph = SpatialGraph(stand, duck, min(spacing, radius), point_filter=point_filter)
    spacing = graph.spacing
    pool, lattice = {}, {}
    sample_step = spacing/2.

    def sample_lattice(step, target, kind):
        for p, pts in enumerate(duck.points):
            lo, hi = min(q[1] for q in pts), max(q[1] for q in pts)
            for row in range(math.ceil(lo/(step*.8660254038)), math.floor(hi/(step*.8660254038))+1):
                y = row*step*.8660254038; shift = (row & 1)*step*.5
                for column in range(math.ceil((min(q[0] for q in pts)-shift)/step),
                                    math.floor((max(q[0] for q in pts)-shift)/step)+1):
                    point = [column*step+shift, y, 0.]
                    if not duck.contains(p, point):
                        continue
                    point[2] = duck.height(p, point)
                    if not point_filter.allows(point, p, kind):
                        continue
                    key = (duck.component[p], *(round(v, 4) for v in point))
                    target.setdefault(key, graph.sample(point, p, kind))
                    if len(target) > 250000:
                        raise ValueError("spatial sample pool exceeds 250000 points; no partial graph written")

    # Keep the entire coarse lattice. Coverage distance is no longer an
    # instruction to delete neighbouring ordinary surface samples. This gives
    # open rooms several rows and alternative paths instead of a sparse spine.
    sample_lattice(spacing, lattice, "spatial_lattice_sample")
    for sample in sorted(lattice.values(), key=lambda n: (n["component"], *n["point"])):
        sample["clearance"] = round(graph.clearance(sample["point"], sample["polygon"]), 4)
        graph.add(dict(sample))
    sample_lattice(sample_step, pool, "spatial_coverage_sample")
    for p in range(len(duck.points)):
        point = duck.centers[p]
        if not point_filter.allows(point, p, "spatial_narrow_sample"):
            continue
        key = (duck.component[p], *(round(v, 4) for v in point))
        pool.setdefault(key, graph.sample(point, p, "spatial_narrow_sample"))
    for sample in pool.values():
        sample["clearance"] = round(graph.clearance(sample["point"], sample["polygon"]), 4)
    # The finer witness lattice and polygon centres fill missed alcoves and
    # narrow corridors. Prefer their central floor, but never thin the coarse
    # lattice or replace it with one point per polygon/visibility cluster.
    ordered = sorted(pool.values(), key=lambda n: (-n["clearance"], n["crouch"],
                     n["component"], n["point"][1], n["point"][0], n["point"][2]))
    for sample in ordered:
        if graph.covered_by(sample, spacing) is None:
            graph.add(dict(sample))
    owners, assigned = [], defaultdict(list)
    for p, point in enumerate(duck.centers):
        if not point_filter.allows(point, p, "polygon_center_witness"):
            owners.append(-1)
            continue
        sample = graph.sample(point, p, "spatial_coverage_repair")
        owner = graph.covered_by(sample, min(radius, spacing))
        if owner is None:
            owner = graph.add(sample)
        owners.append(owner); assigned[owner].append(p)
    spawns, spawn_errors = [], []
    seed_spawns, excluded_spawns = navigation_spawns(geometry)
    for spawn in seed_spawns:
        point = spawn["origin_bsp"][:]; point[2] -= profile["origin_above_feet"]
        found = duck.closest(point)
        if not found:
            spawn_errors.append(spawn); continue
        p, point = found
        if not point_filter.allows(point, p, "spawn_entry"):
            reason = "projected_spawn_outside_bsp_clusters" if not leaf_filter.allows(point) else "projected_spawn_in_world_liquid"
            spawn_errors.append({**spawn, "reason": reason})
            continue
        sample = graph.sample(point, p, "spawn_entry")
        owner = graph.covered_by(sample, min(128., spacing), same_stance=False)
        if owner is None:
            owner = graph.add(sample)
        graph.nodes[owner]["spawn"] = True
        spawns.append(dict(entity=spawn["entity"], polygon=p, point=point, node=owner))
    objective_seeds=objective_anchors(graph,geometry,profile)
    potential = graph.local_network()
    # The coverage partition discovers room/corridor contacts, but it no
    # longer exports a portal for each contact. Add a pulled corridor only
    # where the surface network has no reasonably direct existing route.
    contacts = set()
    for p, poly in enumerate(duck.polygons):
        for n in poly["neighbors"]:
            if n > p and owners[p] >= 0 and owners[n] >= 0 and owners[p] != owners[n]:
                contacts.add(tuple(sorted((owners[p], owners[n]))))
    corridors, unresolved = [], []
    for a, b in sorted(contacts, key=lambda e: (distance(graph.nodes[e[0]]["point"], graph.nodes[e[1]]["point"]), e)):
        direct = distance(graph.nodes[a]["point"], graph.nodes[b]["point"])
        if graph.has_route(a, b, max(spacing, direct*1.75)):
            continue
        route = graph.corridor(a, b)
        if route:
            corridors.append(dict(endpoints=[a, b], **route))
        else:
            unresolved.append(dict(nodes=[a, b], reason="no_mesh_certified_stance_corridor"))
    # Added spawn points/isolated surface samples must be attached even when
    # they did not own a polygon centre (for example at the end of an alcove).
    for a, node in enumerate(graph.nodes[:]):
        if graph.adjacency[a]:
            continue
        options = sorted((distance(node["point"], other["point"]), b)
                         for b, other in enumerate(graph.nodes) if b != a and other["component"] == node["component"])
        for _, b in options[:8]:
            route = graph.corridor(a, b)
            if route:
                corridors.append(dict(endpoints=[a, b], **route)); break
    validate_candidate_capacity(len(graph.nodes), len(graph.edges)*2, True)
    surface_witnesses = []
    for sample in pool.values():
        owner = graph.covered_by(sample, spacing)
        if owner is None:
            raise ValueError("spatial floor witness lost its visible, same-stance owner")
        surface_witnesses.append(dict(point=sample["point"], polygon=sample["polygon"], node=owner,
                                      distance=round(distance(sample["point"], graph.nodes[owner]["point"]), 4)))
    return graph.nodes, sorted(graph.edges), {
        "movement_profile": profile,
        "graph_policy": "preserve_spatial_samples", "candidate_mode": "automatic_spatial",
        "bsp_leaf_filter": leaf_filter.report(),
        "world_liquid_filter": liquid_filter.report(),
        "spatial_sampling": {"model": "regular_triangular_surface_lattice_v2", "spacing": spacing,
                             "area_sample_step": sample_step, "pool_samples": len(pool),
                             "retained_lattice_samples": len(lattice),
                             "visible_neighbor_candidates": potential,
                             "network": "local_visible_radius_neighborhood_plus_required_corridors",
                             "corridors": corridors, "crossings_are_not_errors": True},
        "coverage": {"kind": "polygon_center_visibility_witness_not_runtime_coverage",
                     "max_center_distance": min(radius, spacing), "polygon_owner": owners,
                     "clusters": [dict(node=n, polygons=ps, area=sum(duck.areas[p] for p in ps)) for n, ps in assigned.items()],
                     "witness_count": sum(owner >= 0 for owner in owners), "unrepresented_polygons": [],
                     "witness_scope": "retained_BSP_leaf_body_and_nonliquid_foot_samples",
                     "excluded_outside_bsp_polygon_centers": [p for p, owner in enumerate(owners)
                                                              if owner < 0 and not leaf_filter.allows(duck.centers[p])],
                     "excluded_world_liquid_polygon_centers": [p for p, owner in enumerate(owners)
                                                               if owner < 0 and leaf_filter.allows(duck.centers[p])],
                     "surface_pool_witness_count": len(pool), "surface_witnesses": surface_witnesses,
                     "max_surface_witness_distance": max((w["distance"] for w in surface_witnesses), default=0),
                     "mesh_area": sum(duck.areas), "continuous_coverage_proven": False},
        "mesh_components": [dict(id=i, polygons=len(ps), area=sum(duck.areas[p] for p in ps),
                                spawn_count=sum(duck.component[s["polygon"]] == i for s in spawns))
                            for i, ps in enumerate(duck.components)],
        "spawn_matches": spawns, "spawn_failures": spawn_errors, "excluded_spawn_seeds": excluded_spawns,
        "objective_contact_anchors": objective_seeds,
        "unresolved_cluster_boundaries": unresolved,
        "separate_portal_runs": 0, "crouch_transition_splits": sum(n["kind"] == "crouch_transition" for n in graph.nodes),
        "contracted_nodes": 0, "removed_edge_witnesses": [],
        "topology_policy": {"preserve_spatial_samples": True, "polygon_boundaries_are_waypoints": False,
                            "ordinary_degree_two_allowed": True, "local_alternative_routes": True,
                            "crossings_are_not_errors": True, "native_hull_validation_required": True}}


def portal_width(node, mesh):
    """Width of this exact shared polygon edge, not a room/door classification."""
    p, choices = node["polygon"], []
    for side, neighbor in enumerate(mesh.polygons[p]["neighbors"]):
        if neighbor < 0:
            continue
        a, b = mesh.points[p][side], mesh.points[p][(side+1) % len(mesh.points[p])]
        midpoint = [(x+y)*.5 for x, y in zip(a, b)]
        choices.append((distance(node["point"], midpoint), distance(a, b)))
    match, width = min(choices, default=(float("inf"), 0))
    return width if match <= 1 else None


def relax_ordinary_chains(nodes, edges, report, duck, native=None, stand=None):
    """Permit native contraction of ordinary bends, retaining narrow portals.

    No route or node is removed here. Native hull walks, owner distances and
    topology guards decide whether any contraction is actually admissible.
    """
    profile = movement_profile(stand, duck)
    adjacency = defaultdict(set)
    states = {i: node for i, node in enumerate(nodes)}
    if native is not None:
        validate_native_profile(profile, native)
        if native.get("map") != report.get("map"):
            raise ValueError("contraction evidence belongs to a different map")
        states = {node["id"]: node for node in native["nodes"]}
        for i, node in enumerate(nodes):
            expected = node["point"][:]
            expected[2] += profile["origin_above_feet"]
            if i not in states or distance(expected, states[i]["raw_position"]) > .02:
                raise ValueError(f"contraction evidence coordinate mismatch at {i}")
        for edge in native["edges"]:
            if edge["accepted"]:
                adjacency[edge["from"]].add(edge["to"])
                adjacency[edge["to"]].add(edge["from"])
    else:
        for a, b in edges:
            adjacency[a].add(b)
            adjacency[b].add(a)
    relaxed, narrow, unknown = [], [], []
    portals = {"turn_portal", "native_repair_portal"}
    helpers = {"portal_approach", "portal_exit"}
    roles = portals | helpers | {"native_repair_center"}
    for i, node in enumerate(nodes):
        node.pop("native_contraction_candidate", None)
        if node["kind"] not in roles:
            continue
        width = None
        if node["kind"] in portals:
            width = portal_width(node, duck)
            if width is None or width <= 2*profile["radius"]:
                node.pop("straight_chain_witness", None)
                node["topology_protected"] = True
                (unknown if width is None else narrow).append(i)
                continue
        state = states[i]
        if node.get("spawn") or not state.get("accepted", True) or len(adjacency[i]) != 2:
            continue
        a, b = sorted(adjacency[i])
        if any(v >= len(nodes) or nodes[v].get("spawn") for v in (a, b)):
            continue
        group = [states[v] for v in (i, a, b)]
        if any(n.get("nav_flags", 0) & ~64 for n in group) or len({n["crouch"] for n in group}) != 1:
            continue
        if any(n.get("reason", "").startswith(("ladder_", "jump_", "generated_jump")) or
               n.get("reason") == "map_entity_spawn_seed" for n in group):
            continue
        proof = {"neighbors": [a, b], "portal_width": width}
        if node["kind"] in helpers:
            # These points were inserted only when the initial region-to-portal
            # shortcut was unavailable. Later repair/junction routes can make
            # their two neighbours directly visible. Retain narrow portals and
            # actual bends; do not protect a redundant helper merely by role.
            if stand is None:
                continue
            if any(nodes[v]["kind"] in portals and
                   (portal_width(nodes[v], duck) or 0) <= 2*profile["radius"] for v in (a, b)):
                continue
            pa, pb = nodes[a]["point"], nodes[b]["point"]
            span = distance(pa, pb)
            if span > 256 or not duck.visible(pa, nodes[a]["polygon"], pb, nodes[b]["polygon"]):
                continue
            if not node["crouch"] and not stand.visible(pa, stand.locate(pa), pb, stand.locate(pb)):
                continue
            proof.update(reason="portal_helper_shared_corridor", direct_span=span)
        node["native_contraction_candidate"] = proof
        node["topology_protected"] = False
        relaxed.append({"node": i, **proof})
    report["ordinary_chain_relaxation"] = {
        "candidates": relaxed, "narrow_portals_retained": narrow, "unknown_portals_retained": unknown,
        "selection": "native_accepted_graph" if native is not None else "recast_candidate_graph",
        "native_hull_owner_topology_proofs_required": True,
        "nodes_or_edges_removed": 0}


def clean_candidate(nodes, edges, report, stand, duck):
    """Merge proven sub-two-unit duplicates; let native proof reduce straight chains.

    This does not discard rooms or remove degree-two floor samples. Those
    samples remain native coverage witnesses even if later contracted there.
    """
    edges = {tuple(sorted(edge)) for edge in edges if edge[0] != edge[1]}
    if report.get("graph_policy") == "preserve_spatial_samples":
        # Spatial spacing is the construction rule. Do not run the legacy
        # portal-chain contraction or split geometrically crossing alternatives.
        graph = SpatialGraph(stand, duck, report["spatial_sampling"]["spacing"], nodes, edges)
        for a, b in edges:
            if not (0 <= a < len(nodes) and 0 <= b < len(nodes)) or not graph.visible(nodes[a], nodes[b]):
                raise ValueError("spatial edge lost its Recast floor/stance certificate")
        groups, seen = [], set()
        for start in range(len(nodes)):
            if start in seen:
                continue
            queue, group = [start], []
            seen.add(start)
            while queue:
                u = queue.pop(); group.append(u)
                for v in graph.adjacency[u] - seen:
                    seen.add(v); queue.append(v)
            groups.append(dict(nodes=group, spawns=sum(nodes[u].get("spawn", False) for u in group),
                               bounds=[[min(nodes[u]["point"][k] for u in group) for k in range(3)],
                                       [max(nodes[u]["point"][k] for u in group) for k in range(3)]]))
        report["spatial_topology"] = {
            "components": groups,
            "isolated_nodes": [i for i in range(len(nodes)) if not graph.adjacency[i]],
            "degree_histogram": dict(Counter(len(graph.adjacency[i]) for i in range(len(nodes)))),
            "component_count": len(groups),
            "native_validation_still_required": True}
        report["candidate_cleanup"] = {"policy": "preserve_spatial_samples", "removed_nodes": 0,
                                        "split_crossings": 0, "existing_ids_and_coordinates_unchanged": True}
        for i, node in enumerate(nodes):
            node["id"] = i
        return sorted(edges)
    adjacency = defaultdict(set)
    for a, b in edges:
        adjacency[a].add(b)
        adjacency[b].add(a)
    representatives, merged = list(range(len(nodes))), []
    owners = report.get("coverage", {}).get("polygon_owner", [])
    radius = report.get("coverage", {}).get("max_center_distance", 320)
    for removed, node in enumerate(nodes):
        if node.get("spawn"):
            continue
        near = [(distance(node["point"], other["point"]), kept) for kept, other in enumerate(nodes[:removed])
                if representatives[kept] == kept and node["component"] == other["component"]
                and node["crouch"] == other["crouch"] and distance(node["point"], other["point"]) < 2]
        for separation, kept in sorted(near):
            other = nodes[kept]
            targets = (adjacency[removed] | adjacency[kept]) - {removed, kept}
            if any(not duck.visible(other["point"], other["polygon"], nodes[n]["point"], nodes[n]["polygon"])
                   for n in targets):
                continue
            if not node["crouch"] and any(not stand.visible(other["point"], stand.locate(other["point"]),
                                                             nodes[n]["point"], stand.locate(nodes[n]["point"]))
                                               for n in targets):
                continue
            if any(distance(duck.centers[p], other["point"]) > radius or
                   not duck.visible(duck.centers[p], p, other["point"], other["polygon"])
                   for p, owner in enumerate(owners) if owner == removed):
                continue
            for neighbor in list(adjacency[removed]):
                edges.discard(tuple(sorted((removed, neighbor))))
                adjacency[neighbor].discard(removed)
                if neighbor != kept:
                    edges.add(tuple(sorted((kept, neighbor))))
                    adjacency[neighbor].add(kept)
            adjacency[kept].update(targets)
            adjacency[kept].discard(removed)
            adjacency[removed].clear()
            representatives[removed] = kept
            merged.append({"removed": removed, "kept": kept, "distance": separation,
                           "neighbors": sorted(targets), "native_revalidation_required": True})
            break
    survivors = [i for i in range(len(nodes)) if representatives[i] == i]
    remap = {old: new for new, old in enumerate(survivors)}
    mapping = {old: remap[representatives[old]] for old in range(len(nodes))}
    # Existing report witnesses refer to node indices, not polygon IDs. Keep
    # these usable after removing a duplicate, with an explicit provenance map.
    def remap_ids(value):
        if isinstance(value, (list, tuple)):
            return [remap_ids(v) for v in value]
        return mapping[value]

    def remap_evidence(value):
        if isinstance(value, list):
            for item in value:
                remap_evidence(item)
        elif isinstance(value, dict):
            for key, item in list(value.items()):
                if key in ("node", "via") and isinstance(item, int):
                    value[key] = mapping[item]
                elif key in ("edge", "chain", "polygon_owner", "shared_floor_junctions", "retained_witness_segments"):
                    value[key] = remap_ids(item)
                elif key not in ("nodes", "edges", "candidate_cleanup"):
                    remap_evidence(item)

    if merged:
        remap_evidence(report)
        nodes[:] = [nodes[i] for i in survivors]
        edges = {(mapping[a], mapping[b]) for a, b in edges}
        if "native_feedback_ordering" in report:
            report["native_feedback_ordering"].update(
                existing_ids_and_coordinates_unchanged=False,
                ids_remapped_by_candidate_cleanup=True,
                preserved_node_prefix=min(item["removed"] for item in merged))
    for i, node in enumerate(nodes):
        node["id"] = i
    adjacency = defaultdict(set)
    for a, b in edges:
        adjacency[a].add(b)
        adjacency[b].add(a)
    relaxed = []
    roles = {"turn_portal", "portal_approach", "portal_exit", "native_repair_center", "native_repair_portal"}
    for i, node in enumerate(nodes):
        # Reset any old proposal before evaluating the current final topology.
        node.pop("straight_chain_witness", None)
        if node["kind"] not in roles or node.get("spawn") or len(adjacency[i]) != 2:
            continue
        a, b = sorted(adjacency[i]);left, right = nodes[a], nodes[b]
        if len({node["crouch"], left["crouch"], right["crouch"]}) != 1:
            continue
        p, pa, pb = node["point"], left["point"], right["point"]
        first, second, span = distance(p, pa), distance(p, pb), distance(pa, pb)
        if min(first, second) < 2 or span > 256:
            continue
        cosine = sum((pa[k]-p[k])*(pb[k]-p[k]) for k in range(3)) / (first*second)
        if cosine > -math.cos(math.radians(10)):
            continue
        if not duck.visible(pa, left["polygon"], pb, right["polygon"]):
            continue
        if not node["crouch"] and not stand.visible(pa, stand.locate(pa), pb, stand.locate(pb)):
            continue
        node["straight_chain_witness"] = [a, b]
        node["topology_protected"] = False
        relaxed.append({"node": i, "neighbors": [a, b], "span": span})
    report["candidate_cleanup"] = {"merged_duplicates": merged,
                                    "previous_to_current_node_ids": mapping if merged else {},
                                    "straight_chain_native_contraction_candidates": relaxed,
                                    "native_revalidation_required": True,
                                    "floor_samples_removed_except_certified_duplicates": 0}
    relax_ordinary_chains(nodes, edges, report, duck, stand=stand)
    return sorted(edges)


def organize_feedback_edges(nodes, edges, report, stand, duck):
    """Order repair routes without invalidating any existing native node ID."""
    previous_points = [node["point"][:] for node in nodes]
    previous_edges = len(edges)
    ordered, evidence = organize_room_edges(nodes, {tuple(sorted(edge)) for edge in edges}, stand, duck)
    if any(node["point"] != point for node, point in zip(nodes, previous_points)):
        raise ValueError("feedback ordering moved an existing node")
    if len(nodes) > 4096 or len(ordered) * 2 > 65536:
        raise ValueError("ordered feedback graph exceeds native graph budget")
    report["native_feedback_ordering"] = {
        "preserved_node_prefix": len(previous_points),
        "existing_ids_and_coordinates_unchanged": True,
        "added_junction_nodes": len(nodes) - len(previous_points),
        "directed_edge_change": 2 * (len(ordered) - previous_edges),
        "input_route_chains_are_pre_ordering_proposals": True,
        **evidence}
    return sorted(ordered)


def native_ordinary_pairs(feedback, count):
    """Only collision-proved, bidirectional ordinary links form a repair guide.

    Quarantine removes reachability, not the earlier collision proof. Candidate
    links and a proved one-way action must never masquerade as a walk detour.
    """
    native = {n["id"]: n for n in feedback["nodes"]}
    directed = set()
    for edge in feedback["edges"]:
        a, b = edge["from"], edge["to"]
        if not (0 <= a < count and 0 <= b < count) or a == b:
            continue
        if not edge.get("accepted") and edge.get("reason") != "quarantined_unreachable_endpoint":
            continue
        if any(native[i].get("nav_flags", 0)&~64 for i in (a, b)):
            continue
        if edge.get("reason") not in {"external_native_validated_walk", "native_validated_walk", "accepted",
                                      "external_native_component_bridge",
                                      "quarantined_unreachable_endpoint"}:
            continue
        directed.add((a, b))
    return {(a, b) for a, b in directed if a < b and (b, a) in directed}


def complete_native_stubs(graph, native, proved_pairs, rejected_pairs, limit=64):
    """Continue a short dead end past its hub when both meshes certify it.

    Only existing two-hop neighbours are candidates; this cannot grow a dense
    all-visible graph. The new edge still requires the normal native import.
    """
    adjacent = defaultdict(set)
    for a, b in proved_pairs:
        adjacent[a].add(b); adjacent[b].add(a)
    proposals = []
    for a in sorted(adjacent):
        if len(proposals) >= limit:
            break
        if len(adjacent[a]) != 1 or not native[a].get("accepted"):
            continue
        hub = next(iter(adjacent[a])); pa = graph.nodes[a]["point"]; ph = graph.nodes[hub]["point"]
        length = distance(pa, ph)
        if length < 2 or length > min(64., graph.spacing*.5):
            continue
        choices = []
        for b in adjacent[hub] - {a}:
            pair = tuple(sorted((a, b)))
            if pair in rejected_pairs or not native[b].get("accepted"):
                continue
            nb, na = graph.nodes[b], graph.nodes[a]
            pb = nb["point"]; gap = distance(pa, pb)
            if na["component"] != nb["component"] or na["crouch"] != nb["crouch"]:
                continue
            if gap < 8 or gap > graph.spacing*1.5 or abs(pa[2]-pb[2]) > 18.25:
                continue
            forward = sum((pa[k]-ph[k])*(pb[k]-pa[k]) for k in range(2))
            if forward <= .25*length*gap or not graph.visible(na, nb):
                continue
            choices.append((gap, b))
        for _, b in sorted(choices):
            if graph.link(a, b):
                proposals.append(dict(edge=[a, b], hub=hub, kind="short_native_dead_end_continuation",
                                      status="candidate_awaiting_native_revalidation"))
                break
    return proposals


def connect_native_components(graph, native, proved_pairs, base_count, limit=24):
    """Join proven floor components with new mesh-certified corridor proposals.

    Quarantined floors retain their earlier physical floor proof. Connecting
    two detached pieces can expose a room's real entrance; selecting only
    already spawn-reachable points leaves interleaved lower-floor pieces out.
    Every proposed segment still needs a new native directed hull check.
    """
    def ordinary(i):
        state=native.get(i,{})
        floor=state.get("accepted") or state.get("native_input",{}).get("floor_accepted")
        return (i<base_count and floor and not state.get("nav_flags",0)&~64 and
                not state.get("reason","").startswith(("ladder_","jump_","generated_","walk_drop")))
    floor_ids=[i for i in range(base_count) if ordinary(i)]
    parents={i:i for i in floor_ids}
    def root(i):
        while parents[i]!=i:
            parents[i]=parents[parents[i]];i=parents[i]
        return i
    def join(a,b):
        a,b=root(a),root(b)
        if a!=b:parents[b]=a
    for a,b in proved_pairs:
        if a in parents and b in parents:join(a,b)
    frontiers={}
    for a in floor_ids:
        na=graph.nodes[a]
        for b in graph.near(na["point"],min(768.,graph.spacing*4)):
            if b<=a or b not in parents or root(a)==root(b):continue
            nb=graph.nodes[b]
            if na["component"]!=nb["component"] or na["crouch"]!=nb["crouch"]:continue
            pair=tuple(sorted((root(a),root(b))))
            gap=distance(na["point"],nb["point"])
            item=(0 if native[a].get("spawn_reachable") or native[b].get("spawn_reachable") else 1,gap,a,b)
            pool=frontiers.setdefault(pair,[])
            pool.append(item);pool.sort()
            del pool[8:]
    attempts,proposals=[],[]
    for _,_,a,b in sorted(item for pool in frontiers.values() for item in pool):
        if len(proposals)>=limit or len(attempts)>=96 or len(graph.nodes)>=SPATIAL_NODE_LIMIT-96:break
        if root(a)==root(b):continue
        if tuple(sorted((a,b))) not in graph.blocked and graph.link(a,b):
            route=dict(chain=[a,b],exported_turns=0,native_revalidation_required=True)
        else:
            route=graph.corridor(a,b,force_bend=True)
        attempts.append(dict(edge=[a,b],status="candidate_awaiting_native_revalidation" if route else "no_mesh_corridor"))
        if route:
            proposals.append(dict(edge=[a,b],**route));join(a,b)
    return dict(model="physical_floor_components_recast_corridors_v1",floor_nodes=len(floor_ids),
                frontier_pairs=len(frontiers),attempts=attempts,proposals=proposals,native_revalidation_required=True)


def connect_native_isolates(graph, native, native_edges, base_count, limit=16):
    """Propose bounded floor corridors when an ordinary native sample has no exit.

    A rejected endpoint can invalidate the only coarse neighbour of a real
    spawn. Retain both inputs and route through the existing Recast polygon
    graph instead. Native state selects candidates, never certifies new edges.
    Generated native action IDs must not be confused with new feedback IDs.
    """
    outgoing = {e['from'] for e in native_edges if e.get('accepted')}
    def ordinary(i):
        state = native.get(i, {})
        return (0 <= i < base_count and state.get('accepted') and state.get('nav_flags') in (0, 64)
                and not state.get('reason', '').startswith(('ladder_', 'jump_', 'generated_', 'walk_drop')))
    isolates = sorted(i for i in native if ordinary(i) and i not in outgoing)
    attempts, proposals = [], []
    for a in isolates[:limit]:
        na = graph.nodes[a]
        choices = sorted((distance(na['point'], graph.nodes[b]['point']), b)
                         for b in outgoing if ordinary(b)
                         and graph.nodes[b]['component'] == na['component']
                         and graph.nodes[b]['crouch'] == na['crouch']
                         and abs(graph.nodes[b]['point'][2]-na['point'][2]) <= 18.25)
        added = 0
        for gap, b in choices[:24]:
            if gap > min(512., graph.spacing*3) or len(graph.nodes) >= SPATIAL_NODE_LIMIT-96:
                break
            if graph.has_route(a, b, min(512., gap*1.25)):
                continue
            route = graph.corridor(a, b, force_bend=True)
            attempts.append(dict(edge=[a,b], distance=gap,
                                 status='candidate_awaiting_native_revalidation' if route
                                 else 'no_new_mesh_certified_corridor'))
            if route:
                proposals.append(dict(edge=[a,b], **route))
                added += 1
                if added >= 2:
                    break
    return dict(model='bounded_same_recast_component_corridor_proposal_v1',
                isolated_base_nodes=isolates, isolate_limit=limit, targets_per_isolate=24,
                successful_corridors_per_isolate=2, attempts=attempts, proposals=proposals,
                native_revalidation_required=True)


def add_native_feedback(nodes, edges, report, stand, duck, feedback_path, point_filter=None):
    """Add explicit bends where native hulls rejected a Recast shortcut.

    Native results never make a candidate valid here. They identify paths that
    need a less aggressive proposal. Each retry follows the polygon dual graph
    through centers and portal midpoints, and is submitted to native collision
    again. Failed floor samples, hazards and missing geometry are not bypassed.
    """
    raw = Path(feedback_path).read_bytes()
    feedback = json.loads(raw)
    profile = movement_profile(stand, duck)
    validate_native_profile(profile, feedback)
    if report.get("movement_profile", profile) != profile:
        raise ValueError("candidate movement profile differs from Recast meshes")
    report["movement_profile"] = profile
    native = {n["id"]: n for n in feedback["nodes"]}
    if feedback.get("map") != report.get("map"):
        raise ValueError("native feedback map differs from candidate")
    for i, node in enumerate(nodes):
        if i not in native:
            raise ValueError("native feedback does not cover the base candidate")
        expected = node["point"][:]
        expected[2] += profile["origin_above_feet"]
        if distance(expected, native[i]["raw_position"]) > .02:
            raise ValueError(f"native feedback candidate coordinate mismatch at {i}")
    blocked = ordinary_collision_rejections()
    floors = {"external_native_validated_floor", "external_stair_floor_reprojected", "map_entity_spawn_seed",
              "quarantined_no_validated_route_from_spawn", "contracted_to_validated_walk_neighbor"}
    retries = sorted({tuple(sorted((e["from"], e["to"]))) for e in feedback["edges"]
                      if e["reason"] in blocked and e["from"] < len(nodes) and e["to"] < len(nodes)
                      and native[e["from"]]["reason"] in floors and native[e["to"]]["reason"] in floors})
    if report.get("graph_policy") == "preserve_spatial_samples":
        original_nodes = len(nodes)
        blocked_pairs = set(retries)
        original_edges = {tuple(sorted(edge)) for edge in edges}
        proved_pairs = native_ordinary_pairs(feedback, original_nodes) - blocked_pairs
        graph = SpatialGraph(stand, duck, report["spatial_sampling"]["spacing"], nodes, proved_pairs,
                             point_filter=point_filter)
        graph.blocked = blocked_pairs
        evidence = []
        for a, b in retries:
            if graph.has_route(a, b, max(graph.spacing, distance(nodes[a]["point"], nodes[b]["point"])*1.75)):
                evidence.append(dict(edge=[a, b], status="native_or_new_mesh_proposal_local_alternative"))
                continue
            if len(nodes) >= SPATIAL_NODE_LIMIT-96:
                evidence.append(dict(edge=[a, b], status="repair_node_reserve_exhausted"))
                continue
            route = graph.corridor(a, b, force_bend=True)
            evidence.append(dict(edge=[a, b], status="candidate_awaiting_native_revalidation" if route
                                 else "no_new_mesh_certified_corridor", **(route or {})))
        rejected_pairs = {tuple(sorted((e["from"], e["to"]))) for e in feedback["edges"]
                          if not e.get("accepted") and e.get("reason") != "quarantined_unreachable_endpoint"}
        continuations = complete_native_stubs(graph, native, proved_pairs, rejected_pairs)
        isolated_repairs = connect_native_isolates(graph, native, feedback['edges'], original_nodes)
        component_repairs=(connect_native_components(graph,native,proved_pairs,original_nodes)
                           if feedback.get("native_input_coverage") else {})
        # Native walk witnesses guide the search but are not Recast edge
        # certificates. The engine can connect a real tread/step that this
        # mesh does not represent. Re-export only the preserved input and
        # newly mesh-certified proposals, never injected helper witnesses.
        # The next native pass will independently recover its own bridges.
        native_only_guides = proved_pairs - original_edges
        result_edges = original_edges | (graph.edges - native_only_guides)
        if len(result_edges)*2 > 253952:
            raise ValueError("native feedback repairs exceed spatial directed edge budget")
        report["native_feedback_repairs"] = {
            "source": str(feedback_path), "sha256": hashlib.sha256(raw).hexdigest(),
            "source_generation_complete": feedback.get("generation_complete"),
            "source_is_acceptance_certificate": False,
            "preserved_node_prefix": original_nodes,
            "existing_ids_and_coordinates_unchanged": True,
            "added_nodes": len(nodes)-original_nodes,
            "existing_links_preserved": True,
            "native_collision_proved_bidirectional_pairs": len(proved_pairs),
            "native_guidance_edges_not_reexported": sorted(native_only_guides),
            "added_directed_edges": 2*(len(result_edges)-len(original_edges)),
            "component_corridors":component_repairs,
            "blocked_pairs_excluded_from_repair_guides": sorted(blocked_pairs),
            "short_dead_end_continuations": continuations,
            "isolated_native_floor_repairs": isolated_repairs,
            "point_filters_new_guides": point_filter.report() if point_filter else None,
            "routes": evidence}
        return sorted(result_edges)
    new_edges = {tuple(sorted(edge)) for edge in edges}
    points = {}
    for i, node in enumerate(nodes):
        points.setdefault((node["component"], *(round(v, 3) for v in node["point"])), i)

    def route(first, last):
        if duck.component[first] != duck.component[last]:
            return None
        queue = [(distance(duck.centers[first], duck.centers[last]), 0., first)]
        costs, parents = {first: 0.}, {}
        while queue:
            _, cost, p = heapq.heappop(queue)
            if cost != costs.get(p):
                continue
            if p == last:
                result = [p]
                while p in parents:
                    p = parents[p]
                    result.append(p)
                return list(reversed(result))
            for n in duck.polygons[p]["neighbors"]:
                if n < 0:
                    continue
                step = distance(duck.centers[p], duck.centers[n])
                candidate = cost + step
                if candidate < costs.get(n, float("inf")):
                    costs[n], parents[n] = candidate, p
                    heapq.heappush(queue, (candidate + distance(duck.centers[n], duck.centers[last]), candidate, n))
        return None

    evidence = []
    before_nodes, before_edges = len(nodes), len(new_edges)
    for a, b in retries:
        polygon_route = route(nodes[a]["polygon"], nodes[b]["polygon"])
        if polygon_route is None:
            evidence.append({"edge": [a, b], "status": "no_polygon_route"})
            continue
        proposals = [(nodes[a]["point"], nodes[a]["polygon"], "feedback_start")]
        for position, p in enumerate(polygon_route):
            proposals.append((duck.centers[p], p, "native_repair_center"))
            if position + 1 < len(polygon_route):
                n = polygon_route[position + 1]
                side = duck.polygons[p]["neighbors"].index(n)
                pts = duck.points[p]
                midpoint = [(pts[side][k] + pts[(side + 1) % len(pts)][k]) * .5 for k in range(3)]
                proposals.append((midpoint, p, "native_repair_portal"))
        proposals.append((nodes[b]["point"], nodes[b]["polygon"], "feedback_end"))
        expanded, valid = [proposals[0]], True
        for previous, current in zip(proposals, proposals[1:]):
            pa, ap, _ = previous
            pb, bp, kind = current
            if not duck.visible(pa, ap, pb, bp):
                valid = False
                break
            count = max(1, math.ceil(distance(pa, pb) / 400))
            for step in range(1, count):
                q = [pa[k] + (pb[k] - pa[k]) * step / count for k in range(3)]
                qp = duck.locate(q, tolerance=24)
                if qp is None:
                    valid = False
                    break
                q[2] = duck.height(qp, q)
                expanded.append((q, qp, "native_repair_long_segment"))
            expanded.append(current)
        if not valid:
            evidence.append({"edge": [a, b], "status": "mesh_route_certificate_failed"})
            continue
        if len(nodes) + len(expanded) >= 4096:
            evidence.append({"edge": [a, b], "status": "candidate_node_budget"})
            continue
        chain = []
        for point, p, kind in expanded:
            key = (duck.component[p], *(round(v, 3) for v in point))
            if key not in points:
                points[key] = len(nodes)
                nodes.append({"point": point[:], "polygon": p, "crouch": stand.locate(point) is None,
                              "spawn": False, "kind": kind, "component": duck.component[p]})
            identity = points[key]
            if identity in chain:
                chain = chain[:chain.index(identity) + 1]
            else:
                chain.append(identity)
        for x, y in zip(chain, chain[1:]):
            new_edges.add(tuple(sorted((x, y))))
        evidence.append({"edge": [a, b], "status": "candidate_awaiting_native_revalidation",
                         "polygons": polygon_route, "chain": chain})
    if len(new_edges) * 2 > 65536:
        raise ValueError("native feedback repairs exceed edge budget")
    # Reaching a boundary endpoint early can make the last polygon-center
    # excursion redundant. Remove these unlinked new samples while preserving
    # all base IDs used by the native feedback report.
    used = {i for edge in new_edges for i in edge} | set(range(before_nodes))
    remap = {old: new for new, old in enumerate(sorted(used))}
    nodes[:] = [node for i, node in enumerate(nodes) if i in used]
    new_edges = {(remap[a], remap[b]) for a, b in new_edges}
    for item in evidence:
        if "chain" in item:
            item["chain"] = [remap[i] for i in item["chain"]]
    report["native_feedback_repairs"] = {"source": str(feedback_path),
                                        "sha256": hashlib.sha256(raw).hexdigest(),
                                        "source_generation_complete": feedback.get("generation_complete"),
                                        "source_is_acceptance_certificate": False,
                                        "added_nodes": len(nodes) - before_nodes,
                                        "added_directed_edges": 2 * (len(new_edges) - before_edges),
                                        "routes": evidence}
    return organize_feedback_edges(nodes, new_edges, report, stand, duck)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--stand", required=True, type=Path)
    parser.add_argument("--duck", required=True, type=Path)
    parser.add_argument("--geometry", required=True, type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--native-feedback", type=Path, help="native JSON rejection report; retries remain unvalidated")
    parser.add_argument("--base-candidate", type=Path, help="existing candidate JSON matching the feedback node IDs")
    parser.add_argument("--coverage-radius", type=float, default=320.)
    parser.add_argument("--spacing", type=float, default=128., help="ordinary triangular lattice spacing (96..256 units)")
    args = parser.parse_args()
    if not 96 <= args.coverage_radius <= 320:
        parser.error("coverage radius must be 96..320")
    if not 96 <= args.spacing <= 256:
        parser.error("spacing must be 96..256")
    geometry = json.loads(args.geometry.read_text(encoding="utf-8"))
    name = geometry["map"]
    if not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", name):
        parser.error("invalid map name")
    inputs = {args.stand.resolve(), args.duck.resolve(), args.geometry.resolve()}
    if args.out.resolve() in inputs or args.out.with_suffix(".json").resolve() in inputs:
        parser.error("output would overwrite an input")
    stand, duck = Mesh(args.stand), Mesh(args.duck)
    profile = movement_profile(stand, duck)
    if args.base_candidate:
        if not args.native_feedback:
            parser.error("--base-candidate requires --native-feedback")
        report = json.loads(args.base_candidate.read_text(encoding="utf-8"))
        if report["sources"]["stand"]["sha256"] != stand.sha256 or report["sources"]["duck"]["sha256"] != duck.sha256:
            parser.error("base candidate and current meshes differ")
        if report.get("movement_profile", profile if profile["radius"] == 16 else None) != profile:
            parser.error("base candidate movement profile differs from current meshes")
        nodes, edges = report["nodes"], report["edges"]
    else:
        nodes, edges, report = build(stand, duck, geometry, args.coverage_radius, args.spacing)
    report["map"] = name
    report["movement_profile"] = profile
    if args.native_feedback:
        point_filter = BspPointFilters(geometry)
        edges = add_native_feedback(nodes, edges, report, stand, duck, args.native_feedback, point_filter)
    edges = clean_candidate(nodes, edges, report, stand, duck)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    version = 4 if report.get("graph_policy") == "preserve_spatial_samples" else 2
    validate_candidate_capacity(len(nodes), len(edges)*2, version == 4)
    rows = [f"TCE_NAVMESH {version} {name} {len(nodes)} {len(edges) * 2}"]
    for i, node in enumerate(nodes):
        node["topology_protected"] = topology_protected(node)
        rows.append("%d %.5f %.5f %.5f %d %d %d" %
                    (i, *node["point"], node["crouch"], node["spawn"], node["topology_protected"]))
    for a, b in edges:
        rows.extend((f"{a} {b}", f"{b} {a}"))
    text = "\n".join(rows) + "\n"
    payload = text.encode("ascii")
    if len(payload) > CANDIDATE_BYTES_LIMIT:
        raise ValueError("candidate exceeds native 5 MiB text input capacity; no candidate written")
    # The report hashes these exact bytes. Text-mode newline conversion on
    # Windows otherwise writes CRLF while recording the hash of LF content.
    args.out.write_bytes(payload)
    report.update({"schema": 1, "map": name, "status": "candidate_requires_native_hull_validation",
                   "sources": {"stand": {"path": str(args.stand), "sha256": stand.sha256},
                               "duck": {"path": str(args.duck), "sha256": duck.sha256},
                               "geometry": {"path": str(args.geometry), "sha256": hashlib.sha256(args.geometry.read_bytes()).hexdigest()}},
                   "geometry_complete": geometry.get("geometry_complete", False),
                   "candidate_sha256": hashlib.sha256(payload).hexdigest(),
                   "input_capacity": {"nodes": SPATIAL_NODE_LIMIT if version == 4 else 4096,
                                      "directed_links": SPATIAL_DIRECTED_LINK_LIMIT if version == 4 else 65536,
                                      "text_bytes": CANDIDATE_BYTES_LIMIT,
                                      "requires_native_16k_storage": version == 4 and
                                      (len(nodes) > 7680 or len(edges)*2 > 122880)},
                   "nodes": [{"id": i, **node} for i, node in enumerate(nodes)],
                   "edges": [[a, b] for a, b in edges],
                   "counts": {"nodes": len(nodes), "directed_edges": len(edges) * 2,
                              "stand_polygons": len(stand.polygons), "duck_polygons": len(duck.polygons),
                              "crouch_nodes": sum(n["crouch"] for n in nodes),
                              "kinds": dict(Counter(n["kind"] for n in nodes))},
                   "limitations": ["Recast circular clearance is not the native TCE square hull.",
                                   "Every node and directed edge must be validated by the server before WAY export.",
                                   "Disconnected surfaces remain in this report; ladders/jumps require explicit movement validation.",
                                   "Polygon-center coverage does not prove every square unit or objective is reachable."]})
    args.out.with_suffix(".json").write_text(json.dumps(report, separators=(",", ":")) + "\n", encoding="utf-8")
    print(json.dumps({"candidate": str(args.out), **report["counts"],
                      "components": len(duck.components), "spawn_failures": len(report["spawn_failures"]),
                      "unresolved_boundaries": len(report["unresolved_cluster_boundaries"])}))


if __name__ == "__main__":
    main()
