"""Local versioned navigation drafts. No game, arbitrary paths or WAY writes."""
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import time


def fingerprint(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(",", ":"),
                                     allow_nan=False).encode("utf-8")).hexdigest()


def integer(value, low, high, label):
    if type(value) is not int or not low <= value <= high:
        raise ValueError(label + " außerhalb des zulässigen Bereichs.")
    return value


def nav_flags(value):
    if isinstance(value, str) and re.fullmatch(r"[0-9]{1,20}", value):
        value = int(value)
    return integer(value, 0, 2**64-1, "Flags")


def arrival_radius(value, generated=False, action=False):
    low,high=(1 if generated else 10),(100 if generated or action else 300)
    if type(value) not in (int,float) or not math.isfinite(value) or not low<=value<=high:
        raise ValueError(('Aktionsradius' if generated or action else 'Radius') + f' außerhalb des zulässigen Bereichs {low}–{high}.')
    return value


def action_radius_node(node):
    """Physical recipes keep their smaller tolerance; tactical flags do not."""
    props=node.get('action_properties',{})
    return bool(node.get('generated_guide') or node.get('action_roles') or node.get('editor_role') or
                nav_flags(node.get('nav_flags',0)) & ((1<<9)|(1<<27)|(1<<8)|(1<<18)) or
                node.get('door_recipe') or any(props.get(key) for key in
                ('tce_ascent_release','tce_ascent_alignment','tce_walk_drop','tce_stand_jump','tce_free_climb','tce_door_role')))


def json_flags(value):
    """JSON numbers cannot carry combined ET bits above 2**53 exactly."""
    if isinstance(value, dict):
        return {k: str(v) if k == "nav_flags" else json_flags(v) for k, v in value.items()}
    if isinstance(value, list): return [json_flags(v) for v in value]
    return value


def point(value):
    if not isinstance(value, list) or len(value) != 3 or any(
            type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 131072 for v in value):
        raise ValueError("Fußpunkt benötigt drei endliche Koordinaten zwischen −131072 und 131072.")
    return [round(float(v), 5) for v in value]


def graph_hash(doc):
    return fingerprint({key: doc[key] for key in ("map", "base", "nodes", "edges")})


def waypoint_metadata(node):
    """Bounded literals shared with the optional native Format5/6 tail."""
    name=node.get('name','')
    if not isinstance(name,str) or len(name.encode('utf8'))>255 or any(ord(c)<32 for c in name):
        raise ValueError('Wegpunktname erlaubt höchstens255 UTF8-Bytes ohne Steuerzeichen.')
    facing=node.get('facing',[0,0,0])
    if not isinstance(facing,list) or len(facing)!=3 or any(type(v) not in (int,float) or not math.isfinite(v) for v in facing) or sum(v*v for v in facing)>1.0201:
        raise ValueError('Blickrichtung benötigt einen endlichen Richtungsvektor mit Länge höchstens1.01.')
    uid=node.get('way_guid',0)
    if uid is None:uid=0
    # Omni-bot's path/door/action IDs use positive signed int, even though WAY
    # stores an unsigned word. Preserve only IDs usable by those consumers.
    integer(uid,0,2**31-1,'WAY-UID')
    return name,[float(v) for v in facing],uid


DOOR_INT_FIELDS = ("entity", "use_entity", "target_source_id", "mode", "team", "role",
                   "model", "use_model", "use_source_id", "entry_source_id")
DOOR_REF_FIELDS = {"target_source_id": "tce_door_target", "use_source_id": "tce_door_use_nav",
                   "entry_source_id": "tce_door_entry"}
DOOR_ROLE_NAMES = {1: "use", 2: "entry", 3: "exit"}
RESERVED_WAY_PROPERTIES = {'tce_node','tce_ascent_release','tce_ascent_alignment',
    'tce_walk_drop','tce_walk_drop_target','tce_stand_jump','tce_stand_jump_target',
    'tce_free_climb','tce_free_climb_target',
    *('tce_door_'+key for key in ('entity','use_entity','target','mode','team','role','model','use_model','use_nav','entry','aim'))}


def custom_way_properties(node):
    """Opaque Omni properties; reserved native action data is regenerated."""
    properties=node.get('action_properties',{})
    if not isinstance(properties,dict):raise ValueError('WAY-Eigenschaften benötigen eine String-Tabelle.')
    result={}
    for key,value in properties.items():
        if not isinstance(key,str) or not isinstance(value,str):raise ValueError('WAY-Eigenschaften benötigen String-Schlüssel und String-Werte.')
        if key.startswith('tce_'):
            if key not in RESERVED_WAY_PROPERTIES:raise ValueError('Unbekannte reservierte TCE-WAY-Eigenschaft: '+key)
            continue
        if not key or len(key.encode('utf8'))>127 or len(value.encode('utf8'))>1023 or any(ord(c)<32 for c in key+value):
            raise ValueError('WAY-Eigenschaft: Schlüssel1–127 und Wert0–1023 UTF8-Bytes, ohne Steuerzeichen.')
        result[key]=value
    if len(result)>32:raise ValueError('Höchstens32 benutzerdefinierte WAY-Eigenschaften pro Punkt.')
    return result


def native_way_uid(node):
    uid=node.get('way_guid')
    if node.get('_candidate_recipe_uid') is not None:
        uid=node['_candidate_recipe_uid']
    if uid is None:uid=node.get('index',-1)+1 # Earlier native receipt format.
    return integer(uid,1,2**31-1,'Exportierte WAY-UID')


