"""Only native-backed spawn entries may have a proven directed DROP exit."""
import copy
import math
from pathlib import Path
import sys
import unittest

sys.path[:0]=[str(Path(__file__).resolve().parent.parent),str(Path(__file__).resolve().parent.parent/'navmesh')]
from repair import propose_descents,terminal_spawn_components


class SpawnTerminalTests(unittest.TestCase):
    def fixture(self,translation=(0.,0.,0.)):
        # Literal two-point52-unit platform layout from the Fracture report;
        # responses below are synthetic proof-contract fixtures, not live
        # assertions about collision on the real map.
        positions={957:[2304.,886.81,52.25],958:[2432.,886.81,52.25],
                   852:[2240.,775.9587,.25],859:[2496.,775.9587,.25],
                   862:[2560.,886.81,.25],800:[2304.,665.1075,.25]}
        positions={i:[round(v[k]+translation[k],5)for k in range(3)]for i,v in positions.items()}
        nodes=[dict(id=i,position=p,radius=35.,source_radius=35.,nav_flags=0,crouch=False,
            spawn=i in(958,862),source_reason='map_entity_spawn_seed'if i in(958,862)else'external_native_validated_floor',
            topology_protected=True,locked=False,generated_guide=False,editor_role=0,action_roles=[],action_properties={})
            for i,p in sorted(positions.items())]
        pairs=[(957,958),(958,957),(852,859),(859,862),(862,800),(800,852)]
        pairs+=list((b,a)for a,b in pairs[2:])
        pairs.sort()
        doc=dict(map='obj_test',mode='manual_draft',revision=0,saved_revision=0,created=0,schema=1,
                 base={},nodes=nodes,edges=[{'from':a,'to':b}for a,b in pairs],next_id=2000,undo=[],redo=[],export=None)
        native=dict(map='obj_test',generation_complete=True,bbox=dict(half_width=16,stand_height=87,crouch_height=61,origin_above_feet=30),
            traversal_physics=dict(gravity=1000),traversal_routes=[],native_input_coverage=dict(ordinary_without_exit=[]),
            nodes=[dict(id=n['id'],accepted=True,spawn_reachable=True,nav_flags=0,crouch=False,editor_role=0,
                        reason=n['source_reason'],native_input=dict(floor_accepted=True,floor_reason=n['source_reason']))for n in nodes],
            edges=[dict(**{'from':a,'to':b},accepted=True,flags=0,crouch=False,reason='external_native_validated_walk')for a,b in pairs])
        return doc,native,positions

    def test_only_native_backed_spawn_cycle_qualifies_and_targets_unique_main_component(self):
        doc,native,_=self.fixture();before=copy.deepcopy((doc,native));result=terminal_spawn_components(doc,native)
        self.assertEqual((doc,native),before);self.assertEqual(set(result),{957,958})
        info=result[957];self.assertEqual(info['members'],[957,958]);self.assertEqual(info['spawn_source_ids'],[958])
        self.assertEqual(info['return_targets'],{800,852,859,862})
        self.assertEqual(info['main_component_source_ids'],[800,852,859,862]);self.assertFalse(info['requires_proved_return'])
        self.assertEqual(info['source_model'],'literal_doc_and_native_spawn_terminal_v1')

    def test_unknown_spawn_actions_unvalidated_edges_and_ambiguous_main_never_authorize_repair(self):
        cases=('candidate_only','native_only','floor_reason','native_reason','rejected_floor','not_reached','source_flag',
               'row_flag','source_crouch','row_crouch','source_role','row_role','generated','action_properties','door',
               'typed_route','unknown_edge','flagged_edge','crouched_edge','rejected_edge','has_exit','main_unproved','equal_main')
        for case in cases:
            doc,native,_=self.fixture();ns={n['id']:n for n in doc['nodes']};rs={n['id']:n for n in native['nodes']}
            internal=next(e for e in native['edges']if(e['from'],e['to'])==(957,958))
            if case=='candidate_only':rs[958]['reason']=rs[958]['native_input']['floor_reason']='external_native_validated_floor'
            elif case=='native_only':ns[958]['spawn']=False
            elif case=='floor_reason':rs[958]['native_input']['floor_reason']='unknown_floor'
            elif case=='native_reason':rs[958]['reason']='unknown_origin'
            elif case=='rejected_floor':rs[958]['native_input']['floor_accepted']=False
            elif case=='not_reached':rs[958]['spawn_reachable']=False
            elif case=='source_flag':ns[957]['nav_flags']=512
            elif case=='row_flag':rs[957]['nav_flags']=512
            elif case=='source_crouch':ns[957]['crouch']=True
            elif case=='row_crouch':rs[957]['crouch']=True
            elif case=='source_role':ns[957]['editor_role']=9
            elif case=='row_role':rs[957]['editor_role']=9
            elif case=='generated':ns[957]['generated_guide']=True
            elif case=='action_properties':ns[957]['action_properties']={'tce_walk_drop':'1'}
            elif case=='door':rs[957]['door_recipe']={'role':2}
            elif case=='typed_route':native['traversal_routes']=[dict(kind='jump',exported=True,launch=852,landing=957)]
            elif case=='unknown_edge':internal['reason']='unproved_manual_walk'
            elif case=='flagged_edge':internal['flags']=1
            elif case=='crouched_edge':internal['crouch']=True
            elif case=='rejected_edge':internal['accepted']=False
            elif case=='has_exit':native['edges'].append(dict(**{'from':958,'to':862},accepted=True))
            elif case=='main_unproved':rs[862]['native_input']['floor_reason']='external_native_validated_floor'
            elif case=='equal_main':
                native['nodes']=[n for n in native['nodes']if n['id']not in(800,852)]
                native['edges']=[e for e in native['edges']if e['from']not in(800,852)and e['to']not in(800,852)]
            with self.subTest(case=case):self.assertEqual(terminal_spawn_components(doc,native),{})

    def probe(self,positions,mode=None):
        high=positions[957];low=positions[852];launch=[low[0]+16,low[1]+16,low[2]];calls=[]
        near=lambda a,b:math.dist(a,b)<.001
        def query(a,b):
            calls.append((a,b));walk='accepted'if abs(a[2]-b[2])<=18.25 else'gap_or_drop'
            if mode=='launch_walk'and(near(a,launch)or near(b,launch)):walk='blocked_sweep'
            value=dict(stand=walk,imports={key:{stance:dict(accepted=True,exact=True,reason='accepted',feet=list(p))
                    for stance in('stand','crouch')}for key,p in(('start',a),('end',b))},
                action_imports={key:{'drop_launch':dict(accepted=True,exact=True,reason='accepted',feet=list(p))}
                                for key,p in(('start',a),('end',b))},
                action_walks={key:{'drop_launch':dict(accepted=walk=='accepted',reason=walk)}for key in('start','end')},
                drop=dict(accepted=near(a,high)and near(b,low)and mode!='drop',delta=10,max_drop=148.4375))
            if near(a,low)and near(b,high)and mode!='drop_only':
                value['free_climb']=dict(accepted=True,launch=launch[:],duration=.96,stamina_used=870)
                if mode=='climb_duration':value['free_climb']['duration']=2.01
                if mode=='climb_stamina':value['free_climb']['stamina_used']=2501
                if mode=='climb_launch':value['free_climb']['launch']=low[:]
            if mode=='landing'and near(a,high)and near(b,low):
                value['imports']['end']['stand'].update(accepted=False,exact=False,reason='no_floor_near_seed')
            if mode=='missing_action_import':value.pop('action_imports')
            elif mode=='rejected_action_import':
                value['action_imports']['start']['drop_launch'].update(accepted=False,exact=False,reason='blocked_clearance')
            elif mode=='inexact_action_import':value['action_imports']['start']['drop_launch']['exact']=False
            elif mode=='foreign_action_import':value['action_imports']['start']['drop_launch']['feet'][0]+=1
            elif mode=='missing_landing_import':value['imports']['end'].pop('stand')
            return value
        return query,calls

    def test_spawn_cycle_gets_independent_native_drop_and_climb_return_to_main_not_other_island(self):
        for translation in((0.,0.,0.),(-4096.,-221.7025,32.)):
            doc,native,positions=self.fixture(translation);before=copy.deepcopy((doc,native));probe,calls=self.probe(positions)
            result,evidence=propose_descents(doc,native,probe,max_probes=96)
            self.assertEqual((doc,native),before);self.assertLessEqual(len(calls),96)
            self.assertEqual(evidence['connected_native_spawn_components'],[[957,958]])
            route=next(r for r in evidence['routes']if r.get('chain'))
            self.assertEqual(route['native_spawn_terminal_component'],[957,958])
            self.assertIsNone(route['proved_traversal_terminal_component'])
            self.assertFalse(route['requires_proved_return_to_native_spawn_component'])
            self.assertTrue(route['spawned_entry_only'])
            self.assertEqual(route['spawn_source_ids'],[958])
            self.assertEqual(route['main_targets'],[800,852,859,862])
            self.assertEqual(route['source_model'],'literal_doc_and_native_spawn_terminal_v1')
            self.assertFalse(route['requires_proved_return_to_quarantined_source'])
            self.assertEqual(route['connection_node'],852);self.assertTrue(route['return_chain'])
            self.assertEqual(route['return_proof']['kind'],'free_climb')
            old={n['id']:n for n in doc['nodes']};new={n['id']:n for n in result['nodes']}
            self.assertTrue(all(new[i]['nav_flags']==n['nav_flags']and new[i]['position']==n['position']for i,n in old.items()))
            self.assertTrue(evidence['native_revalidation_required'])

    def test_directed_spawn_exit_never_fabricates_a_missing_or_invalid_return(self):
        for mode in('drop_only','climb_duration','climb_stamina','climb_launch','launch_walk'):
            doc,native,positions=self.fixture();probe,calls=self.probe(positions,mode)
            for node in doc['nodes']:
                node.update(name='original_%s'%node['id'],uid=node['id']+20,facing=[1.,2.,3.])
            before=copy.deepcopy(doc)
            result,evidence=propose_descents(doc,native,probe,max_probes=96)
            with self.subTest(mode=mode):
                self.assertEqual(doc,before);self.assertLessEqual(len(calls),96)
                self.assertEqual(evidence['connected_native_spawn_components'],[[957,958]])
                route=next(r for r in evidence['routes']if r.get('chain'))
                self.assertTrue(route['spawned_entry_only']);self.assertIsNone(route['return_chain'])
                self.assertIsNone(route['return_proof']);self.assertIsNone(route['return_chain_proof_sha256'])
                self.assertFalse(route['requires_proved_return_to_native_spawn_component'])
                old={n['id']:n for n in doc['nodes']};new={n['id']:n for n in result['nodes']}
                for i,node in old.items():
                    for key in('position','nav_flags','source_radius','name','uid','facing'):
                        self.assertEqual(new[i][key],node[key])
                self.assertTrue(all(e in result['edges']for e in doc['edges']))
                self.assertFalse(any(r.get('kind')in('free_climb','standing_jump')for n in result['nodes']for r in n.get('action_roles',[])))
                self.assertTrue(evidence['native_revalidation_required'])

    def test_missing_drop_or_literal_action_or_landing_proof_keeps_spawn_component_unrepaired(self):
        for mode in('drop','landing','missing_action_import','rejected_action_import','inexact_action_import',
                    'foreign_action_import','missing_landing_import'):
            doc,native,positions=self.fixture();probe,calls=self.probe(positions,mode)
            result,evidence=propose_descents(doc,native,probe,max_probes=96)
            with self.subTest(mode=mode):
                self.assertEqual(result,doc);self.assertLessEqual(len(calls),96)
                self.assertEqual(evidence['connected_native_spawn_components'],[])


if __name__=='__main__':unittest.main()
