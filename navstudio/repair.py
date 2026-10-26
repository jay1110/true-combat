"""Mesh-guided repairs of the current authored graph, retaining IDs/actions.

These are proposals. Only a subsequent native hull/transition check accepts
them. No stale source graph is imported and no unreachable point is deleted.
"""
import copy
from collections import defaultdict
import hashlib
import heapq
import json
import math
import re
from navmesh.waygraph import ordinary_collision_rejections


# A rejected ordinary segment may be withdrawn only when the same current
# native report proves a bounded, directed ordinary alternate. These are
# completed collision/support decisions, never budget/identity/hazard errors.
ORDINARY_COLLISION_REJECTIONS = ordinary_collision_rejections()


def native_crouch_feedback(doc, native):
    """Identify a pure ordinary CROUCH mismatch, never certify a WAY.

    The caller must first bind this report to the exact current candidate,
    draft revision, geometry and native input hashes. A manual import with
    every floor/link checked can withhold its WAY solely because a new walk
    needs CROUCH. Return IDs for a copy-and-promote retry, including when that
    mismatch made generation_complete false. No identities or coordinates are
    adopted, and the changed candidate still needs a full native check.
    """
    from edit import custom_way_properties, waypoint_metadata
    try:
        if (native.get('map') != doc.get('map') or doc.get('mode') != 'manual_draft' or
                native.get('candidate_mode') != 'manual_draft' or
                type(native.get('generation_complete')) is not bool or
                native.get('waypoint_metadata_contract') != 'TCE_NODE_METADATA_1'):
            return None
        original = {n['id']: n for n in doc['nodes']}
        rows = {n['id']: n for n in native['nodes']}
        if (not original or len(original) != len(doc['nodes']) or
                len(rows) != len(native['nodes']) or set(rows) != set(original)):
            return None
        coverage = native.get('native_input_coverage', {})
        capacity = coverage.get('capacity', {})
        pairs = {(e['from'], e['to']) for e in doc['edges']}
        links = {(e['from'], e['to']): e for e in native['edges']}
        if (len(pairs) != len(doc['edges']) or len(links) != len(native['edges']) or set(links) != pairs or
                any(e.get('accepted') is not True for e in links.values()) or
                capacity.get('input_validation_complete') is not True or
                capacity.get('input_truncated') is not False or
                coverage.get('input_nodes') != len(original) or
                coverage.get('native_floor_accepted') != len(original) or
                coverage.get('native_floor_rejected') != 0 or
                coverage.get('initial_directed_links') != len(pairs) or
                coverage.get('initial_accepted_links') != len(pairs) or
                coverage.get('initial_rejected_links') != 0 or
                coverage.get('connectivity_missing_input_nodes') != 0 or
                coverage.get('ordinary_without_exit') != [] or
                coverage.get('geometry_rejections') != [] or coverage.get('link_rejections') != [] or
                not 0 <= native['traces'] < native['trace_budget']):
            return None
        offset = native['bbox']['origin_above_feet']
        if type(offset) not in (int, float) or not math.isfinite(offset):
            return None
        action_ids = {r[key] for r in native.get('traversal_routes', []) for key in
                      ('low', 'high', 'approach', 'launch', 'landing', 'entry', 'exit', 'paired_jump_launch')
                      if type(r.get(key)) is int and r[key] >= 0}
        changed = []
        for identity, node in original.items():
            row = rows[identity]
            name, facing, uid = waypoint_metadata(node)
            flags = node['nav_flags']
            raw, projected = row['raw_position'], row['position']
            feet = [raw[0], raw[1], raw[2] - offset]
            if (row.get('accepted') is not True or
                    row.get('native_input', {}).get('floor_accepted') is not True or
                    row.get('nav_flags') != flags or type(row.get('crouch')) is not bool or
                    bool(node.get('crouch')) != bool(flags & 64) or
                    any(not math.isfinite(v) for v in raw + projected + node['position']) or
                    math.dist(feet, node['position']) > .011 or
                    math.dist(raw[:2], projected[:2]) > .011 or abs(raw[2] - projected[2]) > 18.25 or
                    row.get('import_uid', 0) != uid or
                    (uid and row.get('way_guid') != uid and
                     not (native['generation_complete'] is False and row.get('way_guid') == 0)) or
                    (name and row.get('name') != name) or
                    ('facing' in node and math.dist(facing, row.get('facing', [])) > .0002) or
                    custom_way_properties(node) != row.get('custom_properties', {})):
                return None
            if row['crouch'] == bool(flags & 64):
                continue
            # Only a plain floor point may acquire CROUCH. Native action
            # participation is excluded even when its authored flags are zero.
            if (flags != 0 or node.get('crouch') or row['crouch'] is not True or
                    identity in action_ids or row.get('editor_role', 0) or row.get('door_recipe') or
                    any(node.get(k) for k in ('editor_role', 'generated_guide', 'action_roles', 'action_properties'))):
                return None
            changed.append(identity)
        if not changed:
            return None
        walk_reasons = {'external_native_validated_walk', 'native_validated_walk', 'accepted'}
        crouched = [pair for pair, e in links.items() if e.get('crouch') is True and
                    e.get('reason') in walk_reasons and (pair[0] in changed or pair[1] in changed) and
                    all(original[i]['nav_flags'] in (0, 64) and i not in action_ids and
                        not rows[i].get('editor_role', 0) and not rows[i].get('door_recipe') and
                        not any(original[i].get(k) for k in
                                ('editor_role', 'generated_guide', 'action_roles', 'action_properties')) for i in pair)]
        if any(not any(identity in pair for pair in crouched) for identity in changed):
            return None
        return dict(model='native_ordinary_crouch_feedback_v1', required_crouch_flags=sorted(changed),
                    accepted_crouch_edges=[list(pair) for pair in sorted(crouched)],
                    generation_complete=native['generation_complete'], input_validation_complete=True,
                    adopts_way_uids=False, changes_positions=False, native_revalidation_required=True)
    except (KeyError, IndexError, TypeError, ValueError, OverflowError):
        return None


def _rejected_input_coverage(native, rows, links, require_rejected=True, require_no_exit=True):
    """Separate checked input rejects from later spawn-route quarantine.

    Quarantine is preserved feedback, never a reason to accept a point/edge.
    Its incident links were initially accepted and are counted separately by
    the native writer. Every repair still requires a subsequent full import.
    """
    quarantined = {identity for identity, row in rows.items()
                   if row.get('accepted') is False and
                   row.get('reason') == 'quarantined_no_validated_route_from_spawn' and
                   row.get('spawn_reachable') is False and
                   row.get('native_input', {}).get('floor_accepted') is True and
                   row.get('native_input', {}).get('status') == 'missing_spawn_route'}
    if any(row.get('native_input', {}).get('floor_accepted') is not True or
           (identity not in quarantined and
            (row.get('accepted') is not True or row.get('spawn_reachable') is not True))
           for identity, row in rows.items()):
        return None
    rejected = {pair: edge for pair, edge in links.items() if edge.get('accepted') is not True}
    quarantined_edges = {pair for pair, edge in rejected.items()
                         if edge.get('reason') == 'quarantined_unreachable_endpoint'}
    initial = {pair: edge for pair, edge in rejected.items() if pair not in quarantined_edges}
    counts = defaultdict(int)
    for edge in initial.values(): counts[edge.get('reason')] += 1
    coverage = native['native_input_coverage']; capacity = coverage['capacity']
    reasons = coverage['link_rejections']
    # A local edge withdrawal is not a whole-graph success certificate. Its
    # independently diagnosed sinks may remain, but the supplied list must
    # exactly match the native writer's accepted, flagless, no-outgoing rule.
    # Other callers keep the former strict no-sink contract by default.
    no_exit=coverage.get('ordinary_without_exit')
    outgoing={a for (a,b),edge in links.items() if edge.get('accepted') is True}
    expected_no_exit={i for i,row in rows.items() if row.get('accepted') is True and
                      row.get('nav_flags')==0 and i not in outgoing}
    if (not isinstance(no_exit,list) or any(type(i) is not int for i in no_exit) or
            len(no_exit)!=len(set(no_exit)) or
            (require_no_exit and no_exit) or (not require_no_exit and set(no_exit)!=expected_no_exit)):
        return None
    if ((require_rejected and not initial) or len(initial) > 128 or any(edge.get('accepted') is not False for edge in rejected.values()) or
            any(not any(i in quarantined for i in pair) for pair in quarantined_edges) or
            any(edge.get('accepted') is True and any(i in quarantined for i in pair) for pair, edge in links.items()) or
            len(reasons) != len(counts) or {row['reason']: row['count'] for row in reasons} != dict(counts) or
            capacity.get('input_validation_complete') is not True or capacity.get('input_truncated') is not False or
            coverage.get('input_nodes') != len(rows) or coverage.get('native_floor_accepted') != len(rows) or
            coverage.get('native_floor_rejected') != 0 or coverage.get('initial_directed_links') != len(links) or
            coverage.get('initial_accepted_links') != len(links)-len(initial) or
            coverage.get('initial_rejected_links') != len(initial) or coverage.get('geometry_rejections') != [] or
            coverage.get('connectivity_missing_input_nodes') != len(quarantined) or
            coverage.get('exported_input_nodes') != len(rows)-len(quarantined) or
            coverage.get('spawn_reachable_input_nodes') != len(rows)-len(quarantined) or
            coverage.get('contracted_input_nodes', 0) != 0 or
            native.get('quarantined_unreachable_nodes', 0) != len(quarantined) or
            not 0 <= native['traces'] < native['trace_budget']):
        return None
    return quarantined, rejected


def rejected_shortcut_feedback(doc, native, receipt, source_native_bytes=None):
    """Only withdraw rejected new ordinary edges from the last automatic patch.

    Caller binds the native report/current candidate and the persistent receipt.
    Nodes, authored edges and accepted proposals are never changed here.
    A mixed CROUCH patch additionally needs the literal archived pre-proposal
    native report, independently hashed against receipt.source_native_report.
    """
    from edit import fingerprint, graph_hash
    try:
        patch=doc['undo'][-1]
        if (doc.get('mode')!='manual_draft' or native.get('candidate_mode')!='manual_draft' or
            native.get('map')!=doc.get('map') or native.get('generation_complete')is not False or
            receipt.get('model')!='current_authored_mesh_corridor_proposals_v1' or
            receipt.get('draft_revision')!=doc['revision'] or receipt.get('added_nodes')!=0 or
            receipt.get('deleted_nodes')!=0 or receipt.get('replaced_rejected_edges')!=[] or
            patch.get('op')!='automatic_repair' or patch.get('removed')!=[] or doc.get('redo')):
            return None
        added={tuple(pair)for pair in patch['added']}
        nodes={n['id']:n for n in doc['nodes']};pairs={(e['from'],e['to'])for e in doc['edges']}
        rows={n['id']:n for n in native['nodes']};links={(e['from'],e['to']):e for e in native['edges']}
        if (not added or len(added)>128 or len(added)!=len(patch['added']) or
            receipt.get('added_directed_edges')!=len(added) or set(receipt['retained_node_ids'])!=set(nodes) or
            set(rows)!=set(nodes) or len(rows)!=len(native['nodes']) or len(nodes)!=len(doc['nodes']) or
            len(pairs)!=len(doc['edges']) or set(links)!=pairs or len(links)!=len(native['edges']) or not added<=pairs):return None
        checked=_rejected_input_coverage(native,rows,links)
        if checked is None:return None
        quarantined,rejected_all=checked
        changed=set(receipt['required_crouch_flags'])
        old={n['id']:n for n in patch['before']};new={n['id']:n for n in patch['after']}
        if (len(changed)!=len(receipt['required_crouch_flags']) or
            len(old)!=len(patch['before']) or len(new)!=len(patch['after']) or set(old)!=changed or set(new)!=changed):return None
        # The only retained node edit can be the exact ordinary CROUCH
        # promotion from this source-bound automatic patch. Never undo it.
        if changed and (type(source_native_bytes)is not bytes or
            hashlib.sha256(source_native_bytes).hexdigest()!=receipt.get('source_native_report')):return None
        for identity in changed:
            before=old[identity];after=new[identity]
            expected=copy.deepcopy(before);expected.update(nav_flags=64,crouch=True)
            if (before.get('nav_flags')!=0 or before.get('crouch')is not False or after!=expected or
                nodes.get(identity)!=after or identity in quarantined):return None
        explained=set()
        for route in receipt['routes']:
            if route.get('status')!='mesh_proposal_requires_native_revalidation':continue
            if route.get('kind')!='same_component_local_gap':return None
            chain=route['chain'];edges=set(zip(chain,chain[1:]))
            if route.get('bidirectional'):edges|={(b,a)for a,b in edges}
            explained|=edges-(pairs-added)
        if explained!=added:return None
        offset=native['bbox']['origin_above_feet']
        for identity,node in nodes.items():
            row=rows[identity];raw=row['raw_position']
            if ((row.get('accepted')is not True and identity not in quarantined) or row.get('native_input',{}).get('floor_accepted')is not True or
                row.get('nav_flags')!=node['nav_flags'] or bool(row.get('crouch'))!=bool(node['crouch']) or
                any(not math.isfinite(v)for v in raw+node['position']) or
                math.dist([raw[0],raw[1],raw[2]-offset],node['position'])>.011):return None
        rejected={pair:e for pair,e in rejected_all.items()if pair in added}
        if not rejected:return None
        action_ids={route[key]for route in native.get('traversal_routes',[])for key in
                    ('low','high','approach','launch','landing','entry','exit','paired_jump_launch')
                    if type(route.get(key))is int and route[key]>=0}
        def ordinary(identity):
            return (identity not in quarantined and identity not in action_ids and
                    nodes[identity]['nav_flags']in(0,64) and not rows[identity].get('editor_role',0) and
                    not rows[identity].get('door_recipe') and not any(nodes[identity].get(k)for k in
                    ('editor_role','generated_guide','action_roles','action_properties')))
        if changed:
            source=json.loads(source_native_bytes)
            source_rows={row['id']:row for row in source['nodes']}
            source_links={(edge['from'],edge['to']):edge for edge in source['edges']}
            if (source.get('map')!=doc['map'] or source.get('candidate_mode')!='manual_draft' or
                source.get('waypoint_metadata_contract')!='TCE_NODE_METADATA_1' or
                len(source_rows)!=len(source['nodes']) or set(source_rows)!=set(nodes) or
                len(source_links)!=len(source['edges']) or set(source_links)!=pairs-added or
                _rejected_input_coverage(source,source_rows,source_links,require_rejected=False)is None or
                source['bbox']['origin_above_feet']!=offset):return None
            source_actions={route[key]for route in source.get('traversal_routes',[])for key in
                            ('low','high','approach','launch','landing','entry','exit','paired_jump_launch')
                            if type(route.get(key))is int and route[key]>=0}
            for identity,node in nodes.items():
                row=source_rows[identity];before_node=old.get(identity,node);raw=row['raw_position']
                if (row.get('nav_flags')!=before_node['nav_flags'] or type(row.get('crouch'))is not bool or
                    row['crouch']!=(True if identity in changed else bool(before_node['nav_flags']&64)) or
                    any(not math.isfinite(v)for v in raw+before_node['position']) or
                    math.dist([raw[0],raw[1],raw[2]-offset],before_node['position'])>.011):return None
                if identity in changed and (row.get('accepted')is not True or identity in source_actions or
                    row.get('editor_role',0) or row.get('door_recipe') or not any(
                        edge.get('accepted')is True and edge.get('crouch')is True and not edge.get('flags',0) and
                        edge.get('reason')in('external_native_validated_walk','native_validated_walk','accepted') and
                        identity in pair and all(ordinary(i)for i in pair)for pair,edge in source_links.items())):return None
        if any(not ordinary(identity) or not any(edge.get('accepted')is True and edge.get('crouch')is True and
                edge.get('reason')in('external_native_validated_walk','native_validated_walk','accepted') and
                not edge.get('flags',0) and identity in pair and all(ordinary(i)for i in pair)
                for pair,edge in links.items())for identity in changed):return None
        reasons={'steep_ground','blocked_clearance','blocked_sweep','blocked_step_up','blocked_step_down','different_floor','gap_or_drop'}
        for (a,b),edge in rejected.items():
            if edge.get('accepted')is not False or edge.get('reason')not in reasons:return None
            if edge.get('flags',0) or not all(ordinary(i)for i in(a,b)):return None
        proposal=copy.deepcopy(doc)
        proposal['edges']=[e for e in proposal['edges']if(e['from'],e['to'])not in rejected]
        feedback=dict(model='rejected_automatic_shortcuts_v1',source_graph_sha256=graph_hash(doc),
            rejected_edges=[dict(edge=list(pair),reason=e['reason'],endpoints=[fingerprint(nodes[i])for i in pair])for pair,e in sorted(rejected.items())],
            retained_nodes=len(nodes),retained_directed_edges=len(proposal['edges']),
            retained_required_crouch_flags=sorted(changed),preserved_quarantined_source_ids=sorted(quarantined),
            remaining_rejected_edges=[dict(edge=list(pair),reason=edge.get('reason'))for pair,edge in sorted(rejected_all.items())if pair not in rejected],
            native_revalidation_required=True)
        return proposal,feedback
    except (KeyError,IndexError,TypeError,ValueError,OverflowError,UnicodeError):return None


def filter_rejected_shortcuts(doc, proposal, memo, context):
    """A source-scoped negative cache never removes an existing authored edge."""
    from edit import fingerprint
    result=copy.deepcopy(proposal);nodes={n['id']:n for n in doc['nodes']}
    existing={(e['from'],e['to'])for e in doc['edges']};blocked=set()
    if memo.get('context')==context:
        for row in memo.get('rejected_edges',[]):
            pair=tuple(row.get('edge',[]))
            if len(pair)==2 and all(i in nodes for i in pair)and row.get('endpoints')==[fingerprint(nodes[i])for i in pair]:blocked.add(pair)
    removed={(e['from'],e['to'])for e in result['edges']} & (blocked-existing)
    result['edges']=[e for e in result['edges']if(e['from'],e['to'])not in removed]
    return result,[list(pair)for pair in sorted(removed)]


