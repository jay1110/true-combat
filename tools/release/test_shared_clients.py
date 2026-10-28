"""Synthetic ABI/PK3 closure tests; these carriers are not executable modules."""
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
import warnings
import zipfile

from aggregate import main as aggregate
import client_packages as clients
from package import package_modules, version
from test_loose_binaries import binary_header
from validate import archive as validate_archive, validate_client_resources


VERSION = version()


def input_archive(folder, target, variant='game', overrides=None, configuration='Release'):
    folder.mkdir(parents=True, exist_ok=True)
    payload = clients.client_assets(VERSION, variant)
    names = clients.module_names(target)
    for role, name in names.items():
        payload[name] = binary_header(target) + (target + '/' + variant + '/' + role).encode()
    payload.update(overrides or {})
    files = {'tce2/' + name: payload[name] for name in names.values()}
    config = clients.target_table()[target]
    files['tce2/' + config.get('prefix', '') + 'qagame' + config['suffix']] = binary_header(target) + b'/qagame'
    files.update({clients.INPUT_PREFIX + name: data for name, data in payload.items()})
    files['README.txt'] = b'CLIENT INPUTS ONLY: use the aggregate release for installation and server downloads.\n'
    if variant == 'navstudio':
        editor = clients.ROOT / 'navstudio/editor-ui'
        addon = {'ui/' + name: (editor / name).read_bytes()
                 for name in (editor / 'editor-files.txt').read_text().splitlines() if name}
        files['tce2/zz_tce2_navstudio_editor.pk3'] = clients.zip_bytes(addon)
    # Runtime proof is separately tested; these fixtures isolate client closure.
    metadata = {'schema': 1, 'target': target, 'version': VERSION, 'variant': variant,
                'configuration': configuration, 'authoring': variant == 'navstudio',
                'client_package': {'scope': 'target-input', 'targets': [target]},
                'gameplay_bot_runtime': True,
                'files': [{'path': name, 'sha256': clients.sha(data), 'bytes': len(data)}
                          for name, data in sorted(files.items())]}
    files['manifest.json'] = clients.json_bytes(metadata)
    path = folder / f'TCE2-{VERSION}-{target}-{variant}-Client-Inputs.zip'
    # Transfer compression is irrelevant to closure. Store the real 6 MiB map
    # images in these synthetic inputs; production canonical ZIPs still use
    # their deterministic compression and are exercised below.
    with zipfile.ZipFile(path, 'w', zipfile.ZIP_STORED) as archive:
        for name, data in sorted(files.items()):
            archive.writestr(name, data)
    return path


