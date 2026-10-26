"""The convenience workflow never treats repair proposals as native proof."""
import copy
import hashlib
import json
from pathlib import Path
import tempfile
from unittest import TestCase, main
from unittest.mock import Mock
from server import Studio, IncompleteNativeFeedback, MOVEMENT_PROFILES, isolated_export_feedback, read_json


class WorkflowTests(TestCase):
    def test_direct_native_check_loads_one_owned_map_and_pauses_bots_first(self):
        s=Studio.__new__(Studio); calls=[]
        s.action_load_map=lambda job:calls.append("load")
        s.quiet_generation=lambda job:calls.append("quiet")
        s.require_loaded=lambda name:calls.append("map")
        def bbox(job):
            calls.append("bbox");raise RuntimeError("stopped before import")
        s.require_bbox=bbox
        with self.assertRaisesRegex(RuntimeError,"stopped before import"):
            s.action_native_check(dict(action="native_check",map="obj_delta",gametype=5))
        self.assertEqual(calls,["load","quiet","map","bbox"])

    def fixture(self):
        s = Studio.__new__(Studio)
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        s.work=Path(temporary.name)
        s.update = lambda job, **kw: job.update(kw)
        s.action_load_map = Mock()
        s.quiet_generation = Mock()
        s.action_rebuild = Mock(return_value={"counts": {"nodes": 3}})
        s.action_native_check = Mock(return_value={"audit": {}, "proof": "exact proof"})
        s.action_repair = Mock(return_value={"repairs": {"added_nodes": 0, "added_directed_edges": 0}})
        # These tests isolate bounded scanner feedback. Generated-draft and
        # cross-GT completion use their own explicit workflow fixtures.
        s.normalize_generated_graph = Mock(return_value=None)
        s.reconcile_objective_anchors = Mock(return_value=None)
        s.complete_generated_project = Mock(return_value=dict(goals={},spawns={},
            generated_gametypes=[2,5],package_status={'state':'ready'}))
        s.install = Mock(side_effect=AssertionError("No automatic install"))
        return s

    def test_no_more_suggestions_stops_after_exact_native_recheck(self):
        s = self.fixture()
        result = s.action_generate({"map": "obj_delta"})
        self.assertEqual(s.action_native_check.call_count, 2)
        s.action_repair.assert_called_once()
        self.assertFalse(result["installed"])
        s.install.assert_not_called()

    def test_repair_passes_are_bounded_and_all_checked(self):
        s = self.fixture()
        s.action_repair.return_value = {"repairs": {"added_directed_edges": 2}}
        s.action_generate({"map": "obj_delta"})
        self.assertEqual(s.action_repair.call_count, 2)
        self.assertEqual(s.action_native_check.call_count, 3)

    def test_native_failure_stops_without_later_repair_or_install(self):
        s = self.fixture()
        s.action_native_check.side_effect = RuntimeError("collision import failed")
        with self.assertRaisesRegex(RuntimeError, "collision import failed"):
            s.action_generate({"map": "obj_delta"})
        s.action_repair.assert_not_called()
        s.install.assert_not_called()

    @staticmethod
    def partial():
        return IncompleteNativeFeedback('1 Punkt ohne Ausgang (Source-IDs: 9766)',
                                        dict(status='incomplete_export_feedback_only', ordinary_without_exit=[9766]))

    def test_incomplete_first_pass_recovers_only_after_fresh_native_success(self):
        s = self.fixture(); error = self.partial()
        s.action_native_check.side_effect = [error, {'audit': {'fresh': True}, 'proof': 'new proof'},
                                             {'audit': {'fresh': True}, 'proof': 'final proof'}]
        s.action_repair.side_effect = [{'repairs': {'added_nodes': 1}}, {'repairs': {}}]
        result = s.action_generate({'map': 'obj_delta'})
        self.assertEqual(result['proof'], 'final proof')
        self.assertEqual(s.action_native_check.call_count, 3)
        self.assertEqual(s.action_repair.call_args_list[0].kwargs['feedback'], error.feedback)
        self.assertNotIn('proof', result['stages'][0])
        self.assertEqual(result['stages'][0]['status'], 'incomplete_feedback_only')
        s.install.assert_not_called()

    def test_unrecoverable_partial_stops_at_two_repairs_without_proof(self):
        s = self.fixture()
        s.action_native_check.side_effect = [self.partial(), self.partial(), self.partial()]
        s.action_repair.return_value = {'repairs': {'added_directed_edges': 2}}
        job = {'map': 'obj_delta'}
        with self.assertRaisesRegex(RuntimeError, '9766.*2/2'):
            s.action_generate(job)
        self.assertEqual(s.action_repair.call_count, 2)
        self.assertEqual(s.action_native_check.call_count, 3)
        self.assertTrue(all('proof' not in stage for stage in job['workflow']))
        s.install.assert_not_called()

    def test_partial_without_new_suggestions_stops_after_one_recheck(self):
        s = self.fixture()
        s.action_native_check.side_effect = [self.partial(), self.partial()]
        with self.assertRaisesRegex(RuntimeError, '1/2'):
            s.action_generate({'map': 'obj_delta'})
        self.assertEqual(s.action_repair.call_count, 1)

    def test_later_partial_never_returns_an_older_success_receipt(self):
        s = self.fixture()
        s.action_native_check.side_effect = [{'audit': {}, 'proof': 'old proof'}, self.partial()]
        with self.assertRaisesRegex(RuntimeError, '9766'):
            s.action_generate({'map': 'obj_delta'})
        s.install.assert_not_called()


