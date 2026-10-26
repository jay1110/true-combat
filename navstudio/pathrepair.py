"""Append explicit native path proposals without replacing authored work.

A partial native repair report certifies only its enumerated accepted chains.
It never certifies a WAY or permits installation. The caller must bind/stage the
current candidate, obtain a fresh report, recheck source hashes after the engine
call, archive its literal inputs and perform another normal full Format5/6 check.
"""
import copy
from collections import Counter
import hashlib
import json
import math
import re

from edit import custom_way_properties, graph_hash, waypoint_metadata


def _require(condition,message):
    if not condition:raise RuntimeError('Native Pfadreparatur: '+message)


def _sha(raw):return hashlib.sha256(raw).hexdigest()


def _rows(report):
    rows={n['id']:n for n in report['nodes']};edges={(e['from'],e['to']):e for e in report['edges']}
    _require(len(rows)==len(report['nodes']) and len(edges)==len(report['edges']), 'doppelte Punkt-/Verbindungsidentität.')
    _require(all(type(i)is int and 0<=i<2147467263 for i in rows) and
             all(type(a)is int and type(b)is int and a in rows and b in rows and a!=b for a,b in edges),'ungültige Graphgrenzen.')
    return rows,edges


def _profile(doc,report,origin,attempt):
    profile=doc['base']['movement_profile'];box=report['bbox']
    _require(type(origin)in(int,float) and math.isfinite(origin) and origin==box['origin_above_feet'] and
             origin==profile['origin_above_feet'] and profile==attempt['movement_profile'] and
             box['half_width']==profile['radius'] and box['stand_height']==profile['standing_height'] and
             box['crouch_height']==profile['crouching_height'],'Körperprofil passt nicht zur Quelle.')


def _old_node(node,row,origin):
    name,facing,uid=waypoint_metadata(node);raw=row['raw_position'];position=row['position']
    _require(len(raw)==len(position)==3 and all(math.isfinite(v)for v in raw+position+node['position']) and
             math.dist([raw[0],raw[1],raw[2]-origin],node['position'])<=.011 and
             math.dist(raw[:2],position[:2])<=.011 and abs(raw[2]-position[2])<=18.25,
             'vorhandene Punktkoordinaten verändert.')
    _require(type(row.get('nav_flags'))is int and row['nav_flags']==node['nav_flags'] and bool(node['crouch'])==bool(node['nav_flags']&64) and
             type(row.get('crouch'))is bool and row.get('import_uid',0)==uid and
             (not uid or row.get('way_guid',0)in(0,uid)) and row.get('name','')==name and
             math.dist(facing,row.get('facing',[0,0,0]))<=.0002 and
             row.get('custom_properties',{})==custom_way_properties(node),'vorhandene Punktmetadaten verändert.')


