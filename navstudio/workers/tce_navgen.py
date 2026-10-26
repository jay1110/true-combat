"""Generate Omni-bot navigation from the loaded TCE2 map, without WPS files.

A dedicated test server loads the BSP and performs collision checks. The tool
checks the server's map, requests generation and audits the binary/report.
Existing navigation is never replaced by generate. Installation keeps a backup.
Graph connectivity is not proof of complete playable-space coverage, mission AI,
or successful ladder/jump traversal. All remaining limits are reported.

Example (server already has obj_overkill loaded; set TCE_RCON_PASSWORD):
  python navstudio/workers/tce_navgen.py generate build/navgen-overkill/tce2 --map obj_overkill --port 27986
  python navstudio/workers/tce_navgen.py inspect build/navgen-overkill/tce2/navgen/obj_overkill_auto.json

Exit codes: 0 graph checks pass, 2 incomplete connectivity/coverage search,
1 input/server/output failure. Optional --wps imports legacy seeds explicitly;
by default no WPS is read or needed. mod_directory must be the locally accessible
fs_homepath/tce2 of the selected test server. Never run on a busy public server.
"""

import argparse
from collections import Counter
from datetime import datetime, timezone
import hashlib
import heapq
import json
import math
import os
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from layout import REPO_ROOT, install_import_paths
install_import_paths()
import re
import shutil
import socket
import struct
import sys
import time


MAP_NAME = re.compile(r"[A-Za-z0-9_-]{1,64}\Z")
HEADER = struct.Struct("<BI32s232s6f")
_last_rcon_send = 0.0


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def valid_map(value):
    if not MAP_NAME.fullmatch(value):
        raise ValueError("map name must contain only letters, digits, '_' or '-'")
    return value


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".tmp")
    temporary.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
    temporary.replace(path)


def writable_destination(path):
    # These folders are reference inputs, even when accidentally selected as a
    # homepath or navigation destination. Resolve junctions/symlinks first.
    resolved = path.resolve()
    if any(part.lower() in {"tcetest", "pak", "cqb", "cqbtest", "etlegacy"}
           for part in resolved.parts):
        raise ValueError("refusing to write into a reference project/assets folder")
    return resolved


def wps_info(path):
    data = path.read_bytes()
    if len(data) < 17 or data[:11] != b"truecombat\0":
        raise ValueError("not a TC WPS file")
    version, count = struct.unpack_from("<fh", data, 11)
    if version != 2 or not 1 <= count <= 1024:
        raise ValueError("unsupported WPS version or node count")
    offset, active, links = 17, set(), []
    for _ in range(count):
        number, x, y, z, flags, _e1, _e2, _e3, _packed, degree = struct.unpack_from(
            "<hfffhhhhih", data, offset)
        offset += 28
        if not 0 <= degree <= 12 or not all(map(math.isfinite, (x, y, z))):
            raise ValueError("invalid WPS position/degree")
        outgoing = [struct.unpack_from("<hH", data, offset + 4 * i)
                    for i in range(degree)]
        offset += degree * 4
        if flags != -1 and number != -1:
            if number in active:
                raise ValueError("duplicate active WPS node ID")
            active.add(number)
            links.extend((number, target, flag) for target, flag in outgoing)
    if offset != len(data):
        raise ValueError("unexpected WPS trailing bytes")
    return {"nodes": count, "active_nodes": len(active), "directed_links": len(links),
            "invalid_targets": sum(target not in active for _, target, _ in links),
            "blocked_links": sum(bool(flag & 16) for _, _, flag in links),
            "sha256": hashlib.sha256(data).hexdigest()}


def prepare(source, mod_directory):
    info = wps_info(source)
    name = valid_map(source.stem)
    mod_directory = writable_destination(mod_directory)
    target = mod_directory / "botroutes" / (name + ".wps")
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.exists() and digest(target) != info["sha256"]:
        raise FileExistsError("staged WPS differs; choose a fresh test homepath: " + str(target))
    if not target.exists():
        shutil.copy2(source, target)
    info.update(source=str(source.resolve()), staged=str(target), map=name,
                staged_at=datetime.now(timezone.utc).isoformat())
    write_json(mod_directory / "navgen" / (name + "_source.json"), info)
    return info


class Reader:
    def __init__(self, data):
        self.data, self.offset = data, 0

    def take(self, count):
        if count < 0 or self.offset + count > len(self.data):
            raise ValueError("truncated Omni-bot v9 waypoint file")
        result = self.data[self.offset:self.offset + count]
        self.offset += count
        return result

    def unpack(self, fmt):
        return struct.unpack(fmt, self.take(struct.calcsize(fmt)))

    def packed(self):
        result = 0
        for shift in range(0, 70, 7):
            byte = self.take(1)[0]
            result |= (byte & 127) << shift
            if byte & 128:
                return result
        raise ValueError("invalid packed integer")

    def string(self):
        return self.take(self.packed()).decode("utf-8", errors="strict")


def read_way(path):
    reader = Reader(path.read_bytes())
    version, count, _author, _comment, *bounds = reader.unpack(HEADER.format)
    if version != 9 or not 1 <= count <= 65536 or not all(map(math.isfinite, bounds)):
        raise ValueError("invalid Omni-bot v9 header")
    nodes, edges, previous_uid, seen_uids = [], set(), 0, set()
    for index in range(count):
        mask = reader.take(1)[0]
        if mask & 128:
            raise ValueError("unknown waypoint record mask")
        uid = reader.packed() if mask & 32 else previous_uid + 1
        if not 1<=uid<=0xffffffff or uid in seen_uids:
            raise ValueError("invalid/duplicate Omni-bot waypoint UID")
        seen_uids.add(uid)
        previous_uid = uid
        position = reader.unpack("<3f")
        if not all(map(math.isfinite, position)):
            raise ValueError("nonfinite waypoint coordinate")
        facing = reader.unpack("<3f") if mask & 64 else (0.0, 0.0, 0.0)
        if not all(map(math.isfinite, facing)):
            raise ValueError("nonfinite waypoint facing")
        flags = reader.packed() if mask & 1 else 0
        radius = (reader.take(1)[0] if mask & 8 else
                  reader.unpack("<f")[0] if mask & 16 else 35)
        if not math.isfinite(radius) or radius < 0:
            raise ValueError("invalid waypoint radius")
        name = reader.string() if mask & 2 else ""
        properties = {}
        if mask & 4:
            property_count = reader.packed()
            if property_count > 1024:
                raise ValueError("too many waypoint properties")
            for _ in range(property_count):
                key, value = reader.string(), reader.string()
                if key in properties:raise ValueError("duplicate Omni-bot waypoint property")
                properties[key] = value
        degree = reader.packed()
        if degree > count:
            raise ValueError("invalid waypoint degree")
        for _ in range(degree):
            value = reader.packed()
            delta = ~(value >> 1) if value & 1 else value >> 1
            target = index + delta
            if not 0 <= target < count or target == index:
                raise ValueError("invalid/self waypoint link")
            if (index, target) in edges:
                raise ValueError("duplicate waypoint link")
            edges.add((index, target))
        nodes.append(dict(index=index, uid=uid, position=position, flags=flags,
                          radius=radius, name=name, facing=facing, properties=properties))
    if reader.offset != len(reader.data):
        raise ValueError("unexpected waypoint trailing bytes")
    return nodes, edges


def components(count, edges):
    """Iterative Kosaraju: directed strongly connected components, no recursion."""
    forward, reverse = [[] for _ in range(count)], [[] for _ in range(count)]
    for a, b in edges:
        forward[a].append(b)
        reverse[b].append(a)
    seen, order = set(), []
    for start in range(count):
        if start in seen:
            continue
        seen.add(start)
        stack = [(start, iter(forward[start]))]
        while stack:
            node, children = stack[-1]
            child = next(children, None)
            if child is None:
                order.append(node)
                stack.pop()
            elif child not in seen:
                seen.add(child)
                stack.append((child, iter(forward[child])))
    seen, groups = set(), []
    for start in reversed(order):
        if start in seen:
            continue
        group, stack = [], [start]
        seen.add(start)
        while stack:
            node = stack.pop()
            group.append(node)
            for child in reverse[node]:
                if child not in seen:
                    seen.add(child)
                    stack.append(child)
        groups.append(group)
    return sorted(groups, key=lambda group: (-len(group), min(group)))