class IncompleteFeedbackTests(TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.s = Studio.__new__(Studio)
        for field in ('preview', 'runtime', 'work'):
            value = self.root/field; value.mkdir(); setattr(self.s, field, value)
        self.job = dict(map='obj_test', log='')
        self.path = self.s.path('obj_test', '_candidate.txt')
        self.path.write_text('TCE_NAVMESH 4 obj_test 2 2\n0 0 0 0 0 1 0\n1 32 0 0 0 0 0\n0 1\n1 0\n')
        self.metadata = dict(graph_policy='preserve_spatial_samples', counts=dict(nodes=2,directed_edges=2),
                             nodes=[dict(id=0,point=[0,0,0]),dict(id=1,point=[32,0,0])],edges=[[0,1]])
        self.path.with_suffix('.json').write_text(json.dumps(self.metadata))
        self.report = dict(schema=1,map='obj_test',source='navgen/obj_test_candidate.txt',generation_complete=False,
            candidate_mode='automatic_spatial',candidate_kind='external_bsp_recast',preserve_spatial_samples=True,
            traces=20,trace_budget=100,bbox=dict(half_width=16,stand_height=87,crouch_height=61,origin_above_feet=30),
            native_input_coverage=dict(input_nodes=2,initial_directed_links=2,input_ids=[0,1],ordinary_without_exit=[0],
                initial_accepted_links=0,initial_rejected_links=2,native_floor_accepted=1,native_floor_rejected=1,
                capacity=dict(input_validation_complete=True,input_truncated=False)),
            nodes=[dict(id=i,raw_position=[i*32,0,30],accepted=i==0,nav_flags=0,
                        native_input=dict(floor_accepted=i==0,initial_incoming=0,initial_outgoing=0)) for i in range(2)],
            edges=[dict(**{'from':a,'to':b},accepted=False) for a,b in ((0,1),(1,0))])
        self.s.update = lambda job, **kw: job.update(kw)
        self.s.require_loaded = Mock(); self.s.require_bbox = Mock(return_value=MOVEMENT_PROFILES[1])
        self.s.require_profile_inputs = Mock(); self.s.candidate = Mock(return_value=self.path)
        self.s.file_hash = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest() if Path(p).is_file() else None
        self.inputs = dict(candidate=self.s.file_hash(self.path))
        self.s.proof_status = Mock(return_value=dict(valid=True,inputs=self.inputs))
        self.s.rcon_with_followup = Mock(return_value='offline response')
        self.s.inspect = Mock(side_effect=AssertionError('Old WAY must not be audited'))
        def fresh(path, *args):
            path.write_text(json.dumps(self.report)); return copy.deepcopy(self.report)
        self.s.wait_fresh = Mock(side_effect=fresh)
        self.s.native = lambda name: read_json(self.s.native_path(name))
        self.s.command = Mock(side_effect=AssertionError('No offline test may run a real worker'))
        self.s.worker = lambda *args: list(args)

    def test_strict_action_fails_but_exposes_bound_repair_feedback(self):
        old_way = self.s.native_path('obj_test').with_suffix('.way'); old_way.write_bytes(b'old WAY')
        with self.assertRaises(IncompleteNativeFeedback) as caught:
            self.s.action_native_check(self.job)
        feedback = caught.exception.feedback
        self.assertEqual(feedback['ordinary_without_exit'], [0])
        self.assertEqual(feedback['native_report'], self.s.file_hash(self.s.native_path('obj_test')))
        self.assertEqual(feedback['staged_candidate'], self.inputs['candidate'])
        self.assertFalse(feedback['acceptance_certificate'])
        self.assertFalse((self.s.work/'obj_test.native-proof.json').exists())
        self.assertEqual(old_way.read_bytes(), b'old WAY')
        self.s.inspect.assert_not_called()

    def test_crouch_is_an_ordinary_floor_but_action_flags_are_not(self):
        self.report['nodes'][0]['nav_flags'] = 64
        feedback = isolated_export_feedback(self.report, self.metadata, self.path)
        self.assertEqual(feedback['ordinary_without_exit'], [0])
        for flags in (1, 65, None):
            self.report['nodes'][0]['nav_flags'] = flags
            with self.subTest(flags=flags), self.assertRaisesRegex(RuntimeError, 'nicht exakt belegt'):
                isolated_export_feedback(self.report, self.metadata, self.path)

    def test_invalid_or_partial_input_never_becomes_recovery_token(self):
        mutations = [lambda r: r['native_input_coverage']['capacity'].update(input_validation_complete=False),
                     lambda r: r['native_input_coverage']['capacity'].update(input_truncated=True),
                     lambda r: r['native_input_coverage'].update(input_ids=[0]),
                     lambda r: r['native_input_coverage'].update(initial_directed_links=3),
                     lambda r: r['native_input_coverage'].update(initial_accepted_links=1),
                     lambda r: r['native_input_coverage'].update(ordinary_without_exit=[]),
                     lambda r: r['nodes'][0].update(raw_position=[1,0,30]),
                     lambda r: r['edges'][0].update(accepted=True),
                     lambda r: r.update(traces=100)]
        original = copy.deepcopy(self.report)
        for mutate in mutations:
            self.report = copy.deepcopy(original); mutate(self.report)
            with self.subTest(report=self.report), self.assertRaises(RuntimeError) as caught:
                self.s.action_native_check(self.job)
            self.assertNotIsInstance(caught.exception, IncompleteNativeFeedback)
        self.s.inspect.assert_not_called()

    def test_input_or_profile_change_rejects_before_feedback(self):
        self.s.proof_status.side_effect = [dict(valid=True,inputs=self.inputs),dict(valid=True,inputs={'candidate':'changed'})]
        with self.assertRaisesRegex(RuntimeError, 'Quelldaten'):
            self.s.action_native_check(self.job)
        self.s.proof_status.side_effect = None
        self.s.require_bbox.side_effect = [MOVEMENT_PROFILES[1],MOVEMENT_PROFILES[0]]
        with self.assertRaisesRegex(RuntimeError, 'waehrend'):
            self.s.action_native_check(self.job)

    def test_partial_manual_repair_and_changed_feedback_are_rejected(self):
        with self.assertRaises(IncompleteNativeFeedback) as caught:
            self.s.action_native_check(self.job)
        with self.assertRaisesRegex(RuntimeError, 'Rückmeldung'):
            self.s.action_repair(self.job)
        for field in ('native_report','candidate','staged_candidate','inputs','movement_profile'):
            value = copy.deepcopy(caught.exception.feedback); value[field] = 'changed'
            with self.subTest(field=field), self.assertRaisesRegex(RuntimeError, 'Rückmeldung'):
                self.s.action_repair(self.job,feedback=value)
        self.s.command.assert_not_called()

    def test_valid_feedback_proposal_still_requires_new_native_proof(self):
        with self.assertRaises(IncompleteNativeFeedback) as caught:
            self.s.action_native_check(self.job)
        def proposal(*args):
            out = self.s.path('obj_test','_candidate_repair.json')
            out.write_text(json.dumps(dict(counts={'nodes':3},native_feedback_repairs=dict(
                sha256=caught.exception.feedback['native_report'],added_nodes=1,added_directed_edges=2))))
        self.s.command.side_effect = proposal
        result = self.s.action_repair(self.job,feedback=caught.exception.feedback)
        self.assertEqual(result['status'], 'candidate_requires_native_validation')
        self.assertNotIn('proof', result)
        self.assertFalse((self.s.work/'obj_test.native-proof.json').exists())

    def test_receipt_false_only_proves_inputs_never_partial_export(self):
        self.s.drafts = Mock(); self.s.drafts.get.return_value = None
        self.s.bsp_hash = Mock(return_value='fixture-bsp')
        for suffix in ('.stand.json','.duck.json','.collision.obj'):
            self.s.path('obj_test',suffix).write_bytes(b'fixture geometry')
        snapshot = self.s.runtime/'obj_test_entities.json'; snapshot.write_bytes(b'fixture entities')
        geometry = self.s.path('obj_test','.geometry.json')
        geometry.write_text(json.dumps(dict(input={'bsp_sha256':'fixture-bsp'},
            output={'obj_sha256':self.s.file_hash(self.s.path('obj_test','.collision.obj'))},
            runtime_snapshot={'sha256':self.s.file_hash(snapshot)})))
        self.metadata.update(map='obj_test',candidate_sha256=hashlib.sha256(
            self.path.read_text(encoding='ascii').encode('ascii')).hexdigest(),
            sources={kind:dict(sha256=self.s.file_hash(self.s.path('obj_test','.'+kind+'.json')))
                     for kind in ('stand','duck','geometry')})
        self.path.with_suffix('.json').write_text(json.dumps(self.metadata))
        source = Studio.proof_status(self.s,'obj_test',self.path,receipt=False)
        self.assertTrue(source['valid']); self.assertTrue(source['candidate_valid'])
        path = self.s.native_path('obj_test'); path.write_text(json.dumps(self.report))
        way = path.with_suffix('.way'); way.write_bytes(b'stale WAY must not grant acceptance')
        staged = self.s.runtime/self.path.name; staged.write_bytes(self.path.read_bytes())
        receipt = dict(inputs=source['inputs'],candidate=self.path.name,native_report=self.s.file_hash(path),
                       way=self.s.file_hash(way),staged_candidate=self.s.file_hash(staged))
        (self.s.work/'obj_test.native-proof.json').write_text(json.dumps(receipt))
        checked = Studio.proof_status(self.s,'obj_test',self.path,receipt=True)
        self.assertTrue(checked['candidate_valid']); self.assertFalse(checked['valid'])
        self.assertEqual(checked['reasons'], ['Vollständiger nativer Prüfbericht fehlt.'])


if __name__ == "__main__":
    main()


class NavigationReloadTests(TestCase):
    def studio(self, reply):
        s = Studio.__new__(Studio)
        s.workspace = Path("workspace"); s.test = Path("test")
        s.native = Mock(return_value={"generation_complete": True})
        s.proof_status = Mock(return_value={"valid": True})
        s.native_path = Mock(return_value=Path("map.json"))
        s.worker = Mock(return_value=[])
        s.command = Mock(return_value=json.dumps({"note": "Restart/reload map navigation before using this graph."}))
        s.server_map = Mock(return_value="obj_mp_office")
        s.rcon = Mock(return_value=reply)
        return s

    def test_positive_engine_reply_replaces_generic_restart_notice(self):
        s = self.studio("Loaded Waypoints.")
        result = s.install({"map": "obj_mp_office"}, False)
        self.assertTrue(result["reload_confirmed"])
        self.assertNotIn("Restart/reload", result["note"])
        s.rcon.assert_called_once_with("bot waypoint_load")

    def test_empty_or_error_reply_is_not_confirmation(self):
        for reply in ("", "ERROR Loading Waypoints.", "Loaded Waypoints. ERROR later"):
            with self.subTest(reply=reply):
                s = self.studio(reply)
                self.assertFalse(s.install({"map": "obj_mp_office"}, False)["reload_confirmed"])
                s.rcon.assert_called_once()

    def test_other_map_does_not_reload_wrong_navigation(self):
        s = self.studio("Loaded Waypoints."); s.server_map.return_value = "obj_delta"
        result = s.install({"map": "obj_mp_office"}, False)
        self.assertIn("obj_delta", result["reload_note"])
        s.rcon.assert_not_called()
