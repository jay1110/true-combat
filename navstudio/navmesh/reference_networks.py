"""Read-only statistics for hand-authored Omni-bot WAY v9 reference networks.

Arrival-radius overlap is a measurement, never an error or installation veto.
Positions in different rooms can be close through a wall; these statistics do
not prove clearance, route safety, bot traffic capacity, or full-map coverage.
No engine, RCON, mutable WAY operation, or external dependency is used.
"""

import argparse
from collections import Counter, defaultdict
import hashlib
import importlib.util
import json
import math
from pathlib import Path


MAPS = ("oasis", "battery", "goldrush", "railgun", "radar", "fueldump")


def way_reader():
    source = Path(__file__).resolve().parents[1] / "workers" / "tce_navgen.py"
    spec = importlib.util.spec_from_file_location("reference_way_reader", source)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.read_way


def distribution(values):
    ordered = sorted(values)
    if not ordered:
        return {"count": 0}

    def quantile(q):
        at = (len(ordered)-1)*q
        lo, hi = math.floor(at), math.ceil(at)
        return round(ordered[lo] + (ordered[hi]-ordered[lo])*(at-lo), 4)

    return {"count": len(ordered), "minimum": round(ordered[0], 4),
            "p10": quantile(.1), "p25": quantile(.25), "median": quantile(.5),
            "p75": quantile(.75), "p90": quantile(.9),
            "maximum": round(ordered[-1], 4), "mean": round(sum(ordered)/len(ordered), 4)}


def cohort(nodes, edges, members, height_tolerance=18.0, example_limit=12):
    members = sorted(members)
    member_set = set(members)
    directed = {(a, b) for a, b in edges if a in member_set and b in member_set}
    pairs = {tuple(sorted(e)) for e in directed}
    adjacency, outgoing = defaultdict(set), Counter(a for a, b in directed)
    for a, b in pairs:
        adjacency[a].add(b)
        adjacency[b].add(a)
    radii = [nodes[i]["radius"] for i in members]
    nearest, nearest_floor = {}, {}
    overlaps_3d, overlaps_xy, linked_overlaps, involved = 0, 0, 0, set()
    examples, coincident = [], []
    for offset, a in enumerate(members):
        p = nodes[a]["position"]
        for b in members[offset+1:]:
            q = nodes[b]["position"]
            distance = math.dist(p, q)
            nearest[a] = min(nearest.get(a, math.inf), distance)
            nearest[b] = min(nearest.get(b, math.inf), distance)
            horizontal = math.hypot(p[0]-q[0], p[1]-q[1])
            same_height = abs(p[2]-q[2]) <= height_tolerance
            if same_height:
                nearest_floor[a] = min(nearest_floor.get(a, math.inf), horizontal)
                nearest_floor[b] = min(nearest_floor.get(b, math.inf), horizontal)
            limit = nodes[a]["radius"] + nodes[b]["radius"]
            if same_height and horizontal + .01 < limit:
                overlaps_xy += 1
            if distance + .01 < limit:
                overlaps_3d += 1
                linked = (a, b) in pairs
                linked_overlaps += int(linked)
                involved.update((a, b))
                if len(examples) < example_limit:
                    examples.append({"indices": [a, b], "uids": [nodes[a]["uid"], nodes[b]["uid"]],
                                     "distance": round(distance, 4), "linked": linked,
                                     "radii": [nodes[a]["radius"], nodes[b]["radius"]]})
            if distance < 2 and len(coincident) < example_limit:
                coincident.append({"indices": [a, b], "distance": round(distance, 4)})

    fans, fan_nodes = 0, set()
    for a in members:
        p, vectors = nodes[a]["position"], []
        for b in adjacency[a]:
            q = nodes[b]["position"]
            dx, dy = q[0]-p[0], q[1]-p[1]
            length = math.hypot(dx, dy)
            if length > 32 and abs(q[2]-p[2]) <= 18:
                vectors.append((dx/length, dy/length))
        for i, u in enumerate(vectors):
            for v in vectors[i+1:]:
                if u[0]*v[0]+u[1]*v[1] > math.cos(math.radians(20)):
                    fans += 1
                    fan_nodes.add(a)
    lengths = [math.dist(nodes[a]["position"], nodes[b]["position"]) for a, b in pairs]
    return {
        "nodes": len(members), "directed_links": len(directed), "undirected_pairs": len(pairs),
        "directed_links_per_node": round(len(directed)/max(1, len(members)), 4),
        "radius": distribution(radii),
        "radius_histogram": dict(sorted(Counter(radii).items())),
        "radius_under_10": sum(r < 10 for r in radii),
        "radius_under_10_percent": round(100*sum(r < 10 for r in radii)/max(1, len(radii)), 4),
        "radius_under_10_examples": [{"index": i, "uid": nodes[i]["uid"],
                                      "position": nodes[i]["position"], "radius": nodes[i]["radius"],
                                      "name": nodes[i]["name"], "flags": nodes[i]["flags"]}
                                     for i in members if nodes[i]["radius"] < 10][:example_limit],
        "out_degree_histogram": dict(sorted(Counter(outgoing[i] for i in members).items())),
        "undirected_degree_histogram": dict(sorted(Counter(len(adjacency[i]) for i in members).items())),
        "degree": distribution([len(adjacency[i]) for i in members]),
        "link_length_3d": distribution(lengths),
        "nearest_point_3d": distribution(list(nearest.values())),
        "nearest_point_xy_within_height_tolerance": distribution(list(nearest_floor.values())),
        "overlaps": {"arrival_spheres_3d": overlaps_3d,
                     "arrival_circles_xy_within_height_tolerance": overlaps_xy,
                     "linked_pairs_3d": linked_overlaps,
                     "unlinked_pairs_3d": overlaps_3d-linked_overlaps,
                     "nodes_involved_3d": len(involved),
                     "examples": examples,
                     "interpretation": "Overlapping arrival tolerances, not colliding bodies or invalid waypoints."},
        "points_with_neighbour_under_2": sum(d < 2 for d in nearest.values()),
        "near_coincident_examples": coincident,
        "parallel_fans_under_20_degrees": fans, "nodes_with_parallel_fans": len(fan_nodes),
        "parallel_fans_per_100_nodes": round(100*fans/max(1, len(members)), 4),
    }


