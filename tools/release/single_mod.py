"""One mod-update PK3 and one all-ABI server bundle per explicit build variant."""
import io
import json
from pathlib import Path
import zipfile

from client_packages import (INPUT_PREFIX, check_canonical, check_module, json_bytes,
                             module_names, sha, target_table, zip_bytes, zip_contents)
from resource_assets import resource_fingerprint, base_dependency
from runtime_payload import prepare_runtime, command_spans, sha256
from verify_omnibot_boundary import (validate_symbol_evidence, AUTHORING_COMMANDS,
                                    RUNTIME_BINDINGS)

ROOT = Path(__file__).resolve().parents[2]


def build_bundle(rows, client_data, client_metadata, release_version, variant,
                 repo=ROOT, expected_targets=None, assets=None):
    expected = set(target_table(repo) if expected_targets is None else expected_targets)
    if len(rows) != len(expected) or {row['metadata']['target'] for row in rows} != expected:
        raise ValueError('Missing/duplicate all-ABI server inputs')
    if check_canonical(client_data, release_version, variant, expected, repo, assets=assets) != client_metadata:
        raise ValueError('Canonical full-mod metadata changed')
    files = {f'tce2/tce2-{release_version}.pk3': client_data}
    sources = {}
    def add(name, data):
        if name in files and files[name] != data:
            raise ValueError('Different common server payload between ABIs: ' + name)
        files[name] = data
    for row in sorted(rows, key=lambda r: r['metadata']['target']):
        metadata = row['metadata'];target = metadata['target']
        if metadata['variant'] != variant or metadata['configuration'] != 'Release':
            raise ValueError('Mixed server variant/configuration')
        config = target_table(repo)[target]
        qgame = config.get('prefix', '') + 'qagame' + config['suffix']
        clients = set(module_names(target, repo).values())
        core = metadata.get('bot_core_boundary', {}).get('library')
        for name, data in row['files'].items():
            if name == 'manifest.json' or name == 'README.txt' or name.startswith(INPUT_PREFIX):
                continue
            if name.startswith('tce2/'):
                relative = name[len('tce2/'):]
                if relative in clients:
                    continue  # Client modules exist only inside the one PK3.
                if relative.endswith('.pk3'):
                    raise ValueError('Additional PK3 in all-ABI server input')
                if relative == qgame or (core and relative == 'omni-bot/' + core):
                    output = 'server/' + target + '/' + relative
                else:
                    output = 'server/common/' + relative
            elif name.startswith('licenses/'):
                output = name
            else:
                raise ValueError('Unclassified server input: ' + name)
            add(output, data)
        if 'server/' + target + '/' + qgame not in files:
            raise ValueError('Missing loose server module: ' + target)
        sources[target] = {'archive_sha256': sha(Path(row['path']).read_bytes()),
                           'metadata': metadata}
    add('install.py', (repo / 'tools/release/install_mod.py').read_bytes())
    for name in ('start_game.py', 'start_game.ps1', 'start_game.sh', 'README.md'):
        add('launcher/' + name, (repo / 'tools/launcher' / name).read_bytes())
    label = 'LOCAL EDITOR ONLY; never install on a public server.\n' if variant == 'navstudio' else 'Public gameplay build; authoring is compiled out.\n'
    add('README.txt', (f'TCE2 {release_version} / {variant}\n' + label +
        f'Exactly one mod-update package: tce2/tce2-{release_version}.pk3.\n'
        'It contains only new/changed pak resources and UI/CGame for the advertised ABIs.\n'
        'Original TCE 0.49b pak0.pk3 through pak4.pk3 must already be installed; they are not shipped here.\n'
        'Server/QGame stays loose in server/<ABI>; common bot/Lua/config data is in server/common.\n'
        'Install into an existing matching ET/ETLegacy installation, not an empty engine folder.\n'
        'Example: python install.py --target windows-x86 --mod-dir C:/Games/ET/tce2\n'
        'Original ET may load a pre-existing loose UI/CGame before reading PK3s at startup.\n'
        'Use --classic-client to extract only the selected matching pair from this same PK3.\n'
        'No extra client PK3 and no loose client modules are shipped in this ZIP.\n'
        'The installer backs up overwritten files outside the mod directory. Close the game first.\n'
        'Keep the original pak0..4 packages. Remove old TCE2-update/theme PK3s after an external backup.\n'
        'No engine binaries or original-binary parity claim are included.\n').encode('utf8'))
    metadata = {'schema': 1, 'kind': 'single-mod-all-abis', 'version': release_version,
                'variant': variant, 'authoring': variant == 'navstudio',
                'targets': sorted(expected), 'client_package': client_metadata,
                'requires_original_tce': True, 'base_dependency': base_dependency(repo, release_version),
                'pk3_path': f'tce2/tce2-{release_version}.pk3', 'pk3_sha256': sha(client_data),
                'resource_tree': resource_fingerprint(variant, repo), 'target_inputs': sources,
                'files': [{'path': name, 'bytes': len(data), 'sha256': sha(data)}
                          for name, data in sorted(files.items())]}
    files['manifest.json'] = json_bytes(metadata)
    return zip_bytes(files), metadata


