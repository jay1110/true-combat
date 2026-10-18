"""Bounded native-probe repair proposals; no RCON or project mutation."""
import copy
import math
import unittest
from repair import propose_descents, parse_native_probe, terminal_action_components


def fixture():
    def node(identity,p,flags=0):
        return dict(id=identity,position=p,radius=35,source_radius=35,nav_flags=flags,crouch=False,
                    spawn=identity==3,topology_protected=False,locked=False,generated_guide=bool(flags),
                    source_reason='manual_added',editor_role=0,action_roles=[],action_properties={})
    doc=dict(schema=1,mode='manual_draft',map='obj_test',revision=0,saved_revision=0,created=0,
             base=dict(environment={'bsp':'fixture'}),next_id=121,undo=[],redo=[],export=None,
             nodes=[node(3,[0,0,102.25]),node(99,[64,-110,.25]),node(120,[256,0,.25],(1<<27)|(1<<55))],
             edges=[{'from':3,'to':99},{'from':120,'to':99}])
    native=dict(bbox=dict(origin_above_feet=30),traversal_physics=dict(gravity=1000),
                nodes=[dict(id=n['id'],accepted=True) for n in doc['nodes']],
                edges=[dict(**{'from':3,'to':99},accepted=False,reason='blocked_clearance')])
    return doc,native


def terminal_fixture():
    doc,native=fixture()
    upper=copy.deepcopy(doc['nodes'][0]);upper.update(id=4,position=[0,50,102.25],spawn=False)
    doc['nodes'][0].update(spawn=False,source_reason='external_native_validated_floor')
    doc['nodes'].append(upper)
    doc['nodes'].sort(key=lambda n:n['id'])
    doc['edges']=[{'from':99,'to':120},{'from':120,'to':99},{'from':120,'to':3},
                  {'from':3,'to':4},{'from':4,'to':3}]
    native['nodes']=[dict(id=n['id'],accepted=True,spawn_reachable=True) for n in doc['nodes']]
    native['edges']=[dict(**edge,accepted=True) for edge in doc['edges']]
    native['native_input_coverage']=dict(ordinary_without_exit=[])
    native['traversal_routes']=[dict(kind='free_climb',exported=True,launch=120,landing=3)]
    return doc,native


def quarantined_floor_fixture(identities=(3,)):
    doc,native=fixture()
    native['edges'][0]['reason']='capsule_support_gap_or_drop'
    native['native_input_coverage']=dict(capacity=dict(input_validation_complete=True,input_truncated=False))
    for row,node in zip(native['nodes'],doc['nodes']):
        row.update(nav_flags=node['nav_flags'],crouch=node['crouch'],editor_role=node['editor_role'],
                   spawn_reachable=True,native_input=dict(floor_accepted=True,status='spawn_reachable'))
        if row['id'] in identities:
            row.update(accepted=False,spawn_reachable=False,reason='quarantined_no_validated_route_from_spawn')
            row['native_input']['status']='missing_spawn_route'
    return doc,native


