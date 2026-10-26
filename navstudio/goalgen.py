"""Create GT2/GT5 Omni-bot companions from BSP objectives and current proof.

Entity-bound TC actions use the native activate protocol. Tactical positions
come exclusively from accepted, spawn-reachable navigation. File creation is
not a declaration that a mission has completed in a live match.
"""
import hashlib
import heapq
import json
import math
from pathlib import Path
import re
import struct
import sys
import time
import uuid

from navbundle import NavBundles
from spawns import bsp_entities
from completion import main_source_ids
TOOLS = Path(__file__).resolve().parent.parent
if str(TOOLS) not in sys.path: sys.path.insert(0, str(TOOLS))
from navmesh.objective_contact import capture_contact,plant_contact,valid_bounds,flag_pickup_contact

MARKER = "// TCE2_NAVSTUDIO_GENERATED_GOALS_V1"
ACTION_FLAGS = (1 << 8) | (1 << 9) | (1 << 18) | (1 << 27)
TACTICAL_TIMES = {"CAMP": (15, 30), "Defend": (25, 45), "Attack": (8, 15)}
TACTICAL_PRIORITIES = {"CAMP": .30, "Defend": 1.30, "Attack": .35}
TACTICAL_ATTACK_PRIORITY = 1.25  # bounded flank role only; ordinary actors keep native missions
STATIONARY_CONTRACT = "tactical_exact_feet_self_walk_v1"
STATIONARY_PROBE_LIMIT = 96


def stationary_binding(name, report, native_sha256, proof_inputs):
    """A graph import accepts transit points, not necessarily resting places."""
    nodes, _ = playable_nodes(report)
    bbox = report.get("bbox", {})
    body = {key:bbox.get(key) for key in ("half_width", "stand_height", "crouch_height", "origin_above_feet")}
    if (not re.fullmatch(r"[0-9a-fA-F]{64}", str(native_sha256)) or
            not isinstance(proof_inputs, dict) or not proof_inputs.get("game_module") or
            any(type(v) not in (int,float) or not math.isfinite(v) or v <= 0 for v in body.values())):
        raise RuntimeError("Stationäre Goal-Prüfung benötigt vollständige aktuelle Native-/Modul-/Körperbelege.")
    candidates = [dict(id=i, position=list(n["position"]), crouch=bool(n.get("crouch")))
                  for i,n in sorted(nodes.items())]
    return dict(contract=STATIONARY_CONTRACT, map=name, native_report_sha256=native_sha256,
                proof_inputs=json.loads(json.dumps(proof_inputs)), body_profile=body,
                candidates_sha256=hashlib.sha256(json.dumps(candidates,sort_keys=True,separators=(",", ":")).encode()).hexdigest())


class StationarySupport:
    """One bounded query/cache pool shared by both gametype plans.

    The native self-WALK tests the literal goal feet with the full capsule and
    support policy. Import acceptance alone is insufficient on a sloped prop.
    Raw replies are always reparsed; cached booleans never authorize a goal.
    """
    def __init__(self, report, binding, probe=None, certificate=None, finalize=None,observation=None):
        self.nodes, _ = playable_nodes(report)
        self.binding = json.loads(json.dumps(binding))
        self.probe = probe
        self.finalizer = finalize
        self.observation=json.loads(json.dumps(observation)) if observation is not None else dict(source="external_readonly_probe",probed_gametype=None)
        self.records = {}
        self.requests = set()
        self.budget_nodes = set()
        self.fresh_queries = 0
        if certificate is not None:
            if (not isinstance(certificate,dict) or certificate.get("schema") != 1 or
                    certificate.get("binding") != self.binding or not isinstance(certificate.get("queries"),list) or
                    len(certificate["queries"]) > STATIONARY_PROBE_LIMIT):
                raise RuntimeError("Stationärer Goal-Prüfbeleg ist veraltet oder unvollständig; keine Goals geschrieben.")
            for record in certificate["queries"]:
                identity=record.get("source_node")
                if identity in self.records or identity not in self.nodes:
                    raise RuntimeError("Stationärer Goal-Prüfbeleg enthält fremde oder doppelte Punkte.")
                checked=self._record(self.nodes[identity],record.get("raw_reply"),record.get("observation"))
                if checked != record:
                    raise RuntimeError("Stationärer Goal-Prüfbeleg stimmt nicht mit seiner nativen Rohantwort überein.")
                self.records[identity]=checked

    def _record(self, node, raw_reply,observation):
        from repair import parse_native_probe
        feet=list(node["position"])
        feet[2]-=self.binding["body_profile"]["origin_above_feet"]
        if (not isinstance(observation,dict) or not isinstance(raw_reply,str) or len(raw_reply)>65536 or
                any(type(v) not in (int,float) or not math.isfinite(v) or abs(v)>131072 for v in feet)):
            raise RuntimeError("Ungültige stationäre native Goal-Probe.")
        parsed=parse_native_probe(raw_reply,feet,feet,self.binding["body_profile"]["origin_above_feet"])
        posture="crouch" if node.get("crouch") else "stand"
        imports=[parsed.get("imports",{}).get(endpoint,{}).get(posture,{}) for endpoint in ("start","end")]
        # A .125u engine floor skin can be imported at a slightly different Z.
        # It cannot bypass the exact self-WALK; no normalized pose is substituted.
        imported=all(row.get("accepted") is True and row.get("reason")=="accepted" for row in imports)
        accepted=parsed.get(posture)=="accepted" and imported
        reason=parsed.get(posture) if parsed.get(posture)!="accepted" else "accepted" if imported else "stationary_import_not_accepted"
        return dict(source_node=node["id"],position=list(node["position"]),feet=feet,posture=posture,
                    accepted=accepted,reason=reason,raw_reply=raw_reply,reply_sha256=parsed["reply_sha256"],
                    observation=json.loads(json.dumps(observation)))

    def allows(self, node):
        identity=node["id"]
        if identity not in self.nodes or node.get("position")!=self.nodes[identity].get("position") or bool(node.get("crouch"))!=bool(self.nodes[identity].get("crouch")):
            raise RuntimeError("Stationäre Goal-Probe gehört zu einem anderen Kandidatenpunkt.")
        self.requests.add(identity)
        if identity not in self.records:
            if self.probe is None or len(self.records)>=STATIONARY_PROBE_LIMIT:
                self.budget_nodes.add(identity)
                return False
            feet=list(node["position"]);feet[2]-=self.binding["body_profile"]["origin_above_feet"]
            self.records[identity]=self._record(node,self.probe(feet,feet),self.observation)
            self.fresh_queries+=1
        return self.records[identity]["accepted"]

    def certificate(self):
        return dict(schema=1,binding=self.binding,limit=STATIONARY_PROBE_LIMIT,
                    queries=[self.records[i] for i in sorted(self.records)])

    def summary(self, goals):
        chosen={g["source_node"] for g in goals if g.get("type") in TACTICAL_TIMES}
        accepted=sorted(i for i in chosen if self.records.get(i,{}).get("accepted") is True)
        rejected=[dict(source_node=i,reason=row["reason"]) for i,row in sorted(self.records.items()) if not row["accepted"]]
        complete=bool(chosen) and len(accepted)==len(chosen) and not self.budget_nodes
        return dict(contract=STATIONARY_CONTRACT,status="complete" if complete else "incomplete",
                    complete=complete,selected_nodes=sorted(chosen),accepted_selected_nodes=accepted,
                    query_count=len(self.records),fresh_query_count=self.fresh_queries,rejected=rejected,
                    probed_gametypes=sorted({row["observation"]["probed_gametype"] for row in self.records.values()
                        if type(row["observation"].get("probed_gametype"))is int}),
                    scope="exact_stationary_geometry_only; no bot arrival or alternate gametype entity-state claim",
                    budget=STATIONARY_PROBE_LIMIT,budget_exhausted=bool(self.budget_nodes),
                    untested_requested_nodes=sorted(self.budget_nodes))


def tactical_graph(report, team):
    """Read accepted directed links, including proven transit actions, never add links."""
    known = ACTION_FLAGS | 1 | 2 | 16 | 64 | 128
    nodes = {}
    for node in report.get("nodes", []):
        flags = int(node.get("nav_flags", 0))
        recipe = node.get("door_recipe")
        if (node.get("accepted") is not True or node.get("spawn_reachable") is not True or flags & ~known or
                flags & 16 and not flags & (1 << (team - 1)) or
                recipe and (not recipe.get("validated") or not recipe.get("exported") or
                            not recipe.get("team", 0) & (1 << (team - 1)))):
            continue
        nodes[node["id"]] = node
    outgoing = {i: [] for i in nodes}
    reverse = {i: [] for i in nodes}
    for edge in report.get("edges", []):
        a, b = edge["from"], edge["to"]
        if edge.get("accepted") is True and a in nodes and b in nodes:
            cost = max(.001, math.dist(nodes[a]["position"], nodes[b]["position"]))
            outgoing[a].append((b, cost)); reverse[b].append((a, cost))
    return nodes, outgoing, reverse


def graph_distances(adjacency, starts):
    distance = {i: 0.0 for i in starts if i in adjacency}
    queue = [(0.0, i) for i in distance]
    heapq.heapify(queue); previous = {}
    while queue:
        cost, a = heapq.heappop(queue)
        if cost != distance[a]: continue
        for b, length in adjacency[a]:
            proposed = cost + length
            if proposed < distance.get(b, math.inf):
                distance[b] = proposed; previous[b] = a
                heapq.heappush(queue, (proposed, b))
    return distance, previous


def graph_path(previous, start, end):
    path = [end]
    while path[-1] != start:
        if path[-1] not in previous or len(path) > len(previous): return []
        path.append(previous[path[-1]])
    return list(reversed(path))


