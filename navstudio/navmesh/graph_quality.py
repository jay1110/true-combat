"""Read-only geometric/topological diagnostics for native or Recast graphs.

This never generates, installs, or approves WAY files. Crossings and density
are review hints; native reachability and live bot traversal remain separate.
Example: python graph_quality.py native.json --compare previous.json --out quality.json
"""

import argparse
from collections import Counter, defaultdict
import hashlib
import importlib.util
import json
import math
from pathlib import Path


def load_graph(path):
    raw = Path(path).read_bytes()
    binary = not raw.lstrip().startswith(b"{")
    if binary:
        # Reuse the strict existing v9 reader, without invoking its CLI or
        # any server operation. Backups retain their .way.<stamp>.bak name.
        module_path = Path(__file__).resolve().parents[1] / "workers" / "tce_navgen.py"
        spec = importlib.util.spec_from_file_location("tce_quality_way_reader", module_path)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        way_nodes, way_edges = module.read_way(Path(path))
        ids = [int(n["properties"].get("tce_node", n["uid"])) for n in way_nodes]
        data = {"map": Path(path).name.split(".way", 1)[0],
                "nodes": [{"id": ids[i], "position": list(n["position"]), "accepted": True,
                           "nav_flags": n["flags"], "crouch": bool(n["flags"] & 64),
                           "radius": n["radius"], "name": n["name"], "uid": n["uid"]}
                          for i, n in enumerate(way_nodes)],
                "edges": [{"from": ids[a], "to": ids[b], "accepted": True,
                           "reason": "binary_way_link"} for a, b in sorted(way_edges)]}
    else:
        data = json.loads(raw)
    native = bool(data.get("nodes") and "accepted" in data["nodes"][0])
    nodes, all_nodes = {}, {}
    for ordinal, record in enumerate(data["nodes"]):
        identity = record.get("id", ordinal)
        if identity in all_nodes:
            raise ValueError(f"duplicate node ID {identity}")
        point = record.get("position", record.get("point"))
        if not isinstance(point, list) or len(point) != 3 or not all(math.isfinite(v) for v in point):
            raise ValueError(f"invalid coordinate at {identity}")
        node = {**record, "id": identity, "point": point}
        all_nodes[identity] = node
        if record.get("accepted", True):
            nodes[identity] = node
    directed, reasons, duplicates = {}, Counter(), 0
    for record in data["edges"]:
        if isinstance(record, dict):
            if not record.get("accepted", True):
                continue
            pairs = [(record["from"], record["to"])]
            reason = record.get("reason", "unspecified")
        else:
            a, b = record
            pairs, reason = [(a, b), (b, a)], "recast_candidate"
        for a, b in pairs:
            if a == b or a not in nodes or b not in nodes:
                raise ValueError(f"invalid accepted edge {a}->{b}")
            reasons[reason] += 1
            if (a, b) in directed:
                duplicates += 1
            directed[a, b] = reason
    return data, nodes, all_nodes, directed, {
        "path": str(Path(path).resolve()), "sha256": hashlib.sha256(raw).hexdigest(),
        "format": "omni_way_v9" if binary else "native_acceptance_report" if native else "recast_candidate",
        "map_inferred_from_filename": binary,
        "accepted_edge_reasons": dict(reasons), "duplicate_directed_links": duplicates}


def component_sizes(nodes, directed, strong=False):
    adjacency, reverse = defaultdict(set), defaultdict(set)
    for a, b in directed:
        adjacency[a].add(b)
        reverse[b].add(a)
        if not strong:
            adjacency[b].add(a)
    visited, order = set(), []
    for root in nodes:
        if root in visited:
            continue
        visited.add(root)
        stack = [(root, iter(adjacency[root]))]
        while stack:
            node, targets = stack[-1]
            target = next(targets, None)
            if target is None:
                stack.pop()
                order.append(node)
            elif target not in visited:
                visited.add(target)
                stack.append((target, iter(adjacency[target])))
    graph = reverse if strong else adjacency
    visited, groups = set(), []
    for root in reversed(order):
        if root in visited:
            continue
        visited.add(root)
        stack, group = [root], []
        while stack:
            node = stack.pop()
            group.append(node)
            for target in graph[node] - visited:
                visited.add(target)
                stack.append(target)
        groups.append(group)
    return sorted(groups, key=lambda group: (-len(group), min(group)))