def intentional_ladder_overlaps(report, by_id, binary_nodes, binary_edges):
    """Authorize only declared overlaps of a complete, exported ladder recipe.

    CLIMB alone is not an exemption. The inside target deliberately sits in
    the ladder plane and the exit target aliases its floor anchor. Validate
    those roles, their geometry and the actual directed WAY edges together.
    This structural audit does not claim live traversal or collision parity.
    """
    allowed, verified, claimed_guides = {}, [], set()
    release_indices, alignment_indices = set(), set()
    all_nodes = {node["id"]: node for node in report["nodes"]}
    if len(all_nodes) != len(report["nodes"]):
        raise ValueError("duplicate report node IDs")
    edge_reasons = {(edge["from"], edge["to"]): edge.get("reason")
                    for edge in report["edges"] if edge.get("accepted")}
    for route_index, route in enumerate(report.get("traversal_routes", [])):
        if route.get("kind") != "ladder" or route.get("exported") is not True:
            continue

        def require(condition, message):
            if not condition:
                raise ValueError(f"ladder route {route_index}: {message}")

        guides = route.get("guides")
        require(isinstance(guides, list) and len(guides) == 4, "four guidance IDs required")
        low, high = route.get("low"), route.get("high")
        ids = [low, high, *guides]
        require(all(type(value) is int for value in ids) and len(set(ids)) == 6,
                "distinct integer anchor/guidance IDs required")
        require(all(value in all_nodes for value in ids), "unknown anchor/guidance ID")
        require(type(route.get("reverse")) is bool, "explicit direction required")
        require(route.get("inside_guide") == guides[1], "inside guide must be second guidance point")
        method, release = route.get("exit_method"), route.get("ascent_release", -1)
        alignment = route.get("ascent_alignment", -1)
        require(method in ("supported", "ballistic") and type(release) is int and
                type(alignment) is int and alignment >= -1, "explicit exit method/guidance IDs required")
        if method == "ballistic":
            require(release >= 0 and release not in ids and release in all_nodes,
                    "ballistic exit requires a distinct ascent-release guide; regenerate older recipes")
        else:
            require(release == -1, "supported exit must not declare a ballistic release guide")
        if alignment >= 0:
            require(method == "ballistic" and alignment not in ids + [release] and alignment in all_nodes,
                    "alignment requires a distinct ballistic guidance point")
        physics = report.get("traversal_physics", {})
        physics_model = physics.get("model")
        forward_model = physics_model == "tce_ladder_forward_or_strafe_then_air_heavy_run_v3"
        exit_control = route.get("exit_control")
        forward_exit = method == "ballistic" and exit_control == "forward_climb_until_contact_loss"
        if forward_model:
            require(physics.get("ladder_forward_vertical_scale") == .9,
                    "forward-climb scale differs from native movement recipe")
            require(exit_control in (("forward_climb_until_contact_loss", "tangent_strafe_until_contact_loss")
                                     if method == "ballistic" else ("supported_walk",)),
                    "current ladder recipe requires its explicit exit control")
        else:
            # V2 did not always emit exit_control. It never proved forward
            # climb or capsule-supported continuation, so cannot claim either.
            require(exit_control in (None, "tangent_strafe_until_contact_loss" if method == "ballistic" else "supported_walk"),
                    "legacy ladder recipe cannot claim a new exit control")
        if method == "ballistic":
            require(physics_model in ("tce_ladder_strafe_then_air_heavy_run_v2",
                                      "tce_ladder_forward_or_strafe_then_air_heavy_run_v3") and
                    isinstance(physics.get("step_seconds"), (int, float)) and
                    abs(physics["step_seconds"]-.008) <= .000001,
                    "ballistic exit requires current native ladder-strafe/air proof metadata")
            require(physics.get("ladder_strafe_friction") == 14 and
                    physics.get("ladder_strafe_acceleration") == 10 and
                    physics.get("ladder_strafe_speed_scale") == .5,
                    "ladder-contact constants differ from native movement recipe")
            for field in ("gravity", "ladder_exit_upward_velocity"):
                require(isinstance(physics.get(field), (int, float)) and math.isfinite(physics[field]),
                        "finite native traversal physics required")
            require(100 <= physics["gravity"] <= 2000 and physics["ladder_exit_upward_velocity"] == 115,
                    "native traversal physics outside validated recipe")
            for field in ("exit_velocity_z", "exit_speed_xy", "exit_duration_seconds", "exit_initial_speed_xy", "exit_initial_wishspeed", "ladder_contact_seconds"):
                require(type(route.get(field)) in (int, float) and math.isfinite(route[field]),
                        "finite native dismount trajectory metadata required")
            require(route["exit_velocity_z"] == 115 and route["exit_initial_speed_xy"] == 0 and
                    route["exit_speed_xy"] >= 0 and route["exit_initial_wishspeed"] >= 0 and
                    0 < route["exit_duration_seconds"] <= 2.048001 and
                    abs(route["exit_duration_seconds"]/.008-round(route["exit_duration_seconds"]/.008)) <= .001 and
                    0 <= route["ladder_contact_seconds"] <= route["exit_duration_seconds"] and
                    abs(route["ladder_contact_seconds"]/.008-round(route["ladder_contact_seconds"]/.008)) <= .001,
                    "dismount trajectory metadata differs from native integration recipe")
        route_guides = guides + ([release] if release >= 0 else []) + ([alignment] if alignment >= 0 else [])
        require(not claimed_guides.intersection(route_guides), "guidance point claimed by multiple routes")
        claimed_guides.update(route_guides)
        g0, g1, g2, g3 = guides
        exported_ids = [low, high, g0, g1, g3] + ([g2] if route["reverse"] else []) + ([release] if release >= 0 else []) + ([alignment] if alignment >= 0 else [])
        require(all(value in by_id for value in exported_ids), "route requires absent/unexported points")
        width = report.get("bbox", {}).get("half_width")
        require(isinstance(width, (int, float)) and math.isfinite(width) and 0 < width <= 64,
                "finite native half-width required")
        physics = report.get("traversal_physics", {})
        support_radius = physics.get("landing_support_radius")
        require(type(physics.get("landing_support_samples")) is int and physics["landing_support_samples"] == 9 and
                isinstance(support_radius, (int, float)) and math.isfinite(support_radius) and
                abs(support_radius-width) <= .01,
                "current nine-sample full-footprint landing proof required; regenerate older ladder recipes")
        points = [all_nodes[value].get("position") for value in ids]
        require(all(isinstance(p, list) and len(p) == 3 and all(isinstance(x, (int, float)) and math.isfinite(x) for x in p)
                    for p in points), "invalid guidance geometry")
        a, b, p0, p1, p2, p3 = points
        touchdown = route.get("dismount_touchdown")
        continuation = route.get("dismount_ground_continuation")
        continuation_model = "native_walk_capsule_support_v1" if forward_exit else "native_walk_center_support_v2"
        require(route.get("ground_continuation_model") == continuation_model and
                physics.get("ground_continuation_step") == 18.25 and
                isinstance(touchdown, list) and len(touchdown) == 3 and
                all(type(x) in (int, float) and math.isfinite(x) for x in touchdown),
                "finite touchdown and current native step-supported ground-continuation proof required")
        require(type(continuation) in (int, float) and math.isfinite(continuation) and
                0 <= continuation <= 96.001 and abs(math.dist(touchdown, b)-continuation) <= .01 and
                abs(touchdown[2]-b[2]) <= 19.05,
                "touchdown-to-exit distance/height differs from native continuation recipe")
        if method == "supported":
            require(math.dist(touchdown, b) <= .05 and abs(continuation) <= .0001 and
                    type(route.get("ladder_contact_seconds")) in (int, float) and route["ladder_contact_seconds"] == 0,
                    "supported exit must not invent a ballistic ground continuation")
        require(b[2] > a[2] + 31.9, "exit must be above approach")
        require(math.dist(a[:2], p0[:2]) <= 64 + width + .1 and abs(a[2]-p0[2]) <= 18.1,
                "base contact is not local to approach")
        require(math.dist(b, p3) <= .05, "exit guide does not alias exit floor")
        require(math.dist(p0[:2], p2[:2]) <= .25 and -.05 <= p2[2]-b[2] <= 42.05,
                "top guidance is not above ladder shaft/exit")
        reasons = ["ladder_base_contact_live_pending", "ladder_inside_guidance_not_standable_live_pending",
                   "ladder_top_clearance_live_pending", "ladder_exit_floor_live_pending"]
        facing = all_nodes[g0].get("facing")
        require(isinstance(facing, list) and len(facing) == 3 and all(math.isfinite(x) for x in facing),
                "finite facing required")
        require(abs(math.hypot(*facing)-1) <= .01 and abs(facing[2]) <= .1,
                "ladder facing must be a horizontal unit vector")
        require(math.dist(p1, [p0[k]+width*facing[k] for k in range(3)]) <= .05,
                "inside guide does not match native half-width and facing")
        landing_binary = binary_nodes[by_id[high]["index"]]
        exit_binary = binary_nodes[by_id[g3]["index"]]
        top_position = binary_nodes[by_id[g2]["index"]]["position"] if g2 in by_id else p2
        separation = min(math.dist(top_position, landing_binary["position"]),
                         math.dist(top_position, exit_binary["position"]))
        expected_top_radius = min(8, math.floor(separation + .001) -
                                  max(landing_binary["radius"], exit_binary["radius"]))
        expected_release_radius = 6
        if release >= 0:
            release_position = binary_nodes[by_id[release]["index"]]["position"]
            gap = math.dist(top_position, release_position)
            if gap < expected_top_radius + expected_release_radius + .001:
                expected_top_radius = min(expected_top_radius, math.floor(gap*.5 + .001))
                expected_release_radius = min(6, math.floor(gap-expected_top_radius + .001)-1)
        require(expected_top_radius >= 1 and expected_release_radius >= 1,
                "descent/release guidance cannot fit separate arrival radii")
        for role, value in enumerate(guides):
            node = all_nodes[value]
            if value not in by_id:
                require(role == 2 and not route["reverse"], "required guidance node missing from WAY")
                continue
            binary = binary_nodes[node["index"]]
            require(binary["flags"] & 512 and node.get("nav_flags", 0) & 512,
                    "exported ladder guidance lacks CLIMB flag")
            require(node.get("reason") == reasons[role], "guidance role reason mismatch")
            expected_radius = width+4 if role == 1 else 8
            if role == 2:
                # Both the landing and a nearby lower ascent release limit
                # this radius; derive the exact native split from WAY points.
                expected_radius = expected_top_radius
                require(binary["radius"] +
                        max(landing_binary["radius"], exit_binary["radius"]) <= separation + .01,
                        "descent guidance radius overlaps actual landing/exit")
            require(abs(binary["radius"]-expected_radius) <= .01,
                    "guidance arrival radius differs from declared recipe")
            require(node.get("facing") is not None and math.dist(node["facing"], facing) <= .01,
                    "guidance facing mismatch")
        if release >= 0:
            node = all_nodes[release]
            binary = binary_nodes[node["index"]]
            launch, actual = route.get("launch"), node.get("position")
            require(all(isinstance(p, list) and len(p) == 3 and
                        all(isinstance(x, (int, float)) and math.isfinite(x) for x in p)
                        for p in (launch, actual)), "finite ascent-release geometry required")
            require(math.dist(actual, launch) <= .05 and
                    p0[2] < actual[2] <= b[2]+42.05, "ascent-release guide does not match shaft launch")
            dx, dy = b[0]-actual[0], b[1]-actual[1]
            tangent = dx*facing[1]-dy*facing[0]
            # The native classifier receives the outward surface normal,
            # whereas guidance facing points into the ladder. Match that
            # sign and bounded cone; CLIMB alone never proves a forward exit.
            horizontal2 = dx*dx+dy*dy
            is_forward = (horizontal2 > .000001 and dx*facing[0]+dy*facing[1] > 0 and
                          tangent*tangent <= .25*horizontal2)
            if forward_model:
                require(is_forward == forward_exit,
                        "exit control differs from native forward/tangent classification")
            if not forward_exit:
                require(abs(tangent) >= 1.95,
                        "ladder-strafe exit has no native minimum tangential displacement")
            if alignment < 0:
                require(math.dist(p0[:2], actual[:2]) <= .25,
                        "offset ascent-release requires upper alignment guidance")
            require(binary["flags"] & 512 and node.get("nav_flags", 0) & 512 and
                    node.get("reason") == "ladder_ascent_release_contact_live_pending",
                    "ascent-release guide lacks CLIMB or correct role")
            require(abs(binary["radius"]-expected_release_radius) <= .01 and node.get("facing") is not None and
                    math.dist(node["facing"], facing) <= .01, "ascent-release radius/facing mismatch")
            require(binary.get("properties", {}).get("tce_ascent_release") == "1",
                    "ballistic release lacks exact WAY property tce_ascent_release=1")
            release_indices.add(node["index"])
            release_incoming = {source for source, target in edge_reasons if target == release}
            release_outgoing = {target for source, target in edge_reasons if source == release}
            require(release_incoming == {alignment if alignment >= 0 else g0} and
                    release_outgoing == {g3} and (g0, g3) not in edge_reasons,
                    "ascent-release is bypassed or has undeclared connections")
            if alignment >= 0:
                alignment_node = all_nodes[alignment]
                alignment_binary = binary_nodes[alignment_node["index"]]
                aligned = alignment_node.get("position")
                require(isinstance(aligned, list) and len(aligned) == 3 and
                        all(isinstance(x, (int, float)) and math.isfinite(x) for x in aligned),
                        "finite upper-alignment geometry required")
                # The native dismount may snap its actual landing by <=1u
                # from the upper floor used for the alignment search.
                require(math.dist(aligned[:2], actual[:2]) <= .05 and actual[2]-aligned[2] >= 7.95 and
                        any(abs(aligned[2]-b[2]-offset) <= 1.05 for offset in (0, 2, 4, 8)),
                        "upper-alignment height/column differs from native recipe")
                delta = [aligned[k]-p0[k] for k in range(2)]
                normal_shift = delta[0]*facing[0]+delta[1]*facing[1]
                tangent_shift = abs(delta[0]*facing[1]-delta[1]*facing[0])
                require(abs(normal_shift) <= .25 and 3.95 <= tangent_shift <= width+8.05 and
                        abs(tangent_shift-2*round(tangent_shift/2)) <= .05,
                        "upper-alignment does not follow a bounded native tangent lane")
                require(alignment_binary["flags"] & 512 and alignment_node.get("nav_flags", 0) & 512 and
                        alignment_node.get("reason") == "ladder_upper_alignment_contact_live_pending",
                        "upper-alignment guide lacks CLIMB or correct role")
                require(abs(alignment_binary["radius"]-1) <= .01 and alignment_node.get("facing") is not None and
                        math.dist(alignment_node["facing"], facing) <= .01,
                        "upper-alignment radius/facing mismatch")
                require(alignment_binary.get("properties", {}).get("tce_ascent_alignment") == "1",
                        "upper-alignment lacks exact WAY property tce_ascent_alignment=1")
                alignment_indices.add(alignment_node["index"])
                alignment_incoming = {source for source, target in edge_reasons if target == alignment}
                alignment_outgoing = {target for source, target in edge_reasons if source == alignment}
                require(alignment_incoming == {g0} and alignment_outgoing == {release} and
                        (g0, release) not in edge_reasons,
                        "upper-alignment is bypassed or has undeclared connections")
        required_edges = [(low, g0, "ladder_approach_validated_live_pending"),
                          (g0, low, "ladder_exit_validated_live_pending"),
                          (g0, g1, "ladder_contact_guidance_live_pending"),
                          (g1, g0, "ladder_contact_guidance_live_pending"),
                          (g3, high, "ladder_exit_floor_live_pending"),
                          (high, g3, "ladder_exit_floor_live_pending")]
        if release >= 0:
            if alignment >= 0:
                required_edges.extend([(g0, alignment, "ladder_aligned_ascent_live_pending"),
                                       (alignment, release, "ladder_release_column_live_pending")])
            else:
                required_edges.append((g0, release, "ladder_directed_ascent_live_pending"))
            required_edges.append((release, g3, "ladder_ballistic_dismount_live_pending"))
        else:
            required_edges.append((g0, g3, "ladder_directed_ascent_live_pending"))
        lifted_exit = route.get("lifted_exit")
        if "lifted_exit" in route:
            require(type(lifted_exit) is bool, "explicit lifted-exit direction must be boolean")
            if lifted_exit:
                require(g2 in by_id, "proved lifted-exit source is not exported")
            else:
                require((g2, g3) not in edge_reasons and
                        (g2 not in by_id or (by_id[g2]["index"], by_id[g3]["index"]) not in binary_edges),
                        "unproved lifted-exit direction is present in JSON or WAY")
        # Older receipts required this extra direction whenever the raised
        # descent guide survived. Only an explicit native false declaration
        # separates the proved ascent/descent from this optional body walk.
        if g2 in by_id and ("lifted_exit" not in route or lifted_exit):
            required_edges.append((g2, g3, "ladder_lifted_exit_validated_live_pending"))
        if route["reverse"]:
            required_edges.extend([(g3, g2, "ladder_approach_validated_live_pending"),
                                   (g2, g1, "ladder_directed_descent_live_pending")])
        for source, target, reason in required_edges:
            require(edge_reasons.get((source, target)) == reason and
                    (by_id[source]["index"], by_id[target]["index"]) in binary_edges,
                    f"missing directed recipe edge {source}->{target} ({reason})")
        inside_incoming = {source for source, target in edge_reasons if target == g1}
        inside_outgoing = {target for source, target in edge_reasons if source == g1}
        require(inside_outgoing == {g0} and inside_incoming == ({g0, g2} if route["reverse"] else {g0}),
                "inside guidance has an ordinary/undeclared traversal connection")
        expected = {frozenset((g0, g1)): "ladder_contact_guidance",
                    frozenset((low, g0)): "ladder_mount_contact",
                    frozenset((low, g1)): "ladder_inside_approach_guidance",
                    frozenset((high, g3)): "ladder_exit_floor_alias"}
        declarations = route.get("overlap_pairs")
        require(isinstance(declarations, list) and len(declarations) == len(expected),
                "exact overlap-pair declarations required")
        declared = {}
        for pair in declarations:
            first, second = pair.get("first"), pair.get("second")
            require(type(first) is int and type(second) is int, "invalid overlap-pair IDs")
            key = frozenset((first, second))
            require(key in expected and key not in declared and pair.get("reason") == expected[key],
                    "undeclared/spoofed overlap exemption")
            declared[key] = dict(route_index=route_index, kind="ladder", reason=expected[key],
                                 low=low, high=high, structurally_verified=True, live_verified=False)
        allowed.update(declared)
        verified.append(dict(route_index=route_index, kind="ladder", low=low, high=high,
                             guides=guides, ascent_release=release, ascent_alignment=alignment,
                             dismount_touchdown=touchdown, dismount_ground_continuation=continuation,
                             physics_model=physics_model, exit_control=exit_control,
                             ground_continuation_model=continuation_model,
                             reverse=route["reverse"], lifted_exit=lifted_exit if "lifted_exit" in route else g2 in by_id,
                             exported=True,
                             structurally_verified=True, live_verified=False))
    if report.get("candidate_mode") == "manual_draft":
        for uid,node in by_id.items():
            role=node.get("editor_role",0)
            if not role&6: continue
            proof=node.get("native_input",{})
            if proof.get("floor_accepted") is not True or proof.get("initial_rejected_incident_links")!=0:
                raise ValueError("authored ladder marker lacks native transition validation")
            index=node["index"]
            if role&2:
                if binary_nodes[index]["properties"].get("tce_ascent_release")!="1": raise ValueError("authored release marker lost")
                release_indices.add(index)
            if role&4:
                if binary_nodes[index]["properties"].get("tce_ascent_alignment")!="1": raise ValueError("authored alignment marker lost")
                alignment_indices.add(index)
    for index, node in enumerate(binary_nodes):
        if "tce_ascent_release" in node.get("properties", {}) and index not in release_indices:
            raise ValueError("WAY tce_ascent_release property on a non-release/unproven waypoint")
        if "tce_ascent_alignment" in node.get("properties", {}) and index not in alignment_indices:
            raise ValueError("WAY tce_ascent_alignment property on a non-alignment/unproven waypoint")
    return allowed, verified


