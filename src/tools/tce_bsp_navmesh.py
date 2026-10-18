"""Prepare ET IBSP47 collision geometry for an external navmesh experiment.

This is a geometry preprocessor, not a waypoint generator. It exports the
boundaries of solid/playerclip world brushes, including invisible clip brushes,
as an outward-wound, Y-up OBJ. Solid patch control grids use the engine's
collision subdivision threshold. Inline brush/patch models use an optional
runtime entity snapshot, or an explicitly provisional BSP spawn-state policy.

Example:
  python src/tools/tce_bsp_navmesh.py map.pk3 --map obj_overkill --out-dir preview
  python src/tools/tce_bsp_navmesh.py --self-test

The manifest always says geometry_candidate_not_validated. Brush union,
playable-region filtering, native collision comparison, special traversal and
movement tests are still required before any Omni-bot navigation can be made.
Only the Python standard library is required; no game process is contacted.
"""

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import sys
import zipfile

from tce_bsp_patch import collision_patch_triangles


# ET IBSP47, checked against ETLegacy src/qcommon/qfiles.h and cm_load.c.
HEADER_SIZE = 8 + 17 * 8
LUMP_NAMES = (
    "entities", "shaders", "planes", "nodes", "leafs", "leafsurfaces",
    "leafbrushes", "models", "brushes", "brushsides", "drawverts",
    "drawindexes", "fogs", "surfaces", "lightmaps", "lightgrid", "visibility",
)
RECORD_SIZES = {1: 72, 2: 16, 3: 36, 4: 48, 5: 4, 6: 4, 7: 40,
                8: 12, 9: 8, 10: 44, 11: 4, 12: 72, 13: 104}
MAP_NAME = re.compile(r"[A-Za-z0-9_-]{1,64}\Z")
MAX_BSP_BYTES = 256 * 1024 * 1024
EPSILON = 0.001


def sha256_file(path):
    result = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()


def source_flags():
    """Read the authoritative production constants, rather than guess flags."""
    path = Path(__file__).resolve().parents[1] / "game" / "surfaceflags.h"
    text = path.read_text(encoding="utf-8")
    required = ("CONTENTS_SOLID", "CONTENTS_PLAYERCLIP", "SURF_LADDER",
                "SURF_SKY", "SURF_SLICK", "SURF_NONSOLID")
    flags = {}
    for name in required:
        match = re.search(r"^\s*#define\s+" + name + r"\s+(0x[\da-fA-F]+)\b",
                          text, re.MULTILINE)
        if not match:
            raise ValueError(f"missing numeric constant {name} in {path}")
        flags[name] = int(match.group(1), 16)
    return flags, {"path": "src/game/surfaceflags.h", "sha256": sha256_file(path),
                   "values": flags}


def read_input(path, map_name):
    if path.suffix.lower() == ".pk3":
        if not map_name or not MAP_NAME.fullmatch(map_name):
            raise ValueError("PK3 input requires --map with a simple map name")
        member_name = f"maps/{map_name}.bsp"
        with zipfile.ZipFile(path) as archive:
            matches = [item for item in archive.infolist()
                       if item.filename.lower() == member_name.lower()]
            if len(matches) != 1:
                raise ValueError(f"expected one {member_name}; found {len(matches)}")
            member = matches[0]
            if not 0 < member.file_size <= MAX_BSP_BYTES:
                raise ValueError("BSP archive member exceeds the input size limit")
            data = archive.read(member)
        input_member = member.filename
    elif path.suffix.lower() == ".bsp":
        map_name = map_name or path.stem
        if not MAP_NAME.fullmatch(map_name):
            raise ValueError("invalid map name")
        if not 0 < path.stat().st_size <= MAX_BSP_BYTES:
            raise ValueError("BSP exceeds the input size limit")
        data = path.read_bytes()
        input_member = None
    else:
        raise ValueError("input must be an explicit .bsp or .pk3 file")
    return data, map_name, {
        "path": str(path.resolve()), "sha256": sha256_file(path),
        "archive_member": input_member, "bsp_bytes": len(data),
        "bsp_sha256": hashlib.sha256(data).hexdigest(),
    }


def read_lumps(data):
    if len(data) < HEADER_SIZE or data[:4] != b"IBSP":
        raise ValueError("not an IBSP file or truncated header")
    version, = struct.unpack_from("<i", data, 4)
    if version != 47:
        raise ValueError(f"expected ET BSP version 47, got {version}")
    records, intervals = [], []
    for index, name in enumerate(LUMP_NAMES):
        offset, length = struct.unpack_from("<ii", data, 8 + index * 8)
        if offset < 0 or length < 0 or offset + length > len(data):
            raise ValueError(f"{name}: lump outside BSP bounds")
        if length and offset < HEADER_SIZE:
            raise ValueError(f"{name}: lump overlaps BSP header")
        if index in RECORD_SIZES and length % RECORD_SIZES[index]:
            raise ValueError(f"{name}: invalid record alignment")
        if length:
            intervals.append((offset, offset + length, name))
        records.append({"name": name, "offset": offset, "length": length})
    intervals.sort()
    for previous, current in zip(intervals, intervals[1:]):
        if current[0] < previous[1]:
            raise ValueError(f"overlapping lumps: {previous[2]}, {current[2]}")
    return records


