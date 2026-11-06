"""Build a single mod/server ZIP from existing Release module/core manifests.

No compilation, downloads or game processes. A diagnostic ABI subset must be
requested explicitly; the manual CI release always requires the full target set.
"""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile

from client_packages import canonical_client, client_assets, read_target, target_table
from package import ROOT, module_inputs, package_modules, version
from resource_assets import resource_fingerprint
from single_mod import build_bundle, check_bundle
from validate import archive as validate_archive


def stamp(path):
    path = Path(path).resolve();stat = path.stat()
    return {'path': str(path), 'bytes': stat.st_size, 'mtime_ns': stat.st_mtime_ns,
            'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--inputs', type=Path, required=True,
                        help='JSON list of {target,module_manifest,core_manifest}')
    parser.add_argument('--variant', choices=('game', 'navstudio'), required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--receipt', type=Path, required=True)
    parser.add_argument('--allow-subset', action='store_true')
    args = parser.parse_args()
    entries = json.loads(args.inputs.read_text(encoding='utf8'))
    targets = [row['target'] for row in entries]
    if (not targets or len(targets) != len(set(targets)) or not set(targets) <= set(target_table()) or
            (not args.allow_subset and set(targets) != set(target_table()))):
        raise ValueError('Require exactly all release ABIs, or explicit --allow-subset')
    watched = [args.inputs]
    for row in entries:
        watched += [row['module_manifest'], row['core_manifest']]
        if row.get('geoip_database'):
            watched.append(row['geoip_database'])
        discovered = module_inputs(None, target_table()[row['target']], args.variant, row['module_manifest'])
        watched += list(discovered.values())
        core = json.loads(Path(row['core_manifest']).read_text(encoding='utf8'))
        watched.append(core['library'])
    before = {str(Path(path).resolve()): stamp(path) for path in watched}
    tree = resource_fingerprint(args.variant)
    with tempfile.TemporaryDirectory(prefix='tce2-native-inputs-') as temporary:
        rows = []
        for entry in entries:
            archive = package_modules(None, Path(temporary) / entry['target'], entry['target'], args.variant,
                                      core_manifest=Path(entry['core_manifest']),
                                      module_manifest=Path(entry['module_manifest']),
                                      geoip=Path(entry['geoip_database']) if entry.get('geoip_database') else None)
            result = validate_archive(archive)
            if not result['complete_runtime']:
                raise ValueError('Missing complete matched gameplay runtime')
            rows.append(read_target(archive, version()))
        frozen = client_assets(version(), args.variant)
        client, metadata = canonical_client(rows, targets, version(), args.variant, assets=frozen)
        data, bundle_metadata = build_bundle(rows, client, metadata, version(), args.variant,
                                             expected_targets=targets, assets=frozen)
        if check_bundle(data, version(), args.variant, expected_targets=targets, assets=frozen) != bundle_metadata:
            raise ValueError('Single mod/server ZIP failed verification')
        if resource_fingerprint(args.variant) != tree or any(stamp(path) != value for path, value in before.items()):
            raise ValueError('Input modules/resources changed while packaging')
        # Only the final ZIP is persistent. No standalone PK3 or target archive.
        args.out.parent.mkdir(parents=True, exist_ok=True);args.out.write_bytes(data)
    report = {'passed': True, 'version': version(), 'variant': args.variant,
              'scope': metadata['scope'], 'targets': sorted(targets), 'inputs_stable': True,
              'native_inputs': before, 'resource_tree': tree,
              'output': stamp(args.out), 'pk3_sha256': bundle_metadata['pk3_sha256'],
              'client_modules': metadata['modules'], 'persistent_pk3_files': 0,
              'persistent_target_transfers': 0, 'builds_started': 0, 'game_processes_started': 0}
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(report, indent=2) + '\n', encoding='utf8', newline='\n')
    args.out.with_suffix('.zip.sha256').write_text(report['output']['sha256'] + '  ' + args.out.name + '\n', encoding='ascii')
    print(json.dumps({key: report[key] for key in ('passed', 'version', 'variant', 'scope', 'targets', 'output', 'pk3_sha256')}))


if __name__ == '__main__':
    main()