WALK_REASONS = frozenset(("accepted", "external_native_validated_walk",
                         "simplified_directed_walk_validated", "external_native_local_shortcut",
                         "external_native_component_bridge", "external_native_junction_walk"))


def final_walk_graph(report, by_id, nodes, binary_edges, junction_only=False):
    """Only exported, native ordinary walks may witness removed ordinary links.

    A ladder's floor-continuation edge can join ordinary anchors, so checking
    node flags alone would accidentally allow an action-specific substitute.
    """
    graph = [[] for _ in nodes]
    for edge in report["edges"]:
        reason = edge.get("reason")
        if not edge["accepted"] or reason not in WALK_REASONS:
            continue
        if junction_only and reason != "external_native_junction_walk":
            continue
        a, b = by_id[edge["from"]]["index"], by_id[edge["to"]]["index"]
        if (a, b) not in binary_edges or (nodes[a]["flags"] | nodes[b]["flags"]) & ~64:
            continue
        duck = bool(edge.get("crouch") or (nodes[a]["flags"] | nodes[b]["flags"]) & 64)
        graph[a].append((b, math.dist(nodes[a]["position"], nodes[b]["position"]), duck))
    return graph


def bounded_walk_distance(graph, start, target, limit, allow_crouch, exact_stance=False):
    """Dijkstra bounded by distance and the finite final graph, without actions."""
    best, pending = {start: 0.0}, [(0.0, start)]
    while pending:
        distance, node = heapq.heappop(pending)
        if distance != best[node]:
            continue
        if node == target:
            return distance
        for neighbor, length, duck in graph[node]:
            if (duck and not allow_crouch) or (exact_stance and duck != allow_crouch):
                continue
            total = distance + length
            if total <= limit and total < best.get(neighbor, math.inf):
                best[neighbor] = total
                heapq.heappush(pending, (total, neighbor))
    return None


def audit_removed_walks(report, by_id, nodes, graph):
    checked = Counter()
    contracted = []
    all_nodes = {node["id"]: node for node in report["nodes"]}
    reasons = {"redundant_chord_shorter_directed_witness": ("chords", 1.25),
               "redundant_traversal_spoke_native_walk_witness": ("seed_spokes", 1.5)}
    for edge in report["edges"]:
        if edge.get("reason") not in reasons:
            continue
        kind, stretch = reasons[edge["reason"]]
        if edge["accepted"]:
            raise ValueError("removed " + kind + " is still accepted")
        missing = [identity for identity in (edge["from"], edge["to"]) if identity not in by_id]
        if missing:
            # Seed fans are pruned before contraction; their original endpoints
            # can cease to be exported. Do not invent an owner or claim a final
            # witness for these explicitly recorded intermediate removals.
            if kind != "seed_spokes" or any(all_nodes.get(identity, {}).get("reason") !=
                                           "contracted_to_validated_walk_neighbor" for identity in missing):
                raise ValueError("removed " + kind + " has an unexplained missing final endpoint")
            contracted.append(dict(first=edge["from"], second=edge["to"], contracted_endpoints=missing))
            continue
        a, b = by_id[edge["from"]]["index"], by_id[edge["to"]]["index"]
        direct = math.dist(nodes[a]["position"], nodes[b]["position"])
        duck = bool(edge.get("crouch") or (nodes[a]["flags"] | nodes[b]["flags"]) & 64)
        if (a == b or (nodes[a]["flags"] | nodes[b]["flags"]) & ~64 or
                bounded_walk_distance(graph, a, b, direct * stretch + 0.001, duck) is None):
            raise ValueError("removed " + kind + " has no safe final directed witness: " +
                             str(edge["from"]) + "->" + str(edge["to"]))
        checked[kind] += 1
    if checked["chords"] != report.get("sparsification", {}).get("removed_chords", 0):
        raise ValueError("sparsification count mismatch")
    return checked, contracted


def audit_native_junctions(report, by_id, nodes, binary_edges):
    """Verify crossing geometry and all replacement directions in the WAY graph."""
    data = report.get("native_junctions")
    replacement_reason = "external_crossing_replaced_by_native_junction"
    walk_reason = "external_native_junction_walk"
    tagged = [edge for edge in report["edges"] if edge.get("reason") in (replacement_reason, walk_reason)]
    if data is None:
        if tagged:
            raise ValueError("junction edges lack their native report")
        return dict(verified_cases=0, verified_directed_replacements=0)
    cases = data.get("cases", [])
    if (data.get("trace_budget") != 20000 or data.get("junction_limit") != 16 or
            data.get("max_height_difference") != 1 or data.get("max_replacement_ratio") != 1.01 or
            not isinstance(cases, list) or len(cases) > 128 or data.get("checked") != len(cases) or
            not isinstance(data.get("traces"), int) or not 0 <= data["traces"] <= 20000 or
            data.get("accepted") != sum(case.get("accepted") is True for case in cases) or
            not 0 <= data["accepted"] <= 16):
        raise ValueError("invalid native junction limits/counts")
    edges = {}
    for edge in report["edges"]:
        pair = (edge["from"], edge["to"])
        if pair in edges and (edge in tagged or edges[pair] in tagged):
            raise ValueError("ambiguous native junction edge records")
        edges[pair] = edge
    graph = final_walk_graph(report, by_id, nodes, binary_edges, junction_only=True)
    declared_originals, declared_legs, checked, new_nodes = set(), set(), 0, set()
    for number, case in enumerate(cases):
        if type(case.get("accepted")) is not bool:
            raise ValueError("invalid junction acceptance flag")
        if not case["accepted"]:
            if case.get("junction") != -1 or case.get("reused"):
                raise ValueError("rejected junction claims a committed vertex")
            continue
        prefix = "native junction " + str(number) + ": "
        identities = case.get("nodes", [])
        point, junction = case.get("position", []), case.get("junction")
        duck = case.get("crouch")
        if (len(identities) != 4 or len(set(identities)) != 4 or
                any(identity not in by_id for identity in identities) or junction not in by_id or
                len(point) != 3 or not all(math.isfinite(value) for value in point) or
                type(duck) is not bool or type(case.get("reused")) is not bool or
                case.get("reason") != "native_floor_and_all_directed_legs_accepted"):
            raise ValueError(prefix + "invalid exported recipe")
        indices = [by_id[identity]["index"] for identity in identities]
        ji = by_id[junction]["index"]
        if (any(nodes[index]["flags"] & ~64 for index in indices + [ji]) or
                any(by_id[identity].get("flags") or by_id[identity].get("packed")
                    for identity in identities + [junction])):
            raise ValueError(prefix + "action vertex used as an ordinary crossing")
        if not duck and any(nodes[index]["flags"] & 64 for index in indices + [ji]):
            raise ValueError(prefix + "standing crossing requires crouch")
        p, q, r, s = [nodes[index]["position"] for index in indices]
        ux, uy, vx, vy = q[0]-p[0], q[1]-p[1], s[0]-r[0], s[1]-r[1]
        denominator = ux*vy-uy*vx
        if abs(denominator) < 0.01:
            raise ValueError(prefix + "parallel segments are not a crossing")
        t = ((r[0]-p[0])*vy-(r[1]-p[1])*vx)/denominator
        w = ((r[0]-p[0])*uy-(r[1]-p[1])*ux)/denominator
        z1, z2 = p[2]+t*(q[2]-p[2]), r[2]+w*(s[2]-r[2])
        intersection = [p[0]+t*ux, p[1]+t*uy, (z1+z2)*0.5]
        actual = nodes[ji]["position"]
        if (not 0.00001 < t < 0.99999 or not 0.00001 < w < 0.99999 or
                abs(z1-z2) > 1.001 or math.dist(point, intersection) > 0.002 or
                abs(actual[2]-point[2]) > 1.001):
            raise ValueError(prefix + "crossing position/height mismatch")
        if case["reused"]:
            if math.dist(actual, point) >= 8.001:
                raise ValueError(prefix + "reused vertex outside native search radius")
        elif (junction in new_nodes or by_id[junction].get("reason") != "external_native_crossing_junction" or
              by_id[junction].get("topology_protected") is not True or
              math.dist(actual[:2], point[:2]) > 0.002):
            raise ValueError(prefix + "new crossing vertex lacks its topology/position proof")
        else:
            new_nodes.add(junction)
        original_pairs = [(identities[0], identities[1]), (identities[2], identities[3])]
        ratio = 1.0
        for pair in original_pairs + [(b, a) for a, b in original_pairs]:
            edge = edges.get(pair)
            if edge is None or edge.get("reason") not in (replacement_reason, walk_reason):
                if pair in original_pairs:
                    raise ValueError(prefix + "original directed crossing edge missing")
                if edge and edge["accepted"]:
                    raise ValueError(prefix + "reverse original was not replaced")
                continue
            a, b = (by_id[identity]["index"] for identity in pair)
            if bool(edge.get("crouch") or (nodes[a]["flags"] | nodes[b]["flags"]) & 64) != duck:
                raise ValueError(prefix + "original stance mismatch")
            direct = math.dist(nodes[a]["position"], nodes[b]["position"])
            split = math.dist(nodes[a]["position"], actual) + math.dist(actual, nodes[b]["position"])
            if direct < 0.01 or split > direct*1.01+0.01:
                raise ValueError(prefix + "replacement detour exceeds native limit")
            ratio = max(ratio, split/direct)
            declared_originals.add(pair)
            for first, second in ((pair[0], junction), (junction, pair[1])):
                if first == second:  # Endpoint reuse has no binary self-edge.
                    continue
                declared_legs.add((first, second))
                fi, si = by_id[first]["index"], by_id[second]["index"]
                length = math.dist(nodes[fi]["position"], nodes[si]["position"])
                if bounded_walk_distance(graph, fi, si, length*1.01+0.01, duck, exact_stance=True) is None:
                    raise ValueError(prefix + "directed leg lacks a final native junction walk")
            if bounded_walk_distance(graph, a, b, direct*1.01+0.01, duck, exact_stance=True) is None:
                raise ValueError(prefix + "final replacement route exceeds native limit")
            checked += 1
        if not math.isfinite(case.get("replacement_ratio", math.nan)) or abs(case["replacement_ratio"]-ratio) > 0.000005:
            raise ValueError(prefix + "replacement ratio differs from exported geometry")
    for edge in tagged:
        pair = (edge["from"], edge["to"])
        if edge["reason"] == replacement_reason:
            if edge["accepted"] or pair not in declared_originals:
                raise ValueError("undeclared/still accepted native crossing replacement")
        elif not edge["accepted"] or pair not in declared_legs:
            raise ValueError("undeclared/rejected native junction walk")
    return dict(verified_cases=data["accepted"], verified_directed_replacements=checked,
                reused_vertices=sum(case.get("accepted") and case.get("reused") for case in cases))