def unpack_lump(data, lumps, number, fmt):
    lump = lumps[number]
    return list(struct.iter_unpack(fmt, data[lump["offset"]:
                                             lump["offset"] + lump["length"]]))


def valid_range(first, count, total, label):
    if first < 0 or count < 0 or first + count > total:
        raise ValueError(f"{label}: invalid record range {first}+{count}/{total}")


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
            a[0] * b[1] - a[1] * b[0])


def subtract(a, b):
    return tuple(x - y for x, y in zip(a, b))


def normalized(vector):
    length = math.sqrt(dot(vector, vector))
    if not math.isfinite(length) or length < 1e-8:
        raise ValueError("invalid zero/non-finite normal")
    return tuple(value / length for value in vector)


def y_up(point):
    # Rotation (determinant +1), so triangle winding is preserved.
    return point[0], point[2], -point[1]


def bounds(points):
    points = list(points)
    if not points:
        return None
    return {"min": [min(p[axis] for p in points) for axis in range(3)],
            "max": [max(p[axis] for p in points) for axis in range(3)]}


def clip_polygon(polygon, normal, distance):
    """Clip a convex face to the brush interior n.p <= d."""
    if not polygon:
        return []
    result = []
    previous = polygon[-1]
    previous_distance = dot(normal, previous) - distance
    for current in polygon:
        current_distance = dot(normal, current) - distance
        previous_inside = previous_distance <= EPSILON
        current_inside = current_distance <= EPSILON
        if previous_inside != current_inside:
            fraction = previous_distance / (previous_distance - current_distance)
            result.append(tuple(a + fraction * (b - a)
                                for a, b in zip(previous, current)))
        if current_inside:
            result.append(current)
        previous, previous_distance = current, current_distance
    # Clipping through an existing corner can emit a duplicate vertex.
    clean = []
    for point in result:
        if not clean or dot(subtract(point, clean[-1]),
                            subtract(point, clean[-1])) > 1e-12:
            clean.append(point)
    if len(clean) > 1 and dot(subtract(clean[0], clean[-1]),
                             subtract(clean[0], clean[-1])) < 1e-12:
        clean.pop()
    return clean


