"""Mod delta resources from pak/, with an explicit local-editor overlay.

The original TCE installation supplies stock assets. No reader ever falls back
to pak_original_tce, src/assets or ui-modern. Stock hashes are metadata only.
"""
from functools import lru_cache
import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]


def _version(repo):
    source = (Path(repo) / 'src/game/tce_version.h').read_text(encoding='utf8')
    found = re.search(r'^#define TCE_VERSION "([0-9]+[.][0-9]+[.][0-9]+)"', source, re.M)
    if not found:
        raise ValueError('Missing mod version for base reference')
    return found[1]


@lru_cache(maxsize=8)
def _reference(path, size, mtime, ctime):
    data = Path(path).read_bytes()
    reference = json.loads(data)
    if len(data) != size or reference.get('schema') != 1:
        raise ValueError('Invalid stock base reference')
    return reference, hashlib.sha256(data).hexdigest()


def base_game_reference(repo=ROOT, release_version=None):
    path = Path(repo) / 'tools/assets/base-game-reference.json'
    if not path.is_file() or path.is_symlink():
        raise ValueError('Missing checked-in stock base signature manifest')
    stat = path.stat()
    reference, digest = _reference(str(path.resolve()), stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns)
    expected = release_version or _version(repo)
    if (reference.get('release_version') != expected or reference.get('baseline_id') != 'tce-0.49b-pak0-4'
            or not isinstance(reference.get('files'), dict) or not reference['files']
            or not isinstance(reference.get('source_packages'), list) or not reference['source_packages']):
        raise ValueError('Wrong/incomplete version-bound stock base signature manifest')
    folded = set()
    for name, row in reference['files'].items():
        normalized = name.replace('\\', '/').casefold()
        if (normalized.startswith('/') or ':' in normalized or '..' in normalized.split('/') or
                normalized in folded or not isinstance(row, dict) or
                not isinstance(row.get('bytes'), int) or row['bytes'] < 0 or
                not re.fullmatch('[0-9a-f]{64}', row.get('sha256', ''))):
            raise ValueError('Unsafe/incomplete stock resource signature: ' + name)
        folded.add(normalized)
    packages = reference['source_packages']
    if {row.get('name', '').casefold() for row in packages} != {f'pak{i}.pk3' for i in range(5)} or len(packages) != 5:
        raise ValueError('Incomplete original TCE base-package signatures')
    for row in packages:
        if (not isinstance(row.get('bytes'), int) or row['bytes'] <= 0 or
                not re.fullmatch('[0-9a-f]{64}', row.get('sha256', ''))):
            raise ValueError('Invalid original TCE base-package signature')
    return reference, digest


def base_dependency(repo=ROOT, release_version=None):
    release_version = release_version or _version(repo)
    reference, digest = base_game_reference(repo, release_version)
    return {'schema': 1, 'kind': 'mod-delta', 'baseline_id': reference['baseline_id'],
            'release_version': release_version, 'reference_sha256': digest,
            'source_packages': reference['source_packages']}


@lru_cache(maxsize=4096)
def _file_digest(path, size, mtime, ctime):
    data = Path(path).read_bytes()
    if len(data) != size:
        raise ValueError('Delta asset changed during validation')
    return hashlib.sha256(data).hexdigest()


def _reject_stock_copies(paths, repo):
    reference, _digest = base_game_reference(repo)
    stock = {name.replace('\\', '/').casefold(): row for name, row in reference['files'].items()}
    for name, path in paths.items():
        row = stock.get(name.casefold())
        if row is None:
            continue
        stat = path.stat()
        if (stat.st_size == row['bytes'] and
                _file_digest(str(path.resolve()), stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns) == row['sha256']):
            raise ValueError('Unchanged original TCE resource in mod delta: ' + name)


def _paths(folder, required=True):
    if not folder.is_dir():
        if required:
            raise ValueError('Missing mod delta resource directory: ' + str(folder))
        return {}
    result = {}; folded = set()
    for path in sorted(folder.rglob('*')):
        if path.is_symlink():
            raise ValueError('Symlink in mod resources: ' + str(path))
        if path.is_dir():
            continue
        name = path.relative_to(folder).as_posix()
        if ('\\' in name or ':' in name or any(part.startswith('.') for part in Path(name).parts)
                or name.casefold() in folded):
            raise ValueError('Unsafe/case-colliding resource: ' + name)
        if (path.suffix.lower() in ('.dll', '.so', '.dylib', '.exe', '.wasm', '.zip', '.pk3', '.pdb', '.pyc')
                or re.search(r'(?:[.]so(?:[.].*)?|_mac)$', path.name, re.I)
                or path.name in ('client.manifest', 'client-abis.json')):
            raise ValueError('Native/generated/nested package in resource source: ' + name)
        result[name] = path; folded.add(name.casefold())
    if required and not result:
        raise ValueError('Empty mod delta resource directory: ' + str(folder))
    return result


