"""Project spawn layer: read BSP points, edit generated points, export GT7."""
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import uuid


SPAWN_CLASSES = frozenset(("info_player_deathmatch", "info_player_start",
    "info_player_intermission", "team_CTF_redspawn", "team_CTF_bluespawn",
    "team_WOLF_spawn", "team_WOLF_spawnpoint"))
START = "// NAVSTUDIO_GT7_SPAWNS_BEGIN"
END = "// NAVSTUDIO_GT7_SPAWNS_END"


def coordinates(value):
    if not isinstance(value, (list, tuple)) or len(value) != 3 or any(
            type(v) not in (int, float) or not math.isfinite(v) or abs(v) > 131072 for v in value):
        raise ValueError("Spawn benötigt drei gültige Koordinaten.")
    return [round(float(v), 4) for v in value]


def bsp_entities(assets, name):
    from tce_bsp_navmesh import parse_entities, read_lumps
    data = assets.read("maps/" + name + ".bsp", 128 * 1024 * 1024)
    lumps = read_lumps(data)
    lump = lumps[0]
    entities = parse_entities(data[lump['offset']:lump['offset'] + lump['length']])
    model_lump = lumps[7]
    models = [struct.unpack_from('<6f4i', data, offset) for offset in
              range(model_lump['offset'], model_lump['offset'] + model_lump['length'], 40)]
    for i, entity in enumerate(entities):
        entity = dict(entity)
        entity['bsp_id'] = i
        if 'origin' in entity:
            try: entity['position'] = coordinates([float(x) for x in entity['origin'].split()])
            except ValueError: continue
        elif re.fullmatch(r'\*[0-9]+', entity.get('model', '')):
            index = int(entity['model'][1:])
            if not 0 <= index < len(models): continue
            bounds = models[index]
            entity['position'] = [(bounds[j] + bounds[j + 3]) * .5 for j in range(3)]
        else: continue
        yield entity


def source_spawns(assets, name):
    result = []
    for entity in bsp_entities(assets, name):
        classname = entity.get('classname', '')
        if classname not in SPAWN_CLASSES: continue
        try: angle = float(entity.get('angle', '0'))
        except ValueError: angle = 0
        result.append(dict(id='bsp-' + str(entity['bsp_id']), kind='bsp', editable=False,
                           classname=classname, position=entity['position'], angle=angle,
                           team='red' if 'redspawn' in classname else 'blue' if 'bluespawn' in classname else 'all'))
    return result


