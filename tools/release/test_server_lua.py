"""Release regressions for the reviewed WolfAdmin/default-script closure."""
import json
from pathlib import Path
import subprocess
import tempfile
import unittest

from server_lua import MANIFEST, payload_paths, stage_server_lua


class ServerLuaPayload(unittest.TestCase):
    def fixture(self, root, names):
        (root / 'tools/release').mkdir(parents=True)
        (root / MANIFEST).write_text(json.dumps({'schema': 1, 'files': sorted(names)}))
        for name in names:
            path = root / 'src/lua' / name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_text('# default' if name.endswith('.toml') else '-- default')

    def test_tracked_defaults_complete_and_all_five_tomls_are_reviewed(self):
        root = Path(__file__).resolve().parents[2]
        actual = {name for name, _ in payload_paths(root)}
        tracked = {name.removeprefix('src/lua/') for name in subprocess.check_output(
            ['git', 'ls-files', '-z', 'src/lua'], cwd=root).decode().split('\0') if name}
        self.assertEqual(actual, tracked)
        self.assertEqual({name for name in actual if name.endswith('.toml')}, {
            'wolfadmin/config/' + name + '.toml'
            for name in ('banners', 'greetings', 'rules', 'sprees', 'wolfadmin')})

    def test_defaults_staged_but_local_config_database_and_logs_never_copied(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            names = ['wolfadmin/main.lua', 'wolfadmin/enable.cfg',
                     'wolfadmin/config/wolfadmin.toml', 'wolfadmin/database/sqlite.sql']
            self.fixture(root, names)
            for name in ('wolfadmin/config/private.toml', 'wolfadmin/rcon.cfg',
                         'wolfadmin/database/wolfadmin.db', 'wolfadmin/chat.log'):
                path = root / 'src/lua' / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('LOCAL STATE MUST NOT SHIP')
            target = root / 'stage/lua'
            self.assertEqual(stage_server_lua(root, target), sorted(names))
            self.assertEqual({p.relative_to(target).as_posix() for p in target.rglob('*')
                              if p.is_file()}, set(names))

    def test_missing_default_fails_before_any_copy(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            self.fixture(root, ['a.lua', 'z.toml'])
            (root / 'src/lua/z.toml').unlink()
            with self.assertRaisesRegex(ValueError, 'Missing/escaped'):
                stage_server_lua(root, root / 'stage')
            self.assertFalse((root / 'stage').exists())

    def test_manifest_cannot_export_parent_paths_absolute_paths_or_databases(self):
        for name in ('../private.cfg', '/private.cfg', 'C:\\private.cfg', 'C:/private.cfg', 'wolfadmin.db'):
            with self.subTest(name=name), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary)
                (root / 'tools/release').mkdir(parents=True)
                (root / MANIFEST).write_text(json.dumps({'schema': 1, 'files': [name]}))
                with self.assertRaisesRegex(ValueError, 'Unsafe'):
                    stage_server_lua(root, root / 'stage')


if __name__ == '__main__':
    unittest.main()