def _source(doc,baseline,origin,binding):
    status,attempt,inputs,metadata=(binding[key]for key in('report_status','attempt','inputs','metadata'))
    raw,candidate,meta=(binding[key]for key in('native_report_bytes','candidate_bytes','candidate_metadata_bytes'))
    _require(all(type(value)is bytes for value in(raw,candidate,meta)) and json.loads(raw)==baseline and
             json.loads(meta)==metadata and _sha(raw)==status['native_report']==attempt['native_report'] and
             _sha(candidate)==inputs['candidate']==status['candidate_sha256']==attempt['staged_candidate'] and
             _sha(meta)==inputs['candidate_metadata'],'Quelldateien/Prüfbelege stimmen nicht überein.')
    text=candidate.decode('ascii');header=text.splitlines()[0].split()
    _require(len(header)==5 and header[:1]==['TCE_NAVMESH'] and header[1]in('5','6') and
             header[2]==doc['map'] and int(header[3])==len(doc['nodes']) and int(header[4])==len(doc['edges']),
             'keine vollständige manuelle Format5/6-Quelle.')
    normalized=_sha(text.replace('\r\n','\n').replace('\r','\n').encode('ascii'))
    edited=metadata['edit_draft'];export=doc['export'];identity=graph_hash(doc)
    _require(doc.get('mode')=='manual_draft' and baseline.get('candidate_mode')=='manual_draft' and
             baseline.get('waypoint_metadata_contract')=='TCE_NODE_METADATA_1' and
             metadata.get('candidate_mode')=='manual_draft' and status.get('current')is True and
             all(obj.get('map')==doc['map']for obj in(baseline,attempt,metadata)) and
             type(baseline.get('generation_complete'))is bool and attempt['generation_complete']==baseline['generation_complete'] and
             doc['saved_revision']==doc['revision']==edited['revision']==export['revision'] and
             identity==edited['sha256']==export['sha256'] and edited['base']==doc['base'] and
             normalized==metadata['candidate_sha256']==export['candidate_sha256'] and
             status['candidate']==attempt['candidate']==export['candidate'] and
             baseline['source'].replace('\\','/').split('/')[-1]==status['candidate'] and
             attempt['inputs']==inputs,'Entwurf oder native Quelle ist veraltet.')
    _require(all(re.fullmatch('[a-f0-9]{64}',inputs.get(key,''))for key in
                 ('candidate','candidate_metadata','bsp','stand','duck','geometry','collision','runtime_snapshot','game_module')) and
             bool(inputs.get('navigation_contract')) and
             all(inputs.get(k)==v for k,v in doc['base']['environment'].items()) and
             all(metadata['sources'][key]['sha256']==inputs[key]for key in('stand','duck','geometry')),
             'Kollisions-/Kandidatenkontext ist nicht gebunden.')
    _profile(doc,baseline,origin,attempt)
    nodes={n['id']:n for n in doc['nodes']};rows,edges=_rows(baseline)
    candidates={n['id']:n for n in metadata['nodes']};pairs={(e['from'],e['to'])for e in doc['edges']}
    _require(bool(nodes) and len(nodes)==len(doc['nodes'])==len(metadata['nodes']) and set(nodes)==set(rows)==set(candidates) and
             len(pairs)==len(doc['edges'])==len(metadata['edges']) and pairs==set(edges)=={tuple(e)for e in metadata['edges']},
             'Quelle enthält nicht den vollständigen vorhandenen Graph.')
    for i,node in nodes.items():
        _require(all(candidates[i].get(k)==v for k,v in node.items()),'Kandidatenmetadaten gehören zu anderem Entwurf.')
        _old_node(node,rows[i],origin)
    coverage=baseline['native_input_coverage'];capacity=coverage['capacity']
    floor=sum(row.get('native_input',{}).get('floor_accepted')is True for row in rows.values())
    rejected={pair:e for pair,e in edges.items()if e.get('accepted')is False and e.get('reason')!='quarantined_unreachable_endpoint'}
    histogram=Counter(e['reason']for e in rejected.values())
    reasons=coverage['link_rejections']
    _require(capacity.get('input_validation_complete')is True and capacity.get('input_truncated')is False and
             coverage['input_nodes']==len(nodes) and coverage['native_floor_accepted']==floor and
             coverage['native_floor_rejected']==len(nodes)-floor and coverage['initial_directed_links']==len(pairs) and
             coverage['initial_rejected_links']==len(rejected) and coverage['initial_accepted_links']==len(pairs)-len(rejected) and
             len(reasons)==len(histogram) and {r['reason']:r['count']for r in reasons}==dict(histogram) and
             all(type(row.get('accepted'))is bool and type(row.get('native_input',{}).get('floor_accepted'))is bool for row in rows.values()) and
             all(type(edge.get('accepted'))is bool for edge in edges.values()) and
             0<=baseline['traces']<baseline['trace_budget'],'native Eingabeprüfung unvollständig.')
    return nodes,rows,edges


def validate_source(doc,baseline,origin,binding):
    """Fail closed before starting a native search, and again before merging."""
    try:
        _source(doc,baseline,origin,binding)
        return True
    except (KeyError,IndexError,TypeError,ValueError,OverflowError,UnicodeError)as error:
        raise RuntimeError('Native Pfadreparatur: ungültige oder unvollständige Quellenbindung.')from error


