"""Cross-GT reconciliation workflow boundaries; no Engine or process access."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
from unittest import TestCase, main
from unittest.mock import Mock, patch

from server import MOVEMENT_PROFILES, Studio
from watch import ProcessIdentityChanged


class ObjectiveAnchorWorkflowTests(TestCase):
    def workflow(self):
        studio = Studio.__new__(Studio)
        studio.game_mode = "tce2"
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup);studio.work=Path(temp.name)
        studio.active_project = dict(id="project", map="obj_test", mod="tce2", gametype=2)
        studio.update = lambda job, **fields: job.update(fields)
        studio.action_load_map = Mock()
        studio.quiet_generation = Mock()
        studio.restore_generation_bots = Mock()
        studio.action_rebuild = Mock(return_value=dict(counts=dict(nodes=3)))
        studio.action_repair = Mock(return_value=dict(repairs={}))
        studio.normalize_generated_graph = Mock(return_value=None)
        studio.native = Mock(return_value=dict(map='obj_test',generation_complete=True,
            bbox=dict(half_width=16,stand_height=87,crouch_height=61,origin_above_feet=30),
            nodes=[dict(id=i,position=[i*128,0,30],accepted=True,spawn_reachable=True,
                        nav_flags=0,crouch=False,radius=35)for i in range(3)],
            edges=[dict(**{'from':a,'to':b},accepted=True)for a,b in((0,1),(1,0),(1,2),(2,1))]))
        studio.action_native_check = Mock(return_value=dict(audit=dict(unresolved_nodes=[]), proof="current proof"))
        studio.reconcile_objective_anchors = Mock(return_value=None)
        studio.complete_generated_project=Mock(return_value=dict(objective_anchor_certificate=None,package_status=dict(ready=True)))
        studio.script_directory=Mock(return_value='mapscripts')
        studio.available_game_types=Mock(return_value=dict(available=[2,5,7],directory='mapscripts'))
        studio.install = Mock(side_effect=AssertionError("No automatic install"))
        return studio

    def test_generate_reconciles_only_after_the_final_native_check(self):
        studio = self.workflow()
        events = []
        studio.action_native_check.side_effect = lambda job: (
            events.append("native") or dict(audit=dict(unresolved_nodes=[]), proof="current proof"))
        studio.complete_generated_project.side_effect=lambda *args,**kwargs:events.append('completion')or dict(objective_anchor_certificate=None)
        job = dict(map="obj_test", gametype=2, action="generate", log="")
        result = studio.action_generate(job)
        self.assertEqual(events, ["native", "native", "completion"])
        studio.complete_generated_project.assert_called_once_with(job)
        studio.reconcile_objective_anchors.assert_not_called()
        self.assertFalse(result["installed"])
        studio.install.assert_not_called()

    def test_repair_check_reconciles_after_a_successful_full_native_check(self):
        studio = self.workflow()
        studio.drafts = Mock()
        studio.drafts.get.return_value = None
        studio.recover_rejected_shortcuts = Mock(return_value=None)
        studio.recover_rejected_ordinary = Mock(return_value=None)
        events = []
        studio.action_native_check.side_effect = lambda job: (
            events.append("native") or dict(audit=dict(unresolved_nodes=[]), proof="current proof"))
        studio.reconcile_objective_anchors.side_effect = lambda *args, **kwargs: events.append("anchors")
        job = dict(map="obj_test", gametype=5, action="repair_check", log="")
        result = studio.action_repair_check(job)
        studio.native.assert_called_once_with('obj_test')
        self.assertEqual(events, ["native", "anchors"])
        self.assertEqual(studio.reconcile_objective_anchors.call_args.args[0], job)
        self.assertFalse(result["installed"])
        studio.install.assert_not_called()

    def test_failed_native_check_cannot_enter_cross_gt_reconciliation(self):
        studio = self.workflow()
        studio.action_native_check.side_effect = RuntimeError("current collision check failed")
        with self.assertRaisesRegex(RuntimeError, "current collision check failed"):
            studio.action_generate(dict(map="obj_test", gametype=2, action="generate", log=""))
        studio.reconcile_objective_anchors.assert_not_called()
        studio.complete_generated_project.assert_not_called()
        studio.install.assert_not_called()

    def test_pure_native_check_never_enters_reconciliation_or_repairs(self):
        studio = self.workflow()
        studio.require_loaded = Mock()
        studio.require_bbox = Mock(side_effect=RuntimeError("stop before native import"))
        with self.assertRaisesRegex(RuntimeError, "stop before native import"):
            Studio.action_native_check(studio, dict(map="obj_test", gametype=5, action="native_check", log=""))
        studio.reconcile_objective_anchors.assert_not_called()
        studio.action_repair.assert_not_called()
        studio.install.assert_not_called()

    def test_goal_generation_uses_reconciled_snapshots_before_stationary_support_and_gm_write(self):
        studio = self.workflow()
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        manifest = Path(temp.name) / "manifest.json"
        manifest.write_text("{}")
        studio.require_map = lambda name: name
        studio.proof_status = Mock(return_value=dict(valid=True, inputs={}))
        snapshots = {2: dict(gametype=2), 5: dict(gametype=5)}
        evidence = {gt: dict(path=f"gt{gt}.json") for gt in snapshots}
        certificate = dict(path="common-anchor-certificate.json")
        events = []
        def reconcile(*args, **kwargs):
            events.append("anchors")
            return dict(certificate=certificate, runtime_snapshots=snapshots,
                        runtime_objective_snapshots=evidence, restored_gametype=5)
        studio.reconcile_objective_anchors.side_effect = reconcile
        studio.stationary_goal_support = Mock(side_effect=lambda job: events.append("stationary") or "support")
        def generate(*args, **kwargs):
            events.append("goals")
            self.assertEqual(kwargs["runtime_snapshots"], snapshots)
            self.assertEqual(kwargs["runtime_snapshot_evidences"], evidence)
            self.assertEqual(kwargs["stationary_support"], "support")
            return dict(manifest=str(manifest))
        job = dict(map="obj_test", gametype=5, action="generate_goals", log="")
        with patch("goalgen.generate", side_effect=generate):
            result = studio.action_generate_goals(job)
        studio.reconcile_objective_anchors.assert_called_once_with(job, (2, 5), capture_goals=True)
        self.assertEqual(events, ["anchors", "stationary", "goals"])
        self.assertEqual(result["objective_anchor_certificate"], certificate)
        self.assertEqual(json.loads(manifest.read_text())["objective_anchor_certificate"], certificate)
        self.assertEqual(result["restored_gametype"], 5)

    def test_gt7_goal_generation_rejects_before_reconciliation_or_writes(self):
        studio = self.workflow()
        studio.require_map = lambda name: name
        studio.proof_status = Mock(return_value=dict(valid=True, inputs={}))
        studio.stationary_goal_support = Mock()
        with patch("goalgen.generate") as generate, self.assertRaisesRegex(ValueError, "GT2-/GT5-Mapscript"):
            studio.action_generate_goals(dict(map="obj_test", gametype=7, action="generate_goals", log=""))
        studio.reconcile_objective_anchors.assert_not_called()
        studio.stationary_goal_support.assert_not_called()
        generate.assert_not_called()


class ObjectiveAnchorVisitTests(TestCase):
    def fixture(self, selected):
        temp = tempfile.TemporaryDirectory()
        self.addCleanup(temp.cleanup)
        studio = Studio.__new__(Studio)
        studio.work = Path(temp.name)
        scan = studio.work / "generation-scans" / "source"
        scan.mkdir(parents=True)
        candidate = scan / "obj_test_candidate.txt"
        candidate.write_text("source candidate")
        candidate.with_suffix(".json").write_text("{}")
        scanner = scan / "obj_test.native.json"
        scanner.write_text("{}")
        (studio.work / "obj_test.generation-diagnostics.json").write_text(json.dumps(
            dict(native_report=str(scanner), candidate_path=str(candidate))))
        doc = dict(map="obj_test", revision=4, base=dict(generation_diagnostics="bound"),
                   nodes=[dict(id=1, position=[0, 0, .25])], edges=[])
        baseline = dict(generation_complete=True, traversal_physics=dict(gravity=800.), bbox=dict(half_width=16, stand_height=87,
                         crouch_height=61, origin_above_feet=30))
        studio.game_mode = "tce2"
        studio.script_directory=Mock(return_value='mapscripts')
        studio.available_game_types=Mock(return_value=dict(available=[2,5,7],directory='mapscripts'))
        studio.active_project = dict(id="project", map="obj_test", mod="tce2", gametype=selected)
        studio.assets = object()
        studio.require_map = lambda name: name
        studio.update = lambda job, **fields: job.update(fields)
        studio.drafts = Mock()
        studio.drafts.get.return_value = doc
        studio.proof_status = Mock(return_value=dict(valid=True, inputs=dict(base="same")))
        studio.native = Mock(return_value=baseline)
        studio.native_path = lambda name: studio.work / (name + ".native.json")
        studio.native_path("obj_test").write_text(json.dumps(baseline))
        studio.native_path("obj_test").with_suffix(".way").write_bytes(b"unchanged WAY fixture")
        (studio.work / "obj_test.native-proof.json").write_text(json.dumps(dict(inputs=dict(base="same"))))
        studio.candidate = lambda name: candidate
        studio.file_hash = lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
        studio.own_test_process = Mock(return_value=dict(pid=12))
        studio.game_executable=studio.work/'ET.exe'
        studio.args=SimpleNamespace(rcon_port=27986)
        studio.process_api = SimpleNamespace(identity=Mock(return_value=["fixture-game", 123]),probe_owner=Mock())
        studio.require_bbox = Mock(return_value=MOVEMENT_PROFILES[1])
        studio.rcon = Mock(side_effect=AssertionError("No Engine call in orchestration fixture"))
        studio.action_native_check = Mock(side_effect=AssertionError("No graph move was proposed"))
        job = dict(map="obj_test", gametype=selected, log="", quiet_generation=dict(token="existing"))
        events = []
        def restore(job):
            events.append(("restore", job["gametype"]))
            job.pop("quiet_generation", None)
        def load(job):
            self.assertNotIn("quiet_generation", job, "Restore the previous VM token before every restart")
            events.append(("load", job["gametype"]))
        def quiet(job):
            self.assertNotIn("quiet_generation", job)
            job["quiet_generation"] = dict(token=f"gt{job['gametype']}")
            events.append(("quiet", job["gametype"]))
        def snapshot(job, allowed_gametypes=(2, 5)):
            self.assertIn(job["gametype"], allowed_gametypes)
            events.append(("snapshot", job["gametype"]))
            return (dict(map="obj_test", gametype=job["gametype"], models=[]),
                    dict(map="obj_test", gametype=job["gametype"], path="snapshot.json", sha256="2" * 64))
        studio.restore_generation_bots = Mock(side_effect=restore)
        studio.action_load_map = Mock(side_effect=load)
        studio.quiet_generation = Mock(side_effect=quiet)
        studio.objective_snapshot = Mock(side_effect=snapshot)
        studio.objective_anchor_scripts = Mock(side_effect=lambda job,require_loaded=False:dict(gametype=job['gametype'],selected=None))
        reconciler = Mock()
        reconciler.queries = []
        reconciler.visits = []
        reconciler.diagnostic.return_value=dict(queries=[],visits=[])
        reconciler.observe.side_effect = lambda gt, *args: reconciler.visits.append(dict(gametype=gt))
        reconciler.proposal.return_value = (copy.deepcopy(doc), [])
        reconciler.certificate.return_value = dict(source_ids=[1], moves=[], query_count=0)
        return studio, job, events, reconciler

    def test_default_scope_uses_both_objective_states_and_gt7_only_its_selected_state(self):
        for selected, scope, loads in ((2, (2, 5), [2, 5, 2]), (5, (2, 5), [5, 2, 5]), (7, (7,), [7])):
            with self.subTest(selected=selected):
                studio, job, events, reconciler = self.fixture(selected)
                with patch("objectiveanchors.archive_anchors", return_value=[dict(source_id=1)]), \
                        patch("objectiveanchors.Reconciler", return_value=reconciler) as construct, \
                        patch("goalgen.objectives", return_value=[]):
                    result = studio.reconcile_objective_anchors(job)
                self.assertEqual(construct.call_args.args[3], scope)
                self.assertEqual([gt for action, gt in events if action == "load"], loads)
                self.assertEqual(events[0], ("restore", selected))
                self.assertEqual(job["gametype"], selected)
                self.assertNotIn("quiet_generation", job)
                self.assertEqual(set(result["runtime_snapshots"]), set(scope))
                self.assertEqual(result["restored_gametype"], selected)
                self.assertEqual(result["certificate"]["gametypes"], list(scope))
                studio.drafts.apply_repair.assert_not_called()
                studio.action_native_check.assert_not_called()

    def test_second_state_failure_restores_bots_and_selected_gt_without_certificate_or_goals(self):
        studio, job, events, reconciler = self.fixture(2)
        snapshot = studio.objective_snapshot.side_effect
        def failed(job, allowed_gametypes=(2, 5)):
            if job["gametype"] == 5:
                raise RuntimeError("GT5 snapshot failed")
            return snapshot(job, allowed_gametypes)
        studio.objective_snapshot.side_effect = failed
        with patch("objectiveanchors.archive_anchors", return_value=[dict(source_id=1)]), \
                patch("objectiveanchors.Reconciler", return_value=reconciler), \
                patch("goalgen.objectives", return_value=[]), \
                patch("goalgen.generate") as generate, self.assertRaisesRegex(RuntimeError, "GT5 snapshot failed"):
            studio.reconcile_objective_anchors(job)
        self.assertEqual([gt for action, gt in events if action == "load"], [2, 5, 2])
        self.assertEqual(events[-2:], [("restore", 5), ("load", 2)])
        self.assertEqual(job["gametype"], 2)
        self.assertNotIn("quiet_generation", job)
        self.assertFalse(list(studio.work.glob("*.objective-anchors.json")))
        studio.drafts.apply_repair.assert_not_called()
        generate.assert_not_called()

    def test_probe_owner_change_blocks_rcon_and_current_certificate(self):
        studio, job, _, reconciler = self.fixture(2)
        studio.game_executable=studio.work/'ET.exe'
        studio.args=SimpleNamespace(rcon_port=27986)
        studio.process_api.probe_owner=Mock(side_effect=ProcessIdentityChanged('owner changed'))
        reconciler.observe.side_effect=lambda gt,snapshot,evidence,probe:probe([0,0,.25],[128,0,.25])
        with patch('objectiveanchors.archive_anchors',return_value=[dict(source_id=1)]), \
                patch('objectiveanchors.Reconciler',return_value=reconciler), \
                patch('goalgen.objectives',return_value=[]), self.assertRaisesRegex(ProcessIdentityChanged,'owner changed'):
            studio.reconcile_objective_anchors(job)
        studio.process_api.probe_owner.assert_called_once_with(12,['fixture-game',123],
            expected_exe=studio.game_executable,port=27986)
        studio.rcon.assert_not_called()
        self.assertFalse((studio.work/'obj_test.objective-anchors.json').exists())
        self.assertNotIn('quiet_generation',job)

    def test_new_failed_attempt_invalidates_the_previous_current_success_pointer(self):
        studio, job, _, _ = self.fixture(2)
        pointer = studio.work / 'obj_test.objective-anchors.json'
        pointer.write_text('{"status":"complete","old":true}')
        studio.proof_status.return_value = dict(valid=False)
        with self.assertRaisesRegex(RuntimeError, 'vollständige native Prüfung'):
            studio.reconcile_objective_anchors(job)
        self.assertFalse(pointer.exists())
        studio.action_load_map.assert_not_called()

    def test_mapscript_change_at_restart_blocks_current_certificate_and_records_failure(self):
        studio, job, _, reconciler = self.fixture(2)
        studio.objective_anchor_scripts.side_effect = lambda job, require_loaded=False: dict(
            gametype=job['gametype'], selected=dict(sha256=('b' if require_loaded else 'a') * 64))
        with patch('objectiveanchors.archive_anchors', return_value=[dict(source_id=1)]), \
                patch('objectiveanchors.Reconciler', return_value=reconciler), \
                patch('goalgen.objectives', return_value=[]), self.assertRaisesRegex(RuntimeError, 'Mapscript/Override'):
            studio.reconcile_objective_anchors(job)
        reconciler.observe.assert_not_called()
        self.assertFalse((studio.work / 'obj_test.objective-anchors.json').exists())
        failure = json.loads((studio.work / 'obj_test.objective-anchors-failed.json').read_text())
        self.assertEqual(failure['status'], 'failed')
        self.assertEqual(job['gametype'], 2)
        self.assertNotIn('quiet_generation', job)

    def test_failed_queries_are_immutably_archived_even_if_gt_restoration_fails(self):
        studio,job,_,reconciler=self.fixture(2)
        diagnostic=dict(queries=[dict(raw_reply='native failed reply',reply_sha256=hashlib.sha256(b'native failed reply').hexdigest())],visits=[])
        reconciler.diagnostic.return_value=diagnostic
        reconciler.observe.side_effect=RuntimeError('native probe failed')
        original_load=studio.action_load_map.side_effect
        calls=[]
        def load(job):
            calls.append(job['gametype'])
            if len(calls)>1:raise RuntimeError('restore failed')
            original_load(job)
        studio.action_load_map.side_effect=load
        with patch('objectiveanchors.archive_anchors',return_value=[dict(source_id=1)]), \
                patch('objectiveanchors.Reconciler',return_value=reconciler), \
                patch('goalgen.objectives',return_value=[]),self.assertRaisesRegex(RuntimeError,'native probe failed.*restore failed'):
            studio.reconcile_objective_anchors(job)
        evidence=job['objective_anchor_failure_evidence'];path=Path(evidence['path']);raw=path.read_bytes()
        self.assertEqual(hashlib.sha256(raw).hexdigest(),evidence['sha256'])
        saved=json.loads(raw)
        self.assertEqual(saved['search'],diagnostic)
        self.assertEqual(saved['status'],'failed')
        self.assertTrue(all(Path(value['path']).is_file()and hashlib.sha256(Path(value['path']).read_bytes()).hexdigest()==value['sha256']
                            for value in saved['source_documents'].values()))
        self.assertFalse((studio.work/'obj_test.objective-anchors.json').exists())

    def test_failed_start_of_selected_gt_still_attempts_physical_gt_restoration(self):
        studio, job, events, reconciler = self.fixture(2)
        load = studio.action_load_map.side_effect
        attempts = []
        def failed_once(job):
            attempts.append(job['gametype'])
            if len(attempts) == 1:
                raise RuntimeError('selected GT process did not start')
            load(job)
        studio.action_load_map.side_effect = failed_once
        with patch('objectiveanchors.archive_anchors', return_value=[dict(source_id=1)]), \
                patch('objectiveanchors.Reconciler', return_value=reconciler), \
                patch('goalgen.objectives', return_value=[]), self.assertRaisesRegex(RuntimeError, 'process did not start'):
            studio.reconcile_objective_anchors(job)
        self.assertEqual(attempts, [2, 2])
        self.assertNotIn('quiet_generation', job)
        self.assertFalse((studio.work / 'obj_test.objective-anchors.json').exists())

    def test_missing_worker_source_provenance_fails_before_engine_access(self):
        studio, job, _, _ = self.fixture(2)
        studio.file_hash = lambda path: None
        with self.assertRaisesRegex(RuntimeError, 'Programmquellen.*fehlen'):
            studio.reconcile_objective_anchors(job)
        studio.action_load_map.assert_not_called()

    def test_et_reconciliation_is_a_noop_before_any_project_or_engine_access(self):
        studio = Studio.__new__(Studio)
        studio.game_mode = "et"
        studio.require_map = Mock(side_effect=AssertionError("ET must keep its existing workflow"))
        studio.action_load_map = Mock(side_effect=AssertionError("No ET restart"))
        self.assertIsNone(studio.reconcile_objective_anchors(dict(map="goldrush", gametype=2)))
        studio.require_map.assert_not_called()
        studio.action_load_map.assert_not_called()


if __name__ == "__main__":
    main()
