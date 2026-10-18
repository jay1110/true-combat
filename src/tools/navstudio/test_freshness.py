"""Changed drafts/new candidates never display or repair an earlier check's IDs."""
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import threading
from unittest import TestCase, main
from unittest.mock import Mock

from edit import Drafts
from server import Studio,imported_way_ids


class FreshnessTests(TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.s = Studio.__new__(Studio)
        for field in ("work", "preview", "runtime"):
            value=self.root/field;value.mkdir();setattr(self.s,field,value)
        self.s.drafts=Drafts(self.s.work/"drafts")
        self.s.hash_cached={};self.s.native_cached={}
        self.s.require_map=lambda n:n
        self.s.draft_environment=lambda n:{"bsp":"same"}
        self.s.proof_status=Mock(return_value=dict(valid=True,candidate_valid=True,reasons=[],inputs={"candidate":"first"}))
        self.s.imported_navigation=Mock(return_value=None)
        self.s.spawn_layer=Mock(return_value={})
        self.s.lock=threading.RLock();self.s.busy=False
        self.name="obj_test"
        self.candidate=self.s.path(self.name,"_candidate.txt")
        self.candidate.write_text("first candidate")
        self.candidate.with_suffix(".json").write_text(json.dumps(dict(map=self.name,nodes=[dict(id=5,point=[0,0,0])],edges=[])))
        self.report=dict(map=self.name,source="navgen/obj_test_candidate.txt",generation_complete=True,
                         nodes=[dict(id=5,position=[0,0,30],raw_position=[0,0,30],nav_flags=0,accepted=True,radius=35)],edges=[])
        self.s.native_path(self.name).write_text(json.dumps(self.report))
        self.bind()

    def bind(self):
        record=dict(candidate=self.s.candidate(self.name).name,inputs=self.s.proof_status.return_value["inputs"],
                    native_report=self.s.file_hash(self.s.native_path(self.name)))
        (self.s.work/(self.name+".native-attempt.json")).write_text(json.dumps(record))
        self.s.path(self.name,".native-audit.json").write_text(json.dumps(dict(unresolved_nodes=[dict(source_id=5)],report_binding=record)))

    def test_live_way_new_nodes_do_not_reuse_compacted_source_ids(self):
        binary=[dict(properties={"tce_node":"5"},uid=2),dict(properties={"tce_node":"9"},uid=3),
                dict(properties={},uid=10),dict(properties={"tce_node":"5"},uid=11)]
        self.assertEqual(imported_way_ids(binary),[5,9,12,13])

    def test_live_way_save_promotes_exact_graph_and_invalidates_old_candidates(self):
        way=self.root/"new.way";way.write_bytes(b"fresh native WAY")
        self.s.nav_bundles=Mock(return_value=SimpleNamespace(path=lambda *args:way))
        self.s.imported_navigation.return_value=dict(nodes=[dict(id=12,position=[32,32,0],radius=35),dict(id=13,position=[64,32,0],radius=35)],
            edges=[dict(**{"from":12,"to":13})],source_sha256=self.s.file_hash(way))
        self.s.recast_body_profile=Mock(side_effect=RuntimeError("not captured"))
        self.s.live_way_saved(self.name)
        self.assertIsNone(self.s.candidate(self.name))
        self.assertEqual([n["id"] for n in self.s.drafts.get(self.name)["nodes"]],[12,13])
        self.assertEqual(self.s.drafts.get(self.name)["saved_revision"],0)
        self.assertFalse((self.s.work/(self.name+".native-attempt.json")).is_file())

    def test_unsaved_deletion_immediately_hides_old_report_and_old_error(self):
        self.s.drafts.start(self.name,dict(nodes=[dict(id=5,position=[0,0,0],radius=35),dict(id=9,position=[32,0,0],radius=35)],edges=[]),
                            dict(environment=self.s.draft_environment(self.name)))
        self.s.drafts.change(self.name,dict(op="delete_node",id=5,revision=0),self.s.draft_environment(self.name))
        self.assertIsNone(self.s.candidate(self.name))
        scene=self.s.scene(self.name,meshes=False)
        self.assertEqual(scene["navigation"]["source"],"draft")
        self.assertEqual([n["id"] for n in scene["navigation"]["nodes"]],[9])
        self.assertIsNone(scene["audit"])
        self.assertFalse(scene["report_status"]["current"])
        self.assertEqual(scene["native_summary"],{})

    def test_regeneration_selection_beats_older_repair_with_newer_mtime(self):
        repair=self.s.path(self.name,"_candidate_repair.txt")
        repair.write_text("oldrepair");repair.with_suffix(".json").write_text("{}")
        self.s.select_candidate(self.name,self.candidate)
        self.assertEqual(self.s.candidate(self.name),self.candidate)
        # After a new graph is selected, a still-existing old audit is historical.
        self.s.proof_status.return_value["inputs"]={"candidate":"second"}
        scene=self.s.scene(self.name,meshes=False)
        self.assertEqual(scene["navigation"]["source"],"candidate")
        self.assertIsNone(scene["audit"])
        self.assertEqual(scene["audit_state"],"stale")

    def test_fresh_failed_report_keeps_own_feedback_without_old_way_acceptance(self):
        self.report["generation_complete"]=False
        self.report["nodes"][0]["accepted"]=False
        self.report["nodes"][0]["reason"]="blocked_clearance"
        self.s.native_path(self.name).write_text(json.dumps(self.report))
        self.bind()
        scene=self.s.scene(self.name,meshes=False)
        self.assertTrue(scene["report_status"]["current"])
        self.assertEqual(scene["status"],"native_failed")
        self.assertEqual(scene["navigation"]["nodes"][0]["reason"],"blocked_clearance")
        self.assertEqual(scene["audit"]["unresolved_nodes"],[dict(source_id=5)])

    def test_current_check_prepares_latest_revision_without_archive_or_old_candidate(self):
        env=self.s.draft_environment(self.name)
        self.s.drafts.start(self.name,dict(nodes=[dict(id=5,position=[0,0,0],radius=35)],edges=[]),dict(environment=env))
        self.s.drafts.change(self.name,dict(op="update_node",id=5,position=[12,0,0],revision=0),env)
        path=self.s.prepare_draft(self.name)
        metadata=json.loads(path.with_suffix(".json").read_text())
        self.assertEqual(metadata["edit_draft"]["revision"],1)
        self.assertEqual(metadata["nodes"][0]["point"],[12,0,0])
        self.assertEqual(self.s.candidate(self.name),path)
        self.assertEqual(self.s.drafts.get(self.name)["saved_revision"],1)

    def test_project_native_and_snapshots_do_not_read_previous_maps_global_report(self):
        self.s.project_native=self.root/"newproject";self.s.project_native.mkdir()
        self.assertFalse(self.s.native_path(self.name).exists())
        self.assertEqual(self.s.native(self.name),None)
        self.assertEqual(self.s.snapshot_path(self.name).parent,self.s.project_native)
        self.assertEqual(self.s.runtime_native_path(self.name).parent,self.s.runtime)


if __name__=="__main__":main()