def candidate_recipe_nodes(native):
    """Plan recipe references for an explicitly incomplete native graph.

    An incomplete import has accepted floor samples but writes no WAY, so its
    way_guid=0 remains correct. This matches NG_AssignOutputUIDs over the complete
    current node array, including reserved imported/rejected identities. The
    transient plan is only used to reconstruct source-ID action roles; final
    native validation/export still supplies the actual WAY identities.
    """
    rows=copy.deepcopy(native.get('nodes',[]))
    for node in rows:node.pop('_candidate_recipe_uid',None)
    if native.get('generation_complete') is not False:
        return {node['id']:node for node in rows}
    need_plan=any(route.get('exported') and (route.get('kind') in ('walk_drop','free_climb') or
        (route.get('kind')=='jump' and route.get('standing_launch'))) for route in native.get('traversal_routes',[])) or any(
        node.get('editor_role',0)&(8|64|128) or (node.get('door_recipe') and node.get('accepted')) for node in rows)
    if not need_plan:return {node['id']:node for node in rows}
    if len(rows)>16384 or any(type(node.get('accepted')) is not bool for node in rows):
        raise ValueError('Unvollständiger nativer Graph überschreitet den tatsächlichen Native-UID-Bereich.')
    accepted=[node for node in rows if node.get('accepted')]
    indices=[node.get('index') for node in accepted]
    if any(type(i) is not int for i in indices) or set(indices)!=set(range(len(accepted))):
        raise ValueError('Unvollständiger nativer Graph hat keine eindeutigen aktuellen Ausgabeindizes.')
    if len({node['id'] for node in rows})!=len(rows):
        raise ValueError('Unvollständiger nativer Graph hat doppelte Quell-IDs.')
    reserved=set()
    for node in rows:
        uid=integer(node.get('import_uid',0),0,2**31-1,'Reservierte Import-UID')
        if uid:
            if uid in reserved:raise ValueError('Doppelte reservierte Import-UID im nativen Graph.')
            reserved.add(uid)
    used=set(reserved);next_uid=1
    for node in accepted:
        uid=node.get('import_uid',0)
        if not uid:
            uid=node['index']+1
            if uid in used:
                while next_uid in used:next_uid+=1
                uid=next_uid;next_uid+=1
            used.add(uid)
        node['_candidate_recipe_uid']=integer(uid,1,2**31-1,'Provisorische Rezept-UID')
    return {node['id']:node for node in rows}


def door_properties(recipe, by_source):
    """Map a native door receipt's source IDs to its exported WAY UIDs."""
    props = {}
    for key in DOOR_INT_FIELDS:
        value = recipe.get(key, -1 if key.endswith("source_id") else 0)
        if key in DOOR_REF_FIELDS:
            if value < 0:
                props[DOOR_REF_FIELDS[key]] = "0"
            else:
                target = by_source.get(value)
                if target is None or not target.get("accepted") or target.get("index", -1) < 0:
                    raise ValueError("Türrezept verweist auf einen nicht exportierten Wegpunkt.")
                props[DOOR_REF_FIELDS[key]] = str(native_way_uid(target))
        else:
            props["tce_door_" + key] = str(value)
    props["tce_door_aim"] = " ".join("%.5f" % v for v in point(recipe["aim"]))
    return props


def door_role(props, by_uid):
    """Keep graph references separate from changing WAY export indices."""
    if not props.get("tce_door_role"):
        return None
    role = integer(int(props["tce_door_role"]), 1, 3, "Türrolle")
    result = dict(kind="door", role=DOOR_ROLE_NAMES[role])
    for key, prop in DOOR_REF_FIELDS.items():
        uid = int(props.get(prop, "0"))
        target = by_uid.get(uid) if uid else None
        if uid and target is None:
            raise ValueError("Türrezept verweist auf einen fehlenden WAY-Punkt.")
        result[key.replace("source_id", "node")] = target
    return result


def door_candidate(node, ids, edges):
    """Format6: ten bounded integer fields, then one absolute aim position."""
    props = node.get("action_properties", {})
    role = next((r for r in node.get("action_roles", []) if r.get("kind") == "door"), None)
    if not props.get("tce_door_role"):
        if role:
            raise ValueError("Türrolle benötigt das zugehörige Türrezept.")
        return [0] * 13
    if role is None:
        raise ValueError("Türrezept benötigt seine aktuellen Wegpunktverweise.")
    result = []
    for key in DOOR_INT_FIELDS:
        if key in DOOR_REF_FIELDS:
            value = role.get(key.replace("source_id", "node"))
            if value is not None and value not in ids:
                raise ValueError("Türrezeptziel wurde gelöscht.")
            result.append(0 if value is None else value + 1)
        else:
            bounds = {"entity": (64, 1021), "use_entity": (64, 1021), "mode": (1, 3),
                      "team": (1, 3), "role": (1, 3), "model": (1, 255), "use_model": (1, 255)}
            low, high = bounds[key]
            value = integer(int(props["tce_door_" + key]), low, high, "Türrezept " + key)
            result.append(value + 1 if key in ("entity", "use_entity") else value)
    if DOOR_ROLE_NAMES[result[5]] != role["role"]:
        raise ValueError("Türrolle und Rezept widersprechen sich.")
    if result[5] in (1, 2):
        target = role.get("target_node")
        if target is None or (node["id"], target) not in edges:
            raise ValueError("Türaktion benötigt ihre gerichtete Anschlussverbindung.")
    if role.get("use_node") is None or role.get("entry_node") is None:
        raise ValueError("Türrezept benötigt Aktivierungs- und Anlaufpunkt.")
    aim = props.get("tce_door_aim", "").split()
    result.extend(point([float(v) for v in aim]))
    return result


def remove_broken_door_recipes(doc):
    """Deleting a recipe endpoint cannot leave stale native/WAY references."""
    ids = {n["id"] for n in doc["nodes"]}
    edges = {(e["from"], e["to"]) for e in doc["edges"]}
    invalid = set()
    def group(node, role):
        return (node.get("action_properties", {}).get("tce_door_entity"), role.get("entry_node"))
    for n in doc["nodes"]:
        for role in n.get("action_roles", []):
            if role.get("kind") != "door":
                continue
            refs = [role.get(key) for key in ("target_node", "use_node", "entry_node")]
            if (any(ref is not None and ref not in ids for ref in refs) or
                role["role"] in ("use", "entry") and
                ((n["id"], role.get("target_node")) not in edges or not n["nav_flags"] & 128)):
                invalid.add(group(n, role))
    changed = []
    for n in doc["nodes"]:
        if not any(r.get("kind") == "door" and group(n, r) in invalid for r in n.get("action_roles", [])):
            continue
        updated = copy.deepcopy(n)
        updated["action_roles"] = [r for r in updated["action_roles"] if r.get("kind") != "door"]
        updated["action_properties"] = {k:v for k,v in updated.get("action_properties", {}).items() if not k.startswith("tce_door_")}
        updated["nav_flags"] &= ~128
        if not updated["action_roles"] and not updated["nav_flags"] & ((1<<9)|(1<<27)|(1<<8)|(1<<18)):
            updated["generated_guide"] = False
            updated["radius"] = max(10, updated["radius"])
        changed.append(updated)
    return changed