def audit_standing_jumps(report,by_id,binary_nodes,binary_edges):
    """Bind full-impulse permission to an exact slow, directed launch recipe."""
    launches=set();verified=[];receipts=set()
    reasons={(e["from"],e["to"]):e.get("reason") for e in report["edges"] if e.get("accepted")}
    def target(a,b):
        if a not in by_id or b not in by_id or a==b:
            raise ValueError("standing jump endpoint is absent from this current report")
        ai,bi=by_id[a]["index"],by_id[b]["index"]
        props=binary_nodes[ai].get("properties",{})
        if (props.get("tce_stand_jump")!="1" or props.get("tce_stand_jump_target")!=str(binary_nodes[bi]["uid"]) or
            (ai,bi) not in binary_edges or by_id[a].get("crouch") or by_id[b].get("crouch")):
            raise ValueError("standing jump lacks its serialized directed native target")
        launches.add(ai)
    def authored(a,b):
        node=by_id[a]
        if (report.get("candidate_mode")!="manual_draft" or
            not node.get("editor_role",0)&64 or node.get("editor_drop_target")!=b or
            node.get("native_input",{}).get("initial_rejected_incident_links")!=0 or
            reasons.get((a,b))!="manual_native_transition_live_pending"):
            raise ValueError("authored standing jump lacks a current native recipe check")
    for r in report.get("traversal_routes",[]):
        if r.get("kind")!="jump" or not r.get("standing_launch") or not r.get("exported"):continue
        a,b=r.get("launch"),r.get("landing");target(a,b)
        is_authored=bool(by_id[a].get("editor_role",0)&64)
        if is_authored:authored(a,b)
        if (a,b) in receipts:
            raise ValueError("duplicate standing jump native receipt")
        impulse=337.5 if report.get("bbox",{}).get("origin_above_feet")==30 else 270
        if (r.get("arc_model")!="low_velocity_forward_jump_capsule_samples_v1" or r.get("impulse_z")!=impulse or
            r.get("launch_speed_samples")!=[36,40,44] or r.get("wishspeed_samples")!=[114,160,204] or
            r.get("launch_offset_samples")!=[0,-2,2] or r.get("sample_count")!=27 or
            r.get("step_seconds")!=.008 or r.get("prime_wishspeed")!=40 or r.get("stop_offset")!=8 or
            r.get("launch_radius")!=2 or by_id[a].get("radius")!=2 or
            (not is_authored and reasons.get((a,b))!="jump_hull_arc_validated_live_pending") or
            r.get("live_verified") is not False):
            raise ValueError("standing jump metadata differs from the actual stopped/primed native recipe")
        for key in ("duration_seconds","ground_continuation","landing_speed_z","crashland_delta"):
            if type(r.get(key)) not in (int,float) or not math.isfinite(r[key]) or r[key]<0:
                raise ValueError("standing jump has invalid sampled physics metadata")
        touchdown=r.get("touchdown")
        if (not isinstance(touchdown,list) or len(touchdown)!=3 or not all(type(x) in (int,float) and math.isfinite(x) for x in touchdown) or
            not 0<r["duration_seconds"]<=1.44 or r["crashland_delta"]>38 or r["ground_continuation"]>96.05 or
            abs(math.dist(touchdown,by_id[b]["position"])-r["ground_continuation"])>.01):
            raise ValueError("standing jump touchdown/continuation is unproved")
        receipts.add((a,b))
        verified.append(dict(kind="manual_standing_jump" if is_authored else "standing_jump",
                             launch=a,landing=b,native_samples=27,live_verified=False))
    for a,node in by_id.items():
        if not node.get("editor_role",0)&64:continue
        b=node.get("editor_drop_target");target(a,b)
        authored(a,b)
        if (a,b) not in receipts:
            verified.append(dict(kind="manual_standing_jump",launch=a,landing=b,native_samples=27,live_verified=False))
    for i,node in enumerate(binary_nodes):
        if {"tce_stand_jump","tce_stand_jump_target"}&node.get("properties",{}).keys() and i not in launches:
            raise ValueError("standing-jump WAY properties lack a current native proof")
    return verified


def audit_free_climbs(report, by_id, binary_nodes, binary_edges):
    """Freiklettern has its own directed native recipe; CLIMB is insufficient."""
    verified, launches = [], set()
    reasons = {(e["from"], e["to"]): e.get("reason") for e in report["edges"] if e["accepted"]}
    scale = 1.25 if report.get("bbox", {}).get("origin_above_feet") == 30 else 1
    for number, route in enumerate(report.get("traversal_routes", [])):
        if route.get("kind") != "free_climb" or route.get("exported") is not True:
            continue
        def require(condition, message):
            if not condition: raise ValueError(f"free-climb route {number}: {message}")
        a, b, approach = (route.get(key) for key in ("launch", "landing", "approach"))
        require(all(type(uid) is int and uid in by_id for uid in (a, b, approach)) and a != b,
                "unknown or aliased directed endpoints")
        launch, landing = by_id[a], by_id[b]
        ai, bi = launch["index"], landing["index"]
        props = binary_nodes[ai].get("properties", {})
        require(ai not in launches and props.get("tce_free_climb") == "1" and
                props.get("tce_free_climb_target") == str(binary_nodes[bi]["uid"]) and
                (ai, bi) in binary_edges and launch.get("nav_flags", 0) & 512 and
                not launch.get("nav_flags", 0) & ((1<<27)|(1<<18)|(1<<8)|(1<<20)) and
                not launch.get("crouch") and not landing.get("crouch"),
                "missing directed target, standing posture or dedicated CLIMB recipe")
        authored = bool(launch.get("editor_role", 0) & 128)
        if authored:
            require(report.get("candidate_mode") == "manual_draft" and launch.get("editor_drop_target") == b and
                    reasons.get((a, b)) == "manual_native_transition_live_pending",
                    "authored climb lacks current directed transition validation")
        else:
            require(reasons.get((a, b)) == "free_climb_hull_samples_validated_live_pending",
                    "generated climb lacks native movement evidence")
        if approach != a:
            require((by_id[approach]["index"], ai) in binary_edges and
                    reasons.get((approach, a)) == "free_climb_approach_validated_live_pending",
                    "missing checked approach route")
        require(route.get("model") == "tce_free_climb_capsule_speed_samples_v1" and
                route.get("wishspeed_samples") == [114,160,204] and route.get("sample_count") == 3 and
                route.get("step_seconds") == .008 and route.get("initial_stamina") == 2500 and
                abs(route.get("initial_impulse_z", 0)-270*scale) < .001 and
                abs(route.get("max_ledge", 0)-101*scale) < .001 and
                launch.get("radius") == route.get("launch_radius") == 2 and route.get("live_verified") is False,
                "climb physics/action contract differs from native PM_Climb recipe")
        for key in ("duration_seconds", "climb_seconds", "stamina_used", "landing_speed_z", "crashland_delta"):
            require(type(route.get(key)) in (int,float) and math.isfinite(route[key]) and route[key] >= 0,
                    "invalid scalar evidence: " + key)
        require(0 < route["climb_seconds"] <= route["duration_seconds"] <= 2.0001 and
                0 < route["stamina_used"] <= 2500 and route["crashland_delta"] <= 38,
                "unbounded climb, missing stamina consumption or damaging impact")
        for key in ("touchdown", "direction"):
            require(isinstance(route.get(key), list) and len(route[key]) == 3 and
                    all(type(v) in (int,float) and math.isfinite(v) for v in route[key]),
                    "invalid movement geometry: " + key)
        direction = route["direction"]
        require(abs(math.hypot(*direction)-1) < .01 and abs(direction[2]) < .001,
                "climb direction must be a horizontal unit vector")
        require(math.dist(route["touchdown"], landing["position"]) <= 98,
                "native touchdown does not reach the checked landing corridor")
        endpoint_rise = landing["position"][2]-launch["position"][2]
        if route.get("endpoint_rule") is None:
            endpoint_valid = endpoint_rise >= 63-.05
        else:
            require(route.get("endpoint_rule") == "measured_lip_and_landing_floor_v1" and
                    route.get("contact_min_ledge") == 63 and route.get("minimum_endpoint_rise") == 18.25,
                    "measured native lip/landing-floor contract differs")
            endpoint_valid = endpoint_rise > 18.25
        require(endpoint_valid and endpoint_rise <= 101*scale+.05 and
                0 < math.dist(landing["position"][:2], launch["position"][:2]) <= 160+.05,
                "free-climb endpoints outside the checked landing-floor range")
        launches.add(ai)
        verified.append(dict(kind="free_climb", launch=a, landing=b, native_samples=3, live_verified=False))
    for identity, node in by_id.items():
        if node.get("editor_role", 0) & 128 and node["index"] not in launches:
            raise ValueError("authored free-climb lacks its current sampled native receipt")
    for index, node in enumerate(binary_nodes):
        if {"tce_free_climb", "tce_free_climb_target"} & node.get("properties", {}).keys() and index not in launches:
            raise ValueError("WAY free-climb property on an unproved waypoint")
    return verified


