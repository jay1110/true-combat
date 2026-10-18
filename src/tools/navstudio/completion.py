"""Directed connectivity required for an automatically completed project.

Native generation_complete confirms physical import, not access between every
spawn region. Compute this from accepted edges, never a cached audit or the
multi-source spawn_reachable marker.
"""
import hashlib
import json
import math
from pathlib import Path

PLAYER_SPAWNS=frozenset({'info_player_start','info_player_deathmatch','team_CTF_redspawn',
    'team_CTF_bluespawn','team_CTF_redplayer','team_CTF_blueplayer'})


def _signature(report):
    return hashlib.sha256(json.dumps(report,sort_keys=True,separators=(',',':')).encode()).hexdigest()


def spawn_bindings(report, report_path, metadata_path=None):
    """Reconstruct literal BSP/candidate/native seed evidence; never write it.

    The external importer validates these seeds against live in-use player
    entities and a fresh grounded WALK (NG_ExternalSpawnAnchor). Its broader
    legacy NG_MapSeeds prefix is deliberately not a qualifying import mode.
    """
    try:
        report_path=Path(report_path);raw=report_path.read_bytes()
        if json.loads(raw)!=report:raise ValueError('native report changed')
        source=Path(report['source'].replace('\\','/')).name
        candidate=report_path.parent/source
        if metadata_path is None:
            metadata_path=report_path.parent.parent/'preview'/Path(source).with_suffix('.json').name
        metadata_path=Path(metadata_path);meta_raw=metadata_path.read_bytes();meta=json.loads(meta_raw)
        text=candidate.read_text(encoding='ascii');header=text.splitlines()[0].split()
        if (header[:2]not in (['TCE_NAVMESH','5'],['TCE_NAVMESH','6'])or header[2]!=report['map']or
                report.get('candidate_mode')!='manual_draft'or
                meta.get('map')!=report['map']or meta.get('candidate_mode')!=report['candidate_mode']or
                hashlib.sha256(text.replace('\r\n','\n').replace('\r','\n').encode('ascii')).hexdigest()!=meta['candidate_sha256']):
            raise ValueError('external candidate is not source-bound')
        if (int(header[3])!=len(meta['nodes'])or len(meta['nodes'])!=len(report['nodes'])or
                int(header[4])!=len(meta['edges'])):raise ValueError('candidate dimensions changed')
        geometry_path=metadata_path.parent/(report['map']+'.geometry.json')
        geo_raw=geometry_path.read_bytes();geometry=json.loads(geo_raw)
        if (hashlib.sha256(geo_raw).hexdigest()!=meta['sources']['geometry']['sha256']or
                geometry.get('map')!=report['map']or geometry.get('entity_state_source')!='runtime_snapshot'):
            raise ValueError('geometry source changed')
        snapshot=geometry['runtime_snapshot'];snapshot_raw=Path(snapshot['path']).read_bytes();runtime=json.loads(snapshot_raw)
        if (hashlib.sha256(snapshot_raw).hexdigest()!=snapshot['sha256']or runtime.get('map')!=report['map']or
                type(runtime.get('gametype'))is not int or runtime['gametype']not in(2,5,7)):
            raise ValueError('map/GT snapshot changed')
        attempt_path=report_path.parent.parent/'work'/(report['map']+'.native-attempt.json')
        attempt_raw=attempt_path.read_bytes();attempt=json.loads(attempt_raw);inputs=attempt['inputs']
        if (attempt.get('map')!=report['map']or attempt.get('candidate')!=source or
                type(attempt.get('gametype'))is not int or attempt['gametype']!=runtime['gametype']or
                attempt.get('generation_complete')is not True or attempt.get('native_report')!=hashlib.sha256(raw).hexdigest()or
                attempt.get('staged_candidate')!=hashlib.sha256(candidate.read_bytes()).hexdigest()or
                inputs.get('candidate_metadata')!=hashlib.sha256(meta_raw).hexdigest()or
                inputs.get('geometry')!=hashlib.sha256(geo_raw).hexdigest()or
                inputs.get('runtime_snapshot')!=snapshot['sha256']or inputs.get('bsp')!=geometry['input']['bsp_sha256']):
            raise ValueError('native attempt/GT/input binding changed')
        from navmesh.waygraph import navigation_spawns
        sources,_=navigation_spawns(geometry);authored={n['id']:n for n in meta['nodes']};rows=[]
        for node in report['nodes']:
            entry=authored.get(node['id'],{});native=node.get('native_input',{})
            if (node.get('accepted')is not True or entry.get('spawn')is not True or
                    entry.get('source_reason')!='map_entity_spawn_seed'or
                    native.get('floor_accepted')is not True or native.get('floor_reason')!='map_entity_spawn_seed'):
                continue
            origin=report['bbox']['origin_above_feet']
            if math.dist([*entry['position'][:2],entry['position'][2]+origin],node['raw_position'])>.011:continue
            for spawn in sources:
                if spawn.get('classname')in PLAYER_SPAWNS and math.dist(spawn['origin_bsp'],node['raw_position'])<=256.001:
                    rows.append(dict(source_id=node['id'],entity=spawn['entity'],classname=spawn['classname'],
                        origin_bsp=spawn['origin_bsp'],gametype=runtime['gametype']))
        return dict(contract='external_native_bsp_spawn_entries_v1',map=report['map'],report_signature=_signature(report),
            report_sha256=hashlib.sha256(raw).hexdigest(),candidate_sha256=hashlib.sha256(candidate.read_bytes()).hexdigest(),
            metadata_sha256=hashlib.sha256(meta_raw).hexdigest(),geometry_sha256=hashlib.sha256(geo_raw).hexdigest(),
            bsp_sha256=geometry['input']['bsp_sha256'],runtime_snapshot_sha256=snapshot['sha256'],
            native_attempt_sha256=hashlib.sha256(attempt_raw).hexdigest(),rows=rows)
    except (OSError,KeyError,IndexError,TypeError,ValueError,ImportError):
        return None