def rejected_historical_shortcut_feedback(doc, native, source_archives, max_history=16):
    """Recover an automatic addition across later *verified* automatic edits.

    No undo is applied. Exact history is reconstructed only to establish that
    a currently rejected edge was absent before an automatic addition. A
    content-hashed native archive must independently match that predecessor.
    The returned proposal preserves every current node and all other edges.
    """
    from edit import action_radius_node, custom_way_properties, fingerprint, graph_hash, waypoint_metadata
    try:
        if (doc.get('mode') != 'manual_draft' or doc.get('redo') or
                native.get('map') != doc.get('map') or native.get('candidate_mode') != 'manual_draft' or
                native.get('generation_complete') is not False or
                native.get('waypoint_metadata_contract') != 'TCE_NODE_METADATA_1'):
            return None
        nodes={n['id']:n for n in doc['nodes']}
        pairs={(e['from'],e['to']) for e in doc['edges']}
        rows={n['id']:n for n in native['nodes']}
        links={(e['from'],e['to']):e for e in native['edges']}
        if (len(nodes)!=len(doc['nodes']) or len(rows)!=len(native['nodes']) or set(rows)!=set(nodes) or
                len(pairs)!=len(doc['edges']) or len(links)!=len(native['edges']) or set(links)!=pairs):return None
        coverage=_rejected_input_coverage(native,rows,links)
        if coverage is None:return None
        quarantined,rejected=coverage

        def matches(report, expected_nodes, expected_pairs):
            rr={n['id']:n for n in report['nodes']}; ee={(e['from'],e['to']):e for e in report['edges']}
            if (report.get('map')!=doc['map'] or report.get('candidate_mode')!='manual_draft' or
                    report.get('waypoint_metadata_contract')!='TCE_NODE_METADATA_1' or
                    len(rr)!=len(report['nodes']) or set(rr)!=set(expected_nodes) or
                    len(ee)!=len(report['edges']) or set(ee)!=expected_pairs or
                    _rejected_input_coverage(report,rr,ee,require_rejected=False) is None):return False
            offset=report['bbox']['origin_above_feet']
            if not math.isfinite(offset) or offset!=native['bbox']['origin_above_feet']:return False
            for identity,node in expected_nodes.items():
                row=rr[identity];raw=row['raw_position'];name,facing,uid=waypoint_metadata(node)
                if (row.get('nav_flags')!=node['nav_flags'] or type(row.get('crouch')) is not bool or
                        bool(node.get('crouch'))!=bool(node['nav_flags']&64) or
                        any(not math.isfinite(v) for v in raw+node['position']) or
                        math.dist([raw[0],raw[1],raw[2]-offset],node['position'])>.011 or
                        abs(row.get('arrival',{}).get('requested_radius',row['radius'])-node['radius'])>.001 or row.get('import_uid',0)!=uid or
                        row.get('name','')!=name or math.dist(row.get('facing',[0,0,0]),facing)>.0002 or
                        row.get('custom_properties',{})!=custom_way_properties(node)):
                    return False
                if row['crouch']!=bool(node['crouch']):
                    # A fresh ordinary crouch requirement can coexist with a
                    # rejected shortcut. Preserve the authored flags here;
                    # its separate promotion still needs a whole-graph retry.
                    if (node['nav_flags']!=0 or row['crouch'] is not True or action_radius_node(node) or
                            row.get('editor_role',0) or row.get('door_recipe') or not any(
                                e.get('accepted') is True and e.get('crouch') is True and not e.get('flags',0) and
                                identity in pair and e.get('reason') in ('accepted','native_validated_walk','external_native_validated_walk')
                                for pair,e in ee.items())):return False
            return True

        if not matches(native,nodes,pairs):return None
        actions={r[key] for r in native.get('traversal_routes',[]) for key in
                 ('low','high','approach','launch','landing','entry','exit','paired_jump_launch')
                 if type(r.get(key)) is int and r[key]>=0}
        def ordinary(identity):
            return (identity in nodes and identity not in quarantined and identity not in actions and
                    nodes[identity]['nav_flags'] in (0,64) and not action_radius_node(nodes[identity]) and
                    not rows[identity].get('editor_role',0) and not rows[identity].get('door_recipe'))
        reasons={'steep_ground','blocked_clearance','blocked_sweep','blocked_step_up','blocked_step_down',
                 'different_floor','gap_or_drop','capsule_support_gap_or_drop','capsule_support_step_or_drop'}
        wanted={pair:e for pair,e in rejected.items() if e.get('reason') in reasons and
                not e.get('flags',0) and all(ordinary(i) for i in pair)}
        if not wanted:return None
        archives=[]
        for digest,raw in list(source_archives.items())[:32]:
            if type(raw) is not bytes or hashlib.sha256(raw).hexdigest()!=digest:continue
            report=json.loads(raw)
            archives.append((digest,report))
        state_nodes=copy.deepcopy(nodes);state_pairs=set(pairs)
        removed={};origins=[]
        history=doc.get('undo',[])
        for depth,patch in enumerate(reversed(history[-max_history:]),1):
            # A manual operation or an unaccounted coordinate/action edit is
            # a hard history boundary, never an invitation to undo the user.
            if patch.get('op')!='automatic_repair':break
            before={n['id']:n for n in patch['before']};after={n['id']:n for n in patch['after']}
            added={tuple(e) for e in patch['added']};deleted={tuple(e) for e in patch['removed']}
            if (len(before)!=len(patch['before']) or len(after)!=len(patch['after']) or set(before)!=set(after) or
                    len(added)!=len(patch['added']) or len(deleted)!=len(patch['removed']) or added&deleted or
                    not added<=state_pairs or deleted&state_pairs or
                    any(len(e)!=2 or e[0]==e[1] or not set(e)<=set(nodes) for e in added|deleted)):
                break
            valid=True
            for identity,old in before.items():
                expected=copy.deepcopy(old);expected.update(nav_flags=64,crouch=True)
                if (old.get('nav_flags')!=0 or old.get('crouch') is not False or
                        after[identity]!=expected or state_nodes.get(identity)!=after[identity] or
                        not ordinary(identity)):valid=False;break
            if not valid:break
            state_nodes.update(copy.deepcopy(before));state_pairs=(state_pairs-added)|deleted
            eligible=(added & set(wanted))-set(removed)
            if not eligible:continue
            sources=[digest for digest,report in archives if matches(report,state_nodes,state_pairs)]
            if not sources:continue
            # Nodes/endpoints have not been moved or reinterpreted anywhere
            # in this verified suffix. Only the rejected additions are removed.
            removed.update({pair:wanted[pair] for pair in eligible})
            origins.append(dict(undo_index=len(history)-depth,automatic_revision=doc['revision']-depth+1,
                                source_native_report=sources[0],edges=[list(e) for e in sorted(eligible)]))
        if not removed:return None
        proposal=copy.deepcopy(doc)
        proposal['edges']=[e for e in proposal['edges'] if (e['from'],e['to']) not in removed]
        return proposal,dict(model='rejected_historical_automatic_shortcuts_v1',source_graph_sha256=graph_hash(doc),
            rejected_edges=[dict(edge=list(pair),reason=e['reason'],endpoints=[fingerprint(nodes[i]) for i in pair])
                            for pair,e in sorted(removed.items())],history_origins=origins,
            retained_nodes=len(nodes),retained_directed_edges=len(proposal['edges']),changes_node_metadata=False,
            preserved_quarantined_source_ids=sorted(quarantined),native_revalidation_required=True)
    except (KeyError,IndexError,TypeError,ValueError,OverflowError,UnicodeError):return None


def _new_automatic_local_chords(doc,native,binding,nodes,pairs):
    """Identify only literal new direct chords from the last bound auto patch.

    This is origin evidence, never clearance or action proof. The current
    accepted ordinary graph must independently provide the replacement path.
    Mixed patches may append action helpers; none are undone or reinterpreted.
    """
    from edit import custom_way_properties,graph_hash,waypoint_metadata
    try:
        raw=binding['automatic_shortcut_receipt_bytes'];source_raw=binding['automatic_shortcut_source_native_bytes']
        rh=binding['automatic_shortcut_receipt_sha256'];sh=binding['automatic_shortcut_source_native_sha256']
        if (type(raw)is not bytes or type(source_raw)is not bytes or
                hashlib.sha256(raw).hexdigest()!=rh or hashlib.sha256(source_raw).hexdigest()!=sh):return set(),None
        receipt=json.loads(raw);source=json.loads(source_raw);patch=doc['undo'][-1]
        if (receipt.get('model')!='current_authored_mesh_corridor_proposals_v1' or
                receipt.get('source_native_report')!=sh or receipt.get('proposal_graph_sha256')!=graph_hash(doc) or
                receipt.get('draft_revision')!=doc['revision'] or doc.get('redo') or
                receipt.get('candidate')!=dict(name=binding['report_status']['candidate'],
                    sha256=binding['inputs']['candidate'],metadata=binding['inputs']['candidate_metadata']) or
                patch.get('op')!='automatic_repair' or patch.get('removed')!=[] or
                receipt.get('deleted_nodes')!=0 or receipt.get('replaced_rejected_edges')!=[]):return set(),None
        before={n['id']:n for n in patch['before']};after={n['id']:n for n in patch['after']}
        added={tuple(e)for e in patch['added']}
        if (len(before)!=len(patch['before']) or len(after)!=len(patch['after']) or
                not set(before)<=set(after) or len(added)!=len(patch['added']) or not added<=pairs or
                any(nodes.get(i)!=n for i,n in after.items()) or
                receipt.get('added_nodes')!=len(set(after)-set(before)) or
                receipt.get('added_directed_edges')!=len(added)):return set(),None
        previous={i:n for i,n in nodes.items()if i not in after};previous.update(before)
        old_pairs=pairs-added
        if (len(receipt['retained_node_ids'])!=len(previous) or set(receipt['retained_node_ids'])!=set(previous) or
                any(a not in previous or b not in previous for a,b in old_pairs)):return set(),None
        rows={n['id']:n for n in source['nodes']};links={(e['from'],e['to']):e for e in source['edges']}
        if (source.get('map')!=doc['map'] or source.get('candidate_mode')!='manual_draft' or
                source.get('waypoint_metadata_contract')!='TCE_NODE_METADATA_1' or
                len(rows)!=len(source['nodes']) or set(rows)!=set(previous) or
                len(links)!=len(source['edges']) or set(links)!=old_pairs or
                source.get('bbox')!=native.get('bbox') or
                _rejected_input_coverage(source,rows,links,require_rejected=False,require_no_exit=False)is None):return set(),None
        offset=source['bbox']['origin_above_feet']
        for i,node in previous.items():
            row=rows[i];raw_position=row['raw_position'];name,facing,uid=waypoint_metadata(node)
            if (row.get('nav_flags')!=node['nav_flags'] or type(row.get('crouch'))is not bool or
                    bool(node['crouch'])!=bool(node['nav_flags']&64) or
                    len(raw_position)!=3 or any(not math.isfinite(v)for v in raw_position+node['position']) or
                    math.dist(raw_position[:2]+[raw_position[2]-offset],node['position'])>.011 or
                    abs(row.get('arrival',{}).get('requested_radius',row['radius'])-node['radius'])>.001 or
                    row.get('import_uid',0)!=uid or row.get('name','')!=name or
                    math.dist(row.get('facing',[0,0,0]),facing)>.0002 or
                    row.get('custom_properties',{})!=custom_way_properties(node)):return set(),None
            if row['crouch']!=bool(node['crouch']):
                if (node['nav_flags']!=0 or row['crouch']is not True or not any(
                        e.get('accepted')is True and e.get('crouch')is True and not e.get('flags',0) and
                        e.get('reason')in('accepted','native_validated_walk','external_native_validated_walk') and
                        i in pair for pair,e in links.items())):return set(),None
        eligible=set()
        for route in receipt['routes']:
            if (route.get('kind')!='same_component_local_gap' or
                    route.get('status')!='mesh_proposal_requires_native_revalidation'):continue
            chain=route.get('chain');edge=route.get('edge')
            if (not isinstance(chain,list)or len(chain)!=2 or chain!=edge or
                    any(type(i)is not int or i not in previous or previous[i]!=nodes[i]for i in chain)):continue
            candidates={tuple(chain)}
            if route.get('bidirectional')is True:candidates.add(tuple(reversed(chain)))
            eligible|=candidates&added
        return eligible,dict(receipt_sha256=rh,source_native_report=sh,draft_revision=doc['revision'])
    except(KeyError,IndexError,TypeError,ValueError,OverflowError,UnicodeError):return set(),None