def audit_walk_drops(report, by_id, binary_nodes, binary_edges):
    """Validate directed no-jump recipes and their exact serialized target UID.

    Native evidence is sampled collision/physics proof, not a live-success
    claim. A property alone never authorizes a drop or an arrival exemption.
    """
    verified, launch_indices, declared_edges = [], set(), set()
    reasons = {(e["from"], e["to"]): e.get("reason") for e in report["edges"] if e["accepted"]}
    jumps = {r.get("launch"): r for r in report.get("traversal_routes", [])
             if r.get("kind") == "jump" and r.get("exported") is True}
    for number, route in enumerate(report.get("traversal_routes", [])):
        if route.get("kind") != "walk_drop" or route.get("exported") is not True:
            continue

        def require(condition, message):
            if not condition:
                raise ValueError(f"walk-drop route {number}: {message}")

        keys = ("approach", "launch", "landing", "paired_jump_launch")
        require(all(type(route.get(k)) is int and route[k] in by_id for k in keys), "unknown endpoint")
        approach, launch, landing, paired = (route[k] for k in keys)
        launch_node, landing_node = by_id[launch], by_id[landing]
        authored = paired == launch
        require(launch != landing, "aliased action endpoints")
        if authored:
            require(report.get("candidate_mode") == "manual_draft" and approach == launch and
                    launch_node.get("editor_role", 0) & 8 and
                    launch_node.get("editor_drop_target") == landing,
                    "standalone descent lacks its authored directed native recipe")
        else:
            require(paired != landing and paired in jumps, "missing exported reciprocal ascent")
            jump = jumps[paired]
            require(all(jump.get(k) in by_id for k in ("approach", "launch", "landing")), "incomplete reciprocal jump")
            require(by_id[paired].get("nav_flags") == 1 << 27, "reciprocal launch is not JUMP")
            for a, b, reason in ((jump["approach"], paired, "jump_takeoff_validated_live_pending"),
                                 (paired, jump["landing"], "jump_hull_arc_validated_live_pending")):
                require(reasons.get((a, b)) == reason, "reciprocal ascent edge is unproved")
        start, end = launch_node["position"], landing_node["position"]
        require(all(by_id[uid].get("nav_flags", 0) == 0 and not by_id[uid].get("crouch")
                    for uid in (approach, launch, landing)), "drop requires ordinary standing endpoints")
        if not authored:
            require(launch_node.get("reason") == "generated_walk_drop_takeoff_live_pending" and
                    landing_node.get("reason") in ("walk_drop_landing_anchor", "map_entity_spawn_seed"), "unexpected drop node roles")
        require(launch_node["radius"] == route.get("launch_radius") == 2 and
                landing_node["radius"] == route.get("landing_radius") and
                0 < landing_node["radius"] <= 6, "unsafe arrival radii")
        require(launch_node["index"] not in launch_indices, "duplicate directed launch recipe")
        launch_indices.add(launch_node["index"])
        props = binary_nodes[launch_node["index"]].get("properties", {})
        require(props.get("tce_walk_drop") == "1" and props.get("tce_walk_drop_target") ==
                str(binary_nodes[landing_node["index"]]["uid"]), "WAY drop marker/target mismatch")
        chain = [(launch, landing, "manual_native_transition_live_pending" if authored else "walk_drop_hull_validated_live_pending")]
        if approach != launch:
            chain.append((approach, launch, "walk_drop_approach_validated_live_pending"))
        for a, b, reason in chain:
            require(reasons.get((a, b)) == reason and
                    (by_id[a]["index"], by_id[b]["index"]) in binary_edges, "missing proved directed edge")
            declared_edges.add((a, b))
        require(route.get("model") == "tce_ground_then_fall_speed_samples_v3" and
                route.get("launch_support_model") == "stationary_capsule_ground_trace_025" and
                route.get("wishspeed_samples") == [114, 160, 204] and route.get("sample_count") == 3 and
                route.get("step_seconds") == .008 and route.get("initial_velocity") == [0, 0, 0] and
                route.get("impulse_z") == 0, "unknown no-jump physics evidence")
        max_drop = route.get("max_drop")
        if any(key in route for key in ("gravity", "crashland_scale", "crashland_limit")):
            gravity = report.get("traversal_physics", {}).get("gravity")
            scale = .64 if report.get("bbox", {}).get("origin_above_feet") == 30 else 1
            require(type(gravity) in (int, float) and math.isfinite(gravity) and 100 <= gravity <= 2000 and
                    route.get("gravity") == gravity and route.get("crashland_scale") == scale and
                    route.get("crashland_limit") == 38 and type(max_drop) in (int, float) and
                    abs(max_drop - 38 / (2 * gravity * scale * .0002)) < .001,
                    "drop bound differs from actual gravity/body-scaled damage threshold")
        else:
            require(not authored and max_drop == 64, "missing native impact-bound evidence")
        require(route.get("max_horizontal") == 160 and
                route.get("landing_support_samples") == 9 and
                route.get("touchdown_support_model") == "native_walk_center_support_v2" and
                route.get("intermediate_contact_limit") == 8 and
                type(route.get("intermediate_contacts")) is int and 0 <= route["intermediate_contacts"] <= 8,
                "unknown geometry limits")
        require(8-.05 <= math.dist(start[:2], end[:2]) <= 160+.05 and
                18.25-.05 < start[2]-end[2] <= max_drop+.05, "drop endpoints outside proved range")
        for key in ("duration_seconds", "contact_seconds", "ground_continuation", "landing_speed_z", "crashland_delta"):
            require(type(route.get(key)) in (int, float) and math.isfinite(route[key]) and route[key] >= 0,
                    "invalid scalar evidence: " + key)
        require(0 < route["duration_seconds"] <= 2.0001 and
                route["contact_seconds"] < route["duration_seconds"], "no bounded falling phase")
        require(route["ground_continuation"] <= 96.05 and route["crashland_delta"] <= 38 and
                route.get("live_verified") is False, "unsupported safety/live claim")
        touchdown = route.get("touchdown")
        require(isinstance(touchdown, list) and len(touchdown) == 3 and
                all(type(v) in (int, float) and math.isfinite(v) for v in touchdown), "invalid touchdown")
        require(abs(touchdown[2]-end[2]) <= 19.05 and start[2]-touchdown[2] <= max_drop+.3 and
                abs(math.dist(touchdown, end)-route["ground_continuation"]) < .01,
                "ground continuation differs from geometry")
        horizontal = math.dist(start[:2], end[:2])
        overshoot = sum((touchdown[k]-end[k])*(end[k]-start[k]) for k in (0, 1)) / horizontal
        max_overshoot, max_travel = 8, 160
        if any(key in route for key in ("max_endpoint_horizontal", "max_airborne_overshoot", "maxtravel")):
            require(route.get("max_endpoint_horizontal") == 160 and
                    route.get("max_airborne_overshoot") == 48 and route.get("maxtravel") == 208,
                    "unknown bounded airborne/ground-return corridor")
            max_overshoot, max_travel = 48, 208
        require(overshoot <= max_overshoot+.05 and
                horizontal+overshoot <= max_travel+.05,
                "touchdown exceeds the checked airborne/ground-return corridor")
        verified.append(dict(route_index=number, kind="manual_walk_drop" if authored else "walk_drop", launch=launch, landing=landing,
                             paired_jump_launch=paired, native_samples=3, live_verified=False))
    for uid,node in by_id.items():
        if not node.get("editor_role",0)&8: continue
        target=node.get("editor_drop_target")
        if (report.get("candidate_mode")!="manual_draft" or target not in by_id or
            reasons.get((uid,target))!="manual_native_transition_live_pending"):
            raise ValueError("authored drop lacks native transition proof")
        index=node["index"]; target_uid=binary_nodes[by_id[target]["index"]]["uid"]
        if (binary_nodes[index]["properties"].get("tce_walk_drop") != "1" or
            binary_nodes[index]["properties"].get("tce_walk_drop_target")!=str(target_uid) or
            (index, by_id[target]["index"]) not in binary_edges):
            raise ValueError("authored drop target was not remapped to WAY UID")
        if index in launch_indices:
            continue
        raise ValueError("authored drop lacks its current sampled impact/geometry receipt")
    for index, node in enumerate(binary_nodes):
        if {"tce_walk_drop", "tce_walk_drop_target"} & node.get("properties", {}).keys() and index not in launch_indices:
            raise ValueError("WAY walk-drop property on an unproved waypoint")
    for pair, reason in reasons.items():
        if reason in ("walk_drop_approach_validated_live_pending", "walk_drop_hull_validated_live_pending") and pair not in declared_edges:
            raise ValueError("undeclared walk-drop action edge")
    return verified


def audit_doors(report, by_id, binary_nodes, binary_edges):
    """Bind conditional door actions to current native receipts and WAY IDs."""
    verified = []
    fields = ('entity','use_entity','target_source_id','mode','team','role',
              'model','use_model','use_source_id','entry_source_id')
    references = {'target_source_id':'target', 'use_source_id':'use_nav', 'entry_source_id':'entry'}
    edges = {(e['from'],e['to']):e for e in report['edges'] if e['accepted']}
    for identity, node in by_id.items():
        props = binary_nodes[node['index']].get('properties', {})
        recipe = node.get('door_recipe')
        if not recipe:
            if any(key.startswith('tce_door_') for key in props):
                raise ValueError('door WAY properties lack a current native receipt')
            continue
        def require(condition, message):
            if not condition:raise ValueError('door audit: '+message)
        require(recipe.get('validated') is True and recipe.get('exported') is True,
                'unvalidated or unexported recipe')
        require(recipe.get('mode') in (1,2,3) and recipe.get('team') in (1,2,3) and recipe.get('role') in (1,2,3),
                'unknown activation mode, team or role')
        if recipe['mode'] == 3:
            require(recipe.get('use_entity') == recipe.get('entity') and
                    recipe.get('use_model') == recipe.get('model') and
                    recipe.get('use_source_id') != recipe.get('entry_source_id'),
                    'staged local USE must address this door from a separate waypoint')
        for field in fields:
            value = recipe.get(field)
            require(type(value) is int, 'integer field missing: '+field)
            if field in references:
                require(value >= 0 or field=='target_source_id' and recipe['role']==3,
                        'required graph reference missing')
                require(value < 0 or value in by_id, 'reference is not an exported node')
                expected = 0 if value < 0 else by_id[value]['index']+1
                key = 'tce_door_'+references[field]
            else:
                expected = value; key = 'tce_door_'+field
                if field in ('entity','use_entity'):require(64 <= value <= 1021,'invalid entity slot')
                if field in ('model','use_model'):require(1 <= value <= 255,'invalid brush model')
            require(props.get(key)==str(expected), 'WAY recipe mismatch: '+key)
        aim = recipe.get('aim')
        require(isinstance(aim,list) and len(aim)==3 and all(type(v) in (float,int) and math.isfinite(v) and abs(v)<=131072 for v in aim),
                'invalid native activation aim')
        try: binary_aim = [float(v) for v in props.get('tce_door_aim','').split()]
        except ValueError:binary_aim=[]
        require(len(binary_aim)==3 and all(math.isfinite(v) for v in binary_aim) and math.dist(aim,binary_aim)<.001,
                'WAY activation aim differs from native receipt')
        require(recipe['role']==3 or node.get('nav_flags',0)&128 and binary_nodes[node['index']]['flags']&128,
                'activation waypoint lost its DOOR flag')
        if recipe['role'] in (1,2):
            target = recipe['target_source_id']; edge = edges.get((identity,target))
            require(edge is not None and (node['index'],by_id[target]['index']) in binary_edges,
                    'directed activation/passage edge missing')
            if recipe['role']==2:
                require(edge.get('reason') in ('door_open_pose_native_walk_live_pending','external_native_validated_walk','manual_native_transition_live_pending'),
                        'door passage is not the declared native transition')
                require(recipe['entry_source_id']==identity,'entry reference is not this waypoint')
        if recipe['mode'] == 3:
            use_id, entry_id = recipe['use_source_id'], recipe['entry_source_id']
            use_recipe = by_id[use_id].get('door_recipe', {})
            entry_recipe = by_id[entry_id].get('door_recipe', {})
            require(use_recipe.get('role') == 1 and entry_recipe.get('role') == 2 and
                    use_recipe.get('target_source_id') == entry_id and
                    all(use_recipe.get(k) == entry_recipe.get(k) == recipe.get(k) for k in
                        ('entity', 'use_entity', 'mode', 'team', 'model', 'use_model',
                         'use_source_id', 'entry_source_id', 'aim')),
                    'staged USE/ENTRY identities do not match')
            require((use_id, entry_id) in edges and (entry_id, use_id) in edges,
                    'staged local alignment lacks bidirectional checked approach')
            require(all(a == use_id for a, b in edges if b == entry_id),
                    'staged ENTRY can be approached without safe USE')
        verified.append(dict(kind='door',source_id=identity,mode=recipe['mode'],role=recipe['role'],
                             entity=recipe['entity'],use_entity=recipe['use_entity'],live_verified=False))
    return verified


