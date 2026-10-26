"""Generated missions retain proof, entity binding and distinct GT contracts."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
from unittest import TestCase, main
from unittest.mock import Mock, patch
from goalgen import capture_teams,make_plan,native_tag,render_goal,generate,map_logic,apply


class GoalPlanTests(TestCase):
    def report(self):
        nodes = [dict(id=i,position=[i*128,0,30],accepted=True,spawn_reachable=True,
                      nav_flags=0,radius=35,crouch=False) for i in range(6)]
        edges = [dict(**{'from':i,'to':j},accepted=True) for i in range(6) for j in range(6) if abs(i-j)==1]
        return dict(generation_complete=True,bbox=dict(origin_above_feet=30),nodes=nodes,edges=edges)

    def objective(self, operation='capture'):
        return dict(tag='capture_A',entity_id=42,position=[256,0,30],aim_position=[256,0,64],
            bounds=[[230,-32,-8],[280,32,72]],owner=1,operation=operation,target='siteA',target_class='trigger_flagonly_multiple',
            capture_teams=4,capture_flag_mask=1)

    def test_tc_missions_are_entity_bound_to_proven_contact_floor(self):
        result = make_plan(self.report(), [self.objective('plant_defuse')], 5)
        objective = next(g for g in result['goals'] if g['type']=='TCEOBJECTIVE')
        self.assertEqual(objective['source_node'], 2)
        self.assertEqual(objective['position'], [256,0,30])
        self.assertTrue(objective['contact_floor_checked'])
        self.assertEqual(objective['operation'], 'plant_defuse')
        rendered = render_goal(objective)
        self.assertIn('CreateOnLoad = 0',rendered)
        self.assertIn('UsePosition = Vec3(256.00000, 0.00000, 30.00000)',rendered)
        self.assertNotIn('Dynamite',rendered)
        self.assertTrue(result['mission_geometry_complete'])
        self.assertFalse(result['mission_behavior_complete'])

    def test_ctf_has_real_touch_cappoints_and_dynamic_native_flag_goals(self):
        capture = self.objective()
        flag = dict(tag='flagspecops',entity_id=55,position=[128,0,30],aim_position=[128,0,30],
                    bounds=None,owner=2,stealing_team=1,operation='take_flag',target='flagspecops',target_class='team_CTF_blueflag')
        result=make_plan(self.report(),[capture,flag],2)
        point=next(g for g in result['goals'] if g['type']=='CapPoint')
        self.assertEqual(point['teams'],4)
        self.assertIn('CreateOnLoad = 1',render_goal(point))
        dynamic=next(g for g in result['goals'] if g['type']=='Flag')
        self.assertEqual(dynamic['name'],'FLAG_flagspecops')
        self.assertEqual(dynamic['teams'],2)
        self.assertIn('CreateOnLoad = 0',render_goal(dynamic))
        self.assertFalse(any(g['type']=='TCEOBJECTIVE' for g in result['goals']))

    def test_flag_use_floor_matches_native_origin_box_with_arrival_reserve(self):
        flag = dict(tag='flag',entity_id=55,position=[128,0,30],aim_position=[128,0,30],
                    bounds=None,owner=2,stealing_team=1,operation='take_flag')
        report=self.report()
        report['nodes'][1]['position']=[136,0,55.125]
        goal=next(g for g in make_plan(report,[flag],2)['goals'] if g['type']=='Flag')
        self.assertTrue(goal['pickup_contact_checked'])
        self.assertEqual(goal['radius'],2)
        rendered=render_goal(goal)
        self.assertIn('TCECheckedUse = 1',rendered)
        self.assertIn('UsePosition = Vec3(136.00000, 0.00000, 55.12500)',rendered)
        self.assertIn('FlagBasePosition = Vec3(128.00000, 0.00000, 30.00000)',rendered)
        self.assertIn('ActivationRadius = 2',rendered)
        # Near is insufficient; every axis must retain arrival/movement reserve.
        for position in ([162,0,30],[128,34,30],[128,0,64],[128,0,90]):
            with self.subTest(position=position):
                report['nodes'][1]['position']=position
                plan=make_plan(report,[flag],2)
                self.assertFalse(any(g['type']=='Flag' for g in plan['goals']))
                self.assertEqual(plan['unresolved'][0]['reason'],'no_native_floor_in_flag_pickup_range')
        # BG_PlayerTouchesItem uses an origin box, not a 36-unit sphere.
        report['nodes'][1]['position']=[161,33,63]
        self.assertTrue(any(g['type']=='Flag' for g in make_plan(report,[flag],2)['goals']))

    def test_capture_team_comes_from_actual_flag_colour_not_oid_owner(self):
        self.assertEqual(capture_teams(5),4)
        self.assertEqual(capture_teams(6),2)
        self.assertEqual(capture_teams(7),6)
        self.assertEqual(capture_teams(None),0)
        self.assertEqual(capture_teams(4),0)
        for teams,defender in ((2,2),(4,4),(6,6)):
            source=self.objective();source['capture_teams']=teams
            result=make_plan(self.report(),[source],2)
            self.assertEqual(next(g for g in result['goals']if g['type']=='CapPoint')['teams'],teams)
            self.assertEqual(next(g for g in result['goals']if g['type']=='Defend')['teams'],defender)
        source=self.objective();source.pop('capture_teams')
        result=make_plan(self.report(),[source],2)
        self.assertFalse(any(g['type']=='CapPoint'for g in result['goals']))
        self.assertEqual(result['unresolved'][0]['reason'],'missing_flagonly_capture_flag_mask')

    def test_cappoint_uses_full_body_touch_outside_solid_flagpost_centre(self):
        report=self.report();report['nodes'][2]['position']=[222,0,30]
        site=self.objective()
        result=make_plan(report,[site],2)
        point=next(g for g in result['goals'] if g['type']=='CapPoint')
        self.assertEqual(point['position'],[222,0,30])
        self.assertEqual(point['radius'],2)
        self.assertTrue(point['contact_floor_checked'])
        # Expanded rectangular origin checks accept this diagonal corner,
        # but its distance from the trigger exceeds the actual hull radius.
        report['nodes'][2]['position']=[218,-44,30]
        result=make_plan(report,[site],2)
        self.assertFalse(any(g['type']=='CapPoint' for g in result['goals']))
        self.assertEqual(result['unresolved'][0]['reason'],'no_native_floor_in_objective_contact_volume')

    def test_unreachable_or_outside_contact_volume_is_reported_not_fabricated(self):
        report = self.report()
        report['nodes'][2]['spawn_reachable'] = False
        result = make_plan(report,[self.objective()],2)
        self.assertFalse(result['mission_geometry_complete'])
        self.assertEqual(result['mission_goal_count'],0)
        self.assertEqual(result['unresolved'][0]['reason'],'no_native_floor_in_objective_contact_volume')
        self.assertTrue(all(g['source_node'] != 2 for g in result['goals']))

    def test_action_guide_never_becomes_camp_or_use_floor(self):
        report = self.report(); report['nodes'][2]['nav_flags'] = 1 << 9
        result = make_plan(report,[],5)
        self.assertTrue(all(g['source_node'] != 2 for g in result['goals']))

    def test_owner_masks_assign_attacker_and_defender_correctly(self):
        goals = make_plan(self.report(),[self.objective('plant_defuse')],5)['goals']
        self.assertEqual(next(g for g in goals if g['type']=='Attack')['teams'], 4)
        self.assertEqual(next(g for g in goals if g['type']=='Defend')['teams'], 2)

    def test_plant_uses_exact_origin_query_and_excludes_crate_roof_and_solid(self):
        report=self.report()
        report['nodes'][2]['position']=[222,0,30.25]
        site=self.objective('plant_defuse')
        site['bounds']=[[230,-32,32],[280,32,76]]
        site['target_bounds']=[[246,-16,32],[264,16,76]]
        plan=make_plan(report,[site],5)
        goal=next(g for g in plan['goals']if g['type']=='TCEOBJECTIVE')
        self.assertEqual(goal['position'],[222,0,30.25])
        self.assertEqual(goal['activation_radius'],2)
        self.assertIn('ActivationRadius = 2',render_goal(goal))
        # Origin30's plant box reaches Z50 even though feet0 is lower than
        # the old guessed loZ-32 threshold. A point on the crate is not usable.
        report['nodes'][2]['position']=[256,0,106.25]
        failed=make_plan(report,[site],5)
        self.assertEqual(failed['mission_goal_count'],0)
        self.assertFalse(failed['mission_geometry_complete'])

    def test_native_name_sanitization_matches_bridge_and_mapgoal(self):
        self.assertEqual(native_tag(dict(track='^1Container [A]')), 'Container_A')
        self.assertEqual(native_tag(dict(track='The control-point (B)')), 'control_point_B')

    def test_generation_does_not_accept_incomplete_or_uncertified_floors(self):
        report = self.report(); report['generation_complete'] = False
        with self.assertRaisesRegex(ValueError, 'vollständige'):
            make_plan(report,[],2)
        report = self.report()
        for node in report['nodes']: node['spawn_reachable'] = False
        with self.assertRaisesRegex(ValueError, 'Keine verbundenen'):
            make_plan(report,[],5)


class GoalFilesTests(GoalPlanTests):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for folder in ('work','bundles','native'): (self.root/folder).mkdir()
        native = self.root/'native/obj_test_navmesh.json'; native.write_text('proof report')
        (self.root/'assets/maps').mkdir(parents=True)
        for gt in(2,5):(self.root/'assets/maps'/('obj_test_gt'+str(gt)+'.script')).write_text('game_manager { spawn { } }')
        self.studio = SimpleNamespace(require_map=lambda n:n,game_mode='tce2',
            active_project=dict(id='project-one',map='obj_test',gametype=2,paths={key:str(self.root/key) for key in ('work','bundles','native')}),
            proof_status=lambda n:dict(valid=True,inputs=dict(candidate='current-hash')),
            native=lambda n:self.report(),native_path=lambda n:native,
            file_hash=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest(),assets=SimpleNamespace(roots=[self.root/'assets']))

    def test_gt2_gt5_pairs_saved_separately_and_regenerate_only_owned_files(self):
        with patch('goalgen.objectives',side_effect=lambda a,n,gt:[self.objective('capture' if gt==2 else 'plant_defuse')]):
            first = generate(self.studio,'obj_test')
            second = generate(self.studio,'obj_test')
        self.assertEqual(len(first['files']),4)
        self.assertTrue((self.root/'bundles/obj_test_gt2.gm').exists())
        self.assertTrue((self.root/'bundles/obj_test_gt5_goals.gm').exists())
        document = json.loads((self.root/'work/generated-goals.json').read_text())
        self.assertEqual(document['project'],'project-one')
        self.assertEqual(document['proof_inputs'],dict(candidate='current-hash'))
        self.assertFalse(second['mission_behavior_complete'])

    def test_manual_edits_or_imported_files_are_never_overwritten(self):
        path = self.root/'bundles/obj_test_gt5.gm'
        path.write_text('// user map logic')
        with patch('goalgen.objectives',return_value=[]), self.assertRaisesRegex(RuntimeError,'überschrieben'):
            generate(self.studio,'obj_test')
        self.assertEqual(path.read_text(),'// user map logic')
        self.assertFalse((self.root/'bundles/obj_test_gt2.gm').exists())

    def test_stale_graph_proof_prevents_all_file_changes(self):
        self.studio.proof_status=lambda n:dict(valid=False,reasons=['draft has changed'])
        with self.assertRaisesRegex(RuntimeError,'draft has changed'):
            generate(self.studio,'obj_test')
        self.assertFalse(list((self.root/'bundles').iterdir()))

    def test_separate_current_objective_snapshot_preserves_geometry_and_manifest_sha(self):
        snapshot=dict(map='obj_test',gametype=2,objective_entities=[])
        path=self.root/'work/obj_test_gt2_objectives.json';path.write_text(json.dumps(snapshot))
        evidence=dict(map='obj_test',gametype=2,path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        geometry=self.root/'native/obj_test_entities.json';geometry.write_bytes(b'{"geometry":"certified old base"}')
        original=geometry.read_bytes()
        with patch('goalgen.objectives',return_value=[]):
            generate(self.studio,'obj_test',runtime_snapshot=snapshot,runtime_snapshot_evidence=evidence)
        manifest=json.loads((self.root/'work/generated-goals.json').read_bytes())
        self.assertEqual(manifest['runtime_objective_snapshot'],evidence)
        self.assertEqual(manifest['plans'][0]['mission_entity_evidence'],evidence)
        self.assertEqual(manifest['plans'][1]['mission_entity_source'],'bsp_proposal_runtime_entities_unconfirmed')
        self.assertIsNone(manifest['plans'][1]['mission_entity_evidence'])
        self.assertEqual(geometry.read_bytes(),original)

    def test_fresh_other_gametype_does_not_replace_certified_native_snapshot(self):
        geometry=dict(map='obj_test',gametype=2,objective_entities=[])
        native_snapshot=self.root/'native/obj_test_entities.json'
        native_snapshot.write_text(json.dumps(geometry))
        original=native_snapshot.read_bytes()
        snapshot=dict(map='obj_test',gametype=5,objective_entities=[])
        path=self.root/'work/obj_test_gt5_objectives.json';path.write_text(json.dumps(snapshot))
        evidence=dict(map='obj_test',gametype=5,path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        with patch('goalgen.objectives') as bsp:
            generate(self.studio,'obj_test',runtime_snapshot=snapshot,runtime_snapshot_evidence=evidence)
        bsp.assert_not_called()
        manifest=json.loads((self.root/'work/generated-goals.json').read_bytes())
        plans={plan['gametype']:plan for plan in manifest['plans']}
        self.assertEqual(plans[2]['mission_entity_source'],'matching_native_runtime_snapshot')
        self.assertIsNone(plans[2]['mission_entity_evidence'])
        self.assertEqual(plans[5]['mission_entity_source'],'matching_native_runtime_snapshot')
        self.assertEqual(plans[5]['mission_entity_evidence'],evidence)
        self.assertEqual(native_snapshot.read_bytes(),original)

    def test_wrong_map_gt_or_changed_snapshot_sha_prevents_all_gm_writes(self):
        for snapshot in (dict(map='obj_other',gametype=2,objective_entities=[]),
                         dict(map='obj_test',gametype=7,objective_entities=[]),
                         dict(map='obj_test',gametype=2)):
            with self.subTest(snapshot=snapshot),self.assertRaises(ValueError):
                generate(self.studio,'obj_test',runtime_snapshot=snapshot)
        snapshot=dict(map='obj_test',gametype=2,objective_entities=[])
        path=self.root/'work/obj_test_gt2_objectives.json';path.write_text(json.dumps(snapshot))
        with self.assertRaisesRegex(RuntimeError,'SHA/Map/GT'):
            generate(self.studio,'obj_test',runtime_snapshot=snapshot,
                runtime_snapshot_evidence=dict(map='obj_test',gametype=2,path=str(path),sha256='old'))
        self.assertFalse(list((self.root/'bundles').iterdir()))

    def paired_runtime_sources(self):
        snapshots={2:dict(map='obj_test',gametype=2,objective_entities=[
            dict(entity=10,classname='team_CTF_redflag',native_goal_kind='take_flag',scriptname='liveflag',
                 position=[128,0,30],dropped=False),
            dict(entity=11,classname='trigger_objective_info',native_goal_kind='capture',track='livecapture',
                 spawnflags=1,position=[256,0,30],bounds=[[224,-40,-8],[288,40,80]],
                 target=dict(classname='trigger_flagonly_multiple',spawnflags=5,targetname='flagonly',
                    position=[256,0,30],bounds=[[230,-32,-8],[280,32,72]]))]),
            5:dict(map='obj_test',gametype=5,objective_entities=[
                dict(entity=12,classname='trigger_objective_info',native_goal_kind='plant_defuse',track='livebomb',
                     spawnflags=2,position=[271,0,32],bounds=[[230,-32,-8],[312,32,72]],contact_top=72,
                     target=dict(classname='func_explosive',targetname='realbomb',position=[288,0,30],
                        bounds=[[280,-8,8],[296,8,52]]))])}
        evidence={}
        for gt,snapshot in snapshots.items():
            path=self.root/('work/real_gt'+str(gt)+'.json');path.write_text(json.dumps(snapshot))
            evidence[gt]=dict(map='obj_test',gametype=gt,path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest())
        return snapshots,evidence

    def test_dual_gt_uses_distinct_matching_native_data_and_hashes_without_legacy_geometry_read(self):
        snapshots,evidence=self.paired_runtime_sources()
        geometry=self.root/'native/obj_test_entities.json';geometry.write_bytes(b'certified geometry must not be parsed as current missions')
        original=geometry.read_bytes()
        with patch('goalgen.objectives',side_effect=AssertionError('no BSP fallback for dual native capture')):
            result=generate(self.studio,'obj_test',runtime_snapshots=snapshots,runtime_snapshot_evidences=evidence)
        self.assertEqual(result['runtime_objective_snapshots'],evidence)
        for plan in result['plans']:
            self.assertEqual(plan['mission_entity_source'],'matching_native_runtime_snapshot')
            self.assertEqual(plan['mission_entity_evidence'],evidence[plan['gametype']])
            self.assertTrue(plan['mission_geometry_complete'])
        self.assertEqual([s['entity_id']for s in result['plans'][0]['objectives']],[10,11])
        self.assertEqual([s['entity_id']for s in result['plans'][1]['objectives']],[12])
        manifest=json.loads((self.root/'work/generated-goals.json').read_bytes())
        self.assertEqual(set(manifest['runtime_objective_snapshots']),{'2','5'})
        self.assertEqual(geometry.read_bytes(),original)

    def test_dual_gt_rejects_missing_wrong_gt_and_hash_or_mixed_source_before_any_gm_write(self):
        snapshots,evidence=self.paired_runtime_sources()
        for payload in (
            dict(runtime_snapshots={2:snapshots[2]},runtime_snapshot_evidences={2:evidence[2]}),
            dict(runtime_snapshots={2:snapshots[5],5:snapshots[2]},runtime_snapshot_evidences=evidence),
            dict(runtime_snapshots=snapshots,runtime_snapshot_evidences={2:evidence[2]}),
            dict(runtime_snapshots=snapshots,runtime_snapshot_evidences={2:dict(evidence[2],sha256='old'),5:evidence[5]}),
            dict(runtime_snapshots=snapshots,runtime_snapshot_evidences=evidence,runtime_snapshot=snapshots[2]),
        ):
            with self.subTest(payload=payload),self.assertRaises((ValueError,RuntimeError)):
                generate(self.studio,'obj_test',**payload)
            self.assertFalse(list((self.root/'bundles').iterdir()))

    def apply_fixture(self):
        with patch('goalgen.objectives',return_value=[self.objective('plant_defuse')]):
            generate(self.studio,'obj_test')
        self.studio.test = self.root/'game'
        checked_way=self.root/'native/obj_test_navmesh.way';checked_way.write_bytes(b'current checked WAY')
        runtime=self.root/'game/omni-bot/et/nav';runtime.mkdir(parents=True)
        (runtime/'obj_test.way').write_bytes(checked_way.read_bytes())
        self.studio.update = lambda job,**kw:job.update(kw)
        self.studio.own_test_process = Mock(return_value=dict(pid=12))
        self.studio.stop_owned_test = Mock()
        self.studio.action_load_map = Mock()
        self.studio.require_loaded = Mock()
        self.studio.live_gametype = Mock(return_value=2)
        manifest=json.loads((self.root/'work/generated-goals.json').read_text())
        self.studio.rcon = Mock(return_value='"omnibot_navstudio_goal_token" is: "'+str(manifest['plans'][0]['map_logic_token'])+'"')
        names=[g['name'] for g in manifest['plans'][0]['goals']]
        self.studio.rcon_with_followup = Mock(side_effect=lambda cmd: '\n'.join(
            str(i+1)+': '+name for i,name in enumerate(names)) if cmd=='bot show_goals' else 'loaded')
        return names

    def test_apply_exact_pair_restarts_same_installation_and_verifies_live_names(self):
        names=self.apply_fixture()
        original=self.root/'game/omni-bot/et/nav/obj_test_gt2.gm'
        original.write_bytes(b'original runtime logic')
        result=apply(self.studio,dict(map='obj_test',gametype=2,log=''))
        self.studio.stop_owned_test.assert_called_once()
        self.studio.action_load_map.assert_called_once()
        self.assertTrue(result['live_load_confirmed'])
        self.assertTrue(result['map_logic_confirmed'])
        self.assertEqual(result['loaded_goal_names'],sorted(names))
        self.assertEqual(Path(result['backups'][0]['backup']).read_bytes(),b'original runtime logic')
        self.assertFalse(result['mission_behavior_complete'])
        self.assertTrue((self.root/'work/goals-applied-gt2.json').exists())
        self.assertNotIn('bot goal_load',[call.args[0] for call in self.studio.rcon_with_followup.call_args_list])

    def test_missing_entity_bound_goal_cannot_be_reported_as_applied(self):
        names=self.apply_fixture()
        self.studio.rcon_with_followup.side_effect=lambda cmd:'\n'.join(
            str(i+1)+': '+name for i,name in enumerate(names) if not name.startswith('TCEOBJECTIVE_')) if cmd=='bot show_goals' else 'loaded'
        with self.assertRaisesRegex(RuntimeError,'TCEOBJECTIVE_capture_A'):
            apply(self.studio,dict(map='obj_test',gametype=2,log=''))
        receipt=json.loads((self.root/'work/goals-applied-gt2.json').read_text())
        self.assertTrue(receipt['installed'])
        self.assertFalse(receipt['live_load_confirmed'])

    def test_stale_native_report_stops_before_game_or_runtime_changes(self):
        self.apply_fixture()
        (self.root/'native/obj_test_navmesh.json').write_text('changed proof')
        with self.assertRaisesRegex(RuntimeError,'früheren Wegnetz'):
            apply(self.studio,dict(map='obj_test',gametype=2,log=''))
        self.studio.stop_owned_test.assert_not_called()
        self.studio.action_load_map.assert_not_called()
        self.assertFalse((self.root/'game/omni-bot/et/nav/obj_test_gt2.gm').exists())

    def test_old_runtime_way_blocks_goal_deploy_even_with_valid_project_proof(self):
        self.apply_fixture()
        runtime=self.root/'game/omni-bot/et/nav/obj_test.way';runtime.write_bytes(b'old partial graph')
        with self.assertRaisesRegex(RuntimeError,'Projekt-Wegnetz im Spiel installiert'):
            apply(self.studio,dict(map='obj_test',gametype=2,log=''))
        self.studio.stop_owned_test.assert_not_called()
        self.studio.action_load_map.assert_not_called()
        self.assertEqual(runtime.read_bytes(),b'old partial graph')
        self.assertFalse((runtime.parent/'obj_test_gt2.gm').exists())


if __name__ == '__main__': main()
