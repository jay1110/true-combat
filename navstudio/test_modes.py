"""Game mode boundaries must never substitute TCE2 modules for original ET."""
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest import TestCase, main
from unittest.mock import Mock, patch
import json
from server import Studio
from edit import Drafts


class ModeTests(TestCase):
    def test_map_prefix_partition(self):
        studio = Studio.__new__(Studio)
        for mode, expected in [('tce2', ['obj_delta', 'OBJ_test']),
                               ('et', ['goldrush', 'battery', 'my_obj_test'])]:
            studio.game_mode = mode
            names = ['obj_delta', 'goldrush', 'battery', 'OBJ_test', 'my_obj_test']
            self.assertEqual([n for n in names if studio.map_in_mode(n)], expected)

    def test_mode_uses_original_modules_and_separate_work_directory(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            studio = Studio.__new__(Studio)
            studio.test = studio.workspace = root
            studio.game_executable = root / 'ET.exe'
            studio.maps_dirs = []
            studio.drafts = Drafts(root / 'old-drafts')
            studio.settings_file = root / 'settings.json'
            studio.settings_file.write_text(json.dumps({'instance_path': str(root)}))
            studio.own_test_process = Mock(return_value=None)
            studio.native_cached = {}; studio.hash_cached = {}
            mod = root / 'omnibot'; mod.mkdir()
            module = mod / 'qagame_mp_x86.dll'; module.write_bytes(b'original ET')
            with patch('server.Assets') as assets:
                assets.return_value.maps = {'obj_delta': {}, 'goldrush': {}}
                result = studio.change_mode({}, 'et')
            self.assertEqual(result['fs_game'], 'omnibot')
            self.assertEqual(studio.selected_map, '')
            self.assertEqual(studio.runtime, mod / 'navgen')
            self.assertEqual(studio.preview, root / 'navmesh-preview-et')
            self.assertEqual(module.read_bytes(), b'original ET')
            settings = json.loads(studio.settings_file.read_text())
            self.assertEqual(settings['game_mode'], 'et')
            self.assertEqual(settings['instance_path'], str(root))

    def test_missing_original_module_never_falls_back_to_tce2(self):
        with TemporaryDirectory() as directory:
            studio = Studio.__new__(Studio)
            studio.test = Path(directory)
            studio.drafts = Drafts(studio.test / 'drafts')
            with self.assertRaisesRegex(RuntimeError, 'Game module missing'):
                studio.change_mode({}, 'et')


if __name__ == '__main__':
    main()