def rejected_ordinary_edge_feedback(doc, native, binding):
    """Withdraw narrowly identified disproved edges over a proved alternate.

    ``binding`` contains the caller's current report_status, native-attempt,
    proof_status(receipt=False).inputs, literal candidate metadata, and raw
    native_report_bytes/candidate_bytes/candidate_metadata_bytes. The
    caller must recheck their file hashes/graph before apply_repair, retain a
    separate withdrawal receipt, then fully validate the resulting candidate.
    Only ordinary support rejects and an exact generated redundant ladder
    return recipe qualify. This never changes a node, promotes an action or
    accepts the failed WAY.
    """
    from edit import custom_way_properties, fingerprint, graph_hash, waypoint_metadata
    try:
        status, attempt = binding['report_status'], binding['attempt']
        inputs, metadata = binding['inputs'], binding['metadata']
        exported, edited = doc['export'], metadata['edit_draft']
        source_graph = graph_hash(doc)
        raw_report, raw_candidate, raw_metadata = (binding[key] for key in
            ('native_report_bytes', 'candidate_bytes', 'candidate_metadata_bytes'))
        if (any(type(raw) is not bytes for raw in (raw_report, raw_candidate, raw_metadata)) or
                hashlib.sha256(raw_report).hexdigest() != status.get('native_report') or
                hashlib.sha256(raw_candidate).hexdigest() != inputs.get('candidate') or
                hashlib.sha256(raw_metadata).hexdigest() != inputs.get('candidate_metadata') or
                json.loads(raw_report) != native or json.loads(raw_metadata) != metadata or
                hashlib.sha256(raw_candidate.decode('ascii').replace('\r\n', '\n').replace('\r', '\n').encode('ascii')).hexdigest()
                    != metadata.get('candidate_sha256')):
            return None
        hashes = ('candidate', 'candidate_metadata', 'stand', 'duck', 'geometry',
                  'bsp', 'collision', 'runtime_snapshot', 'game_module')
        if (doc.get('mode') != 'manual_draft' or native.get('candidate_mode') != 'manual_draft' or
                metadata.get('candidate_mode') != 'manual_draft' or status.get('current') is not True or
                any(obj.get('map') != doc.get('map') for obj in (native, attempt, metadata)) or
                native.get('generation_complete') is not False or attempt.get('generation_complete') is not False or
                native.get('waypoint_metadata_contract') != 'TCE_NODE_METADATA_1' or
                doc['saved_revision'] != doc['revision'] or edited.get('revision') != doc['revision'] or
                edited.get('sha256') != source_graph or exported.get('revision') != doc['revision'] or
                exported.get('sha256') != source_graph or
                exported.get('candidate_sha256') != metadata.get('candidate_sha256') or
                not re.fullmatch('[0-9a-f]{64}', metadata.get('candidate_sha256', '')) or
                any(not re.fullmatch('[0-9a-f]{64}', inputs.get(key, '')) for key in hashes) or
                not inputs.get('navigation_contract') or attempt.get('inputs') != inputs or
                status.get('candidate_sha256') != inputs['candidate'] or
                status.get('candidate') != exported.get('candidate') or
                attempt.get('candidate') != status['candidate'] or
                native.get('source', '').replace('\\', '/').split('/')[-1] != status['candidate'] or
                not re.fullmatch('[0-9a-f]{64}', status.get('native_report', '')) or
                attempt.get('native_report') != status['native_report'] or
                attempt.get('staged_candidate') != inputs['candidate']):
            return None
        environment = doc['base']['environment']
        if (edited.get('base') != doc['base'] or not environment or
                any(inputs.get(key) != value for key, value in environment.items()) or
                any(metadata.get('sources', {}).get(key, {}).get('sha256') != inputs[key]
                    for key in ('stand', 'duck', 'geometry'))):
            return None
        nodes = {n['id']: n for n in doc['nodes']}
        rows = {n['id']: n for n in native['nodes']}
        candidates = {n['id']: n for n in metadata['nodes']}
        pairs = {(e['from'], e['to']) for e in doc['edges']}
        links = {(e['from'], e['to']): e for e in native['edges']}
        if (not nodes or len(nodes) != len(doc['nodes']) or len(rows) != len(native['nodes']) or
                len(candidates) != len(metadata['nodes']) or set(rows) != set(nodes) or set(candidates) != set(nodes) or
                len(pairs) != len(doc['edges']) or len(links) != len(native['edges']) or set(links) != pairs or
                len(metadata['edges']) != len(pairs) or {tuple(pair) for pair in metadata['edges']} != pairs):
            return None
        offset = native['bbox']['origin_above_feet']
        if type(offset) not in (int, float) or not math.isfinite(offset):
            return None
        checked = _rejected_input_coverage(native, rows, links, require_no_exit=False)
        if checked is None:
            return None
        quarantined, rejected = checked
        for identity, node in nodes.items():
            row = rows[identity]; raw, projected = row['raw_position'], row['position']
            name, facing, uid = waypoint_metadata(node)
            if (any(candidates[identity].get(key) != value for key, value in node.items()) or
                    (row.get('accepted') is not True and identity not in quarantined) or
                    row.get('native_input', {}).get('floor_accepted') is not True or
                    row.get('nav_flags') != node['nav_flags'] or type(row.get('crouch')) is not bool or
                    row['crouch'] != bool(node['nav_flags'] & 64) or bool(node.get('crouch')) != row['crouch'] or
                    len(raw) != 3 or len(projected) != 3 or len(node['position']) != 3 or
                    any(not math.isfinite(v) for v in raw + projected + node['position']) or
                    math.dist([raw[0], raw[1], raw[2]-offset], node['position']) > .011 or
                    math.dist(raw[:2], projected[:2]) > .011 or abs(raw[2]-projected[2]) > 18.25 or
                    row.get('import_uid', 0) != uid or (uid and row.get('way_guid', 0) not in (0, uid)) or
                    row.get('name', '') != name or math.dist(facing, row.get('facing', [0, 0, 0])) > .0002 or
                    custom_way_properties(node) != row.get('custom_properties', {})):
                return None
        action_ids = {route[key] for route in native.get('traversal_routes', []) for key in
                      ('low', 'high', 'approach', 'launch', 'landing', 'entry', 'exit', 'paired_jump_launch',
                       'release', 'alignment', 'ascent_release', 'ascent_alignment')
                      if type(route.get(key)) is int and route[key] >= 0}
        protected = {tuple(pair) for pair in doc['base'].get('protected_action_edges', [])}
        def ordinary(identity):
            node, row = nodes[identity], rows[identity]
            plain_helper=(node.get('generated_guide') is True and node.get('editor_role',0) in (0,1) and
                          row.get('editor_role',0) in (0,1) and node.get('source_reason') in
                          ('generated_native_path_repair_bend','automatic_repair_corridor'))
            ordinary_role=plain_helper or (not row.get('editor_role',0) and not node.get('editor_role',0) and
                                          not node.get('generated_guide'))
            return (row.get('accepted') is True and row.get('spawn_reachable') is True and
                    node['nav_flags'] in (0, 64) and identity not in action_ids and
                    ordinary_role and not row.get('door_recipe') and not node.get('door_recipe') and
                    not any(node.get(key) for key in ('action_roles', 'action_properties')))
        walk_reasons = {'external_native_validated_walk', 'native_validated_walk', 'accepted',
                        'manual_native_transition_live_pending'}
        allowed = {identity for identity in nodes if ordinary(identity)}
        # Rejected edges never help each other pass. All alternates are already
        # accepted ordinary walks, so they survive every withdrawal in this call.
        adjacency = defaultdict(list)
        transitions = defaultdict(list)
        for pair, edge in links.items():
            if (edge.get('accepted') is True and edge.get('reason') in walk_reasons and
                    not edge.get('flags', 0) and pair not in protected and all(i in allowed for i in pair) and
                    (not edge.get('crouch') or any(nodes[i]['nav_flags'] & 64 for i in pair))):
                length = math.dist(nodes[pair[0]]['position'], nodes[pair[1]]['position'])
                if length > .000001: adjacency[pair[0]].append((pair[1], length))
            if (edge.get('accepted') is True and not edge.get('flags', 0) and
                    all(rows[i].get('accepted') is True and rows[i].get('spawn_reachable') is True for i in pair) and
                    (edge.get('reason') == 'manual_native_transition_live_pending' or
                     any(target == pair[1] for target, _ in adjacency[pair[0]]))):
                length = math.dist(nodes[pair[0]]['position'], nodes[pair[1]]['position'])
                if length > .000001: transitions[pair[0]].append((pair[1], length))
        for edges in adjacency.values(): edges.sort()
        for edges in transitions.values(): edges.sort()
        new_chords,chord_origin=_new_automatic_local_chords(doc,native,binding,nodes,pairs)
        def generated_chord_alternate(start,end):
            # Positive ordinary lengths yield a simple shortest path with at
            # most N-1 hops. No expanded hop-state search or action edge is
            # needed, even when two nearby surfaces connect far around a room.
            queue=[(0.,start)];distance={start:0.};previous={};settled=set()
            while queue and len(settled)<len(allowed):
                cost,identity=heapq.heappop(queue)
                if identity in settled or cost!=distance[identity]:continue
                settled.add(identity)
                if identity==end:
                    path=[identity]
                    while identity in previous:identity=previous[identity];path.append(identity)
                    return list(reversed(path)),cost
                for target,length in adjacency[identity]:
                    if target not in settled and cost+length<distance.get(target,math.inf):
                        distance[target]=cost+length;previous[target]=identity
                        heapq.heappush(queue,(cost+length,target))
            return None
        def alternate(start, end, maximum, graph, max_hops):
            queue = [(0., 0, start)]; distance = {(start, 0): 0.}; previous = {}
            while queue:
                cost, hops, identity = heapq.heappop(queue)
                if distance[(identity, hops)] != cost: continue
                if identity == end:
                    state = (identity, hops); path = [identity]
                    while state in previous:
                        state = previous[state]; path.append(state[0])
                    return list(reversed(path)), cost
                if hops == max_hops: continue
                for target, length in graph[identity]:
                    proposed = cost + length; state = (target, hops+1)
                    if proposed <= maximum and proposed < distance.get(state, math.inf):
                        distance[state] = proposed; previous[state] = (identity, hops)
                        heapq.heappush(queue, (proposed, hops+1, target))
            return None
        reasons = ORDINARY_COLLISION_REJECTIONS
        incoming, outgoing = defaultdict(set), defaultdict(set)
        for start, end in pairs: incoming[end].add(start); outgoing[start].add(end)
        def ladder_guide(identity, reason, role):
            node, row = nodes[identity], rows[identity]
            return (node.get('generated_guide') is True and node['nav_flags'] == 512 and node.get('crouch') is False and
                    node.get('source_reason') == reason and row.get('editor_role') == role and
                    row.get('accepted') is True and row.get('spawn_reachable') is True and
                    not row.get('door_recipe') and not node.get('action_roles') and not node.get('action_properties'))
        def ladder_reverse(pair, edge):
            start, end = pair
            if (edge.get('reason') != 'capsule_support_gap_or_drop' or
                    not ladder_guide(start, 'ladder_top_clearance_live_pending', 33) or
                    not ladder_guide(end, 'ladder_exit_floor_live_pending', 1) or
                    incoming[start] != {end} or len(outgoing[start]) != 2 or end not in outgoing[start]):
                return None
            interior = next(iter(outgoing[start]-{end}))
            if (not ladder_guide(interior, 'ladder_inside_guidance_not_standable_live_pending', 17) or
                    not all(links[p].get('accepted') is True and
                            links[p].get('reason') == 'manual_native_transition_live_pending' and
                            not links[p].get('flags', 0) for p in ((start, interior), (end, start)))):
                return None
            height = nodes[start]['position'][2]-nodes[interior]['position'][2]
            if height <= 0: return None
            return interior, height, min(2048., 2*height+256.)
        def jump_approach(pair, edge):
            start,end=pair;node,row=nodes[end],rows[end]
            if (edge.get('reason') not in reasons or start not in allowed or
                    node.get('generated_guide') is not True or
                    node.get('source_reason')!='generated_jump_takeoff_live_pending' or
                    node['nav_flags']!=1<<27 or node.get('crouch') is not False or
                    node.get('editor_role',0) not in (0,1) or row.get('editor_role')!=1 or
                    row.get('crouch') is not False or row.get('accepted') is not True or
                    row.get('spawn_reachable') is not True or row.get('editor_drop_target',-1)!=-1 or
                    row.get('door_recipe') or any(node.get(key) for key in
                        ('action_roles','action_properties','door_recipe')) or len(outgoing[end])!=1):return None
            landing=next(iter(outgoing[end]));jump=links[(end,landing)]
            if (jump.get('accepted') is not True or jump.get('crouch',False) or jump.get('flags',0) or
                    jump.get('reason')!='manual_native_transition_live_pending' or
                    rows[landing].get('accepted') is not True or rows[landing].get('spawn_reachable') is not True):return None
            # The generated JUMP is a terminal only. Its existing outgoing
            # action is preserved, never traversed as part of the walk proof.
            graph=defaultdict(list,{i:list(edges) for i,edges in adjacency.items()})
            for previous in sorted(incoming[end]):
                link=links[(previous,end)]
                if (previous not in allowed or nodes[previous]['nav_flags']!=0 or
                        link.get('accepted') is not True or link.get('crouch',False) or link.get('flags',0) or
                        link.get('reason') not in walk_reasons or (previous,end) in protected):continue
                length=math.dist(nodes[previous]['position'],node['position'])
                if length>.000001:graph[previous].append((end,length))
            return graph,landing
        withdrawn = []
        for pair, edge in sorted(rejected.items()):
            if edge.get('flags', 0) or pair in protected:
                continue
            recipe = None;jump = None
            if edge.get('reason') in reasons and all(identity in allowed for identity in pair):
                kind = 'ordinary_walk'; hops = 12; graph = adjacency
                direct = math.dist(nodes[pair[0]]['position'], nodes[pair[1]]['position'])
                maximum = min(2048.,max(8*direct,direct+512))
            else:
                jump=jump_approach(pair,edge)
                if jump is not None:
                    kind='generated_jump_approach';hops=12;graph,landing=jump
                    direct=math.dist(nodes[pair[0]]['position'],nodes[pair[1]]['position'])
                    maximum=max(4*direct,direct+256)
                else:
                    recipe = ladder_reverse(pair, edge)
                    if recipe is None: continue
                    kind = 'generated_ladder_reverse'; hops = 24; graph = transitions
                    interior, height, maximum = recipe
            result = alternate(*pair, maximum, graph, hops)
            chord=False
            if not result and kind=='ordinary_walk' and pair in new_chords:
                result=generated_chord_alternate(*pair);chord=bool(result)
            if result:
                path, length = result
                row = dict(edge=list(pair), kind=kind, reason=edge['reason'], alternate_source_ids=path,
                           hops=len(path)-1, length_units=length, maximum_length_units=maximum)
                if recipe: row.update(interior_source_id=interior, shaft_height_units=height)
                if jump is not None:row.update(jump_source_id=pair[1],preserved_jump_edge=[pair[1],landing])
                if chord:row.update(kind='new_automatic_local_gap_ordinary_walk',
                    automatic_addition_origin=copy.deepcopy(chord_origin),maximum_hops=len(allowed)-1,
                    maximum_length_units=None,search_scope='current_accepted_ordinary_graph_only')
                withdrawn.append(row)
        if not withdrawn:
            return None
        removed = {tuple(row['edge']) for row in withdrawn}
        proposal = copy.deepcopy(doc)
        proposal['edges'] = [e for e in proposal['edges'] if (e['from'], e['to']) not in removed]
        evidence = dict(model='native_rejected_edge_alternate_v2', draft_revision=doc['revision'],
                        source_graph_sha256=source_graph, source_native_report=status['native_report'],
                        candidate=status['candidate'], source_inputs=copy.deepcopy(inputs),
                        withdrawn_edges=withdrawn, retained_nodes=len(nodes),
                        preserved_quarantined_source_ids=sorted(quarantined),
                        remaining_ordinary_without_exit=list(native['native_input_coverage']['ordinary_without_exit']),
                        suppressed_automatic_shortcut_candidates=[dict(edge=row['edge'],reason=row['reason'],
                            endpoints=[fingerprint(nodes[i])for i in row['edge']])for row in withdrawn
                            if row['kind']=='new_automatic_local_gap_ordinary_walk'],
                        remaining_rejected_edges=[dict(edge=list(pair), reason=edge.get('reason'))
                                                  for pair, edge in sorted(rejected.items()) if pair not in removed],
                        retained_directed_edges=len(proposal['edges']), changes_positions=False,
                        changes_node_metadata=False, directed_reachability_preserved=True,
                        native_revalidation_required=True)
        return proposal, evidence
    except (AttributeError, KeyError, IndexError, TypeError, ValueError, OverflowError, UnicodeError):
        return None


def redundant_pending_jump_feedback(doc,native,binding,original_native_bytes):
    """Retire one unchanged machine jump helper with two proved walk bypasses.

    The original automatic report is mandatory provenance, not a present
    geometry certificate. Only the current bound report proves both ordinary
    directions. No ordinary point or user-edited action is eligible.
    """
    from pathrepair import validate_source
    from edit import waypoint_metadata,graph_hash
    validate_source(doc,native,native['bbox']['origin_above_feet'],binding)
    base=doc['base'];origin=native['bbox']['origin_above_feet']
    if (type(original_native_bytes) is not bytes or
            hashlib.sha256(original_native_bytes).hexdigest()!=base.get('native_report')):return None
    try:original=json.loads(original_native_bytes)
    except (ValueError,TypeError):return None
    if (original.get('map')!=doc['map'] or original.get('generation_complete') is not True or
            original.get('candidate_mode')!='automatic_spatial' or original.get('bbox')!=native['bbox'] or
            original.get('source','').replace('\\','/').split('/')[-1]!=base.get('candidate_name')):return None
    nodes={n['id']:n for n in doc['nodes']};rows={n['id']:n for n in native['nodes']}
    old={n['id']:n for n in original.get('nodes',[])}
    if len(old)!=len(original.get('nodes',[])):return None
    pairs={(e['from'],e['to']):e for e in native['edges']}
    protected={tuple(e) for e in base.get('protected_action_edges',[])}
    incoming,outgoing=defaultdict(set),defaultdict(set)
    for a,b in pairs:outgoing[a].add(b);incoming[b].add(a)
    def ordinary(identity):
        n,r=nodes[identity],rows[identity]
        return (r.get('accepted') is True and r.get('spawn_reachable') is True and
                n['nav_flags'] in(0,64) and not n.get('generated_guide') and not n.get('editor_role',0) and
                not r.get('editor_role',0) and not r.get('door_recipe') and
                not any(n.get(k)for k in('action_roles','action_properties','door_recipe')))
    allowed={i for i in nodes if ordinary(i)};adjacency=defaultdict(list)
    for (a,b),edge in pairs.items():
        if (a in allowed and b in allowed and edge.get('accepted') is True and not edge.get('flags',0) and
                (a,b) not in protected and edge.get('reason') in
                ('accepted','external_native_validated_walk','native_validated_walk','manual_native_transition_live_pending') and
                (not edge.get('crouch') or (nodes[a]['nav_flags']|nodes[b]['nav_flags'])&64)):
            length=math.dist(nodes[a]['position'],nodes[b]['position'])
            if length>.000001:adjacency[a].append((b,length))
    def alternate(start,end,limit):
        queue=[(0.,0,start)];distance={(start,0):0.};previous={}
        while queue:
            cost,hops,identity=heapq.heappop(queue)
            if distance[(identity,hops)]!=cost:continue
            if identity==end:
                state=(identity,hops);path=[identity]
                while state in previous:state=previous[state];path.append(state[0])
                return dict(source_ids=list(reversed(path)),length_units=cost,hops=hops,maximum_length_units=limit)
            if hops==12:continue
            for target,length in adjacency[identity]:
                proposed=cost+length;state=(target,hops+1)
                if proposed<=limit and proposed<distance.get(state,math.inf):
                    distance[state]=proposed;previous[state]=(identity,hops);heapq.heappush(queue,(proposed,hops+1,target))
        return None
    known={'blocked_clearance','blocked_sweep','blocked_step_up','blocked_step_down','different_floor',
           'gap_or_drop','step_or_drop','capsule_support_gap_or_drop','capsule_support_step_or_drop',
           'capsule_walk_startsolid','capsule_walk_step_up','capsule_walk_sweep','capsule_walk_step_down'}
    fields={'id','position','radius','source_radius','nav_flags','crouch','spawn','topology_protected','locked',
            'generated_guide','source_reason','editor_role','action_roles','action_properties','name','facing','way_guid'}
    for identity,n in sorted(nodes.items()):
        row=rows[identity];source=old.get(identity,{});floor=row.get('native_input',{})
        name,facing,uid=waypoint_metadata(n)
        if (set(n)-fields or n.get('generated_guide') is not True or n.get('locked') is not False or
                n.get('source_reason')!='generated_jump_takeoff_live_pending' or n['nav_flags']!=1<<27 or
                n.get('crouch') is not False or n.get('spawn') is not False or name or any(facing) or
                n.get('editor_role',0)!=0 or any(n.get(k)for k in('action_roles','action_properties')) or
                identity in base.get('protected_anchor_ids',[]) or
                row.get('editor_role')!=1 or row.get('editor_drop_target',-1)!=-1 or row.get('door_recipe') or
                row.get('accepted') is not False or row.get('spawn_reachable') is not False or
                row.get('reason')!='quarantined_no_validated_route_from_spawn' or
                floor.get('floor_accepted') is not True or floor.get('status')!='missing_spawn_route' or
                floor.get('initial_incoming')!=0 or floor.get('initial_outgoing')!=1 or
                floor.get('initial_rejected_incident_links')!=1 or len(incoming[identity])!=1 or len(outgoing[identity])!=1):continue
        if (source.get('accepted') is not True or source.get('reason')!='generated_jump_takeoff_live_pending' or
                source.get('nav_flags')!=1<<27 or source.get('crouch') is not False or source.get('name','') or
                any(source.get('facing',[0,0,0])) or source.get('editor_role',0) or source.get('custom_properties') or
                source.get('radius')!=n['radius'] or source.get('radius')!=n.get('source_radius') or
                source.get('topology_protected',False)!=n.get('topology_protected') or
                not uid or uid!=source.get('way_guid',source.get('index',-1)+1) or
                len(source.get('position',[]))!=3 or
                math.dist(source['position'][:2]+[source['position'][2]-origin],n['position'])>.011):continue
        # Any touch recorded since generation is a hard boundary. A user can
        # explicitly delete the point, but this cleanup never undoes edits.
        if any(any(v.get('id')==identity for k in('before','after')for v in patch.get(k,[])) or
               any(identity in pair for k in('added','removed')for pair in patch.get(k,[]))
               for history in('undo','redo')for patch in doc.get(history,[])):continue
        approach=next(iter(incoming[identity]));landing=next(iter(outgoing[identity]))
        if approach==landing or approach not in allowed or landing not in allowed:continue
        entry,pending=pairs[(approach,identity)],pairs[(identity,landing)]
        if (entry.get('accepted') is not False or entry.get('reason') not in known or entry.get('flags',0) or
                pending.get('accepted') is not False or pending.get('reason')!='quarantined_unreachable_endpoint' or
                pending.get('crouch') or pending.get('flags',0) or
                any(identity in pair for pair in protected)):continue
        original_edges=[e for e in original.get('edges',[])if identity in(e['from'],e['to'])]
        if (len(original_edges)!=2 or {(e['from'],e['to'])for e in original_edges}!={(approach,identity),(identity,landing)} or
                any(e.get('accepted') is not True or e.get('crouch') for e in original_edges)):continue
        routes=[r for r in original.get('traversal_routes',[])if identity in
                [r.get(k)for k in('approach','launch','landing','entry','exit','low','high','release','alignment')]]
        if (len(routes)!=1 or routes[0].get('kind')!='jump' or routes[0].get('exported') is not True or
                any(routes[0].get(k)!=v for k,v in(('approach',approach),('launch',identity),('landing',landing)))):continue
        # No other authored recipe may refer to the removed helper.
        if any(any(r.get('landing_node')==identity for r in other.get('action_roles',[])) or
               identity in [other.get('door_recipe',{}).get(k)for k in('target','entry','use_source')]
               for other in nodes.values()if other['id']!=identity):continue
        direct=math.dist(nodes[approach]['position'],nodes[landing]['position'])
        limit=min(2048.,4*direct)
        forward,backward=alternate(approach,landing,limit),alternate(landing,approach,limit)
        if not forward or not backward:continue
        proposal=copy.deepcopy(doc);proposal['nodes']=[v for v in proposal['nodes']if v['id']!=identity]
        proposal['edges']=sorted([e for e in proposal['edges']if identity not in(e['from'],e['to'])],
                                 key=lambda e:(e['from'],e['to']))
        receipt=dict(model='redundant_unentered_pending_jump_v1',removed_node=copy.deepcopy(n),
            removed_incident_edges=[copy.deepcopy(e)for e in doc['edges']if identity in(e['from'],e['to'])],
            native_incident_edges=[copy.deepcopy(entry),copy.deepcopy(pending)],original_jump_route=routes[0],
            ordinary_alternatives=[forward,backward],tombstoned_source_id=identity,tombstoned_way_guid=uid,
            next_id_retained=doc['next_id'],source_graph_sha256=graph_hash(doc),draft_revision=doc['revision'],
            source_native_report=binding['report_status']['native_report'],original_native_report=base['native_report'],
            candidate=binding['report_status']['candidate'],source_inputs=copy.deepcopy(binding['inputs']),
            native_revalidation_required=True,installed=False)
        return proposal,receipt
    return None