class SpawnStore:
    def __init__(self, path, name):
        self.path, self.name = Path(path), name

    def read(self):
        if not self.path.exists(): return dict(schema=1, map=self.name, revision=0, next_id=1, enabled=True, nodes=[])
        doc = json.loads(self.path.read_text(encoding='utf8'))
        if doc.get('schema') != 1 or doc.get('map') != self.name: raise ValueError('Spawn-Projekt gehört zu einer anderen Karte.')
        if len(doc.get('nodes', [])) > 128: raise ValueError('Spawn-Limit überschritten.')
        for node in doc['nodes']: coordinates(node['position'])
        return doc

    def write(self, doc):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        temp = self.path.with_name('.spawns-' + uuid.uuid4().hex + '.tmp')
        try:
            temp.write_text(json.dumps(doc, ensure_ascii=False, indent=2), encoding='utf8')
            temp.replace(self.path)
        finally:
            if temp.exists(): temp.unlink()

    def change(self, payload, report=None):
        doc = self.read()
        if payload.get('revision') != doc['revision']: raise RuntimeError('Spawns wurden inzwischen geändert. Neu laden.')
        op = payload.get('op')
        if op == 'generate':
            if not report or not report.get('generation_complete'): raise ValueError('Aktuelle geprüfte Navigation erforderlich.')
            nodes = {n['id']: n for n in report['nodes'] if n.get('accepted') and n.get('spawn_reachable') and
                     not n.get('crouch') and not int(n.get('nav_flags', 0)) & ((1 << 9) | (1 << 27) | (1 << 8) | (1 << 18))}
            out, incoming = {}, set()
            for e in report['edges']:
                if e.get('accepted') and e['from'] in nodes and e['to'] in nodes:
                    out.setdefault(e['from'], []).append(e['to']); incoming.add(e['to'])
            pool = [n for i, n in nodes.items() if i in out and i in incoming]
            selected = []
            distance = lambda a, b: sum((x - y) ** 2 for x, y in zip(a, b))
            while pool and len(selected) < min(64, max(4, int(payload.get('count', 24)))):
                pick = max(pool, key=lambda n: min(distance(n['position'], p['position']) for p in selected)) if selected else min(pool, key=lambda n: n['id'])
                if selected and min(distance(pick['position'], p['position']) for p in selected) < 192 ** 2: break
                selected.append(pick); pool.remove(pick)
            if len(selected) < 2: raise ValueError('Zu wenige erreichbare Standflächen für Spawns.')
            # Replace only the generated layer, never BSP spawns.
            doc['nodes'] = []
            for node in selected:
                target = nodes[out[node['id']][0]]['position']; position = node['position']
                angle = math.degrees(math.atan2(target[1] - position[1], target[0] - position[0]))
                doc['nodes'].append(dict(id='generated-' + str(doc['next_id']), kind='generated', editable=True,
                    classname='info_player_deathmatch', position=coordinates(position), angle=angle,
                    team='all', status='native_floor', source_node=node['id']))
                doc['next_id'] += 1
        elif op in ('move', 'delete'):
            node = next((n for n in doc['nodes'] if n['id'] == payload.get('id')), None)
            if not node: raise ValueError('Nur automatisch hinzugefügte Spawns können verändert werden.')
            if op == 'delete': doc['nodes'].remove(node)
            else:
                node['position'] = coordinates(payload.get('position'))
                if 'angle' in payload:
                    if type(payload['angle']) not in (float, int) or not math.isfinite(payload['angle']): raise ValueError('Ungültiger Winkel.')
                    node['angle'] = float(payload['angle']) % 360
                node['status'] = 'edited_requires_native_check'
        elif op == 'enabled': doc['enabled'] = payload.get('enabled') is True
        else: raise ValueError('Unbekannte Spawn-Aktion.')
        doc['revision'] += 1; self.write(doc)
        return doc


def spawn_candidates(report):
    """Only ordinary supported, two-way-connected positions; native GT7 decides occupancy."""
    if report.get('generation_complete')is not True:raise ValueError('Vollständiges nativ geprüftes Netz erforderlich.')
    nodes={n['id']:n for n in report['nodes']if n.get('accepted')is True and n.get('spawn_reachable')is True and
           not n.get('crouch')and not n.get('nav_flags')and not n.get('editor_role')}
    pairs={(e['from'],e['to'])for e in report['edges']if e.get('accepted')is True}
    joined={a for a,b in pairs if a in nodes and b in nodes and(b,a)in pairs}
    return [nodes[i]for i in sorted(joined)]