def crossing(a, b, c, d, height):
    ux, uy, vx, vy = b[0]-a[0], b[1]-a[1], d[0]-c[0], d[1]-c[1]
    denominator = ux*vy-uy*vx
    if abs(denominator) < 1e-6:
        return None
    wx, wy = c[0]-a[0], c[1]-a[1]
    t, s = (wx*vy-wy*vx)/denominator, (wx*uy-wy*ux)/denominator
    if not (1e-6 < t < 1-1e-6 and 1e-6 < s < 1-1e-6):
        return None
    za, zb = a[2]+t*(b[2]-a[2]), c[2]+s*(d[2]-c[2])
    if abs(za-zb) > height:
        return None
    return {"position": [a[0]+t*ux, a[1]+t*uy, (za+zb)*.5], "height_difference": abs(za-zb)}


def geometry(nodes, pairs, height, examples):
    lengths, adjacency, grid = [], defaultdict(set), defaultdict(list)
    pairs = sorted(pairs)
    for index, (a, b) in enumerate(pairs):
        pa, pb = nodes[a]["point"], nodes[b]["point"]
        lengths.append(math.dist(pa, pb))
        adjacency[a].add(b)
        adjacency[b].add(a)
        for x in range(math.floor(min(pa[0], pb[0])/128), math.floor(max(pa[0], pb[0])/128)+1):
            for y in range(math.floor(min(pa[1], pb[1])/128), math.floor(max(pa[1], pb[1])/128)+1):
                grid[x, y].append(index)
    checked, crossings, sample = set(), 0, []
    for candidates in grid.values():
        for at, left in enumerate(candidates):
            for right in candidates[at+1:]:
                identity = (min(left, right), max(left, right))
                if identity in checked:
                    continue
                checked.add(identity)
                a, b = pairs[left]
                c, d = pairs[right]
                if {a, b} & {c, d}:
                    continue
                result = crossing(nodes[a]["point"], nodes[b]["point"],
                                  nodes[c]["point"], nodes[d]["point"], height)
                if result is not None:
                    crossings += 1
                    if len(sample) < examples:
                        sample.append({"edges": [[a, b], [c, d]], **result})
    fans, fan_examples = 0, []
    for center, targets in adjacency.items():
        vectors, p = [], nodes[center]["point"]
        for target in sorted(targets):
            q = nodes[target]["point"]
            dx, dy = q[0]-p[0], q[1]-p[1]
            length = math.hypot(dx, dy)
            if length > 32 and abs(q[2]-p[2]) <= 18:
                vectors.append((target, dx/length, dy/length))
        for at, a in enumerate(vectors):
            for b in vectors[at+1:]:
                if a[1]*b[1]+a[2]*b[2] > math.cos(math.radians(20)):
                    fans += 1
                    if len(fan_examples) < examples:
                        fan_examples.append({"center": center, "targets": [a[0], b[0]], "position": p})
    degrees = Counter(len(targets) for targets in adjacency.values())
    return {"linked_nodes": len(adjacency), "undirected_pairs": len(pairs),
            "same_height_crossings": crossings, "crossing_examples": sample,
            "parallel_fans": fans, "fan_examples": fan_examples,
            "long_links": {str(n): sum(length > n for length in lengths) for n in (128, 192, 256, 320, 384)},
            "total_line_length": sum(lengths), "maximum_link_length": max(lengths, default=0),
            "maximum_degree": max(degrees, default=0), "degree_histogram": dict(sorted(degrees.items()))}