def parse_native_probe(reply, start, end, origin_above_feet):
    """Bind the readonly native diagnostic to the exact literal feet query.

    Text alone never publishes an edge. A later full native import and binary
    audit must validate the complete proposal with its current source hashes.
    """
    number = r"[-+0-9.eE]+"
    vector = r"\((" + number + r") (" + number + r") (" + number + r")\)"
    matches = list(re.finditer(r"navgen probe -1->-1 feet=" + vector + "->" + vector +
                      r" stand=([a-z0-9_]+) crouch=([a-z0-9_]+) traces=\d+ quarantined=0/0 "
                      r"\(diagnostic only; graph unchanged\)", reply))
    if len(matches) != 1:
        raise RuntimeError("Native Wegprobe lieferte keine eindeutige Antwort für die angefragten Fußpunkte.")
    match = matches[0]
    coordinates = [float(v) for v in match.groups()[:6]]
    if any(not math.isfinite(v) or abs(v-expected) > .011 for v, expected in zip(coordinates, list(start)+list(end))):
        raise RuntimeError("Native Wegprobe gehört zu anderen Fußpunkten; keine Reparatur übernommen.")
    result = dict(stand=match[7], crouch=match[8], reply_sha256=hashlib.sha256(reply.encode()).hexdigest())
    imports=list(re.finditer(r"navgen probe import endpoint=([01]) crouch=([01]) accepted=([01]) exact=([01]) "
                            r"reason=([a-z0-9_]+) feet="+vector,reply))
    if imports or 'navgen probe import' in reply:
        if len(imports)!=4 or {(item[1],item[2]) for item in imports}!={('0','0'),('0','1'),('1','0'),('1','1')}:
            raise RuntimeError('Native Importprobe fehlt oder ist mehrdeutig; keine Ankerkorrektur übernommen.')
        result['imports']={'start':{},'end':{}}
        for item in imports:
            feet=[float(v) for v in item.groups()[5:]]
            expected=start if item[1]=='0' else end
            if (any(not math.isfinite(v) or abs(v)>131072 for v in feet) or
                    item[4]=='1' and (item[3]!='1' or math.dist(feet,expected)>.011)):
                raise RuntimeError('Ungültige oder fremde native Importpose.')
            result['imports']['start' if item[1]=='0' else 'end']['crouch' if item[2]=='1' else 'stand']=dict(
                accepted=item[3]=='1',exact=item[4]=='1',reason=item[5],feet=feet)
    action_import_lines=re.findall(r'navgen probe action_import[^\r\n]*',reply)
    action_walk_lines=re.findall(r'navgen probe action_walk[^\r\n]*',reply)
    if ((action_import_lines or action_walk_lines) and
            (not action_import_lines or not action_walk_lines or 'imports' not in result)):
        raise RuntimeError('Native Aktionsprobe ist unvollständig; keine Reparatur übernommen.')
    if action_import_lines:
        pattern=(r'navgen probe action_import kind=drop_launch endpoint=([01]) accepted=([01]) exact=([01]) '
                 r'reason=([a-z0-9_]+) feet='+vector)
        records=[re.fullmatch(pattern,line) for line in action_import_lines]
        if (len(records)!=2 or any(row is None for row in records) or
                {row[1] for row in records}!={'0','1'}):
            raise RuntimeError('Native Aktionsimportprobe fehlt oder ist mehrdeutig.')
        result['action_imports']={}
        for row in records:
            expected=start if row[1]=='0' else end
            feet=[float(v) for v in row.groups()[4:]]
            if (any(not math.isfinite(v) or abs(v)>131072 for v in feet) or
                    (row[2]=='1')!=(row[4]=='accepted') or
                    row[3]=='1' and (row[2]!='1' or math.dist(feet,expected)>.011)):
                raise RuntimeError('Ungültige oder fremde native Aktionsimportpose.')
            result['action_imports']['start' if row[1]=='0' else 'end']={'drop_launch':dict(
                accepted=row[2]=='1',exact=row[3]=='1',reason=row[4],feet=feet)}
    if action_walk_lines:
        pattern=r'navgen probe action_walk kind=drop_launch endpoint=([01]) accepted=([01]) reason=([a-z0-9_]+)'
        records=[re.fullmatch(pattern,line) for line in action_walk_lines]
        if (len(records)!=2 or any(row is None for row in records) or
                {row[1] for row in records}!={'0','1'} or 'action_imports' not in result):
            raise RuntimeError('Native Aktionswegprobe fehlt oder ist mehrdeutig.')
        result['action_walks']={}
        for row in records:
            endpoint='start' if row[1]=='0' else 'end'
            imported=result['action_imports'][endpoint]['drop_launch']
            if ((row[2]=='1')!=(row[3]=='accepted') or
                    row[2]=='1' and not (imported['accepted'] and imported['exact'])):
                raise RuntimeError('Native Aktionswegprobe hat keinen exakten Aktionsimport.')
            result['action_walks'][endpoint]={'drop_launch':dict(accepted=row[2]=='1',reason=row[3])}
    drops = list(re.finditer(r"navgen probe drop -1->-1 accepted=([01]) reason=([a-z0-9_]+) "
                     r"crash_limit=38 max_drop=(" + number + r") delta=(" + number + r") landing=" + vector +
                     r" \(readonly samples\)", reply))
    if len(drops)>1 or end[2]<start[2]-18.25 and len(drops)!=1:
        raise RuntimeError("Native Fallprobe fehlt oder ist mehrdeutig; keine Reparatur übernommen.")
    drop = drops[0] if drops else None
    if drop:
        values = [float(v) for v in drop.groups()[2:]]
        if not all(math.isfinite(v) for v in values): raise RuntimeError("Ungültige native Fallprobe.")
        result['drop'] = dict(accepted=drop[1]=='1', reason=drop[2], max_drop=values[0], delta=values[1],
                              touchdown=[values[2],values[3],values[4]-origin_above_feet])
    climbs = list(re.finditer(r"navgen probe freeclimb -1->-1 accepted=([01]) launch=" + vector +
                      r" max_ledge=(" + number + r") duration=(" + number + r") stamina_used=(" + number +
                      r") \(readonly3speed samples; graph unchanged\)", reply))
    if len(climbs)>1 or end[2]-start[2]>=63 and len(climbs)!=1:
        raise RuntimeError("Native Freikletterprobe fehlt oder ist mehrdeutig; keine Reparatur übernommen.")
    climb = climbs[0] if climbs else None
    if climb:
        values = [float(v) for v in climb.groups()[1:]]
        if not all(math.isfinite(v) for v in values): raise RuntimeError("Ungültige native Freikletterprobe.")
        result['free_climb'] = dict(accepted=climb[1]=='1', launch=[values[0],values[1],values[2]-origin_above_feet],
                                    max_ledge=values[3], duration=values[4], stamina_used=values[5])
    if 'navgen probe jump' in reply:
        # Native records are independent, small G_Printf lines. Bind every
        # part to this exact feet query; a positive boolean without its full
        # 27-sample standing proof never creates an authored action.
        def unique_object(pairs):
            value={}
            for key,item in pairs:
                if key in value:raise ValueError('duplicate JSON key')
                value[key]=item
            return value
        def finite_vector(value):
            return (isinstance(value,list) and len(value)==3 and
                    all(type(v) in (int,float) and math.isfinite(v) and abs(v)<=131072 for v in value))
        try:
            records={}
            for line in re.findall(r'navgen probe jump ([^\r\n]*)',reply):
                row=json.loads(line,object_pairs_hook=unique_object)
                part=row['part']
                if part not in ('points','running','standing','standing_proof') or part in records:
                    raise ValueError('duplicate or unknown jump part')
                records[part]=row
            if not {'points','running','standing'}<=records.keys():raise ValueError('missing jump part')
            points=records['points']
            if (points.get('coordinates')!='feet' or points.get('readonly') is not True or
                    points.get('live_verified') is not False or
                    not all(finite_vector(points.get(key)) for key in ('from','to')) or
                    math.dist(points['from'],start)>.011 or math.dist(points['to'],end)>.011):
                raise ValueError('jump points do not match query')
            reasons={'accepted','gravity_or_stance','horizontal_range','height_range','no_real_arc',
                     'return_crash_damage','horizontal_speed','trace_budget','start_or_allsolid',
                     'blocked_hull_arc','liquid_hazard','hurt_trigger','landing_continuation','unstable_landing'}
            for part in ('running','standing'):
                row=records[part]
                if (type(row.get('accepted')) is not bool or row.get('reason') not in reasons or
                        row['accepted']!=(row['reason']=='accepted') or
                        type(row.get('queries')) is not int or not 0<=row['queries']<=2000000 or
                        not finite_vector(row.get('hit'))):raise ValueError('invalid jump result')
            standing=records['standing']['accepted'];proof=records.get('standing_proof')
            if standing!=(proof is not None):raise ValueError('standing proof missing or contradicts result')
            if proof is not None:
                if (type(proof.get('samples')) is not int or proof['samples']!=27 or
                        type(proof.get('duration')) not in (int,float) or not math.isfinite(proof['duration']) or
                        not 0<proof['duration']<=2 or type(proof.get('crash')) not in (int,float) or
                        not math.isfinite(proof['crash']) or not 0<=proof['crash']<=38 or
                        not finite_vector(proof.get('touchdown')) or
                        math.dist(proof['touchdown'][:2],end[:2])>96.01 or
                        abs(proof['touchdown'][2]-end[2])>18.01 or end[2]-start[2]<=18.25):
                    raise ValueError('invalid standing jump proof')
            result['jump']=records
        except (ValueError,TypeError,KeyError,OverflowError) as error:
            raise RuntimeError('Native Sprungprobe fehlt, ist ungültig oder gehört zu anderen Fußpunkten.') from error
    return result


def _accepted_native_components(native):
    """Return the literal accepted graph and its directed strong components."""
    actual = {n['id']:n for n in native.get('nodes',[]) if n.get('accepted') is True}
    forward, reverse = {i:set() for i in actual}, {i:set() for i in actual}
    accepted = set()
    for edge in native.get('edges',[]):
        a,b = edge['from'],edge['to']
        if edge.get('accepted') is True and a in actual and b in actual:
            accepted.add((a,b));forward[a].add(b);reverse[b].add(a)
    seen, order = set(), []
    for identity in sorted(actual):
        if identity in seen:continue
        seen.add(identity);stack=[(identity,iter(sorted(forward[identity])))]
        while stack:
            current,children=stack[-1];child=next(children,None)
            if child is None:order.append(current);stack.pop()
            elif child not in seen:seen.add(child);stack.append((child,iter(sorted(forward[child]))))
    seen, groups = set(), []
    for identity in reversed(order):
        if identity in seen:continue
        group=set();stack=[identity];seen.add(identity)
        while stack:
            current=stack.pop();group.add(current)
            for previous in reverse[current]-seen:seen.add(previous);stack.append(previous)
        groups.append(group)
    return actual,forward,reverse,accepted,groups


def terminal_action_components(native):
    """Find reached directed cycles trapped after an actually proved ascent.

    Degree-zero checks miss a ledge with two points walking back and forth.
    Only current accepted, spawn-reachable cycles with an exported jump/climb
    entering from outside qualify. Return targets already reach that ingress
    on accepted directed edges; a lower isolated island is never a repair exit.
    """
    actual,forward,reverse,accepted,groups=_accepted_native_components(native)
    result={}
    for group in groups:
        if (len(group)<=1 or any(actual[i].get('spawn_reachable') is not True for i in group) or
            any(forward[i]-group for i in group)):continue
        routes=[r for r in native.get('traversal_routes',[]) if r.get('exported') is True and
                r.get('kind') in ('jump','free_climb') and r.get('landing') in group and
                r.get('launch') in actual and r.get('launch') not in group and
                actual[r['launch']].get('spawn_reachable') is True and
                (r['launch'],r['landing']) in accepted]
        if not routes:continue
        launches={r['launch'] for r in routes};targets=set(launches);stack=list(launches)
        while stack:
            current=stack.pop()
            for previous in reverse[current]-targets:
                targets.add(previous);stack.append(previous)
        info=dict(members=sorted(group),proved_landings=sorted({r['landing'] for r in routes}),
                  incoming_launches=sorted(launches),return_targets=targets-group)
        result.update({i:info for i in group})
    return result


def terminal_spawn_components(doc,native):
    """Find genuine native spawn cycles stranded outside one proved main SCC.

    A candidate/user spawn flag alone is insufficient. Both its authored
    machine origin and its native accepted floor reason must identify a map
    spawn. Only wholly ordinary terminal components qualify; unknown actions
    never become walking guides. The unique larger main SCC must itself have
    a separately native-validated map spawn. Such a spawn platform may be a
    directed entry: its physical descent still needs fresh exact action and
    floor proofs, but a return must never be fabricated to make it one SCC.
    """
    actual,forward,reverse,accepted,groups=_accepted_native_components(native)
    nodes={n['id']:n for n in doc.get('nodes',[])}
    if (len(nodes)!=len(doc.get('nodes',[])) or not groups or not set(actual)<=set(nodes)):
        return {}
    def seed(i):
        n,r=nodes[i],actual[i]
        return (n.get('spawn')is True and n.get('source_reason')=='map_entity_spawn_seed' and
                r.get('reason')=='map_entity_spawn_seed' and r.get('spawn_reachable')is True and
                r.get('native_input',{}).get('floor_accepted')is True and
                r.get('native_input',{}).get('floor_reason')=='map_entity_spawn_seed')
    def ordinary(i):
        n,r=nodes[i],actual[i]
        return (n.get('nav_flags')==0 and r.get('nav_flags')==0 and n.get('crouch')is False and
                r.get('crouch')is False and r.get('spawn_reachable')is True and
                r.get('native_input',{}).get('floor_accepted')is True and
                not n.get('editor_role',0) and not r.get('editor_role',0) and
                r.get('editor_drop_target',-1)==-1 and not r.get('door_recipe') and
                not any(n.get(k)for k in('generated_guide','action_roles','action_properties','door_recipe')))
    ranked=sorted(groups,key=lambda g:-len(g));main=ranked[0]
    if (len(ranked)<2 or len(main)<=len(ranked[1]) or not any(seed(i)for i in main)):
        return {}
    main_targets={i for i in main if ordinary(i)}
    if not main_targets:return {}
    links={(e['from'],e['to']):e for e in native.get('edges',[])}
    if len(links)!=len(native.get('edges',[])):return {}
    result={};walk_reasons={'accepted','native_validated_walk','external_native_validated_walk',
                           'manual_native_transition_live_pending'}
    action_ids={r[key]for r in native.get('traversal_routes',[])for key in
                ('low','high','approach','launch','landing','entry','exit','paired_jump_launch',
                 'release','alignment','ascent_release','ascent_alignment')
                if type(r.get(key))is int and r[key]>=0}
    for group in ranked[1:]:
        seeds=sorted(i for i in group if seed(i))
        if (len(group)<=1 or not seeds or group&action_ids or any(not ordinary(i)or forward[i]-group for i in group) or
                any(e.get('accepted')is not True or e.get('flags',0) or e.get('crouch') or e.get('reason')not in walk_reasons
                    for (a,b),e in links.items()if a in group and b in group)):
            continue
        info=dict(kind='native_spawn_terminal_component',members=sorted(group),proved_landings=[],
                  spawn_source_ids=seeds,main_component_source_ids=sorted(main),return_targets=main_targets,
                  source_model='literal_doc_and_native_spawn_terminal_v1',
                  requires_proved_return=False)
        result.update({i:info for i in group})
    return result


