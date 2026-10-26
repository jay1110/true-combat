"""Real GT2 explosive missions remain distinct from CTF and GT5 carriers."""
import copy
import hashlib
import json
import struct
from types import SimpleNamespace
from unittest import TestCase,main
from unittest.mock import patch

import goalgen
import test_goalgen
import test_objectiveanchors


def snapshot():
    # Each team owns one depot. Its opponent plants there; owners defend it.
    rows=[]
    for identity,owner,x in ((301,1,128),(303,2,384)):
        rows.append(dict(entity=identity,classname='trigger_objective_info',native_goal_kind='plant_defuse',
            track='Depot_'+str(owner),scriptname='depot_info_'+str(owner),spawnflags=64|owner,
            linked=True,contents=1073741824,state=0,position=[x,0,32],bounds=[[x-40,-32,-8],[x+40,32,76]],contact_top=76,
            target=dict(entity=identity+1,classname='func_explosive',targetname='depot_'+str(owner),
                scriptname='depot_'+str(owner),spawnflags=12,weaponclass=2,state=0,
                position=[x+28,0,32],bounds=[[x+20,-12,0],[x+36,12,64]])))
    return dict(map='obj_test',gametype=2,objective_entities=rows,models=[])


class GT2SabotageTests(TestCase):
    def test_two_native_enemy_depots_generate_explicit_operations_for_both_teams(self):
        sources=goalgen.observed_objectives(snapshot(),'obj_test',2)
        self.assertEqual([s['operation']for s in sources],['plant_defuse','plant_defuse'])
        self.assertEqual([s['owner']for s in sources],[1,2])
        plan=goalgen.make_plan(test_goalgen.GoalPlanTests().report(),sources,2)
        self.assertTrue(plan['mission_geometry_complete'])
        self.assertFalse(plan['mission_behavior_complete'])
        self.assertEqual(plan['mission_mode'],'sabotage')
        missions=[g for g in plan['goals']if g['type']=='TCEOBJECTIVE']
        self.assertEqual([g['entity_id']for g in missions],[301,303])
        self.assertEqual([g['source_node']for g in missions],[1,3])
        self.assertFalse(any(g['type']in('Flag','CapPoint')for g in plan['goals']))
        for mission in missions:
            self.assertEqual(mission['operation'],'plant_defuse')
            self.assertEqual(mission['teams'],6)
            self.assertIn('Operation = "plant_defuse"',goalgen.render_goal(mission))
            owner=mission['objective_owner'];enemy=3-owner
            tag=mission['name'].removeprefix('TCEOBJECTIVE_')
            attack=next(g for g in plan['goals']if g['name']=='ATTACK_'+tag)
            defend=next(g for g in plan['goals']if g['name']=='DEFEND_'+tag)
            self.assertEqual(attack['teams'],1<<enemy)
            self.assertEqual(defend['teams'],1<<owner)
            self.assertEqual(mission['tactical_attack_teams'],1<<enemy)
            self.assertEqual(mission['tactical_defend_teams'],1<<owner)
            self.assertNotIn('bomb_carrier_required',mission)

    def test_matching_inventory_never_revives_removed_bsp_flags_or_ordinary_gt5_crates(self):
        current=snapshot()
        current['objective_entities'].append(dict(entity=309,classname='trigger_objective_info',track='removed_crate',
            native_goal_kind='unsupported',spawnflags=130,target=None,position=[0,0,0]))
        with patch('goalgen.objectives',side_effect=AssertionError('must not revive BSP missions')):
            sources,mode=goalgen.mission_sources(None,'obj_test',2,current)
        self.assertEqual(mode,'matching_native_runtime_snapshot')
        self.assertEqual([s['entity_id']for s in sources],[301,303])

    def test_active_unknown_mission_blocks_instead_of_disappearing_into_tactics(self):
        for changes,reason in ((dict(native_goal_kind='unsupported'),'nicht unterstützter nativer Missionstyp'),
                               (dict(native_goal_kind='capture'),'kein nativer Flagonly'),
                               (dict(bounds=None),'Kontaktgeometrie')):
            current=snapshot();current['objective_entities'][1].update(changes)
            with self.subTest(changes=changes),self.assertRaisesRegex(ValueError,'GT2: aktives Missionsziel Entity 303.*'+reason):
                goalgen.observed_objectives(current,'obj_test',2)
        current=snapshot();current['objective_entities'][0]['target']['weaponclass']=1
        with self.assertRaisesRegex(ValueError,'Entity 301.*Waffenklasse 2'):
            goalgen.observed_objectives(current,'obj_test',2)

    def test_gt5_uses_same_geometry_but_retains_distinct_mode_and_operation(self):
        current=snapshot();current['gametype']=5
        plan=goalgen.make_plan(test_goalgen.GoalPlanTests().report(),goalgen.observed_objectives(current,'obj_test',5),5)
        self.assertEqual(plan['mission_mode'],'plant_defuse')
        self.assertTrue(plan['mission_geometry_complete'])
        self.assertTrue(all(g['operation']=='plant_defuse'for g in plan['goals']if g['type']=='TCEOBJECTIVE'))

    def test_bsp_gt2_explosive_proposal_uses_toi_contact_not_the_solid_target(self):
        models=[(-40,-32,-8,40,32,76,0,0,0,0),(20,-12,0,36,12,64,0,0,0,0)]
        data=b''.join(struct.pack('<6f4i',*m)for m in models)
        entities=[dict(bsp_id=20,classname='trigger_objective_info',spawnflags='65',model='*0',
                      target='depot',track='Depot',position=[0,0,32]),
                  dict(bsp_id=21,classname='func_explosive',targetname='depot',model='*1',position=[28,0,32])]
        lumps=[{}for _ in range(8)];lumps[7]=dict(offset=0,length=len(data))
        with patch('goalgen.bsp_entities',return_value=entities),patch('tce_bsp_navmesh.read_lumps',return_value=lumps):
            sources=goalgen.objectives(SimpleNamespace(read=lambda *args:data),'obj_test',2)
        self.assertEqual(len(sources),1)
        self.assertEqual(sources[0]['operation'],'plant_defuse')
        self.assertEqual(sources[0]['bounds'],[[-40,-32,-8],[40,32,76]])
        self.assertEqual(sources[0]['target_bounds'],[[20,-12,0],[36,12,64]])
        self.assertEqual(sources[0]['contact_top'],76)

    def test_unknown_source_operation_is_unresolved_not_synthetic_activation(self):
        source=goalgen.observed_objectives(snapshot(),'obj_test',2)[0];source['operation']='unknown'
        plan=goalgen.make_plan(test_goalgen.GoalPlanTests().report(),[source],2)
        self.assertFalse(plan['mission_geometry_complete'])
        self.assertEqual(plan['mission_goal_count'],0)
        self.assertEqual(plan['unresolved'][0]['reason'],'unsupported_native_mission_operation')

    def test_unknown_second_gt_aborts_before_any_pair_or_manifest_is_written(self):
        fixture=test_goalgen.GoalFilesTests();fixture.setUp();self.addCleanup(fixture.doCleanups)
        snapshots={2:snapshot(),5:dict(snapshot(),gametype=5)}
        snapshots[5]['objective_entities'][0]['native_goal_kind']='unsupported'
        evidences={}
        for gt,current in snapshots.items():
            path=fixture.root/('work/current-'+str(gt)+'.json');raw=json.dumps(current).encode();path.write_bytes(raw)
            evidences[gt]=dict(map='obj_test',gametype=gt,path=str(path),sha256=hashlib.sha256(raw).hexdigest())
        with self.assertRaisesRegex(ValueError,'GT5: aktives Missionsziel Entity 301'):
            goalgen.generate(fixture.studio,'obj_test',runtime_snapshots=snapshots,runtime_snapshot_evidences=evidences)
        self.assertEqual(list((fixture.root/'bundles').iterdir()),[])
        self.assertFalse((fixture.root/'work/generated-goals.json').exists())

    def test_removed_bsp_flag_has_no_phantom_cross_gt_contact_obligation(self):
        fixture=test_objectiveanchors.ObjectiveAnchorsTests();self.addCleanup(fixture.doCleanups)
        r,doc,_=fixture.fixture()
        current=dict(map=doc['map'],gametype=2,objective_entities=[],models=[])
        before=copy.deepcopy(doc)
        self.assertTrue(r._contacts(2,1952,r.nodes[1952]['position'],current))
        self.assertEqual(doc,before)
        # Once the actual matching native capture exists, its exact contact
        # remains compulsory. Removal is not a geometry tolerance waiver.
        self.assertFalse(r._contacts(2,1952,[10000,0,0],fixture.snapshot(2)))

    def test_cross_gt_sabotage_contact_uses_actual_toi_and_rejects_target_roof(self):
        fixture=test_objectiveanchors.ObjectiveAnchorsTests();self.addCleanup(fixture.doCleanups)
        r,_,_=fixture.fixture();current=snapshot();current['map']=r.doc['map']
        spec=goalgen.observed_objectives(current,r.doc['map'],2)[0]
        r.objectives={2:[dict(spec,entity_id=146)]}
        self.assertTrue(r._contacts(2,1952,[128,0,0],current))
        self.assertFalse(r._contacts(2,1952,[156,0,64],current))
        current['objective_entities'][0]['native_goal_kind']='unsupported'
        with self.assertRaisesRegex(ValueError,'aktives Missionsziel Entity 301'):
            r._contacts(2,1952,[128,0,0],current)


if __name__=='__main__':main()
