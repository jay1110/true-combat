"""Stage and collect Game binaries into one flat, binary-only archive.

Intermediate manifests and validation evidence stay outside the final ZIP.
The downloadable Actions artifact uploads the resulting binaries directory
directly, so GitHub's ZIP also contains no nested ZIP, directory or metadata.
"""
import argparse
from collections import Counter
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import zipfile

from package import ROOT, digest, module_inputs, version, zip_folder
from verify_omnibot_boundary import (verify_core, validate_symbol_evidence,
                                    verify_module_bytes, macho_architectures)


ROLES = {'cgame', 'ui', 'qagame', 'omnibot'}
MACHO = (b'\xfe\xed\xfa\xce', b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xcf',
         b'\xcf\xfa\xed\xfe', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca',
         b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca')
CORE_IDENTITIES = {
    'windows-x86': ('Windows', 'i386'), 'windows-x64': ('Windows', 'x86_64'),
    'linux-i386': ('Linux', 'i386'), 'linux-x86_64': ('Linux', 'x86_64'),
    'linux-armv7': ('Linux', 'armv7'), 'linux-aarch64': ('Linux', 'aarch64'),
    'android-armv7': ('Android', 'armeabi-v7a'), 'android-arm64': ('Android', 'arm64-v8a'),
    'wasm32': ('Emscripten', 'wasm32'), 'macos-universal': ('Darwin', 'universal'),
}


def target_table(repo=ROOT):
    rows = json.loads((repo / 'tools/release/targets.json').read_text(encoding='utf-8'))['targets']
    ids = [row['id'] for row in rows]
    if len(ids) != len(set(ids)) or any(not re.fullmatch(r'[a-z0-9]+(?:[-_][a-z0-9]+)*', name) for name in ids):
        raise ValueError('Invalid or duplicate release target IDs')
    return {row['id']: row for row in rows}


def safe_filename(name):
    if not isinstance(name, str) or not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]*', name) or name in ('.', '..'):
        raise ValueError('Unsafe binary filename: ' + repr(name))
    return name


def check_binary(path, target, role, config):
    name = safe_filename(path.name)
    if role in {'cgame', 'ui', 'qagame'}:
        expected = config.get('prefix', '') + role + config['suffix']
        if name != expected:
            raise ValueError('Wrong module filename for ' + target + '/' + role)
    elif role != 'omnibot' or not name.startswith('omnibot_et'):
        raise ValueError('Unexpected loose binary role/name: ' + role + '/' + name)
    with path.open('rb') as stream:
        header = stream.read(4096)
    if target.startswith('windows-'):
        valid = header.startswith(b'MZ') and name.endswith('.dll')
        if valid and len(header) >= 64:
            pe = struct.unpack_from('<I', header, 60)[0]
            with path.open('rb') as stream:
                stream.seek(pe)
                coff = stream.read(26)
            expected = (0x14c, 0x10b) if target == 'windows-x86' else (0x8664, 0x20b)
            valid = (len(coff) == 26 and coff[:4] == b'PE\0\0' and
                     (struct.unpack_from('<H', coff, 4)[0], struct.unpack_from('<H', coff, 24)[0]) == expected and
                     bool(struct.unpack_from('<H', coff, 22)[0] & 0x2000))
        else:
            valid = False
    elif target.startswith(('linux-', 'android-')):
        expected = {'linux-i386': (1, 3), 'linux-x86_64': (2, 62),
                    'linux-armv7': (1, 40), 'linux-aarch64': (2, 183),
                    'android-armv7': (1, 40), 'android-arm64': (2, 183)}[target]
        valid = (len(header) >= 20 and header.startswith(b'\x7fELF') and name.endswith('.so') and
                 header[5] == 1 and (header[4], struct.unpack_from('<H', header, 18)[0]) == expected and
                 struct.unpack_from('<H', header, 16)[0] == 3)
    elif target == 'wasm32':
        valid = header.startswith(b'\0asm\x01\0\0\0') and name.endswith('.so')
    elif target.startswith('macos-'):
        valid = header[:4] in MACHO and name.endswith(('_mac', '.so', '.dylib'))
        if valid:
            try:
                valid = sorted(macho_architectures(header)) == ['arm64', 'x86_64']
            except (ValueError, struct.error, TypeError):
                valid = False
    else:
        valid = False
    if not valid:
        raise ValueError('Not a binary for ' + target + ': ' + str(path))
    if role in {'cgame', 'ui', 'qagame'}:
        verify_module_bytes(role, path.read_bytes(), 'game')