def propose_anchor_relocations(doc,native,probe,binding,max_probes=96):
    """Propose one bounded machine-floor correction, retaining every edge/ID.

    Only an explicit Repair caller may use this. Source binding precedes all
    native queries; the resulting draft still requires a complete import.
    A probe's normal-import floor may suggest the final Z, but a second exact
    roundtrip and fresh directional walks must prove that literal final pose.
    """
    from pathrepair import validate_source
    origin=native['bbox']['origin_above_feet']
    validate_source(doc,native,origin,binding)
    result=copy.deepcopy(doc)
    nodes={n['id']:n for n in doc['nodes']};rows={n['id']:n for n in native['nodes']}
    pairs={(e['from'],e['to']):e for e in native['edges']}
    actions={r[key] for r in native.get('traversal_routes',[]) for key in
             ('low','high','approach','launch','landing','entry','exit','paired_jump_launch',
              'release','alignment','ascent_release','ascent_alignment')
             if type(r.get(key)) is int and r[key]>=0}
    protected={tuple(pair) for pair in doc.get('base',{}).get('protected_action_edges',[])}
    machine_reasons={'external_native_validated_floor','native_validated_floor','spatial_surface_sample',
                     'generated_doorway_detour','automatic_repair_corridor'}
    queries=[];cache={};moves=[];max_probes=min(96,max(0,int(max_probes)))

    def ordinary(identity):
        node=nodes[identity];row=rows[identity]
        return (identity not in actions and node.get('nav_flags') in (0,64) and
                bool(node.get('crouch'))==bool(node['nav_flags']&64) and
                row.get('crouch')==bool(node['nav_flags']&64) and not row.get('editor_role',0) and
                row.get('editor_drop_target',-1)==-1 and not node.get('editor_role',0) and
                not node.get('generated_guide') and not row.get('door_recipe') and
                not any(node.get(k) for k in ('action_roles','action_properties','door_recipe')) and
                row.get('native_input',{}).get('floor_accepted') is True)

    def reached(identity):
        return rows[identity].get('accepted') is True and rows[identity].get('spawn_reachable') is True

    def check(a,b):
        key=tuple(round(float(v),5) for v in list(a)+list(b))
        if key in cache:return cache[key]
        if len(queries)>=max_probes:return None
        value=probe(list(key[:3]),list(key[3:]))
        if not isinstance(value,dict) or not re.fullmatch('[a-f0-9]{64}',value.get('reply_sha256','')):
            raise RuntimeError('Ankerkorrektur benötigt einen eindeutigen nativen Probenbeleg.')
        cache[key]=value;queries.append(dict(start=list(key[:3]),end=list(key[3:]),proof=value))
        return value

    def imported(value,endpoint,crouch):
        if not value:return None
        item=value.get('imports',{}).get('end' if endpoint else 'start',{}).get('crouch' if crouch else 'stand',{})
        point=item.get('feet')
        if (item.get('accepted') is not True or type(point) is not list or len(point)!=3 or
                any(type(v) not in (int,float) or not math.isfinite(v) or abs(v)>131072 for v in point)):
            return None
        return point

    for identity,node in sorted(nodes.items()):
        row=rows[identity];floor=row.get('native_input',{})
        if (not ordinary(identity) or node.get('locked') is not False or node.get('spawn') or
                node.get('name') or any(node.get('facing',[0,0,0])) or node.get('properties') or
                row.get('custom_properties') or node.get('source_reason') not in machine_reasons or
                row.get('accepted') is not False or row.get('spawn_reachable') is not False or
                row.get('reason')!='quarantined_no_validated_route_from_spawn' or
                floor.get('status')!='missing_spawn_route'):continue
        incident={pair:edge for pair,edge in pairs.items() if identity in pair}
        if (not incident or any(pair in protected or not all(ordinary(i) for i in pair) for pair in incident)):
            continue
        inbound=sorted((a for a,b in incident if b==identity and reached(a)),
                       key=lambda i:(math.dist(nodes[i]['position'],node['position']),i))
        outbound=sorted(b for a,b in incident if a==identity and reached(b))
        if not inbound or not outbound or not any(incident[a,identity].get('accepted') is False and
                incident[a,identity].get('reason') in ORDINARY_COLLISION_REJECTIONS for a in inbound):continue
        required=sorted(pair for pair,edge in incident.items() if edge.get('accepted') is True or
                        edge.get('reason')=='quarantined_unreachable_endpoint')
        # Quarantine changes only initially accepted links into this reason.
        # Unknown/hazard/budget failures must not become relocation requests.
        if any(edge.get('accepted') is not True and edge.get('reason') not in
               ORDINARY_COLLISION_REJECTIONS|{'quarantined_unreachable_endpoint'} for edge in incident.values()):continue
        radius=node.get('radius',0)
        if type(radius) not in (int,float) or not math.isfinite(radius) or radius<=0:continue
        bound=min(8.,radius*.25);old=node['position'];tested=set()

        def bounded(point):
            return math.dist(old,point)<=bound+.00001

        def walk(pair,point):
            a,b=pair;start=point if a==identity else nodes[a]['position'];end=point if b==identity else nodes[b]['position']
            value=check(start,end);exact=imported(value,b==identity,bool(node['nav_flags']&64))
            endpoint='end' if b==identity else 'start';stance='crouch' if node['nav_flags']&64 else 'stand'
            if (exact is None or math.dist(exact,point)>.011 or
                    value['imports'][endpoint][stance].get('exact') is not True):return False
            stance='crouch' if nodes[a]['nav_flags']&64 and nodes[b]['nav_flags']&64 else 'stand'
            return value.get(stance)=='accepted'

        offsets=[(0,4),(4,0),(0,-4),(-4,0),(4,4),(-4,4),(4,-4),(-4,-4),
                 (0,8),(8,0),(0,-8),(-8,0)]
        direction=[nodes[inbound[0]]['position'][k]-old[k] for k in (0,1)]
        offsets.sort(key=lambda xy:(math.hypot(*xy),-(xy[0]*direction[0]+xy[1]*direction[1])))
        chosen=None
        for dx,dy in offsets:
            for dz in (0,-4,4,-8,8):
                seed=[round(old[0]+dx,5),round(old[1]+dy,5),round(old[2]+dz,5)]
                if not bounded(seed):continue
                value=check(nodes[inbound[0]]['position'],seed)
                if value is None:break
                point=imported(value,True,bool(node['nav_flags']&64))
                if point is None:continue
                point=[round(v,5) for v in point]
                if not bounded(point) or math.dist(point,old)<=.011 or tuple(point) in tested:continue
                tested.add(tuple(point))
                if not all(walk(pair,point) for pair in required):continue
                restored=next((a for a in inbound if walk((a,identity),point)),None)
                exit_id=next((b for b in outbound if walk((identity,b),point)),None) if restored is not None else None
                if restored is not None and exit_id is not None:
                    chosen=(point,restored,exit_id);break
            if chosen or len(queries)>=max_probes:break
        if chosen:
            point,entry,exit_id=chosen
            next(n for n in result['nodes'] if n['id']==identity)['position']=point
            moves.append(dict(source_id=identity,way_guid=node.get('way_guid',0),old_position=list(old),new_position=point,
                              distance=math.dist(old,point),xy_distance=math.dist(old[:2],point[:2]),z_distance=abs(old[2]-point[2]),limit=bound,
                              retained_accepted_incident_edges=[list(p) for p in required],
                              restored_inbound=[entry,identity],retained_outbound=[identity,exit_id]))
            break  # Rebind a complete new native report before another move.
        if len(queries)>=max_probes:break
    return result,dict(model='native_bounded_ordinary_anchor_relocation_v1',moves=moves,queries=queries,
                       max_probes=max_probes,added_nodes=0,deleted_nodes=0,added_directed_edges=0,
                       replaced_rejected_edges=[],source_native_report=hashlib.sha256(binding['native_report_bytes']).hexdigest(),
                       changes_only_position=True,native_revalidation_required=True,installed=False)


