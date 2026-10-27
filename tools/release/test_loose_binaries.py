"""Synthetic packaging tests: no target compiler, game launch or network access."""
import hashlib
import json
from pathlib import Path
import shutil
import struct
import tempfile
import unittest
from unittest.mock import patch
import zipfile

from build_loose import build_commands
import loose_binaries as loose
from validate import workflows


CORE_NAMES = {
    'windows-x86': 'omnibot_et.dll', 'windows-x64': 'omnibot_et_x64.dll',
    'linux-i386': 'omnibot_et.so', 'linux-x86_64': 'omnibot_et.x86_64.so',
    'linux-armv7': 'omnibot_et.armv7.so', 'linux-aarch64': 'omnibot_et.aarch64.so',
    'android-armv7': 'omnibot_et.android.armeabi-v7a.so',
    'android-arm64': 'omnibot_et.android.arm64-v8a.so',
    'wasm32': 'omnibot_et.wasm32.so', 'macos-universal': 'omnibot_et_mac.so',
}


def binary_header(target):
    """Structural ABI carriers, not executable library fixtures."""
    if target.startswith('windows-'):
        data = bytearray(256)
        data[:2] = b'MZ'
        struct.pack_into('<I', data, 60, 128)
        data[128:132] = b'PE\0\0'
        struct.pack_into('<H', data, 132, 0x14c if target == 'windows-x86' else 0x8664)
        struct.pack_into('<H', data, 150, 0x2000)
        struct.pack_into('<H', data, 152, 0x10b if target == 'windows-x86' else 0x20b)
        return bytes(data)
    if target == 'wasm32':
        return b'\0asm\x01\0\0\0'
    if target.startswith('macos-'):
        return (b'\xca\xfe\xba\xbe' + struct.pack('>I', 2)
                + struct.pack('>IIIII', 0x01000007, 3, 48, 1, 0)
                + struct.pack('>IIIII', 0x0100000c, 0, 49, 1, 0) + b'xx')
    data = bytearray(64)
    data[:4] = b'\x7fELF'
    data[4:6] = bytes((2 if target in ('linux-x86_64', 'linux-aarch64', 'android-arm64') else 1, 1))
    struct.pack_into('<H', data, 16, 3)
    struct.pack_into('<H', data, 18, {
        'linux-i386': 3, 'linux-x86_64': 62, 'linux-armv7': 40, 'linux-aarch64': 183,
        'android-armv7': 40, 'android-arm64': 183}[target])
    return bytes(data)


def fixture_boundary(library, variant, symbol_map=None):
    # Symbol adjudication has its own tests. This packaging fixture establishes
    # that both staging and collection invoke it with the exact supplied bytes.
    return {'variant': variant, 'sha256': loose.digest(library), 'fixture': True}


def check_fixture_boundary(evidence, data):
    if evidence != {'variant': 'game', 'sha256': hashlib.sha256(data).hexdigest(), 'fixture': True}:
        raise ValueError('Wrong core evidence in packaging fixture')
    return True