def _components(report):
    nodes = [n for n in report.get('nodes', []) if n.get('accepted') is True]
    ids = [n.get('id') for n in nodes]
    if not ids or any(type(i) is not int for i in ids) or len(set(ids)) != len(ids):
        raise ValueError('Native Punktliste ist leer oder uneindeutig.')
    forward = {i: set() for i in ids}; reverse = {i: set() for i in ids}
    for edge in report.get('edges', []):
        if edge.get('accepted') is not True:
            continue
        a, b = edge.get('from'), edge.get('to')
        if type(a) is not int or type(b) is not int or a not in forward or b not in forward:
            raise ValueError('Native Verbindung verweist auf einen ungeprüften Punkt.')
        forward[a].add(b); reverse[b].add(a)
    seen = set(); order = []
    for start in ids:
        if start in seen:
            continue
        seen.add(start); stack = [(start, iter(forward[start]))]
        while stack:
            node, children = stack[-1]; child = next(children, None)
            if child is None:
                order.append(node); stack.pop()
            elif child not in seen:
                seen.add(child); stack.append((child, iter(forward[child])))
    seen = set(); groups = []
    for start in reversed(order):
        if start in seen:
            continue
        group = []; stack = [start]; seen.add(start)
        while stack:
            node = stack.pop(); group.append(node)
            for child in reverse[node]:
                if child not in seen:
                    seen.add(child); stack.append(child)
        groups.append(sorted(group))
    groups.sort(key=lambda group: (-len(group), group[0]))
    return groups, forward, reverse


def main_source_ids(report):
    """Destination selection only; this is not completion/installation proof."""
    try:
        groups, _, _ = _components(report)
    except (KeyError, TypeError, ValueError):
        return frozenset()
    if len(groups)>1 and len(groups[0])==len(groups[1]):
        return frozenset()
    return frozenset(groups[0])


