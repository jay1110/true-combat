"""Projects isolate every map revision and never overwrite library packages."""
import json
from pathlib import Path
import tempfile
from unittest import TestCase, main
import zipfile
from projects import Projects, sha256, write_json


class ProjectTests(TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.library = self.root / "workspace" / "etmain" / "all_maps"
        self.etmain = self.root / "game" / "etmain"
        self.library.mkdir(parents=True); self.etmain.mkdir(parents=True)
        self.package = self.library / "obj_test.pk3"
        with zipfile.ZipFile(self.package, "w") as archive:
            archive.writestr("maps/obj_test.bsp", b"test map")
            archive.writestr("maps/goldrush.bsp", b"normal ET map")
        self.manager = Projects(self.root / "game" / "projects", self.etmain, [self.library])

    def create(self, name="Test", map_name="obj_test", mod="tce2"):
        catalog, _ = self.manager.catalog()
        item = next(v for v in catalog if v["map"] == map_name)
        return self.manager.create(name, map_name, mod, 5, item["source_id"])

    def test_named_project_leases_only_selected_archive_and_returns_exact_bytes(self):
        original = self.package.read_bytes()
        active = self.create("Village routes")
        self.assertEqual(active["name"], "Village routes")
        self.assertFalse(self.package.exists())
        target = self.etmain / self.package.name
        self.assertEqual(target.read_bytes(), original)
        self.assertTrue(active["lease_pending"])
        self.manager.save(name="Renamed project")
        self.assertTrue(target.exists(), "Save keeps an open project's map loaded")
        self.manager.close()
        self.assertEqual(self.package.read_bytes(), original)
        self.assertFalse(target.exists())
        self.assertIsNone(self.manager.active)
        self.assertEqual(self.manager.document(active["id"])["name"], "Renamed project")

    def test_same_map_in_two_projects_does_not_share_drafts_or_reports(self):
        first = self.create("First")
        draft = Path(first["paths"]["drafts"]) / "obj_test.draft.json"
        draft.write_text("first edits")
        report = Path(first["paths"]["native"]) / "obj_test_navmesh.json"
        report.write_text("old report")
        self.manager.close()
        second = self.create("Second")
        self.assertNotEqual(first["id"], second["id"])
        self.assertFalse((Path(second["paths"]["drafts"]) / draft.name).exists())
        self.assertFalse((Path(second["paths"]["native"]) / report.name).exists())
        self.manager.close()
        opened = self.manager.open(first["id"])
        self.assertEqual(Path(opened["paths"]["drafts"]).joinpath(draft.name).read_text(), "first edits")

    def test_target_conflict_never_overwrites_installed_pk3(self):
        target = self.etmain / self.package.name
        target.write_bytes(b"different installed map")
        with self.assertRaisesRegex(RuntimeError, "anderes Paket"):
            self.create()
        self.assertEqual(target.read_bytes(), b"different installed map")
        self.assertTrue(self.package.exists())
        self.assertIsNone(self.manager.active)

    def test_return_conflict_keeps_owned_package_and_project_open(self):
        active = self.create()
        self.package.write_bytes(b"different returned map")
        with self.assertRaisesRegex(RuntimeError, "anderes Paket"):
            self.manager.close()
        self.assertEqual(self.package.read_bytes(), b"different returned map")
        self.assertEqual(self.manager.active["id"], active["id"])
        self.assertTrue((self.etmain / self.package.name).exists())

    def test_modified_leased_archive_is_not_moved_or_deleted(self):
        active = self.create()
        target = self.etmain / self.package.name
        target.write_bytes(b"externally modified")
        with self.assertRaisesRegex(RuntimeError, "wurde geändert"):
            self.manager.close()
        self.assertEqual(target.read_bytes(), b"externally modified")
        self.assertFalse(self.package.exists())
        self.assertTrue(Path(active["paths"]["root"]).joinpath("lease.json").exists())

    def test_startup_does_not_auto_open_or_return_potentially_running_archive(self):
        active = self.create()
        fresh = Projects(self.manager.directory, self.etmain, [self.library])
        self.assertIsNone(fresh.active)
        self.assertEqual(fresh.recovery()[0]["id"], active["id"])
        self.assertFalse(self.package.exists())
        reopened = fresh.open(active["id"])
        self.assertEqual(reopened["id"], active["id"])
        fresh.close()
        self.assertTrue(self.package.exists())

    def test_recovery_handles_crash_between_hard_link_and_source_unlink(self):
        active = self.create()
        target = self.etmain / self.package.name
        # Reproduce both directory entries existing with exact owned bytes.
        import os
        os.link(target, self.package)
        journal = Path(active["paths"]["root"]) / "lease.json"
        lease = json.loads(journal.read_text()); lease["phase"] = "linked"
        write_json(journal, lease)
        fresh = Projects(self.manager.directory, self.etmain, [self.library])
        fresh.open(active["id"]); fresh.close()
        self.assertTrue(self.package.exists()); self.assertFalse(target.exists())
        self.assertFalse(journal.exists())

    def test_installed_archives_remain_in_place(self):
        target = self.etmain / self.package.name
        self.package.rename(target)
        catalog, _ = self.manager.catalog({"obj_test": {"path": target}})
        item = next(v for v in catalog if v["map"] == "obj_test")
        active = self.manager.create("Installed", "obj_test", "tce2", 2, item["source_id"])
        self.manager.close()
        self.assertTrue(target.exists())
        self.assertFalse(active["lease_pending"])

    def test_map_mod_mismatch_and_source_id_spoof_are_rejected(self):
        catalog, _ = self.manager.catalog()
        item = next(v for v in catalog if v["map"] == "obj_test")
        with self.assertRaises(ValueError):
            self.manager.create("bad", "obj_test", "et", 2, item["source_id"])
        with self.assertRaises(ValueError):
            self.manager.create("bad", "obj_test", "tce2", 2, "../../anything")
        with self.assertRaises(ValueError):
            self.manager.open("../../outside")

    def test_catalog_records_tce_and_et_sources_from_same_package(self):
        catalog, warnings = self.manager.catalog()
        self.assertFalse(warnings)
        self.assertEqual([(v["map"], v["mod"]) for v in catalog], [("goldrush", "et"), ("obj_test", "tce2")])
        self.assertTrue(all(v["location"] == "all_maps" for v in catalog))

    def test_interrupted_prepare_is_resumed_without_overwrite(self):
        active = self.create()
        self.manager.close()
        document = self.manager.document(active["id"])
        journal = self.manager.path(active["id"]) / "lease.json"
        write_json(journal, dict(schema=1,owner=active["id"],source=str(self.package),
            target=str(self.etmain/self.package.name),sha256=sha256(self.package),phase="prepared"))
        self.manager.open(active["id"])
        self.assertFalse(self.package.exists())
        self.manager.close()
        self.assertTrue(self.package.exists())

    def test_crashed_lease_cannot_be_borrowed_by_a_different_project(self):
        active = self.create()
        fresh = Projects(self.manager.directory, self.etmain, [self.library])
        target = self.etmain / self.package.name
        catalog, _ = fresh.catalog({"obj_test": {"path": target}})
        item = next(v for v in catalog if v["map"] == "obj_test")
        with self.assertRaisesRegex(RuntimeError, "anderen Projekt"):
            fresh.create("Other", "obj_test", "tce2", 5, item["source_id"])
        fresh.open(active["id"]); fresh.close()
        self.assertTrue(self.package.exists())


class StudioProjectTests(TestCase):
    def setUp(self):
        from types import SimpleNamespace
        from unittest.mock import Mock
        from server import Studio
        self.temp = tempfile.TemporaryDirectory(); self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for directory in ("etmain/all_maps", "tce2", "omnibot"):
            (self.root / directory).mkdir(parents=True)
        for mod in ("tce2", "omnibot"):
            (self.root/mod/"qagame_mp_x86.dll").write_bytes(b"fake module, never launched")
        package = self.root / "etmain/all_maps/obj_test.pk3"
        with zipfile.ZipFile(package, "w") as archive:
            archive.writestr("maps/obj_test.bsp", b"fake BSP")
        self.studio = Studio(SimpleNamespace(workspace=str(self.root),settings_file=self.root/"settings.json",
                            maps_dir=[],rcon_port=27986))
        self.addCleanup(self.studio.executor.shutdown)
        self.studio.own_test_process = Mock(return_value=None)
        self.studio.stop_owned_test = Mock()

    def create(self):
        item = self.studio.project_state()["catalog"][0]
        return self.studio.project(dict(op="create",name="Routes",map="obj_test",mod="tce2",
                                       gametype=5,source_id=item["source_id"]))["active"]

    def test_create_open_close_bind_real_studio_directories_and_native_results(self):
        active = self.create()
        self.assertEqual(self.studio.selected_map, "obj_test")
        self.assertEqual(self.studio.preview, Path(active["paths"]["preview"]))
        self.assertEqual(self.studio.work, Path(active["paths"]["work"]))
        self.assertEqual(self.studio.drafts.directory, Path(active["paths"]["drafts"]))
        self.assertEqual(self.studio.native_path("obj_test").parent, Path(active["paths"]["native"]))
        self.assertEqual(self.studio.nav_bundles().root, Path(active["paths"]["bundles"]))
        with self.assertRaisesRegex(ValueError, "anderen Projekt"):
            self.studio.require_map("obj_other")
        self.studio.project(dict(op="close"))
        self.assertIsNone(self.studio.active_project)
        self.assertEqual(self.studio.selected_map, "")
        self.assertTrue((self.root/"etmain/all_maps/obj_test.pk3").exists())
        reopened = self.studio.project(dict(op="open",id=active["id"]))["active"]
        self.assertEqual(reopened["paths"], active["paths"])

    def test_close_stops_exact_owned_game_before_returning_map(self):
        self.create()
        process = dict(pid=123,owns_port=True)
        self.studio.own_test_process.return_value = process
        def stop(job, actual):
            self.assertEqual(actual, process)
            self.assertTrue((self.root/"etmain/obj_test.pk3").exists())
            self.assertFalse((self.root/"etmain/all_maps/obj_test.pk3").exists())
        self.studio.stop_owned_test.side_effect = stop
        self.studio.project(dict(op="save_close"))
        self.studio.stop_owned_test.assert_called_once()
        self.assertTrue((self.root/"etmain/all_maps/obj_test.pk3").exists())

    def test_failed_game_close_keeps_project_and_lease_active(self):
        active = self.create()
        self.studio.own_test_process.return_value = dict(pid=123,owns_port=True)
        self.studio.stop_owned_test.side_effect = RuntimeError("cannot close owned game")
        with self.assertRaisesRegex(RuntimeError, "cannot close"):
            self.studio.project(dict(op="close"))
        self.assertEqual(self.studio.active_project["id"], active["id"])
        self.assertTrue((self.root/"etmain/obj_test.pk3").exists())
        self.assertFalse((self.root/"etmain/all_maps/obj_test.pk3").exists())


if __name__ == "__main__":
    main()