def propose_descents(doc, native, probe, max_routes=4, max_probes=96):
    """Replace failed directed descents by native-probed bends and returns.

    Exact feet coordinates are never projected. Ordinary user IDs, positions
    and flags survive. Optional ascent is added only with its distinct native
    free-climb launch/target recipe; there is no reverse-drop or wall waiver.
    """
    result = copy.deepcopy(doc)
    nodes = {n['id']:n for n in result['nodes']}
    actual = {n['id']:n for n in native.get('nodes',[])}
    existing = {(e['from'],e['to']) for e in result['edges']}
    original_ids = set(nodes)
    failed = [e for e in native.get('edges',[]) if not e.get('accepted') and
              e.get('reason') in {'blocked_clearance','blocked_sweep','blocked_step_up','blocked_step_down',
                                  'different_floor','gap_or_drop','step_or_drop','manual_drop_arc_rejected',
                                  'capsule_support_gap_or_drop','capsule_support_step_or_drop'} and
              (e['from'],e['to']) in existing]
    cache, queries, routes, replacements = {}, [], [], []
    blocked_drop_rays=set()
    probe_limit=max_probes;active_allocation=None;allocations=[]
    next_id = result['next_id']
    gravity = native.get('traversal_physics',{}).get('gravity')
    scale = .64 if native.get('bbox',{}).get('origin_above_feet') == 30 else 1
    safe_drop = 38/(2*gravity*scale*.0002) if type(gravity) in (int,float) and 100<=gravity<=2000 else 0
    accepted_outgoing = {e['from'] for e in native.get('edges',[]) if e.get('accepted')}
    accepted_incoming = {e['to'] for e in native.get('edges',[]) if e.get('accepted')}
    no_exit = set(native.get('native_input_coverage',{}).get('ordinary_without_exit',[]))
    accepted_pairs = {(e['from'],e['to']) for e in native.get('edges',[]) if e.get('accepted')}
    proved_landings = {r.get('landing') for r in native.get('traversal_routes',[]) if r.get('exported') is True and
                       r.get('kind') in ('free_climb','jump') and (r.get('launch'),r.get('landing')) in accepted_pairs}
    terminal_components=terminal_action_components(native)
    spawn_components=terminal_spawn_components(doc,native)
    terminal_components.update(spawn_components)
    terminal_landings={i for info in terminal_components.values()for i in
                       (info['members']if info.get('kind')=='native_spawn_terminal_component'else info['proved_landings'])}
    # Generated spawn areas and proved ordinary jump/climb landing sinks need
    # an exit. An arbitrary user point/closed action/unproved floor remains
    # untouched. Every proposed descent still needs exact native body proof.
    for identity in sorted(nodes,key=lambda i:(i not in terminal_landings,i)):
        node=nodes[identity]
        landing_sink = identity in no_exit and identity in proved_landings and identity in accepted_incoming
        terminal_sink=terminal_components.get(identity)
        generated_spawn = node.get('source_reason')=='map_entity_spawn_seed' and node.get('spawn') is True
        if ((identity in accepted_outgoing and not terminal_sink) or not (generated_spawn or landing_sink or terminal_sink) or
            node.get('nav_flags',0) or node.get('crouch') or
            node.get('action_roles') or node.get('editor_role',0)&~1 or
            not actual.get(identity,{}).get('accepted')):continue
        choices=[]
        for target_id, target in nodes.items():
            if (target_id==identity or target_id not in accepted_outgoing or target.get('nav_flags',0) or
                target.get('crouch') or target.get('action_roles') or
                not actual.get(target_id,{}).get('accepted')):continue
            if terminal_sink and (target_id not in terminal_sink['return_targets'] or
                                  actual[target_id].get('spawn_reachable') is not True):continue
            descent=node['position'][2]-target['position'][2]
            distance=math.dist(node['position'][:2],target['position'][:2])
            if 18.25<descent<=safe_drop and .25<distance<=256:
                choices.append((distance,target_id))
        for distance,target_id in sorted(choices)[:8]:
            if (identity,target_id) not in existing:
                failed.append({'from':identity,'to':target_id,
                               'reason':'native_spawn_terminal_component' if terminal_sink and terminal_sink.get('kind')=='native_spawn_terminal_component'else
                                        'proved_traversal_terminal_component' if terminal_sink else
                                        'proved_traversal_landing_without_exit' if landing_sink else 'isolated_generated_spawn_seed',
                               'synthetic_request':True})

    def request_endpoints(edge):
        a,b=edge['from'],edge['to'];start,target=nodes.get(a),nodes.get(b)
        if not start or not target or any(n.get('nav_flags',0) or n.get('crouch') for n in(start,target)):return None
        if start.get('editor_role',0)&~(1|8) or target.get('action_roles') or target.get('editor_role',0)&~1:return None
        if any(r.get('kind')!='walk_drop' or r.get('role')!='launch' or r.get('landing_node')!=b
               for r in start.get('action_roles',[])):return None
        if not 18.25<start['position'][2]-target['position'][2]<=safe_drop:return None
        return start,target

    source_order=list(dict.fromkeys(e['from']for e in failed if request_endpoints(e)))[:max_routes]
    first_requests={a:next(e for e in failed if e['from']==a and request_endpoints(e))for a in source_order}
    source_limits={};refined_sources=set();initial_requests=len(failed)
    landing_launches=[]
    processed_sources, resolved_sources = set(), set()
    resolved_terminal_components=set()
    quarantined_floor_probes=set()
    direct_used = defaultdict(int)
    direct_ceiling = min(32,max(0,max_probes-24))

    def checked_quarantined_floor_pair(edge,a,b):
        # A completed floor check may survive spawn-route quarantine. It is
        # only a reason to ask for a new physical descent proof, never an
        # accepted endpoint or permission to invent an isolated connection.
        capacity=native.get('native_input_coverage',{}).get('capacity',{})
        if (edge.get('synthetic_request') or edge.get('reason') not in
                ('capsule_support_gap_or_drop','capsule_support_step_or_drop') or
                capacity.get('input_validation_complete') is not True or
                capacity.get('input_truncated') is not False):return None
        action_ids={r[key] for r in native.get('traversal_routes',[]) for key in
                    ('low','high','approach','launch','landing','entry','exit','paired_jump_launch',
                     'release','alignment','ascent_release','ascent_alignment')
                    if type(r.get(key)) is int and r[key]>=0}
        quarantined=[]
        for identity in (a,b):
            node=nodes[identity];row=actual.get(identity,{})
            floor=row.get('native_input',{})
            if (identity in action_ids or node.get('nav_flags',0)!=0 or node.get('crouch') or
                    node.get('editor_role',0) not in (0,1) or
                    any(node.get(k) for k in ('action_roles','action_properties','door_recipe')) or
                    row.get('nav_flags')!=0 or row.get('crouch') is not False or
                    row.get('editor_role',0) not in (0,1) or row.get('editor_drop_target',-1)!=-1 or
                    row.get('door_recipe') or floor.get('floor_accepted') is not True):return None
            if row.get('accepted') is True:continue
            if (row.get('accepted') is not False or row.get('spawn_reachable') is not False or
                    row.get('reason')!='quarantined_no_validated_route_from_spawn' or
                    floor.get('status')!='missing_spawn_route'):return None
            quarantined.append(identity)
        return quarantined or None

    def check(a,b):
        key = tuple(round(float(x),5) for x in list(a)+list(b))
        if key in cache:return cache[key]
        if len(queries)>=probe_limit:return None
        value = probe(list(key[:3]),list(key[3:]))
        if not isinstance(value,dict):raise RuntimeError('Ungültige Antwort der nativen Reparaturprobe.')
        cache[key] = value; queries.append(dict(start=list(key[:3]),end=list(key[3:]),proof=value))
        if active_allocation is not None:active_allocation['queries']+=1
        return value

    def ordinary(a,b):
        value=check(a,b)
        return bool(value and value.get('stand')=='accepted')

    protected_walk_edges={tuple(e)for e in doc.get('base',{}).get('protected_action_edges',[])}
    def connector_floor(identity):
        node,row=nodes[identity],actual.get(identity,{})
        return (identity in original_ids and row.get('accepted') is True and row.get('spawn_reachable') is True and
                row.get('native_input',{}).get('floor_accepted') is True and
                node.get('nav_flags',0)==0 and row.get('nav_flags')==0 and
                node.get('crouch') is False and row.get('crouch') is False and
                not node.get('generated_guide') and not node.get('editor_role',0) and not row.get('editor_role',0) and
                not row.get('door_recipe') and row.get('editor_drop_target',-1)==-1 and
                not any(node.get(key)for key in('action_roles','action_properties','door_recipe')))
    connector_edges=defaultdict(list)
    for row in native.get('edges',[]):
        source,destination=row['from'],row['to']
        if (source in nodes and destination in nodes and (source,destination) in existing and
                row.get('accepted') is True and not row.get('flags',0) and not row.get('crouch') and
                (source,destination) not in protected_walk_edges and row.get('reason') in
                ('accepted','external_native_validated_walk','native_validated_walk','manual_native_transition_live_pending')):
            connector_edges[source].append((destination,row))
    def connector_path(source,destination):
        if not connector_floor(source) or not connector_floor(destination):return None
        queue=[(0.,0,source)];distances={(source,0):0.};parents={}
        while queue:
            length,hops,identity=heapq.heappop(queue);state=(identity,hops)
            if distances[state]!=length:continue
            if identity==destination:
                ids=[identity];edges=[]
                while state in parents:
                    state,row=parents[state];ids.append(state[0]);edges.append(copy.deepcopy(row))
                return dict(source_ids=ids[::-1],native_edges=edges[::-1],length_units=length,hops=hops,
                            maximum_length_units=512,maximum_hops=8)
            if hops==8:continue
            for target,row in connector_edges[identity]:
                if not connector_floor(target):continue
                proposal=length+math.dist(nodes[identity]['position'],nodes[target]['position']);next_state=(target,hops+1)
                if proposal<=512 and proposal<distances.get(next_state,math.inf):
                    distances[next_state]=proposal;parents[next_state]=(state,row)
                    heapq.heappush(queue,(proposal,hops+1,target))
        return None

    def exact_import(row,feet):
        pos=row.get('feet')
        return (row.get('accepted') is True and row.get('exact') is True and
                isinstance(pos,list) and len(pos)==3 and
                all(type(v) in (int,float) and math.isfinite(v) for v in pos) and math.dist(pos,feet)<=.011)

    def imported_new_floor(row,point):
        pos=row.get('feet')
        if (row.get('accepted') is not True or not isinstance(pos,list) or len(pos)!=3 or
                any(type(v) not in (int,float) or not math.isfinite(v) or abs(v)>131072 for v in pos) or
                math.dist(pos[:2],point[:2])>.011):return None
        return [point[0],point[1],round(float(pos[2]),5)]

    # Only newly proposed, physically grounded DROP guides can consume the
    # typed action import/walk adapter. An existing ordinary anchor keeps its
    # literal pose and ordinary import contract even if a probe suggests a
    # nearby action pose for it.
    typed_drop_poses=set()
    def pose_key(point):return tuple(round(float(v),5) for v in point)
    def exact_drop_import(value,endpoint,point):
        row=value.get('action_imports',{}).get(endpoint,{}).get('drop_launch',{})
        return row.get('reason')=='accepted' and exact_import(row,point)
    def typed_walk(value,endpoint,point):
        row=value.get('action_walks',{}).get(endpoint,{}).get('drop_launch',{})
        return (exact_drop_import(value,endpoint,point) and row.get('accepted') is True and
                row.get('reason')=='accepted')

    def staging_candidate(start,point,literal_typed=False):
        value=check(start,point)
        if not value:return None
        imports=value.get('imports');changed=False;action_seed=False
        if imports is not None:
            row=imports.get('end',{}).get('stand',{})
            if not exact_import(row,point):
                suggested=imported_new_floor(row,point)
                if suggested is None:
                    # An actual ascent touchdown can be at PMove's contact
                    # skin, outside the ordinary floor-import seed interval.
                    # Only a NEW guide may try the Role9 import's same-XY
                    # nearby recommendation. It is a fresh candidate, not a
                    # waiver: literal ordinary AND typed imports, and both
                    # ordinary approach sweeps, must subsequently be exact.
                    if row.get('reason')!='no_floor_near_seed':return None
                    action=value.get('action_imports',{}).get('end',{}).get('drop_launch',{})
                    suggested=imported_new_floor(action,point)
                    if (action.get('reason')!='accepted' or suggested is None or
                            not .011<math.dist(suggested,point)<=2):return None
                    action_seed=True
                if suggested==point:return None
                point=suggested;changed=True;value=check(start,point)
                if not value or not exact_import(value.get('imports',{}).get('end',{}).get('stand',{}),point):return None
                if action_seed and not exact_drop_import(value,'end',point):return None
        # A gentle slope can support the player's full capsule slightly below
        # the ordinary flat-footbox anchor. Only the native copied Role9
        # import may suggest this NEW guide, at the same XY and at most two
        # units down from the freshly exact ordinary floor. Its literal pose
        # and both standing body paths must then be probed again.
        physical=False
        action=value.get('action_imports',{}).get('end',{}).get('drop_launch',{})
        if imports is not None and action.get('accepted') is True and action.get('reason')=='accepted':
            suggested=imported_new_floor(action,point)
            if (suggested is not None and .011<point[2]-suggested[2]<=2 and
                    math.dist(suggested,point)<=2):
                point=suggested;changed=True;physical=True;value=check(start,point)
                if not value or not exact_drop_import(value,'end',point):return None
        if value.get('stand')!='accepted' and not (physical and typed_walk(value,'end',point)):return None
        returning=check(point,start)
        reverse=bool(returning and (returning.get('stand')=='accepted' or
                                   physical and typed_walk(returning,'start',point)))
        if literal_typed:
            if (value.get('stand')!='accepted' or not returning or returning.get('stand')!='accepted' or
                    not exact_import(value.get('imports',{}).get('end',{}).get('stand',{}),point) or
                    not exact_import(returning.get('imports',{}).get('start',{}).get('stand',{}),point) or
                    not exact_drop_import(value,'end',point) or not exact_drop_import(returning,'start',point)):
                return None
        if physical:
            if not reverse or not exact_drop_import(returning,'start',point):return None
            typed_drop_poses.add(pose_key(point))
        elif changed:
            if not reverse or not exact_import(returning.get('imports',{}).get('start',{}).get('stand',{}),point):return None
            if action_seed and not exact_drop_import(returning,'start',point):return None
        return point,reverse

    def descent_candidate(start,point):
        """Only a new landing may use a native import height suggestion.

        Neither existing endpoint nor XY moves. The suggested literal floor
        is queried again and must round-trip exactly before its drop proof
        can be used. Older probes without imports can only prove the original
        literal candidate; they cannot authorize a projection.
        """
        # NG_TrWalkDrop has a literal 8..160 XY endpoint domain. Height
        # imports cannot change XY, so out-of-domain requests cannot yield a
        # proof and must not consume another landing's finite query share.
        if not 8<=math.dist(start[:2],point[:2])<=160:return None
        dx,dy=point[0]-start[0],point[1]-start[1];length=math.hypot(dx,dy)
        ray=(pose_key(start),round(dx/length,10),round(dy/length,10),round(point[2],5))
        # The native drop normalizes direction once and keeps its sampled
        # input speeds. Changing only endpoint distance cannot clear a known
        # body collision on that same launch/ray/lower floor. This finite
        # search memo skips redundant failures; it never supplies a proof.
        if ray in blocked_drop_rays:return None
        value=check(start,point)
        if not value:return None
        failure=value.get('drop',{})
        impact=failure.get('touchdown')
        if (failure.get('accepted')is False and failure.get('reason')=='blocked_hull_or_wrong_floor' and
                isinstance(impact,list)and len(impact)==3 and
                all(type(v)in(int,float)and math.isfinite(v)for v in impact)and abs(impact[2]-point[2])>18.25):
            blocked_drop_rays.add(ray)
        if pose_key(start) in typed_drop_poses and not exact_drop_import(value,'start',start):return None
        imports=value.get('imports')
        if imports is not None:
            launch=imports.get('start',{}).get('stand',{})
            landing=imports.get('end',{}).get('stand',{})
            launch_imported=exact_import(launch,start) or (pose_key(start) in typed_drop_poses and
                                                         exact_drop_import(value,'start',start))
            if not launch_imported:return None
            if not exact_import(landing,point):
                suggested=imported_new_floor(landing,point)
                if suggested is None or suggested==point or not 18.25<start[2]-suggested[2]<=safe_drop:return None
                point=suggested;value=check(start,point)
                if not value:return None
                if pose_key(start) in typed_drop_poses and not exact_drop_import(value,'start',start):return None
                imports=value.get('imports',{})
                if (not (exact_import(imports.get('start',{}).get('stand',{}),start) or
                         pose_key(start) in typed_drop_poses and exact_drop_import(value,'start',start)) or
                        not exact_import(imports.get('end',{}).get('stand',{}),point)):return None
        drop=value.get('drop',{})
        return (point,value) if drop.get('accepted') and 0<=drop.get('delta',39)<=38 else None

    def create(position,radius,flags,reason,roles=None,props=None,editor_role=1):
        nonlocal next_id
        identity=next_id;next_id+=1
        node=dict(id=identity,position=[round(float(v),5) for v in position],radius=radius,source_radius=radius,
                  nav_flags=flags,crouch=False,spawn=False,topology_protected=True,locked=False,
                  generated_guide=True,source_reason=reason,editor_role=editor_role,
                  action_roles=roles or [],action_properties=props or {})
        nodes[identity]=node;result['nodes'].append(node)
        return identity

    alternate_return_attempted=set()
    def alternate_standing_return(old_high,drop_launch,connection):
        """A separate ordinary street launch can return to a NEW DROP guide.

        A proved downward landing can be too far from the upper guide or its
        braking continuation can meet a curb. Search one finite set of closer
        street poses, retaining the actual new upper pose and the ordinary
        source. This is independent of the existing two-jump return search.
        """
        identity=pose_key(drop_launch)
        if identity not in typed_drop_poses or identity in alternate_return_attempted:return None
        alternate_return_attempted.add(identity)
        direction=[drop_launch[k]-old_high[k] for k in (0,1)]
        magnitude=math.hypot(*direction)
        if magnitude<=.25 or not connector_floor(connection):return None
        direction=[v/magnitude for v in direction]
        for distance in (32,40,48,56,64):
            guess=[round(drop_launch[k]+direction[k]*distance,5) for k in (0,1)]+[nodes[connection]['position'][2]]
            if max(abs(v) for v in guess)>131072:continue
            value=check(drop_launch,guess)
            if not value:break
            row=value.get('imports',{}).get('end',{}).get('stand',{})
            street=imported_new_floor(row,guess)
            if street is None:continue
            if not exact_import(row,street):
                value=check(drop_launch,street)
                if not value or not exact_import(value.get('imports',{}).get('end',{}).get('stand',{}),street):continue
            if not exact_drop_import(value,'start',drop_launch):continue
            ascent=check(street,drop_launch)
            if not ascent:break
            proof=ascent.get('jump',{})
            if (not exact_import(ascent.get('imports',{}).get('start',{}).get('stand',{}),street) or
                    not exact_drop_import(ascent,'end',drop_launch) or
                    proof.get('standing',{}).get('accepted') is not True or
                    proof.get('standing_proof',{}).get('samples')!=27):continue
            nearby=sorted((math.dist(street,nodes[i]['position']),i) for i in original_ids
                          if connector_floor(i) and math.dist(street,nodes[i]['position'])<=160)
            checked=0
            for _,connector in nearby:
                outward=connector_path(connection,connector);inward=connector_path(connector,connection)
                if not outward or not inward:continue
                checked+=1
                target=nodes[connector]['position']
                forward=check(street,target);reverse=check(target,street)
                if (forward and reverse and forward.get('stand')=='accepted' and reverse.get('stand')=='accepted' and
                        exact_import(forward.get('imports',{}).get('start',{}).get('stand',{}),street) and
                        exact_import(reverse.get('imports',{}).get('end',{}).get('stand',{}),street)):
                    return dict(model='native_standing_jump_alternate_launch_v1',street=street,
                        drop_launch=drop_launch,connector=connector,from_descent_connector=outward,to_descent_connector=inward,
                        street_walks=[dict(start=street,end=target,proof=forward),dict(start=target,end=street,proof=reverse)],
                        action_probe=ascent,direction=direction,selected_distance=distance,search_probes=len(queries))
                if checked==4:break
        return None

    def staged_return(high,connection,limit):
        """One bounded independent two-jump return; every new pose is literal."""
        stop=min(probe_limit,len(queries)+limit)
        def query(a,b):
            key=tuple(round(float(x),5)for x in list(a)+list(b))
            if key not in cache and len(queries)>=stop:return None
            return check(a,b)
        def floor_at(guess):
            value=query(high,guess)
            if not value:return None
            row=value.get('imports',{}).get('end',{}).get('stand',{})
            point=imported_new_floor(row,guess)
            if point is None:return None
            if not exact_import(row,point):
                value=query(high,point)
                if not value or not exact_import(value.get('imports',{}).get('end',{}).get('stand',{}),point):return None
            return point
        def jump(a,b):
            value=query(a,b)
            if not value:return None,value
            imports=value.get('imports',{})
            proof=value.get('jump',{})
            valid=(exact_import(imports.get('start',{}).get('stand',{}),a) and
                   exact_import(imports.get('end',{}).get('stand',{}),b) and
                   proof.get('standing',{}).get('accepted') is True and
                   proof.get('standing_proof',{}).get('samples')==27)
            return value if valid else None,value
        def walks(a,b):
            forward=query(a,b)
            if not forward or forward.get('stand')!='accepted':return None
            reverse=query(b,a)
            if not reverse or reverse.get('stand')!='accepted':return None
            return [dict(start=a,end=b,proof=forward),dict(start=b,end=a,proof=reverse)]
        # Four cardinal surface samples. Previously seen exact import poses
        # are reused; no map name, absolute coordinate or unbounded grid.
        directions=[]
        for axis in(0,1):
            for sign in(-1,1):
                guess=list(high);guess[axis]+=sign*64
                point=floor_at(guess)
                if point is not None and high[2]-point[2]>18.25:
                    directions.append((high[2]-point[2],axis,sign,point))
        for _,axis,sign,first in sorted(directions):
            floors={64:first}
            for length in(80,96):
                guess=list(high);guess[axis]+=sign*length
                point=floor_at(guess)
                if point is not None and high[2]-point[2]>18.25:floors[length]=point
            candidates=[floors[length]for length in(64,80,96)if length in floors]
            # Refine only one interval whose endpoints prove the same real
            # intermediate floor. Midpoint first, then a finite two-unit grid.
            interval=next(((a,b)for a,b in((64,80),(80,96))if a in floors and b in floors and
                           abs(floors[a][2]-floors[b][2])<=1),None)
            refinements=[]
            if interval:
                a,b=interval;middle=(a+b)//2
                refinements=sorted(range(a+2,b,2),key=lambda v:(abs(v-middle),v))
            # Lazy refinement leaves the same finite budget available to a
            # successful coarse pose and its two complete action proofs.
            for index in range(len(candidates)+len(refinements)):
                if len(queries)>=stop:break
                if index<len(candidates):middle=candidates[index]
                else:
                    guess=list(high);guess[axis]+=sign*refinements[index-len(candidates)]
                    middle=floor_at(guess)
                    if middle is None or abs(middle[2]-floors[interval[0]][2])>1:continue
                if high[2]-middle[2]<=18.25:continue
                # The existing graph supplies the lower floor elevation.
                nearby=sorted((math.dist(middle[:2],nodes[i]['position'][:2]),i)for i in original_ids
                              if connector_floor(i) and middle[2]-nodes[i]['position'][2]>18.25 and
                              math.dist(middle[:2],nodes[i]['position'][:2])<=160)
                connections=[]
                for _,identity in nearby:
                    outward=connector_path(connection,identity);inward=connector_path(identity,connection)
                    if outward and inward:connections.append((identity,outward,inward))
                    if len(connections)==4:break
                for identity,outward,inward in connections:
                    z=nodes[identity]['position'][2];trials={};unstable=False
                    offsets=[48,40,56,64]
                    refined=False;cursor=0
                    while cursor<len(offsets) and len(queries)<stop:
                        distance=offsets[cursor];cursor+=1
                        guess=list(middle);guess[axis]+=sign*distance;guess[2]=z
                        value=query(middle,guess)
                        if not value:break
                        row=value.get('imports',{}).get('end',{}).get('stand',{})
                        street=imported_new_floor(row,guess)
                        if street is None:continue
                        if not exact_import(row,street):
                            value=query(middle,street)
                            if not value or not exact_import(value.get('imports',{}).get('end',{}).get('stand',{}),street):continue
                        first_jump,rejected=jump(street,middle)
                        reason=(rejected or {}).get('jump',{}).get('standing',{}).get('reason')
                        trials[distance]=reason
                        if reason=='unstable_landing':unstable=True;break
                        if first_jump:
                            street_walks=walks(street,nodes[identity]['position'])
                            if not street_walks:continue
                            # Separate UIDs prevent the landing controller
                            # from also consuming the next jump launch.
                            for side in(1,-1,0):
                                launch=list(middle);launch[1-axis]+=side*6
                                pair=walks(middle,launch)
                                if not pair:continue
                                if not exact_import(pair[0]['proof'].get('imports',{}).get('end',{}).get('stand',{}),launch):continue
                                second_jump,_=jump(launch,high)
                                if second_jump:
                                    return dict(street=street,middle=middle,launch=launch,connector=identity,
                                        from_descent_connector=outward,to_descent_connector=inward,
                                        street_walks=street_walks,middle_walks=pair,
                                        first_jump=first_jump,second_jump=second_jump,search_probes=len(queries),
                                        model='native_two_standing_jumps_separate_launch_v1')
                        if cursor==4 and not refined:
                            # Only one bounded interval between two queried,
                            # geometrically imported launch positions.
                            interval2=next(((a,b)for a,b in((48,56),(40,48),(56,64))if a in trials and b in trials),None)
                            if interval2:
                                offsets+=list(range(interval2[0]+2,interval2[1],2));refined=True
                    if unstable:break
        return None

    for request_index,edge in enumerate(failed):
        # First give every eligible source its fair share. A difficult roof
        # must not consume all native calls before another landing is tried.
        # Revisit unresolved sources once with any budget left by easy ones;
        # cached proofs do not spend new calls in that bounded second pass.
        if request_index==initial_requests-1:
            failed.extend(first_requests[i]for i in source_order if i not in resolved_sources)
        if len(queries)>=max_probes:break
        a,b=edge['from'],edge['to'];start,target=nodes.get(a),nodes.get(b)
        if a in resolved_sources:continue
        terminal=terminal_components.get(a)
        terminal_members=tuple(terminal['members']) if terminal else None
        spawn_terminal=bool(terminal and terminal.get('kind')=='native_spawn_terminal_component')
        if terminal_members in resolved_terminal_components:continue
        if a not in source_order or not request_endpoints(edge):continue
        quarantined=[]
        if any(not actual.get(i,{}).get('accepted') for i in (a,b)):
            quarantined=checked_quarantined_floor_pair(edge,a,b)
            if quarantined is None:continue
        p,q=start['position'],target['position']
        refinement=request_index>=initial_requests
        if a not in source_limits or refinement and a not in refined_sources:
            remaining=[i for i in source_order if i not in resolved_sources and
                       (i not in refined_sources if refinement else i not in source_limits)]
            allowance=math.ceil((max_probes-len(queries))/max(1,len(remaining)))
            source_limits[a]=len(queries)+allowance
            active_allocation=dict(source_id=a,phase='refinement'if refinement else'fair_first_pass',
                                   start_query=len(queries),allowance=allowance,queries=0)
            allocations.append(active_allocation)
            if refinement:refined_sources.add(a)
        else:
            active_allocation=next(v for v in reversed(allocations)if v['source_id']==a)
        probe_limit=source_limits[a]
        if len(queries)>=probe_limit:continue
        processed_sources.add(a)
        quarantined_floor_probes.update(quarantined)
        needs_return=(bool(terminal and terminal.get('requires_proved_return')) or
                      a in quarantined and a not in accepted_incoming and
                      not actual[a].get('native_input',{}).get('initial_incoming',0))
        delta=[q[k]-p[k] for k in (0,1)]
        target_ids=[b]
        if edge.get('synthetic_request'):
            target_ids=list(dict.fromkeys(e['to'] for e in failed if e['from']==a and
                                          e.get('synthetic_request') and e['to'] in nodes))

        deferred=[];staged_attempted=False
        def finish_candidate(point,value,drop_start,upper=None):
            if spawn_terminal:
                # A genuine map spawn may be a directed entry into the main
                # play area. Its exemption from a fabricated return never
                # exempts the new directed action/floor from literal imports.
                if (not exact_drop_import(value,'start',drop_start) or
                        not exact_import(value.get('imports',{}).get('end',{}).get('stand',{}),point)):
                    return None
            eligible=list(target_ids)
            if edge.get('synthetic_request'):
                eligible.sort(key=lambda i:math.dist(point,nodes[i]['position']))
            connection=next((i for i in eligible if ordinary(point,nodes[i]['position'])),None)
            connector_proof=None
            if connection is None and not edge.get('synthetic_request'):
                # The failed literal edge fixes the destination, not the
                # final straight segment. A nearby existing road point may
                # reach that same target through two proved ordinary paths.
                nearby=sorted((math.dist(point,n['position']),i)for i,n in nodes.items()
                              if i!=b and connector_floor(i) and math.dist(point,n['position'])<=128)
                checked=0
                for _,identity in nearby:
                    to_target=connector_path(identity,b);from_target=connector_path(b,identity)
                    if not to_target or not from_target:continue
                    checked+=1
                    if ordinary(point,nodes[identity]['position']) and ordinary(nodes[identity]['position'],point):
                        connection=identity;connector_proof=dict(connector=identity,original_target=b,
                            to_original_target=to_target,from_original_target=from_target)
                        break
                    if checked==4:break
            if connection is None:return None
            destination=nodes[connection]['position']
            reverse=ordinary(destination,point)
            ascent=check(point,drop_start) if reverse else None
            if (ascent and pose_key(drop_start) in typed_drop_poses and
                    not exact_drop_import(ascent,'end',drop_start)):ascent=None
            climb=ascent.get('free_climb',{}) if ascent else {}
            jump=ascent.get('jump',{}) if ascent else {}
            launch=climb.get('launch')
            returning=False
            if (climb.get('accepted') and isinstance(launch,list) and len(launch)==3 and
                all(type(x) in (int,float) and math.isfinite(x) for x in launch) and
                0<climb.get('duration',0)<=2 and 0<climb.get('stamina_used',0)<=2500 and
                math.dist(launch,point)>2 and math.dist(launch,drop_start)>2):
                returning=ordinary(point,launch) and ordinary(launch,point)
            standing=(jump if reverse and not returning and jump.get('standing',{}).get('accepted') is True and
                      jump.get('standing_proof',{}).get('samples')==27 else None)
            if needs_return and (not reverse or not (returning or standing) or (upper and not upper[1])):
                if reverse and upper and upper[1] and not returning and standing is None:
                    alternate=alternate_standing_return(p,drop_start,connection)
                    if alternate:return point,value,reverse,None,None,ascent,upper,connection,connector_proof,alternate
                if reverse:
                    deferred.append((point,value,reverse,None,None,ascent,upper,connection,connector_proof))
                return None
            return point,value,reverse,climb if returning else None,standing,ascent,upper,connection,connector_proof,None

        # Actual connected lower anchors are better first requests than
        # arbitrary axial distances. Their imports/drop still need fresh
        # literal proofs, and none of these native inputs is changed.
        candidates=([list(nodes[i]['position'])for i in target_ids]
                    if edge.get('synthetic_request')else[])
        for axis in sorted((0,1),key=lambda k:-abs(delta[k])):
            if abs(delta[axis])<.25:continue
            signs=(1 if delta[axis]>0 else -1, -1 if delta[axis]>0 else 1)
            for sign in signs:
                distances=[min(160,max(32,abs(delta[axis]))),128,144,154,160]
                for length in distances:
                    point=[p[0],p[1],q[2]];point[axis]+=sign*length
                    point=[round(v,5) for v in point]
                    if max(abs(v) for v in point)>131072 or point in candidates:continue
                    candidates.append(point)
        chosen=None
        for point in candidates:
            # Keep native queries available for a real upper walking approach
            # when a direct departure hits a cab/tank wall or leaves its fall
            # corridor. Do not consume every query trying the same launch.
            if len(queries)>=probe_limit or direct_used[a]>=12 or sum(direct_used.values())>=direct_ceiling:break
            before=len(queries)
            descent=descent_candidate(p,point)
            if descent:chosen=finish_candidate(*descent,p)
            direct_used[a]+=len(queries)-before
            if chosen:break
        if not chosen and deferred and needs_return:
            staged_attempted=True;best=deferred[0]
            staged=staged_return(p,best[7],min(56,probe_limit-len(queries)))
            if staged:chosen=best+(staged,)
        if not chosen:
            # First walk on the real upper support, then continue off its
            # edge. Every segment is independently native-checked; the
            # original point stays ordinary and a new precise guide owns the
            # drop recipe. Try the side axis before the blocked direct axis.
            stages=[];inset_stage_keys=set()
            magnitude=math.hypot(*delta)
            # An exported ascent records a physical touchdown in player-
            # origin coordinates. It can seed a NEW upper approach near the
            # real lip. This is never inherited DROP clearance: ordinary/
            # typed imports and both approach directions are checked anew.
            for ascent_route in native.get('traversal_routes',[]):
                if (not edge.get('synthetic_request') or ascent_route.get('exported')is not True or
                    ascent_route.get('kind')not in('free_climb','jump') or ascent_route.get('landing')!=a or
                    (ascent_route.get('launch'),a)not in accepted_pairs):continue
                touchdown=ascent_route.get('touchdown');origin=native.get('bbox',{}).get('origin_above_feet')
                if (not isinstance(touchdown,list)or len(touchdown)!=3 or
                    any(type(v)not in(int,float)or not math.isfinite(v)for v in touchdown)or
                    type(origin)not in(int,float)or not math.isfinite(origin)):continue
                top=[round(touchdown[k]-(origin if k==2 else 0),5)for k in range(3)]
                if (max(abs(v)for v in top)>131072 or math.dist(top,p)<=.011 or
                    math.dist(top[:2],p[:2])>256 or abs(top[2]-p[2])>18.25):continue
                for identity in sorted(target_ids,key=lambda i:math.dist(top,nodes[i]['position']))[:3]:
                    direction=[nodes[identity]['position'][k]-top[k]for k in(0,1)];length=math.hypot(*direction)
                    if length>.25:stages.append(([v/length for v in direction],top))
                if not any(v['source_id']==a and v['position']==top for v in landing_launches):
                    landing_launches.append(dict(source_id=a,position=top,ascent_launch=ascent_route['launch'],
                                                model='exported_ascent_touchdown_new_launch_candidate'))
                # The ascent's rounded capsule may touch the lip while its
                # centre has no ordinary floor ray. Try NEW approaches one
                # and two actual hull radii inward, at the unchanged ordinary
                # anchor's floor height. These are candidates only: exact
                # ordinary imports and both full-body walks still apply.
                radius=native.get('bbox',{}).get('half_width')
                inward=[p[k]-top[k]for k in(0,1)];distance=math.hypot(*inward)
                if type(radius)in(int,float)and math.isfinite(radius)and 1<=radius<=64 and distance>radius:
                    outward=[-v/distance for v in inward]
                    side=[-outward[1],outward[0]]
                    def fan(direction):
                        tangent=[-direction[1],direction[0]]
                        root=math.sqrt(.5)
                        return [direction,[root*(direction[k]+tangent[k])for k in(0,1)],
                                [root*(direction[k]-tangent[k])for k in(0,1)]]
                    for amount in(radius,2*radius):
                        if amount>=distance:continue
                        candidate=[round(top[k]+inward[k]/distance*amount,5)for k in(0,1)]+[p[2]]
                        # Put a bounded set near the real lip ahead of the
                        # exhaustive old axial guesses. A failed launch's
                        # landing fan must leave room to try another launch.
                        stages.extend((direction,candidate)for direction in fan(outward))
                        inset_stage_keys.add(pose_key(candidate))
                    # The climbed lip need not be the safe departure. Walk
                    # toward another roof edge with independent literal body
                    # proofs. Prefer the two lateral edges before more points
                    # along the already blocked direct ray; no map coordinates
                    # or assumed roof extent enter this finite search.
                    root=math.sqrt(.5)
                    directions=[side,[-v for v in side],outward,[-v for v in outward]]
                    directions.extend([root*(outward[k]+sign*side[k])for k in(0,1)]for sign in(1,-1))
                    directions.extend([root*(-outward[k]+sign*side[k])for k in(0,1)]for sign in(1,-1))
                    for amount in(4*radius,2*radius,6*radius):
                        for direction in directions:
                            candidate=[round(p[k]+direction[k]*amount,5)for k in(0,1)]+[p[2]]
                            if max(abs(v)for v in candidate)>131072:continue
                            stages.extend((ray,candidate)for ray in fan(direction))
                            inset_stage_keys.add(pose_key(candidate))
            # Native typed import support enables short approaches in the
            # actual direction of the lower target, independent of the map's
            # axes. The same 96-query budget and all physical proofs apply.
            if magnitude>.25 and any('action_imports' in value for value in cache.values()):
                direction=[v/magnitude for v in delta]
                for length in (8,12,16,24,32):
                    top=[round(p[k]+direction[k]*length,5) for k in (0,1)]+[p[2]]
                    if max(abs(v) for v in top)<=131072:stages.append((direction,top))
            for axis in sorted((0,1),key=lambda k:abs(delta[k])):
                sign=1 if delta[axis]>=0 else -1
                for length in (64,32):
                    for side in (sign,-sign):
                        top=list(p);top[axis]+=side*length
                        direction=[0,0];direction[axis]=side
                        if max(abs(v) for v in top)<=131072:stages.append((direction,top))
            for direction,top in stages:
                if len(queries)>=probe_limit:break
                inset_stage=pose_key(top)in inset_stage_keys
                stage=staging_candidate(p,top,literal_typed=inset_stage)
                if not stage:continue
                top,top_reverse=stage
                landings=([list(nodes[i]['position'])for i in target_ids]
                          if edge.get('synthetic_request')else[])
                if inset_stage:
                    # Tall falls often need the longer physical continuation.
                    # Ask a distinct outward ray before repeating the same
                    # blocked room anchors for every neighbouring upper pose.
                    landings=[[round(top[k]+direction[k]*length,5)for k in(0,1)]+[q[2]]
                              for length in(96,128,80)]+landings
                else:
                    landings.extend([round(top[k]+direction[k]*length,5)for k in(0,1)]+[q[2]]
                                    for length in(56,64,80,96,128))
                inset_trials=0
                for point in landings:
                    if len(queries)>=probe_limit:break
                    if max(abs(v) for v in point)>131072:continue
                    if inset_stage and inset_trials>=6:break
                    if inset_stage and 8<=math.dist(top[:2],point[:2])<=160:inset_trials+=1
                    descent=descent_candidate(top,point)
                    if not descent:continue
                    chosen=finish_candidate(*descent,top,(top,top_reverse))
                    if not chosen and deferred and needs_return and not staged_attempted:
                        # The first complete descent reserves the remaining
                        # shared budget for its independent return. Trying
                        # every further fall first would starve that proof.
                        staged_attempted=True;best=deferred[0]
                        staged=staged_return(p,best[7],probe_limit-len(queries))
                        if staged:chosen=best+(staged,)
                    if chosen:break
                if chosen:break
        if not chosen and deferred and needs_return and not staged_attempted:
            staged_attempted=True;best=deferred[0]
            staged=staged_return(p,best[7],probe_limit-len(queries))
            if staged:chosen=best+(staged,)
        if not chosen:
            # A body can leave a roof corner diagonally while both axial falls
            # hit a neighbouring cab or wall. Prefer combinations whose two
            # individual upper walking offsets have already been proved, then
            # target directions toward actual connected lower floor anchors.
            # Coordinates stay literal; no guessed floor-height projection.
            def cached_ordinary(a,b):
                key=tuple(round(float(x),5) for x in list(a)+list(b))
                return cache.get(key,{}).get('stand')=='accepted'
            corners=[]
            for dx,dy in ((32,64),(64,32)):
                for sx in (-1,1):
                    for sy in (-1,1):
                        top=[p[0]+sx*dx,p[1]+sy*dy,p[2]]
                        if max(abs(v) for v in top)>131072:continue
                        prior=int(cached_ordinary(p,[top[0],p[1],p[2]]))+int(cached_ordinary(p,[p[0],top[1],p[2]]))
                        gap=min(math.dist(top[:2],nodes[i]['position'][:2]) for i in target_ids)
                        corners.append((-prior,gap,top))
            for _,_,top in sorted(corners,key=lambda row:(row[0],row[1],row[2])):
                if len(queries)>=probe_limit:break
                stage=staging_candidate(p,top)
                if not stage:continue
                top,top_reverse=stage
                diagonal=[]
                for dx,dy in ((2,3),(3,2),(1,1),(1,2),(2,1)):
                    magnitude=math.hypot(dx,dy)
                    for sx in (-1,1):
                        for sy in (-1,1):
                            direction=[sx*dx/magnitude,sy*dy/magnitude]
                            alignment=min(1-(direction[0]*(nodes[i]['position'][0]-top[0])+direction[1]*(nodes[i]['position'][1]-top[1]))/
                                          max(.001,math.dist(top[:2],nodes[i]['position'][:2])) for i in target_ids)
                            for length in (64,80):
                                point=[round(top[0]+direction[0]*length,5),round(top[1]+direction[1]*length,5),q[2]]
                                gap=min(math.dist(point[:2],nodes[i]['position'][:2]) for i in target_ids)
                                diagonal.append((alignment,gap,point))
                # Leave room for another corner and its ground/return checks.
                for _,_,point in sorted(diagonal,key=lambda row:(row[0],row[1],row[2]))[:16]:
                    if len(queries)>=probe_limit:break
                    descent=descent_candidate(top,point)
                    if not descent:continue
                    chosen=finish_candidate(*descent,top,(top,top_reverse))
                    if chosen:break
                if chosen:break
        if not chosen:
            routes.append(dict(edge=[a,b],status='no_native_proved_descent_bend',probes=len(queries),
                               requires_proved_return_to_quarantined_source=needs_return and not spawn_terminal,
                               requires_proved_return_to_native_spawn_component=spawn_terminal and needs_return,
                               spawned_entry_only=False,
                               native_spawn_terminal_component=list(terminal_members)if spawn_terminal else None,
                               isolated_generated_spawn_seed=edge.get('reason')=='isolated_generated_spawn_seed',
                               proved_traversal_landing_without_exit=edge.get('reason')=='proved_traversal_landing_without_exit',
                               proved_traversal_terminal_component=list(terminal_members) if terminal_members and not spawn_terminal else None))
            continue
        point,value,reverse,climb,standing,ascent,upper,connection,connector_proof,staged=chosen
        original_b=b;b=connection
        landing=create(point,6,0,'automatic_repair_native_drop_landing',
                       roles=[dict(kind='walk_drop',role='landing')])
        existing.discard((a,original_b))
        drop_source=a
        if upper:
            top,top_reverse=upper
            drop_source=create(top,2,0,'automatic_repair_native_upper_drop_launch',
                               roles=[dict(kind='walk_drop',role='launch',landing_node=landing)],
                               props=dict(tce_walk_drop='1'),editor_role=9)
            existing.add((a,drop_source))
            if top_reverse:existing.add((drop_source,a))
            # An existing explicit drop is now reached through a walking
            # approach. Clear only that superseded own target recipe.
            if start.get('editor_role',0)&8:
                start['editor_role']&=~8
                start['action_roles']=[r for r in start.get('action_roles',[]) if r.get('kind')!='walk_drop']
                for key in ('tce_walk_drop','tce_walk_drop_target'):start.get('action_properties',{}).pop(key,None)
        existing.update(((drop_source,landing),(landing,b)))
        if reverse:existing.add((b,landing))
        if not upper:
            start['generated_guide']=True;start['editor_role']=start.get('editor_role',0)|8
            start['radius']=2;start['topology_protected']=True
            start['action_roles']=[dict(kind='walk_drop',role='launch',landing_node=landing)]
            start.setdefault('action_properties',{})['tce_walk_drop']='1'
            start['action_properties'].pop('tce_walk_drop_target',None)
        chain=[a]+([drop_source] if upper else [])+[landing,b];return_chain=None
        if climb:
            launch=create(climb['launch'],2,512,'automatic_repair_native_free_climb_launch',
                          roles=[dict(kind='free_climb',role='launch',landing_node=drop_source)],
                          props=dict(tce_free_climb='1'),editor_role=129)
            existing.update(((landing,launch),(launch,landing),(launch,drop_source)))
            if not upper or upper[1]:return_chain=[b,landing,launch,drop_source]+([a] if upper else [])
        elif standing:
            # The newly created drop landing is also the precise, directed
            # standing-jump launch. Do not add overlapping duplicate guides
            # or reinterpret any pre-existing node as a jump.
            nodes[landing].update(radius=2,source_radius=2,nav_flags=1<<27,editor_role=65)
            nodes[landing]['action_roles'].append(dict(kind='standing_jump',role='launch',landing_node=drop_source))
            nodes[landing]['action_properties']['tce_stand_jump']='1'
            existing.add((landing,drop_source))
            if not upper or upper[1]:return_chain=[b,landing,drop_source]+([a] if upper else [])
        return_proof=None
        if staged and staged.get('model')=='native_standing_jump_alternate_launch_v1':
            street=create(staged['street'],2,1<<27,'automatic_repair_native_standing_jump_launch',
                roles=[dict(kind='standing_jump',role='launch',landing_node=drop_source)],
                props=dict(tce_stand_jump='1'),editor_role=65)
            existing.update(((staged['connector'],street),(street,staged['connector']),(street,drop_source)))
            return_chain=staged['from_descent_connector']['source_ids']+[street,drop_source,a]
            upper_key=tuple(round(float(x),5) for x in nodes[drop_source]['position']+nodes[a]['position'])
            return_proof=dict(kind='standing_jump_alternate_launch',proof=staged,street_launch_source_id=street,
                action_probe=staged['action_probe'],walk_probes=staged['street_walks']+[
                    dict(start=nodes[drop_source]['position'],end=nodes[a]['position'],proof=cache[upper_key])])
            if connector_proof:return_proof['existing_graph_walk']=connector_proof['from_original_target']
        elif staged:
            middle=create(staged['middle'],2,0,'automatic_repair_native_standing_jump_landing')
            middle_launch=create(staged['launch'],2,1<<27,'automatic_repair_native_standing_jump_launch',
                roles=[dict(kind='standing_jump',role='launch',landing_node=a)],props=dict(tce_stand_jump='1'),editor_role=65)
            street=create(staged['street'],2,1<<27,'automatic_repair_native_standing_jump_launch',
                roles=[dict(kind='standing_jump',role='launch',landing_node=middle)],props=dict(tce_stand_jump='1'),editor_role=65)
            existing.update(((staged['connector'],street),(street,staged['connector']),(street,middle),
                             (middle,middle_launch),(middle_launch,middle),(middle_launch,a)))
            return_chain=staged['from_descent_connector']['source_ids']+[street,middle,middle_launch,a]
            return_proof=dict(kind='two_standing_jumps',proof=staged,
                              landing_source_id=middle,launch_source_id=middle_launch,street_launch_source_id=street)
            if connector_proof:return_proof['existing_graph_walk']=connector_proof['from_original_target']
        elif return_chain:
            action_pair=(return_chain[-3],return_chain[-2]) if upper else (return_chain[-2],return_chain[-1])
            walk_proofs=[]
            for source,destination in zip(return_chain,return_chain[1:]):
                if (source,destination)==action_pair:continue
                begin,end=nodes[source]['position'],nodes[destination]['position']
                key=tuple(round(float(x),5) for x in begin+end)
                walk_proofs.append(dict(start=begin,end=end,proof=cache[key]))
            return_proof=dict(kind='free_climb' if climb else 'standing_jump',action_probe=ascent,
                              walk_probes=walk_proofs)
            if connector_proof:return_proof['existing_graph_walk']=connector_proof['from_original_target']
        if return_chain:
            counterpart=(original_b,a)
            rejected_return=next((e for e in native.get('edges',[]) if (e['from'],e['to'])==counterpart and
                e.get('accepted') is False and not e.get('flags',0) and e.get('reason') in
                {'blocked_clearance','blocked_sweep','blocked_step_up','blocked_step_down','different_floor',
                 'gap_or_drop','step_or_drop','capsule_support_gap_or_drop','capsule_support_step_or_drop'}),None)
            if ((b==original_b or connector_proof) and counterpart in existing and rejected_return and
                    counterpart not in {tuple(e) for e in doc.get('base',{}).get('protected_action_edges',[])}):
                existing.remove(counterpart);replacements.append(list(counterpart))
        if not edge.get('synthetic_request'):replacements.append([a,original_b])
        resolved_sources.add(a)
        if terminal_members:resolved_terminal_components.add(terminal_members)
        if connector_proof:
            chain+=connector_proof['to_original_target']['source_ids'][1:]
            if return_chain:return_chain=connector_proof['from_original_target']['source_ids'][:-1]+return_chain
        routes.append(dict(edge=[a,original_b],connection_node=b,connector_paths=connector_proof,chain=chain,return_chain=return_chain,
                           status='native_probed_action_proposal_requires_full_revalidation',
                           original_flags_preserved=True,drop_proof=value['drop'],
                           requires_proved_return_to_quarantined_source=needs_return and not spawn_terminal,
                           requires_proved_return_to_native_spawn_component=spawn_terminal and needs_return,
                           native_spawn_terminal_component=list(terminal_members)if spawn_terminal else None,
                           spawned_entry_only=spawn_terminal and not needs_return,
                           spawn_source_ids=terminal['spawn_source_ids']if spawn_terminal else[],
                           main_targets=sorted(terminal['return_targets'])if spawn_terminal else[],
                           source_model=terminal['source_model']if spawn_terminal else None,
                           return_proof=return_proof,
                           return_chain_proof_sha256=hashlib.sha256(json.dumps(return_proof,sort_keys=True,separators=(',',':')).encode()).hexdigest() if return_proof else None,
                           upper_approach=upper[0] if upper else None,
                           isolated_generated_spawn_seed=edge.get('reason')=='isolated_generated_spawn_seed',
                           proved_traversal_landing_without_exit=edge.get('reason')=='proved_traversal_landing_without_exit',
                           proved_traversal_terminal_component=list(terminal_members) if terminal_members and not spawn_terminal else None))
    result['nodes'].sort(key=lambda n:n['id'])
    result['edges']=[dict(zip(('from','to'),pair)) for pair in sorted(existing)]
    result['next_id']=next_id
    evidence=dict(model='native_directed_descent_bend_proposals_v1',routes=routes,queries=queries,
                  max_probes=max_probes,added_nodes=len(nodes)-len(original_ids),
                  source_probe_allocations=allocations,eligible_source_ids=source_order,
                  unprobed_source_ids=[i for i in source_order if i not in processed_sources],
                  ascent_touchdown_launch_candidates=landing_launches,
                  quarantined_floor_probe_source_ids=sorted(quarantined_floor_probes),
                  direct_probes=sum(direct_used.values()),staging_probe_reserve=24,
                  blocked_drop_ray_count=len(blocked_drop_rays),
                  connected_isolated_source_ids=sorted(r['edge'][0] for r in routes if
                      (r.get('isolated_generated_spawn_seed') or r.get('proved_traversal_landing_without_exit')) and r.get('chain')),
                  connected_terminal_component_source_ids=sorted(r['edge'][0] for r in routes if
                      r.get('proved_traversal_terminal_component') and r.get('chain')),
                  connected_terminal_components=[list(group) for group in sorted(resolved_terminal_components)],
                  connected_native_spawn_components=[list(group)for group in sorted(resolved_terminal_components)
                      if any(i in spawn_components for i in group)],
                  replaced_rejected_edges=replacements,deleted_nodes=0,native_revalidation_required=True)
    return result,evidence