def stage_target(module_manifest, core_manifest, output, target, repo=ROOT):
    config = target_table(repo)[target]
    modules = module_inputs(None, config, 'game', module_manifest)
    core = json.loads(Path(core_manifest).read_text(encoding='utf-8'))
    if core.get('schema') != 1 or core.get('variant') != 'game':
        raise ValueError('Loose release requires a Game bot core manifest')
    if (core.get('system'), core.get('architecture')) != CORE_IDENTITIES[target]:
        raise ValueError('Wrong bot core system/architecture for ' + target)
    library = Path(core['library']).resolve()
    paths = {**modules, 'omnibot': library}
    if len({path.name.casefold() for path in paths.values()}) != len(ROLES):
        raise ValueError('Duplicate filenames within target ' + target)
    for role, path in paths.items():
        if not path.is_file():
            raise ValueError('Missing target binary: ' + str(path))
        check_binary(path, target, role, config)
    symbol_map = library.with_suffix('.map')
    boundary = verify_core(library, 'game', symbol_map if symbol_map.is_file() else None)
    output = Path(output)
    if output.exists():
        raise ValueError('Refusing to mix loose target outputs: ' + str(output))
    payload = output / 'binaries'
    payload.mkdir(parents=True)
    files = []
    for role, path in sorted(paths.items()):
        destination = payload / path.name
        shutil.copy2(path, destination)
        files.append({'role': role, 'name': path.name, 'bytes': destination.stat().st_size,
                      'sha256': digest(destination)})
    metadata = {'schema': 1, 'version': version(repo), 'target': target,
                'variant': 'game', 'authoring': False, 'files': files,
                'core_identity': {key: core[key] for key in ('system', 'architecture')},
                'bot_core_boundary': boundary}
    manifest = output / 'loose-target.json'
    manifest.write_text(json.dumps(metadata, indent=2) + '\n', encoding='utf-8', newline='\n')
    return manifest


def read_target(manifest, configs, release_version):
    metadata = json.loads(manifest.read_text(encoding='utf-8'))
    target = metadata.get('target')
    if (metadata.get('schema') != 1 or metadata.get('version') != release_version or
            metadata.get('variant') != 'game' or metadata.get('authoring') is not False or target not in configs):
        raise ValueError('Invalid/non-Game loose target manifest: ' + str(manifest))
    identity = metadata.get('core_identity', {})
    if (identity.get('system'), identity.get('architecture')) != CORE_IDENTITIES[target]:
        raise ValueError('Wrong collected core system/architecture for ' + target)
    rows = metadata.get('files', [])
    if len(rows) != len(ROLES) or {row.get('role') for row in rows} != ROLES:
        raise ValueError('Incomplete loose binary roles: ' + str(manifest))
    root = manifest.parent.resolve()
    expected = {'loose-target.json'}
    found = []
    for row in rows:
        name = safe_filename(row.get('name'))
        path = root / 'binaries' / name
        if path.is_symlink() or not path.resolve().is_relative_to(root) or not path.is_file():
            raise ValueError('Missing/escaped loose binary: ' + str(path))
        if path.stat().st_size != row.get('bytes') or digest(path) != row.get('sha256'):
            raise ValueError('Loose binary fingerprint mismatch: ' + str(path))
        check_binary(path, target, row['role'], configs[target])
        expected.add('binaries/' + name)
        found.append({**row, 'target': target, 'source': path})
    actual = {path.relative_to(root).as_posix() for path in root.rglob('*') if path.is_file()}
    directories = {path.relative_to(root).as_posix() for path in root.rglob('*') if path.is_dir()}
    if expected != actual or directories != {'binaries'}:
        raise ValueError('Unexpected file/directory in loose target: ' + str(manifest))
    core = next(row for row in found if row['role'] == 'omnibot')
    boundary = metadata.get('bot_core_boundary', {})
    if boundary.get('variant') != 'game':
        raise ValueError('Missing Game core boundary evidence')
    validate_symbol_evidence(boundary, core['source'].read_bytes())
    return target, found


def flat_names(rows):
    """Keep native names where unique; tag every conflicting ABI explicitly."""
    counts = Counter(row['name'].casefold() for row in rows)
    names = []
    for row in rows:
        name = safe_filename(row['name'])
        if counts[name.casefold()] > 1:
            path = Path(name)
            name = path.stem + '.' + row['target'] + path.suffix
        names.append(safe_filename(name))
    if len({name.casefold() for name in names}) != len(names):
        raise ValueError('Ambiguous flat target filenames; refusing an overwrite')
    return names