def audit_arrival_regions(report, by_id, binary_nodes):
    """Check arrival semantics and native evidence without treating radii as cells.

    A normal radius may include positions behind walls: these are unreachable
    arrivals, not occupied space. Large open-region radii require the stronger
    native evidence; ordinary fallback tolerances use the runtime next-leg gate.
    """
    region = report.get("arrival_regions")
    if region is None:
        return {"model": "legacy_report", "region_evidence_available": False}

    def require(condition, message):
        if not condition:
            raise ValueError("arrival-region audit: " + message)

    def number(value):
        return type(value) in (int, float) and math.isfinite(value)

    require(region.get("model") == "native_local_arrival_region_v1", "unknown model")
    maximum = region.get("max_ordinary_radius")
    require(region.get("default_radius") == 35 and region.get("min_ordinary_radius") == 10
            and maximum in (100, 300), "unsupported tolerance range")
    require(region.get("overlap_allowed") is True, "arrival tolerance is not an exclusion cell")
    require(region.get("runtime_gate") == "can_walk_to_next_ordinary_native_step_capsule",
            "ordinary runtime transition gate is not declared")
    require(type(region.get("traces")) is int and type(region.get("trace_budget")) is int
            and 0 <= region["traces"] <= region["trace_budget"], "invalid trace accounting")
    require(type(region.get("budget_exhausted")) is bool, "missing budget outcome")

    action_ids = set()
    action_ids.update(uid for uid,node in by_id.items() if node.get('door_recipe',{}).get('validated') is True
                      and node['door_recipe'].get('exported') is True)
    for route in report.get("traversal_routes", []):
        if not route.get("exported"):
            continue
        keys = (("low", "high", "inside_guide", "ascent_release", "ascent_alignment")
                if route.get("kind") == "ladder" else ("approach", "launch", "landing"))
        action_ids.update(route.get(key) for key in keys if route.get(key) in by_id)
        action_ids.update(uid for uid in route.get("guides", []) if uid in by_id)
    # Format5 validates authored transitions directly rather than replacing
    # them with a newly numbered automatic recipe. Every input link must pass.
    if report.get("candidate_mode") == "manual_draft":
        for uid,node in by_id.items():
            proof=node.get("native_input",{})
            if (node.get("editor_role",0) or node.get("nav_flags",0)&((1<<9)|(1<<27)|(1<<18)|(1<<8))) and "editor_role" in node:
                require(proof.get("floor_accepted") is True and proof.get("initial_rejected_incident_links")==0,
                        "authored action has rejected native input")
                action_ids.add(uid)
    exemptions = region.get("action_exemptions")
    require(isinstance(exemptions, list), "missing action exceptions")
    exempt_ids = [entry.get("id") for entry in exemptions if isinstance(entry, dict)]
    require(len(exempt_ids) == len(exemptions) == len(set(exempt_ids))
            and set(exempt_ids) <= action_ids, "unproven action exception")
    for entry in exemptions:
        node = by_id[entry["id"]]
        require(number(entry.get("radius")) and abs(entry["radius"] - node["radius"]) < .001
                and isinstance(entry.get("role"), str) and bool(entry["role"]),
                "action exception differs from exported node")

    ordinary = {uid for uid, node in by_id.items() if not node.get("nav_flags", 0) & ((1<<7)|(1<<9)|(1<<27)|(1<<18)|(1<<8))}
    neighbors = {uid: {} for uid in ordinary}
    for edge in report["edges"]:
        a, b = edge["from"], edge["to"]
        if not edge["accepted"] or a not in ordinary or b not in ordinary:
            continue
        for uid, other, direction in ((a, b, "outgoing"), (b, a, "incoming")):
            entry = neighbors[uid].setdefault(other, {"id": other, "incoming": False,
                                                     "outgoing": False, "crouch": False})
            entry[direction] = True
            entry["crouch"] |= bool(edge.get("crouch") or by_id[a].get("crouch") or by_id[b].get("crouch"))
    policies = Counter()
    radii = Counter()
    for uid, node in by_id.items():
        proof = node.get("arrival")
        require(isinstance(proof, dict), f"node {uid} lacks arrival policy")
        radius = binary_nodes[node["index"]]["radius"]
        require(number(proof.get("effective_radius")) and abs(proof["effective_radius"]-radius) < .001,
                f"node {uid} effective radius differs from WAY")
        policy = proof.get("policy")
        policies[policy] += 1
        if policy in ("preserved_action_recipe", "preserved_action_anchor"):
            require(uid in exempt_ids, f"node {uid} has no validated action recipe")
            continue
        require(uid in ordinary and uid not in exempt_ids, f"node {uid} has ordinary action policy")
        require(policy in ("validated_local_region", "restricted_runtime_guard"), f"node {uid} unknown policy")
        require(10 <= radius <= maximum, f"node {uid} ordinary radius outside 10..{maximum}")
        requested = proof.get("requested_radius")
        require(number(requested) and 10 <= requested <= maximum, f"node {uid} invalid requested radius")
        source = proof.get("requested_source")
        require(source in ("automatic_default", "manual_draft"), f"node {uid} missing request provenance")
        if source == "manual_draft":
            require(radius <= requested, f"node {uid} radius increased beyond manual request")
        else:
            require(requested == 35 and radius <= 100,
                    f"node {uid} automatic radius exceeds unchanged generation policy")
        require(type(proof.get("precision_required")) is bool, f"node {uid} missing precision decision")
        if radius < 35 and not (source == "manual_draft" and requested < 35):
            require(proof["precision_required"], f"node {uid} shrunk without corner/door evidence")
        require(isinstance(proof.get("limit_reason"), str) and proof["limit_reason"],
                f"node {uid} missing radius decision")
        if policy == "validated_local_region":
            require(proof.get("region_verified") is True and number(proof.get("region_radius"))
                    and proof["region_radius"] + .001 >= radius, f"node {uid} unverified large region")
            require(type(proof.get("hull_checks")) is int and proof["hull_checks"] >= 1
                    and type(proof.get("floor_samples")) is int and proof["floor_samples"] >= 9,
                    f"node {uid} missing hull/floor samples")
            declared = proof.get("transition_neighbors")
            require(isinstance(declared, list) and sorted(declared, key=lambda e: e["id"]) ==
                    sorted(neighbors[uid].values(), key=lambda e: e["id"]),
                    f"node {uid} final transition-neighbor evidence differs")
            directions = sum(int(e["incoming"])+int(e["outgoing"]) for e in neighbors[uid].values())
            require(type(proof.get("transition_checks")) is int and proof["transition_checks"] >= 8*directions,
                    f"node {uid} missing directed perimeter transitions")
        else:
            require(radius <= 35 and proof.get("region_verified") is False,
                    f"node {uid} ungated large radius or false full-region claim")
        attempts = proof.get("attempts")
        require(isinstance(attempts, list), f"node {uid} missing bounded attempts")
        for attempt in attempts:
            require(isinstance(attempt, dict) and number(attempt.get("radius"))
                    and 10 <= attempt["radius"] <= maximum and type(attempt.get("traces")) is int
                    and attempt["traces"] >= 0 and isinstance(attempt.get("reason"), str),
                    f"node {uid} invalid attempt evidence")
            if source == "automatic_default":
                require(attempt["radius"] <= 100, f"node {uid} unexpected enlarged automatic attempt")
        radii[str(radius)] += 1
    return dict(model=region["model"], region_evidence_available=True, policies=dict(policies),
                ordinary_radii=dict(radii), action_exemptions=len(exempt_ids),
                budget_exhausted=region["budget_exhausted"], traces=region["traces"])


def audit_spatial_samples(report):
    """Reconcile retained input samples, without equating quarantine to obstacles."""
    if report.get("generation_mode") != "spatial_surface_samples":
        return {"applicable": False}
    def require(condition, message):
        if not condition:
            raise ValueError("spatial sample audit: " + message)
    require(report.get("preserve_spatial_samples") is True, "preservation policy missing")
    coverage = report.get("native_input_coverage", {})
    require(coverage.get("full_map_verified") is False, "input samples cannot certify the full map")
    inputs = [node for node in report["nodes"] if "native_input" in node]
    ids = {node["id"] for node in inputs}
    recorded = coverage.get("input_ids", [])
    require(len(ids) == len(inputs) == len(recorded) and ids == set(recorded), "input ID ledger mismatch")
    floor = [n for n in inputs if n["native_input"].get("floor_accepted") is True]
    rejected = [n for n in inputs if n["native_input"].get("floor_accepted") is False]
    missing = [n for n in floor if n["native_input"].get("status") == "missing_spawn_route"]
    kept = [n for n in inputs if n["accepted"]]
    require(len(floor) + len(rejected) == len(inputs), "unknown floor result")
    require(all(n["native_input"].get("status") == "spawn_reachable" for n in kept), "exported sample lacks spawn route")
    require(all(not n["accepted"] for n in rejected + missing), "rejected sample was exported")
    require(len(kept) + len(missing) == len(floor), "valid spatial sample silently removed or contracted")
    for key, count in (("input_nodes", len(inputs)), ("native_floor_accepted", len(floor)),
                       ("native_floor_rejected", len(rejected)), ("exported_input_nodes", len(kept)),
                       ("spawn_reachable_input_nodes", len(kept)), ("connectivity_missing_input_nodes", len(missing)),
                       ("contracted_input_nodes", 0)):
        require(coverage.get(key) == count, key + " does not reconcile")
    grouped = [uid for group in coverage.get("missing_regions", []) for uid in group.get("node_ids", [])]
    require(len(grouped) == len(set(grouped)) and set(grouped) == {n["id"] for n in missing}, "missing-region ledger mismatch")
    reasons = Counter(n["native_input"].get("floor_reason") for n in rejected)
    require(dict(reasons) == {x["reason"]: x["count"] for x in coverage.get("geometry_rejections", [])}, "geometry rejection counts mismatch")
    accepted_links = coverage.get("initial_accepted_links")
    require(sum(n["native_input"].get("initial_incoming", 0) for n in inputs) == accepted_links and
            sum(n["native_input"].get("initial_outgoing", 0) for n in inputs) == accepted_links,
            "initial link ledger mismatch")
    return {"applicable": True, "input_nodes": len(inputs), "exported_input_nodes": len(kept),
            "geometry_rejected": len(rejected), "missing_spawn_route": len(missing),
            "silent_sample_removals": 0, "full_map_verified": False}


def audit_water_policy(report):
    policy=report.get("water_policy")
    if policy is None:return dict(model="legacy_unspecified",native_policy_evidence_available=False)
    model=policy.get("model")
    if model=="grounded_step_depth_wading_v1":
        if policy.get("dry_probe_above_feet")!=18 or policy.get("swimming_supported") is not False:
            raise ValueError("legacy native water policy differs from its18-unit contract")
        return dict(policy,legacy_conservative_policy=True)
    if model!="grounded_pm_middle_sample_wading_v2":raise ValueError("unknown native water policy")
    origin=report.get("bbox",{}).get("origin_above_feet")
    if origin not in (24,30):raise ValueError("native water policy has an unknown TC body profile")
    stand,duck=(41,28) if origin==30 else (33,22)
    if (policy.get("standing_middle_sample_height")!=stand or policy.get("crouching_middle_sample_height")!=duck or
        policy.get("foot_hazard_probe_height")!=0 or policy.get("pm_first_sample_height")!=1 or
        policy.get("integer_sample_truncation") is not True or policy.get("wet_middle_sample_rejected") is not True or
        policy.get("swimming_supported") is not False or policy.get("lava_slime_hurt_rejected") is not True):
        raise ValueError("native water policy differs from actual PM_SetWaterLevel posture samples")
    return dict(policy,native_policy_evidence_available=True)


def authored_tail_sections(report,report_path):
    source=report.get('source')
    if not isinstance(source,str) or not source:raise ValueError('metadata source is missing')
    candidate=Path(report_path).parent/Path(source).name
    if not candidate.is_file():raise ValueError('authored metadata source is unavailable: '+str(candidate))
    if not 1<=candidate.stat().st_size<=5*1024*1024:raise ValueError('authored metadata source exceeds native input bounds')
    tokens=candidate.read_text(encoding='ascii').split()
    if len(tokens)<5 or tokens[:1]!=['TCE_NAVMESH'] or tokens[1] not in ('5','6') or tokens[2]!=report['map']:
        raise ValueError('metadata source is not the current authored map candidate')
    node_count,edge_count=int(tokens[3]),int(tokens[4])
    if not 1<=node_count<=15872 or not 1<=edge_count<=253952:raise ValueError('invalid authored metadata source dimensions')
    start=5+node_count*(12 if tokens[1]=='5' else 25)+edge_count*2
    tail=tokens[start:];metadata=[];properties=[]
    if tail[:1]==['TCE_NODE_METADATA']:
        if len(tail)<3 or tail[1]!='1':raise ValueError('invalid metadata source tail version')
        count=int(tail[2]);end=3+count*6
        if not 0<=count<=node_count or len(tail)<end:raise ValueError('invalid metadata source tail size')
        metadata=[tail[3+i*6:9+i*6] for i in range(count)];tail=tail[end:]
    if tail[:1]==['TCE_NODE_PROPERTIES']:
        if len(tail)<3 or tail[1]!='1':raise ValueError('invalid custom property source tail version')
        count=int(tail[2]);end=3+count*3
        if not 0<=count<=4096 or len(tail)<end:raise ValueError('invalid custom property source tail size')
        properties=[tail[3+i*3:6+i*3] for i in range(count)];tail=tail[end:]
    if tail:raise ValueError('unexpected authored metadata/property tail data')
    return candidate,metadata,properties


def audit_waypoint_metadata(report,report_path,binary_nodes):
    """Bind the optional metadata tail to source, current native output and WAY."""
    contract=report.get('waypoint_metadata_contract')
    if contract is None:return dict(contract=None,source_bound=False)
    if contract!='TCE_NODE_METADATA_1':raise ValueError('unknown waypoint metadata contract')
    count=report.get('metadata_input_count')
    if type(count) is not int or not 0<=count<=len(report['nodes']):
        raise ValueError('invalid native metadata input count')
    by_id={node['id']:node for node in report['nodes']}
    if len(by_id)!=len(report['nodes']):raise ValueError('duplicate metadata source identity')
    supplied={node['id'] for node in report['nodes'] if node.get('metadata_supplied') is True}
    if len(supplied)!=count:raise ValueError('native metadata input count differs from source records')
    reserved,generated,output_uids=set(),set(),set()
    for node in report['nodes']:
        if type(node.get('metadata_supplied')) is not bool:
            raise ValueError('native metadata receipt is missing its supplied marker')
        uid=node.get('way_guid');import_uid=node.get('import_uid')
        if type(import_uid) is not int or not 0<=import_uid<=0x7fffffff or (import_uid and node['id'] not in supplied):
            raise ValueError('invalid native imported waypoint identity')
        if import_uid:reserved.add(import_uid)
        if not node['accepted']:
            if uid!=0:raise ValueError('rejected waypoint claims an output identity')
            continue
        if type(uid) is not int or not 1<=uid<=0x7fffffff or binary_nodes[node['index']]['uid']!=uid:
            raise ValueError('native/WAY waypoint identity mismatch')
        if uid in output_uids:raise ValueError('duplicate native output waypoint identity')
        output_uids.add(uid)
        if import_uid and uid!=import_uid:raise ValueError('native output changed imported waypoint identity')
        if not import_uid:generated.add(uid)
        binary=binary_nodes[node['index']]
        facing=node.get('facing')
        if (binary.get('name')!=node.get('name') or not isinstance(facing,list) or len(facing)!=3 or
            any(type(v) not in (float,int) or not math.isfinite(v) for v in facing) or
            any(abs(a-b)>.0002 for a,b in zip(binary['facing'],facing))):
            raise ValueError('native/WAY waypoint name or facing mismatch')
    if reserved&generated:raise ValueError('generated waypoint aliases a reserved imported identity')
    if not count:return dict(contract=contract,input_records=0,source_bound=False)
    candidate,metadata,properties=authored_tail_sections(report,report_path)
    try:
        if len(metadata)!=count:
            raise ValueError('metadata source tail count/shape differs from current native receipt')
        records={};source_uids=set()
        for fields in metadata:
            identity,uid=int(fields[0]),int(fields[1]);facing=[float(v) for v in fields[2:5]]
            name='' if fields[5]=='-' else bytes.fromhex(fields[5]).decode('utf8',errors='strict')
            if (identity in records or identity not in supplied or not 0<=uid<=0x7fffffff or
                (uid and uid in source_uids) or len(name.encode('utf8'))>255 or any(ord(c)<32 for c in name) or
                not all(math.isfinite(v) for v in facing) or sum(v*v for v in facing)>1.0201002):
                raise ValueError('invalid authored metadata source record')
            if uid:source_uids.add(uid)
            node=by_id[identity]
            if (node['import_uid']!=uid or node.get('name')!=name or len(node.get('facing',[]))!=3 or
                any(abs(a-b)>.0002 for a,b in zip(facing,node['facing']))):
                raise ValueError('authored/native name, facing or identity mismatch')
            records[identity]=dict(name=name,facing=facing,way_guid=uid)
        if set(records)!=supplied:raise ValueError('native metadata records differ from authored source IDs')
    except (IndexError,TypeError,UnicodeError) as error:
        raise ValueError('invalid authored metadata source') from error
    candidate_json=candidate.with_suffix('.json')
    if candidate_json.is_file():
        data=json.loads(candidate_json.read_text(encoding='utf8'))
        if data.get('candidate_sha256')!=digest(candidate) or data.get('waypoint_metadata_contract')!=contract:
            raise ValueError('authored metadata JSON/text certificate mismatch')
        draft={n['id']:n for n in data.get('nodes',[])}
        for identity,record in records.items():
            n=draft.get(identity,{})
            if (n.get('name','')!=record['name'] or n.get('way_guid',0)!=record['way_guid'] or
                len(n.get('facing',[0,0,0]))!=3 or any(abs(a-b)>.0002 for a,b in zip(n.get('facing',[0,0,0]),record['facing']))):
                raise ValueError('authored metadata JSON fields differ from text/native receipt')
    return dict(contract=contract,input_records=count,source_bound=True,source_sha256=digest(candidate))


