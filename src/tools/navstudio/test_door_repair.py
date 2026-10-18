"""Door repair imports new native actions while retaining the user's graph."""
import copy
import tempfile
from pathlib import Path
from unittest import TestCase, main

from doorrepair import merge
from edit import Drafts


class DoorRepairTests(TestCase):
    def setUp(self):
        self.doc=dict(map='obj_test',next_id=1200,nodes=[],edges=[dict(zip(('from','to'),p)) for p in [(1,2),(2,1)]])
        for i in (1,2):
            self.doc['nodes'].append(dict(id=i,position=[i*100.,0.,0.25],radius=80,
                nav_flags=0,crouch=False,spawn=False,locked=False,generated_guide=False,way_guid=i,
                name='Authored '+str(i),action_properties={'note':'keep me'},action_roles=[]))
        self.report=dict(map='obj_test',generation_complete=True,door_generation=dict(explicit_repair=True),
                         traversal_routes=[],nodes=[],edges=[])
        for i,n in enumerate(self.doc['nodes']):
            self.report['nodes'].append(dict(n,accepted=True,index=i,position=[*n['position'][:2],30.25]))
        for i,role in [(1200,1),(1201,2),(1202,3)]:
            recipe=dict(entity=70,use_entity=70,target_source_id=(i+1 if role!=3 else -1),mode=1,team=3,
                role=role,model=4,use_model=4,use_source_id=1200,entry_source_id=1201,
                aim=[160,10,50],validated=True,exported=True)
            self.report['nodes'].append(dict(id=i,index=i-1198,position=[150.,i-1200.,30.25],radius=8,
                nav_flags=128 if role!=3 else 0,way_guid=i,accepted=True,door_recipe=recipe))
        self.report['edges']=[dict(zip(('from','to'),p),accepted=True) for p in
            [(1,2),(2,1),(1,1200),(1200,1201),(1201,1202),(1202,2)]]

    def test_current_authored_nodes_and_opaque_fields_are_exactly_retained(self):
        before=copy.deepcopy(self.doc);result,evidence=merge(self.doc,self.report,30)
        self.assertEqual(self.doc,before)
        self.assertEqual(result['nodes'][:2],before['nodes'])
        self.assertEqual(evidence['added_node_ids'],[1200,1201,1202])
        self.assertEqual(result['next_id'],1203)
        self.assertEqual(result['nodes'][2]['action_roles'][0]['target_node'],1201)
        self.assertEqual(result['nodes'][2]['action_properties']['tce_door_target'],'1201')
        self.assertEqual(result['nodes'][2]['position'][2],.25)

    def test_rejects_stale_incomplete_unvalidated_or_removed_input(self):
        for mutate in [lambda r:r.update(map='obj_other'),lambda r:r.update(generation_complete=False),
                       lambda r:r['door_generation'].update(explicit_repair=False),
                       lambda r:r['nodes'].pop(0),lambda r:r['edges'].pop(0),
                       lambda r:r['nodes'][2]['door_recipe'].update(validated=False),
                       lambda r:r['nodes'][0].update(position=[999,0,30.25]),
                       lambda r:r['nodes'][0].update(nav_flags=64),
                       lambda r:r['nodes'][0].update(way_guid=7)]:
            report=copy.deepcopy(self.report);mutate(report)
            with self.assertRaises(RuntimeError):merge(self.doc,report,30)

    def test_deleted_id_reservations_and_ordinary_topology_cannot_be_restored(self):
        self.doc['next_id']=1300
        with self.assertRaises(RuntimeError):merge(self.doc,self.report,30)
        self.doc['next_id']=1200
        self.report['edges'].append(dict(**{'from':2,'to':1},accepted=True))
        # An actual novel ordinary link is forbidden, not its duplicate.
        self.doc['edges'].pop()
        with self.assertRaises(RuntimeError):merge(self.doc,self.report,30)

    def test_only_native_bound_ordinary_door_approaches_can_be_added(self):
        guide=dict(id=1203,index=5,position=[130.,0.,30.25],radius=35,nav_flags=0,way_guid=1203,
                   accepted=True,topology_protected=True,reason='generated_door_approach_live_pending')
        self.report['nodes'].append(guide)
        self.report['edges'].extend([dict(**{'from':a,'to':b},accepted=True) for a,b in [(1,1203),(1203,1),(1203,1200),(1200,1203)]])
        with self.assertRaises(RuntimeError):merge(self.doc,self.report,30)
        self.report['door_generation']['approach_source_ids']=[1203]
        result,evidence=merge(self.doc,self.report,30)
        self.assertEqual(evidence['approach_node_ids'],[1203])
        self.assertTrue(result['nodes'][-1]['generated_guide'])
        self.assertFalse(result['nodes'][-1]['action_roles'])

    def test_unchanged_native_floor_projection_binds_raw_literal_without_moving_user_point(self):
        self.doc['nodes'][0]['position'][2]=-4
        self.report['nodes'][0].update(raw_position=[100,0,26],position=[100,0,30.25])
        baseline=copy.deepcopy(self.report)
        result,_=merge(self.doc,self.report,30,baseline)
        self.assertEqual(result['nodes'][0],self.doc['nodes'][0])
        self.report['nodes'][0]['position'][2]+=2
        with self.assertRaises(RuntimeError):merge(self.doc,self.report,30,baseline)
        self.report['nodes'][0]['position'][2]-=2
        self.report['nodes'][0]['raw_position'][2]+=1
        with self.assertRaises(RuntimeError):merge(self.doc,self.report,30,baseline)

    def test_undo_and_export_keep_both_authored_data_and_door_source_references(self):
        result,_=merge(self.doc,self.report,30)
        with tempfile.TemporaryDirectory() as temp:
            store=Drafts(Path(temp)/'drafts')
            base=store.start('obj_test',dict(nodes=self.doc['nodes'],edges=self.doc['edges']),dict(environment={}))
            proposal=copy.deepcopy(base);proposal.update(nodes=result['nodes'],edges=result['edges'],next_id=result['next_id'])
            changed=store.apply_repair('obj_test',0,{},proposal)
            store.save('obj_test',changed['revision'],{})
            candidate=store.export_candidate('obj_test',changed['revision'],{},Path(temp)/'preview')
            self.assertTrue(candidate.read_text().startswith('TCE_NAVMESH 6'))
            rows={int(line.split()[0]):line.split() for line in candidate.read_text().splitlines()[1:6]}
            for identity in (1200,1201,1202):self.assertEqual(rows[identity][10],'0')
            restored=store.change('obj_test',dict(op='undo',revision=changed['revision']),{})
            self.assertEqual(restored['nodes'],base['nodes'])
            self.assertEqual(restored['edges'],base['edges'])


if __name__=='__main__':main()