def action_protection(native):
    """Preserve input anchors, omit regenerated guides/action links in Format3."""
    by_id = candidate_recipe_nodes(native)
    anchors, guides, edges, roles = set(), set(), set(), {}
    properties={n['id']:custom_way_properties(dict(action_properties=n.get('custom_properties',{})))
                for n in native.get('nodes',[]) if n.get('custom_properties')}
    for route in native.get("traversal_routes", []):
        if not route.get("exported"):
            continue
        anchors.update(route[key] for key in ("low", "high", "approach", "landing")
                       if type(route.get(key)) is int and route[key] >= 0)
        if route.get("kind") == "free_climb":
            launch, landing = route.get("launch"), route.get("landing")
            if launch not in by_id or landing not in by_id:
                raise ValueError("Freikletter-Landeziel fehlt im aktuellen nativen Beleg.")
            properties.setdefault(launch, {}).update(tce_free_climb="1", tce_free_climb_target=str(native_way_uid(by_id[landing])))
            roles.setdefault(launch, []).append(dict(kind="free_climb", role="launch", landing_node=landing))
            guides.add(launch); edges.add((launch, landing))
        if route.get("kind")=="jump" and route.get("standing_launch"):
            launch,landing=route.get("launch"),route.get("landing")
            if launch not in by_id or landing not in by_id:
                raise ValueError("Standing jump target missing from current native proof.")
            properties.setdefault(launch,{}).update(tce_stand_jump="1",tce_stand_jump_target=str(native_way_uid(by_id[landing])))
            roles.setdefault(launch,[]).append(dict(kind="standing_jump",role="launch",landing_node=landing))
            guides.add(launch);edges.add((launch,landing))
        if route.get("kind") != "walk_drop":
            continue
        for key in ("approach", "launch", "landing", "paired_jump_launch"):
            identity = route.get(key)
            if type(identity) is not int or identity < 0 or identity not in by_id or not by_id[identity].get("accepted"):
                raise ValueError("Unvollständiges walk_drop-Rezept; vor Bearbeitung neu nativ prüfen.")
            roles.setdefault(identity, []).append({"kind": "walk_drop", "role": key})
        launch, landing = route["launch"], route["landing"]
        target_index = by_id[landing].get("index")
        if type(target_index) is not int or target_index < 0:
            raise ValueError("walk_drop-Landeziel hat keine exportierte WAY-ID.")
        target_uid=native_way_uid(by_id[landing])
        properties.setdefault(launch,{}).update(tce_walk_drop="1",tce_walk_drop_target=str(target_uid))
        next(role for role in reversed(roles[launch]) if role["role"] == "launch").update(
            landing_node=landing, landing_way_uid=target_uid)
        # A generated drop start can also be the outward jump's landing.
        # Only native_input proves it was an original user/input anchor.
        (anchors if isinstance(by_id[launch].get("native_input"), dict) else guides).add(launch)
        if route["approach"] != launch:
            edges.add((route["approach"], launch))
        edges.add((launch, landing))
        if route["paired_jump_launch"] == launch:
            if (native.get("candidate_mode") != "manual_draft" or
                not by_id[launch].get("editor_role", 0) & 8 or
                by_id[launch].get("editor_drop_target") != landing):
                raise ValueError("Einbahn-Abstieg benötigt sein aktuell geprüftes gerichtetes Rezept.")
            continue
        paired = [r for r in native.get("traversal_routes", []) if r.get("kind") == "jump"
                  and r.get("exported") and r.get("launch") == route["paired_jump_launch"]]
        if len(paired) != 1:
            raise ValueError("walk_drop benötigt genau ein exportiertes Gegen-Sprungrezept.")
        paired = paired[0]
        edges.update(((paired["approach"], paired["launch"]), (paired["launch"], paired["landing"])))
    for node in native.get("nodes", []):
        role = node.get("editor_role", 0)
        identity = node["id"]
        recipe = node.get("door_recipe")
        if recipe and node.get("accepted"):
            props = door_properties(recipe, by_id)
            properties.setdefault(identity, {}).update(props)
            roles.setdefault(identity, []).append(door_role(props, {native_way_uid(n): n["id"]
                                                                   for n in by_id.values() if n.get("accepted")}))
            guides.add(identity)
            target = recipe.get("target_source_id", -1)
            if recipe.get("role") in (1, 2) and target >= 0:
                edges.add((identity, target))
        if role & 1: guides.add(identity)
        if role & 2: properties.setdefault(identity, {})["tce_ascent_release"] = "1"
        if role & 4: properties.setdefault(identity, {})["tce_ascent_alignment"] = "1"
        if role & 64:
            landing=node.get("editor_drop_target")
            if landing not in by_id: raise ValueError("Standing jump landing target missing.")
            properties.setdefault(identity,{}).update(tce_stand_jump="1",tce_stand_jump_target=str(native_way_uid(by_id[landing])))
            roles.setdefault(identity,[]).append(dict(kind="standing_jump",role="launch",landing_node=landing))
        if role & 128:
            landing = node.get("editor_drop_target")
            if landing not in by_id: raise ValueError("Freikletter-Landeziel fehlt.")
            properties.setdefault(identity, {}).update(tce_free_climb="1", tce_free_climb_target=str(native_way_uid(by_id[landing])))
            if not any(r.get("kind") == "free_climb" and r.get("landing_node") == landing for r in roles.get(identity, [])):
                roles.setdefault(identity, []).append(dict(kind="free_climb", role="launch", landing_node=landing))
        if role & 8:
            landing = node.get("editor_drop_target")
            if landing not in by_id: raise ValueError("Abstieg-Landeziel fehlt.")
            properties.setdefault(identity, {}).update(tce_walk_drop="1", tce_walk_drop_target=str(native_way_uid(by_id[landing])))
            roles.setdefault(identity, []).append(dict(kind="walk_drop", role="launch", landing_node=landing))
    return {"protected_anchor_ids": sorted(anchors), "generated_guide_ids": sorted(guides),
            "protected_action_edges": [list(e) for e in sorted(edges)],
            "action_roles": roles, "action_properties": properties}


