"""Changing paths must never launch, copy, or commandeer an existing game."""
import copy
import json
from pathlib import Path
import tempfile
import threading
import unittest
from unittest.mock import Mock, patch
from settings import validate_settings, save_settings
from server import Studio
from edit import Drafts


class SettingsTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        (self.root / "tce2").mkdir()
        (self.root / "etmain").mkdir()
        (self.root / "ET.exe").write_bytes(b"not executed")
        self.value = dict(instance_path=str(self.root), game_executable=str(self.root / "ET.exe"), maps_dirs=[])

    def test_existing_installation_only_and_round_trip(self):
        data = validate_settings(self.value)
        path = self.root / "navstudio-settings.json"
        save_settings(path, data)
        self.assertEqual(json.loads(path.read_text()), data)
        self.assertFalse(path.with_suffix(".tmp").exists())

    def test_wrong_executable_outside_installation_and_relative_paths_rejected(self):
        for field, value in (("game_executable", str(self.root / "not-exist.exe")),
                             ("game_executable", str(self.root / "tce2" / "ET.exe")),
                             ("game_executable", "ET.exe"), ("instance_path", "relative"),
                             ("maps_dirs", [str(self.root / "missing")])):
            with self.subTest(field=field, value=value), self.assertRaises(ValueError):
                validate_settings({**self.value, field: value})

    def test_busy_or_running_game_keeps_settings_unchanged(self):
        studio = Studio.__new__(Studio)
        studio.lock = threading.RLock()
        for busy, processes in ((True, []), (False, [{"pid": 123}])):
            studio.busy = busy
            studio.test_processes = Mock(return_value=processes)
            with patch("server.save_settings") as save, self.assertRaises(RuntimeError):
                studio.configure(self.value)
            save.assert_not_called()

    def test_target_game_is_not_adopted(self):
        studio = Studio.__new__(Studio)
        studio.lock = threading.RLock()
        studio.busy = False
        studio.drafts = Drafts(self.root/'drafts')
        studio.test_processes = Mock(side_effect=[[], [{"pid": 987}]])
        with patch("server.save_settings") as save, self.assertRaisesRegex(RuntimeError, "bereits ein Spiel"):
            studio.configure(self.value)
        save.assert_not_called()

    def test_unsaved_drafts_block_even_identical_or_map_directory_only_settings(self):
        studio = Studio.__new__(Studio)
        studio.lock = threading.RLock()
        studio.busy = False
        studio.test_processes = Mock(return_value=[])
        studio.drafts = Drafts(self.root/'drafts')
        settings_path = self.root/'navstudio-settings.json'
        settings_path.write_text('existing settings sentinel')
        studio.settings_file = settings_path
        directory = self.root/'extra maps'; directory.mkdir()
        for revision, saved in ((0, None), (3, 2)):
            for payload in (self.value, {**self.value, 'maps_dirs': [str(directory)]}):
                with self.subTest(revision=revision, saved=saved, payload=payload):
                    studio.drafts.documents = {'obj_delta': dict(revision=revision, saved_revision=saved,
                                                                nodes=[{'id': 41}], undo=[{'edit': 'keep'}])}
                    original = studio.drafts
                    contents = copy.deepcopy(original.documents)
                    with patch('server.save_settings') as save, patch('server.Assets') as assets, \
                         self.assertRaisesRegex(RuntimeError, 'speichern oder archivieren'):
                        studio.configure(payload)
                    save.assert_not_called(); assets.assert_not_called()
                    self.assertIs(studio.drafts, original)
                    self.assertEqual(studio.drafts.documents, contents)
                    self.assertEqual(settings_path.read_text(), 'existing settings sentinel')
                    self.assertFalse((self.root/'navmesh-preview').exists())

    def test_clean_or_archived_drafts_allow_settings_update(self):
        for documents in ({}, {'obj_delta': dict(revision=2, saved_revision=2)}):
            with self.subTest(documents=documents):
                studio = Studio.__new__(Studio)
                studio.lock = threading.RLock()
                studio.busy = False
                studio.test_processes = Mock(return_value=[])
                studio.drafts = Drafts(self.root/'drafts')
                studio.drafts.documents = copy.deepcopy(documents)
                studio.workspace = self.root
                studio.game_mode = 'tce2'
                studio.settings_file = self.root/'navstudio-settings.json'
                studio.process_api = Mock()
                studio.native_cached, studio.hash_cached = {}, {}
                studio.find_recast = Mock(return_value=None)
                studio.selected_map = 'obj_delta'
                studio.settings = Mock(return_value={'saved': True})
                with patch('server.Assets', return_value=Mock(maps={})):
                    self.assertEqual(studio.configure(self.value), {'saved': True})
                self.assertIsNone(studio.process_api)
                self.assertEqual(json.loads(studio.settings_file.read_text()), {**validate_settings(self.value), 'game_mode': 'tce2'})


if __name__ == "__main__":
    unittest.main()
