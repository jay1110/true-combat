"""Release discovery must follow CMake TARGET_FILE with either generator type."""
import json
from pathlib import Path
import tempfile
import unittest

from package import module_inputs


class ModuleDiscovery(unittest.TestCase):
    def test_single_and_multi_config_exact_targets(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for config_directory in ('bin', 'bin/Release'):
                folder = root / config_directory
                folder.mkdir(parents=True, exist_ok=True)
                # An existing config directory does not establish output location.
                (root / 'bin/Release').mkdir(exist_ok=True)
                (root / 'bin/Release/tce-build-variant.txt').write_text('game\n')
                paths = {}
                for name in ('cgame', 'ui', 'qagame'):
                    path = folder / (name + '.mp.x86_64.so')
                    path.write_bytes(b'target fixture')
                    paths[name] = str(path)
                manifest = root / 'tce-modules-Release.json'
                manifest.write_text(json.dumps({'schema': 1, 'variant': 'game', 'modules': paths}))
                found = module_inputs(None, {'suffix': '.mp.x86_64.so'}, 'game', manifest)
                self.assertEqual(found, {name: Path(path).resolve() for name, path in paths.items()})

    def test_missing_wrong_role_and_wrong_name_are_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            manifest = root / 'tce-modules-Release.json'
            paths = {name: str(root / (name + '.mp.x86_64.so')) for name in ('cgame', 'ui', 'qagame')}
            metadata = {'schema': 1, 'variant': 'game', 'modules': paths}
            manifest.write_text(json.dumps(metadata))
            with self.assertRaises(ValueError):
                module_inputs(None, {'suffix': '.mp.x86_64.so'}, 'game', manifest)
            for path in paths.values():
                Path(path).write_bytes(b'fixture')
            with self.assertRaises(ValueError):
                module_inputs(None, {'suffix': '.mp.x86_64.so'}, 'navstudio', manifest)
            with self.assertRaises(ValueError):
                module_inputs(None, {'suffix': '.mp.i386.so'}, 'game', manifest)


if __name__ == '__main__':
    unittest.main()
