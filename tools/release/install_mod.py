"""Install one unpacked TCE2 release variant into an existing mod directory.

Run with the game stopped. This script never downloads or launches anything.
"""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import shutil
import zipfile


def install(bundle, target, mod, classic_client=False):
    bundle, mod = Path(bundle).resolve(), Path(mod).resolve()
    if not mod.is_dir() or mod.name.lower() != 'tce2':
        raise ValueError('--mod-dir must be an existing tce2 directory')
    manifest = json.loads((bundle / 'manifest.json').read_text(encoding='utf8'))
    if target not in manifest['targets']:
        raise ValueError('Target is absent from this release')
    dependency = manifest.get('base_dependency')
    if manifest.get('requires_original_tce') is not True or not dependency or dependency.get('kind') != 'mod-delta':
        raise ValueError('Missing original TCE base dependency')
    # Validate the installed base before any mutation. No stock data is fetched
    # from the repository, another mod directory or a network location.
    for package in dependency['source_packages']:
        name = package['name'];path = mod / name
        if Path(name).name != name or not path.resolve().is_relative_to(mod) or not path.is_file() or path.is_symlink():
            raise ValueError('Install the original TCE 0.49b base first; missing package: ' + name)
        with path.open('rb') as stream:
            actual = hashlib.file_digest(stream, 'sha256').hexdigest()
        if path.stat().st_size != package['bytes'] or actual != package['sha256']:
            raise ValueError('Original TCE base package differs from required version: ' + name)
    payload = {}
    paths = [bundle / manifest['pk3_path']]
    for folder in (bundle / 'server/common', bundle / 'server' / target):
        if folder.is_dir():
            paths.extend(path for path in folder.rglob('*') if path.is_file())
    records = {row['path']: row for row in manifest['files']}
    for path in paths:
        if path.is_symlink() or not path.resolve().is_relative_to(bundle):
            raise ValueError('Unsafe release source')
        name = path.relative_to(bundle).as_posix();data = path.read_bytes()
        if name not in records or hashlib.sha256(data).hexdigest() != records[name]['sha256']:
            raise ValueError('Changed release source: ' + name)
        if name.startswith('tce2/'):
            relative = name[len('tce2/'):]
        else:
            relative = '/'.join(name.split('/')[2:])
        if relative in payload and payload[relative] != data:
            raise ValueError('Colliding install payload: ' + relative)
        payload[relative] = data
    if classic_client:
        with zipfile.ZipFile(bundle / manifest['pk3_path']) as archive:
            modules = manifest['client_package']['modules']
            selected = [name for name, row in modules.items() if row['target'] == target]
            if len(selected) != 2:
                raise ValueError('Incomplete selected client ABI')
            for name in selected:
                data = archive.read(name)
                if hashlib.sha256(data).hexdigest() != modules[name]['sha256']:
                    raise ValueError('Changed native client module: ' + name)
                payload[name] = data
    # Validate every destination before the first mutation.
    for name in payload:
        path = mod / name
        if path.is_symlink() or not path.resolve().is_relative_to(mod):
            raise ValueError('Unsafe install destination: ' + name)
    backup = mod.parent / ('tce2-backup-' + datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ'))
    changed = []
    for name, data in sorted(payload.items()):
        path = mod / name
        if path.exists() and path.read_bytes() == data:
            continue
        if path.exists():
            saved = backup / name;saved.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, saved)
        path.parent.mkdir(parents=True, exist_ok=True);path.write_bytes(data);changed.append(name)
    return {'target': target, 'mod_dir': str(mod), 'changed': changed,
            'backup': str(backup) if backup.exists() else None, 'classic_client': classic_client}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', required=True)
    parser.add_argument('--mod-dir', type=Path, required=True)
    parser.add_argument('--classic-client', action='store_true')
    args = parser.parse_args()
    print(json.dumps(install(Path(__file__).resolve().parent, args.target, args.mod_dir,
                             args.classic_client), indent=2))


if __name__ == '__main__':
    main()