def distributed_tactics(report, destinations, existing, stationary_support=None):
    """Map-wide stations with a directed route to AND from their own mission.

    A wall-near or one-way island is not an extra defended sector. Positions
    are existing ordinary native floors; neither collision proof nor an aim
    ray is invented. Facing follows an actual corridor edge.
    """
    ordinary, _ = playable_nodes(report)
    result = []
    for team in (1, 2):
        nodes, forward, reverse = tactical_graph(report, team)
        for kind in ("Defend", "Attack"):
            targets = [g for g in destinations if g.get("tactical_" + kind.lower() + "_teams", 0) & (1 << team)
                       and g["source_node"] in nodes and g["source_node"] in ordinary]
            if not targets: continue
            distances = {g["name"]: (graph_distances(reverse, [g["source_node"]])[0],
                                     graph_distances(forward, [g["source_node"]])[0]) for g in targets}
            pool = [n for i, n in ordinary.items() if i in nodes and all(
                math.dist(n["position"], g["position"]) >= 96 for g in destinations) and any(
                i in distances[g["name"]][0] and i in distances[g["name"]][1] for g in targets)]
            selected = [g for g in existing if g["type"] == kind and g.get("teams", 6) & (1 << team)]
            while len(selected) < 12:
                if not pool: break
                chosen = max(pool, key=lambda n: (min(math.dist(n["position"], s["position"]) for s in selected)
                             if selected else math.inf, -n["id"]))
                gap = min((math.dist(chosen["position"], s["position"]) for s in selected), default=math.inf)
                if gap < 320: break
                pool.remove(chosen)
                if stationary_support is not None and not stationary_support.allows(chosen):
                    continue
                target = min((g for g in targets if chosen["id"] in distances[g["name"]][0] and
                              chosen["id"] in distances[g["name"]][1]),
                             key=lambda g: (distances[g["name"]][0][chosen["id"]], g["name"]))
                toward = distances[target["name"]][0]
                neighbors = [nodes[i] for i, _ in forward[chosen["id"]] if i in toward]
                if not neighbors:
                    continue
                watch = (max if kind == "Defend" else min)(neighbors, key=lambda n: toward[n["id"]])
                delta = [b-a for a, b in zip(chosen["position"], watch["position"])]
                length = math.sqrt(sum(x*x for x in delta)) or 1
                goal = dict(name=kind.upper()+f'_NavStudio_T{team}_{chosen["id"]}', type=kind,
                    position=list(chosen["position"]), teams=1 << team, source_node=chosen["id"],
                    aim_vector=[v/length for v in delta], crouch=bool(chosen.get("crouch")),
                    radius=max(20, min(80, round(chosen.get("radius", 35)))),
                    group=f'NavStudio_{kind}_T{team}', tactical_objective=target["name"],
                    station_route_checked=True,stationary_support_checked=stationary_support is not None)
                selected.append(goal); result.append(goal)
    return result


def corridor_routes(report, destinations, prior_goals=(), prior_routes=()):
    """Bounded real alternate corridors, not randomly displaced waypoints.

    Two shortest directed legs must be loop-free, bounded in length, and
    separated from the primary interior by a graph detour (a wall/one-way
    barrier). A uniformly open mesh cannot manufacture a second corridor.
    Ends lie outside ALL start catchments: the native RouteTo consumer then
    finishes at its original goal instead of recursively selecting a loop.
    """
    ordinary, _ = playable_nodes(report)
    goals, routes, families = {}, [], []
    prior_by_name = {g["name"]: g for g in prior_goals}
    for team in (1, 2):
        nodes, forward, reverse = tactical_graph(report, team)
        end_pool = set(); family_count = 0
        candidates = {i: n for i, n in ordinary.items() if i in nodes}
        spawns = {i: n for i, n in candidates.items() if "spawn" in str(n.get("reason", "")) or
                  "spawn" in str(n.get("native_input", {}).get("floor_reason", ""))}
        launches = spread(spawns, maximum=4, spacing=384)
        for node in spread(candidates, maximum=8, spacing=512):
            if all(math.dist(node["position"], old["position"]) >= 512 for old in launches): launches.append(node)
            if len(launches) >= 8: break
        launch_data = {n["id"]: graph_distances(forward, [n["id"]]) for n in launches}
        radii = {n["id"]: (192 if n["id"] in spawns else 96) for n in launches}
        for destination in sorted(destinations, key=lambda g:g.get("type") not in ("TCEOBJECTIVE","Flag","CapPoint")):
            if family_count >= 32: break
            destination_families = 0
            target = destination["source_node"]
            if target not in candidates or not destination.get("teams", 6) & (1 << team): continue
            remaining, following = graph_distances(reverse, [target])
            old = [r for r in prior_routes if r["goal"] == destination["name"] and r.get("team") == team]
            old_starts = [prior_by_name[r["start"]] for r in old if r["start"] in prior_by_name]
            old_ends = [prior_by_name[r["end"]] for r in old if r["end"] in prior_by_name]
            def clear_of(node, starts, extra=40):
                return all(math.dist(node["position"][:2], start["position"][:2]) > start["radius"]+extra
                           or abs(node["position"][2]-start["position"][2]) > start["radius"]+192
                           for start in starts)
            # A door continuation must not re-enter a new alternative's
            # catchment, and no new end may re-enter a door start catchment.
            eligible = [n for n in launches if remaining.get(n["id"], 0) >= 512 and all(
                math.dist(n["position"][:2], end["position"][:2]) > radii[n["id"]]+end["radius"]+8 or
                abs(n["position"][2]-end["position"][2]) > radii[n["id"]]+192 for end in old_ends)]
            def outside_starts(node):
                return clear_of(node, old_starts) and all(math.dist(node["position"][:2], start["position"][:2]) > radii[start["id"]]+40
                           or abs(node["position"][2]-start["position"][2]) > radii[start["id"]]+192
                           for start in eligible)
            for start in eligible:
                if family_count >= 32 or destination_families >= 2: break
                identity = start["id"]
                from_start, previous = launch_data[identity]
                baseline = graph_path(following, target, identity)[::-1]
                if len(baseline) < 5: continue
                base_length = remaining[identity]
                interior = [i for i in baseline if .2*base_length < remaining[i] < .8*base_length]
                if not interior: continue
                to_interior = graph_distances(reverse, interior)[0]
                options = []
                for i, node in candidates.items():
                    if (len(end_pool) >= 16 and i not in end_pool or
                            i not in from_start or i not in remaining or not outside_starts(node) or
                            remaining[i] >= base_length-radii[identity]-64 or from_start[i] < 128 or
                            from_start[i]+remaining[i] > base_length*1.75+128): continue
                    separation = min(math.dist(node["position"], nodes[b]["position"]) for b in interior)
                    if separation < 160 or to_interior.get(i, math.inf) <= separation*1.4+64: continue
                    path = graph_path(previous, identity, i) + graph_path(following, target, i)[-2::-1]
                    if not path or len(path) != len(set(path)): continue
                    options.append((separation, from_start[i]+remaining[i], i, path))
                if not options: continue
                options.sort(key=lambda item: (item[2] not in end_pool, -item[0], item[1], item[2]))
                # The primary endpoint must also leave every launch catchment.
                primary = [i for i in interior if i in candidates and outside_starts(candidates[i]) and
                           remaining[i] < base_length-radii[identity]-64 and from_start[i] >= 128]
                if not primary: continue
                primary.sort(key=lambda i: (i not in end_pool, abs(remaining[i]-base_length*.5), i))
                pair = next(((i, option) for option in options for i in primary
                             if len(end_pool | {i, option[2]}) <= 16), None)
                if pair is None: continue
                mid, alternate = pair
                end_pool.update((mid, alternate[2]))
                branches = [(mid, baseline, 0.0), (alternate[2], alternate[3], alternate[0])]
                start_name = f'ROUTE_NavStudio_Corridor_T{team}_{identity}'
                goals.setdefault(start_name, dict(name=start_name, type="ROUTE", position=list(start["position"]),
                    radius=radii[identity], teams=1 << team, source_node=identity, corridor_role="entry"))
                family = dict(goal=destination["name"], team=team, source_node=identity,
                              target_node=target, shortest_distance=base_length, branches=[])
                for endpoint, path, separation in branches:
                    end_name = f'ROUTE_NavStudio_Corridor_T{team}_{endpoint}_end'
                    goals.setdefault(end_name, dict(name=end_name, type="ROUTE", position=list(nodes[endpoint]["position"]),
                        radius=32, teams=1 << team, source_node=endpoint, corridor_role="exit"))
                    route = dict(goal=destination["name"], start=start_name, end=end_name, weight=1,
                        team=team, source_node=identity, target_node=endpoint, path=path,
                        remaining_distance=remaining[endpoint], corridor_separation=separation,
                        activation="native_directed_alternate_corridor")
                    routes.append(route); family["branches"].append(route)
                families.append(family)
                family_count += 1
                destination_families += 1
    return list(goals.values()), routes, families


def vector(value):
    if len(value) != 3 or any(type(v) not in (int, float) or not math.isfinite(v) for v in value):
        raise ValueError("Goal benötigt drei endliche Koordinaten.")
    return "Vec3(" + ", ".join(format(float(v), ".5f") for v in value) + ")"


def native_tag(entity):
    """Match _GetEntityName and MapGoal::GenerateName for stable deferred data."""
    tag = next((entity.get(key) for key in ("track", "scriptname", "targetname", "message")
                if entity.get(key)), "")
    tag = re.sub(r"\^[0-9]", "", tag).replace("-", "_")
    tag = re.sub(r"[\[\]#!*`^&<>+=|'%.:/()]", "", tag)
    if tag[:4].lower() == "the ": tag = tag[4:]
    return re.sub(r"[^A-Za-z0-9_]", "", tag.replace(" ", "_"))