def collect(inputs, output, repo=ROOT):
    inputs, output = Path(inputs).resolve(), Path(output).resolve()
    if output.is_relative_to(inputs):
        raise ValueError('Place final output outside the collected inputs')
    configs, release_version = target_table(repo), version(repo)
    manifests = sorted(inputs.rglob('loose-target.json'))
    rows, seen = [], set()
    for manifest in manifests:
        target, found = read_target(manifest, configs, release_version)
        if target in seen:
            raise ValueError('Duplicate loose target: ' + target)
        seen.add(target)
        rows.extend(found)
    if seen != set(configs):
        raise ValueError('Incomplete loose target matrix: missing ' + repr(sorted(set(configs) - seen)))
    names = flat_names(rows)
    if output.exists():
        raise ValueError('Refusing to mix final loose outputs: ' + str(output))
    payload = output / 'binaries'
    payload.mkdir(parents=True)
    records = []
    for row, name in sorted(zip(rows, names), key=lambda pair: pair[1]):
        destination = payload / name
        shutil.copy2(row['source'], destination)
        records.append({key: row[key] for key in ('target', 'role', 'name', 'sha256', 'bytes')} | {'archive_name': name})
    archive = output / f'TCE2-{release_version}-Game-Loose-Binaries.zip'
    zip_folder(payload, archive)
    with zipfile.ZipFile(archive) as zipped:
        if set(zipped.namelist()) != set(names) or any(info.is_dir() for info in zipped.infolist()):
            raise ValueError('Final loose ZIP contains an unexpected file/directory')
    (output / 'loose-report.json').write_text(json.dumps({
        'schema': 1, 'version': release_version, 'variant': 'game', 'targets': sorted(seen),
        'binary_count': len(records), 'archive': archive.name, 'files': records,
        'metadata_inside_archive': False,
    }, indent=2) + '\n', encoding='utf-8', newline='\n')
    return archive


def cleanup_intermediates(repository, run_id, request, repo=ROOT):
    """Remove only this successful run's exact temporary matrix artifacts."""
    if not re.fullmatch(r'[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+', repository) or not str(run_id).isdigit():
        raise ValueError('Invalid Actions repository/run identity')
    release_version = version(repo)
    expected = {f'loose-input-{release_version}-{target}' for target in target_table(repo)}
    artifacts, page = [], 1
    while True:
        response = request('GET', f'repos/{repository}/actions/runs/{run_id}/artifacts?per_page=100&page={page}')
        batch = response['artifacts']
        artifacts.extend(batch)
        if len(batch) < 100:
            break
        page += 1
    final = f'TCE2-{release_version}-Game-Loose-Binaries'
    if not any(artifact['name'] == final and not artifact.get('expired') for artifact in artifacts):
        raise ValueError('Final loose artifact is absent; keeping all intermediate evidence')
    selected = [artifact for artifact in artifacts if artifact['name'] in expected]
    for artifact in selected:
        request('DELETE', f"repos/{repository}/actions/artifacts/{int(artifact['id'])}")
    return len(selected)


def github_request(method, endpoint):
    result = subprocess.run(['gh', 'api', '--method', method, endpoint],
                            check=True, capture_output=True, text=True)
    return json.loads(result.stdout) if result.stdout.strip() else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command', required=True)
    stage = commands.add_parser('stage')
    stage.add_argument('--module-manifest', type=Path, required=True)
    stage.add_argument('--core-manifest', type=Path, required=True)
    stage.add_argument('--target', required=True)
    stage.add_argument('--out', type=Path, required=True)
    aggregate = commands.add_parser('collect')
    aggregate.add_argument('--input', type=Path, required=True)
    aggregate.add_argument('--out', type=Path, required=True)
    commands.add_parser('cleanup-intermediates')
    args = parser.parse_args()
    if args.command == 'stage':
        print(stage_target(args.module_manifest, args.core_manifest, args.out, args.target))
    elif args.command == 'collect':
        print(collect(args.input, args.out))
    else:
        print('Removed intermediate artifacts:', cleanup_intermediates(
            os.environ['GITHUB_REPOSITORY'], os.environ['GITHUB_RUN_ID'], github_request))


if __name__ == '__main__':
    main()