def checked_spawn(name, node, report, check):
    """Bind a native spawncheck to this exact serialized ordinary candidate.

    The command sends %.4f; C casts atof to float and prints %.5f. Compare
    against that precise echo, including float32 rounding, not a spatial
    tolerance that could authorize a different point. The feet may differ in
    Z by the existing native one-unit floor rule; the spawn origin is never
    replaced with the discovered floor.
    """
    if (not isinstance(report, dict) or report.get('map') != name or report.get('generation_complete') is not True or
            not isinstance(node, dict) or not isinstance(check, dict)):
        return False
    if (node.get('accepted') is not True or node.get('spawn_reachable') is not True or
            node.get('crouch') or node.get('nav_flags') or node.get('editor_role')): return False
    bbox = report.get('bbox')
    if not isinstance(bbox, dict): return False
    profile = tuple(bbox.get(key) for key in ('half_width', 'stand_height', 'crouch_height', 'origin_above_feet'))
    if profile not in ((16, 87, 61, 30), (14, 70, 49, 24)): return False
    if (check.get('accepted') is not True or check.get('map') != name or
            check.get('origin_above_feet') != profile[3] or
            type(check.get('traces')) is not int or check['traces'] <= 0): return False
    try:
        coordinates(node['position'])  # Validate without changing the source.
        native = [struct.unpack('<f', struct.pack('<f', float(format(v, '.4f'))))[0] for v in node['position']]
        expected = [float(format(v, '.5f')) for v in native]
        origin, feet = check.get('origin'), check.get('feet')
        if any(not isinstance(v, (list, tuple)) or len(v) != 3 or
               any(type(n) not in (int, float) or not math.isfinite(n) for n in v) for v in (origin, feet)):
            return False
        if list(origin) != expected or list(feet[:2]) != expected[:2]: return False
        native_feet_z = struct.unpack('<f', struct.pack('<f', native[2] - profile[3]))[0]
        # %.5f loses at most half its last decimal; this is serialization
        # precision only, not an extra allowance in the native floor rule.
        if abs(feet[2] - native_feet_z) > 1 + .000005 + 1e-12: return False
    except (KeyError, TypeError, ValueError, OverflowError, struct.error):
        return False
    return True


def checked_document(name,previous,nodes,report,attempts):
    """A generator result, bound to actual engine spawnchecks; no manual coordinate edits."""
    allowed={n['id']:n for n in spawn_candidates(report)}
    checks={}
    for node in nodes:
        matches=[r for r in attempts if isinstance(r,dict)and r.get('source_id')==node['id']]
        if len(matches)!=1 or not checked_spawn(name,node,report,matches[0].get('check')):
            raise ValueError('Spawns sind nicht an die aktuellen nativen Prüfungen gebunden.')
        checks[node['id']]=matches[0]['check']
    if (previous.get('map')!=name or not nodes or len({n['id']for n in nodes})!=len(nodes)or
            any(allowed.get(n['id'])!=n for n in nodes)):
        raise ValueError('Spawns sind nicht an die aktuellen nativen Prüfungen gebunden.')
    doc=dict(schema=1,map=name,revision=previous['revision']+1,next_id=previous['next_id']+len(nodes),enabled=previous.get('enabled',True),nodes=[])
    outgoing={n['id']:sorted(e['to']for e in report['edges']if e.get('accepted')and e['from']==n['id']and e['to']in allowed)for n in nodes}
    for index,node in enumerate(nodes):
        position=node['position'];target=allowed[outgoing[node['id']][0]]['position']
        doc['nodes'].append(dict(id='generated-'+str(previous['next_id']+index),kind='generated',editable=True,
            classname='info_player_deathmatch',position=coordinates(position),angle=math.degrees(math.atan2(target[1]-position[1],target[0]-position[0])),
            team='all',status='native_checked',source_node=node['id'],native_check=json.loads(json.dumps(checks[node['id']],allow_nan=False))))
    return doc


def script_block(text, doc):
    lines = [START, '// 1 = generated deathmatch spawns; 0 = standard BSP spawns.',
             'tce_dmspawns ' + ('1' if doc.get('enabled') else '0')]
    lines.extend('tce_dmspawn ' + ' '.join(format(v, '.4f') for v in (*n['position'], n['angle'])) for n in doc['nodes'])
    lines.append(END)
    block = '\n\t\t' + '\n\t\t'.join(lines) + '\n'
    if START in text or END in text:
        if text.count(START) != 1 or text.count(END) != 1 or text.index(END) < text.index(START): raise ValueError('Mehrdeutiger Spawn-Block.')
        return text[:text.index(START)] + block.strip() + text[text.index(END) + len(END):]
    clean = re.sub(r'/\*.*?\*/|//[^\r\n]*', lambda m: ' ' * len(m[0]), text, flags=re.S)
    match = re.search(r'\bgame_manager\s*\{\s*spawn\s*\{', clean)
    if not match: raise ValueError('Mapscript enthält keinen game_manager/spawn-Block.')
    return text[:match.end()] + block + text[match.end():]