class Drafts:
    def __init__(self, directory):
        self.directory = Path(directory).resolve()
        self.documents = {}
        self.disk_hashes = {}

    def path(self, name):
        if not isinstance(name, str) or not re.fullmatch(r"[A-Za-z0-9_-]{1,64}", name):
            raise ValueError("Ungültiger Entwurfsmapname.")
        target = (self.directory / (name + ".draft.json")).resolve()
        if target.parent != self.directory:
            raise ValueError("Entwurfspfad verlässt den lokalen Projektordner.")
        return target

    def get(self, name):
        path = self.path(name)
        if name not in self.documents and path.is_file():
            if path.stat().st_size > 32 * 1024 * 1024:
                raise ValueError("Entwurf überschreitet das Größenlimit.")
            wrapper = json.loads(path.read_text(encoding="utf-8"))
            doc = wrapper.get("draft")
            if not isinstance(doc, dict) or wrapper.get("sha256") != fingerprint(doc):
                raise ValueError("Entwurfsdatei wurde außerhalb des Editors verändert oder ist beschädigt.")
            self.validate(doc, name)
            for node in doc["nodes"]: node["locked"] = False
            self.documents[name] = doc
            self.disk_hashes[name] = hashlib.sha256(path.read_bytes()).hexdigest()
        return self.documents.get(name)

    def validate(self, doc, name):
        if doc.get("schema") != 1 or doc.get("map") != name or doc.get("mode") != "manual_draft":
            raise ValueError("Entwurfsschema oder Map stimmt nicht überein.")
        integer(doc.get("revision"), 0, 2**31-1, "Revision")
        if not isinstance(doc.get("base"), dict) or not isinstance(doc["base"].get("environment"), dict):
            raise ValueError("Entwurfsherkunft fehlt.")
        if not 0 <= len(doc.get("nodes", [])) <= 15872 or len(doc.get("edges", [])) > 253952:
            raise ValueError("Entwurf erlaubt höchstens 15872 Punkte und 253952 gerichtete Verbindungen.")
        ids,way_uids = set(),set()
        custom_count=custom_bytes=0
        for n in doc["nodes"]:
            identity = integer(n.get("id"), 0, 2**31-1, "Punkt-ID")
            if identity in ids:
                raise ValueError("Doppelte Punkt-ID im Entwurf.")
            ids.add(identity)
            point(n.get("position"))
            _name,_facing,uid=waypoint_metadata(n)
            custom=custom_way_properties(n);custom_count+=len(custom)
            custom_bytes+=sum(len(k.encode('utf8'))+len(v.encode('utf8'))+2 for k,v in custom.items())
            if uid:
                if uid in way_uids:raise ValueError('Doppelte WAY-UID im Entwurf.')
                way_uids.add(uid)
            if type(n.get("locked")) is not bool or type(n.get("generated_guide")) is not bool:
                raise ValueError("Ungültiger Aktionspunktschutz.")
            nav_flags(n.get("nav_flags"))
            arrival_radius(n.get('radius'),n['generated_guide'],action_radius_node(n))
            if not n["generated_guide"]:
                if n.get("crouch") != bool(n["nav_flags"] & 64):
                    raise ValueError("Ducken und Flagmaske widersprechen sich.")
        if custom_count>4096 or custom_bytes>1024*1024:raise ValueError('Benutzerdefinierte WAY-Eigenschaften überschreiten4096 Paare/1MiB pro Netz.')
        pairs = set()
        for e in doc["edges"]:
            pair = (e.get("from"), e.get("to"))
            if pair[0] not in ids or pair[1] not in ids or pair[0] == pair[1] or pair in pairs:
                raise ValueError("Ungültige, doppelte oder fehlende Verbindungsendpunkte.")
            pairs.add(pair)
        if len(doc.get("undo", [])) > 64 or len(doc.get("redo", [])) > 64:
            raise ValueError("Entwurfshistorie überschreitet das Limit.")

    def start(self, name, navigation, base):
        existing = self.get(name)
        if existing:
            return existing
        nodes = []
        for source in navigation.get("nodes", []):
            if source.get("accepted") is False:
                continue
            flags = int(source.get("nav_flags", 0)) | (64 if source.get("crouch") else 0)
            generated = bool(source.get("editor_role", 0) & 1) or bool(flags & ((1<<9)|(1<<27)|(1<<8)|(1<<18))) or source["id"] in base.get("generated_guide_ids", [])
            locked = False
            old_radius = source.get("radius", 35)
            radius = old_radius
            if not generated and old_radius < 10:
                radius = 35
            nodes.append(dict(id=source["id"], position=point(source.get("position", source.get("point"))),
                              radius=radius, source_radius=old_radius, nav_flags=flags,
                              crouch=bool(flags & 64), spawn=bool(source.get("spawn") or source.get("reason") == "map_entity_spawn_seed"),
                              topology_protected=bool(source.get("topology_protected")), locked=locked, generated_guide=generated,
                              source_reason=source.get("reason", source.get("kind", "")), editor_role=source.get("editor_role", 0),
                              action_roles=copy.deepcopy(source.get("action_roles", [])),
                              action_properties=copy.deepcopy(source.get("action_properties", {}))))
            for key in ('name','facing','way_guid'):
                if key in source:nodes[-1][key]=copy.deepcopy(source[key])
            if source.get('way_guid')==0 and source.get('import_uid'):
                # Requested source identity survives an incomplete import,
                # while the report continues to claim no exported WAY UID.
                nodes[-1]['way_guid']=integer(source['import_uid'],1,2**31-1,'Reservierte Import-UID')
        ids = {n["id"] for n in nodes}
        edges = sorted({(e["from"], e["to"]) for e in navigation.get("edges", [])
                        if e.get("accepted") is not False and e["from"] in ids and e["to"] in ids})
        doc = dict(schema=1, mode="manual_draft", map=name, revision=0, saved_revision=None,
                   created=time.time(), base=copy.deepcopy(base), nodes=nodes,
                   edges=[dict(zip(("from", "to"), pair)) for pair in edges],
                   next_id=max(ids, default=-1)+1, undo=[], redo=[], export=None)
        self.validate(doc, name)
        self.documents[name] = doc
        return doc

    def view(self, name, environment):
        doc = self.get(name)
        if not doc:
            return None
        stale = doc["base"]["environment"] != environment
        return {key: copy.deepcopy(doc[key]) for key in
                ("schema", "mode", "map", "revision", "saved_revision", "base", "nodes", "edges", "export")} | {
                    "sha256": graph_hash(doc), "stale": stale, "undo_count": len(doc["undo"]),
                    "redo_count": len(doc["redo"]), "path": str(self.path(name)),
                    "radius_migrations": sum(not n["generated_guide"] and n.get("source_radius", 35) < 10 for n in doc["nodes"])}

    def require(self, name, revision, environment):
        doc = self.get(name)
        if not doc:
            raise ValueError("Zuerst einen Bearbeitungsentwurf anlegen.")
        if doc["base"]["environment"] != environment:
            raise RuntimeError("Die BSP-/Geometriequelle des Entwurfs wurde geändert. Entwurf exportieren/sichern und auf aktueller Grundlage neu beginnen.")
        if type(revision) is not int or revision != doc["revision"]:
            raise RuntimeError("Entwurfsrevision ist veraltet. Ansicht aktualisieren; die Änderung wurde nicht angewendet.")
        return doc

    @staticmethod
    def apply(doc, patch, forward=True):
        before, after = ("before", "after") if forward else ("after", "before")
        changed = {n["id"] for n in patch[before] + patch[after]}
        nodes = [n for n in doc["nodes"] if n["id"] not in changed] + copy.deepcopy(patch[after])
        edges = {(e["from"], e["to"]) for e in doc["edges"]}
        remove, add = ("removed", "added") if forward else ("added", "removed")
        edges.difference_update(map(tuple, patch[remove]))
        edges.update(map(tuple, patch[add]))
        doc["nodes"] = sorted(nodes, key=lambda n: n["id"])
        doc["edges"] = [dict(zip(("from", "to"), e)) for e in sorted(edges)]

    @staticmethod
    def adopt_way_identities(doc, identities):
        """Pin certified native UIDs only as part of an actual graph mutation.

        History copies need the same pin: undoing a deletion must not recreate
        an old zero-UID snapshot and silently renumber the restored graph.
        The caller supplies a source-bound, complete native identity map.
        """
        if not identities:return
        if not isinstance(identities,dict):raise ValueError('Native WAY-UID-Zuordnung fehlt.')
        used=set()
        for identity,uid in identities.items():
            integer(identity,0,2**31-1,'Native Punkt-ID')
            integer(uid,1,2**31-1,'Native WAY-UID')
            if uid in used:raise ValueError('Doppelte native WAY-UID.')
            used.add(uid)
        # New patches can still refer to nodes in the original document.
        # Detach history before pinning, including on a later rejected change.
        doc['undo']=copy.deepcopy(doc['undo']);doc['redo']=copy.deepcopy(doc['redo'])
        groups=[doc['nodes']]
        for history in ('undo','redo'):
            for patch in doc[history]:groups.extend((patch['before'],patch['after']))
        for nodes in groups:
            for node in nodes:
                uid=identities.get(node['id'])
                if uid is None:continue # New, not yet certified points retain UID0.
                old=waypoint_metadata(node)[2]
                if old and old!=uid:raise ValueError('Native WAY-UID widerspricht der erhaltenen Punktidentität.')
                node['way_guid']=uid

    def change(self, name, payload, environment, identity_provider=None):
        doc = self.require(name, payload.get("revision"), environment)
        action = payload.get("op")
        result = copy.deepcopy(doc)
        if action in ("undo", "redo"):
            source, target = ("undo", "redo") if action == "undo" else ("redo", "undo")
            if not result[source]:
                raise ValueError("Keine Änderung zum " + ("Rückgängigmachen." if action == "undo" else "Wiederholen."))
            patch = result[source].pop()
            self.apply(result, patch, forward=action == "redo")
            result[target].append(patch)
        else:
            by_id = {n["id"]: n for n in doc["nodes"]}
            patch = dict(before=[], after=[], removed=[], added=[], op=action)
            if action == "add_node":
                node = dict(id=result["next_id"], position=point(payload.get("position")),
                            radius=arrival_radius(payload.get("radius",35)),
                            nav_flags=nav_flags(payload.get("nav_flags", 0)), spawn=False,
                            topology_protected=True, locked=False, generated_guide=False, source_reason="manual_added")
                node["crouch"] = bool(node["nav_flags"] & 64)
                patch["after"] = [node]
                result["next_id"] += 1
            elif action in ("update_node", "delete_node"):
                node = by_id.get(integer(payload.get("id"), 0, 2**31-1, "Punkt-ID"))
                if not node:
                    raise ValueError("Punkt fehlt oder ist ein gesperrter Generator-Aktionspunkt.")
                patch["before"] = [node]
                if action == "delete_node":
                    patch["removed"] = [(e["from"], e["to"]) for e in doc["edges"] if node["id"] in (e["from"], e["to"])]
                    # Deleted landing targets must not leave invisible drop
                    # properties that keep referring to a removed point.
                    for other in doc["nodes"]:
                        roles=other.get("action_roles",[])
                        if other["id"]==node["id"] or not any(r.get("landing_node")==node["id"] for r in roles):
                            continue
                        updated=copy.deepcopy(other)
                        updated["action_roles"]=[r for r in roles if r.get("landing_node")!=node["id"]]
                        for kind,prefix,mask in (("walk_drop","tce_walk_drop",8),("standing_jump","tce_stand_jump",64),("free_climb","tce_free_climb",128)):
                            if any(r.get("kind")==kind and r.get("landing_node")==node["id"] for r in roles):
                                updated["action_properties"].pop(prefix,None)
                                updated["action_properties"].pop(prefix+"_target",None)
                                updated["editor_role"]=updated.get("editor_role",0)&~mask
                                if kind=="standing_jump": updated["nav_flags"]&=~((1<<27)|(1<<18)|(1<<8))
                                if kind=="free_climb": updated["nav_flags"]&=~(1<<9)
                        patch["before"].append(other);patch["after"].append(updated)
                else:
                    updated = copy.deepcopy(node)
                    if "position" in payload:
                        updated["position"] = point(payload["position"])
                    if "radius" in payload:
                        updated["radius"] = arrival_radius(payload["radius"],node.get('generated_guide',False),action_radius_node(node))
                    for key in ('name','facing'):
                        if key in payload:updated[key]=copy.deepcopy(payload[key])
                    if 'facing' in payload and payload['facing']!=node.get('facing',[0,0,0]) and (node.get('generated_guide') or node.get('action_roles')):
                        raise ValueError('Aktions-Blickrichtungen werden aus dem geprüften Leiter-/Sprung-/Türrezept erzeugt.')
                    waypoint_metadata(updated)
                    if 'properties' in payload:
                        custom=custom_way_properties(dict(action_properties=payload['properties']))
                        if len(custom)!=len(payload['properties']):raise ValueError('Reservierte TCE-Aktionswerte nur über die geprüften Aktionsrezepte bearbeiten.')
                        updated['action_properties']={k:v for k,v in updated.get('action_properties',{}).items() if k.startswith('tce_')}
                        updated['action_properties'].update(custom)
                    if "nav_flags" in payload:
                        updated["nav_flags"] = nav_flags(payload["nav_flags"])
                        updated["crouch"] = bool(updated["nav_flags"] & 64)
                    # A single inspector save can replace one action type
                    # with another. Remove explicitly cleared recipes first
                    # so evaluation order cannot reject the replacement.
                    for field, kind, prefix, mask, flag_mask in (
                            ("standing_jump_target", "standing_jump", "tce_stand_jump", 64, (1<<27)|(1<<18)|(1<<8)),
                            ("free_climb_target", "free_climb", "tce_free_climb", 128, 1<<9)):
                        if field in payload and payload[field] is None:
                            had_recipe = any(r.get("kind") == kind for r in updated.get("action_roles", []))
                            updated["action_roles"] = [r for r in updated.get("action_roles", []) if r.get("kind") != kind]
                            for key in (prefix, prefix + "_target"): updated["action_properties"].pop(key, None)
                            updated["editor_role"] = updated.get("editor_role", 0) & ~mask
                            if had_recipe: updated["nav_flags"] &= ~flag_mask
                    if "standing_jump_target" in payload:
                        target=payload["standing_jump_target"]
                        updated["action_roles"]=[r for r in updated.get("action_roles",[]) if r.get("kind")!="standing_jump"]
                        for key in ("tce_stand_jump","tce_stand_jump_target"):
                            updated["action_properties"].pop(key,None)
                        updated["editor_role"]=updated.get("editor_role",0)&~64
                        if target is None and any(r.get("kind") == "standing_jump" for r in node.get("action_roles", [])):
                            updated["nav_flags"]&=~((1<<27)|(1<<18)|(1<<8))
                        if target is not None:
                            target=integer(target,0,2**31-1,"Standing jump target")
                            if target not in by_id or target==node["id"]:
                                raise ValueError("Standing jump needs a distinct existing landing point.")
                            if not any(e["from"]==node["id"] and e["to"]==target for e in doc["edges"]):
                                raise ValueError("Connect the directed standing-jump landing route first.")
                            if any(r.get("kind") in ("walk_drop", "free_climb") and r.get("role")=="launch" for r in updated["action_roles"]):
                                raise ValueError("One launch cannot have both standing jump and walk-drop recipes.")
                            updated["action_properties"].update(tce_stand_jump="1")
                            updated["action_roles"].append(dict(kind="standing_jump",role="launch",landing_node=target))
                            if updated["nav_flags"]&((1<<6)|(1<<9)|(1<<20)):
                                raise ValueError("Standing jump requires standing posture without CLIMB or PRONE.")
                            updated["nav_flags"]|=1<<27;updated["generated_guide"]=True
                            updated["radius"]=2;updated["editor_role"]|=64
                    if "free_climb_target" in payload:
                        target = payload["free_climb_target"]
                        updated["action_roles"] = [r for r in updated.get("action_roles", []) if r.get("kind") != "free_climb"]
                        for key in ("tce_free_climb", "tce_free_climb_target"):
                            updated["action_properties"].pop(key, None)
                        updated["editor_role"] = updated.get("editor_role", 0) & ~128
                        if target is None and any(r.get("kind") == "free_climb" for r in node.get("action_roles", [])):
                            updated["nav_flags"] &= ~(1 << 9)
                        if target is not None:
                            target = integer(target, 0, 2**31-1, "Freikletter-Landeziel")
                            if target not in by_id or target == node["id"]:
                                raise ValueError("Freiklettern benötigt einen anderen vorhandenen Landepunkt.")
                            if not any(e["from"] == node["id"] and e["to"] == target for e in doc["edges"]):
                                raise ValueError("Zuerst die gerichtete Freikletter-Verbindung zum Landepunkt erstellen.")
                            if any(r.get("kind") in ("walk_drop", "standing_jump") and r.get("role") == "launch" for r in updated["action_roles"]):
                                raise ValueError("Freiklettern und Sprung/Abstieg benötigen getrennte Startpunkte.")
                            if updated["nav_flags"] & ((1 << 6) | (1 << 20) | (1 << 27) | (1 << 18) | (1 << 8)):
                                raise ValueError("Freiklettern benötigt stehende Haltung ohne Sprung- oder Liegeflags.")
                            updated["action_properties"]["tce_free_climb"] = "1"
                            updated["action_roles"].append(dict(kind="free_climb", role="launch", landing_node=target))
                            updated["nav_flags"] |= 1 << 9; updated["generated_guide"] = True
                            updated["radius"] = 2; updated["editor_role"] |= 128
                    old_fields,new_fields=dict(node),dict(updated)
                    for key,default in (('name',''),('facing',[0,0,0]),('way_guid',0)):
                        old_fields.setdefault(key,default);new_fields.setdefault(key,default)
                    if old_fields==new_fields:updated=copy.deepcopy(node)
                    else:updated["topology_protected"] = True
                    patch["after"] = [updated]
            elif action in ("link", "unlink"):
                a, b = (integer(payload.get(key), 0, 2**31-1, "Punkt-ID") for key in ("from", "to"))
                if a == b or any(i not in by_id for i in (a, b)):
                    raise ValueError("Zwei verschiedene gewöhnliche Wegpunkte auswählen; Aktionsverbindungen bleiben geschützt.")
                pairs = [(a, b)] + ([(b, a)] if payload.get("bidirectional") is True else [])
                existing = {(e["from"], e["to"]) for e in doc["edges"]}
                patch["added" if action == "link" else "removed"] = [e for e in pairs if (e not in existing) == (action == "link")]
                if not patch["added"] and not patch["removed"]:
                    raise ValueError("Verbindung ist bereits in diesem Zustand.")
                if action=="unlink":
                    for source,target in patch["removed"]:
                        other=by_id[source];roles=other.get("action_roles",[])
                        if not any(r.get("role")=="launch" and r.get("landing_node")==target for r in roles):continue
                        updated=copy.deepcopy(other)
                        updated["action_roles"]=[r for r in roles if not(r.get("role")=="launch" and r.get("landing_node")==target)]
                        for kind,prefix,mask in (("walk_drop","tce_walk_drop",8),("standing_jump","tce_stand_jump",64),("free_climb","tce_free_climb",128)):
                            if any(r.get("kind")==kind and r.get("landing_node")==target for r in roles):
                                updated["action_properties"].pop(prefix,None);updated["action_properties"].pop(prefix+"_target",None)
                                updated["editor_role"]=updated.get("editor_role",0)&~mask
                                if kind=="standing_jump": updated["nav_flags"]&=~((1<<27)|(1<<18)|(1<<8))
                                if kind=="free_climb": updated["nav_flags"]&=~(1<<9)
                        patch["before"].append(other);patch["after"].append(updated)
            else:
                raise ValueError("Unbekannte Bearbeitungsoperation.")
            proposed = copy.deepcopy(doc)
            self.apply(proposed, patch)
            for updated in remove_broken_door_recipes(proposed):
                if not any(n["id"] == updated["id"] for n in patch["before"]):
                    patch["before"].append(by_id[updated["id"]])
                patch["after"] = [n for n in patch["after"] if n["id"] != updated["id"]] + [updated]
            patch["removed"] = [list(pair) for pair in patch["removed"]]
            patch["added"] = [list(pair) for pair in patch["added"]]
            self.apply(result, patch)
            result["undo"] = (result["undo"] + [patch])[-64:]
            result["redo"] = []
        if graph_hash(result)==graph_hash(doc):return doc
        if identity_provider:self.adopt_way_identities(result,identity_provider())
        result["revision"] += 1
        result["export"] = None
        self.validate(result, name)
        self.documents[name] = result
        return result

    def save(self, name, revision, environment):
        doc = self.require(name, revision, environment)
        self.validate(doc, name)
        self.directory.mkdir(parents=True, exist_ok=True)
        target = self.path(name)
        current = hashlib.sha256(target.read_bytes()).hexdigest() if target.is_file() else None
        if current != self.disk_hashes.get(name):
            raise RuntimeError("Entwurfsdatei wurde außerhalb dieser Sitzung geändert. Kein Überschreiben; aktuellen Entwurf als JSON sichern.")
        doc["saved_revision"] = revision
        temporary = target.with_suffix(".json.tmp")
        temporary.write_text(json.dumps({"sha256": fingerprint(doc), "draft": doc},
                                        separators=(",", ":"), allow_nan=False), encoding="utf-8")
        temporary.replace(target)
        self.disk_hashes[name] = hashlib.sha256(target.read_bytes()).hexdigest()
        return doc

    def apply_repair(self, name, revision, environment, proposal, identity_provider=None):
        doc=self.require(name,revision,environment)
        result=copy.deepcopy(doc)
        old={n["id"]:n for n in doc["nodes"]};new={n["id"]:n for n in proposal["nodes"]}
        if set(old)-set(new):
            raise ValueError("Automatische Reparatur darf keine vorhandenen Punkte löschen.")
        changed={i for i in new if old.get(i)!=new[i]}
        before={(e["from"],e["to"]) for e in doc["edges"]}
        after={(e["from"],e["to"]) for e in proposal["edges"]}
        if not changed and before==after:return doc
        patch=dict(op="automatic_repair",before=[old[i] for i in sorted(changed) if i in old],
                   after=[new[i] for i in sorted(changed)],removed=[list(p) for p in sorted(before-after)],
                   added=[list(p) for p in sorted(after-before)])
        self.apply(result,patch)
        result["next_id"]=max(doc["next_id"],proposal["next_id"])
        result["revision"]+=1;result["export"]=None
        result["undo"]=(result["undo"]+[patch])[-64:];result["redo"]=[]
        if identity_provider:self.adopt_way_identities(result,identity_provider())
        self.validate(result,name);self.documents[name]=result
        return result

    def export_candidate(self, name, revision, environment, preview):
        doc = self.require(name, revision, environment)
        if doc["saved_revision"] != revision:
            raise ValueError("Entwurf vor dem Export speichern.")
        self.save(name, revision, environment)
        ordinary = doc["nodes"]
        ids = {n["id"] for n in ordinary}
        action_edges = {tuple(e) for e in doc["base"].get("protected_action_edges", [])}
        edges = [e for e in doc["edges"] if e["from"] in ids and e["to"] in ids ]
        if not ordinary:
            raise ValueError("Keine gewöhnlichen Wegpunkte zum Prüfen vorhanden.")
        version = 6 if any(n.get("action_properties", {}).get("tce_door_role") for n in ordinary) else 5
        rows = [f"TCE_NAVMESH {version} {name} {len(ordinary)} {len(edges)}"]
        pairs = {(e["from"], e["to"]) for e in edges}
        for n in ordinary:
            flags = nav_flags(n["nav_flags"])
            props = n.get("action_properties", {})
            # Door recipes carry their own complete identity. Format6 forbids
            # traversal roles on them; editor guide protection is not a jump.
            role = (1 if n.get("generated_guide") and not props.get("tce_door_role") else 0)
            if props.get("tce_ascent_release") or n.get("source_reason") == "ladder_ascent_release_contact_live_pending": role |= 2
            if props.get("tce_ascent_alignment") or n.get("source_reason") == "ladder_upper_alignment_contact_live_pending": role |= 4
            landing = next((r.get("landing_node") for r in n.get("action_roles", []) if r.get("kind")=="walk_drop" and r.get("role") == "launch"), None)
            standing = next((r.get("landing_node") for r in n.get("action_roles", []) if r.get("kind")=="standing_jump" and r.get("role")=="launch"), None)
            free_climb = next((r.get("landing_node") for r in n.get("action_roles", []) if r.get("kind") == "free_climb" and r.get("role") == "launch"), None)
            if props.get("tce_stand_jump") and standing is None:
                raise ValueError("Standing jump needs its directed landing ID.")
            if standing is not None:
                if standing not in ids: raise ValueError("Standing jump landing point was deleted.")
                if landing is not None: raise ValueError("Conflicting standing-jump and drop recipes.")
                role|=64
            if props.get("tce_walk_drop") and landing is None:
                raise ValueError("Abstieg benötigt eine Landeziel-ID im Entwurf.")
            if landing is not None and landing not in ids: raise ValueError("Landeziel des Abstiegs wurde gelöscht.")
            if landing is not None: role |= 8
            if props.get("tce_free_climb") and free_climb is None:
                raise ValueError("Freiklettern benötigt seine gerichtete Landeziel-ID im Entwurf.")
            if free_climb is not None:
                if free_climb not in ids or (n["id"], free_climb) not in pairs:
                    raise ValueError("Freikletter-Landeziel oder gerichtete Verbindung wurde gelöscht.")
                if standing is not None or landing is not None or flags & ((1 << 6) | (1 << 20) | (1 << 27) | (1 << 18) | (1 << 8)) or not flags & (1 << 9):
                    raise ValueError("Widersprüchliches Freikletter-Rezept oder Haltung.")
                role |= 128
            if n.get("source_reason") == "ladder_inside_guidance_not_standable_live_pending" or n.get("editor_role", 0) & 16: role |= 16
            if n.get("source_reason") == "ladder_top_clearance_live_pending" or n.get("editor_role", 0) & 32: role |= 32
            rows.append("%d %.5f %.5f %.5f %d %d %d %.9g %d %d %d %d" %
                        (n["id"], *n["position"], n["crouch"], n["spawn"], True, n["radius"], flags & 0xffffffff, flags >> 32, role, 0 if landing is None and standing is None and free_climb is None else (free_climb if free_climb is not None else standing if standing is not None else landing) + 1))
            if version == 6:
                values = door_candidate(n, ids, pairs)
                rows[-1] += " " + " ".join(str(v) for v in values[:10]) + " " + " ".join("%.5f" % v for v in values[10:])
        rows.extend(f"{e['from']} {e['to']}" for e in edges)
        records=[]
        for node in ordinary:
            name_value,facing_value,uid=waypoint_metadata(node)
            if name_value or uid or any(facing_value):
                records.append('%d %d %.9g %.9g %.9g %s' % (node['id'],uid,*facing_value,name_value.encode('utf8').hex() or '-'))
        if records:
            rows.append('TCE_NODE_METADATA 1 '+str(len(records)));rows.extend(records)
        property_records=[]
        for node in ordinary:
            for key,value in sorted(custom_way_properties(node).items()):
                property_records.append('%d %s %s'%(node['id'],key.encode('utf8').hex(),value.encode('utf8').hex() or '-'))
        if property_records:
            rows.append('TCE_NODE_PROPERTIES 1 '+str(len(property_records)));rows.extend(property_records)
        content = "\n".join(rows) + "\n"
        if len(content)>5*1024*1024:raise ValueError('Prüfkandidat überschreitet das native5MiB-Eingabelimit.')
        destination = Path(preview).resolve() / (name + "_candidate_edit.txt")
        metadata = dict(schema=1, map=name, candidate_mode="manual_draft", directed=True,
                        status="manual_draft_requires_native_hull_validation",
                        movement_profile=doc["base"].get("movement_profile"),
                        candidate_sha256=hashlib.sha256(content.encode("ascii")).hexdigest(),
                        sources={kind: {"sha256": environment.get(kind)} for kind in ("stand", "duck", "geometry")},
                        edit_draft=dict(revision=revision, sha256=graph_hash(doc), base=doc["base"]),
                        nodes=[dict(n, point=n["position"], requested_radius=n["radius"], kind="manual") for n in ordinary],
                        waypoint_metadata_contract='TCE_NODE_METADATA_1' if records else None,
                        custom_property_contract='TCE_NODE_PROPERTIES_1' if property_records else None,
                        edges=[[e["from"], e["to"]] for e in edges],
                        locked_action_nodes_excluded=0)
        destination.parent.mkdir(parents=True, exist_ok=True)
        for path, body in ((destination, content), (destination.with_suffix(".json"), json.dumps(metadata, separators=(",", ":")))):
            temporary = path.with_suffix(path.suffix + ".tmp")
            temporary.write_text(body, encoding="ascii" if path.suffix == ".txt" else "utf-8")
            temporary.replace(path)
        doc["export"] = dict(revision=revision, sha256=graph_hash(doc), candidate=destination.name,
                             candidate_sha256=metadata["candidate_sha256"])
        self.save(name, revision, environment)
        return destination

    def restore_latest(self, name):
        if self.get(name):
            raise RuntimeError("Ein Entwurf ist bereits geöffnet; er wird nicht überschrieben.")
        archives = sorted(self.directory.glob(name + ".r*.archive.json"), key=lambda p:p.stat().st_mtime_ns)
        if not archives:
            raise RuntimeError("Für diese Map gibt es keinen archivierten Entwurf.")
        wrapper = json.loads(archives[-1].read_text(encoding="utf-8"))
        doc = wrapper["draft"]
        if fingerprint(doc) != wrapper["sha256"]:
            raise RuntimeError("Archiv-Prüfsumme stimmt nicht; Original bleibt unverändert.")
        self.validate(doc, name)
        doc["export"] = None
        doc["saved_revision"] = doc["revision"]
        target = self.path(name)
        # Exclusive create prevents overwriting a draft created concurrently.
        with target.open("x", encoding="utf-8") as stream:
            json.dump({"sha256": fingerprint(doc), "draft": doc}, stream, separators=(",", ":"), allow_nan=False)
        self.documents[name] = doc
        self.disk_hashes[name] = hashlib.sha256(target.read_bytes()).hexdigest()
        return doc

    def archive(self, name, revision):
        doc = self.get(name)
        if not doc or type(revision) is not int or doc["revision"] != revision:
            raise RuntimeError("Entwurf fehlt oder wurde inzwischen geändert.")
        self.directory.mkdir(parents=True, exist_ok=True)
        destination = self.directory / (name + ".r" + str(revision) + "." + str(time.time_ns()) + ".archive.json")
        destination.write_text(json.dumps({"sha256": fingerprint(doc), "draft": doc},
                                          separators=(",", ":"), allow_nan=False), encoding="utf-8")
        source = self.path(name)
        if source.is_file():
            if hashlib.sha256(source.read_bytes()).hexdigest() != self.disk_hashes.get(name):
                raise RuntimeError("Extern geänderten Entwurf nicht entfernt. Aktueller Stand wurde separat archiviert.")
            source.unlink()
        self.documents.pop(name)
        self.disk_hashes.pop(name, None)