class SharedClients(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def row(self, target, variant='game', **kwargs):
        path = input_archive(self.root / 'inputs' / (target + '-' + variant), target, variant, **kwargs)
        return clients.read_target(path, VERSION)

    def test_linux_server_download_contains_windows_pair_without_stale_fallback(self):
        windows, linux = self.row('windows-x86'), self.row('linux-x86_64')
        windows_name = clients.module_names('windows-x86')['cgame']
        # The old same-name server PK3 contains only Linux; a Windows lookup
        # therefore falls through to a stale locally installed module package.
        server_partial = clients.zip_bytes(linux['payload'])
        client_partial = clients.zip_bytes(windows['payload'])
        stale = {windows_name: b'old client from obsolete PK3'}
        old_server = clients.zip_contents(server_partial)
        self.assertNotIn(windows_name, old_server)
        self.assertEqual(old_server.get(windows_name, stale[windows_name]), stale[windows_name])
        self.assertNotEqual(clients.sha(server_partial), clients.sha(client_partial))
        canonical, metadata = clients.canonical_client([windows, linux],
            ['windows-x86', 'linux-x86_64'], VERSION, 'game')
        self.assertEqual(clients.zip_contents(canonical)[windows_name], windows['payload'][windows_name])
        self.assertEqual(metadata['scope'], 'local-diagnostic')
        with self.assertRaisesRegex(ValueError, 'Wrong/incomplete'):
            clients.promote_target(windows, canonical, metadata, self.root / 'partial.zip', VERSION)
        again, _ = clients.canonical_client([linux, windows],
            ['linux-x86_64', 'windows-x86'], VERSION, 'game')
        self.assertEqual(again, canonical)

    def test_missing_or_duplicate_abi_prevents_promotion(self):
        row = self.row('linux-x86_64')
        for inputs in ([row], [row, row]):
            with self.assertRaisesRegex(ValueError, 'Missing/duplicate'):
                clients.canonical_client(inputs, ['linux-x86_64', 'windows-x86'], VERSION, 'game')

    def test_every_matrix_client_abi_round_trips_with_its_native_name(self):
        rows = [self.row(target) for target in clients.target_table()]
        data, metadata = clients.canonical_client(rows, clients.target_table(), VERSION, 'game')
        self.assertEqual(clients.check_canonical(data, VERSION, 'game'), metadata)
        self.assertEqual(len(metadata['modules']), 20)
        for row in rows:
            for name in clients.module_names(row['metadata']['target']).values():
                self.assertEqual(clients.zip_contents(data)[name], row['payload'][name])

    def test_shared_ui_equal_across_abis_and_both_variants(self):
        packages = {}
        for variant in ('game', 'navstudio'):
            rows = [self.row(target, variant) for target in ('windows-x86', 'linux-x86_64')]
            data, _ = clients.canonical_client(rows, ['windows-x86', 'linux-x86_64'], VERSION, variant)
            packages[variant] = clients.zip_contents(data)
        for name in clients.SHARED_UI:
            self.assertEqual(packages['game']['ui/' + name], packages['navstudio']['ui/' + name])
        for name in ('ui/options_system.menu',
                     'gfx/tce2/fireteam/assault.tga', 'gfx/tce2/fireteam/recon.tga',
                     'gfx/tce2/fireteam/sniper.tga', 'gfx/tce2/fireteam/SOURCE.json'):
            self.assertEqual(packages['game'][name], packages['navstudio'][name])
            self.assertEqual(packages['game'][name], (clients.ROOT/'src/assets'/name).read_bytes())
        for variant in packages:
            self.assertNotIn('ui/options_quality.menu', packages[variant])
        self.assertIn('ui/menus.txt', packages['game'])
        self.assertNotIn('ui/menus.txt', packages['navstudio'])
        self.assertFalse(any('navstudio' in name or 'tce_waypoint' in name for name in packages['game']))

    def test_differing_ui_resource_or_editor_asset_is_rejected(self):
        for name in ('ui/options_controls.menu', 'gfx/misc/radar_dot.tga', 'levelshots/obj_delta_cc.tga',
                     'gfx/tce2/nav/player.tga', 'gfx/tce2/nav/SOURCE.json',
                     'ui/options_system.menu',
                     'gfx/tce2/fireteam/assault.tga', 'gfx/tce2/fireteam/recon.tga',
                     'gfx/tce2/fireteam/sniper.tga', 'gfx/tce2/fireteam/SOURCE.json'):
            with self.subTest(name=name), self.assertRaisesRegex(ValueError, 'Different/stale shared'):
                self.row('windows-x86', overrides={name: b'stale resource'})
        with self.assertRaisesRegex(ValueError, 'Wrong client inputs/assets'):
            self.row('linux-x86_64', overrides={'ui/tce_editor.menu': b'editor'})
        with self.assertRaisesRegex(ValueError, 'Wrong client inputs/assets'):
            self.row('linux-x86_64', 'navstudio', overrides={'ui/menus.txt': b'public menu list'})
        for variant in ('game', 'navstudio'):
            with self.subTest(variant=variant), self.assertRaisesRegex(ValueError, 'Wrong client inputs/assets'):
                self.row('windows-x86', variant, overrides={'ui/options_quality.menu': b'removed menu'})

    def test_cmake_and_canonical_clients_include_user_delta_images_in_both_variants(self):
        expected = clients.levelshot_assets()
        required = {'levelshots/obj_delta_cc.tga', 'levelshots/obj_delta_cc_blueprint.tga',
                    'levelshots/SOURCE.json'}
        self.assertEqual(set(expected), required)
        for variant in ('game', 'navstudio'):
            folder = self.root / 'asset-package' / variant
            folder.mkdir(parents=True)
            for name in ('cgame_mp_x86.dll', 'ui_mp_x86.dll'):
                (folder / name).write_bytes(binary_header('windows-x86'))
            (folder / 'tce-build-variant.txt').write_text(variant + '\n')
            subprocess.run(['cmake', '-DMODULE_DIRECTORY=' + str(folder), '-DBUILD_VARIANT=' + variant,
                            '-P', str(clients.ROOT / 'src/cmake/package_modules.cmake')],
                           check=True, capture_output=True, text=True)
            payload = clients.zip_contents((folder / f'tce2-{VERSION}.pk3').read_bytes())
            self.assertEqual({name: data for name, data in payload.items() if name.startswith('levelshots/')}, expected)
            self.assertEqual(clients.validate_levelshots(payload)['map'], 'obj_delta')
            hud = clients.fireteam_assets()
            hud.update({'ui/' + name: (clients.ROOT/'src/assets/ui'/name).read_bytes()
                        for name in ('options_system.menu',)})
            self.assertEqual({name: payload[name] for name in hud}, hud)
            self.assertNotIn('ui/options_quality.menu', payload)
            row = self.row('windows-x86', variant)
            data, _ = clients.canonical_client([row], ['windows-x86'], VERSION, variant)
            canonical = clients.zip_contents(data)
            self.assertEqual({name: canonical[name] for name in hud}, hud)
            self.assertNotIn('ui/options_quality.menu', canonical)
            self.assertEqual({name: data for name, data in canonical.items() if name.startswith('levelshots/')}, expected)
            # No images are fabricated for unsupplied maps; their existing
            # CGame fallback remains applicable.
            self.assertNotIn('levelshots/obj_snow_cc.tga', canonical)

    def test_fireteam_provenance_requires_all_three_correct_rgba_icons(self):
        original = clients.fireteam_assets()
        for path in original:
            for missing in (False, True):
                with self.subTest(path=path, missing=missing):
                    files = dict(original)
                    if missing:
                        del files[path]
                    else:
                        files[path] = b'changed asset/provenance'
                    with self.assertRaises(ValueError):
                        clients.validate_fireteam_icons(files)
        files = dict(original)
        path = 'gfx/tce2/fireteam/assault.tga'
        data = bytearray(files[path]); data[12] = 32; files[path] = bytes(data)
        provenance = json.loads(files['gfx/tce2/fireteam/SOURCE.json'])
        provenance['files'][0]['sha256'] = clients.sha(files[path])
        files['gfx/tce2/fireteam/SOURCE.json'] = clients.json_bytes(provenance)
        # An honestly fingerprinted but wrong-size image still cannot pass.
        with self.assertRaisesRegex(ValueError, 'invalid Fireteam icon'):
            clients.validate_fireteam_icons(files)

    def test_release_resource_validator_requires_new_hud_assets_in_both_variants(self):
        for variant in ('game', 'navstudio'):
            original = clients.client_assets(VERSION, variant)
            for missing in (None, 'ui/options_system.menu',
                            'gfx/tce2/fireteam/recon.tga', 'gfx/tce2/fireteam/SOURCE.json'):
                with self.subTest(variant=variant, missing=missing):
                    files = dict(original)
                    if missing:
                        del files[missing]
                    with zipfile.ZipFile(io.BytesIO(clients.zip_bytes(files))) as pk3:
                        if missing:
                            with self.assertRaises(ValueError):
                                validate_client_resources(pk3, {'version': VERSION}, variant == 'navstudio')
                        else:
                            validate_client_resources(pk3, {'version': VERSION}, variant == 'navstudio')
            files = dict(original)
            files['ui/options_quality.menu'] = b'removed menu'
            with zipfile.ZipFile(io.BytesIO(clients.zip_bytes(files))) as pk3:
                with self.assertRaisesRegex(ValueError, 'Removed graphics-quality menu'):
                    validate_client_resources(pk3, {'version': VERSION}, variant == 'navstudio')

    def test_missing_or_corrupt_user_delta_images_and_provenance_are_rejected(self):
        original = clients.levelshot_assets()
        for path in ('levelshots/obj_delta_cc.tga', 'levelshots/obj_delta_cc_blueprint.tga', 'levelshots/SOURCE.json'):
            with self.subTest(path=path):
                missing = dict(original); del missing[path]
                with self.assertRaises(ValueError):
                    clients.validate_levelshots(missing)
        corrupt = dict(original); corrupt['levelshots/obj_delta_cc.tga'] = b'changed image'
        with self.assertRaisesRegex(ValueError, 'Missing/changed'):
            clients.validate_levelshots(corrupt)

    def test_debug_wrong_variant_or_native_abi_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Not a Release'):
            self.row('windows-x86', configuration='Debug')
        with self.assertRaisesRegex(ValueError, 'Editor entry point'):
            self.row('windows-x86', overrides={'cgame_mp_x86.dll': binary_header('windows-x86') + b'wp_add\0'})
        with self.assertRaisesRegex(ValueError, 'Wrong native client ABI'):
            self.row('windows-x86', overrides={'ui_mp_x86.dll': binary_header('linux-x86_64')})
        editor = self.row('windows-x86', 'navstudio')
        with self.assertRaisesRegex(ValueError, 'Mixed client variant'):
            clients.canonical_client([editor], ['windows-x86'], VERSION, 'game')

    def test_canonical_bytes_reject_missing_abi_stale_assets_or_variant(self):
        row = self.row('windows-x86')
        data, _ = clients.canonical_client([row], ['windows-x86'], VERSION, 'game')
        with self.assertRaisesRegex(ValueError, 'Wrong/incomplete'):
            clients.check_canonical(data, VERSION, 'game')
        with self.assertRaisesRegex(ValueError, 'Wrong/incomplete'):
            clients.check_canonical(data, VERSION, 'navstudio', ['windows-x86'])
        files = clients.zip_contents(data)
        del files['cgame_mp_x86.dll']
        with self.assertRaisesRegex(ValueError, 'Missing canonical'):
            clients.check_canonical(clients.zip_bytes(files), VERSION, 'game', ['windows-x86'])
        files = clients.zip_contents(data)
        files['ui/options_controls.menu'] = b'stale'
        with self.assertRaisesRegex(ValueError, 'fingerprint/assets'):
            clients.check_canonical(clients.zip_bytes(files), VERSION, 'game', ['windows-x86'])

    def test_duplicate_case_collision_or_unsafe_zip_paths_are_rejected(self):
        for entries in ([('a', b'x'), ('a', b'y')], [('A', b'x'), ('a', b'y')], [('../escape', b'x')]):
            stream = io.BytesIO()
            with warnings.catch_warnings():
                warnings.simplefilter('ignore', UserWarning)
                with zipfile.ZipFile(stream, 'w') as archive:
                    for name, data in entries:
                        archive.writestr(name, data)
            with self.assertRaises(ValueError):
                clients.zip_contents(stream.getvalue())

    def test_validator_rejects_unclassified_platform_only_client_pk3(self):
        row = self.row('windows-x86')
        files = {name: data for name, data in row['files'].items() if not name.startswith(clients.INPUT_PREFIX)}
        files[f'tce2/tce2-{VERSION}.pk3'] = clients.zip_bytes(row['payload'])
        metadata = dict(row['metadata'])
        del metadata['client_package']
        metadata['files'] = [{'path': name, 'sha256': clients.sha(data), 'bytes': len(data)}
                             for name, data in files.items() if name != 'manifest.json']
        files['manifest.json'] = clients.json_bytes(metadata)
        path = self.root / 'old-platform-only.zip'; path.write_bytes(clients.zip_bytes(files))
        with self.assertRaisesRegex(ValueError, 'Unclassified/partial'):
            validate_archive(path)

    def built(self, target='windows-x86'):
        folder = self.root / 'built'
        folder.mkdir()
        paths = {}
        config = clients.target_table()[target]
        for role in ('cgame', 'ui', 'qagame'):
            path = folder / (config.get('prefix', '') + role + config['suffix'])
            path.write_bytes(binary_header(target) + (target + '/game/' + role).encode())
            paths[role] = str(path)
        manifest = folder / 'modules.json'
        manifest.write_bytes(clients.json_bytes({'schema': 1, 'variant': 'game',
                                                 'configuration': 'Release', 'modules': paths}))
        return folder, manifest

    def test_package_default_inputs_and_explicit_local_smoke_remain_distinct(self):
        folder, manifest = self.built()
        transfer = package_modules(None, self.root / 'transfer', 'windows-x86', 'game', module_manifest=manifest)
        files = clients.zip_contents(transfer.read_bytes())
        self.assertTrue(transfer.name.endswith('-Client-Inputs.zip'))
        self.assertNotIn(f'tce2/tce2-{VERSION}.pk3', files)
        self.assertEqual(clients.read_target(transfer, VERSION)['metadata']['client_package']['scope'], 'target-input')
        # This fixture omits bot runtime intentionally; exercise actual archive
        # validation of the client inputs without inventing a core symbol proof.
        self.assertTrue(validate_archive(transfer)['passed'])
        smoke = package_modules(folder, self.root / 'smoke', 'windows-x86', 'game', local_client_pk3=True)
        self.assertIn('-Local-Test.zip', smoke.name)
        self.assertEqual(validate_archive(smoke)['client_package_scope'], 'local-test')
        with self.assertRaisesRegex(ValueError, 'Not a Release client-input'):
            clients.read_target(smoke, VERSION)

    def test_shared_option_requires_full_matrix_and_matching_target_build(self):
        _, manifest = self.built()
        rows = [self.row(target) for target in clients.target_table()]
        data, _ = clients.canonical_client(rows, clients.target_table(), VERSION, 'game')
        shared = self.root / 'canonical.pk3'; shared.write_bytes(data)
        target = package_modules(None, self.root / 'shared', 'windows-x86', 'game',
                                 module_manifest=manifest, shared_client_pk3=shared)
        self.assertTrue(validate_archive(target)['passed'])
        self.assertEqual(clients.zip_contents(target.read_bytes())[f'tce2/tce2-{VERSION}.pk3'], data)
        Path(json.loads(manifest.read_text())['modules']['ui']).write_bytes(binary_header('windows-x86') + b'other build')
        with self.assertRaisesRegex(ValueError, 'differs from target module'):
            package_modules(None, self.root / 'mismatch', 'windows-x86', 'game',
                            module_manifest=manifest, shared_client_pk3=shared)
        two, _ = clients.canonical_client(rows[:2], [row['metadata']['target'] for row in rows[:2]], VERSION, 'game')
        shared.write_bytes(two)
        with self.assertRaisesRegex(ValueError, 'Wrong/incomplete'):
            package_modules(None, self.root / 'partial', 'windows-x86', 'game',
                            module_manifest=manifest, shared_client_pk3=shared)

    def test_missing_or_different_configuration_cannot_enter_release_inputs(self):
        folder, manifest = self.built()
        with self.assertRaisesRegex(ValueError, 'module/configuration manifest'):
            package_modules(folder, self.root / 'missing', 'windows-x86', 'game')
        metadata = json.loads(manifest.read_text()); metadata['configuration'] = 'Debug'
        manifest.write_bytes(clients.json_bytes(metadata))
        with self.assertRaisesRegex(ValueError, 'configuration provenance'):
            package_modules(None, self.root / 'debug', 'windows-x86', 'game', module_manifest=manifest)


class AggregateClients(unittest.TestCase):
    def test_all_ten_targets_receive_same_variant_pk3_and_missing_target_stays_inputs(self):
        for missing in (False, True):
            with self.subTest(missing=missing), tempfile.TemporaryDirectory() as temporary:
                root = Path(temporary); source = root / 'collected'; output = root / 'out'
                configs = clients.target_table()
                for target in configs:
                    for variant in ('game', 'navstudio'):
                        if missing and target == 'windows-x86' and variant == 'game':
                            continue
                        input_archive(source / (target + '-' + variant), target, variant)
                source.mkdir(exist_ok=True)
                (source / f'TCE2-{VERSION}-Source.zip').write_bytes(clients.zip_bytes({'fixture.txt': b'source selection only'}))
                needs = {'plan': {'result': 'success', 'outputs': {'matrix': json.dumps({'include': [
                    {'target': target, 'variant': variant} for target in configs for variant in ('game', 'navstudio')]})}},
                    'binaries': {'result': 'failure' if missing else 'success'}, 'navstudio-desktop': {'result': 'skipped'}}
                with patch.object(sys, 'argv', ['aggregate', '--input', str(source), '--out', str(output)]), \
                        patch.dict(os.environ, {'NEEDS_JSON': json.dumps(needs)}):
                    self.assertEqual(aggregate(), int(missing))
                report = json.loads((output / 'release-manifest.json').read_text())
                self.assertEqual(report['client_inputs_promoted'], not missing)
                self.assertEqual(report['complete'], not missing)
                if missing:
                    self.assertFalse(report['client_packages'])
                    self.assertEqual(len(list((output / 'bundle/inputs').glob('*-Client-Inputs.zip'))), 19)
                    continue
                for variant in ('game', 'navstudio'):
                    payloads = []
                    for target in configs:
                        path = output / 'bundle/archives' / f'TCE2-{VERSION}-{target}-{variant}.zip'
                        with zipfile.ZipFile(path) as archive:
                            payloads.append(archive.read(f'tce2/tce2-{VERSION}.pk3'))
                            self.assertFalse(any(name.startswith(clients.INPUT_PREFIX) for name in archive.namelist()))
                            self.assertNotIn(b'CLIENT INPUTS ONLY', archive.read('README.txt'))
                            metadata = json.loads(archive.read('manifest.json'))
                            self.assertEqual(metadata['client_package']['targets'], sorted(configs))
                    self.assertTrue(all(data == payloads[0] for data in payloads))
                    self.assertEqual(report['client_packages'][variant]['sha256'], clients.sha(payloads[0]))
                self.assertNotEqual(report['client_packages']['game']['sha256'], report['client_packages']['navstudio']['sha256'])
                for row in report['files']:
                    path = output / 'bundle' / row['path']
                    self.assertEqual(clients.sha(path.read_bytes()), row['sha256'])
                    self.assertEqual(path.stat().st_size, row['bytes'])


if __name__ == '__main__':
    unittest.main()
