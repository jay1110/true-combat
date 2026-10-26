"""Current mission snapshots cannot replace certified collision/draft inputs."""
import hashlib,json
from pathlib import Path
import tempfile
from types import SimpleNamespace
from unittest import TestCase,main
from unittest.mock import Mock,patch
from server import MOVEMENT_PROFILES,Studio


class GoalSnapshotTests(TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)
        for d in ('work','engine','native','bundles'):(self.root/d).mkdir()
        self.geometry=self.root/'native/obj_test_entities.json'
        self.geometry.write_text('{"old":"certified map geometry"}')
        self.draft=self.root/'work/draft.json';self.draft.write_text('{"revision":8,"nodes":[1,2]}')
        self.before={p:p.read_bytes()for p in (self.geometry,self.draft)}
        self.snapshot=dict(map='obj_test',gametype=2,objective_entities=[],models=[])
        self.studio=Studio.__new__(Studio)
        self.studio.game_mode='tce2'
        self.studio.script_directory=Mock(return_value='mapscripts')
        self.studio.available_game_types=Mock(return_value=dict(available=[2,5,7],directory='mapscripts'))
        self.studio.work=self.root/'work'
        self.studio.drafts=Mock();self.studio.drafts.get.return_value=None
        self.studio.native=Mock(return_value=dict(generation_complete=True))
        self.studio.process_api=SimpleNamespace(identity=Mock(return_value=['fixture-game',123]))
        self.studio.require_bbox=Mock(return_value=MOVEMENT_PROFILES[1])
        self.studio.active_project=dict(id='project',map='obj_test',mod='tce2',gametype=2,
            paths={d:str(self.root/d)for d in ('work','native','bundles')})
        self.studio.update=lambda job,**changes:job.update(changes)
        self.studio.require_map=lambda name:name
        self.studio.proof_status=lambda name:dict(valid=True,inputs={'base':'unchanged'})
        self.studio.own_test_process=Mock(return_value=dict(pid=12,owns_port=True))
        self.studio.require_loaded=Mock()
        self.current_gt=2
        self.studio.live_gametype=Mock(side_effect=lambda:self.current_gt)
        self.studio.engine_nav_root=lambda:self.root/'engine'
        self.studio.native_path=lambda name:self.root/'native/obj_test_navmesh.json'
        self.studio.native_path('obj_test').write_bytes(b'checked native report')
        self.studio.file_hash=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
        self.studio.action_load_map=Mock(side_effect=self.load_map)
        self.studio.quiet_generation=Mock(side_effect=lambda job:job.update(quiet_generation=dict(token='fixture')))
        self.studio.restore_generation_bots=Mock(side_effect=lambda job:job.pop('quiet_generation',None))
        self.studio.objective_anchor_scripts=Mock(side_effect=lambda job,require_loaded=False:dict(gametype=job['gametype'],selected=None))
        self.studio.stationary_goal_support=Mock(return_value="stationary-test-marker")
        self.studio.rcon_with_followup=Mock(side_effect=self.export)
        self.studio.wait_fresh=Mock(side_effect=lambda path,before,timeout:json.loads(path.read_bytes()))
        self.job=dict(map='obj_test',gametype=2,log='')

    def load_map(self,job):
        self.assertNotIn('quiet_generation',job)
        self.current_gt=job['gametype'];self.snapshot['gametype']=self.current_gt

    def export(self,command):
        self.assertEqual(command,'tce_navgen_entities')
        (self.root/'engine/obj_test_entities.json').write_text(json.dumps(self.snapshot))
        return 'fresh entities exported'

    def test_fresh_snapshot_archived_separately_before_goal_generation(self):
        with patch('goalgen.generate',return_value={'ok':True})as generate:
            result=self.studio.action_generate_goals(self.job)
            self.assertTrue(result['ok']);self.assertEqual(result['restored_gametype'],2)
        kwargs=generate.call_args.kwargs
        self.assertEqual(kwargs['stationary_support'],'stationary-test-marker')
        self.assertEqual(kwargs['runtime_snapshots'][2],self.snapshot)
        self.assertEqual(set(kwargs['runtime_snapshots']),{2,5})
        self.assertEqual(kwargs['runtime_snapshots'][5]['gametype'],5)
        evidence=kwargs['runtime_snapshot_evidences'][2];archive=Path(evidence['path'])
        self.assertEqual(archive.parent,self.root/'work/objective-snapshots')
        self.assertEqual(evidence['sha256'],hashlib.sha256(archive.read_bytes()).hexdigest())
        self.assertEqual((evidence['map'],evidence['gametype']),('obj_test',2))
        self.assertEqual(json.loads((self.root/'work/obj_test_gt2_objectives.json').read_bytes()),self.snapshot)
        self.assertTrue((self.root/'work/obj_test_gt2_objectives.proof.json').is_file())
        self.assertTrue((self.root/'work/obj_test_gt5_objectives.proof.json').is_file())
        self.assertEqual(self.current_gt,2)
        self.assertEqual(self.studio.action_load_map.call_count,3)
        for path,raw in self.before.items():self.assertEqual(path.read_bytes(),raw)

    def test_wrong_map_gt_or_missing_entities_cannot_generate_goals(self):
        for change in (dict(map='other'),dict(gametype=5),dict(objective_entities=None)):
            with self.subTest(change=change),patch('goalgen.generate')as generate:
                saved=dict(self.snapshot);self.snapshot.update(change)
                with self.assertRaisesRegex(RuntimeError,'Map/Gametype'):
                    self.studio.objective_snapshot(self.job)
                generate.assert_not_called();self.snapshot=saved
        self.assertFalse(list((self.root/'bundles').iterdir()))
        self.assertFalse((self.root/'work/objective-snapshots').exists())
        for path,raw in self.before.items():self.assertEqual(path.read_bytes(),raw)

    def test_failed_fresh_read_or_process_change_cannot_generate_goals(self):
        self.studio.wait_fresh.side_effect=RuntimeError('no fresh data')
        with patch('goalgen.generate')as generate,self.assertRaisesRegex(RuntimeError,'no fresh data'):
            self.studio.objective_snapshot(self.job)
        generate.assert_not_called()
        self.studio.wait_fresh.side_effect=lambda path,before,timeout:json.loads(path.read_bytes())
        self.studio.own_test_process.side_effect=[dict(pid=12),dict(pid=13)]
        with patch('goalgen.generate')as generate,self.assertRaisesRegex(RuntimeError,'Testinstanz'):
            self.studio.objective_snapshot(self.job)
        generate.assert_not_called()
        self.assertFalse((self.root/'work/objective-snapshots').exists())

    def test_inactive_gt_or_stale_navigation_never_exports_entities(self):
        self.job['gametype']=7
        with self.assertRaisesRegex(ValueError,'GT2-/GT5-Mapscript'):
            self.studio.action_generate_goals(self.job)
        self.studio.rcon_with_followup.assert_not_called()
        self.job['gametype']=2
        self.studio.proof_status=lambda name:dict(valid=False,reasons=['edited draft'])
        with self.assertRaisesRegex(RuntimeError,'edited draft'):
            self.studio.action_generate_goals(self.job)
        self.studio.rcon_with_followup.assert_not_called()

    def test_second_snapshot_failure_restores_selected_gt_and_never_generates(self):
        def read(path,before,timeout):
            if self.current_gt==5:raise RuntimeError('GT5 read failed')
            return json.loads(path.read_bytes())
        self.studio.wait_fresh.side_effect=read
        with patch('goalgen.generate')as generate,self.assertRaisesRegex(RuntimeError,'GT5 read failed'):
            self.studio.action_generate_goals(self.job)
        generate.assert_not_called()
        self.assertEqual(self.current_gt,2);self.assertEqual(self.job['gametype'],2)
        self.assertEqual(self.studio.action_load_map.call_args.args[0]['gametype'],2)
        self.assertNotIn('quiet_generation',self.job)
        for path,raw in self.before.items():self.assertEqual(path.read_bytes(),raw)

    def test_selected_gt5_is_restored_and_project_metadata_unchanged(self):
        self.job['gametype']=5;original=json.loads(json.dumps(self.studio.active_project))
        with patch('goalgen.generate',return_value={})as generate:
            result=self.studio.action_generate_goals(self.job)
        self.assertEqual(result['restored_gametype'],5)
        self.assertEqual(self.current_gt,5);self.assertEqual(self.job['gametype'],5)
        self.assertEqual(self.studio.active_project,original)
        self.assertEqual(set(generate.call_args.kwargs['runtime_snapshots']),{2,5})

    def test_restore_failure_is_explicit_and_no_goal_write_can_follow(self):
        calls=[]
        def load(job):
            calls.append(job['gametype'])
            if len(calls)==3:raise RuntimeError('restore start failed')
            self.load_map(job)
        self.studio.action_load_map.side_effect=load
        with patch('goalgen.generate')as generate,self.assertRaisesRegex(RuntimeError,'Wiederherstellung.*fehlgeschlagen'):
            self.studio.action_generate_goals(self.job)
        self.assertEqual(calls,[2,5,2]);self.assertEqual(self.job['gametype'],2)
        generate.assert_not_called()

    def test_changed_base_proof_blocks_gm_writes_after_both_reads(self):
        self.studio.proof_status=Mock(side_effect=lambda name:dict(valid=True,inputs={'base':
            'changed'if self.studio.rcon_with_followup.call_count>=4 else'unchanged'}))
        with patch('goalgen.generate')as generate,self.assertRaisesRegex(RuntimeError,'Projekt/Wegnetz.*keine Übernahme'):
            self.studio.action_generate_goals(self.job)
        self.assertEqual(self.studio.rcon_with_followup.call_count,4)
        self.assertEqual(self.current_gt,2);generate.assert_not_called()


if __name__=='__main__':main()