def diagnose(path, height=16, examples=32):
    data, nodes, all_nodes, directed, source = load_graph(path)
    pairs = {tuple(sorted(edge)) for edge in directed}
    ordinary = {edge for edge in pairs if all(not (nodes[n].get("nav_flags", 0) & ~64) for n in edge)}
    weak, strong = component_sizes(nodes, directed), component_sizes(nodes, directed, True)
    rejected_nodes = Counter(n.get("reason", "unspecified") for n in all_nodes.values() if not n.get("accepted", True))
    rejected_edges = Counter(e.get("reason", "unspecified") for e in data["edges"]
                             if isinstance(e, dict) and not e.get("accepted", True))
    outgoing, incoming = Counter(a for a, b in directed), Counter(b for a, b in directed)
    traces, budget = data.get("traces"), data.get("trace_budget")
    result = {"source": source, "map": data.get("map"), "input_nodes": len(all_nodes),
              "accepted_nodes": len(nodes), "directed_links": len(directed),
              "weak_component_sizes": [len(group) for group in weak],
              "strong_component_sizes": [len(group) for group in strong],
              "isolated_nodes": [n for n in nodes if not outgoing[n] and not incoming[n]],
              "no_outgoing_nodes": [n for n in nodes if not outgoing[n]],
              "all_geometry": geometry(nodes, pairs, height, examples),
              "ordinary_geometry": geometry(nodes, ordinary, height, examples),
              "quarantined_nodes": [n["id"] for n in all_nodes.values() if "quarantin" in n.get("reason", "")],
              "rejected_node_reasons": dict(rejected_nodes), "rejected_edge_reasons": dict(rejected_edges),
              "trace_usage": {"used": traces, "budget": budget,
                              "remaining": budget-traces if budget is not None and traces is not None else None},
              "generation_complete": data.get("generation_complete"),
              "coverage": data.get("coverage"), "local_shortcut_stop": data.get("local_shortcuts", {}).get("stop_reason"),
              "component_examples": [{"size": len(group), "node_ids": group[:examples]} for group in strong[1:]],
              "limitations": "Geometric diagnosis only; source acceptance is not live traversal or full-map coverage proof."}
    return result, all_nodes


def compare_sources(old, new, raw_coordinates=True):
    def key(n):
        point = n.get("raw_position", n["point"]) if raw_coordinates else n["point"]
        return tuple(round(value, 3) for value in point)
    current = defaultdict(list)
    for node in new.values():
        current[key(node)].append(node)
    quarantined, rejected, missing = [], [], 0
    for node in old.values():
        if not node.get("accepted", True):
            continue
        matches = current[key(node)]
        if not matches:
            missing += 1
        elif not any(n.get("accepted", True) or "contracted" in n.get("reason", "") for n in matches):
            item = {"previous_id": node["id"], "position": node["point"],
                    "current": [{"id": n["id"], "reason": n.get("reason")} for n in matches]}
            (quarantined if any("quarantin" in n.get("reason", "") for n in matches) else rejected).append(item)
    return {"previous_accepted_now_quarantined": quarantined, "previous_accepted_now_rejected": rejected,
            "previous_accepted_coordinate_absent_from_new_candidates": missing,
            "matching": ("raw source" if raw_coordinates else "player-origin") +
            " coordinates rounded to0.001; absent coordinates are changed sampling, not proven coverage loss"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--compare", type=Path)
    parser.add_argument("--out", required=True, type=Path)
    parser.add_argument("--height-tolerance", type=float, default=16)
    parser.add_argument("--examples", type=int, default=32)
    args = parser.parse_args()
    if args.examples < 0 or not math.isfinite(args.height_tolerance) or args.height_tolerance < 0:
        parser.error("invalid diagnostic limits")
    if args.out.resolve() in {args.input.resolve(), args.compare.resolve() if args.compare else None}:
        parser.error("output would overwrite an input")
    current, nodes = diagnose(args.input, args.height_tolerance, args.examples)
    report = {"schema": 1, "height_tolerance": args.height_tolerance, "current": current}
    if args.compare:
        before, old_nodes = diagnose(args.compare, args.height_tolerance, args.examples)
        if before["map"] != current["map"]:
            parser.error("comparison maps differ")
        report.update({"previous": before, "coverage_changes": compare_sources(old_nodes, nodes,
                       before["source"]["format"] != "omni_way_v9" and current["source"]["format"] != "omni_way_v9")})
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(json.dumps({"output": str(args.out), "nodes": current["accepted_nodes"],
                      "directed_links": current["directed_links"],
                      "ordinary_crossings": current["ordinary_geometry"]["same_height_crossings"],
                      "strong_components": len(current["strong_component_sizes"]),
                      "quarantined": len(current["quarantined_nodes"])}))


if __name__ == "__main__":
    main()