def analyze(path, height_tolerance=18, mirror_directory=None):
    path = Path(path)
    raw = path.read_bytes()
    nodes, edges = way_reader()(path)
    if len({n["uid"] for n in nodes}) != len(nodes):
        raise ValueError(f"duplicate waypoint UIDs in {path}")
    source = {"path": str(path.resolve()), "bytes": len(raw),
              "sha256": hashlib.sha256(raw).hexdigest(), "format": "Omni-bot WAY v9"}
    if mirror_directory is not None:
        mirror = Path(mirror_directory)/path.name
        source["mirror"] = {"path": str(mirror.resolve()), "exists": mirror.is_file()}
        if mirror.is_file():
            sha = hashlib.sha256(mirror.read_bytes()).hexdigest()
            source["mirror"].update(sha256=sha, byte_identical=sha == source["sha256"])
    ordinary = [i for i, n in enumerate(nodes) if not (n["flags"] & ~64)]
    all_nodes = list(range(len(nodes)))
    result = {"map": path.stem, "source": source,
            "flags_histogram": dict(sorted(Counter(n["flags"] for n in nodes).items())),
            "all": cohort(nodes, edges, all_nodes, height_tolerance),
            "ordinary": cohort(nodes, edges, ordinary, height_tolerance)}
    if path.read_bytes() != raw:
        raise ValueError(f"WAY changed while being measured: {path}")
    return result


def report(paths, compare, height_tolerance=18, mirror_directory=None):
    references = [analyze(path, height_tolerance, mirror_directory) for path in paths]
    compared = analyze(compare, height_tolerance) if compare else None
    total = sum(r["ordinary"]["nodes"] for r in references)
    tiny = sum(r["ordinary"]["radius_under_10"] for r in references)
    return {"schema": 1, "read_only": True,
            "method": {"ordinary": "flags contain only NONE or CROUCH (bit6); other semantic/action flags excluded",
                       "overlap_3d": "Euclidean distance +0.01 < sum of radii; matches former blanket audit metric",
                       "overlap_xy": "XY distance +0.01 < sum of radii, abs(deltaZ) <= height tolerance",
                       "height_tolerance": height_tolerance,
                       "quantiles": "linear interpolation between ranked values",
                       "fan": "two undirected neighbours within18 height and beyond32 XY, angle<20 degrees",
                       "limitations": "No geometry, live traversal, map area normalization or safety approval. ET references are not TCE hull proofs."},
            "references": references, "comparison": compared,
            "reference_ordinary_total": total, "reference_ordinary_radius_under_10": tiny,
            "reference_ordinary_radius_under_10_percent": round(100*tiny/max(1, total), 4)}


def main():
    root = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nav-directory", type=Path, default=root/"tce2/omni-bot/et/nav")
    parser.add_argument("--maps", nargs="+", default=MAPS)
    parser.add_argument("--compare", type=Path, default=root/"tce2/omni-bot/et/nav/obj_overkill.way")
    parser.add_argument("--mirror-directory", type=Path)
    parser.add_argument("--height-tolerance", type=float, default=18)
    parser.add_argument("--out", type=Path, required=True)
    args = parser.parse_args()
    if not math.isfinite(args.height_tolerance) or args.height_tolerance < 0:
        parser.error("height tolerance must be finite and nonnegative")
    if any(Path(name).name != name or not name or name in (".", "..") for name in args.maps):
        parser.error("map names must be basenames")
    paths = [args.nav_directory/(name+".way") for name in args.maps]
    if args.out.resolve() in {path.resolve() for path in paths+[args.compare]}:
        parser.error("output cannot overwrite an input WAY")
    result = report(paths, args.compare, args.height_tolerance, args.mirror_directory)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    for row in result["references"]+[result["comparison"]]:
        a, o = row["all"], row["ordinary"]
        print(f"{row['map']}: {a['nodes']} nodes/{a['directed_links']} links; "
              f"ordinary radius median {o['radius'].get('median')}, <10 {o['radius_under_10']}/{o['nodes']}; "
              f"overlap pairs {o['overlaps']['arrival_spheres_3d']}; "
              f"fans {o['parallel_fans_under_20_degrees']}")


if __name__ == "__main__":
    main()