def capture_teams(flags):
    """Flagonly accepts flag colours, not the enclosing OID owner's team.

    RED_FLAG is carried by Allies after stealing the Axis flag; BLUE_FLAG
    is carried by Axis. No colour bits means this is not a usable cap point.
    """
    if type(flags) is not int:return 0
    return (4 if flags&1 else 0)|(2 if flags&2 else 0)


def playable_nodes(report, include_doors=False, *, destination_scope=True):
    if not report or report.get("generation_complete") is not True:
        raise ValueError("Goals benötigen eine aktuelle vollständige native Navigation.")
    main = main_source_ids(report) if destination_scope else None
    nodes = {n["id"]: n for n in report.get("nodes", []) if n.get("accepted") is True and
             n.get("spawn_reachable") is True and not int(n.get("nav_flags", 0)) & ACTION_FLAGS}
    incoming, outgoing = set(), {}
    for edge in report.get("edges", []):
        if edge.get("accepted") and edge["from"] in nodes and edge["to"] in nodes:
            incoming.add(edge["to"])
            outgoing.setdefault(edge["from"], []).append(edge["to"])
    kept = {identity: node for identity, node in nodes.items() if identity in incoming and identity in outgoing}
    while True:
        links = {identity: [other for other in outgoing.get(identity, []) if other in kept] for identity in kept}
        updated = {identity: node for identity, node in kept.items() if links[identity]}
        if len(updated) == len(kept):
            # Door action/wait anchors are route metadata, never ordinary camps
            # or objective-use positions. Keep the full adjacency for routing.
            # A one-way spawn entrance can lead into this graph, but cannot be
            # a global mission/resting destination that ordinary actors must
            # reach from the main network. Selection alone grants no graph
            # acceptance; the shared readiness/install contract does that.
            destinations = {i:n for i,n in kept.items() if main is None or i in main}
            return (destinations if include_doors else {i:n for i,n in destinations.items() if not n.get("door_recipe")}), links
        kept = updated


def spread(nodes, maximum=20, spacing=256, allowed=None):
    pool, selected = list(nodes.values()), []
    distance = lambda a, b: sum((x-y)**2 for x, y in zip(a["position"], b["position"]))
    while pool and len(selected) < maximum:
        chosen = max(pool, key=lambda n: min(distance(n, p) for p in selected)) if selected else min(pool, key=lambda n: n["id"])
        if selected and min(distance(chosen, p) for p in selected) < spacing**2:
            break
        pool.remove(chosen)
        if allowed is None or allowed(chosen):selected.append(chosen)
    return selected


def objectives(assets, name, gametype):
    from tce_bsp_navmesh import read_lumps, entity_transform
    data = assets.read("maps/" + name + ".bsp", 128 * 1024 * 1024)
    lump = read_lumps(data)[7]
    models = [struct.unpack_from("<6f4i", data, offset) for offset in
              range(lump["offset"], lump["offset"] + lump["length"], 40)]
    entities = list(bsp_entities(assets, name))
    targets = {e["targetname"]: e for e in entities if e.get("targetname")}
    def model_bounds(entity):
        if not re.fullmatch(r"\*[0-9]+", entity.get("model", "")): return None
        index = int(entity["model"][1:])
        if not 0 <= index < len(models): return None
        model = models[index]
        try:
            origin = [float(v) for v in entity.get("origin", "0 0 0").split()]
            angles = [float(v) for v in entity.get("angles", "0 "+entity.get("angle", "0")+" 0").split()]
            if len(origin)!=3 or len(angles)!=3: return None
            transform, _ = entity_transform(origin, angles)
            corners = [transform([model[x],model[1+y],model[2+z]]) for x in (0,3) for y in (0,3) for z in (0,3)]
            return [[min(v[i] for v in corners) for i in range(3)],
                    [max(v[i] for v in corners) for i in range(3)]]
        except (ValueError, OverflowError): return None
    result = []
    for entity in entities:
        if gametype == 2 and entity.get("classname") in ("team_CTF_redflag","team_CTF_blueflag"):
            team = 2 if entity["classname"] == "team_CTF_redflag" else 1
            tag = native_tag(entity)
            if tag:
                result.append(dict(tag=tag,entity_id=entity["bsp_id"],position=entity["position"],
                    aim_position=entity["position"],bounds=None,owner=3-team,stealing_team=team,
                    operation="take_flag",target=entity.get("scriptname"),target_class=entity["classname"]))
            continue
        if entity.get("classname") != "trigger_objective_info": continue
        try: flags = int(entity.get("spawnflags", "0"))
        except ValueError: continue
        if not flags & 3 or gametype == 2 and flags & 128 or gametype == 5 and flags & 64: continue
        target = targets.get(entity.get("target"))
        if not target: continue
        if gametype == 2 and target.get("classname") not in ("trigger_flagonly_multiple", "trigger_flagonly", "func_obj_use", "func_explosive"):
            continue
        if gametype == 5 and target.get("classname") != "func_explosive": continue
        tag = native_tag(entity)
        if not tag: continue
        plant = target.get("classname") == "func_explosive"
        contact = entity if plant else target
        bounds = model_bounds(contact)
        target_bounds=model_bounds(target) if plant else None
        aim_position=[(a+b)*.5 for a,b in zip(*target_bounds)] if target_bounds else target["position"]
        contact_top=None
        if plant and re.fullmatch(r"\*[0-9]+",entity.get("model","")):
            model_index=int(entity["model"][1:])
            if 0<=model_index<len(models):
                raw_origin=[float(v)for v in entity.get("origin","0 0 0").split()]
                if len(raw_origin)==3:contact_top=models[model_index][5]+raw_origin[2]
        result.append(dict(tag=tag, entity_id=entity["bsp_id"], position=entity["position"],
                           aim_position=aim_position, bounds=bounds,
                           target_bounds=target_bounds,contact_top=contact_top, owner=flags & 3,
                           operation="plant_defuse" if plant else "capture" if target["classname"].startswith("trigger_flagonly") else "activate_capture",
                           target=target.get("targetname"), target_class=target.get("classname")))
        if result[-1]["operation"]=="capture":
            try: target_flags=int(target.get("spawnflags","0"))
            except ValueError: target_flags=0
            result[-1].update(capture_teams=capture_teams(target_flags),capture_flag_mask=target_flags&3,
                              capture_team_source="bsp_flagonly_spawnflags")
    return result


def observed_objectives(snapshot, name, gametype):
    """Use only a matching live snapshot, never GT2 state as GT5 entity proof."""
    if (not isinstance(snapshot,dict) or snapshot.get("map")!=name or
            snapshot.get("gametype")!=gametype or "objective_entities" not in snapshot):
        return None
    if not isinstance(snapshot["objective_entities"],list):
        raise ValueError("Aktuelles natives Missionsinventar ist unvollständig.")
    result=[]
    for entity in snapshot["objective_entities"]:
        if not isinstance(entity,dict) or entity.get("dropped"):continue
        classname=entity.get("classname");target=entity.get("target")
        is_flag=gametype==2 and classname in ("team_CTF_redflag","team_CTF_blueflag")
        flags=entity.get("spawnflags",0)
        # An orphan TOI whose target was removed by the selected map script is
        # not an active mission. Never revive its original BSP flag/crate.
        is_toi=classname=="trigger_objective_info" and type(flags)is int and flags&3 and isinstance(target,dict)
        if not is_flag and not is_toi:continue
        def unsupported(reason):
            label=entity.get('track') or entity.get('scriptname') or entity.get('targetname') or classname
            target_info=(str(target.get('entity','?'))+' / '+str(target.get('classname','?'))+
                         ' / '+str(target.get('targetname') or target.get('scriptname') or '?'))if isinstance(target,dict)else classname
            raise ValueError('GT%d: aktives Missionsziel Entity %s (%s), Ziel %s: %s. Keine Phantom- oder Ersatzmission erzeugt.'%
                (gametype,entity.get('entity','?'),label,target_info,reason))
        tag=native_tag(entity);classname=entity.get("classname");position=entity.get("position")
        if not tag or not isinstance(position,list) or len(position)!=3 or any(
                type(v) not in (int,float) or not math.isfinite(v) for v in position):unsupported('Name oder native Position fehlt')
        if type(entity.get("entity")) is not int or not 0<=entity["entity"]<1024:unsupported('Native Entity-ID fehlt')
        operation=entity.get("native_goal_kind")
        if gametype==2 and operation=="take_flag" and classname in ("team_CTF_redflag","team_CTF_blueflag"):
            team=2 if classname=="team_CTF_redflag" else 1
            result.append(dict(tag=tag,entity_id=entity["entity"],position=position,
                aim_position=position,bounds=None,owner=3-team,stealing_team=team,
                operation="take_flag",target=entity.get("scriptname"),target_class=classname,
                mission_entity_source="native_runtime_snapshot"))
            continue
        if is_flag:unsupported('nicht unterstützter nativer Flagtyp '+str(operation))
        target_class=target.get("classname")
        allowed=("capture","activate_capture","plant_defuse")if gametype==2 else("plant_defuse",)
        if operation not in allowed:unsupported('nicht unterstützter nativer Missionstyp '+str(operation))
        if operation=='plant_defuse' and (target_class!='func_explosive' or
                target.get('weaponclass',2)!=2):unsupported('Plant/Defuse-Ziel ist kein natives Sprengziel der Waffenklasse 2')
        if operation=='capture' and target_class not in('trigger_flagonly','trigger_flagonly_multiple'):
            unsupported('Capture-Ziel ist kein nativer Flagonly-Trigger')
        if operation=='activate_capture' and target_class!='func_obj_use':
            unsupported('Aktivierungsziel ist kein natives func_obj_use')
        bounds=entity.get("bounds") if operation=="plant_defuse" else target.get("bounds")
        aim=target.get("position")
        if (not valid_bounds(bounds) or not isinstance(aim,list) or len(aim)!=3 or
                any(type(v) not in (int,float) or not math.isfinite(v) for v in aim)):unsupported('Native Kontaktgeometrie fehlt')
        result.append(dict(tag=tag,entity_id=entity["entity"],position=position,aim_position=aim,bounds=bounds,
            target_bounds=target.get("bounds") if operation=="plant_defuse" else None,contact_top=entity.get("contact_top"),
            owner=flags&3,operation=operation,
            target=target.get("targetname"),target_class=target_class,mission_entity_source="native_runtime_snapshot"))
        if operation=="capture":
            target_flags=target.get("spawnflags")
            result[-1].update(capture_teams=capture_teams(target_flags),
                capture_flag_mask=target_flags&3 if type(target_flags)is int else None,
                capture_team_source="native_flagonly_spawnflags" if type(target_flags)is int else "native_flagonly_spawnflags_missing")
    return result