def brush_faces(planes, extent):
    """Return (plane_index, polygon) faces with their outward BSP winding."""
    faces, seen = [], set()
    for face_index, (normal, distance) in enumerate(planes):
        key = tuple(round(v, 7) for v in (*normal, distance))
        if key in seen:
            continue
        seen.add(key)
        reference_axis = min(range(3), key=lambda axis: abs(normal[axis]))
        reference = tuple(1.0 if i == reference_axis else 0.0 for i in range(3))
        u = normalized(cross(reference, normal))
        v = cross(normal, u)
        center = tuple(value * distance for value in normal)
        polygon = [tuple(center[i] + extent * (su * u[i] + sv * v[i])
                         for i in range(3))
                   for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        for other_index, (other_normal, other_distance) in enumerate(planes):
            if other_index != face_index:
                polygon = clip_polygon(polygon, other_normal, other_distance)
                if len(polygon) < 3:
                    break
        if len(polygon) < 3:
            continue
        for point in polygon:
            delta = subtract(point, center)
            if max(abs(dot(delta, u)), abs(dot(delta, v))) >= extent - EPSILON:
                raise ValueError("brush appears unbounded at clipping extent")
        area_normal = (0.0, 0.0, 0.0)
        for a, b in zip(polygon[1:-1], polygon[2:]):
            triangle_normal = cross(subtract(a, polygon[0]),
                                    subtract(b, polygon[0]))
            area_normal = tuple(x + y for x, y in zip(area_normal, triangle_normal))
        if dot(area_normal, normal) > 1e-7:
            faces.append((face_index, polygon))
    if len(faces) < 4:
        raise ValueError("brush has fewer than four non-degenerate boundary faces")
    return faces


def parse_entities(raw):
    text = raw.rstrip(b"\x00").decode("latin-1")
    tokens = []
    position = 0
    token = re.compile(r'\s+|//[^\n]*|/\*.*?\*/|[{}]|"(?:\\.|[^"\\])*"', re.DOTALL)
    while position < len(text):
        match = token.match(text, position)
        if not match:
            raise ValueError(f"unsupported entity text at byte {position}")
        value = match.group()
        position = match.end()
        if value.isspace() or value.startswith(("//", "/*")):
            continue
        if value.startswith('"'):
            value = re.sub(r'\\(["\\])', r'\1', value[1:-1])
        tokens.append(value)
    entities, index = [], 0
    while index < len(tokens):
        if tokens[index] != "{":
            raise ValueError("entity opening brace expected")
        index += 1
        entity = {}
        while index < len(tokens) and tokens[index] != "}":
            if index + 1 >= len(tokens) or tokens[index + 1] in ("{", "}"):
                raise ValueError("entity key without value")
            entity[tokens[index]] = tokens[index + 1]
            index += 2
        if index >= len(tokens):
            raise ValueError("unterminated entity")
        entities.append(entity)
        index += 1
    return entities


def point_text(value):
    try:
        point = tuple(map(float, value.split()))
    except ValueError:
        return None
    return point if len(point) == 3 and all(map(math.isfinite, point)) else None


def entity_transform(origin, angles):
    """Model local -> world inverse of ET's AngleVectors trace rotation.

    BSP submodels already subtract their origin brush pivot at map compile;
    do not subtract the entity origin a second time. Angles are pitch/yaw/roll.
    """
    if (len(origin) != 3 or len(angles) != 3
            or not all(map(math.isfinite, (*origin, *angles)))):
        raise ValueError("non-finite inline model transform")
    pitch, yaw, roll = map(math.radians, angles)
    sp, cp, sy, cy, sr, cr = (math.sin(pitch), math.cos(pitch),
                             math.sin(yaw), math.cos(yaw),
                             math.sin(roll), math.cos(roll))
    forward = (cp*cy, cp*sy, -sp)
    left = (sr*sp*cy-cr*sy, sr*sp*sy+cr*cy, sr*cp)
    up = (cr*sp*cy+sr*sy, cr*sp*sy-sr*cy, cr*cp)
    def rotate(point):
        return tuple(point[0]*forward[i] + point[1]*left[i] + point[2]*up[i]
                     for i in range(3))
    def transform(point):
        return tuple(a+b for a, b in zip(rotate(point), origin))
    return transform, rotate


def inline_instances(models, entities, mask, runtime=None):
    """Explicit collision state; triggers never acquire solidity from shaders."""
    instances = []
    if runtime is not None:
        if not isinstance(runtime.get("models"), list):
            raise ValueError("runtime snapshot requires a models list")
        seen_entities = set()
        for state in runtime["models"]:
            required = {"entity", "modelindex", "linked", "contents", "origin", "angles"}
            if (not isinstance(state, dict) or not required.issubset(state)
                    or not isinstance(state["linked"], bool)):
                raise ValueError("runtime snapshot has an invalid model state")
            if state["entity"] in seen_entities:
                raise ValueError("runtime snapshot duplicates an entity")
            seen_entities.add(state["entity"])
            model = int(state["modelindex"])
            valid_range(model, 1, len(models), "runtime inline model")
            if model == 0:
                continue
            enabled = bool(state["linked"]) and bool(int(state["contents"]) & mask)
            moving=(state.get("stationary") is False or state.get("classname") in {
                "func_rotating","func_bobbing","func_pendulum","func_train","func_plat",
                "func_door","func_door_rotating","func_secret","script_mover"})
            reason="linked_collision_snapshot" if enabled else "runtime_unlinked_or_non_player_collision"
            if enabled and moving:
                # A rotating arm's arbitrary pose must not become permanent
                # Recast floor/wall geometry. Keep it as a dynamic obstacle;
                # final engine hull queries still collide with it normally.
                enabled=False;reason="dynamic_obstacle_requires_current_native_gate"
            instances.append({**state, "model": model, "enabled": enabled,
                              "state_source": state.get("state_source", "runtime_snapshot"),
                              "reason": reason,"dynamic_obstacle":moving,
                              "origin": tuple(state["origin"]),
                              "angles": tuple(state["angles"])})
        present = {i["model"] for i in instances}
        for model in range(1, len(models)):
            if model not in present:
                instances.append({"model": model, "enabled": False,
                                  "reason": "not_present_in_runtime_snapshot"})
        return instances
    known_solid = {"func_static", "func_wall", "func_explosive", "func_leaky",
                   "func_rotating", "func_bobbing", "func_pendulum",
                   "func_door", "func_door_rotating", "func_button",
                   "func_plat", "func_train", "func_secret", "script_mover",
                   "func_constructible"}
    for model in range(1, len(models)):
        found = [(i, e) for i, e in enumerate(entities) if e.get("model") == f"*{model}"]
        if not found:
            instances.append({"model": model, "enabled": False,
                              "reason": "no_bsp_entity_for_model"})
        for number, entity in found:
            classname = entity.get("classname", "")
            spawnflags = int(entity.get("spawnflags", "0"))
            origin = point_text(entity.get("origin", "0 0 0"))
            angles = point_text(entity.get("angles", "0 " + entity.get("angle", "0") + " 0"))
            if origin is None or angles is None:
                raise ValueError(f"entity {number}: invalid model transform")
            enabled = classname in known_solid
            reason = "provisional_bsp_spawn_pose"
            if classname.startswith("trigger_"):
                enabled, reason = False, "trigger_not_solid_collision"
            elif not enabled:
                reason = "unknown_entity_collision_policy_requires_runtime_snapshot"
            elif ((classname in {"func_static", "func_explosive"} and spawnflags & 1)
                  or (classname == "func_rotating" and spawnflags & 2)):
                enabled, reason = False, "spawnflag_initially_unlinked"
            elif classname == "func_constructible":
                enabled, reason = False, "construction_state_requires_runtime_snapshot"
            moving=classname in {"func_rotating","func_bobbing","func_pendulum","func_train","func_plat",
                                "func_door","func_door_rotating","func_secret","script_mover"}
            if enabled and moving:
                enabled=False;reason="dynamic_obstacle_requires_current_native_gate"
            instances.append({"model": model, "entity": number, "classname": classname,
                              "origin": origin, "angles": angles, "enabled": enabled,
                              "spawnflags": spawnflags, "reason": reason,
                              "state_source": "bsp_spawn_policy",
                              "dynamic": classname not in {"func_static", "func_wall"},"dynamic_obstacle":moving})
    return instances


def vertical_raster_domain(models, instances, spawns):
    """Bound navigation work, without changing any collision triangle.

    Some q3map-generated model brushes extend thin vertical solids to 65536.
    The visible model bounds and actual spawn positions delimit this offline
    working volume; the original infinite/extended solids stay in the OBJ.
    128 units exceeds the largest supported TCE hull (87) plus its step (18).
    The rasterizer must intersect this interval, never add horizontal caps.
    """
    basis = []
    for instance in instances:
        if not instance["enabled"]:
            continue
        model_number = instance["model"]
        model = models[model_number]
        transform, _ = entity_transform(instance["origin"], instance["angles"])
        corners = [transform((model[x], model[y], model[z]))
                   for x in (0, 3) for y in (1, 4) for z in (2, 5)]
        heights = [point[2] for point in corners]
        if not all(math.isfinite(value) for value in heights):
            raise ValueError("non-finite model navigation domain")
        basis.append({"kind": "world_model" if model_number == 0 else "active_inline_model",
                      "model": model_number, "minimum": min(heights), "maximum": max(heights)})
    for spawn in spawns:
        origin = spawn.get("origin_bsp")
        if origin is not None:
            if len(origin) != 3 or not all(math.isfinite(value) for value in origin):
                raise ValueError("invalid spawn navigation domain")
            basis.append({"kind": "spawn", "entity": spawn["entity"],
                          "minimum": origin[2], "maximum": origin[2]})
    if not basis or not any(item["kind"] == "world_model" for item in basis):
        raise ValueError("navigation raster domain requires the world model")
    padding = 128.0
    minimum = min(item["minimum"] for item in basis) - padding
    maximum = max(item["maximum"] for item in basis) + padding
    if not math.isfinite(minimum) or not math.isfinite(maximum) or minimum >= maximum:
        raise ValueError("invalid navigation raster height domain")
    return {"coordinate_system": "BSP_Z_equals_OBJ_Y", "minimum": minimum,
            "maximum": maximum, "padding": padding,
            "rule": "playable_model_and_spawn_bounds_plus_hull_step_padding",
            "basis": basis, "collision_geometry_modified": False,
            "add_cut_caps": False,
            "limitation": "Navigation outside this explicit working volume is not generated; native validation remains mandatory."}


def prepare_geometry(data, flags, runtime=None, include_vertical_raster_domain=False):
    lumps = read_lumps(data)
    shaders = unpack_lump(data, lumps, 1, "<64sII")
    shaders = [{"name": name.split(b"\0", 1)[0].decode("latin-1"),
                "surface_flags": surface, "content_flags": content}
               for name, surface, content in shaders]
    planes = []
    for x, y, z, distance in unpack_lump(data, lumps, 2, "<4f"):
        if not all(map(math.isfinite, (x, y, z, distance))):
            raise ValueError("non-finite BSP plane")
        length = math.sqrt(x * x + y * y + z * z)
        if length < 1e-8:
            raise ValueError("zero BSP plane")
        planes.append(((x / length, y / length, z / length), distance / length))
    models = unpack_lump(data, lumps, 7, "<6f4i")
    brushes = unpack_lump(data, lumps, 8, "<3i")
    sides = unpack_lump(data, lumps, 9, "<2i")
    surfaces = unpack_lump(data, lumps, 13, "<12i12f2i")
    draw_vertices = unpack_lump(data, lumps, 10, "<10f4B")
    vertex_count = len(draw_vertices)
    index_count = lumps[11]["length"] // 4
    if not models:
        raise ValueError("BSP has no world model")
    for number, model in enumerate(models):
        if not all(map(math.isfinite, model[:6])):
            raise ValueError(f"model {number}: non-finite bounds")
        if any(model[i] > model[i + 3] for i in range(3)):
            raise ValueError(f"model {number}: inverted bounds")
        valid_range(model[6], model[7], len(surfaces), f"model {number} surfaces")
        valid_range(model[8], model[9], len(brushes), f"model {number} brushes")
    for number, (first_side, num_sides, shader) in enumerate(brushes):
        valid_range(first_side, num_sides, len(sides), f"brush {number} sides")
        valid_range(shader, 1, len(shaders), f"brush {number} shader")
    for number, (plane, shader) in enumerate(sides):
        valid_range(plane, 1, len(planes), f"side {number} plane")
        valid_range(shader, 1, len(shaders), f"side {number} shader")
    for number, surface in enumerate(surfaces):
        valid_range(surface[0], 1, len(shaders), f"surface {number} shader")
        valid_range(surface[3], surface[4], vertex_count, f"surface {number} vertices")
        valid_range(surface[5], surface[6], index_count, f"surface {number} indexes")
        if surface[2] == 2:
            width, height = surface[-2:]
            if (width < 3 or height < 3 or width % 2 != 1 or height % 2 != 1
                    or width * height > surface[4] or width * height > 1024):
                raise ValueError(f"surface {number}: invalid patch dimensions")
    entity_lump = lumps[0]
    entities = parse_entities(data[entity_lump["offset"]:
                                  entity_lump["offset"] + entity_lump["length"]])
    mask = flags["CONTENTS_SOLID"] | flags["CONTENTS_PLAYERCLIP"]
    world = models[0]
    extent = max(4096.0, max(abs(v) for v in world[:6]) * 4.0 + 4096.0,
                 max((abs(p[1]) for p in planes), default=0) * 4.0 + 4096.0)
    vertices, triangles, vertex_map = [], [], {}
    skipped = Counter()
    failures, ladders, sky_faces = [], [], []
    patch_records, brush_fallbacks = [], []
    counts = Counter()
    instances = [{"model": 0, "enabled": True, "origin": (0, 0, 0),
                  "angles": (0, 0, 0), "reason": "world_collision"}]
    inline_models = inline_instances(models, entities, mask, runtime)
    instances.extend(inline_models)

    def emit_polygon(polygon, group, surface_flags):
        indices = []
        for point in polygon:
            vertex = tuple(round(value, 6) for value in y_up(point))
            if vertex not in vertex_map:
                vertex_map[vertex] = len(vertices) + 1
                vertices.append(vertex)
            indices.append(vertex_map[vertex])
        for i in range(1, len(indices) - 1):
            a, b, c = indices[0], indices[i], indices[i + 1]
            normal = cross(subtract(vertices[b - 1], vertices[a - 1]),
                           subtract(vertices[c - 1], vertices[a - 1]))
            if dot(normal, normal) > 1e-12:
                triangles.append((a, b, c, group, surface_flags))

    for instance in instances:
        if not instance["enabled"]:
            skipped[instance["reason"]] += 1
            continue
        model_number = instance["model"]
        model = models[model_number]
        transform, rotate = entity_transform(instance["origin"], instance["angles"])
        start_triangles = len(triangles)
        prefix = "world" if model_number == 0 else "inline"
        for number in range(model[8], model[8] + model[9]):
            first_side, num_sides, shader_index = brushes[number]
            shader = shaders[shader_index]
            if not shader["content_flags"] & mask:
                skipped[prefix + "_brush_without_solid_or_playerclip"] += 1
                continue
            brush_sides = sides[first_side:first_side + num_sides]
            brush_planes = [planes[side[0]] for side in brush_sides]
            try:
                polygons = brush_faces(brush_planes, extent)
            except ValueError as error:
                # Malformed/degenerated compiler brushes can have mutually
                # contradictory bevels. Do not treat these as empty space.
                # The first six native brush sides give conservative bounds;
                # record that they are an overestimate, not native parity.
                fallback = False
                padded_axes = []
                if len(brush_planes) >= 6:
                    axial = list(brush_planes[:6])
                    expected = ((-1,0,0),(1,0,0),(0,-1,0),(0,1,0),(0,0,-1),(0,0,1))
                    if all(math.dist(n, e) < 1e-6 for (n, d), e in zip(axial, expected)):
                        for axis in range(3):
                            low, high = -axial[axis*2][1], axial[axis*2+1][1]
                            if 0 <= high - low < 0.125:
                                center = (low + high) * 0.5
                                axial[axis*2] = (expected[axis*2], -(center-0.0625))
                                axial[axis*2+1] = (expected[axis*2+1], center+0.0625)
                                padded_axes.append(axis)
                        try:
                            polygons = brush_faces(axial, extent)
                            fallback = True
                        except ValueError:
                            pass
                record = {"model": model_number, "brush": number, "reason": str(error),
                          "shader": shader["name"], "content_flags": shader["content_flags"],
                          "planes_bsp": [{"normal": n, "distance": d} for n, d in brush_planes]}
                if not fallback:
                    failures.append(record)
                    continue
                record["replacement"] = "conservative_first_six_axial_bounds"
                record["min_thickness_0_125_axes"] = padded_axes
                brush_fallbacks.append(record)
            counts["exported_" + prefix + "_brushes"] += 1
            for local_side, polygon in polygons:
                side_shader = shaders[brush_sides[local_side][1]]
                world_polygon = [transform(point) for point in polygon]
                emit_polygon(world_polygon, f"{number}_model_{model_number}",
                             side_shader["surface_flags"])
                metadata = {"model": model_number, "brush": number,
                            "side": first_side + local_side,
                            "shader": side_shader["name"],
                            "normal_bsp": list(rotate(brush_planes[local_side][0])),
                            "bounds_bsp": bounds(world_polygon),
                            "polygon_y_up": [list(y_up(point)) for point in world_polygon]}
                if side_shader["surface_flags"] & flags["SURF_LADDER"]:
                    ladders.append(metadata)
                if side_shader["surface_flags"] & flags["SURF_SKY"]:
                    sky_faces.append({"model": model_number, "brush": number,
                                      "side": first_side + local_side})
        for number in range(model[6], model[6] + model[7]):
            surface = surfaces[number]
            shader = shaders[surface[0]]
            if surface[2] != 2 or not shader["content_flags"] & mask:
                continue
            width, height = surface[-2:]
            points = [vertex[:3] for vertex in
                      draw_vertices[surface[3]:surface[3] + width * height]]
            record = {"model": model_number, "surface": number, "shader": shader["name"],
                      "content_flags": shader["content_flags"],
                      "surface_flags": shader["surface_flags"],
                      "control_width": width, "control_height": height}
            try:
                patch_triangles, grid_size = collision_patch_triangles(width, height, points)
                for triangle in patch_triangles:
                    emit_polygon([transform(p) for p in triangle],
                                 f"patch_{number}_model_{model_number}", shader["surface_flags"])
                record.update({"exported": True, "collision_grid": grid_size,
                               "triangles": len(patch_triangles)})
                counts["exported_" + prefix + "_patches"] += 1
            except ValueError as error:
                record.update({"exported": False, "reason": str(error)})
            patch_records.append(record)
        instance["triangles"] = len(triangles) - start_triangles
        if model_number:
            counts["exported_inline_models"] += 1
    spawns, special_entities, objective_volumes = [], [], []
    objective_targets={}
    for candidate in entities:
        if candidate.get("targetname"):
            objective_targets.setdefault(candidate["targetname"],[]).append(candidate)
    for number, entity in enumerate(entities):
        classname = entity.get("classname", "")
        is_spawn = (classname.startswith("info_player_")
                    or classname in {"team_CTF_redspawn", "team_CTF_bluespawn",
                                     "team_CTF_redplayer", "team_CTF_blueplayer"})
        origin = point_text(entity.get("origin", ""))
        if is_spawn:
            spawns.append({"entity": number, "classname": classname,
                           "origin_bsp": origin,
                           "origin_y_up": y_up(origin) if origin else None,
                           "spawnflags": entity.get("spawnflags", "0")})
        if (classname.startswith(("trigger_", "func_"))
                or any(word in classname.lower() for word in ("objective", "ladder", "teleport"))):
            special_entities.append({"entity": number, "properties": entity})
        if classname == "trigger_objective_info" and re.fullmatch(r"\*[0-9]+", entity.get("model", "")):
            model_number=int(entity["model"][1:])
            if not 0<model_number<len(models):
                raise ValueError("objective trigger references an invalid BSP model")
            model=models[model_number]
            transform,_=entity_transform(origin or (0,0,0),point_text(entity.get("angles","")) or (0,0,0))
            corners=[transform((model[x],model[1+y],model[2+z])) for x in (0,3) for y in (0,3) for z in (0,3)]
            limits=bounds(corners)
            candidates=objective_targets.get(entity.get("target"),[])
            target=candidates[0] if len(candidates)==1 else None
            target_limits=None
            if target and re.fullmatch(r"\*[0-9]+",target.get("model","")):
                target_number=int(target["model"][1:])
                if 0<target_number<len(models):
                    target_model=models[target_number]
                    target_origin=point_text(target.get("origin","")) or (0,0,0)
                    target_angles=point_text(target.get("angles","")) or (0,float(target.get("angle","0")),0)
                    target_transform,_=entity_transform(target_origin,target_angles)
                    target_limits=bounds([target_transform((target_model[x],target_model[1+y],target_model[2+z]))
                                          for x in (0,3) for y in (0,3) for z in (0,3)])
            objective_volumes.append(dict(entity=number,bsp_entity=number,model=model_number,classname=classname,
                                          mins=limits["min"],maxs=limits["max"],properties=dict(entity),
                                          contact_top=model[5]+(origin or (0,0,0))[2],
                                          target_class=target.get("classname") if target else None,
                                          target_bounds=[target_limits["min"],target_limits["max"]] if target_limits else None,
                                          state_source="bsp_all_gametypes_objective_metadata",
                                          native_validation_required=True))
    manifest = {
        "format": "tce2_bsp_collision_geometry_v1",
        "phase": "geometry_candidate_not_validated",
        "created_utc": datetime.now(timezone.utc).isoformat(),
        "geometry_complete": False,
        "coordinate_system": {"bsp": "Z-up, game units", "obj": "Y-up, game units",
                              "transform": ["x", "z", "-y"],
                              "winding": "outward, rotation preserves handedness"},
        "obj_materials": "usemtl surfaceflags_<decimal> encodes per-side BSP surface flags",
        "collision_filter": {"world_model_only": False, "contents_mask": mask,
                             "invisible_playerclip_included": True},
        "bsp_lumps": lumps,
        "counts": {"all_brushes": len(brushes), "world_brushes": world[9],
                   **dict(counts),
                   "vertices": len(vertices), "triangles": len(triangles),
                   "entities": len(entities), "spawns": len(spawns),
                   "ladder_faces": len(ladders), "sky_faces": len(sky_faces),
                   "unsupported_world_patches": sum(not p["exported"] and not p["model"] for p in patch_records),
                   "excluded_inline_models": sum(not i["enabled"] for i in inline_models),
                   "conservative_brush_fallbacks": len(brush_fallbacks),
                   "failed_collision_brushes": len(failures)},
        "bounds_bsp": {"min": list(world[:3]), "max": list(world[3:6])},
        "bounds_obj_y_up": bounds(vertices), "spawns": spawns,
        "ladders": ladders, "sky_faces": sky_faces,
        "special_entities": special_entities,
        "objective_volumes": objective_volumes,
        "skipped": dict(skipped), "failed_brushes": failures,
        "collision_patches": patch_records,
        "unsupported_collision_patches": [p for p in patch_records if not p["exported"]],
        "inline_models": inline_models,
        "excluded_inline_models": [i for i in inline_models if not i["enabled"]],
        "dynamic_obstacles": [i for i in inline_models if i.get("dynamic_obstacle")],
        "conservative_brush_fallbacks": brush_fallbacks,
        "entity_state_source": "runtime_snapshot" if runtime is not None else "provisional_bsp_spawn_policy",
        "runtime_time": runtime.get("time") if runtime is not None else None,
        "patch_method": {"subdivide_distance": 16.0, "duplicate_point_epsilon": 0.1,
                         "method": "CM adaptive quadratic grid, transposed facet surface winding",
                         "native_facet_border_bevels": False},
        "limitations": [
            "Collision brush boundary soup only; no CSG union of overlapping brushes.",
            "Interior/sky/clip brush faces are present and are not certified playable floors.",
            "Patch grids use CM subdivision; native facet border/bevel planes and float rounding require native trace validation.",
            "Malformed brushes use conservative axial bounds when possible and are explicitly reported.",
            "BSP-only inline spawn states are provisional; --entities uses linked/contents/origin/angles from a current native snapshot.",
            "A runtime snapshot describes one instant; moving, destructible and scripted collision can change later.",
            "Moving/door models are dynamic obstacles, not baked Recast support; every transition still needs current native hull collision.",
            "Spawn regions, player clearance, steps, crouching, ladders and jumps need native validation.",
            "No navmesh, waypoint file, game installation or gameplay certification is produced.",
        ],
    }
    if include_vertical_raster_domain:
        manifest["vertical_raster_domain"] = vertical_raster_domain(models, instances, spawns)
    return vertices, triangles, manifest


def write_outputs(directory, map_name, vertices, triangles, manifest):
    directory = directory.resolve()
    if any(part.lower() in {"tcetest", "pak", "cqb", "cqbtest", "etlegacy"}
           for part in directory.parts):
        raise ValueError("refusing output into reference assets/project directory")
    directory.mkdir(parents=True, exist_ok=True)
    obj = directory / f"{map_name}.collision.obj"
    report = directory / f"{map_name}.geometry.json"
    temporary = obj.with_suffix(".obj.tmp")
    with temporary.open("w", encoding="ascii", newline="\n") as output:
        output.write("# ET BSP47 static collision candidate; see adjacent geometry.json\n")
        output.write("# Y-up transform: (x, y, z) BSP -> (x, z, -y) OBJ\n")
        for point in vertices:
            output.write("v " + " ".join(f"{value:.6f}" for value in point) + "\n")
        current_brush, current_flags = None, None
        for a, b, c, brush, flags in triangles:
            if brush != current_brush:
                output.write(f"g brush_{brush}\n")
                current_brush = brush
            if flags != current_flags:
                output.write(f"usemtl surfaceflags_{flags}\n")
                current_flags = flags
            output.write(f"f {a} {b} {c}\n")
    temporary.replace(obj)
    manifest["output"] = {"obj": str(obj), "obj_sha256": sha256_file(obj),
                          "manifest": str(report)}
    temporary = report.with_suffix(".json.tmp")
    temporary.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    temporary.replace(report)
    return obj, report


def self_test():
    cube = [((1, 0, 0), 1), ((-1, 0, 0), 1), ((0, 1, 0), 1),
            ((0, -1, 0), 1), ((0, 0, 1), 1), ((0, 0, -1), 1)]
    faces = brush_faces(cube, 4096)
    assert len(faces) == 6 and all(len(polygon) == 4 for _, polygon in faces)
    for index, polygon in faces:
        normal = cross(subtract(polygon[1], polygon[0]),
                       subtract(polygon[2], polygon[0]))
        assert dot(normal, cube[index][0]) > 0
        transformed = [y_up(point) for point in polygon]
        assert dot(cross(subtract(transformed[1], transformed[0]),
                         subtract(transformed[2], transformed[0])),
                   y_up(cube[index][0])) > 0
        assert all(dot(n, point) <= d + EPSILON for point in polygon for n, d in cube)
    diagonal = normalized((1, 1, 0))
    clipped = brush_faces(cube + [(diagonal, 0.5)], 4096)
    assert len(clipped) == 7
    assert all(dot(diagonal, point) <= 0.5 + EPSILON
               for _, polygon in clipped for point in polygon)
    assert len(brush_faces(cube + [cube[0]], 4096)) == 6
    try:
        brush_faces(cube[:4], 4096)
    except ValueError:
        pass
    else:
        raise AssertionError("unbounded brush was accepted")
    header = bytearray(b"IBSP" + struct.pack("<i", 47) + b"\0" * (17 * 8))
    assert len(read_lumps(header)) == 17
    invalid = bytearray(header)
    struct.pack_into("<ii", invalid, 8, HEADER_SIZE, 999)
    try:
        read_lumps(invalid)
    except ValueError:
        pass
    else:
        raise AssertionError("out-of-bounds lump was accepted")
    invalid = bytearray(header + b" " * 100)
    struct.pack_into("<ii", invalid, 8, HEADER_SIZE, 72)
    struct.pack_into("<ii", invalid, 16, HEADER_SIZE + 4, 72)
    try:
        read_lumps(invalid)
    except ValueError:
        pass
    else:
        raise AssertionError("overlapping lumps were accepted")
    assert parse_entities(b'// note\n{ "classname" "worldspawn" }\0') == [
        {"classname": "worldspawn"}]
    transform, rotate = entity_transform((100, 200, 300), (0, 90, 0))
    assert math.dist(transform((10, 0, 0)), (100, 210, 300)) < 1e-9
    assert math.dist(transform((0, 10, 0)), (90, 200, 300)) < 1e-9
    # Verify all Euler angles using the right-handed orthonormal basis.
    transform, rotate = entity_transform((0, 0, 0), (23, 47, -19))
    basis = [rotate(p) for p in ((1,0,0), (0,1,0), (0,0,1))]
    assert abs(dot(cross(basis[0], basis[1]), basis[2]) - 1) < 1e-9
    assert all(abs(dot(basis[i], basis[j])) < 1e-9
               for i in range(3) for j in range(i+1, 3))
    planar = [(x * 64., y * 64., 0.) for y in range(3) for x in range(3)]
    patch_tris, grid = collision_patch_triangles(3, 3, planar)
    assert grid == (2, 2) and len(patch_tris) == 2
    assert all(cross(subtract(b, a), subtract(c, a))[2] > 0
               for a, b, c in patch_tris)
    curved = [(x * 64., y * 64., 128. if x == 1 else 0.)
              for y in range(3) for x in range(3)]
    patch_tris, grid = collision_patch_triangles(3, 3, curved)
    assert grid == (2, 5) and len(patch_tris) == 8
    assert max(p[2] for tri in patch_tris for p in tri) == 64.
    assert all(cross(subtract(b, a), subtract(c, a))[2] > 0
               for a, b, c in patch_tris)
    patch_tris, grid = collision_patch_triangles(3, 3, [(0.,0.,0.)] * 9)
    assert not patch_tris
    states = inline_instances([None] * 4,
        [{"model": "*1", "classname": "trigger_multiple"},
         {"model": "*2", "classname": "func_static", "spawnflags": "1"},
         {"model": "*3", "classname": "func_static", "origin": "1 2 3"}], 65537)
    assert [s["enabled"] for s in states] == [False, False, True]
    states = inline_instances([None] * 4, [], 65537, {"models": [
        {"entity": 1, "modelindex": 1, "linked": True, "contents": 1073741824,
         "origin": [0,0,0], "angles": [0,0,0]},
        {"entity": 2, "modelindex": 2, "linked": False, "contents": 1,
         "origin": [0,0,0], "angles": [0,0,0]},
        {"entity": 3, "modelindex": 3, "linked": True, "contents": 1,
         "origin": [1,2,3], "angles": [0,90,0]}]})
    assert [s["enabled"] for s in states] == [False, False, True]
    return {"self_test": "passed", "cases": ["cube winding", "Y-up winding",
            "convex clipping", "duplicate planes", "unbounded brush rejection",
            "header parsing", "invalid lump bounds", "overlapping lump rejection",
            "entity parsing", "inline translation and yaw", "Euler handedness",
            "planar and curved CM patch grids", "patch surface winding",
            "degenerate patch removal", "BSP trigger and invisible exclusion",
            "runtime contents and linked-state exclusion"]}


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("input", type=Path, nargs="?")
    parser.add_argument("--map", dest="map_name")
    parser.add_argument("--out-dir", type=Path)
    parser.add_argument("--entities", type=Path,
                        help="native tce_navgen_entities JSON for authoritative inline model state")
    parser.add_argument("--vertical-raster-domain", action="store_true",
                        help="describe an explicit BSP model/spawn height work volume; preserve the full collision OBJ")
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()
    try:
        if args.self_test:
            print(json.dumps(self_test()))
            if args.input is None:
                return 0
        if args.input is None or args.out_dir is None:
            parser.error("input and --out-dir are required unless only --self-test is used")
        data, map_name, source = read_input(args.input, args.map_name)
        flags, flag_source = source_flags()
        runtime = None
        if args.entities:
            runtime = json.loads(args.entities.read_text(encoding="utf-8"))
            if runtime.get("map") != map_name or runtime.get("schema") != 1:
                raise ValueError("runtime entity snapshot map/schema mismatch")
        vertices, triangles, manifest = prepare_geometry(
            data, flags, runtime, include_vertical_raster_domain=args.vertical_raster_domain)
        if args.entities:
            manifest["runtime_snapshot"] = {"path": str(args.entities.resolve()),
                                            "sha256": sha256_file(args.entities)}
        manifest.update({"map": map_name, "input": source, "flag_source": flag_source})
        obj, report = write_outputs(args.out_dir, map_name, vertices, triangles, manifest)
        print(json.dumps({"phase": manifest["phase"], "geometry_complete": False,
                          "counts": manifest["counts"], "obj": str(obj),
                          "manifest": str(report)}, indent=2))
        return 0
    except (OSError, ValueError, zipfile.BadZipFile) as error:
        print(f"BSP geometry export failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