def _local_gap_requests(doc, native, graph, index, existing, limit=64):
    """Measure missing local links on the current reciprocal ordinary graph.

    A strongly connected graph can still take a whole-room detour around a
    missing short connection. Never infer walk connectivity from one-way,
    action, team, property-driven or merely candidate edges.
    """
    authored={n['id']:n for n in doc['nodes']}
    actual={n['id']:n for n in native.get('nodes',[])}
    action_ids={r[key] for r in native.get('traversal_routes',[]) for key in
                ('approach','launch','landing','entry','exit') if type(r.get(key)) is int}
    simple=set()
    offset=native.get('bbox',{}).get('origin_above_feet',30)
    for identity in index:
        node=authored[identity];source=actual.get(identity,{})
        flags=int(node.get('nav_flags',0))
        if (identity in action_ids or flags not in (0,64) or
            bool(node.get('crouch'))!=bool(flags&64) or
            any(node.get(k) for k in ('editor_role','generated_guide','action_roles','action_properties')) or
            source.get('accepted') is not True or int(source.get('nav_flags',flags))!=flags or
            bool(source.get('crouch',node.get('crouch')))!=bool(node.get('crouch')) or
            source.get('editor_role',0)):
            continue
        if 'position' in source and math.dist(node['position'],
                [source['position'][0],source['position'][1],source['position'][2]-offset])>.26:
            continue
        simple.add(identity)
    walk_reasons={'accepted','external_native_validated_walk','native_validated_walk','external_native_component_bridge'}
    accepted={(e['from'],e['to']) for e in native.get('edges',[]) if e.get('accepted') is True and
              e.get('reason','accepted') in walk_reasons and (e['from'],e['to']) in existing}
    adjacent=defaultdict(dict);incident=defaultdict(set)
    for a,b in existing:incident[a].add(b);incident[b].add(a)
    for a,b in accepted:
        if a in simple and b in simple and (b,a) in accepted:
            adjacent[a][b]=math.dist(authored[a]['position'],authored[b]['position'])
    # An endpoint beside an authored one-way/action/rejected link does not
    # acquire a reverse escape simply because the map is globally connected.
    endpoints={a for a in simple if adjacent[a] and all(b in simple and
                (a,b) in accepted and (b,a) in accepted for b in incident[a])}
    components={}
    for start in sorted(simple):
        if start in components:continue
        stack=[start];components[start]=start
        while stack:
            for target in adjacent[stack.pop()]:
                if target not in components:components[target]=start;stack.append(target)
    potential=[]
    for a in sorted(endpoints):
        for ib in graph.near(authored[a]['position'],256):
            if ib>=len(index):continue
            b=graph.nodes[ib].get('source_id')
            if b is None:continue
            if b<=a or b not in endpoints or components[a]!=components[b] or (a,b) in existing or (b,a) in existing:
                continue
            p,q=authored[a]['position'],authored[b]['position'];gap=math.dist(p,q)
            if gap<24 or abs(p[2]-q[2])>18.25 or graph.nodes[index[a]]['component']!=graph.nodes[ib]['component']:
                continue
            potential.append((gap,a,b))
    potential=sorted(set(potential))
    distances={};measured=[];budget=8192;source_limit=1024
    def shortest(start):
        if start in distances:return distances[start]
        if len(distances)>=source_limit:return None
        costs={start:0.};queue=[(0.,start)]
        while queue:
            cost,current=heapq.heappop(queue)
            if cost!=costs[current]:continue
            for target,length in adjacent[current].items():
                total=cost+length
                if total<costs.get(target,float('inf')):
                    costs[target]=total;heapq.heappush(queue,(total,target))
        distances[start]=costs;return costs
    for gap,a,b in potential[:budget]:
        forward,reverse=shortest(a),shortest(b)
        if forward is None or reverse is None:continue
        before=[forward.get(b),reverse.get(a)]
        if any(value is None for value in before) or min(before)<max(gap*1.8,gap+96):continue
        measured.append(dict(edge=[a,b],detour_before=before,direct_distance=gap))
    measured.sort(key=lambda r:(-(min(r['detour_before'])-r['direct_distance']),r['direct_distance'],r['edge']))
    return measured[:limit],simple,dict(model='reciprocal_native_ordinary_local_detours_v1',
        eligible_endpoints=len(endpoints),nearby_pairs=len(potential),measured_sources=len(distances),
        pair_budget=budget,source_budget=source_limit,proposed_route_limit=8,
        search_truncated=len(potential)>budget or len(distances)>=source_limit,
        qualifying_pairs=len(measured),native_revalidation_required=True)


