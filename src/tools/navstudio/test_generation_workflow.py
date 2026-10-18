"""Actual scanner/draft files and workflow boundaries; never starts a game."""
import copy
import json
from pathlib import Path
import sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import tempfile
import threading
from unittest import TestCase, main
from unittest.mock import Mock, patch

from edit import Drafts, graph_hash
from server import Studio, MOVEMENT_PROFILES, IncompleteNativeFeedback, read_json


class NormalizedGenerationTests(TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(); self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.s = Studio.__new__(Studio)
        for name in ("work", "runtime", "preview"):
            value = self.root/name; value.mkdir(); setattr(self.s, name, value)
        self.s.hash_cached = {}; self.s.native_cached = {}
        self.s.drafts = Drafts(self.s.work/"drafts")
        self.s.lock = threading.RLock(); self.s.busy = False
        self.s.require_map = lambda n: n
        self.s.draft_environment = lambda n: {"bsp": "fixture", "stand": "same"}
        self.s.recast_body_profile = lambda n: MOVEMENT_PROFILES[1]
        self.s.proof_status = Mock(return_value=dict(valid=True, candidate_valid=True, reasons=[], inputs={"fixture":"same"}))
        self.s.imported_navigation = Mock(return_value=None)
        self.s.spawn_layer = Mock(return_value={})
        self.name = "obj_test"
        self.candidate = self.s.path(self.name, "_candidate.txt")
        self.candidate.write_text("scanner candidate")
        self.candidate.with_suffix(".json").write_text(json.dumps(dict(map=self.name, nodes=[], edges=[])))
        self.report = dict(schema=1, map=self.name, source="navgen/obj_test_candidate.txt", candidate_mode="automatic_spatial",
            generation_complete=False, traces=20, trace_budget=100,
            bbox=dict(half_width=16, stand_height=87, crouch_height=61, origin_above_feet=30),
            native_input_coverage=dict(ordinary_without_exit=[4], capacity=dict(input_validation_complete=True, input_truncated=False)),
            nodes=[dict(id=i, position=[i*32,0,30], raw_position=[i*32,0,30], accepted=accepted,
                        nav_flags=0, radius=35, index=i if accepted else -1,
                        reason="map_entity_spawn_seed" if i==3 else "floor" if accepted else reason,
                        native_input=dict(floor_accepted=floor))
                   for i,accepted,floor,reason in ((3,True,True,""),(4,True,True,""),(7,False,False,"blocked_clearance"),
                                                 (8,False,True,"quarantined_no_validated_route_from_spawn"))],
            edges=[dict(**{"from":3,"to":4}, accepted=True), dict(**{"from":4,"to":8}, accepted=False, reason="gap_or_drop")])
        self.write_report()

    def write_report(self):
        self.s.native_path(self.name).write_text(json.dumps(self.report))
        record = dict(candidate=self.candidate.name, inputs=self.s.proof_status.return_value["inputs"],
                      native_report=self.s.file_hash(self.s.native_path(self.name)))
        (self.s.work/(self.name+".native-attempt.json")).write_text(json.dumps(record))

    def test_scanner_and_network_become_explicit_separate_sets_without_an_editor_click(self):
        result = self.s.normalize_generated_graph({"map":self.name})
        doc = self.s.drafts.get(self.name)
        self.assertEqual([n["id"] for n in doc["nodes"]], [3,4])
        self.assertEqual(doc["edges"], [{"from":3,"to":4}])
        self.assertEqual(doc["nodes"][0]["position"], [96,0,0])
        # Accepted ordinary node4 has no exit: do not pretend it was repaired.
        self.assertIn(4, result["accepted_node_ids"])
        self.assertTrue(result["requires_native_validation"])
        self.assertFalse((self.s.work/(self.name+".native-proof.json")).exists())
        diagnosis = self.s.generation_diagnostics(self.name)
        self.assertTrue(diagnosis["current"])
        self.assertEqual(diagnosis["counts"]["connectivity_pending_nodes"], 1)
        self.assertEqual(diagnosis["counts"]["geometry_rejected_nodes"], 1)
        self.assertFalse(diagnosis["full_map_verified"])
        self.assertEqual([n["id"] for n in diagnosis["nodes"]], [7,8])
        self.assertEqual(diagnosis["edges"][0]["from_position"], [128,0,0])
        self.assertEqual(diagnosis["edges"][0]["to_position"], [256,0,0])
        self.assertEqual(read_json(Path(result["diagnostic_manifest"]))["candidate"]["name"], self.candidate.name)
        preserved = read_json(Path(read_json(Path(result["diagnostic_manifest"]))["native_report"]))
        self.assertEqual(preserved, self.report)

    def test_incomplete_report_action_uids_allow_repair_without_using_an_old_way(self):
        self.report['nodes'][0].update(index=0,way_guid=0,import_uid=900,editor_role=65,
            editor_drop_target=4,nav_flags=1<<27,radius=2)
        self.report['nodes'][1].update(index=1,way_guid=0,import_uid=0)
        self.report['traversal_routes']=[dict(kind='jump',standing_launch=True,exported=True,
            approach=3,launch=3,landing=4)]
        self.write_report();before=copy.deepcopy(self.report)
        self.s.native_path(self.name).with_suffix('.way').write_bytes(b'OLD UNRELATED WAY MUST NOT BE READ')
        result=self.s.normalize_generated_graph({'map':self.name})
        doc=self.s.drafts.get(self.name)
        launch=next(n for n in doc['nodes'] if n['id']==3);landing=next(n for n in doc['nodes'] if n['id']==4)
        self.assertEqual(launch['way_guid'],900) # Requested original source identity is retained.
        self.assertEqual(landing['way_guid'],0) # No new output identity is claimed.
        self.assertEqual(launch['action_properties']['tce_stand_jump_target'],'2')
        self.assertEqual(launch['action_roles'][0]['landing_node'],4)
        self.assertTrue(result['requires_native_validation'])
        self.assertFalse((self.s.work/(self.name+'.native-proof.json')).exists())
        self.assertEqual(self.s.native(self.name),before)

    def test_begin_edit_without_change_is_a_graph_and_revision_noop(self):
        self.s.normalize_generated_graph({"map":self.name})
        before = copy.deepcopy(self.s.drafts.get(self.name)); before_hash = graph_hash(before)
        result = self.s.edit(dict(map=self.name, op="begin"))
        after = self.s.drafts.get(self.name)
        self.assertEqual(graph_hash(after), before_hash)
        self.assertEqual(after["nodes"], before["nodes"])
        self.assertEqual(after["edges"], before["edges"])
        self.assertEqual(after["revision"], before["revision"])
        self.assertEqual(result["draft"]["sha256"], before_hash)

    def test_rejected_scanner_rows_survive_native_report_replacement_and_project_reload(self):
        self.s.normalize_generated_graph({"map":self.name})
        self.s.native_path(self.name).write_text(json.dumps(dict(map=self.name, candidate_mode="manual_draft", nodes=[],edges=[])))
        self.s.native_cached.clear()
        self.s.drafts = Drafts(self.s.work/"drafts")
        diagnosis = self.s.generation_diagnostics(self.name)
        self.assertTrue(diagnosis["current"])
        self.assertEqual([n["id"] for n in diagnosis["nodes"]], [7,8])
        self.assertEqual(diagnosis["nodes"][1]["diagnostic_kind"], "missing_floor_area")

    def test_stale_or_incomplete_input_does_not_promote_or_drop_any_node(self):
        self.s.proof_status.return_value["inputs"] = {"fixture":"changed"}
        with self.assertRaisesRegex(RuntimeError,"veraltet"):
            self.s.normalize_generated_graph({"map":self.name})
        self.assertIsNone(self.s.drafts.get(self.name))
        self.s.proof_status.return_value["inputs"] = {"fixture":"same"}
        self.report["native_input_coverage"]["capacity"]["input_validation_complete"] = False
        self.write_report()
        with self.assertRaisesRegex(RuntimeError,"Unvollständige"):
            self.s.normalize_generated_graph({"map":self.name})
        self.assertIsNone(self.s.drafts.get(self.name))

    def test_direct_check_of_raw_generation_checks_the_normalized_graph_too(self):
        self.report["generation_complete"] = True
        self.report["edges"].append(dict(**{"from":4,"to":3},accepted=True))
        self.write_report()
        self.s.action_load_map = Mock(); self.s.quiet_generation = Mock()
        self.s.require_loaded = Mock()
        self.s.require_bbox = Mock(return_value=MOVEMENT_PROFILES[1])
        self.s.require_profile_inputs = Mock()
        self.s.update = lambda job, **kw: job.update(kw)
        self.s.inspect = Mock(return_value={"actual":"final binary"})
        self.s.proof_status = lambda name, candidate=None, receipt=True: dict(
            valid=True, candidate_valid=True, reasons=[], inputs={"candidate":self.s.file_hash(candidate or self.s.candidate(name))})
        self.s.rcon_with_followup = Mock(return_value="import accepted")
        sources = []
        def fresh(path, *args):
            candidate = self.s.candidate(self.name)
            sources.append(candidate.name)
            report = copy.deepcopy(self.report)
            report["source"] = "navgen/"+candidate.name
            if candidate.name.endswith("_edit.txt"):
                report["candidate_mode"] = "manual_draft"
                report["nodes"] = [n for n in report["nodes"] if n["accepted"]]
                report["edges"] = [e for e in report["edges"] if e["accepted"]]
            path.write_text(json.dumps(report))
            path.with_suffix(".way").write_bytes(b"new binary "+candidate.name.encode())
            return report
        self.s.wait_fresh = fresh
        result = self.s.action_native_check(dict(map=self.name,action="native_check",log=""))
        self.assertEqual(sources,["obj_test_candidate.txt","obj_test_candidate_edit.txt"])
        proof = read_json(Path(result["proof"]))
        self.assertEqual(proof["candidate"],"obj_test_candidate_edit.txt")
        self.assertTrue(result["normalization"]["requires_native_validation"])
        self.assertEqual(result["audit"],{"actual":"final binary"})
        self.assertEqual([n["id"] for n in self.s.drafts.get(self.name)["nodes"]],[3,4])
        self.assertTrue(self.s.generation_diagnostics(self.name)["current"])

    def test_manual_incomplete_report_names_current_exit_gap_instead_of_unsupported_scanner(self):
        self.s.normalize_generated_graph({"map":self.name})
        self.s.require_loaded = Mock(); self.s.require_bbox = Mock(return_value=MOVEMENT_PROFILES[1])
        self.s.require_profile_inputs = Mock(); self.s.update = lambda job, **kw: job.update(kw)
        self.s.inspect = Mock(side_effect=AssertionError("No stale WAY audit"))
        self.s.proof_status = lambda name, candidate=None, receipt=True: dict(
            valid=True,candidate_valid=True,reasons=[],inputs={"candidate":self.s.file_hash(candidate or self.s.candidate(name))})
        self.s.rcon_with_followup = Mock(return_value="current manual report")
        def fresh(path,*args):
            report=copy.deepcopy(self.report);report["candidate_mode"]="manual_draft"
            report["source"]="navgen/"+self.s.candidate(self.name).name
            report["nodes"]=[n for n in report["nodes"] if n["accepted"]]
            report["edges"]=[e for e in report["edges"] if e["accepted"]]
            path.write_text(json.dumps(report));return report
        self.s.wait_fresh=fresh
        with self.assertRaisesRegex(RuntimeError,"1 gewöhnliche.*4.*Aktueller bearbeiteter Netzstand") as caught:
            self.s.action_native_check(dict(map=self.name,action="repair_check",log=""))
        self.assertNotIn("räumlicher Zwischenbericht",str(caught.exception))
        self.s.inspect.assert_not_called()
        self.assertFalse((self.s.work/(self.name+".native-proof.json")).exists())

    def test_standalone_repair_uses_normalized_current_ids_without_edit_mode(self):
        self.s.repair_manual_draft = Mock(return_value={"status":"candidate_requires_native_validation"})
        result = self.s.action_repair(dict(map=self.name))
        self.s.repair_manual_draft.assert_called_once()
        self.assertEqual(result["status"],"candidate_requires_native_validation")
        self.assertEqual([n["id"] for n in self.s.drafts.get(self.name)["nodes"]],[3,4])
        self.assertTrue(self.s.generation_diagnostics(self.name)["current"])

    def install_checked_floor_repair(self, x=256, complete=True, reverse=True):
        self.s.normalize_generated_graph({"map":self.name})
        report = dict(map=self.name,candidate_mode="manual_draft",generation_complete=complete,
                      bbox=self.report["bbox"],nodes=[dict(id=3,position=[96,0,30],accepted=True),
                                                    dict(id=900,position=[x,0,30],accepted=True)],
                      edges=[dict(**{"from":3,"to":900},accepted=True)])
        if reverse: report["edges"].append(dict(**{"from":900,"to":3},accepted=True))
        self.s.native_path(self.name).write_text(json.dumps(report))
        self.s.report_status = Mock(return_value={"current":True})

    def test_exact_checked_floor_repair_resolves_display_without_destroying_scanner_history(self):
        self.install_checked_floor_repair(x=256.015)
        diagnosis = self.s.generation_diagnostics(self.name)
        self.assertEqual(diagnosis["counts"]["connectivity_pending_nodes"],0)
        self.assertEqual(diagnosis["counts"]["connectivity_resolved_nodes"],1)
        self.assertEqual(diagnosis["scanner_counts"]["connectivity_pending_nodes"],1)
        floor = next(n for n in diagnosis["nodes"] if n["id"]==8)
        self.assertTrue(floor["diagnostic_resolved"])
        self.assertEqual(floor["resolved_by_source_ids"],[900])
        self.assertEqual(floor["position"],[256,0,0])
        self.assertFalse(diagnosis["full_map_verified"])

    def test_nearby_or_one_way_or_unchecked_edit_never_erases_missing_area(self):
        for x,complete,reverse,current in ((257,True,True,True),(256,False,True,True),
                                          (256,True,False,True),(256,True,True,False)):
            with self.subTest(x=x,complete=complete,reverse=reverse,current=current):
                if self.s.drafts.get(self.name):
                    self.s.drafts.archive(self.name,self.s.drafts.get(self.name)["revision"])
                self.write_report()
                self.s.report_status = Studio.report_status.__get__(self.s,Studio)
                self.install_checked_floor_repair(x=x,complete=complete,reverse=reverse)
                self.s.report_status.return_value={"current":current}
                diagnosis = self.s.generation_diagnostics(self.name)
                self.assertEqual(diagnosis["counts"]["connectivity_pending_nodes"],1)
                self.assertEqual(diagnosis["counts"]["connectivity_resolved_nodes"],0)

    def test_changed_archive_is_never_presented_as_current_diagnosis(self):
        result = self.s.normalize_generated_graph({"map":self.name})
        report = Path(read_json(Path(result["diagnostic_manifest"]))["native_report"])
        report.write_text("changed")
        diagnosis = self.s.generation_diagnostics(self.name)
        self.assertFalse(diagnosis["current"]); self.assertTrue(diagnosis["invalid"])

    def test_existing_authored_ids_and_flags_are_never_replaced_by_scanner_rows(self):
        self.s.drafts.start(self.name, dict(nodes=[dict(id=1000,position=[1,2,3],radius=35,nav_flags=1<<40)],edges=[]),
                            dict(environment=self.s.draft_environment(self.name)))
        before = copy.deepcopy(self.s.drafts.get(self.name))
        self.assertIsNone(self.s.normalize_generated_graph({"map":self.name}))
        self.assertEqual(self.s.drafts.get(self.name), before)


class GeneratedRepairFlowTests(TestCase):
    def fixture(self):
        s = Studio.__new__(Studio)
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        s.work=Path(temporary.name)
        s.update = lambda job, **kw: job.update(kw)
        s.action_load_map = Mock(); s.quiet_generation = Mock()
        s.action_rebuild = Mock(return_value={"counts":{"nodes":10000}})
        s.action_native_check = Mock(return_value=dict(audit={}, proof="checked"))
        s.action_repair = Mock(return_value={"repairs":{}})
        s.action_repair_check = Mock(return_value=dict(repair={'repairs':{}},audit={},proof='checked'))
        s.normalize_generated_graph = Mock(return_value=dict(counts={"accepted_nodes":800}, requires_native_validation=True))
        s.report_status = Mock(return_value={"current":True})
        s.reconcile_objective_anchors = Mock(return_value=None)
        s.complete_generated_project = Mock(return_value=dict(goals={},spawns={},
            generated_gametypes=[2,5],package_status={'state':'ready'}))
        s.install = Mock(side_effect=AssertionError("Never installs"))
        return s


class SharedGeneratedRepairLoopTests(TestCase):
    """Both entry paths exercise the real common retry coordinator, file-only."""
    def fixture(self,regenerate=False,unsafe=False):
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        root=Path(temporary.name);events=[];snapshots=[];phase={'normalized':False,'checks':0}
        studio=Studio.__new__(Studio);studio.work=root;studio.drafts=Drafts(root/'drafts');studio.hash_cached={}
        environment={'fixture':'same-generated-source'}
        studio.draft_environment=lambda name:environment
        studio.update=lambda job,**kwargs:job.update(kwargs)
        studio.path=lambda name,suffix:root/(name+suffix)
        report=root/'native.json';report.write_text('{}')
        studio.native_path=lambda name:report
        (root/'obj_test.generation-ready.json').write_text('{"old_unrelated_ready":true}')
        studio.native=Mock(return_value={})
        studio.report_status=Mock(return_value={'current':True})
        studio.prepare_draft=Mock(return_value=root/'candidate.txt')
        studio.action_load_map=Mock();studio.quiet_generation=Mock();studio.install=Mock()
        studio.recover_rejected_shortcuts=Mock(return_value=None)
        studio.recover_rejected_ordinary=Mock(return_value=None)
        studio.repair_pending_jump_draft=Mock(return_value=None)
        studio.repair_descent_draft=Mock(return_value=None)
        studio.reconcile_objective_anchors=Mock(return_value=None)
        if regenerate:
            studio.drafts.start('obj_test',dict(nodes=[dict(id=900,position=[400,400,0],radius=35)],edges=[]),
                dict(environment=environment))
        def build(job):events.append('rebuild');return {'counts':{'nodes':2}}
        studio.action_rebuild=Mock(side_effect=build)
        def normalize(job):
            events.append('normalize');phase['normalized']=True
            self.assertIsNone(studio.drafts.get('obj_test'))
            studio.drafts.start('obj_test',dict(nodes=[dict(id=i,position=[(i-1)*64,0,0],radius=35)for i in(1,2)],
                edges=[{'from':1,'to':2},{'from':2,'to':1}]),dict(environment=environment))
            return {'requires_native_validation':True,'accepted_node_ids':[1,2]}
        studio.normalize_generated_graph=Mock(side_effect=normalize)
        def repair(job,**kwargs):
            events.append('scanner_repair'if kwargs.get('scanner_only')else'common_repair')
            return {'repairs':{}}
        studio.action_repair=Mock(side_effect=repair)
        def check(job):
            if not phase['normalized']:
                events.append('scanner_check');return {'audit':{},'proof':'raw-scanner-proof'}
            phase['checks']+=1;events.append('common_check'+str(phase['checks']))
            snapshots.append(copy.deepcopy(studio.drafts.get('obj_test')))
            if phase['checks']<4 or unsafe:raise RuntimeError('still unsafe current generated graph')
            return {'audit':{'unresolved_nodes':[],'fresh':True},'proof':'full-current-draft-proof'}
        studio.action_native_check=Mock(side_effect=check)
        def posture(doc,native):
            if phase['checks']==1:
                events.append('posture');return {'required_crouch_flags':[2]}
            return None
        def path(job):
            self.assertEqual(job['action'],'repair_check')
            job['_native_path_search_attempted']=True;events.append('path')
            doc=studio.drafts.get('obj_test');proposal=copy.deepcopy(doc)
            identity=doc['next_id'];proposal['nodes'].append(dict(doc['nodes'][0],id=identity,position=[32,8,0]))
            proposal['next_id']=identity+1;proposal['edges']+=[{'from':1,'to':identity},{'from':identity,'to':2}]
            studio.drafts.apply_repair('obj_test',doc['revision'],environment,proposal)
            return {'added_node_ids':[identity],'native_revalidation_required':True}
        studio.repair_path_draft=Mock(side_effect=path)
        def anchor(job):
            self.assertEqual(job['action'],'repair_check')
            job['_native_anchor_search_attempted']=True;events.append('anchor')
            doc=studio.drafts.get('obj_test');proposal=copy.deepcopy(doc)
            next(n for n in proposal['nodes']if n['id']==2)['position'][1]=4
            studio.drafts.apply_repair('obj_test',doc['revision'],environment,proposal)
            return {'moves':[{'source_id':2}],'native_revalidation_required':True}
        studio.repair_anchor_draft=Mock(side_effect=anchor)
        def complete(job):
            self.assertEqual(job['action'],'generate')
            self.assertEqual(phase['checks'],4)
            self.assertFalse(unsafe);events.append('completion')
            self.assertFalse((root/'obj_test.generation-ready.json').exists())
            (root/'obj_test.generation-ready.json').write_text('{"proof":"full-current-draft-proof"}')
            return dict(goals={'checked':True},spawns={'checked':True},generated_gametypes=[2,5],
                package_status={'ready':True})
        studio.complete_generated_project=Mock(side_effect=complete)
        return studio,events,snapshots,posture

    def test_fresh_project_and_regeneration_use_same_posture_path_anchor_checks_before_completion(self):
        all_events=[];all_graphs=[]
        for regenerate in(False,True):
            with self.subTest(regenerate=regenerate):
                studio,events,snapshots,posture=self.fixture(regenerate)
                original=studio.action_repair_check
                studio.action_repair_check=Mock(wraps=original)
                job={'map':'obj_test','gametype':2,'action':'generate','log':''}
                with patch('repair.native_crouch_feedback',side_effect=posture):result=studio.action_generate(job)
                studio.action_repair_check.assert_called_once()
                self.assertIs(studio.action_repair_check.call_args.kwargs['reconcile'],False)
                self.assertEqual(job['action'],'generate')
                self.assertEqual(events[-1],'completion');studio.complete_generated_project.assert_called_once()
                self.assertEqual(result['proof'],'full-current-draft-proof')
                self.assertTrue(result['completion']['package_status']['ready'])
                self.assertEqual(read_json(studio.work/'obj_test.generation-ready.json')['proof'],'full-current-draft-proof')
                self.assertFalse(result['installed']);studio.install.assert_not_called()
                self.assertEqual(len(snapshots),4)
                self.assertEqual(next(n for n in snapshots[0]['nodes']if n['id']==2)['nav_flags'],0)
                self.assertEqual(next(n for n in snapshots[1]['nodes']if n['id']==2)['nav_flags'],64)
                self.assertEqual(len(snapshots[2]['nodes']),3)
                self.assertEqual(next(n for n in snapshots[3]['nodes']if n['id']==2)['position'],[64,4,0])
                all_events.append(events);all_graphs.append(studio.drafts.get('obj_test')['nodes'])
        self.assertEqual(all_events[0],all_events[1]);self.assertEqual(all_graphs[0],all_graphs[1])

    def test_rejected_generated_draft_never_reaches_goals_spawns_or_ready_completion(self):
        for regenerate in(False,True):
            with self.subTest(regenerate=regenerate):
                studio,events,_,posture=self.fixture(regenerate,unsafe=True)
                job={'map':'obj_test','gametype':2,'action':'generate','log':''}
                with patch('repair.native_crouch_feedback',side_effect=posture),self.assertRaisesRegex(RuntimeError,'still unsafe'):
                    studio.action_generate(job)
                studio.complete_generated_project.assert_not_called();studio.install.assert_not_called()
                self.assertNotIn('completion',events);self.assertEqual(job['action'],'generate')
                self.assertFalse((studio.work/'obj_test.generation-ready.json').exists())

class GeneratedRepairFlowContractTests(GeneratedRepairFlowTests):
    def test_generate_runs_polygon_feedback_then_the_same_native_action_repair_without_begin_edit(self):
        s = self.fixture()
        result = s.action_generate(dict(map="obj_test"))
        self.assertEqual(s.action_native_check.call_count,3)
        self.assertEqual(s.action_repair.call_count,1)
        self.assertTrue(s.action_repair.call_args_list[0].kwargs["scanner_only"])
        s.action_repair_check.assert_called_once()
        self.assertIs(s.action_repair_check.call_args.kwargs['reconcile'],False)
        self.assertEqual([stage["stage"] for stage in result["stages"]],
                         ["generated_and_checked","repair_and_check","accepted_network_draft","network_action_repair_and_check"])
        self.assertEqual(result["proof"],"checked")
        s.complete_generated_project.assert_called_once()
        s.install.assert_not_called()

    def test_current_authored_failure_gets_bounded_repair_then_full_recheck(self):
        s = self.fixture()
        s.action_native_check.side_effect = [dict(audit={},proof="raw"),dict(audit={},proof="raw2"),
                                             RuntimeError("Bearbeitete Navigation nicht übernommen")]
        s.action_repair_check.return_value=dict(repair={'repairs':{}},audit={},proof='actual repaired')
        result = s.action_generate(dict(map="obj_test"))
        self.assertEqual(result["proof"],"actual repaired")
        self.assertEqual(result["stages"][2]["status"],"authored_graph_requires_repair")
        self.assertNotIn("proof",result["stages"][2])
        s.complete_generated_project.assert_called_once()

    def test_stale_authored_failure_is_not_a_repair_authorization(self):
        s = self.fixture();s.report_status.return_value={"current":False}
        s.action_native_check.side_effect=[dict(audit={},proof="raw"),dict(audit={},proof="raw2"),RuntimeError("changed geometry")]
        with self.assertRaisesRegex(RuntimeError,"changed geometry"):
            s.action_generate(dict(map="obj_test"))
        self.assertEqual(s.action_repair.call_count,1)
        s.action_repair_check.assert_not_called();s.complete_generated_project.assert_not_called()
        s.install.assert_not_called()

    def test_open_accepted_node_stays_failed_after_noop_repair_without_using_raw_proof(self):
        s = self.fixture()
        s.action_native_check.side_effect=[dict(audit={},proof="raw"),dict(audit={},proof="raw2"),
                                           RuntimeError("accepted node4 without exit")]
        s.action_repair_check.side_effect=RuntimeError('accepted node4 without exit')
        with self.assertRaisesRegex(RuntimeError,"node4 without exit"):
            s.action_generate(dict(map="obj_test"))
        self.assertEqual(s.action_repair.call_count,1)
        s.action_repair_check.assert_called_once();s.complete_generated_project.assert_not_called()
        s.install.assert_not_called()


if __name__ == "__main__": main()