def merge(doc,report,origin,baseline=None,binding=None):
    """Return an append-only draft proposal plus source-bound chain evidence."""
    try:
        nodes,previous,old_edges=_source(doc,baseline,origin,binding)
        raw=binding['repair_report_bytes']
        _require(type(raw)is bytes and json.loads(raw)==report,'Reparaturbericht entspricht nicht seinen Rohdaten.')
        _require(report.get('map')==doc['map'] and report.get('candidate_mode')=='manual_draft' and
                 type(report.get('generation_complete'))is bool and
                 report.get('waypoint_metadata_contract')=='TCE_NODE_METADATA_1' and report['bbox']==baseline['bbox'] and
                 report['source']==baseline['source'],'Reparatur gehört zu anderer Karte/Körperquelle.')
        rows,edges=_rows(report);recipe=report['path_repair'];boundary=doc['next_id']
        _require(type(boundary)is int and boundary>max(nodes) and boundary<2147467263 and
                 recipe.get('model')=='native_append_paths_v1' and recipe.get('explicit_repair')is True and
                 recipe.get('preserved_input')is True and recipe.get('minimum_new_source_id')==boundary and
                 recipe.get('input_nodes')==len(nodes) and recipe.get('input_edges')==len(old_edges) and
                 recipe.get('input_validation_complete')is True and recipe.get('budget_complete')is True and
                 type(recipe.get('stop_reason'))is str and bool(recipe['stop_reason']) and
                 0<=recipe['traces']<recipe['trace_budget'] and 0<=report['traces']<report['trace_budget'],
                 'kein vollständiger expliziter Pfadreparaturbeleg.')
        coverage=report['native_input_coverage'];old_coverage=baseline['native_input_coverage']
        _require(coverage['capacity'].get('input_validation_complete')is True and coverage['capacity'].get('input_truncated')is False and
                 all(coverage.get(k)==old_coverage.get(k)for k in('input_nodes','native_floor_accepted','native_floor_rejected',
                     'initial_directed_links','initial_accepted_links','initial_rejected_links','geometry_rejections','link_rejections')),
                 'ursprüngliche Eingabeprüfungen wurden verändert.')
        _require(set(nodes)<=set(rows) and set(old_edges)<=set(edges),'vorhandene Punkte/Verbindungen würden entfernt.')
        for i,node in nodes.items():
            _old_node(node,rows[i],origin)
            _require(rows[i]['position']==previous[i]['position'] and rows[i]['crouch']==previous[i]['crouch'] and
                     rows[i].get('native_input',{}).get('floor_accepted')==previous[i].get('native_input',{}).get('floor_accepted') and
                     rows[i].get('native_input',{}).get('floor_reason')==previous[i].get('native_input',{}).get('floor_reason') and
                     rows[i].get('editor_role',0)==previous[i].get('editor_role',0) and
                     rows[i].get('editor_drop_target',-1)==previous[i].get('editor_drop_target',-1),
                     'vorhandene Boden-/Aktionshaltung wurde verändert.')
        for pair,old in old_edges.items():
            new=edges[pair]
            _require(new.get('crouch',False)==old.get('crouch',False) and new.get('flags',0)==old.get('flags',0),
                     'vorhandene Verbindung wurde verändert.')
            if old.get('accepted')is False and old.get('reason')=='quarantined_unreachable_endpoint':
                _require((new.get('accepted')is False and new.get('reason')==old['reason']) or
                         (new.get('accepted')is True and new.get('reason')in
                          ('external_native_validated_walk','manual_native_transition_live_pending')),
                         'quarantänierte Altverbindung wurde ungeprüft verändert.')
            else:
                _require(new.get('accepted')==old.get('accepted') and new.get('reason')==old.get('reason'),
                         'vorhandene Ablehnung wurde überschrieben.')
        added=set(rows)-set(nodes);listed=recipe['added_node_ids']
        _require(len(set(listed))==len(listed) and set(listed)==added and all(type(i)is int and i>=boundary for i in added),
                 'neue Punkte haben fremde/wiederverwendete IDs.')
        def ordinary(identity,standing=False):
            node=nodes.get(identity,rows[identity]);row=rows[identity]
            role=row.get('editor_role',0)
            plain_guide=(identity in nodes and node.get('generated_guide')is True and node.get('editor_role',0)in(0,1) and role==1)
            return (node.get('nav_flags',0)in((0,)if standing else(0,64)) and
                    row.get('accepted')is True and row.get('spawn_reachable')is True and
                    row.get('crouch')==bool(node.get('nav_flags',0)&64) and
                    (identity in added or row.get('native_input',{}).get('floor_accepted')is True) and
                    (not role or plain_guide) and not row.get('door_recipe') and
                    (not node.get('editor_role',0) or plain_guide) and
                    not any(node.get(k)for k in('action_roles','action_properties')))
        for identity in added:
            row=rows[identity]
            _require(ordinary(identity) and type(row.get('nav_flags'))is int and not row.get('custom_properties') and
                     row.get('reason')=='generated_native_path_repair_bend' and
                     type(row.get('radius'))in(int,float) and 0<row['radius']<=300 and math.isfinite(row['radius']) and
                     len(row['position'])==3 and all(math.isfinite(v)for v in row['position']) and
                     len(row.get('facing',[0,0,0]))==3 and all(math.isfinite(v)for v in row.get('facing',[0,0,0])),
                     'neuer Punkt ist kein bewiesener gewöhnlicher Bodenanker.')
        chain_edges=set();used_new=set();chains=recipe['chains']
        _require(type(chains)is list,'Pfadketten fehlen.')
        for chain in chains:
            path=chain['source_ids'];crouch=chain['crouch'];kind=chain['kind']
            _require(chain.get('accepted')is True and kind in('ordinary','jump_launch_approach') and
                     2<=len(path)<=32 and len(set(path))==len(path) and path[0]in nodes and path[-1]in nodes and
                     all(type(i)is int and i in rows for i in path) and len(crouch)==len(path)-1 and all(type(v)is bool for v in crouch),
                     'ungültige gerichtete Reparaturkette.')
            if kind=='ordinary':_require(all(ordinary(i)for i in path),'gewöhnlicher Pfad verwendet eine unbewiesene Aktion.')
            else:
                target=nodes[path[-1]];row=rows[path[-1]];flags=target['nav_flags'];jump=(1<<27)|(1<<18)|(1<<8)
                outgoing={p for p in old_edges if p[0]==path[-1]}
                _require(all(ordinary(i,standing=True)for i in path[:-1]) and flags&jump and not flags&~jump and
                         target.get('crouch')is False and row.get('crouch')is False and row.get('accepted')is True and
                         row.get('spawn_reachable')is True and row.get('native_input',{}).get('floor_accepted')is True and
                         len(outgoing)==1 and all(edges[p].get('accepted')is True for p in outgoing) and not any(crouch),
                         'Sprunganlauf ist kein reiner gerichteter Stand-Anlauf zur bestehenden Aktion.')
            directed=set(zip(path,path[1:]));_require(bool(directed-set(old_edges)),'Kette ergänzt keinen neuen Pfad.')
            for pair,duck in zip(zip(path,path[1:]),crouch):
                edge=edges.get(pair)
                # A low corridor may meet an unchanged standing terminal only
                # after the native search proved this exact boundary at full
                # standing height. A DUCK-only proof must not silently make the
                # old terminal crouch or waive clearance at the last new bend.
                standing_boundary=(kind=='ordinary' and any(
                    bend in added and rows[bend]['nav_flags']==64 and terminal in nodes and
                    terminal in(path[0],path[-1]) and nodes[terminal]['nav_flags']==0
                    for bend,terminal in(pair,tuple(reversed(pair)))))
                _require(not standing_boundary or duck is False,'Duck-/Stehgrenze benötigt eine native Stehprüfung.')
                _require(edge and edge.get('accepted')is True and not edge.get('flags',0) and
                         edge.get('crouch',False)==duck and edge.get('reason')in
                         ('external_native_validated_walk','external_native_component_bridge','native_portal_search_walk',
                          'manual_native_transition_live_pending','accepted','native_path_repair_walk','native_path_repair_jump_approach') and
                         (not duck or any(rows[i]['nav_flags']&64 for i in pair)),
                         'Reparaturkante ist nicht als aktuelle Körperbewegung belegt.')
            chain_edges|=directed;used_new.update(set(path)&added)
        new_edges=set(edges)-set(old_edges)
        _require(new_edges==chain_edges-set(old_edges) and used_new==added,
                 'Bericht enthält zusätzliche oder ungebundene Verbindungen/Punkte.')
        result=copy.deepcopy(doc)
        for identity in sorted(added):
            row=rows[identity];position=list(row['position']);position[2]-=origin;flags=row['nav_flags']
            result['nodes'].append(dict(id=identity,position=position,radius=row['radius'],source_radius=row['radius'],
                nav_flags=flags,crouch=bool(flags&64),spawn=False,topology_protected=True,locked=False,generated_guide=True,
                source_reason=row.get('reason','native_path_repair'),editor_role=0,name=row.get('name',''),
                facing=list(row.get('facing',[0,0,0])),way_guid=0,action_roles=[],action_properties={}))
        result['nodes'].sort(key=lambda n:n['id'])
        result['edges']=copy.deepcopy(doc['edges'])+[dict(zip(('from','to'),p))for p in sorted(new_edges)]
        result['next_id']=max(boundary,max(added,default=boundary-1)+1)
        evidence=dict(model='native_append_paths_proposal_v1',source_graph_sha256=graph_hash(doc),
            source_native_report=_sha(binding['native_report_bytes']),repair_report_sha256=_sha(raw),
            source_inputs=copy.deepcopy(binding['inputs']),added_node_ids=sorted(added),
            added_directed_edges=len(new_edges),retained_node_ids=sorted(nodes),deleted_nodes=0,
            retained_rejected_edges=[list(p)for p,e in sorted(old_edges.items())if e.get('accepted')is False],
            chains=copy.deepcopy(chains),changes_existing_node_metadata=False,adopts_existing_way_uids=False,
            native_generation=copy.deepcopy(recipe),native_revalidation_required=True,installed=False)
        return result,evidence
    except (KeyError,IndexError,TypeError,ValueError,OverflowError,UnicodeError)as error:
        raise RuntimeError('Native Pfadreparatur: ungültiger oder unvollständiger Reparaturbeleg.')from error