def resource_paths(variant, repo=ROOT):
    if variant not in ('game', 'navstudio'):
        raise ValueError('Unknown resource variant')
    repo = Path(repo)
    result = _paths(repo / 'pak')
    if any(re.fullmatch(r'ui/tce_(?:omnibot|ob_.*|waypoint.*|goal.*|editor.*)[.]menu', name.casefold())
           for name in result):
        raise ValueError('Editor menu leaked into public pak/')
    if variant == 'navstudio':
        overlay = _paths(repo / 'navstudio/editor-pak')
        folded = {name.casefold(): name for name in result}
        for name, path in overlay.items():
            if name.casefold() in folded and folded[name.casefold()] != name:
                raise ValueError('Case-colliding editor overlay: ' + name)
            result[name] = path
    _reject_stock_copies(result, repo)
    return result


@lru_cache(maxsize=4)
def _fingerprint(signature):
    files = {}
    for name, absolute, size, _mtime, _ctime in signature:
        data = Path(absolute).read_bytes()
        if len(data) != size:
            raise ValueError('Resource changed during inventory: ' + name)
        files[name] = {'bytes': size, 'sha256': hashlib.sha256(data).hexdigest()}
    encoded = (json.dumps(files, sort_keys=True, separators=(',', ':')) + '\n').encode()
    return {'schema': 1, 'files': len(files), 'bytes': sum(row['bytes'] for row in files.values()),
            'sha256': hashlib.sha256(encoded).hexdigest()}


def resource_fingerprint(variant, repo=ROOT):
    paths = resource_paths(variant, repo)
    signature = []
    for name, path in sorted(paths.items()):
        stat = path.stat()
        signature.append((name, str(path.resolve()), stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns))
    reference, digest = base_game_reference(repo)
    result = dict(_fingerprint(tuple(signature)))
    result.update(kind='mod-delta', base_reference_sha256=digest, baseline_id=reference['baseline_id'])
    if variant == 'navstudio':
        path = Path(repo) / 'tools/assets/base-ui-reference.json'
        if not path.is_file():
            raise ValueError('Missing checked-in stock UI signature manifest')
        result['base_ui_reference_sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def resource_assets(release_version, variant, repo=ROOT):
    files = {name: path.read_bytes() for name, path in resource_paths(variant, repo).items()}
    files['tce2/' + release_version + '/client.manifest'] = release_version.encode('ascii')
    dependency = base_dependency(repo, release_version)
    files['tce2/' + release_version + '/base-game.json'] = (json.dumps(dependency, sort_keys=True, indent=2) + '\n').encode('utf8')
    if variant == 'navstudio':
        _validate_editor_resources(files, release_version, repo)
    return files


def _validate_editor_resources(files, release_version, repo=ROOT):
    assets_tools = str(ROOT / 'tools/assets')
    if assets_tools not in sys.path:
        sys.path.insert(0, assets_tools)
    from editor_ui import validate_editor_ui
    reference = json.loads((Path(repo) / 'tools/assets/base-ui-reference.json').read_bytes())
    return validate_editor_ui(files, release_version=release_version, reference=reference)


def package_directory(directory, variant, output, repo=ROOT):
    # Late imports avoid a cycle: client_packages consumes this resource reader.
    from client_packages import check_module, module_names, target_table, zip_bytes
    from package import version
    directory, output = Path(directory), Path(output)
    native = {}
    found = 0
    for target in target_table(repo):
        pair = module_names(target, repo)
        present = [name for name in pair.values() if (directory / name).is_file()]
        if present and len(present) != 2:
            raise ValueError('Incomplete native client pair: ' + target)
        if not present:
            continue
        for role, name in pair.items():
            data = (directory / name).read_bytes()
            check_module(data, role, target, variant)
            native[name] = data
        found += 1
    if not found:
        raise ValueError('No supported native client modules')
    files = resource_assets(version(repo), variant, repo)
    if files.keys() & native.keys():
        raise ValueError('Native/resource collision')
    files.update(native)
    output.write_bytes(zip_bytes(files))
    return {'path': str(output), 'targets': found, 'files': len(files)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant', choices=('game', 'navstudio'), required=True)
    parser.add_argument('--module-directory', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if args.module_directory or args.output:
        if not args.module_directory or not args.output:
            parser.error('--module-directory and --output are required together')
        result = package_directory(args.module_directory, args.variant, args.output)
    else:
        result = resource_fingerprint(args.variant)
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