def mission_sources(assets,name,gametype,snapshot=None):
    observed=observed_objectives(snapshot,name,gametype)
    if observed is not None:
        return observed,"matching_native_runtime_snapshot"
    return objectives(assets,name,gametype),"bsp_proposal_runtime_entities_unconfirmed"


def render_goal(goal):
    fields = {"GoalType": json.dumps(goal["type"]), "Version": "1", "Position": vector(goal["position"]),
              "Radius": str(goal.get("radius", 35)), "TeamAvailability": str(goal.get("teams", 6))}
    if goal["type"] == "ROUTE":
        fields["CreateOnLoad"] = "1"
    elif goal["type"] == "TCEOBJECTIVE":
        fields.update(CreateOnLoad="0", UsePosition=vector(goal["position"]),
                      AimPosition=vector(goal["aim_position"]), ActivationRadius=str(goal.get("activation_radius",12)),
                      Operation=json.dumps(goal["operation"]))
    elif goal["type"] in ("CapPoint","Flag"):
        fields["CreateOnLoad"] = "0" if goal["type"] == "Flag" else "1"
        if goal["type"] == "Flag" and goal.get("pickup_contact_checked"):
            fields.update(TCECheckedUse="1", UsePosition=vector(goal["position"]),
                          FlagBasePosition=vector(goal["flag_base_position"]), ActivationRadius="2")
    else:
        minimum,maximum=TACTICAL_TIMES.get(goal["type"],(2,6))
        minimum=goal.get("min_camp_time",minimum);maximum=goal.get("max_camp_time",maximum)
        if (any(type(v) not in (int,float) or not math.isfinite(v) for v in (minimum,maximum)) or
                not 0<=minimum<=maximum<=9999):
            raise ValueError("Ungültige minimale/maximale Verweildauer.")
        fields.update(AimVectors="{ [0] = " + vector(goal["aim_vector"]) + ", }",
                      Stance=json.dumps("crouch" if goal.get("crouch") else "stand"),
                      MinCampTime=str(minimum), MaxCampTime=str(maximum), Weapons="{}", Roles="{}")
        if goal.get("group"): fields["GroupName"] = json.dumps(goal["group"])
    return "    " + goal["name"] + " =\n    {\n" + "".join(
        "        " + key + " = " + value + ",\n" for key, value in fields.items()) + "    },\n"


def door_routes(report, destinations):
    """Standard MapGoal routes; physical activation stays in FollowPath.

    A route advances towards its existing destination on accepted directed
    edges. No USE->ENTRY->USE cycle and no new high-level door task is created.
    ROUTE's real schema requires radius >=32: endpoints must be separated in
    XY so RouteTo cannot select the same start again after reaching its end.
    """
    nodes, outgoing = playable_nodes(report, include_doors=True, destination_scope=False)
    main = main_source_ids(report)
    goals, routes, unresolved = {}, [], []
    valid_entries = []
    identity = ("entity", "use_entity", "mode", "team", "model", "use_model", "use_source_id", "entry_source_id", "aim")
    def same(a,b): return all(a.get(key)==b.get(key) for key in identity)
    def certified(node):
        r=node.get("door_recipe",{})
        return r.get("validated") is True and r.get("exported") is True and int(node.get("nav_flags",0))&128
    def allowed(node,team):
        flags=int(node.get("nav_flags",0))
        return not flags&16 or bool(flags&(1<<(team-1)))
    def route_goal(node,entry,role,teams):
        name=f'ROUTE_NavStudio_Door_{entry["door_recipe"]["entity"]}_{entry["id"]}_{role}'
        goals.setdefault(name,dict(name=name,type="ROUTE",position=list(node["position"]),
            radius=32,teams=teams,source_node=node["id"],door_role=role,door_entry=entry["id"]))
        return name
    for entry in sorted(nodes.values(),key=lambda n:n["id"]):
        recipe=entry.get("door_recipe",{})
        if recipe.get("role")!=2: continue
        exit=nodes.get(recipe.get("target_source_id"))
        use=nodes.get(recipe.get("use_source_id"))
        if (not certified(entry) or recipe.get("team") not in (1,2,3) or recipe.get("mode") not in (1,2,3) or
                recipe.get("entry_source_id")!=entry["id"] or not exit or not use or
                not certified(exit) or exit["door_recipe"].get("role")!=3 or not same(recipe,exit["door_recipe"]) or
                not certified(use) or not same(recipe,use["door_recipe"]) or
                use["door_recipe"].get("role")!=(1 if recipe.get("mode") in (2,3) else 2) or
                exit["id"] not in outgoing.get(entry["id"],[])):
            unresolved.append(dict(source_node=entry["id"],reason="door_route_requires_current_entry_use_and_safe_exit_proof"))
            continue
        teams=sum(1<<t for t in (1,2) if recipe["team"]&(1<<(t-1)))
        entry_name=route_goal(entry,entry,"entry",teams)
        use_name=route_goal(use,entry,"use",teams)
        # A staged local USE must precede its swing-area alignment point.
        # Starting RouteTo at ENTRY would let the planner skip safe activation.
        approach=use if recipe["mode"]==3 else entry
        start=use_name if recipe["mode"]==3 else entry_name
        route_goal(exit,entry,"exit",teams)
        valid_entries.append((approach,entry,exit,start))
    if not valid_entries:return list(goals.values()),routes,unresolved
    # Team-aware reverse Dijkstra: strict positive progress prevents route
    # loops, and the selected continuation follows native edges, not a chord.
    reverse={i:[] for i in nodes}
    for a,links in outgoing.items():
        if a not in nodes:continue
        for b in links:
            if b in nodes:reverse[b].append(a)
    def distances(target,team):
        result={target:0.0}; next_node={}; queue=[(0.0,target)]
        while queue:
            distance,b=heapq.heappop(queue)
            if distance!=result[b]:continue
            for a in reverse[b]:
                recipe=nodes[a].get("door_recipe",{})
                if (not allowed(nodes[a],team) or not allowed(nodes[b],team) or
                        recipe.get("role")==2 and recipe.get("target_source_id")==b and
                        (not certified(nodes[a]) or not recipe.get("team",0)&(1<<(team-1)))):continue
                proposed=distance+max(.001,math.dist(nodes[a]["position"],nodes[b]["position"]))
                if proposed<result.get(a,math.inf):
                    result[a]=proposed;next_node[a]=b;heapq.heappush(queue,(proposed,a))
        return result,next_node
    for destination in destinations:
        target=destination["source_node"]
        if target not in nodes or target not in main:continue
        for team in (1,2):
            if not destination.get("teams",6)&(1<<team) or not allowed(nodes[target],team):continue
            distance,next_node=distances(target,team)
            for approach,entry,exit,start in valid_entries:
                if not entry["door_recipe"]["team"]&(1<<(team-1)) or next_node.get(entry["id"])!=exit["id"]:continue
                if approach is not entry and next_node.get(approach["id"])!=entry["id"]:continue
                path=([approach["id"]] if approach is not entry else [])+[entry["id"],exit["id"]]; end=exit
                while (end["id"] not in main or
                       sum((approach["position"][i]-end["position"][i])**2 for i in (0,1))<=66**2):
                    following=next_node.get(end["id"])
                    if following is None:break
                    end=nodes[following];path.append(following)
                    if end.get("door_recipe"):break
                if (end["id"] not in main or
                        sum((approach["position"][i]-end["position"][i])**2 for i in (0,1))<=66**2):
                    continue
                # A continuation must be this recipe's safe exit or an ordinary
                # point, never another action point masquerading as its exit.
                if end is not exit and end.get("door_recipe"):continue
                end_name=route_goal(end,entry,f'exit_team{team}' if end is exit else f'continue_{end["id"]}_team{team}',1<<team)
                routes.append(dict(goal=destination["name"],start=start,end=end_name,weight=1,
                    team=team,source_node=approach["id"],target_node=end["id"],path=path,
                    remaining_distance=distance[end["id"]],activation="native_waypoint_door_recipe"))
    # The end's availability scopes each assignment to its certified team.
    unique={ (r["goal"],r["start"],r["end"]):r for r in routes }
    return list(goals.values()),list(unique.values()),unresolved