class NativeDescentRepairTests(unittest.TestCase):
    def probe(self,a,b):
        if a[2]>b[2]+18.25:
            accepted=a[0]==b[0] and b[1]<=-154
            return dict(stand='gap_or_drop',drop=dict(accepted=accepted,delta=23.2499,max_drop=148.4375))
        if b[2]>a[2]+63:
            return dict(stand='blocked_clearance',free_climb=dict(accepted=True,launch=[0,-64,.25],
                                                                duration=.96,stamina_used=870,max_ledge=126.25))
        return dict(stand='accepted')

    def test_native_bend_preserves_original_ids_flags_and_downward_direction_with_distinct_climb(self):
        doc,native=fixture();before=copy.deepcopy(doc)
        result,evidence=propose_descents(doc,native,self.probe)
        self.assertEqual(doc,before)
        self.assertEqual(evidence['added_nodes'],2)
        by_id={n['id']:n for n in result['nodes']}
        self.assertTrue({3,99,120}<=by_id.keys());self.assertNotIn(100,by_id)
        for original in before['nodes']:
            self.assertEqual(by_id[original['id']]['position'],original['position'])
            self.assertEqual(by_id[original['id']]['nav_flags'],original['nav_flags'])
        self.assertEqual(by_id[120],before['nodes'][2])
        chain=evidence['routes'][0]['chain'];return_chain=evidence['routes'][0]['return_chain']
        self.assertEqual(chain,[3,121,99]);self.assertEqual(return_chain,[99,121,122,3])
        edges={(e['from'],e['to']) for e in result['edges']}
        self.assertNotIn((3,99),edges);self.assertNotIn((121,3),edges)
        self.assertTrue({(3,121),(121,99),(99,121),(121,122),(122,121),(122,3)}<=edges)
        self.assertEqual(by_id[3]['action_roles'],[dict(kind='walk_drop',role='launch',landing_node=121)])
        self.assertEqual(by_id[122]['action_roles'],[dict(kind='free_climb',role='launch',landing_node=3)])
        self.assertEqual(by_id[122]['nav_flags'],512)
        self.assertTrue(evidence['native_revalidation_required']);self.assertEqual(evidence['deleted_nodes'],0)
        self.assertLessEqual(len(evidence['queries']),32)

    def test_uncertain_damaging_or_exhausted_native_probes_leave_authored_edge_intact(self):
        for mode in ('rejected','damage','budget'):
            doc,native=fixture()
            def probe(a,b):
                return dict(stand='gap_or_drop',drop=dict(accepted=mode=='damage',delta=38.01,max_drop=148.4375))
            result,evidence=propose_descents(doc,native,probe,max_probes=1 if mode=='budget' else 8)
            self.assertEqual(result['nodes'],doc['nodes']);self.assertEqual(result['edges'],doc['edges'])
            self.assertEqual(evidence['added_nodes'],0)
            self.assertLessEqual(len(evidence['queries']),1 if mode=='budget' else 8)

    def test_support_rejection_is_only_a_probe_trigger_never_an_automatic_drop_approval(self):
        for reason in ('capsule_support_gap_or_drop','capsule_support_step_or_drop'):
            doc,native=fixture();native['edges'][0]['reason']=reason
            calls=[]
            def rejected(a,b):
                calls.append((a,b))
                return dict(stand=reason,drop=dict(accepted=False,delta=0,max_drop=148.4375))
            result,evidence=propose_descents(doc,native,rejected,max_probes=8)
            with self.subTest(reason=reason):
                self.assertTrue(calls)
                self.assertEqual(result['nodes'],doc['nodes'])
                self.assertEqual(result['edges'],doc['edges'])
                self.assertEqual(evidence['added_nodes'],0)
                self.assertTrue(evidence['native_revalidation_required'])

    def test_checked_quarantined_floor_requests_fresh_drop_and_distinct_return_proof(self):
        for identities in ((3,),(99,),(3,99)):
            doc,native=quarantined_floor_fixture(identities)
            before_doc,before_native=copy.deepcopy(doc),copy.deepcopy(native)
            result,evidence=propose_descents(doc,native,self.probe)
            with self.subTest(quarantined=identities):
                self.assertEqual(doc,before_doc);self.assertEqual(native,before_native)
                self.assertEqual(evidence['quarantined_floor_probe_source_ids'],list(identities))
                self.assertEqual(evidence['added_nodes'],2)
                route=next(r for r in evidence['routes'] if r.get('chain'))
                self.assertEqual(route['chain'],[3,121,99])
                self.assertEqual(route['return_chain'],[99,121,122,3])
                self.assertTrue(route['drop_proof']['accepted'])
                self.assertTrue(evidence['native_revalidation_required'])
                self.assertEqual(evidence['replaced_rejected_edges'],[[3,99]])
                self.assertFalse(any('accepted' in n or 'spawn_reachable' in n for n in result['nodes']))
                for old in doc['nodes']:
                    new=next(n for n in result['nodes'] if n['id']==old['id'])
                    self.assertEqual(new['position'],old['position']);self.assertEqual(new['nav_flags'],old['nav_flags'])

    def test_checked_quarantined_floor_native_rejection_preserves_entire_graph(self):
        for reason in ('capsule_support_gap_or_drop','capsule_support_step_or_drop'):
            doc,native=quarantined_floor_fixture();native['edges'][0]['reason']=reason
            before_native=copy.deepcopy(native);calls=[]
            def rejected(a,b):
                calls.append((a,b))
                return dict(stand=reason,drop=dict(accepted=False,delta=0,max_drop=148.4375))
            result,evidence=propose_descents(doc,native,rejected,max_probes=8)
            with self.subTest(reason=reason):
                self.assertTrue(calls);self.assertLessEqual(len(calls),8)
                self.assertEqual(result,doc);self.assertEqual(native,before_native)
                self.assertEqual(evidence['quarantined_floor_probe_source_ids'],[3])
                self.assertEqual(evidence['added_nodes'],0);self.assertEqual(evidence['replaced_rejected_edges'],[])

    def test_quarantine_exception_excludes_unchecked_floors_actions_crouch_and_other_failures(self):
        for mode in ('floor_rejected','wrong_quarantine','wrong_status','reachable','unfinished','truncated',
                     'native_flags','native_crouch','native_action_role','native_drop','native_door','route_action',
                     'crouch','action_roles','action_properties','door_recipe','target_action',
                     'blocked_clearance','quarantined_unreachable_endpoint','trace_budget','unknown',
                     'no_authored_edge','no_gravity','tiny_drop','harmful_drop'):
            doc,native=quarantined_floor_fixture();node=doc['nodes'][0];row=native['nodes'][0]
            if mode=='floor_rejected':row['native_input']['floor_accepted']=False
            elif mode=='wrong_quarantine':row['reason']='no_floor_near_seed'
            elif mode=='wrong_status':row['native_input']['status']='geometry_rejected'
            elif mode=='reachable':row['spawn_reachable']=True
            elif mode=='unfinished':native['native_input_coverage']['capacity']['input_validation_complete']=False
            elif mode=='truncated':native['native_input_coverage']['capacity']['input_truncated']=True
            elif mode=='native_flags':row['nav_flags']=1<<27
            elif mode=='native_crouch':row['crouch']=True
            elif mode=='native_action_role':row['editor_role']=8
            elif mode=='native_drop':row['editor_drop_target']=99
            elif mode=='native_door':row['door_recipe']={'mode':3}
            elif mode=='route_action':native['traversal_routes']=[dict(kind='walk_drop',launch=3,landing=99)]
            elif mode=='crouch':node.update(nav_flags=64,crouch=True)
            elif mode=='action_roles':node['action_roles']=[dict(kind='walk_drop',role='launch',landing_node=99)]
            elif mode=='action_properties':node['action_properties']={'tce_walk_drop':'1'}
            elif mode=='door_recipe':node['door_recipe']={'mode':3}
            elif mode=='target_action':doc['nodes'][1]['action_properties']={'tce_free_climb':'1'}
            elif mode=='no_authored_edge':doc['edges']=doc['edges'][1:]
            elif mode=='no_gravity':native['traversal_physics']={}
            elif mode=='tiny_drop':node['position'][2]=18.5
            elif mode=='harmful_drop':node['position'][2]=1000
            else:native['edges'][0]['reason']=mode
            before=copy.deepcopy(doc)
            with self.subTest(mode=mode):
                result,evidence=propose_descents(doc,native,lambda a,b:self.fail('Unsafe request reached native probe'))
                self.assertEqual(result,before);self.assertEqual(doc,before)
                self.assertEqual(evidence['queries'],[])
                self.assertEqual(evidence['quarantined_floor_probe_source_ids'],[])

    def test_unproved_mounting_retreat_does_not_invent_climb_recipe(self):
        doc,native=fixture()
        def probe(a,b):
            if a==[0,-64,.25]:return dict(stand='blocked_sweep')
            return self.probe(a,b)
        result,evidence=propose_descents(doc,native,probe)
        self.assertEqual(evidence['added_nodes'],1)
        self.assertIsNone(evidence['routes'][0]['return_chain'])
        self.assertFalse(any(n['nav_flags']&512 for n in result['nodes']))

    def test_fragmented_actual_engine_drop_header_is_unique_and_query_bound(self):
        start=[-3840,-221.7025,102.25];end=[-3840,-376,.25]
        reply=('navgen probe -1->-1 feet=(-3840.00 -221.70 102.25)->(-3840.00 -376.00 0.25) '
               'stand=gap_or_drop crouch=gap_or_drop traces=74 quarantined=0/0 (diagnostic only; graph unchanged)\n'
               'navgen drop contact: velocity=(0.0000 -204.0000 -409.1841'
               'navgen probe drop -1->-1 accepted=1 reason=accepted crash_limit=38 max_drop=148.4375 '
               'delta=23.2499 landing=(-3840.0000 -335.5607 30.1250) (readonly samples)\n')
        proof=parse_native_probe(reply,start,end,30)
        self.assertTrue(proof['drop']['accepted']);self.assertEqual(proof['drop']['delta'],23.2499)
        self.assertEqual(proof['drop']['touchdown'],[-3840,-335.5607,.125])
        for altered in (reply+reply,reply.replace('accepted=1','accepted=0')+reply,
                        reply.replace('-376.00','-375.00')):
            with self.assertRaises(RuntimeError):parse_native_probe(altered,start,end,30)

    def test_isolated_real_generated_spawn_seed_gets_native_probed_route_without_removing_an_edge(self):
        doc,native=fixture()
        doc['nodes'][0]['source_reason']='map_entity_spawn_seed'
        doc['edges']=[{'from':99,'to':120},{'from':120,'to':99}]
        native['edges']=[dict(**edge,accepted=True) for edge in doc['edges']]
        result,evidence=propose_descents(doc,native,self.probe)
        self.assertEqual(evidence['connected_isolated_source_ids'],[3])
        self.assertEqual(evidence['replaced_rejected_edges'],[])
        self.assertEqual(evidence['deleted_nodes'],0)
        self.assertTrue(all(edge in result['edges'] for edge in doc['edges']))
        self.assertTrue(evidence['routes'][0]['return_chain'])
        self.assertEqual(result['nodes'][0]['position'],doc['nodes'][0]['position'])
        self.assertEqual(result['nodes'][0]['nav_flags'],doc['nodes'][0]['nav_flags'])

    def test_proved_free_climb_landing_sink_gets_a_checked_exit_and_keeps_original_ingress(self):
        doc,native=fixture()
        doc['nodes'][0]['spawn']=False
        doc['nodes'][0]['source_reason']='external_native_validated_floor'
        doc['edges']=[{'from':99,'to':120},{'from':120,'to':99},{'from':120,'to':3}]
        native['edges']=[dict(**edge,accepted=True) for edge in doc['edges']]
        native['native_input_coverage']=dict(ordinary_without_exit=[3])
        native['traversal_routes']=[dict(kind='free_climb',exported=True,launch=120,landing=3)]
        result,evidence=propose_descents(doc,native,self.probe)
        self.assertEqual(evidence['connected_isolated_source_ids'],[3])
        self.assertTrue(all(edge in result['edges'] for edge in doc['edges']))
        self.assertEqual(evidence['replaced_rejected_edges'],[])
        self.assertTrue(evidence['routes'][0]['proved_traversal_landing_without_exit'])
        self.assertFalse(evidence['routes'][0]['isolated_generated_spawn_seed'])
        self.assertEqual(next(n for n in result['nodes'] if n['id']==3)['position'],doc['nodes'][0]['position'])
        self.assertTrue(evidence['native_revalidation_required'])

    def test_terminal_two_point_ledge_cycle_gets_proved_exit_without_losing_ingress_or_walks(self):
        doc,native=terminal_fixture();original=copy.deepcopy(doc)
        groups=terminal_action_components(native)
        self.assertEqual(groups[3]['members'],[3,4])
        self.assertEqual(groups[3]['return_targets'],{99,120})
        result,evidence=propose_descents(doc,native,self.probe)
        self.assertEqual(doc,original)
        self.assertEqual(evidence['connected_terminal_components'],[[3,4]])
        self.assertEqual(evidence['connected_terminal_component_source_ids'],[3])
        self.assertEqual(evidence['connected_isolated_source_ids'],[])
        self.assertEqual(evidence['replaced_rejected_edges'],[])
        self.assertTrue(all(edge in result['edges'] for edge in doc['edges']))
        route=next(r for r in evidence['routes'] if r.get('chain'))
        self.assertEqual(route['proved_traversal_terminal_component'],[3,4])
        self.assertEqual(route['chain'][-1],99)
        by_id={n['id']:n for n in result['nodes']}
        self.assertEqual(by_id[4],next(n for n in original['nodes'] if n['id']==4))
        self.assertEqual({3,4,99,120},set(by_id)&{3,4,99,120})
        self.assertEqual(evidence['deleted_nodes'],0)
        self.assertLessEqual(len(evidence['queries']),96)

    def test_terminal_cycle_prioritizes_proved_landing_then_other_ordinary_upper_node(self):
        doc,native=terminal_fixture()
        # The proved landing has the larger ID, but must be probed first.
        native['traversal_routes'][0]['landing']=4
        doc['edges'][2]['to']=4;native['edges'][2]['to']=4
        visited=[]
        def probe(a,b):
            if a[2]>b[2]+18.25:
                visited.append(tuple(a))
                return dict(stand='gap_or_drop',drop=dict(accepted=a==[0,0,102.25] and b[1]<=-154,
                                                          delta=23.2499,max_drop=148.4375))
            if a[2]==b[2]==102.25:return dict(stand='blocked_clearance')
            if b[2]>a[2]+63:return dict(stand='blocked_clearance',free_climb=dict(accepted=False))
            return dict(stand='accepted')
        result,evidence=propose_descents(doc,native,probe,max_probes=96)
        self.assertEqual(visited[0],(0,50,102.25))
        self.assertIn((0,0,102.25),visited)
        self.assertEqual(evidence['connected_terminal_component_source_ids'],[3])
        self.assertTrue(all(edge in result['edges'] for edge in doc['edges']))

    def test_terminal_cycle_is_not_authorized_by_unproved_drop_unreachable_or_internal_ascent(self):
        for mode in ('no_recipe','not_exported','drop_only','unreachable_member','unreachable_ingress',
                     'rejected_ingress','internal_ascent','not_terminal'):
            doc,native=terminal_fixture()
            route=native['traversal_routes'][0]
            if mode=='no_recipe':native['traversal_routes']=[]
            elif mode=='not_exported':route['exported']=False
            elif mode=='drop_only':route['kind']='walk_drop'
            elif mode=='unreachable_member':next(n for n in native['nodes'] if n['id']==4)['spawn_reachable']=False
            elif mode=='unreachable_ingress':next(n for n in native['nodes'] if n['id']==120)['spawn_reachable']=False
            elif mode=='rejected_ingress':native['edges'][2]['accepted']=False
            elif mode=='internal_ascent':route['launch']=4
            else:
                edge={'from':4,'to':99};doc['edges'].append(edge);native['edges'].append(dict(**edge,accepted=True))
            with self.subTest(mode=mode):
                result,evidence=propose_descents(doc,native,self.probe)
                self.assertEqual(terminal_action_components(native),{})
                self.assertEqual(result['nodes'],doc['nodes'])
                self.assertEqual({(e['from'],e['to']) for e in result['edges']},{(e['from'],e['to']) for e in doc['edges']})
                self.assertEqual(evidence['queries'],[])

    def test_terminal_exit_targets_must_already_reach_the_proved_ingress(self):
        doc,native=terminal_fixture()
        foreign=copy.deepcopy(doc['nodes'][1]);foreign.update(id=6,position=[0,-40,.25])
        foreign2=copy.deepcopy(foreign);foreign2.update(id=7,position=[0,-50,.25])
        doc['nodes'].extend((foreign,foreign2))
        doc['edges'].extend((dict(zip(('from','to'),(6,7))),dict(zip(('from','to'),(7,6)))))
        native['nodes'].extend((dict(id=6,accepted=True,spawn_reachable=True),dict(id=7,accepted=True,spawn_reachable=True)))
        native['edges']=[dict(**edge,accepted=True) for edge in doc['edges']]
        result,evidence=propose_descents(doc,native,self.probe)
        self.assertEqual(terminal_action_components(native)[3]['return_targets'],{99,120})
        self.assertTrue(evidence['connected_terminal_components'])
        self.assertEqual(next(r for r in evidence['routes'] if r.get('chain'))['chain'][-1],99)
        self.assertTrue(all(edge in result['edges'] for edge in doc['edges']))

    def test_terminal_cycle_never_bypasses_drop_damage_or_native_probe_budget(self):
        doc,native=terminal_fixture()
        def probe(a,b):
            return dict(stand='blocked_clearance',drop=dict(accepted=True,delta=38.01,max_drop=148.4375))
        result,evidence=propose_descents(doc,native,probe,max_probes=3)
        self.assertEqual(result['nodes'],doc['nodes'])
        self.assertEqual(evidence['added_nodes'],0)
        self.assertEqual(evidence['connected_terminal_components'],[])
        self.assertLessEqual(len(evidence['queries']),3)

    def test_unproved_or_non_sink_action_landing_does_not_authorize_a_proximity_exit(self):
        for mode in ('not_exported','not_listed','wrong_kind','no_accepted_ingress'):
            doc,native=fixture();doc['nodes'][0]['spawn']=False
            doc['edges']=[{'from':99,'to':120},{'from':120,'to':99},{'from':120,'to':3}]
            native['edges']=[dict(**edge,accepted=True) for edge in doc['edges']]
            native['native_input_coverage']=dict(ordinary_without_exit=[3])
            route=dict(kind='free_climb',exported=True,launch=120,landing=3)
            native['traversal_routes']=[route]
            if mode=='not_exported':route['exported']=False
            elif mode=='not_listed':native['native_input_coverage']['ordinary_without_exit']=[]
            elif mode=='wrong_kind':route['kind']='walk_drop'
            else:native['edges'][-1]['accepted']=False;native['edges'][-1]['reason']='manual_climb_rejected'
            with self.subTest(mode=mode):
                result,evidence=propose_descents(doc,native,self.probe)
                self.assertEqual(result['nodes'],doc['nodes'])
                self.assertEqual({(e['from'],e['to']) for e in result['edges']},{(e['from'],e['to']) for e in doc['edges']})
                self.assertEqual(evidence['queries'],[])

    def test_diagonal_roof_corner_fallback_has_literal_upper_and_lower_native_proofs(self):
        doc,native=fixture();doc['nodes'][0].update(spawn=False,position=[0,0,100.25])
        doc['nodes'][1]['position']=[-64,130,.25]
        doc['edges']=[{'from':99,'to':120},{'from':120,'to':99},{'from':120,'to':3}]
        native['edges']=[dict(**edge,accepted=True) for edge in doc['edges']]
        native['native_input_coverage']=dict(ordinary_without_exit=[3])
        native['traversal_routes']=[dict(kind='free_climb',exported=True,launch=120,landing=3)]
        top=[-32,64,100.25]
        landing=[round(-32-64*2/math.sqrt(13),5),round(64+64*3/math.sqrt(13),5),.25]
        original=copy.deepcopy(doc)
        def probe(a,b):
            if a[2]==b[2]==100.25:
                pairs={frozenset(((0,0,100.25),(-32,0,100.25))),
                       frozenset(((0,0,100.25),(0,64,100.25))),frozenset(((0,0,100.25),tuple(top)))}
                return dict(stand='accepted' if frozenset((tuple(a),tuple(b))) in pairs else 'blocked_clearance')
            if a[2]>b[2]+18.25:
                return dict(stand='gap_or_drop',drop=dict(accepted=a==top and b==landing,delta=23.2499,max_drop=148.4375))
            if b[2]>a[2]+63:return dict(stand='blocked_clearance',free_climb=dict(accepted=False))
            return dict(stand='accepted')
        result,evidence=propose_descents(doc,native,probe,max_probes=96)
        self.assertEqual(doc,original)
        self.assertEqual(evidence['connected_isolated_source_ids'],[3])
        route=next(r for r in evidence['routes'] if r.get('chain'))
        self.assertEqual(route['upper_approach'],top)
        self.assertLessEqual(len(evidence['queries']),96)
        self.assertTrue(all(edge in result['edges'] for edge in doc['edges']))
        self.assertIsNone(route['return_chain'])
        by_id={n['id']:n for n in result['nodes']}
        self.assertEqual(by_id[3],original['nodes'][0])
        self.assertEqual(by_id[route['chain'][2]]['position'],landing)
        self.assertTrue(evidence['native_revalidation_required'])

    def test_user_closed_action_and_rejected_floor_isolates_are_not_connected(self):
        for mode in ('user','closed','action','rejected'):
            doc,native=fixture()
            doc['nodes'][0]['source_reason']='map_entity_spawn_seed'
            doc['edges']=[{'from':99,'to':120},{'from':120,'to':99}]
            native['edges']=[dict(**edge,accepted=True) for edge in doc['edges']]
            if mode=='user':doc['nodes'][0]['source_reason']='manual_added'
            elif mode=='closed':doc['nodes'][0]['nav_flags']=1
            elif mode=='action':doc['nodes'][0]['editor_role']=128
            else:native['nodes'][0]['accepted']=False
            with self.subTest(mode=mode):
                result,evidence=propose_descents(doc,native,self.probe)
                self.assertEqual(result['nodes'],doc['nodes'])
                self.assertEqual({(e['from'],e['to']) for e in result['edges']},{(e['from'],e['to']) for e in doc['edges']})
                self.assertEqual(evidence['queries'],[])

    def test_upper_approach_has_reserved_budget_and_own_drop_recipe_without_invented_inbound(self):
        doc,native=fixture()
        doc['nodes'][0].update(position=[0,0,100.25],source_reason='map_entity_spawn_seed')
        doc['nodes'][1]['position']=[-86,-18,.25]
        other=copy.deepcopy(doc['nodes'][1]);other.update(id=101,position=[-20,92,.25])
        doc['nodes'].append(other)
        doc['edges']=[{'from':99,'to':101},{'from':101,'to':99},{'from':120,'to':99}]
        native['nodes'].append(dict(id=101,accepted=True))
        native['edges']=[dict(**edge,accepted=True) for edge in doc['edges']]
        original=copy.deepcopy(doc)
        def probe(a,b):
            if a[2]==b[2]==100.25:
                valid={tuple(a),tuple(b)}=={(0,0,100.25),(0,64,100.25)}
                return dict(stand='accepted' if valid else 'gap_or_drop')
            if a[2]>b[2]+18.25:
                valid=a==[0,64,100.25] and b==[0,120,.25]
                return dict(stand='gap_or_drop',drop=dict(accepted=valid,delta=23.2499,max_drop=148.4375))
            if b[2]>a[2]+63:
                return dict(stand='blocked_clearance',free_climb=dict(accepted=False))
            valid={tuple(a),tuple(b)}=={(0,120,.25),(-20,92,.25)}
            return dict(stand='accepted' if valid else 'blocked_clearance')
        result,evidence=propose_descents(doc,native,probe,max_probes=32)
        self.assertEqual(doc,original)
        by_id={n['id']:n for n in result['nodes']}
        self.assertEqual(by_id[3],original['nodes'][0])
        self.assertEqual(evidence['added_nodes'],2)
        route=next(r for r in evidence['routes'] if r.get('chain'))
        self.assertEqual(route['chain'],[3,122,121,101])
        self.assertEqual(route['upper_approach'],[0,64,100.25])
        self.assertIsNone(route['return_chain'])
        self.assertEqual(by_id[122]['action_roles'],[dict(kind='walk_drop',role='launch',landing_node=121)])
        edges={(e['from'],e['to']) for e in result['edges']}
        self.assertTrue({(3,122),(122,3),(122,121),(121,101),(101,121)}<=edges)
        self.assertNotIn((121,122),edges)
        self.assertFalse(any(n['nav_flags']&512 for n in result['nodes']))
        self.assertLessEqual(evidence['direct_probes'],8)
        self.assertEqual(evidence['staging_probe_reserve'],24)
        self.assertLessEqual(len(evidence['queries']),32)


if __name__=='__main__':unittest.main()