def check_bundle(data, release_version, variant, repo=ROOT, expected_targets=None, assets=None):
    files = zip_contents(data)
    metadata = json.loads(files.pop('manifest.json'))
    expected = set(target_table(repo) if expected_targets is None else expected_targets)
    if (metadata.get('schema') != 1 or metadata.get('kind') != 'single-mod-all-abis' or
            metadata.get('version') != release_version or metadata.get('variant') != variant or
            metadata.get('authoring') is not (variant == 'navstudio') or
            metadata.get('targets') != sorted(expected) or set(metadata.get('target_inputs', {})) != expected or
            metadata.get('resource_tree') != resource_fingerprint(variant, repo)):
        raise ValueError('Wrong/incomplete single-mod server bundle')
    if metadata.get('requires_original_tce') is not True or metadata.get('base_dependency') != base_dependency(repo, release_version):
        raise ValueError('Missing/wrong original TCE base dependency')
    records = {row['path']: row for row in metadata['files']}
    if len(records) != len(metadata['files']) or set(records) != set(files):
        raise ValueError('Single-mod bundle file closure mismatch')
    for name, payload in files.items():
        if records[name]['sha256'] != sha(payload) or records[name]['bytes'] != len(payload):
            raise ValueError('Single-mod bundle byte mismatch: ' + name)
    pk3 = f'tce2/tce2-{release_version}.pk3'
    if [name for name in files if name.endswith('.pk3')] != [pk3]:
        raise ValueError('Exactly one complete mod PK3 required')
    if metadata.get('pk3_path') != pk3 or metadata.get('pk3_sha256') != sha(files[pk3]):
        raise ValueError('Wrong complete mod PK3 fingerprint')
    canonical = check_canonical(files[pk3], release_version, variant, expected, repo, assets=assets)
    if canonical != metadata['client_package']:
        raise ValueError('Canonical mod metadata mismatch')
    native_names = set()
    for target in expected:
        config = target_table(repo)[target]
        qgame = config.get('prefix', '') + 'qagame' + config['suffix']
        path = 'server/' + target + '/' + qgame
        if path not in files:
            raise ValueError('Missing server ABI: ' + target)
        check_module(files[path], 'qagame', target, variant)
        native_names.add(path)
        source = metadata['target_inputs'][target]['metadata']
        if (source.get('target') != target or source.get('variant') != variant or
                source.get('configuration') != 'Release' or source.get('gameplay_bot_runtime') is not True):
            raise ValueError('Missing complete source runtime provenance: ' + target)
        boundary = source.get('bot_core_boundary')
        if not boundary or boundary.get('variant') != variant:
            raise ValueError('Wrong bot core variant: ' + target)
        core_path = 'server/' + target + '/omni-bot/' + boundary['library']
        core_data = files[core_path]
        check_module(core_data, 'omnibot', target, variant)
        validate_symbol_evidence(boundary, core_data)
        if ((b'ExportClientFunctionsFromDLL\0' in core_data) != (variant == 'navstudio') or
                (variant == 'game' and any(marker.encode() + b'\0' in core_data for marker in AUTHORING_COMMANDS)) or
                not all(binding.encode() + b'\0' in core_data for binding in RUNTIME_BINDINGS)):
            raise ValueError('Wrong compiled gameplay/editor bot boundary: ' + target)
        diagnostics = boundary.get('navigation_diagnostic_formats')
        if not diagnostics or any((marker.encode() + b'\0' in core_data) != recorded or
                                  (recorded and variant == 'game') for marker, recorded in diagnostics.items()):
            raise ValueError('Wrong compiled bot diagnostic boundary: ' + target)
        for name in module_names(target, repo).values():
            if any(Path(path).name == name for path in files):
                raise ValueError('Loose client module outside the canonical PK3')
    for name in files:
        if name.startswith('server/') and 'qagame' in Path(name).name and name not in native_names:
            raise ValueError('Unexpected server module: ' + name)
    runtime = json.loads(files['server/common/omni-bot/runtime-manifest.json'])
    expected_payload, _, registry, _, diagnostics = prepare_runtime(repo, variant)
    if (runtime.get('schema') != 2 or runtime.get('policy_sha256') != sha256(repo / 'tools/release/runtime-policy.json') or
            runtime.get('command_registry') != registry or runtime.get('diagnostics') != diagnostics):
        raise ValueError('Stale runtime command/diagnostic policy')
    for relative, expected_data in expected_payload.items():
        output = ('server/common/mapscripts/' + Path(relative).name if relative.startswith('release_mapscripts/')
                  else 'server/common/omni-bot/' + relative)
        if files.get(output) != expected_data:
            raise ValueError('Nonreproducible full server script: ' + relative)
    policy = json.loads((repo / 'tools/release/runtime-policy.json').read_text())
    for relative in policy['editor_files']:
        if ('server/common/omni-bot/' + relative in files) != (variant == 'navstudio'):
            raise ValueError('Wrong runtime editor helper boundary')
    for row in runtime['files']:
        if sha(files['server/common/omni-bot/' + row['path']]) != row['sha256']:
            raise ValueError('Changed runtime manifest entry: ' + row['path'])
    for row in runtime['mod_files']:
        if sha(files['server/common/' + row['path']]) != row['sha256']:
            raise ValueError('Changed server config/mapscript: ' + row['path'])
    return metadata