def make_plan(report, source_objectives, gametype, stationary_support=None):
    if gametype not in (2, 5): raise ValueError("Goal-Generierung unterstützt GT2 und GT5.")
    nodes, outgoing = playable_nodes(report)
    if not nodes: raise ValueError("Keine verbundenen erreichbaren Goal-Positionen vorhanden.")
    all_ordinary, _ = playable_nodes(report, destination_scope=False)
    goals, missing = [], []
    native_names = set()
    origin = report.get("bbox", {}).get("origin_above_feet", 30)
    radius = report.get("bbox", {}).get("half_width", 16 if origin==30 else 14)
    height = report.get("bbox", {}).get("stand_height", 87 if origin==30 else 70)
    duck_height = report.get("bbox", {}).get("crouch_height", 61 if origin==30 else 49)
    for source in source_objectives:
        if source.get('operation')not in('take_flag','capture','activate_capture','plant_defuse'):
            missing.append(dict(tag=source.get('tag'),entity_id=source.get('entity_id'),reason='unsupported_native_mission_operation'))
            continue
        native_name = ("FLAG_" if source["operation"] == "take_flag" else
                       "CAPPOINT_" if source["operation"] == "capture" else "TCEOBJECTIVE_") + source["tag"]
        if native_name in native_names:
            missing.append(dict(tag=source["tag"],entity_id=source["entity_id"],reason="duplicate_native_objective_tag"))
            continue
        native_names.add(native_name)
        if source["operation"]=="capture" and source.get("capture_teams") not in (2,4,6):
            missing.append(dict(tag=source["tag"],entity_id=source["entity_id"],
                                reason="missing_flagonly_capture_flag_mask"))
            continue
        bounds = source.get("bounds")
        possible = list(nodes.values())
        # The origin/feet offset matches the profile certified in this report.
        # Prefer a proven floor within the trigger contact volume, not its solid
        # target's center. Never generate a new untraced floor point for a goal.
        def contact(n):
            if source["operation"]=="plant_defuse":
                return plant_contact(
                [n["position"][0],n["position"][1],n["position"][2]-origin],
                origin,bounds,radius,duck_height if n.get("crouch") else height,source.get("target_bounds"),
                source.get("contact_top"))
            if source["operation"]=="take_flag":
                # BG_PlayerTouchesItem uses an origin box, with the existing
                # final arrival reserve, rather than EntityContactCapsule.
                return flag_pickup_contact(n["position"],source["position"])
            if source["operation"]=="capture":
                return capture_contact(
                [n["position"][0],n["position"][1],n["position"][2]-origin],bounds,radius,
                duck_height if n.get("crouch") else height)
            return bool(bounds and bounds[0][0]-12 <= n["position"][0] <= bounds[1][0]+12 and
                      bounds[0][1]-12 <= n["position"][1] <= bounds[1][1]+12 and
                      bounds[0][2]-32 <= n["position"][2]-origin <= bounds[1][2])
        inside = [n for n in possible if contact(n)]
        if not inside:
            outside = sorted(i for i,n in all_ordinary.items() if i not in nodes and contact(n))
            if outside:
                missing.append(dict(tag=source["tag"],entity_id=source["entity_id"],
                                    reason="objective_contact_outside_main_component",source_node_ids=outside))
                continue
        distance = lambda n: sum((x-y)**2 for x,y in zip(n["position"], source["position"]))
        chosen = min(inside or possible, key=distance)
        if source["operation"] == "take_flag":
            if not inside:
                missing.append(dict(tag=source["tag"],entity_id=source["entity_id"],
                                    reason="no_native_floor_in_flag_pickup_range",nearest_node=chosen["id"]))
                continue
            goals.append(dict(name="FLAG_"+source["tag"],type="Flag",position=list(chosen["position"]),
                radius=2,teams=1 << source["stealing_team"],source_node=chosen["id"],operation="take_flag",
                tactical_defend_teams=1 << source["owner"], tactical_attack_teams=1 << source["stealing_team"],
                flag_base_position=list(source["position"]),pickup_contact_checked=True,
                pickup_contact_rule="BG_PlayerTouchesItem_origin_box_36_margin_2_125",
                entity_id=source["entity_id"],contact_floor_checked=True))
            continue
        if not inside:
            missing.append(dict(tag=source["tag"], entity_id=source["entity_id"],
                                reason="no_native_floor_in_objective_contact_volume", nearest_node=chosen["id"]))
            continue
        mission_type = "CapPoint" if source["operation"] == "capture" else "TCEOBJECTIVE"
        goals.append(dict(name=mission_type.upper()+"_"+source["tag"],type=mission_type,
            position=list(chosen["position"]), aim_position=source["aim_position"], radius=2 if mission_type=="CapPoint" else 12,
            activation_radius=2 if source["operation"]=="plant_defuse" else 12,
            teams=source["capture_teams"] if mission_type=="CapPoint" else 6,
            source_node=chosen["id"], operation=source["operation"], entity_id=source["entity_id"],
            objective_owner=source["owner"], contact_floor_checked=True))
        tactical_owner=({2:1,4:2,6:3}[source["capture_teams"]] if mission_type=="CapPoint" else source["owner"])
        enemy = 2 if tactical_owner == 1 else 1 if tactical_owner == 2 else 0
        goals[-1].update(tactical_defend_teams=(1 << tactical_owner) if tactical_owner < 3 else 6,
                         tactical_attack_teams=(1 << enemy) if enemy else 6)
        for kind, team in (("Attack", enemy), ("Defend", tactical_owner if tactical_owner < 3 else 0)):
            neighbor_ids = [i for i in outgoing[chosen["id"]] if i in nodes]
            anchors=sorted((nodes[i] for i in neighbor_ids),key=distance,reverse=True)
            if chosen not in anchors:anchors.append(chosen)
            anchor=next((n for n in anchors if stationary_support is None or stationary_support.allows(n)),None)
            if anchor is None:continue
            delta = [y-x for x,y in zip(anchor["position"], source["aim_position"])]
            length = math.sqrt(sum(x*x for x in delta)) or 1
            goals.append(dict(name=kind.upper()+"_"+source["tag"],type=kind,position=list(anchor["position"]),
                aim_vector=[x/length for x in delta], teams=(1 << team) if team else 6,
                crouch=bool(anchor.get("crouch")),radius=max(20,min(80,round(anchor.get("radius",35)))),
                source_node=anchor["id"],group="NavStudio_"+kind+"_"+source["tag"],
                stationary_support_checked=stationary_support is not None))
    missions = [g for g in goals if g["type"] in ("TCEOBJECTIVE", "CapPoint", "Flag")]
    goals.extend(distributed_tactics(report, missions, goals,stationary_support))
    for node in spread(nodes,allowed=stationary_support.allows if stationary_support is not None else None):
        neighbor = next((nodes[i] for i in outgoing[node["id"]] if i in nodes),node)
        delta = [y-x for x,y in zip(node["position"],neighbor["position"])]
        if not any(delta):delta=[1,0,0]
        length = math.sqrt(sum(x*x for x in delta)) or 1
        goals.append(dict(name="CAMP_NavStudio_"+str(node["id"]),type="CAMP",position=list(node["position"]),
            aim_vector=[x/length for x in delta],teams=6,crouch=bool(node.get("crouch")),
            radius=max(20,min(100,round(node.get("radius",35)))),source_node=node["id"],
            stationary_support_checked=stationary_support is not None))
    route_goals,routes,route_unresolved=door_routes(report,goals)
    corridor_goals,corridors,families=corridor_routes(report,
        [g for g in goals if g["type"] in ("TCEOBJECTIVE","CapPoint","Flag","Attack","Defend")],route_goals,routes)
    route_goals.extend(corridor_goals);routes.extend(corridors)
    goals.extend(route_goals)
    types = {goal["type"] for goal in goals}
    supported_mission = "TCEOBJECTIVE" in types if gametype == 5 else (
        {"Flag","CapPoint"} <= types or "TCEOBJECTIVE" in types)
    if not supported_mission:
        missing.append(dict(reason="missing_native_bomb_sites" if gametype == 5 else "missing_native_ctf_flag_or_capture_point"))
    return dict(gametype=gametype, goals=goals, objectives=source_objectives, unresolved=missing,
        routes=routes,route_goal_count=len(route_goals),route_assignment_count=len(routes),
        corridor_family_count=len(families),corridor_families=families,
        corridor_limits=dict(route_anchors=48,families=64,assignments=128,branches_per_family=2),
        tactical_distribution={str(team):{kind:sum(g["type"]==kind and bool(g.get("teams",6)&(1<<team))
            for g in goals) for kind in ("Defend","Attack")} for team in (1,2)},
        route_unresolved=route_unresolved,route_behavior_live_verified=False,
        tactical_goal_count=sum(g["type"] in ("CAMP","Attack","Defend") for g in goals),
        stationary_support=(stationary_support.summary(goals) if stationary_support is not None else
                            dict(status="unchecked",complete=False,contract=STATIONARY_CONTRACT,
                                 reason="no_explicit_native_stationary_certificate")),
        mission_goal_count=sum(g["type"] in ("TCEOBJECTIVE","CapPoint","Flag") for g in goals),
        mission_geometry_complete=supported_mission and not missing,
        mission_mode=("sabotage" if gametype==2 and {s['operation']for s in source_objectives}=={'plant_defuse'} else
                      "mixed_capture_sabotage" if gametype==2 and any(s['operation']=='plant_defuse'for s in source_objectives) else
                      "capture_or_activation" if gametype==2 else "plant_defuse"),
        mission_entities_detected=len(source_objectives),
        planning_status="mission_geometry_complete_live_pending" if supported_mission and not missing else "mission_geometry_incomplete",
        mission_behavior_complete=False, live_test_status="pending",
        note="Native floor/graph positions are checked; stationary support is reported separately. Capture/plant/defuse completion needs a live match test.")