class LooseBinaries(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.inputs = self.root / 'collected'
        self.output = self.root / 'final'
        self.configs = loose.target_table()
        verifier = patch.object(loose, 'verify_core', side_effect=fixture_boundary)
        self.verify = verifier.start()
        self.addCleanup(verifier.stop)
        validator = patch.object(loose, 'validate_symbol_evidence', side_effect=check_fixture_boundary)
        self.validate = validator.start()
        self.addCleanup(validator.stop)

    def target(self, target, core_name=None):
        config = self.configs[target]
        source = self.root / 'built' / target
        source.mkdir(parents=True)
        magic = binary_header(target)
        paths = {}
        for role in ('cgame', 'ui', 'qagame'):
            binary = source / (config.get('prefix', '') + role + config['suffix'])
            binary.write_bytes(magic + (target + '/' + role).encode())
            paths[role] = str(binary)
        module_manifest = source / 'modules.json'
        module_manifest.write_text(json.dumps({'schema': 1, 'variant': 'game', 'modules': paths}))
        core = source / (core_name or CORE_NAMES[target])
        core.write_bytes(magic + (target + '/omnibot').encode())
        core_manifest = source / 'core.json'
        system, architecture = loose.CORE_IDENTITIES[target]
        core_manifest.write_text(json.dumps({'schema': 1, 'variant': 'game', 'library': str(core),
                                            'system': system, 'architecture': architecture}))
        manifest = loose.stage_target(module_manifest, core_manifest, self.inputs / target, target)
        return manifest

    def matrix(self, core_names=None):
        return [self.target(target, (core_names or {}).get(target)) for target in self.configs]

    def test_full_matrix_contains_only_40_flat_binaries_with_native_names(self):
        manifests = self.matrix()
        archive = loose.collect(self.inputs, self.output)
        expected = {row['name'] for path in manifests for row in json.loads(path.read_text())['files']}
        with zipfile.ZipFile(archive) as zipped:
            self.assertEqual(len(zipped.infolist()), 40)
            self.assertEqual(set(zipped.namelist()), expected)
            self.assertIsNone(zipped.testzip())
            for entry in zipped.infolist():
                self.assertFalse(entry.is_dir())
                self.assertNotIn('/', entry.filename)
                self.assertNotIn('\\', entry.filename)
                self.assertFalse(entry.filename.endswith(('.zip', '.pk3', '.json', '.txt', '.md')))
        self.assertEqual(self.verify.call_count, len(self.configs))
        self.assertEqual(self.validate.call_count, len(self.configs))
        report = json.loads((self.output / 'loose-report.json').read_text())
        self.assertEqual(report['binary_count'], 40)
        self.assertEqual(set(report['targets']), set(self.configs))
        self.assertFalse(report['metadata_inside_archive'])
        self.assertEqual({path.name for path in (self.output / 'binaries').iterdir()}, expected)

    def test_colliding_core_names_get_explicit_target_tags(self):
        self.matrix({'windows-x64': 'omnibot_et.dll', 'linux-x86_64': 'omnibot_et.so'})
        archive = loose.collect(self.inputs, self.output)
        with zipfile.ZipFile(archive) as zipped:
            names = set(zipped.namelist())
            for name in ('omnibot_et.windows-x86.dll', 'omnibot_et.windows-x64.dll',
                         'omnibot_et.linux-i386.so', 'omnibot_et.linux-x86_64.so'):
                self.assertIn(name, names)
            self.assertNotIn('omnibot_et.dll', names)
            self.assertNotIn('omnibot_et.so', names)
            self.assertIn('cgame_mp_x86.dll', names)
            self.assertEqual(len(names), 40)

    def test_collision_check_is_case_insensitive_and_never_overwrites(self):
        names = loose.flat_names([{'name': 'Omnibot_et.dll', 'target': 'windows-x86'},
                                  {'name': 'omnibot_et.dll', 'target': 'windows-x64'}])
        self.assertEqual(names, ['Omnibot_et.windows-x86.dll', 'omnibot_et.windows-x64.dll'])
        with self.assertRaisesRegex(ValueError, 'Ambiguous'):
            loose.flat_names([{'name': 'core.so', 'target': 'linux-i386'},
                              {'name': 'core.so', 'target': 'linux-x86_64'},
                              {'name': 'core.linux-i386.so', 'target': 'linux-armv7'}])

    def test_incomplete_matrix_and_duplicate_target_are_rejected(self):
        manifest = self.target('windows-x86')
        with self.assertRaisesRegex(ValueError, 'Incomplete loose target matrix'):
            loose.collect(self.inputs, self.output)
        shutil.copytree(manifest.parent, self.inputs / 'duplicate')
        with self.assertRaisesRegex(ValueError, 'Duplicate loose target'):
            loose.collect(self.inputs, self.output)

    def test_editor_variant_and_missing_core_are_rejected(self):
        manifest = self.target('windows-x86')
        metadata = json.loads(manifest.read_text())
        metadata['variant'] = 'navstudio'
        manifest.write_text(json.dumps(metadata))
        with self.assertRaisesRegex(ValueError, 'non-Game'):
            loose.collect(self.inputs, self.output)
        metadata['variant'] = 'game'
        metadata['files'] = [row for row in metadata['files'] if row['role'] != 'omnibot']
        manifest.write_text(json.dumps(metadata))
        with self.assertRaisesRegex(ValueError, 'Incomplete loose binary roles'):
            loose.collect(self.inputs, self.output)

    def test_tampered_core_and_invalid_boundary_evidence_are_rejected(self):
        manifest = self.target('windows-x86')
        metadata = json.loads(manifest.read_text())
        metadata['bot_core_boundary']['variant'] = 'navstudio'
        manifest.write_text(json.dumps(metadata))
        with self.assertRaisesRegex(ValueError, 'Game core boundary'):
            loose.collect(self.inputs, self.output)
        metadata['bot_core_boundary']['variant'] = 'game'
        manifest.write_text(json.dumps(metadata))
        (manifest.parent / 'binaries/omnibot_et.dll').write_bytes(b'MZmodified')
        with self.assertRaisesRegex(ValueError, 'fingerprint mismatch'):
            loose.collect(self.inputs, self.output)

    def test_extra_asset_nested_zip_or_directory_are_rejected(self):
        manifest = self.target('windows-x86')
        for relative in ('README.txt', 'binaries/extra.zip', 'binaries/nested/asset.pk3'):
            with self.subTest(relative=relative):
                path = manifest.parent / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(b'unrequested file')
                with self.assertRaisesRegex(ValueError, 'Unexpected file/directory'):
                    loose.collect(self.inputs, self.output)
                path.unlink()

    def test_unsafe_metadata_paths_and_fake_binary_are_rejected(self):
        manifest = self.target('windows-x86')
        metadata = json.loads(manifest.read_text())
        metadata['files'][0]['name'] = '../outside.dll'
        manifest.write_text(json.dumps(metadata))
        with self.assertRaisesRegex(ValueError, 'Unsafe binary filename'):
            loose.collect(self.inputs, self.output)
        source = self.root / 'built/windows-x86'
        (source / 'cgame_mp_x86.dll').write_text('text renamed as a DLL')
        with self.assertRaisesRegex(ValueError, 'Not a binary'):
            loose.stage_target(source / 'modules.json', source / 'core.json',
                               self.root / 'other', 'windows-x86')

    def test_reruns_refuse_to_mix_existing_output(self):
        self.matrix()
        loose.collect(self.inputs, self.output)
        with self.assertRaisesRegex(ValueError, 'Refusing to mix final'):
            loose.collect(self.inputs, self.output)
        source = self.root / 'built/windows-x86'
        with self.assertRaisesRegex(ValueError, 'Refusing to mix loose target'):
            loose.stage_target(source / 'modules.json', source / 'core.json',
                               self.inputs / 'windows-x86', 'windows-x86')

    def test_swapped_core_abi_and_wrong_manifest_identity_are_rejected(self):
        self.target('windows-x86')
        source = self.root / 'built/windows-x86'
        (source / 'omnibot_et.dll').write_bytes(binary_header('windows-x64'))
        with self.assertRaisesRegex(ValueError, 'Not a binary for windows-x86'):
            loose.stage_target(source / 'modules.json', source / 'core.json', self.root / 'wrong-abi', 'windows-x86')
        (source / 'omnibot_et.dll').write_bytes(binary_header('windows-x86'))
        core = json.loads((source / 'core.json').read_text())
        core['architecture'] = 'x86_64'
        (source / 'core.json').write_text(json.dumps(core))
        with self.assertRaisesRegex(ValueError, 'Wrong bot core system/architecture'):
            loose.stage_target(source / 'modules.json', source / 'core.json', self.root / 'wrong-manifest', 'windows-x86')

    def test_thin_macos_module_cannot_satisfy_universal_target(self):
        self.target('macos-universal')
        source = self.root / 'built/macos-universal'
        (source / 'cgame_mac').write_bytes(b'\xcf\xfa\xed\xfe' + struct.pack('<I', 0x01000007) + bytes(56))
        with self.assertRaisesRegex(ValueError, 'Not a binary for macos-universal'):
            loose.stage_target(source / 'modules.json', source / 'core.json', self.root / 'thin-macos', 'macos-universal')

    def test_module_authoring_literal_is_rejected_during_staging_and_collection(self):
        manifest = self.target('windows-x86')
        source = self.root / 'built/windows-x86'
        (source / 'cgame_mp_x86.dll').write_bytes(binary_header('windows-x86') + b'wp_add\0')
        with self.assertRaisesRegex(ValueError, 'Editor entry point in public cgame'):
            loose.stage_target(source / 'modules.json', source / 'core.json', self.root / 'editor-module', 'windows-x86')
        binary = manifest.parent / 'binaries/cgame_mp_x86.dll'
        binary.write_bytes(binary_header('windows-x86') + b'wp_add\0')
        metadata = json.loads(manifest.read_text())
        row = next(row for row in metadata['files'] if row['role'] == 'cgame')
        row.update(bytes=binary.stat().st_size, sha256=loose.digest(binary))
        manifest.write_text(json.dumps(metadata))
        with self.assertRaisesRegex(ValueError, 'Editor entry point in public cgame'):
            loose.collect(self.inputs, self.output)

    def test_intermediate_cleanup_requires_final_and_only_deletes_exact_own_names(self):
        release_version = loose.version()
        artifacts = [{'id': index + 1, 'name': f'loose-input-{release_version}-{target}'}
                     for index, target in enumerate(self.configs)]
        artifacts += [{'id': 100, 'name': 'unrelated-artifact'},
                      {'id': 101, 'name': f'loose-input-{release_version}-unknown-target'}]
        calls = []
        def request(method, endpoint):
            calls.append((method, endpoint))
            return {'artifacts': artifacts} if method == 'GET' else None
        with self.assertRaisesRegex(ValueError, 'Final loose artifact is absent'):
            loose.cleanup_intermediates('jay1110/true-combat', 123, request)
        self.assertTrue(all(method == 'GET' for method, endpoint in calls))
        artifacts.append({'id': 102, 'name': f'TCE2-{release_version}-Game-Loose-Binaries', 'expired': False})
        calls.clear()
        self.assertEqual(loose.cleanup_intermediates('jay1110/true-combat', 123, request), 10)
        deleted = {endpoint.rsplit('/', 1)[1] for method, endpoint in calls if method == 'DELETE'}
        self.assertEqual(deleted, {str(i) for i in range(1, 11)})

    def test_builds_only_game_targets_and_workflow_upload_is_flat(self):
        for target in self.configs:
            with self.subTest(target=target):
                commands = dict(build_commands(target, self.root / 'build', self.root / 'boost'))
                modules = commands['configure-modules']
                for flag in ('FEATURE_NAVSTUDIO', 'TCE_BUILD_NAVSTUDIO', 'TCE_BUILD_NAVMESH_TOOL',
                             'TCE_BUILD_TCE_HELPERS', 'TCE_BUILD_REFERENCE_TESTS'):
                    self.assertIn('-D' + flag + '=OFF', modules)
                self.assertIn('-DFEATURE_NAVSTUDIO=OFF', commands['configure-core'])
                self.assertIn('tce_modules', commands['build-modules'])
                self.assertIn('omnibot-et', commands['build-core'])
                self.assertEqual(set(commands), {'configure-modules', 'build-modules', 'configure-core', 'build-core'})
        workflows(loose.ROOT)
        workflow = (loose.ROOT / '.github/workflows/loose-binaries.yml').read_text()
        self.assertIn("INCLUDE_EDITOR: 'false'", workflow)
        self.assertIn('path: out/loose/binaries/*', workflow)
        self.assertNotIn('path: out/loose/*.zip', workflow)
        self.assertNotIn('publish_release:', workflow)
        self.assertNotIn('workflow_call:', workflow)


if __name__ == '__main__':
    unittest.main()