def _local_corridor_clear(graph, route, geometry):
    """Negative mesh/liquid/dynamic-body filters, never a native certificate."""
    obstacles=[];seen=set()
    for obstacle in geometry.get('dynamic_obstacles',[])+geometry.get('excluded_inline_models',[]):
        if not (obstacle.get('dynamic_obstacle') or obstacle.get('stationary') is False or
                obstacle.get('classname','').startswith(('func_door','func_button','func_train','func_plat'))):continue
        lo,hi=obstacle.get('mins'),obstacle.get('maxs')
        if not isinstance(lo,list) or not isinstance(hi,list) or len(lo)!=3 or len(hi)!=3:continue
        key=tuple(lo+hi)
        if key not in seen:seen.add(key);obstacles.append((lo,hi))
    for ia,ib in zip(route['chain'],route['chain'][1:]):
        a,b=graph.nodes[ia],graph.nodes[ib];p,q=a['point'],b['point']
        if not graph.visible(a,b) or not graph.repair_margin(a,b):return False
        height=graph.movement_profile['crouching_height' if a['crouch'] or b['crouch'] else 'standing_height']
        radius=graph.movement_profile['radius']
        for lo,hi in obstacles:
            first,last=0.,1.
            for axis in range(3):
                low=lo[axis]-(radius if axis<2 else height);high=hi[axis]+(radius if axis<2 else 0)
                delta=q[axis]-p[axis]
                if abs(delta)<1e-9:
                    if not low<=p[axis]<=high:first,last=1.,0.;break
                else:
                    u,v=(low-p[axis])/delta,(high-p[axis])/delta
                    first=max(first,min(u,v));last=min(last,max(u,v))
            if first<=last:return False
        previous=None;steps=max(1,math.ceil(math.dist(p,q)/8))
        for step in range(steps+1):
            t=step/steps;point=[p[k]+(q[k]-p[k])*t for k in range(3)]
            polygon=graph.mesh.locate(point,tolerance=18.25)
            if polygon is None or graph.mesh.component[polygon]!=a['component']:return False
            point[2]=graph.mesh.height(polygon,point)
            if previous is not None and abs(previous-point[2])>18.25:return False
            previous=point[2]
            if graph.point_filter and not graph.point_filter.allows(point,polygon,'local_gap_corridor'):return False
    return True


def propose(doc, native, stand, duck, geometry, max_routes=128):
    from waygraph import SpatialGraph, BspPointFilters, distance, native_ordinary_pairs
    result = copy.deepcopy(doc)
    authored = {n["id"]: n for n in result["nodes"]}
    original_ids=set(authored)
    actual = {n["id"]: n for n in native.get("nodes", [])}
    action_ids={r[key] for r in native.get('traversal_routes',[]) for key in
                ('low','high','approach','launch','landing','entry','exit','paired_jump_launch','release','alignment')
                if type(r.get(key)) is int and r[key]>=0}
    mesh_nodes, identities, changed_flags = [], [], []
    for identity, node in sorted(authored.items()):
        flags=int(node["nav_flags"])
        # Explicit actions/team/tactical flags stay authored. Ordinary floor
        # corridors must never be used to replace jump/drop/ladder recipes.
        floor_helper=(node.get('generated_guide') is True and node.get('editor_role')==1)
        if (flags & ~64 or identity in action_ids or node.get('action_roles') or
                node.get('action_properties') or node.get('door_recipe') or
                (node.get("editor_role",0) or node.get("generated_guide")) and not floor_helper):
            continue
        source=actual.get(identity,{})
        if not source.get("accepted") and not source.get("native_input",{}).get("floor_accepted"):
            continue
        p=duck.locate(node["position"], tolerance=18.25)
        if p is None:
            continue
        crouch=bool(node.get("crouch") or source.get("crouch") or stand.locate(node["position"]) is None)
        # Plain generated floor helpers may be endpoints of a failed walk.
        # Keep their metadata/posture intact; physical action roles stay out.
        if floor_helper and crouch!=bool(node.get('crouch')):continue
        if crouch and not flags&64:
            node["nav_flags"]=flags|64;node["crouch"]=True;changed_flags.append(identity)
        identities.append(identity)
        mesh_nodes.append(dict(point=list(node["position"]),polygon=p,component=duck.component[p],source_id=identity,
                               crouch=crouch,spawn=bool(node.get("spawn")),kind="authored_repair_anchor"))
    index={identity:i for i,identity in enumerate(identities)}
    existing={(e["from"],e["to"]) for e in result["edges"]}
    accepted={(e["from"],e["to"]) for e in native.get("edges",[]) if e.get("accepted")}
    # SpatialGraph is undirected. A single accepted direction, or an action
    # transition, cannot certify its reverse and suppress a needed corridor.
    # Use the same reciprocal ordinary evidence as fresh generation.
    proved={tuple(sorted((index[a],index[b])))
            for a,b in native_ordinary_pairs(native,max(index,default=-1)+1)
            if a in index and b in index}
    graph=SpatialGraph(stand,duck,128.,mesh_nodes,proved,point_filter=BspPointFilters(geometry))
    blocked=ORDINARY_COLLISION_REJECTIONS
    retry={(e["from"],e["to"]) for e in native.get("edges",[]) if not e.get("accepted")
           and e.get("reason") in blocked and (e["from"],e["to"]) in existing and e["from"] in index and e["to"] in index}
    graph.blocked={tuple(sorted((index[a],index[b]))) for a,b in retry}
    requests=[(a,b,False) for a,b in sorted(retry)]

    # Preserve actions when grouping accepted components; they can already
    # join floor patches across stairs/ladder spans absent in Recast.
    neighbours=defaultdict(set)
    for a,b in accepted:neighbours[a].add(b);neighbours[b].add(a)
    components={};groups=[]
    for identity in sorted(authored):
        if identity in components:continue
        group=set();stack=[identity]
        while stack:
            current=stack.pop()
            if current in group:continue
            group.add(current);stack.extend(neighbours[current]-group)
        number=len(groups);groups.append(group)
        components.update({i:number for i in group})
    main=max(range(len(groups)),key=lambda i:len(groups[i]),default=None)
    targets=[i for i in identities if components.get(i)==main]
    for group_id,group in enumerate(groups):
        if group_id==main:continue
        choices=sorted((distance(authored[a]["position"],authored[b]["position"]),a,b)
                       for a in group if a in index for b in targets
                       if mesh_nodes[index[a]]["component"]==mesh_nodes[index[b]]["component"])
        for gap,a,b in choices[:8]:
            if gap>768:break
            requests.append((a,b,True))

    local_requests,local_ordinary,local_search=_local_gap_requests(result,native,graph,index,existing)
    local_info={tuple(r['edge']):r for r in local_requests}
    requests.extend((a,b,True) for a,b in local_info)

    routes=[];new_edges=set(existing);replaced=set();next_id=result["next_id"]
    mapping=dict(enumerate(identities))
    # A corridor turn may coincide with an existing ordinary floor anchor.
    # Reuse only exact feet/posture/flags; action lips and recipes must retain
    # their separate identities and arrival tolerances. No source field changes.
    def ordinary_reuse(node):
        flags=int(node.get("nav_flags",0))
        return (flags in (0,64) and bool(node.get("crouch"))==bool(flags&64) and
                node.get("radius",35)>6 and not node.get("editor_role",0) and
                not node.get("generated_guide") and not node.get("action_roles") and
                not node.get("action_properties"))
    reusable=[n for _,n in sorted(authored.items()) if ordinary_reuse(n)]
    reused=[];local_count=0;local_endpoints=set()
    for a,b,bidirectional in requests[:max_routes]:
        if len(graph.nodes)>=7580:break
        ia,ib=index[a],index[b]
        local=local_info.get((a,b)) if (a,b) not in retry and components.get(a)==components.get(b) else None
        if local and (local_count>=8 or a in local_endpoints or b in local_endpoints):continue
        if bidirectional and graph.has_route(ia,ib,max(256.,distance(graph.nodes[ia]["point"],graph.nodes[ib]["point"])*1.75)):
            continue
        # A rejected shortcut needs a real bend. A disconnected patch may use
        # a new clear direct local connection when the mesh certifies it.
        if local:
            checkpoint=(len(graph.nodes),set(graph.edges),{i:set(v) for i,v in graph.adjacency.items()})
        route=graph.corridor(ia,ib,force_bend=not bidirectional)
        if not route:
            routes.append(dict(edge=[a,b],status="no_mesh_certified_corridor"));continue
        if local:
            length=sum(math.dist(graph.nodes[u]['point'],graph.nodes[v]['point']) for u,v in zip(route['chain'],route['chain'][1:]))
            valid=(len(graph.nodes)-checkpoint[0]<=2 and length<=local['direct_distance']*1.6 and
                   min(local['detour_before'])>=max(length*1.8,length+96) and
                   all(i>=checkpoint[0] or mapping[i] in local_ordinary for i in route['chain']) and
                   _local_corridor_clear(graph,route,geometry))
            if not valid:
                for i in range(checkpoint[0],len(graph.nodes)):graph.grid[graph.cell(graph.nodes[i]['point'])].remove(i)
                del graph.nodes[checkpoint[0]:];graph.edges=checkpoint[1]
                graph.adjacency=defaultdict(set,checkpoint[2])
                continue
            reused.extend(mapping[i] for i in route['chain'][1:-1] if i<checkpoint[0] and mapping[i] in original_ids)
        for i in range(len(mapping),len(graph.nodes)):
            source=graph.nodes[i];flags=64 if source["crouch"] else 0
            match=next((n for n in reusable if int(n["nav_flags"])==flags and
                        bool(n["crouch"])==bool(source["crouch"]) and
                        math.dist(n["position"],source["point"])<=.001),None)
            if match is not None:
                mapping[i]=match["id"];reused.append(match["id"])
                continue
            identity=next_id;next_id+=1;mapping[i]=identity
            node=dict(id=identity,position=list(source["point"]),radius=35,nav_flags=flags,
                      crouch=bool(source["crouch"]),spawn=False,topology_protected=True,locked=False,
                      generated_guide=False,source_reason="automatic_repair_corridor")
            result["nodes"].append(node);authored[identity]=node;reusable.append(node)
        chain=[]
        for i in route["chain"]:
            identity=mapping[i]
            if not chain or chain[-1]!=identity:chain.append(identity)
        if len(chain)<2:continue
        links=set(zip(chain,chain[1:]))
        if bidirectional:links|={(v,u) for u,v in links}
        new_edges.update(links)
        if (a,b) in retry and len(chain)>2:
            new_edges.discard((a,b));replaced.add((a,b))
        routes.append(dict(edge=[a,b],chain=chain,bidirectional=bidirectional,
                           status="mesh_proposal_requires_native_revalidation",polygons=route["polygons"]))
        if local:
            routes[-1].update(kind='same_component_local_gap',detour_before=local['detour_before'],
                              proposed_distance=length,direct_distance=local['direct_distance'])
            local_count+=1;local_endpoints.update((a,b))
    result["nodes"].sort(key=lambda n:n["id"])
    result["edges"]=[dict(zip(("from","to"),pair)) for pair in sorted(new_edges)]
    result["next_id"]=next_id
    evidence=dict(model="current_authored_mesh_corridor_proposals_v1",added_nodes=len(set(authored)-original_ids),
                  added_directed_edges=len(new_edges-existing),replaced_rejected_edges=[list(p) for p in sorted(replaced)],
                  required_crouch_flags=changed_flags,retained_node_ids=sorted(original_ids),deleted_nodes=0,
                  routes=routes,reused_ordinary_source_ids=sorted(set(reused)),native_revalidation_required=True,
                  native_collision_proved_bidirectional_pairs=len(proved))
    evidence['local_gap_search']=dict(local_search,proposed_routes=local_count)
    return result,evidence