NATIVE_TCE_PROPERTIES={'tce_node','tce_ascent_release','tce_ascent_alignment',
    'tce_walk_drop','tce_walk_drop_target','tce_stand_jump','tce_stand_jump_target',
    'tce_free_climb','tce_free_climb_target','tce_door_entity','tce_door_use_entity',
    'tce_door_target','tce_door_mode','tce_door_team','tce_door_role','tce_door_model',
    'tce_door_use_model','tce_door_use_nav','tce_door_entry','tce_door_aim'}


def audit_custom_properties(report,report_path,binary_nodes):
    contract=report.get('custom_property_contract')
    if contract is None:return dict(contract=None,source_bound=False)
    if contract!='TCE_NODE_PROPERTIES_1':raise ValueError('unknown custom property contract')
    count=report.get('custom_property_input_count')
    if type(count) is not int or not 0<=count<=4096:raise ValueError('invalid native custom property count')
    by_id={node['id']:node for node in report['nodes']};total=0;storage=0
    def bounded(values):
        if not isinstance(values,dict) or len(values)>32:raise ValueError('invalid native custom property map')
        size=0
        for key,value in values.items():
            if (not isinstance(key,str) or not isinstance(value,str) or not key or key.startswith('tce_') or
                len(key.encode('utf8'))>127 or len(value.encode('utf8'))>1023 or
                any(ord(c)<32 for c in key+value)):
                raise ValueError('invalid/reserved native custom property literal')
            size+=len(key.encode('utf8'))+len(value.encode('utf8'))+2
        return size
    for node in report['nodes']:
        values=node.get('custom_properties');storage+=bounded(values);total+=len(values)
        if node['accepted']:
            props=binary_nodes[node['index']].get('properties',{})
            if props.get('tce_node')!=str(node['id']) or any(k.startswith('tce_') and k not in NATIVE_TCE_PROPERTIES for k in props):
                raise ValueError('WAY source identity/reserved native property differs from current recipe')
            if {k:v for k,v in props.items() if not k.startswith('tce_')}!=values:
                raise ValueError('native/WAY custom property map mismatch')
    if total!=count or storage>1024*1024:raise ValueError('native custom property count/storage mismatch')
    if not count:return dict(contract=contract,input_pairs=0,source_bound=False)
    candidate,metadata,properties=authored_tail_sections(report,report_path)
    if len(properties)!=count:raise ValueError('custom property source/native count mismatch')
    source={};source_size=0
    for fields in properties:
        identity=int(fields[0]);key=bytes.fromhex(fields[1]).decode('utf8',errors='strict')
        value='' if fields[2]=='-' else bytes.fromhex(fields[2]).decode('utf8',errors='strict')
        if identity not in by_id:raise ValueError('unknown custom property source identity')
        values=source.setdefault(identity,{})
        if key in values:raise ValueError('duplicate custom property source key')
        values[key]=value
        source_size+=bounded({key:value})
        if len(values)>32 or source_size>1024*1024:raise ValueError('custom property source exceeds native bounds')
    for identity,node in by_id.items():
        if node['custom_properties']!=source.get(identity,{}):
            raise ValueError('authored/native custom properties differ or were inherited by a guide')
    candidate_json=candidate.with_suffix('.json')
    if candidate_json.is_file():
        data=json.loads(candidate_json.read_text(encoding='utf8'))
        if data.get('candidate_sha256')!=digest(candidate) or data.get('custom_property_contract')!=contract:
            raise ValueError('custom property JSON/text certificate mismatch')
        draft={n['id']:n for n in data.get('nodes',[])}
        for identity,values in source.items():
            supplied=draft.get(identity,{}).get('action_properties',{})
            if {k:v for k,v in supplied.items() if not k.startswith('tce_')}!=values:
                raise ValueError('custom property JSON fields differ from source/native receipt')
    return dict(contract=contract,input_pairs=count,source_bound=True,source_sha256=digest(candidate))


def inspect(report_path, write=False):
    report = json.loads(report_path.read_text(encoding="utf-8"))
    if report.get("generation_complete") is not True:
        raise ValueError("server generation incomplete; staged waypoint output must not be installed")
    map_name = valid_map(report["map"])
    way_path = report_path.with_suffix(".way")
    nodes, binary_edges = read_way(way_path)
    accepted = [node for node in report["nodes"] if node["accepted"]]
    by_id = {node["id"]: node for node in accepted}
    if len(by_id) != len(nodes) or {node["index"] for node in accepted} != set(range(len(nodes))):
        raise ValueError("report/output node count or output indices do not match")
    metadata_audit=audit_waypoint_metadata(report,report_path,nodes)
    property_audit=audit_custom_properties(report,report_path,nodes)
    if metadata_audit.get('source_bound') and property_audit.get('source_bound') and metadata_audit['source_sha256']!=property_audit['source_sha256']:
        raise ValueError('authored source changed between metadata/property audits')
    for node in accepted:
        binary = nodes[node["index"]]
        actual = binary["position"]
        if "radius" in node and abs(binary["radius"] - node["radius"]) > 0.0002:
            raise ValueError("report/output waypoint radius mismatch")
        if "nav_flags" in node and binary["flags"] != (node["nav_flags"] | (64 if node.get("crouch") else 0)):
            raise ValueError("report/output navigation flags mismatch")
        if "name" in node and binary["name"] != node["name"]:
            raise ValueError("report/output waypoint name mismatch")
        if "facing" in node and any(abs(a-b)>0.0002 for a,b in zip(binary["facing"],node["facing"])):
            raise ValueError("report/output facing mismatch")
        if len(node["position"]) != 3 or not all(map(math.isfinite, node["position"])):
            raise ValueError("invalid report node position")
        if any(abs(a - b) > 0.05 for a, b in zip(actual, node["position"])):
            raise ValueError("report/output node position mismatch")
    report_edges = set()
    for edge in report["edges"]:
        if edge["accepted"]:
            report_edges.add((by_id[edge["from"]]["index"], by_id[edge["to"]]["index"]))
    if report_edges != binary_edges:
        raise ValueError("report/output directed links do not match")
    # Final graph witnesses survive later junction splitting without allowing
    # action shortcuts or increasing the original distance/stance limits.
    walk_graph = final_walk_graph(report, by_id, nodes, binary_edges)
    removed_checked, contracted_spokes = audit_removed_walks(report, by_id, nodes, walk_graph)
    junction_audit = audit_native_junctions(report, by_id, nodes, binary_edges)
    groups = components(len(nodes), binary_edges)
    undirected = binary_edges | {(b, a) for a, b in binary_edges}
    weak_groups = components(len(nodes), undirected)
    incoming = Counter(b for a, b in binary_edges)
    outgoing = Counter(a for a, b in binary_edges)
    rejected_nodes = Counter(node.get("reason", "unspecified") for node in report["nodes"] if not node["accepted"])
    rejected_edges = Counter(edge.get("reason", "unspecified") for edge in report["edges"] if not edge["accepted"])
    by_index = {node["index"]: node for node in accepted}
    component_of = {index: component for component, group in enumerate(groups) for index in group}
    unresolved = []
    for index in sorted(i for group in groups[1:] for i in group):
        node = by_index[index]
        reasons = Counter(edge.get("reason", "unspecified") for edge in report["edges"]
                          if not edge["accepted"] and node["id"] in (edge["from"], edge["to"]))
        unresolved.append(dict(source_id=node["id"], output_index=index,
                               component=component_of[index], position=node["position"],
                               incoming=incoming[index], outgoing=outgoing[index],
                               rejected_connection_reasons=dict(reasons),
                               reason="outside_main_strong_component"))
    allowed_overlaps, traversal_routes = intentional_ladder_overlaps(report, by_id, nodes, binary_edges)
    traversal_routes.extend(audit_standing_jumps(report, by_id, nodes, binary_edges))
    traversal_routes.extend(audit_free_climbs(report, by_id, nodes, binary_edges))
    traversal_routes.extend(audit_walk_drops(report, by_id, nodes, binary_edges))
    traversal_routes.extend(audit_doors(report, by_id, nodes, binary_edges))
    # A real player spawn may intentionally drop into the shared map without
    # an impossible return jump. Use the same source-bound classifier as the
    # Studio, after the exact WAY/action recipes above have passed.
    from navstudio.completion import navigation_readiness,spawn_bindings
    navigation=navigation_readiness(report,spawn_bindings(report,report_path))
    entry_only=set(navigation.get('entry_only_source_ids',[]))if navigation.get('ready')else set()
    unresolved=[row for row in unresolved if row['source_id']not in entry_only]
    arrival_audit = audit_arrival_regions(report, by_id, nodes)
    spatial_audit = audit_spatial_samples(report)
    overlaps, intentional_overlaps = [], []
    for i, first in enumerate(nodes):
        for j in range(i + 1, len(nodes)):
            second = nodes[j]
            distance = math.dist(first["position"], second["position"])
            if distance + 0.01 < first["radius"] + second["radius"]:
                pair = dict(first=by_index[i]["id"], second=by_index[j]["id"], distance=round(distance, 3))
                exception = allowed_overlaps.get(frozenset((pair["first"], pair["second"])))
                if exception:
                    intentional_overlaps.append(dict(pair, **exception))
                else:
                    overlaps.append(pair)
    summary = dict(map=map_name, way=str(way_path.resolve()), sha256=digest(way_path),
                   binary_report_match=True, overlapping_pairs=overlaps,
                   waypoint_metadata=metadata_audit,
                   custom_properties=property_audit,
                   radius_semantics="arrival_tolerance_not_reserved_space",
                   overlaps_are_diagnostic=True,
                   arrival_regions=arrival_audit,
                   water_policy=audit_water_policy(report),
                   spatial_samples=spatial_audit,
                   native_input_coverage=report.get("native_input_coverage", {}),
                   simplification=report.get("simplification", {}), doorway_search=report.get("doorway_search", {}), nodes=len(nodes), directed_links=len(binary_edges),
                   intentional_traversal_overlaps=intentional_overlaps, validated_traversal_routes=traversal_routes,
                   sparsification=dict(report.get("sparsification", {}), verified_final_witnesses=removed_checked["chords"]),
                   seed_spoke_pruning=dict(verified_final_witnesses=removed_checked["seed_spokes"],
                                           max_stretch=1.5, contracted_intermediate_pairs=contracted_spokes,
                                           contracted_pairs_final_witness_verified=False),
                   native_junctions=dict(report.get("native_junctions", {}), **junction_audit),
                   quarantined_unreachable_nodes=report.get("quarantined_unreachable_nodes", 0),
                   rejected_nodes=dict(rejected_nodes), rejected_links=dict(rejected_edges),
                   added_links=sum(bool(edge.get("added")) for edge in report["edges"] if edge["accepted"]),
                   crouch_nodes=sum(bool(node.get("crouch")) for node in accepted),
                   strong_component_sizes=[len(group) for group in groups],
                   weak_component_sizes=[len(group) for group in weak_groups],
                   largest_mutually_reachable_fraction=len(groups[0]) / len(nodes),
                   no_outgoing_nodes=[i for i in range(len(nodes)) if not outgoing[i]],
                   no_incoming_nodes=[i for i in range(len(nodes)) if not incoming[i]],
                   no_outgoing_source_ids=[by_index[i]["id"] for i in range(len(nodes)) if not outgoing[i]],
                   no_incoming_source_ids=[by_index[i]["id"] for i in range(len(nodes)) if not incoming[i]],
                   component_source_ids=[[by_index[i]["id"] for i in sorted(group)] for group in groups],
                   unresolved_nodes=unresolved,
                   excluded_source_nodes=[dict(source_id=node["id"], reason=node.get("reason", "unspecified"),
                                               position=node.get("raw_position", node.get("position")))
                                          for node in report["nodes"] if not node["accepted"]],
                   pruning_policy="No nodes are silently removed by the audit; all server exclusions are listed.",
                   limitations=report.get("limitations", ""),
                   source=report.get("source"),
                   map_geometry=report.get("map_geometry", {}),
                   traversal=report.get("traversal", {}),
                   playability=report.get("playability", {}),
                   camp_candidates=report.get("camp_candidates", 0),
                   coverage=report.get("coverage", {}),
                   detour_simplification=report.get("detour_simplification", {}),
                   full_map_verified=False,
                   live_bot_traversal_verified=False,
                   checked_at=datetime.now(timezone.utc).isoformat())
    # Connectivity failures are reported rather than silently pruning islands
    # that could contain gameplay objectives. Installation requires an override.
    summary["connectivity_pass"] = len(groups) == 1 and len(nodes) > 1
    summary['directed_entry_connectivity_pass']=navigation.get('directed_entry_connectivity_pass',False)
    summary['navigation_ready']=navigation['ready']
    summary['entry_only_source_ids']=sorted(entry_only)
    summary['spawn_entry_connectivity']=navigation
    coverage = summary["coverage"]
    summary["coverage_search_complete"] = bool(coverage) and coverage.get("pending_nodes") == 0 and coverage.get("stop_reason") == "frontier_exhausted"
    summary["spawn_connected_search_complete"] = bool(coverage) and coverage.get("spawn_connected_pending_nodes") == 0 and coverage.get("stop_reason") in ("frontier_exhausted", "spawn_frontier_exhausted_islands_quarantined")
    # Omni-bot radii are path-arrival tolerances, not disjoint occupied cells.
    # Overlapping radii are normal in the shipped standard-map networks. The
    # actual directed hull/floor paths and special traversal recipes are checked
    # above; projected overlap alone neither invalidates nor validates a route.
    summary["review_required"] = bool(summary["quarantined_unreachable_nodes"]) or not summary['navigation_ready'] or (bool(coverage) and not summary["coverage_search_complete"]) or (bool(summary["map_geometry"]) and not summary["map_geometry"].get("scan_finished"))
    if write:
        write_json(writable_destination(report_path.with_name(report_path.stem + "_audit.json")), summary)
    return summary