def tactical_logic(name, gametype, goals):
    tactical=[g for g in goals if g.get("type") in TACTICAL_TIMES]
    if not tactical:return ""
    if any(not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*',g['name']) for g in tactical):
        raise ValueError('Generierter Taktikname ist kein GM-Bezeichner.')
    sectors=[]
    for team in (1,2):
        kinds=[]
        for kind in ("Defend","Attack"):
            candidates={i:dict(id=i,position=g["position"],goal=g["name"]) for i,g in enumerate(tactical)
                        if g["type"]==kind and g.get("teams",6)&(1<<team) and "position" in g}
            ordered=spread(candidates,maximum=len(candidates),spacing=0)
            kinds.append(kind+' = { '+', '.join(json.dumps(n['goal']) for n in ordered)+' },')
        sectors.append('['+str(team)+'] = { '+' '.join(kinds)+' },')
    text=('    Map.NavStudioTactics = { MapName = '+json.dumps(name)+
        ', Gametype = '+str(gametype)+', Goals = { '+
        ' '.join(g['name']+' = '+json.dumps(g['type'])+',' for g in tactical)+' }, Sectors = { '+
        ' '.join(sectors)+' } };\n')
    # Exact generated names, not a wildcard that changes authored goals.
    return text+''.join('    SetGoalPriority('+json.dumps('^'+re.escape(g['name'])+'$')+', '+
        str(TACTICAL_ATTACK_PRIORITY if g['type']=='Attack' else TACTICAL_PRIORITIES[g['type']])+');\n'+
        '    if (Util.SetMaxUsers) { Util.SetMaxUsers(1, '+json.dumps('^'+re.escape(g['name'])+'$')+'); }\n'
        for g in tactical)


def map_logic(name, gametype, token=0, routes=(), goals=()):
    table={}
    for route in routes:
        table.setdefault(route["goal"],{}).setdefault(route["start"],{})[route["end"]]={"Weight":route["weight"]}
    route_text="    MapRoutes =\n    {\n"
    for goal,starts in table.items():
        route_text+="        "+goal+" =\n        {\n"
        for start,ends in starts.items():
            route_text+="            "+start+" = { "+" ".join(end+" = { Weight = "+str(child["Weight"])+", }," for end,child in ends.items())+" },\n"
        route_text+="        },\n"
    route_text+="    };\n    Util.Routes(MapRoutes);\n"
    return (MARKER + "\n// Map-bound TC native actions; no ET engineer weapon substitution.\n"
        "global Map = {};\nglobal OnMapLoad = function()\n{\n"
        '    SetAvailableMapGoals(TEAM.AXIS, false, "PLANT_.*|DEFUSE_.*|BUILD_.*", true);\n'
        '    SetAvailableMapGoals(TEAM.ALLIES, false, "PLANT_.*|DEFUSE_.*|BUILD_.*", true);\n'
        '    SetGoalPriority("TCEOBJECTIVE_.*|FLAG_.*|CAPPOINT_.*", 1.2);\n'
        +tactical_logic(name,gametype,goals)+
        '    SetCvar("omnibot_navstudio_goal_token", ' + str(token) + ');\n'
        '    print("NAVSTUDIO_GOALS ' + name + ' GT' + str(gametype) + ' loaded");\n};\n'
        "global InitializeRoutes = function()\n{\n"+route_text+"};\n")


def generate(studio, name, gametypes=(2, 5), runtime_snapshot=None, runtime_snapshot_evidence=None,
             runtime_snapshots=None, runtime_snapshot_evidences=None,stationary_support=None,map_script_directory=''):
    name = studio.require_map(name)
    if studio.game_mode != "tce2":
        raise ValueError("Automatische TC-Goals benötigen ein TCE2-Projekt. ET verwendet seine eigenen Missionsskripte.")
    active = getattr(studio, "active_project", None)
    if not active: raise ValueError("Zuerst ein Map-Projekt öffnen.")
    gametypes=tuple(dict.fromkeys(gametypes))
    if any(type(gametype)is not int or gametype not in (2,5)for gametype in gametypes):
        raise ValueError("GT2 oder GT5 auswählen.")
    import mapscript
    requested_gametypes=gametypes
    available=mapscript.availability(studio.assets,name,map_script_directory)
    gametypes=tuple(gt for gt in requested_gametypes if gt in available['available'])
    skipped=[gt for gt in requested_gametypes if gt not in gametypes]
    if not gametypes:raise ValueError('Keine ausgewählte Spielart hat ein exaktes GT-Mapscript; keine Goals erzeugt.')
    def unchanged_scripts():
        if mapscript.availability(studio.assets,name,map_script_directory)!=available:
            raise RuntimeError('GT-Mapscripts/Overrides wurden während der Goal-Erstellung geändert; keine Goals übernommen.')
    def validate_snapshot(snapshot,evidence,gametype=None,require_evidence=False):
        if (not isinstance(snapshot,dict) or snapshot.get("map")!=name or
                snapshot.get("gametype") not in gametypes or snapshot.get("gametype") not in (2,5) or
                gametype is not None and snapshot.get("gametype")!=gametype or
                not isinstance(snapshot.get("objective_entities"),list)):
            raise ValueError("Aktueller Goal-Snapshot gehört nicht zur ausgewählten Map/Gametype.")
        if require_evidence and not isinstance(evidence,dict):
            raise ValueError("Für jeden Goal-Gametype ist ein aktueller SHA/Map/GT-Quellbeleg erforderlich.")
        if evidence is not None:
            if not isinstance(evidence,dict):raise ValueError("Ungültiger Goal-Snapshot-Quellbeleg.")
            path=Path(evidence.get("path",""));raw=path.read_bytes()if path.is_file()else None
            if (evidence.get("map")!=name or evidence.get("gametype")!=snapshot["gametype"] or
                    raw is None or hashlib.sha256(raw).hexdigest()!=evidence.get("sha256") or
                    json.loads(raw)!=snapshot):
                raise RuntimeError("Aktuelle Goal-Snapshot-Quelldaten stimmen nicht mit dem SHA/Map/GT-Beleg überein.")
    if runtime_snapshots is not None:
        if (runtime_snapshot is not None or runtime_snapshot_evidence is not None or
                not isinstance(runtime_snapshots,dict) or not isinstance(runtime_snapshot_evidences,dict) or
                set(runtime_snapshots)!=set(gametypes) or set(runtime_snapshot_evidences)!=set(gametypes)):
            raise ValueError("Genau ein aktueller Goal-Snapshot samt Quellbeleg pro ausgewähltem Gametype ist erforderlich.")
        for gametype in gametypes:
            validate_snapshot(runtime_snapshots[gametype],runtime_snapshot_evidences[gametype],gametype,True)
    elif runtime_snapshot_evidences is not None:
        raise ValueError("Goal-Snapshot-Quellbelege benötigen die zugehörigen Gametype-Snapshots.")
    elif runtime_snapshot is not None:
        validate_snapshot(runtime_snapshot,runtime_snapshot_evidence)
    proof = studio.proof_status(name)
    if not proof.get("valid"):
        raise RuntimeError("Goals benötigen die Prüfung des aktuellen Wegnetzes: " + "; ".join(proof.get("reasons", [])))
    report = studio.native(name)
    if stationary_support is not None:
        binding=stationary_binding(name,report,studio.file_hash(studio.native_path(name)),proof["inputs"])
        if isinstance(stationary_support,dict):
            stationary_support=StationarySupport(report,binding,certificate=stationary_support)
        if not isinstance(stationary_support,StationarySupport) or stationary_support.binding!=binding:
            raise RuntimeError("Stationäre Goal-Prüfung gehört nicht zum aktuellen Wegnetz/Modul/Körperprofil.")
    plans, writes = [], {}
    manifest_path = Path(active["paths"]["work"]) / "generated-goals.json"
    previous = json.loads(manifest_path.read_text(encoding="utf8")) if manifest_path.exists() else {}
    for gametype in gametypes:
        if type(gametype) is not int or gametype not in (2, 5): raise ValueError("GT2 oder GT5 auswählen.")
        snapshot_path=studio.native_path(name).parent/(name+"_entities.json")
        fresh_for_gt=runtime_snapshots is not None or runtime_snapshot is not None and runtime_snapshot.get("gametype")==gametype
        snapshot=runtime_snapshots[gametype]if runtime_snapshots is not None else runtime_snapshot if fresh_for_gt else (
            json.loads(snapshot_path.read_text(encoding="utf8")) if snapshot_path.is_file() else None)
        source_evidence=runtime_snapshot_evidences[gametype]if runtime_snapshots is not None else runtime_snapshot_evidence
        source_objectives,source_mode=mission_sources(studio.assets,name,gametype,snapshot)
        plan = make_plan(report, source_objectives, gametype,stationary_support)
        if stationary_support is not None:
            baseline=make_plan(report,source_objectives,gametype)
            def station_counts(value):
                return {str(team):{kind:sum(g["type"]==kind and bool(g.get("teams",6)&(1<<team))
                    for g in value["goals"]) for kind in TACTICAL_TIMES} for team in (1,2)}
            requested=station_counts(baseline);actual=station_counts(plan)
            plan["stationary_support"]["requested_distribution"]=requested
            plan["stationary_support"]["shortfalls"]=[dict(team=team,type=kind,requested=requested[team][kind],actual=actual[team][kind])
                for team in requested for kind in requested[team] if actual[team][kind]<requested[team][kind]]
        plan["mission_entity_source"]=source_mode
        plan["mission_entity_evidence"]=(source_evidence if
            source_mode=="matching_native_runtime_snapshot" and fresh_for_gt else None)
        plan['map_script_source']=available['gametypes'][str(gametype)]['resolved']
        bundles = NavBundles(active["paths"]["bundles"],gametype)
        goal_data = (MARKER + "\nglobal _MG =\n{\n    Version = 1,\n" +
            "".join(render_goal(goal) for goal in plan["goals"]) + "};\n").encode("utf8")
        plan["map_logic_token"] = int(hashlib.sha256(goal_data).hexdigest()[:7],16) or 1
        writes[bundles.path(name,"goals")] = goal_data
        writes[bundles.path(name,"script")] = map_logic(name,gametype,plan["map_logic_token"],plan["routes"],plan["goals"]).encode("utf8")
        plans.append(plan)
    if not plans: raise ValueError("Mindestens einen Gametype auswählen.")
    for path in writes:
        if path.exists() and previous.get("files",{}).get(path.name) != hashlib.sha256(path.read_bytes()).hexdigest():
            raise RuntimeError("Vorhandene bearbeitete/importierte Goals werden nicht überschrieben: " + path.name)
    if runtime_snapshots is not None:
        for gametype in gametypes:
            validate_snapshot(runtime_snapshots[gametype],runtime_snapshot_evidences[gametype],gametype,True)
    stationary_evidence=None
    if stationary_support is not None:
        current=studio.proof_status(name)
        if (not current.get("valid") or current.get("inputs")!=proof["inputs"] or
                stationary_binding(name,studio.native(name),studio.file_hash(studio.native_path(name)),current["inputs"])!=stationary_support.binding):
            raise RuntimeError("Wegnetz oder Prüfgrundlage während stationärer Goal-Prüfung geändert; keine Goals geschrieben.")
        for plan in plans:
            previous_summary=plan["stationary_support"]
            summary=stationary_support.summary(plan["goals"])
            summary.update(requested_distribution=previous_summary["requested_distribution"],shortfalls=previous_summary["shortfalls"])
            summary["complete"]=summary["complete"] and not summary["shortfalls"]
            summary["status"]="complete" if summary["complete"] else "incomplete"
            plan["stationary_support"]=summary
        if stationary_support.finalizer is not None:
            stationary_evidence=stationary_support.finalizer(stationary_support.certificate())
        final_proof=studio.proof_status(name)
        if (not final_proof.get("valid") or final_proof.get("inputs")!=proof["inputs"] or
                stationary_binding(name,studio.native(name),studio.file_hash(studio.native_path(name)),final_proof["inputs"])!=stationary_support.binding):
            raise RuntimeError("Stationäre Goal-Prüfgrundlage vor dem Speichern geändert; keine Goals geschrieben.")
    # Remove only obsolete companions whose exact bytes are still owned by
    # the previous generator manifest. Authored/imported files stay intact.
    pruned=[]
    for gt in(2,5):
        if gt in available['available']:continue
        bundles=NavBundles(active['paths']['bundles'],gt)
        for kind in('goals','script'):
            path=bundles.path(name,kind)
            if path.is_file()and previous.get('files',{}).get(path.name)==hashlib.sha256(path.read_bytes()).hexdigest()and path.read_bytes().startswith(MARKER.encode()):
                pruned.append(path)
    unchanged_scripts()
    created, originals = [], {p:p.read_bytes() for p in set(writes)|set(pruned) if p.exists()}
    try:
        for path, data in writes.items():
            temporary = path.with_name(".goalgen-"+uuid.uuid4().hex+".tmp")
            temporary.write_bytes(data); temporary.replace(path); created.append(path)
        for path in pruned:path.unlink();created.append(path)
        unchanged_scripts()
        manifest = dict(schema=1,project=active["id"],map=name,created=time.time(),
            native_report_sha256=studio.file_hash(studio.native_path(name)), proof_inputs=proof["inputs"],
            runtime_objective_snapshot=runtime_snapshot_evidence,
            runtime_objective_snapshots=runtime_snapshot_evidences,
            stationary_support_certificate=stationary_support.certificate() if stationary_support is not None else None,
            stationary_support_evidence=stationary_evidence,
            plans=plans,files={p.name:hashlib.sha256(data).hexdigest() for p,data in writes.items()},
            map_script_availability=available,requested_gametypes=list(requested_gametypes),
            generated_gametypes=list(gametypes),skipped_unavailable_gametypes=skipped,
            pruned_owned_unavailable_files=[p.name for p in pruned])
        from projects import write_json
        write_json(manifest_path,manifest)
    except Exception:
        for path in created:
            if path in originals: path.write_bytes(originals[path])
            else: path.unlink()
        raise
    return dict(manifest=str(manifest_path),project=active["id"],map=name,plans=plans,
        files=[str(p) for p in writes],installed=False,mission_behavior_complete=False,
        stationary_support_evidence=stationary_evidence,
        generated_gametypes=list(gametypes),skipped_unavailable_gametypes=skipped,map_script_availability=available,
        pruned_owned_unavailable_files=[p.name for p in pruned],
        runtime_objective_snapshots=runtime_snapshot_evidences)


def refresh_generated_tactics(project_directory, gametypes=(2,5), apply_changes=False):
    """Refresh exact generated companions only, without rebuilding any geometry.

    Edited/imported files stop the whole operation. Mission blocks, route code,
    coordinates and explicit nonlegacy dwell times remain byte-for-byte intact.
    """
    from navbundle import goal_overlay
    directory=Path(project_directory).resolve()
    project=json.loads((directory/'project.json').read_text(encoding='utf8'))
    name=project.get('map')
    if project.get('mod')!='tce2' or not isinstance(name,str) or not re.fullmatch(r'[A-Za-z0-9_-]{1,64}',name):
        raise ValueError('Taktik-Refresh benötigt ein benanntes TCE2-Projekt.')
    if not gametypes or any(type(gt)is not int or gt not in (2,5) for gt in gametypes):
        raise ValueError('Taktik-Refresh unterstützt GT2 und GT5.')
    manifest_path=directory/'work/generated-goals.json'
    original_manifest=manifest_path.read_bytes();manifest=json.loads(original_manifest)
    native=directory/'native'/(name+'_navmesh.json')
    if (manifest.get('project')!=project.get('id') or manifest.get('map')!=name or
            manifest.get('native_report_sha256')!=hashlib.sha256(native.read_bytes()).hexdigest()):
        raise RuntimeError('Goal-Projekt bzw. nativer Quellbeleg ist veraltet; keine Dateien geändert.')
    originals={manifest_path:original_manifest};writes={};changes=[]
    for gt in dict.fromkeys(gametypes):
        plan=next((p for p in manifest.get('plans',[]) if p.get('gametype')==gt),None)
        if not plan:raise ValueError('Generierter GT'+str(gt)+'-Goalplan fehlt.')
        bundle=NavBundles(directory/'bundles',gt)
        data={}
        for kind in ('goals','script'):
            path=bundle.path(name,kind);raw=path.read_bytes();originals[path]=raw
            if not raw.startswith(MARKER.encode()) or hashlib.sha256(raw).hexdigest()!=manifest.get('files',{}).get(path.name):
                raise RuntimeError('Bearbeitete/importierte Goal-Datei bleibt unverändert: '+path.name)
            data[kind]=raw.decode('utf8')
        overlay=goal_overlay(data['goals'])
        if overlay.get('error') or overlay.get('skipped') or {g['name'] for g in overlay['goals']}!={g['name'] for g in plan['goals']}:
            raise RuntimeError('Generierte Goal-Literale stimmen nicht mit dem Plan überein.')
        for goal in plan['goals']:
            if goal.get('type') not in TACTICAL_TIMES:continue
            pattern=re.compile(r'(?m)^    '+re.escape(goal['name'])+r' =\r?\n    \{\r?\n([\s\S]*?)^    \},\r?\n')
            matches=list(pattern.finditer(data['goals']))
            if len(matches)!=1:raise RuntimeError('Generierter Taktikblock nicht eindeutig: '+goal['name'])
            match=matches[0];body=match[1];values=[]
            for key in ('MinCampTime','MaxCampTime'):
                found=re.findall(r'(?m)^        '+key+r' = ([0-9.]+),\r?$',body)
                if len(found)!=1:raise RuntimeError('Generierte Verweildauer nicht eindeutig: '+goal['name'])
                values.append(float(found[0]))
            if values==[2,6]:
                minimum,maximum=TACTICAL_TIMES[goal['type']]
                for key,value in (('MinCampTime',minimum),('MaxCampTime',maximum)):
                    body=re.sub(r'(?m)^(        '+key+r' = )[0-9.]+(,\r?)$',lambda m:m[1]+str(value)+m[2],body)
                data['goals']=data['goals'][:match.start(1)]+body+data['goals'][match.end(1):]
                goal.update(min_camp_time=minimum,max_camp_time=maximum)
                changes.append(dict(gametype=gt,goal=goal['name'],previous=[2,6],duration=[minimum,maximum]))
        block=tactical_logic(name,gt,plan['goals'])
        if '\r\n' in data['script']:block=block.replace('\n','\r\n')
        legacy=re.compile(r'    SetGoalPriority\("ATTACK_\.\*\|DEFEND_\.\*", 0\.35\);\r?\n'
                          r'    SetGoalPriority\("CAMP_NavStudio_\.\*", 0\.(?:08|12)\);\r?\n')
        if len(legacy.findall(data['script']))==1:
            data['script']=legacy.sub(lambda _:block,data['script'])
        elif block not in data['script']:
            raise RuntimeError('Generierte Taktiklogik nicht exakt erkannt; keine Dateien geändert.')
        goal_bytes=data['goals'].encode('utf8')
        token=int(hashlib.sha256(goal_bytes).hexdigest()[:7],16) or 1
        old_token=plan.get('map_logic_token')
        token_line='    SetCvar("omnibot_navstudio_goal_token", '+str(old_token)+');'
        if data['script'].count(token_line)!=1:raise RuntimeError('Generierter Goal-Token ist nicht eindeutig.')
        data['script']=data['script'].replace(token_line,'    SetCvar("omnibot_navstudio_goal_token", '+str(token)+');')
        plan['map_logic_token']=token
        for kind,raw in (('goals',goal_bytes),('script',data['script'].encode('utf8'))):
            path=bundle.path(name,kind);manifest['files'][path.name]=hashlib.sha256(raw).hexdigest()
            if raw!=originals[path]:writes[path]=raw
    result=dict(project=project['id'],map=name,changes=changes,files=[str(p) for p in writes],
                applied=False,navigation_regenerated=False,mission_blocks_regenerated=False)
    if not apply_changes or not writes:return result
    if any(path.read_bytes()!=raw for path,raw in originals.items()):
        raise RuntimeError('Goal-Dateien änderten sich während der Prüfung; keine Änderungen übernommen.')
    backup=directory/'work'/('tactics-backup-'+uuid.uuid4().hex)
    backup.mkdir()
    for path,raw in originals.items():(backup/path.name).write_bytes(raw)
    manifest['last_tactical_refresh']=dict(created=time.time(),changes=changes,backup=str(backup))
    writes[manifest_path]=(json.dumps(manifest,ensure_ascii=False,indent=2)+'\n').encode('utf8')
    changed=[]
    try:
        for path,raw in writes.items():
            temporary=path.with_name('.tactics-'+uuid.uuid4().hex+'.tmp')
            try:temporary.write_bytes(raw);temporary.replace(path);changed.append(path)
            finally:temporary.unlink(missing_ok=True)
    except Exception:
        for path in changed:path.write_bytes(originals[path])
        raise
    result.update(applied=True,backup=str(backup))
    return result


def apply(studio, job,map_script_directory=''):
    """Deploy the selected pair and prove its live goal names after one restart.

    This function is called only by an explicit Studio job. The same installation
    is stopped/restarted, because waypoint_load cannot activate a map logic file.
    """
    name = studio.require_map(job["map"])
    active = getattr(studio, "active_project", None)
    if not active: raise ValueError("Zuerst ein Map-Projekt öffnen.")
    gametype = int(job.get("gametype", active["gametype"]))
    if gametype not in (2,5,7): raise ValueError("GT2, GT5 oder GT7 auswählen.")
    if studio.game_mode=='tce2':
        import mapscript
        available=mapscript.require_gametype(studio.assets,name,gametype,map_script_directory)
    selected_gt = gametype if studio.game_mode == "tce2" else None
    source = NavBundles(active["paths"]["bundles"], selected_gt)
    root = studio.test / ("omni-bot" if studio.game_mode == "tce2" else "omnibot") / "et" / "nav"
    destination = NavBundles(root, selected_gt)
    proof = studio.proof_status(name)
    checked_way = studio.native_path(name).with_suffix(".way")
    runtime_way = destination.path(name,"way")
    if (not proof.get("valid") or not checked_way.is_file() or not runtime_way.is_file() or
            studio.file_hash(checked_way) != studio.file_hash(runtime_way)):
        raise RuntimeError("Goals können erst übernommen werden, wenn das aktuell vollständig geprüfte Projekt-Wegnetz im Spiel installiert ist. Zuerst Änderungen ins Spiel übernehmen.")
    pair = {kind:source.path(name,kind) for kind in ("script","goals")}
    if not all(path.is_file() for path in pair.values()):
        raise ValueError("Beide Map-Dateien müssen im Projekt vorhanden sein: .gm und _goals.gm.")
    data = {kind:path.read_bytes() for kind,path in pair.items()}
    if any(len(value)>1024*1024 for value in data.values()): raise ValueError("Goal-Skript überschreitet 1 MiB.")
    generated_manifest_path = Path(active["paths"]["work"])/"generated-goals.json"
    manifest = json.loads(generated_manifest_path.read_text(encoding="utf8")) if generated_manifest_path.exists() else {}
    plan = next((p for p in manifest.get("plans",[]) if p.get("gametype")==gametype),None)
    exact_generated = bool(plan and manifest.get("project")==active["id"] and all(
        manifest.get("files",{}).get(pair[kind].name)==hashlib.sha256(value).hexdigest() for kind,value in data.items()))
    if exact_generated:
        if (not proof.get("valid") or manifest.get("native_report_sha256") != studio.file_hash(studio.native_path(name)) or
                manifest.get("proof_inputs") != proof.get("inputs")):
            raise RuntimeError("Die generierten Goals gehören zu einem früheren Wegnetz. Aktuelle Navigation prüfen und Goals neu erzeugen.")
    if studio.game_mode=='tce2'and mapscript.availability(studio.assets,name,map_script_directory)!=available:
        raise RuntimeError('GT-Mapscripts/Overrides vor der Goal-Übernahme geändert; keine Dateien kopiert.')
    process = studio.own_test_process(require_port=False)
    if process:
        if process.get("owns_port"):
            studio.rcon_with_followup("set omnibot_navstudio_goal_token 0")
        studio.stop_owned_test(job,process)
    root.mkdir(parents=True,exist_ok=True)
    backups, deployed = [], {}
    try:
        for kind,value in data.items():
            target = destination.path(name,kind)
            if target.exists():
                backup_root = root/".navstudio-project-backups"; backup_root.mkdir(exist_ok=True)
                backup = backup_root/(target.name+"."+str(time.time_ns())+".bak")
                with backup.open("xb") as stream: stream.write(target.read_bytes())
                backups.append(dict(target=str(target),backup=str(backup)))
            temporary = target.with_name(".project-goals-"+uuid.uuid4().hex+".tmp")
            try:
                temporary.write_bytes(value); temporary.replace(target)
            finally:
                if temporary.exists(): temporary.unlink()
            digest = hashlib.sha256(value).hexdigest()
            if studio.file_hash(target) != digest: raise RuntimeError("Übernommene Goal-Datei stimmt nicht mit dem Projekt überein.")
            deployed[target.name] = digest
    except Exception:
        # The game is stopped here, so restoring the exact old pair is unambiguous.
        for item in backups: Path(item["target"]).write_bytes(Path(item["backup"]).read_bytes())
        for kind in pair:
            target=destination.path(name,kind)
            if target.name in deployed and not any(item["target"]==str(target) for item in backups): target.unlink()
        raise
    studio.action_load_map(job)
    studio.require_loaded(name)
    actual_gt = studio.live_gametype()
    if actual_gt != gametype: raise RuntimeError(f"Goals wurden nicht bestätigt: aktiver GT{actual_gt}, erwartet GT{gametype}.")
    logic_confirmed = False
    if exact_generated:
        marker_reply = studio.rcon("omnibot_navstudio_goal_token",read_only=True)
        match = re.search(r'\bis:\s*"([0-9]+)', re.sub(r'\^[0-9]','',marker_reply))
        logic_confirmed = bool(match and int(match[1]) == plan["map_logic_token"])
        if not logic_confirmed:
            raise RuntimeError("Goal-Daten installiert, aber die aktuelle .gm-Maplogik wurde vom Spiel nicht ausgeführt.")
    replies = []
    # This job just restarted the map and ran InitializeRoutes. goal_load here
    # would replace ROUTE/CAMP goals without rerunning that initializer, leaving
    # stale route objects or erasing assignments from recreated mission goals.
    for command in ("set omnibot_tce_objective_debug 1", "bot show_goals", "bot draw_goals on"):
        reply = studio.rcon_with_followup(command)
        replies.append(reply)
        studio.update(job,log=(job.get("log","")+"\n"+reply)[-250000:])
    plain = re.sub(r"\^[0-9]","","\n".join(replies))
    if re.search(r"Unknown Goal Type|Goals could not load|compil(?:e|ation) error|script.*exception|unknown action",plain,re.I):
        raise RuntimeError("Goal-Dateien wurden kopiert, aber das Spiel meldet einen Ladefehler: "+plain[-4000:])
    seen = set(re.findall(r"(?:^|\n)\s*\d+:\s+([A-Za-z0-9_]+)",plain))
    expected = {g["name"] for g in plan["goals"]} if exact_generated else set()
    missing = sorted(expected-seen)
    result = dict(schema=1,project=active["id"],map=name,gametype=gametype,created=time.time(),
        files=deployed,backups=backups,generated_bytes_match=exact_generated,
        map_logic_confirmed=logic_confirmed,
        loaded_goal_names=sorted(seen),expected_goal_count=len(expected),missing_goal_names=missing,
        installed=True,live_load_confirmed=not missing and (bool(seen) or bool(
            re.search(r"\b0 Goals Loaded, 0 Goals Deferred\b",plain) and "- Goal List -" in plain)),
        mission_behavior_complete=False)
    from projects import write_json
    receipt = Path(active["paths"]["work"])/("goals-applied-gt"+str(gametype)+".json")
    write_json(receipt,result)
    if not result["live_load_confirmed"]:
        raise RuntimeError("Goal-Dateien installiert, aber aktueller Live-Goalstand nicht vollständig bestätigt. Fehlend: "+
                           (", ".join(missing) if missing else "keine vollständige Goal-Liste empfangen"))
    result["receipt"] = str(receipt)
    return result


if __name__ == '__main__':
    import argparse
    parser=argparse.ArgumentParser(description='Refresh only exact generated TCE2 tactical settings; never control the game.')
    parser.add_argument('--refresh-tactics',type=Path,required=True,metavar='PROJECT_DIRECTORY')
    parser.add_argument('--gametype',type=int,choices=(2,5),action='append')
    parser.add_argument('--apply',action='store_true',help='Write the checked refresh with local backups; default is read-only.')
    args=parser.parse_args()
    print(json.dumps(refresh_generated_tactics(args.refresh_tactics,args.gametype or (2,5),args.apply),ensure_ascii=False,indent=2))