def navigation_readiness(report, spawn_bindings=None):
    if not isinstance(report, dict) or report.get('generation_complete') is not True:
        return dict(ready=False, reasons=['Navigation ist nicht vollständig nativ geprüft.'])
    try:
        groups, forward, reverse = _components(report)
    except (KeyError, TypeError, ValueError) as error:
        return dict(ready=False, reasons=[str(error)])
    if len(forward)<2:
        return dict(ready=False, reasons=['Native Punktliste enthält weniger als zwei Punkte.'])
    outside = sorted(i for group in groups[1:] for i in group)
    reasons = [] if not outside else [
        'Getrennte oder nur einseitig angeschlossene Bereiche (Source-IDs: ' +
        ', '.join(map(str, outside[:24])) + (' …' if len(outside) > 24 else '') + '). Automatische Reparatur erforderlich.']
    result=dict(ready=not reasons, reasons=reasons, component_sizes=[len(g) for g in groups],
                outside_main_source_ids=outside, native_accepted_nodes=len(forward),
                directed_entry_connectivity_pass=False,entry_only_source_ids=[])
    if not outside:return result
    binding=spawn_bindings or{};capacity=report.get('native_input_coverage',{}).get('capacity',{})
    if (len(groups[0])==len(groups[1])or binding.get('contract')!='external_native_bsp_spawn_entries_v1'or
            binding.get('map')!=report.get('map')or binding.get('report_signature')!=_signature(report)or
            capacity.get('input_validation_complete')is not True or capacity.get('input_truncated')is not False):
        return result
    nodes={n['id']:n for n in report['nodes']if n.get('accepted')is True}
    seeds={row['source_id']for row in binding.get('rows',[])if row.get('source_id')in nodes and
        row.get('classname')in PLAYER_SPAWNS and type(row.get('gametype'))is int and row['gametype']in(2,5,7)and
        nodes[row['source_id']].get('native_input',{}).get('floor_reason')=='map_entity_spawn_seed'}
    main=set(groups[0]);outside_set=set(outside)
    if not seeds&main:return result
    # Every satellite point must reach the core. Every root satellite SCC must
    # be an actual source-bound player spawn; a camera/ordinary island cannot
    # borrow the multi-source spawn_reachable marker as permission.
    reached=set(main);stack=list(main)
    while stack:
        for node in reverse[stack.pop()]-reached:reached.add(node);stack.append(node)
    if reached!=set(nodes):return result
    roots=[set(group)for group in groups[1:]if not any(reverse[i]-set(group)for i in group)]
    if not roots or any(not group&seeds for group in roots):return result
    drops=[]
    for route in report.get('traversal_routes',[]):
        a,b=route.get('launch'),route.get('landing')
        if (route.get('kind')=='walk_drop'and route.get('exported')is True and
                route.get('model')=='tce_ground_then_fall_speed_samples_v3'and
                route.get('sample_count')==3 and route.get('wishspeed_samples')==[114,160,204]and
                route.get('initial_velocity')==[0,0,0]and route.get('impulse_z')==0 and
                route.get('crashland_limit')==38 and type(route.get('crashland_delta'))in(int,float)and
                math.isfinite(route['crashland_delta'])and 0<=route['crashland_delta']<=38 and
                a in outside_set and b in nodes and b in forward[a]):
            drops.append((a,b))
    entries=[]
    for group in roots:
        visited=set(group);stack=list(group)
        while stack:
            for node in (forward[stack.pop()]&outside_set)-visited:visited.add(node);stack.append(node)
        matches=[dict(launch=a,landing=b)for a,b in drops if a in visited]
        if not matches:return result
        entries.append(dict(spawn_source_ids=sorted(group&seeds),source_ids=sorted(group),drop_edges=matches))
    result.update(ready=True,reasons=[],directed_entry_connectivity_pass=True,
                  entry_only_source_ids=outside,spawn_entries=entries,spawn_binding=binding)
    return result