def generate(args):
    """Stage original seed, request server validation and produce an audit."""
    if getattr(args, "wps", None):
        staged = prepare(args.wps, args.mod_directory)
        if staged["map"] != args.map:
            raise ValueError("optional WPS does not match --map")
    result = run(args)
    result["standard_navigation_replaced"] = False
    result["audit_path"] = str((args.mod_directory / "navgen" / (args.map + "_auto_audit.json")).resolve())
    result["next_step"] = (
        "Connectivity passed. Review objective/special-movement requirements, then use the explicit install command."
        if not result["review_required"] else
        "Navigation needs review. Check unresolved_nodes and coverage stop_reason/pending_nodes; standard navigation is unchanged.")
    return result


def rcon(host, port, password, command, timeout=2):
    global _last_rcon_send
    if any(c in password + command for c in '\n\r\0"'):
        raise ValueError("invalid RCON password or command")
    packet = b"\xff" * 4 + ('rcon "' + password + '" ' + command + "\n").encode("utf-8")
    chunks = []
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
        sock.connect((host, port))
        sock.settimeout(timeout)
        # ET rate-limits valid RCON packets too. Fast localhost replies must not
        # cause the next command to be silently dropped (engine threshold500ms).
        delay = 0.65 - (time.monotonic() - _last_rcon_send)
        if delay > 0:
            time.sleep(delay)
        sock.send(packet)
        _last_rcon_send = time.monotonic()
        while True:
            try:
                data = sock.recv(65535)
            except socket.timeout:
                break
            if data.startswith(b"\xff" * 4 + b"print\n"):
                chunks.append(data[10:].decode("utf-8", errors="replace"))
            sock.settimeout(0.3)
    response = "".join(chunks)
    if "Bad rconpassword" in response or "No rconpassword" in response:
        raise ValueError("server rejected RCON authentication")
    return response


def run(args):
    mod = writable_destination(args.mod_directory)
    name = valid_map(args.map)
    staged = mod / "botroutes" / (name + ".wps")
    source_info = wps_info(staged) if getattr(args, "wps", None) else None
    password = os.environ.get(args.password_env)
    if not password:
        raise ValueError("set the RCON password in environment variable " + args.password_env)
    status = rcon(args.host, args.port, password, "status")
    loaded = re.search(r"(?im)^map\s*:\s*(\S+)", status)
    if not loaded or loaded.group(1).lower() != name.lower():
        raise ValueError("RCON status does not confirm requested map " + name + "; response: " + status)
    report_path = mod / "navgen" / (name + "_auto.json")
    way_path = report_path.with_suffix(".way")
    # Clear stale outputs by preserving them, so failed commands cannot be
    # mistaken for a successful run using yesterday's result.
    for path in (report_path, way_path):
        if path.exists():
            backup(path)
            path.unlink()
    response = rcon(args.host, args.port, password,
                    ("tce_navgen botroutes/" + name + ".wps") if source_info else "tce_navgen", timeout=min(args.timeout, 5))
    if re.search(r"navgen:\s*(?:FAILED|ERROR|invalid)|usage:\s*tce_navgen|unknown (?:server )?command", response, re.IGNORECASE):
        raise ValueError("server rejected/failed generation: " + response.strip())
    end = time.monotonic() + args.timeout
    last_error = None
    while time.monotonic() < end:
        if report_path.exists():
            try:
                partial_report = json.loads(report_path.read_text(encoding="utf-8"))
            except (OSError, ValueError):
                partial_report = None
            if partial_report is not None and partial_report.get("generation_complete") is False:
                raise ValueError("server reports incomplete generation; inspect " + str(report_path))
        if report_path.exists() and way_path.exists():
            try:
                summary = inspect(report_path, write=True)
                if source_info and source_info["sha256"] != digest(staged):
                    raise ValueError("staged source changed during generation")
                summary["rcon_response"] = response
                summary["source_sha256"] = source_info["sha256"] if source_info else None
                summary["input_mode"] = "wps" if source_info else "map_geometry"
                write_json(report_path.with_name(name + "_run.json"), summary)
                return summary
            except (ValueError, KeyError, struct.error) as error:
                last_error = str(error)
        time.sleep(0.25)
    raise TimeoutError("no complete validated generator output; server response: " + response +
                       ("; validation: " + last_error if last_error else ""))


def backup(path):
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    destination = path.parent / "navgen-backups" / (path.name + "." + stamp + ".bak")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(path, destination)
    if digest(destination) != digest(path):
        raise OSError("backup hash mismatch")
    return destination


def test_component_bytes(source):
    """Keep validated edges inside the largest SCC; never invent a connection."""
    nodes, edges = read_way(source)
    keep = sorted(components(len(nodes), edges)[0])
    if len(keep) < 2:
        raise ValueError("Kein zusammenhaengendes Testnetz vorhanden; zuerst neu erzeugen und pruefen.")
    remap = {old: new for new, old in enumerate(keep)}
    uids = {nodes[i]["uid"] for i in keep}
    def packed(value):
        result = bytearray()
        while value >= 128:
            result.append(value & 127); value >>= 7
        result.append(value | 128)
        return result
    def string(value):
        value = value.encode("utf-8")
        return packed(len(value)) + value
    header = list(HEADER.unpack(source.read_bytes()[:HEADER.size])); header[1] = len(keep)
    data = bytearray(HEADER.pack(*header))
    for old in keep:
        node = nodes[old]
        # Preserve UID-based traversal recipes. Unknown dangling targets must
        # block installation rather than becoming corrupt navigation.
        for key, value in node["properties"].items():
            if key.endswith("_target") and (not value.isdigit() or int(value) not in uids):
                raise ValueError("Traversal target excluded from test component: " + key)
        data.append(1 | 2 | 4 | 16 | 32 | 64)
        data += packed(node["uid"]) + struct.pack("<3f", *node["position"])
        data += struct.pack("<3f", *node["facing"]) + packed(node["flags"])
        data += struct.pack("<f", node["radius"]) + string(node["name"])
        data += packed(len(node["properties"]))
        for key, value in node["properties"].items(): data += string(key) + string(value)
        targets = sorted(remap[b] for a, b in edges if a == old and b in remap)
        data += packed(len(targets))
        for target in targets:
            delta = target - remap[old]
            data += packed((delta << 1) if delta >= 0 else ((~delta << 1) | 1))
    return bytes(data), [nodes[i]["properties"].get("tce_node", str(nodes[i]["uid"]))
                         for i in range(len(nodes)) if i not in remap]


def install(args):
    if not args.install:
        raise ValueError("installation requires explicit --install")
    summary = inspect(args.report)
    subset = getattr(args, "test_main_component", False) and not summary['navigation_ready']
    if not summary['navigation_ready'] and not args.allow_disconnected and not subset:
        raise ValueError("disconnected directed graph; review audit, then use --allow-disconnected only for intentional islands")
    directory = writable_destination(args.nav_directory)
    directory.mkdir(parents=True, exist_ok=True)
    source = args.report.with_suffix(".way")
    destination = directory / (summary["map"] + ".way")
    previous = str(backup(destination)) if destination.exists() else None
    temporary = destination.with_suffix(".way.tmp")
    if subset:
        data, excluded = test_component_bytes(source)
        temporary.write_bytes(data)
        kept, links = read_way(temporary)
        if len(components(len(kept), links)) != 1:
            raise ValueError("Test component roundtrip failed")
        summary.update(test_subset=True, excluded_test_source_ids=excluded,
                       installed_nodes=len(kept), installed_directed_links=len(links),
                       installed_sha256=digest(temporary))
    else:
        shutil.copy2(source, temporary)
    if not subset and digest(temporary) != summary["sha256"]:
        raise OSError("installation hash mismatch")
    temporary.replace(destination)
    summary.update(installed=str(destination), backup=previous,
                   note="Restart/reload map navigation before using this graph. Objective and traversal checks remain separate.")
    write_json(directory / (summary["map"] + "_navgen_install.json"), summary)
    return summary


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    commands = parser.add_subparsers(dest="command", required=True)
    generate_parser = commands.add_parser("generate", help="generate from loaded map geometry without WPS, collision-check and audit; never auto-install")
    generate_parser.add_argument("--map", required=True)
    generate_parser.add_argument("--wps", type=Path, help="optional legacy seeds; default is map geometry only")
    generate_parser.add_argument("mod_directory", type=Path, help="loaded server fs_homepath/tce2, accessible locally")
    generate_parser.add_argument("--host", default="127.0.0.1")
    generate_parser.add_argument("--port", type=int, default=27960)
    generate_parser.add_argument("--password-env", default="TCE_RCON_PASSWORD")
    generate_parser.add_argument("--timeout", type=float, default=60)
    prepare_parser = commands.add_parser("prepare", help="validate and stage raw WPS without modifying original")
    prepare_parser.add_argument("source", type=Path)
    prepare_parser.add_argument("mod_directory", type=Path, help="server fs_homepath/tce2")
    run_parser = commands.add_parser("run", help="generate and audit navigation from the loaded map geometry")
    run_parser.add_argument("mod_directory", type=Path)
    run_parser.add_argument("--map", required=True)
    run_parser.add_argument("--host", default="127.0.0.1")
    run_parser.add_argument("--port", type=int, default=27960)
    run_parser.add_argument("--password-env", default="TCE_RCON_PASSWORD")
    run_parser.add_argument("--timeout", type=float, default=60)
    inspect_parser = commands.add_parser("inspect", help="cross-check Omni v9 bytes with trace report and directed reachability")
    inspect_parser.add_argument("report", type=Path)
    inspect_parser.add_argument("--write-audit", action="store_true")
    install_parser = commands.add_parser("install", help="explicitly install an audited graph, retaining any previous file")
    install_parser.add_argument("report", type=Path)
    install_parser.add_argument("nav_directory", type=Path)
    install_parser.add_argument("--install", action="store_true")
    install_parser.add_argument("--allow-disconnected", action="store_true")
    install_parser.add_argument("--test-main-component", action="store_true", help="Install only the mutually reachable main network for local testing; retain original candidate")
    args = parser.parse_args()
    try:
        if args.command == "generate":
            result = generate(args)
        elif args.command == "prepare":
            result = prepare(args.source, args.mod_directory)
        elif args.command == "run":
            result = run(args)
        elif args.command == "inspect":
            result = inspect(args.report, args.write_audit)
        else:
            result = install(args)
        print(json.dumps(result, indent=2))
        if args.command == "generate" and result["review_required"]:
            return 2
        return 0
    except (OSError, ValueError, KeyError, struct.error) as error:
        print("tce_navgen: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
