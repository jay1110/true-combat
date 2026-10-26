"""Current native capsule rejects may withdraw only redundant ordinary edges."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

from edit import Drafts, graph_hash
from repair import rejected_ordinary_edge_feedback


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


class RejectedOrdinaryFeedbackTests(unittest.TestCase):
    def fixture(self, positions=None, edges=None, rejected=None):
        positions = positions or {1: [0., 0., 0.], 2: [64., 0., 0.], 3: [32., 32., 0.]}
        edges = edges or [(1, 2), (1, 3), (3, 2), (2, 1)]
        rejected = rejected or {(1, 2): 'capsule_support_gap_or_drop'}
        nodes = [dict(id=i, position=pos, radius=35, nav_flags=0, crouch=False, spawn=i == 1,
                      name='point'+str(i), facing=[1, 0, 0], way_guid=i,
                      topology_protected=False, locked=False, generated_guide=False,
                      editor_role=0, action_roles=[], action_properties={}) for i, pos in sorted(positions.items())]
        self.environment = {key: digest(key.encode()) for key in
                            ('bsp', 'stand', 'duck', 'geometry', 'collision', 'runtime_snapshot')}
        doc = dict(schema=1, map='obj_test', mode='manual_draft', revision=4, saved_revision=4,
                   base=dict(environment=self.environment, protected_action_edges=[]), nodes=nodes,
                   edges=[dict(**{'from': a, 'to': b}) for a, b in edges], next_id=max(positions)+1,
                   undo=[], redo=[], export=None)
        native = dict(map='obj_test', candidate_mode='manual_draft', generation_complete=False,
                      source='maps/obj_test_candidate_edit.txt', waypoint_metadata_contract='TCE_NODE_METADATA_1',
                      bbox=dict(origin_above_feet=30), traces=100, trace_budget=1000,
                      traversal_routes=[], nodes=[], edges=[])
        for node in nodes:
            raw = node['position'][:2]+[node['position'][2]+30]
            native['nodes'].append(dict(id=node['id'], raw_position=raw, position=raw, accepted=True,spawn_reachable=True,
                native_input=dict(floor_accepted=True), nav_flags=node['nav_flags'], crouch=node['crouch'],
                import_uid=node['way_guid'], way_guid=0, name=node['name'], facing=node['facing'],
                custom_properties={}))
        for a, b in edges:
            reason = rejected.get((a, b))
            native['edges'].append(dict(**{'from': a, 'to': b}, accepted=not bool(reason),
                                       reason=reason or 'external_native_validated_walk', crouch=False))
        return doc, native, self.bind(doc, native)

    def bind(self, doc, native):
        """Independent raw-byte receipt builder; no game/API or report mutation."""
        candidate = b'TCE_NAVMESH 5 obj_test supplied-counts\r\n'
        metadata = dict(map=doc['map'], candidate_mode='manual_draft',
                        candidate_sha256=digest(candidate.replace(b'\r\n', b'\n')),
                        edit_draft=dict(revision=doc['revision'], sha256=graph_hash(doc), base=copy.deepcopy(doc['base'])),
                        sources={key: dict(sha256=self.environment[key]) for key in ('stand', 'duck', 'geometry')},
                        nodes=copy.deepcopy(doc['nodes']),
                        edges=[[e['from'], e['to']] for e in doc['edges']])
        doc['export'] = dict(revision=doc['revision'], sha256=graph_hash(doc),
                             candidate='obj_test_candidate_edit.txt', candidate_sha256=metadata['candidate_sha256'])
        quarantined = [n for n in native['nodes'] if n.get('accepted') is False]
        rejected = [e for e in native['edges'] if e['accepted'] is False and e['reason']!='quarantined_unreachable_endpoint']
        counts = {}
        for edge in rejected: counts[edge['reason']] = counts.get(edge['reason'],0)+1
        native['quarantined_unreachable_nodes'] = len(quarantined)
        native['native_input_coverage'] = dict(input_nodes=len(doc['nodes']), native_floor_accepted=len(doc['nodes']),
            native_floor_rejected=0, initial_directed_links=len(doc['edges']),
            initial_accepted_links=len(doc['edges'])-len(rejected), initial_rejected_links=len(rejected),
            geometry_rejections=[], connectivity_missing_input_nodes=len(quarantined),
            ordinary_without_exit=[n['id']for n in native['nodes']if n.get('accepted')is True and n.get('nav_flags')==0 and
                not any(e.get('accepted')is True and e['from']==n['id']for e in native['edges'])],
            exported_input_nodes=len(doc['nodes'])-len(quarantined),spawn_reachable_input_nodes=len(doc['nodes'])-len(quarantined),
            contracted_input_nodes=0,link_rejections=[dict(reason=r,count=c)for r,c in counts.items()],
            capacity=dict(input_validation_complete=True, input_truncated=False))
        report_raw = json.dumps(native).encode(); metadata_raw = json.dumps(metadata).encode()
        inputs = dict(self.environment, game_module=digest(b'native module'),
                      candidate=digest(candidate), candidate_metadata=digest(metadata_raw),
                      navigation_contract='tce_pm_middle_water_v2_flat_slick_v1')
        status = dict(current=True, candidate='obj_test_candidate_edit.txt',
                      candidate_sha256=inputs['candidate'], native_report=digest(report_raw))
        attempt = dict(map=doc['map'], generation_complete=False, candidate=status['candidate'],
                       inputs=copy.deepcopy(inputs), native_report=status['native_report'], staged_candidate=inputs['candidate'])
        return dict(report_status=status, attempt=attempt, inputs=inputs, metadata=metadata,
                    native_report_bytes=report_raw, candidate_bytes=candidate, candidate_metadata_bytes=metadata_raw)

    def test_exact_failed_old_edge_only_with_real_directed_alternate_and_undo(self):
        doc, native, binding = self.fixture(); before = copy.deepcopy((doc, native, binding))
        proposal, receipt = rejected_ordinary_edge_feedback(doc, native, binding)
        self.assertEqual((doc, native, binding), before)
        self.assertEqual(proposal['nodes'], doc['nodes'])
        self.assertEqual({(e['from'], e['to']) for e in proposal['edges']}, {(1, 3), (3, 2), (2, 1)})
        self.assertEqual(receipt['withdrawn_edges'][0]['alternate_source_ids'], [1, 3, 2])
        self.assertEqual(receipt['withdrawn_edges'][0]['edge'], [1, 2])
        self.assertTrue(receipt['directed_reachability_preserved']); self.assertTrue(receipt['native_revalidation_required'])
        self.assertFalse(receipt['changes_node_metadata']); self.assertFalse(receipt['changes_positions'])
        with tempfile.TemporaryDirectory() as folder:
            drafts = Drafts(Path(folder)); drafts.documents[doc['map']] = copy.deepcopy(doc)
            applied = drafts.apply_repair(doc['map'], doc['revision'], self.environment, proposal)
            self.assertEqual(applied['nodes'], doc['nodes']); self.assertIsNone(applied['export'])
            self.assertEqual(applied['undo'][-1]['before'], []); self.assertEqual(applied['undo'][-1]['after'], [])
            self.assertEqual(applied['undo'][-1]['removed'], [[1, 2]])
            restored = drafts.change(doc['map'], dict(op='undo', revision=applied['revision']), self.environment)
            self.assertEqual(restored['nodes'], doc['nodes'])
            self.assertEqual({(e['from'], e['to']) for e in restored['edges']},
                             {(e['from'], e['to']) for e in doc['edges']})
        def reachability(document):
            pairs = {(e['from'], e['to']) for e in document['edges']}; result = set()
            for start in [n['id'] for n in document['nodes']]:
                visited = {start}; frontier = [start]
                while frontier:
                    source = frontier.pop()
                    for a, b in pairs:
                        if a == source and b not in visited: visited.add(b); frontier.append(b)
                result.update((start, end) for end in visited)
            return result
        self.assertEqual(reachability(doc), reachability(proposal))

    def test_independent_exactly_reported_sink_does_not_block_local_proved_withdrawal(self):
        doc,native,_=self.fixture(positions={1:[0.,0.,0.],2:[64.,0.,0.],3:[32.,32.,0.],4:[160.,0.,80.]},
            edges=[(1,2),(1,3),(3,2),(2,1),(2,4)])
        bound=self.bind(doc,native)
        native['native_input_coverage']['ordinary_without_exit']=[4]
        raw=json.dumps(native).encode();bound['native_report_bytes']=raw
        bound['report_status']['native_report']=bound['attempt']['native_report']=digest(raw)
        before=copy.deepcopy((doc,native,bound))
        proposal,evidence=rejected_ordinary_edge_feedback(doc,native,bound)
        self.assertEqual((doc,native,bound),before)
        self.assertEqual(proposal['nodes'],doc['nodes'])
        self.assertEqual(evidence['remaining_ordinary_without_exit'],[4])
        self.assertEqual(evidence['withdrawn_edges'][0]['alternate_source_ids'],[1,3,2])
        self.assertEqual(proposal['edges'],[e for e in doc['edges']if(e['from'],e['to'])!=(1,2)])
        self.assertTrue(evidence['native_revalidation_required'])
        # Partial coverage is literal, never a license to omit or invent a
        # sink, coerce IDs, or overlook another unfinished input phase.
        for bad in([], [3], [4,4], ['4'], None):
            changed=copy.deepcopy(native);changed['native_input_coverage']['ordinary_without_exit']=bad
            other=copy.deepcopy(bound);raw=json.dumps(changed).encode();other['native_report_bytes']=raw
            other['report_status']['native_report']=other['attempt']['native_report']=digest(raw)
            with self.subTest(sinks=bad):self.assertIsNone(rejected_ordinary_edge_feedback(doc,changed,other))

    def test_current_binding_raw_bytes_and_saved_export_must_all_match(self):
        for case in ('not_current', 'attempt_inputs', 'attempt_report', 'attempt_candidate', 'staged_candidate',
                     'report_bytes', 'candidate_bytes', 'metadata_bytes', 'native_object', 'metadata_object',
                     'unsaved', 'stale_export', 'missing_export', 'source_name', 'source_geometry', 'missing_hash'):
            doc, native, bound = self.fixture()
            if case == 'not_current': bound['report_status']['current'] = False
            elif case == 'attempt_inputs': bound['attempt']['inputs']['game_module'] = digest(b'old')
            elif case == 'attempt_report': bound['attempt']['native_report'] = digest(b'old')
            elif case == 'attempt_candidate': bound['attempt']['candidate'] = 'obj_other.txt'
            elif case == 'staged_candidate': bound['attempt']['staged_candidate'] = digest(b'old')
            elif case == 'report_bytes': bound['native_report_bytes'] += b' '
            elif case == 'candidate_bytes': bound['candidate_bytes'] += b' '
            elif case == 'metadata_bytes': bound['candidate_metadata_bytes'] += b' '
            elif case == 'native_object': native['edges'][0]['reason'] = 'capsule_support_step_or_drop'
            elif case == 'metadata_object': bound['metadata']['edit_draft']['revision'] -= 1
            elif case == 'unsaved': doc['saved_revision'] -= 1
            elif case == 'stale_export': doc['export']['sha256'] = digest(b'old')
            elif case == 'missing_export': doc['export'] = None
            elif case == 'source_name': native['source'] = 'obj_other.txt'; bound = self.bind(doc, native)
            elif case == 'source_geometry': bound['inputs']['geometry'] = digest(b'other')
            else: bound['inputs'].pop('runtime_snapshot')
            with self.subTest(case=case): self.assertIsNone(rejected_ordinary_edge_feedback(doc, native, bound))

    def test_unsafe_nodes_actions_unknown_reasons_and_incomplete_inputs_do_not_disappear(self):
        for case in ('invalid_floor', 'changed_position', 'changed_flags', 'changed_name', 'changed_facing',
                     'changed_uid', 'changed_properties', 'duplicate_nodes', 'missing_edge', 'truncated',
                     'incomplete', 'trace_budget', 'geometry_rejection', 'no_exit', 'source_action',
                     'alternate_action', 'native_action', 'protected', 'unknown_reason', 'other_reason'):
            doc, native, _ = self.fixture()
            if case == 'invalid_floor': native['nodes'][2]['native_input']['floor_accepted'] = False
            elif case == 'changed_position': native['nodes'][2]['raw_position'] = [32, 33, 30]
            elif case == 'changed_flags': native['nodes'][2]['nav_flags'] = 64
            elif case == 'changed_name': native['nodes'][2]['name'] = 'different'
            elif case == 'changed_facing': native['nodes'][2]['facing'] = [0, 1, 0]
            elif case == 'changed_uid': native['nodes'][2]['import_uid'] += 1
            elif case == 'changed_properties': native['nodes'][2]['custom_properties'] = {'pause': '10'}
            elif case == 'duplicate_nodes': native['nodes'].append(copy.deepcopy(native['nodes'][0]))
            elif case == 'missing_edge': native['edges'].pop()
            elif case == 'source_action': doc['nodes'][0]['action_roles'] = [dict(kind='walk_drop', role='launch', landing_node=2)]
            elif case == 'alternate_action': doc['nodes'][2]['nav_flags'] = 1 << 9; native['nodes'][2]['nav_flags'] = 1 << 9
            elif case == 'native_action': native['traversal_routes'] = [dict(launch=3, landing=2)]
            elif case == 'protected': doc['base']['protected_action_edges'] = [[1, 2]]
            elif case == 'unknown_reason': native['edges'][0]['reason'] = 'unknown_collision_decision'
            elif case == 'other_reason': native['edges'][0]['reason'] = 'capsule_support_hazard'
            bound = self.bind(doc, native)
            if case == 'truncated': native['native_input_coverage']['capacity']['input_truncated'] = True
            elif case == 'incomplete': native['native_input_coverage']['capacity']['input_validation_complete'] = False
            elif case == 'trace_budget': native['traces'] = native['trace_budget']
            elif case == 'geometry_rejection': native['native_input_coverage']['geometry_rejections'] = [dict(id=3)]
            elif case == 'no_exit': native['native_input_coverage']['ordinary_without_exit'] = [3]
            if case in ('truncated', 'incomplete', 'trace_budget', 'geometry_rejection', 'no_exit'):
                bound['native_report_bytes'] = json.dumps(native).encode()
                bound['report_status']['native_report'] = bound['attempt']['native_report'] = digest(bound['native_report_bytes'])
            with self.subTest(case=case): self.assertIsNone(rejected_ordinary_edge_feedback(doc, native, bound))

    def test_both_precise_capsule_reasons_and_ordinary_crouch_remain_supported(self):
        for reason in ('capsule_support_gap_or_drop', 'capsule_support_step_or_drop'):
            doc, native, _ = self.fixture(rejected={(1, 2): reason})
            doc['nodes'][2].update(nav_flags=64, crouch=True); native['nodes'][2].update(nav_flags=64, crouch=True)
            bound = self.bind(doc, native)
            proposal, receipt = rejected_ordinary_edge_feedback(doc, native, bound)
            self.assertEqual(receipt['withdrawn_edges'][0]['reason'], reason)
            self.assertEqual(proposal['nodes'], doc['nodes'])

    def test_known_completed_collision_decisions_use_only_already_accepted_ordinary_alternate(self):
        reasons=('blocked_clearance','blocked_sweep','blocked_step_up','blocked_step_down','different_floor',
                 'gap_or_drop','step_or_drop','steep_ground','capsule_support_surface','capsule_support_solid',
                 'capsule_support_skin_blocked','capsule_support_clearance','capsule_walk_startsolid',
                 'capsule_walk_step_up','capsule_walk_sweep','capsule_walk_step_down')
        for reason in reasons:
            doc,native,binding=self.fixture(rejected={(1,2):reason})
            proposal,evidence=rejected_ordinary_edge_feedback(doc,native,binding)
            with self.subTest(reason=reason):
                self.assertEqual(evidence['withdrawn_edges'][0]['alternate_source_ids'],[1,3,2])
                self.assertEqual(evidence['withdrawn_edges'][0]['reason'],reason)
                self.assertEqual(proposal['nodes'],doc['nodes'])
                self.assertTrue(evidence['native_revalidation_required'])
        for reason in ('trace_budget','unknown','quarantined_unreachable_endpoint','rejected_endpoint',
                       'invalid_metadata','capsule_support_hazard','capsule_support_hurt',
                       'capsule_support_water','capsule_support_dynamic_ground','manual_drop_arc_rejected'):
            doc,native,binding=self.fixture(rejected={(1,2):reason})
            with self.subTest(reason=reason):self.assertIsNone(rejected_ordinary_edge_feedback(doc,native,binding))

    def guide_fixture(self):
        doc,native,_=self.fixture(rejected={(1,2):'capsule_walk_step_up'})
        doc['nodes'][2].update(generated_guide=True,editor_role=0,source_reason='generated_native_path_repair_bend',
                               nav_flags=64,crouch=True)
        native['nodes'][2].update(editor_role=1,nav_flags=64,crouch=True)
        for edge in native['edges']:
            if edge['accepted'] and 3 in(edge['from'],edge['to']):
                edge.update(crouch=True,reason='manual_native_transition_live_pending')
        return doc,native

    def test_fully_rechecked_plain_generated_bend_can_prove_ordinary_crouched_alternate(self):
        for role in (0,1):
            doc,native=self.guide_fixture();doc['nodes'][2]['editor_role']=role
            bound=self.bind(doc,native);before=copy.deepcopy((doc,native,bound))
            proposal,evidence=rejected_ordinary_edge_feedback(doc,native,bound)
            self.assertEqual((doc,native,bound),before)
            self.assertEqual(evidence['withdrawn_edges'][0]['alternate_source_ids'],[1,3,2])
            self.assertEqual(proposal['nodes'],doc['nodes'])
            self.assertEqual(proposal['nodes'][2]['nav_flags'],64)

    def test_plain_helper_exception_does_not_waive_actions_missing_floor_or_unverified_edges(self):
        cases=('unknown_reason','ladder_reason','drop_reason','door_reason','missing_reason','generated_false',
               'doc_role','native_role','flag','action_roles','action_properties','door_recipe','native_action',
               'native_alignment','unaccepted','missing_floor','unaccepted_edge','unknown_edge','edge_flags','bad_crouch')
        for case in cases:
            doc,native=self.guide_fixture();node=doc['nodes'][2];row=native['nodes'][2]
            if case.endswith('_reason'):
                value={'unknown_reason':'authored_unknown','ladder_reason':'ladder_exit_floor_live_pending',
                       'drop_reason':'automatic_repair_native_drop_landing','door_reason':'generated_door_approach_live_pending',
                       'missing_reason':''}[case];node['source_reason']=value
            elif case=='generated_false':node['generated_guide']=False
            elif case=='doc_role':node['editor_role']=3
            elif case=='native_role':row['editor_role']=17
            elif case=='flag':node.update(nav_flags=512,crouch=False);row.update(nav_flags=512,crouch=False)
            elif case=='action_roles':node['action_roles']=[dict(kind='jump',role='landing')]
            elif case=='action_properties':node['action_properties']={'tce_walk_drop':'1'}
            elif case=='door_recipe':row['door_recipe']={'role':2}
            elif case=='native_action':native['traversal_routes']=[dict(landing=3)]
            elif case=='native_alignment':native['traversal_routes']=[dict(ascent_alignment=3)]
            elif case=='unaccepted':row['accepted']=False
            elif case=='missing_floor':row['native_input']['floor_accepted']=False
            elif case=='unaccepted_edge':native['edges'][1].update(accepted=False,reason='blocked_clearance')
            elif case=='unknown_edge':native['edges'][1]['reason']='unvalidated_manual_transition'
            elif case=='edge_flags':native['edges'][1]['flags']=1
            elif case=='bad_crouch':node.update(nav_flags=0,crouch=False);row.update(nav_flags=0,crouch=False)
            bound=self.bind(doc,native)
            with self.subTest(case=case):self.assertIsNone(rejected_ordinary_edge_feedback(doc,native,bound))

    def jump_approach_fixture(self):
        doc,native,_=self.fixture(positions={1:[0.,0.,0.],2:[64.,0.,16.],3:[32.,32.,8.],4:[128.,0.,0.]},
            edges=[(1,2),(1,3),(3,2),(2,4),(4,1)],rejected={(1,2):'capsule_support_step_or_drop'})
        doc['nodes'][1].update(nav_flags=1<<27,generated_guide=True,source_reason='generated_jump_takeoff_live_pending')
        native['nodes'][1].update(nav_flags=1<<27,editor_role=1,editor_drop_target=-1)
        doc['nodes'][2].update(generated_guide=True,editor_role=0,source_reason='generated_native_path_repair_bend')
        native['nodes'][2]['editor_role']=1
        for edge in native['edges']:
            if (edge['from'],edge['to']) in ((1,3),(3,2),(2,4)):edge['reason']='manual_native_transition_live_pending'
        return doc,native

    def test_only_bad_generated_jump_approach_is_replaced_preserving_real_jump_and_all_nodes(self):
        doc,native=self.jump_approach_fixture();bound=self.bind(doc,native);before=copy.deepcopy((doc,native,bound))
        proposal,evidence=rejected_ordinary_edge_feedback(doc,native,bound)
        self.assertEqual((doc,native,bound),before)
        self.assertEqual(proposal['nodes'],doc['nodes'])
        self.assertEqual(proposal['edges'],[e for e in doc['edges'] if (e['from'],e['to'])!=(1,2)])
        row=evidence['withdrawn_edges'][0]
        self.assertEqual(row['kind'],'generated_jump_approach')
        self.assertEqual(row['alternate_source_ids'],[1,3,2])
        self.assertEqual(row['preserved_jump_edge'],[2,4]);self.assertEqual(row['jump_source_id'],2)
        self.assertTrue(evidence['native_revalidation_required'])

    def test_generated_jump_approach_never_loosens_other_actions_or_accepts_unproved_launch(self):
        cases=('not_generated','unknown_source','other_flag','extra_flag','crouch_target','target_role',
               'doc_role','drop_target','door','properties','roles','quarantine','bad_jump','jump_crouch','jump_reason',
               'two_outgoing','no_outgoing','crouch_last','last_rejected','last_flags','unknown_last','protected',
               'reverse_only','source_action','use_jump_as_walk')
        for case in cases:
            doc,native=self.jump_approach_fixture();node,row=doc['nodes'][1],native['nodes'][1]
            if case=='not_generated':node['generated_guide']=False
            elif case=='unknown_source':node['source_reason']='authored_jump'
            elif case in('other_flag','extra_flag'):
                flags=512 if case=='other_flag' else (1<<27)|512;node['nav_flags']=row['nav_flags']=flags
            elif case=='crouch_target':node['crouch']=row['crouch']=True
            elif case=='target_role':row['editor_role']=9
            elif case=='doc_role':node['editor_role']=9
            elif case=='drop_target':row['editor_drop_target']=4
            elif case=='door':row['door_recipe']={'role':2}
            elif case=='properties':node['action_properties']={'tce_stand_jump':'1'}
            elif case=='roles':node['action_roles']=[dict(kind='standing_jump',role='launch')]
            elif case=='quarantine':row.update(accepted=False,spawn_reachable=False)
            elif case=='bad_jump':native['edges'][3].update(accepted=False,reason='manual_jump_arc_rejected')
            elif case=='jump_crouch':native['edges'][3]['crouch']=True
            elif case=='jump_reason':native['edges'][3]['reason']='external_native_validated_walk'
            elif case=='two_outgoing':
                doc['edges'].append({'from':2,'to':3});native['edges'].append(dict(**doc['edges'][-1],accepted=True,reason='manual_native_transition_live_pending',crouch=False))
            elif case=='no_outgoing':doc['edges'].pop(3);native['edges'].pop(3)
            elif case=='crouch_last':
                doc['nodes'][2].update(nav_flags=64,crouch=True);native['nodes'][2].update(nav_flags=64,crouch=True)
                native['edges'][2]['crouch']=True
            elif case=='last_rejected':native['edges'][2].update(accepted=False,reason='blocked_clearance')
            elif case=='last_flags':native['edges'][2]['flags']=1
            elif case=='unknown_last':native['edges'][2]['reason']='unproved_manual_transition'
            elif case=='protected':doc['base']['protected_action_edges']=[[1,2]]
            elif case=='reverse_only':
                doc['edges'][2]={'from':2,'to':3};native['edges'][2].update(**doc['edges'][2])
            elif case=='source_action':doc['nodes'][0]['action_properties']={'tce_walk_drop':'1'}
            elif case=='use_jump_as_walk':
                # The only available detour would use the JUMP backwards;
                # neither its action nor another launch becomes a walk edge.
                doc['edges'][1]={'from':1,'to':4};native['edges'][1].update(**doc['edges'][1])
                doc['edges'][2]={'from':4,'to':3};native['edges'][2].update(**doc['edges'][2])
            bound=self.bind(doc,native)
            with self.subTest(case=case):self.assertIsNone(rejected_ordinary_edge_feedback(doc,native,bound))

    def test_jump_approach_can_start_crouched_but_ends_standing_and_keeps_ordinary_distance_bounds(self):
        doc,native=self.jump_approach_fixture()
        doc['nodes'][0].update(nav_flags=64,crouch=True);native['nodes'][0].update(nav_flags=64,crouch=True)
        native['edges'][1]['crouch']=True
        self.assertIsNotNone(rejected_ordinary_edge_feedback(doc,native,self.bind(doc,native)))
        doc['nodes'][2]['position'][1]=200
        native['nodes'][2]['raw_position'][1]=native['nodes'][2]['position'][1]=200
        self.assertIsNone(rejected_ordinary_edge_feedback(doc,native,self.bind(doc,native)))
        for hops in(12,13):
            chain=[1,*range(10,10+hops-1),2]
            positions={1:[0.,0.,0.],2:[64.,0.,0.],4:[128.,0.,0.]}
            positions.update({i:[64.*step/hops,16.,0.] for step,i in enumerate(chain[1:-1],1)})
            doc,native,_=self.fixture(positions=positions,edges=[(1,2),*zip(chain,chain[1:]),(2,4),(4,1)],
                                      rejected={(1,2):'capsule_support_step_or_drop'})
            node=next(n for n in doc['nodes']if n['id']==2);row=next(n for n in native['nodes']if n['id']==2)
            node.update(nav_flags=1<<27,generated_guide=True,source_reason='generated_jump_takeoff_live_pending')
            row.update(nav_flags=1<<27,editor_role=1,editor_drop_target=-1)
            for edge in native['edges']:
                if edge['accepted']:edge['reason']='manual_native_transition_live_pending'
            result=rejected_ordinary_edge_feedback(doc,native,self.bind(doc,native))
            with self.subTest(hops=hops):self.assertEqual(result is not None,hops==12)

    def test_native_allocated_uids_are_never_adopted_into_unbound_nodes(self):
        doc, native, _ = self.fixture()
        for node, row in zip(doc['nodes'], native['nodes']):
            node['way_guid'] = 0; row['import_uid'] = 0; row['way_guid'] = row['id']
        bound = self.bind(doc, native); proposal, receipt = rejected_ordinary_edge_feedback(doc, native, bound)
        self.assertEqual(proposal['nodes'], doc['nodes'])
        self.assertTrue(all(node['way_guid'] == 0 for node in proposal['nodes']))

    def test_no_directed_alternate_rejected_alternates_and_other_removals_cannot_support_each_other(self):
        cases = (
            ([(1, 2), (2, 1), (2, 3), (3, 1)], {(1, 2): 'capsule_support_gap_or_drop'}),
            ([(1, 2), (1, 3), (3, 2), (2, 1)], {(1, 2): 'capsule_support_gap_or_drop', (3, 2): 'blocked_sweep'}),
            ([(1, 2), (1, 3), (3, 2), (2, 1)], {(1, 2): 'capsule_support_gap_or_drop', (1, 3): 'capsule_support_gap_or_drop'}),
        )
        for edges, rejected in cases:
            doc, native, bound = self.fixture(edges=edges, rejected=rejected)
            self.assertIsNone(rejected_ordinary_edge_feedback(doc, native, bound))

    def test_alternate_has_twelve_hop_and_physical_length_bounds(self):
        for hops, accepted in ((12, True), (13, False)):
            positions = {i+1: [64.*i/hops, 0., 0.] for i in range(hops+1)}
            end = hops+1; edges = [(1, end), *[(i, i+1) for i in range(1, end)], (end, 1)]
            doc, native, bound = self.fixture(positions, edges, {(1, end): 'capsule_support_gap_or_drop'})
            result = rejected_ordinary_edge_feedback(doc, native, bound)
            self.assertEqual(result is not None, accepted)
            if result: self.assertEqual(result[1]['withdrawn_edges'][0]['hops'], 12)
        for y, accepted in ((286., True), (287., False)):
            doc, native, bound = self.fixture(positions={1: [0., 0., 0.], 2: [64., 0., 0.], 3: [32., y, 0.]})
            self.assertEqual(rejected_ordinary_edge_feedback(doc, native, bound) is not None, accepted)

    def test_long_ordinary_alternate_has_absolute_2048_limit_and_never_uses_action(self):
        for y, accepted in ((991.,True),(992.,False)):
            doc,native,bound=self.fixture(positions={1:[0.,0.,0.],2:[512.,0.,0.],3:[256.,y,0.]})
            result=rejected_ordinary_edge_feedback(doc,native,bound)
            with self.subTest(y=y):
                self.assertEqual(result is not None,accepted)
                if result:self.assertEqual(result[1]['withdrawn_edges'][0]['maximum_length_units'],2048.)
        doc,native,_=self.fixture(positions={1:[0.,0.,0.],2:[512.,0.,0.],3:[256.,991.,0.]})
        doc['nodes'][2]['nav_flags']=native['nodes'][2]['nav_flags']=1<<9
        self.assertIsNone(rejected_ordinary_edge_feedback(doc,native,self.bind(doc,native)))

    def test_actual_village_six_and_seven_hop_ordinary_detours_keep_all_points_and_only_remove_bad_chords(self):
        # Literal accepted positions/postures from Village revision12,
        # native report d8edc29f7bbf...; this checks withdrawal policy,
        # not a new collision run.
        positions={588:[1152.,-1330.215,69.9124],566:[1024.,-1330.215,89.0902],
                   556:[960.,-1219.3638,84.37],567:[1024.,-1108.5126,58.6726],
                   589:[1152.,-1108.5126,42.9377],612:[1280.,-1108.5126,32.8212],
                   1693:[1180.377,-1198.1836,51.7332],611:[1280.,-1330.215,72.2517]}
        chain=[611,588,566,556,567,589,612,1693]
        rejected={(588,1693):'capsule_support_gap_or_drop',(611,1693):'capsule_support_gap_or_drop',
                  (1693,588):'capsule_support_gap_or_drop',(1693,611):'capsule_support_gap_or_drop'}
        walks=list(zip(chain,chain[1:]));edges=walks+[(b,a)for a,b in walks]+list(rejected)
        doc,native,_=self.fixture(positions,edges,rejected)
        for node,row in zip(doc['nodes'],native['nodes']):
            flags=0 if node['id'] in (589,611) else 64
            node.update(nav_flags=flags,crouch=bool(flags));row.update(nav_flags=flags,crouch=bool(flags))
        for edge in native['edges']:
            if not edge['accepted']:edge['crouch']=True
        bound=self.bind(doc,native);before=copy.deepcopy((doc,native,bound))
        proposal,evidence=rejected_ordinary_edge_feedback(doc,native,bound)
        self.assertEqual((doc,native,bound),before);self.assertEqual(proposal['nodes'],doc['nodes'])
        self.assertEqual({tuple(r['edge'])for r in evidence['withdrawn_edges']},set(rejected))
        self.assertEqual({(e['from'],e['to'])for e in proposal['edges']},set(walks)|{(b,a)for a,b in walks})
        for row in evidence['withdrawn_edges']:
            far=611 in row['edge']
            self.assertEqual(row['hops'],7 if far else 6)
            self.assertAlmostEqual(row['length_units'],908.81725401995 if far else 780.7958795370537)
            self.assertEqual(row['kind'],'ordinary_walk')
        self.assertTrue(evidence['native_revalidation_required'])

    def test_unrelated_failed_action_is_retained_and_never_marked_valid(self):
        doc, native, _ = self.fixture(edges=[(1, 2), (1, 3), (3, 2), (2, 1), (2, 3)],
            rejected={(1, 2): 'capsule_support_gap_or_drop', (2, 3): 'manual_drop_arc_rejected'})
        bound = self.bind(doc, native); proposal, receipt = rejected_ordinary_edge_feedback(doc, native, bound)
        self.assertIn({'from': 2, 'to': 3}, proposal['edges'])
        self.assertEqual(len(receipt['withdrawn_edges']), 1)
        self.assertEqual(receipt['remaining_rejected_edges'], [dict(edge=[2, 3], reason='manual_drop_arc_rejected')])
        self.assertTrue(receipt['native_revalidation_required']); self.assertFalse(native['generation_complete'])

    def quarantined_fixture(self):
        doc,native,_=self.fixture(positions={1:[0.,0.,0.],2:[64.,0.,0.],3:[32.,32.,0.],4:[500.,0.,0.],5:[550.,0.,0.]},
            edges=[(1,2),(1,3),(3,2),(2,1),(4,5),(5,4),(2,4)],
            rejected={(1,2):'capsule_support_step_or_drop',(4,5):'quarantined_unreachable_endpoint',
                      (5,4):'quarantined_unreachable_endpoint',(2,4):'quarantined_unreachable_endpoint'})
        for row in native['nodes'][3:]:
            row.update(accepted=False,spawn_reachable=False,reason='quarantined_no_validated_route_from_spawn')
            row['native_input'].update(status='missing_spawn_route',initial_incoming=2,initial_outgoing=2,
                                       final_incoming=0,final_outgoing=0,initial_rejected_incident_links=0)
        return doc,native,self.bind(doc,native)

    def test_real_schema_quarantine_keeps_unrelated_island_and_input_reject_accounting(self):
        doc,native,bound=self.quarantined_fixture();before=copy.deepcopy((doc,native,bound))
        proposal,receipt=rejected_ordinary_edge_feedback(doc,native,bound)
        self.assertEqual((doc,native,bound),before);self.assertEqual(proposal['nodes'],doc['nodes'])
        self.assertEqual(receipt['preserved_quarantined_source_ids'],[4,5])
        self.assertEqual(receipt['withdrawn_edges'][0]['alternate_source_ids'],[1,3,2])
        self.assertEqual(len(receipt['remaining_rejected_edges']),3)
        self.assertTrue(all(row['reason']=='quarantined_unreachable_endpoint'for row in receipt['remaining_rejected_edges']))
        self.assertEqual(native['native_input_coverage']['initial_rejected_links'],1)
        self.assertEqual(native['native_input_coverage']['initial_accepted_links'],6)
        self.assertFalse(native['generation_complete']);self.assertTrue(receipt['native_revalidation_required'])

    def test_quarantine_never_licenses_unknown_nodes_bad_counts_or_unproved_alternate(self):
        for case in ('reason','floor','reachable','status','top_count','missing_count','export_count',
                     'spawn_count','initial_reject_count','rejection_reasons','fake_endpoint','accepted_endpoint','quarantined_alternate'):
            doc,native,_=self.quarantined_fixture();row=native['nodes'][3]
            if case=='reason':row['reason']='blocked_clearance'
            elif case=='floor':row['native_input']['floor_accepted']=False
            elif case=='reachable':row['spawn_reachable']=True
            elif case=='status':row['native_input']['status']='not_evaluated'
            elif case=='fake_endpoint':native['edges'][3].update(accepted=False,reason='quarantined_unreachable_endpoint')
            elif case=='accepted_endpoint':native['edges'][4].update(accepted=True,reason='accepted')
            elif case=='quarantined_alternate':
                native['nodes'][2].update(accepted=False,spawn_reachable=False,reason='quarantined_no_validated_route_from_spawn')
                native['nodes'][2]['native_input']['status']='missing_spawn_route'
                for edge in native['edges']:
                    if 3 in (edge['from'],edge['to']):edge.update(accepted=False,reason='quarantined_unreachable_endpoint')
            bound=self.bind(doc,native)
            coverage=native['native_input_coverage']
            if case=='top_count':native['quarantined_unreachable_nodes']+=1
            elif case=='missing_count':coverage['connectivity_missing_input_nodes']+=1
            elif case=='export_count':coverage['exported_input_nodes']-=1
            elif case=='spawn_count':coverage['spawn_reachable_input_nodes']-=1
            elif case=='initial_reject_count':coverage['initial_rejected_links']+=1
            elif case=='rejection_reasons':coverage['link_rejections'][0]['count']+=1
            bound['native_report_bytes']=json.dumps(native).encode()
            bound['report_status']['native_report']=bound['attempt']['native_report']=digest(bound['native_report_bytes'])
            with self.subTest(case=case):self.assertIsNone(rejected_ordinary_edge_feedback(doc,native,bound))

    def ladder_fixture(self):
        doc,native,_=self.fixture(positions={1:[0.,0.,200.],2:[0.,44.,200.],3:[0.,-16.,0.],
                                            4:[0.,0.,0.],5:[0.,0.,184.]},
            edges=[(1,2),(2,1),(1,3),(3,4),(4,5),(5,2)],rejected={(1,2):'capsule_support_gap_or_drop'})
        kinds={1:('ladder_top_clearance_live_pending',33),2:('ladder_exit_floor_live_pending',1),
               3:('ladder_inside_guidance_not_standable_live_pending',17),4:('ladder_base_contact_live_pending',1),
               5:('ladder_ascent_release_contact_live_pending',3)}
        for node,row in zip(doc['nodes'],native['nodes']):
            reason,role=kinds[node['id']];node.update(nav_flags=512,generated_guide=True,source_reason=reason)
            row.update(nav_flags=512,editor_role=role)
        for edge in native['edges']:
            if edge['accepted']:edge['reason']='manual_native_transition_live_pending'
        return doc,native,self.bind(doc,native)

    def test_exact_generated_ladder_reverse_has_proved_descent_and_separate_ascent_alternate(self):
        doc,native,bound=self.ladder_fixture();before=copy.deepcopy((doc,native,bound))
        proposal,receipt=rejected_ordinary_edge_feedback(doc,native,bound)
        self.assertEqual((doc,native,bound),before);self.assertEqual(proposal['nodes'],doc['nodes'])
        self.assertEqual(receipt['model'],'native_rejected_edge_alternate_v2')
        self.assertEqual(receipt['withdrawn_edges'],[dict(edge=[1,2],kind='generated_ladder_reverse',
            reason='capsule_support_gap_or_drop',alternate_source_ids=[1,3,4,5,2],hops=4,
            length_units=receipt['withdrawn_edges'][0]['length_units'],maximum_length_units=656.,
            interior_source_id=3,shaft_height_units=200.)])
        self.assertEqual(proposal['edges'],[e for e in doc['edges']if(e['from'],e['to'])!=(1,2)])
        self.assertTrue(receipt['directed_reachability_preserved']);self.assertTrue(receipt['native_revalidation_required'])

    def test_ladder_exception_does_not_withdraw_manual_actions_or_unproved_returns(self):
        for case in ('manual_source','manual_target','source_reason','target_reason','source_role','target_role',
                     'interior_role','interior_reason','mixed_flags','action_property','rejection_reason',
                     'extra_incoming','extra_outgoing','blocked_interior','blocked_reverse','unproved_alternate',
                     'rejected_alternate','wrong_direction','protected','long_alternate','low_height','quarantined_alternate'):
            doc,native,_=self.ladder_fixture()
            if case=='manual_source':doc['nodes'][0]['generated_guide']=False
            elif case=='manual_target':doc['nodes'][1]['generated_guide']=False
            elif case=='source_reason':doc['nodes'][0]['source_reason']='manual_climb'
            elif case=='target_reason':doc['nodes'][1]['source_reason']='manual_climb'
            elif case=='source_role':native['nodes'][0]['editor_role']=3
            elif case=='target_role':native['nodes'][1]['editor_role']=33
            elif case=='interior_role':native['nodes'][2]['editor_role']=1
            elif case=='interior_reason':doc['nodes'][2]['source_reason']='ladder_base_contact_live_pending'
            elif case=='mixed_flags':doc['nodes'][0]['nav_flags']=native['nodes'][0]['nav_flags']=513
            elif case=='action_property':doc['nodes'][0]['action_properties']={'pause':'10'}
            elif case=='rejection_reason':native['edges'][0]['reason']='capsule_support_step_or_drop'
            elif case in('extra_incoming','extra_outgoing'):
                a,b=(4,1)if case=='extra_incoming'else(1,4)
                doc['edges'].append(dict(**{'from':a,'to':b}))
                native['edges'].append(dict(**{'from':a,'to':b},accepted=True,reason='manual_native_transition_live_pending'))
            elif case=='blocked_interior':native['edges'][2].update(accepted=False,reason='blocked_sweep')
            elif case=='blocked_reverse':native['edges'][1].update(accepted=False,reason='blocked_sweep')
            elif case=='unproved_alternate':native['edges'][3]['reason']='unchecked_manual_transition'
            elif case=='rejected_alternate':native['edges'][3].update(accepted=False,reason='blocked_sweep')
            elif case=='wrong_direction':
                doc['edges'][3]={'from':4,'to':3};native['edges'][3].update({'from':4,'to':3})
            elif case=='protected':doc['base']['protected_action_edges']=[[1,2]]
            elif case in('long_alternate','low_height'):
                index,pos=(3,[0.,2000.,0.])if case=='long_alternate'else(2,[0.,-16.,200.])
                doc['nodes'][index]['position']=pos
                native['nodes'][index]['raw_position']=native['nodes'][index]['position']=pos[:2]+[pos[2]+30]
            elif case=='quarantined_alternate':
                native['nodes'][3].update(accepted=False,spawn_reachable=False,reason='quarantined_no_validated_route_from_spawn')
                native['nodes'][3]['native_input']['status']='missing_spawn_route'
                for edge in native['edges']:
                    if 4 in(edge['from'],edge['to']):edge.update(accepted=False,reason='quarantined_unreachable_endpoint')
            bound=self.bind(doc,native)
            with self.subTest(case=case):self.assertIsNone(rejected_ordinary_edge_feedback(doc,native,bound))

    def test_ladder_alternate_is_bounded_to_twenty_four_hops_and_two_shaft_lengths(self):
        for hops,accepted in ((24,True),(25,False)):
            doc,native,_=self.ladder_fixture();extra=hops-4;chain=[3]+list(range(6,6+extra))+[4]
            doc['edges']=[e for e in doc['edges']if(e['from'],e['to'])!=(3,4)]
            native['edges']=[e for e in native['edges']if(e['from'],e['to'])!=(3,4)]
            for j,identity in enumerate(chain[1:-1],1):
                node=copy.deepcopy(doc['nodes'][3]);node.update(id=identity,way_guid=identity,position=[0.,-16+16*j/(extra+1),0.])
                row=copy.deepcopy(native['nodes'][3]);row.update(id=identity,import_uid=identity,
                    raw_position=node['position'][:2]+[30.],position=node['position'][:2]+[30.])
                doc['nodes'].append(node);native['nodes'].append(row)
            for a,b in zip(chain,chain[1:]):
                doc['edges'].append(dict(**{'from':a,'to':b}))
                native['edges'].append(dict(**{'from':a,'to':b},accepted=True,reason='manual_native_transition_live_pending'))
            bound=self.bind(doc,native);result=rejected_ordinary_edge_feedback(doc,native,bound)
            with self.subTest(hops=hops):self.assertEqual(result is not None,accepted)
            if result:self.assertEqual(result[1]['withdrawn_edges'][0]['hops'],24)


if __name__ == '__main__':
    unittest.main()
