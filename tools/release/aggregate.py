"""Collect ABI inputs into one complete mod/server ZIP per selected variant."""
import argparse
import json
import os
from pathlib import Path
import shutil
import zipfile

from package import ROOT, digest, version
from client_packages import canonical_client, client_assets, read_target, target_table, sha
from single_mod import build_bundle, check_bundle
from validate import archive as validate_archive


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    needs = json.loads(os.environ['NEEDS_JSON'])
    matrix = json.loads(needs['plan']['outputs']['matrix'])['include']
    expected = {(row['target'], row['variant']) for row in matrix}
    variants = {variant for _, variant in expected}
    required = {(target, variant) for target in target_table(ROOT) for variant in variants}
    if not variants or not variants <= {'game', 'navstudio'} or expected != required or len(matrix) != len(expected):
        raise ValueError('Release plan must cover every ABI once per selected variant')
    complete = (needs['plan'].get('result') == 'success' and needs['binaries'].get('result') == 'success'
                and needs.get('navstudio-desktop', {}).get('result', 'skipped') in ('success', 'skipped'))
    rows, sources, reports, seen = [], [], [], set()
    for path in sorted(args.input.rglob('*')):
        relative = path.relative_to(args.input)
        if relative.parts[0] in {f'TCE2-{version(ROOT)}-Complete-Release',
                                 f'TCE2-{version(ROOT)}-Mod-Packages',
                                 f'TCE2-{version(ROOT)}-Release-Closure'} or not path.is_file():
            continue
        if path.name.endswith('-Source.zip'):
            sources.append(path)
        elif path.name.endswith('-Client-Inputs.zip'):
            row = read_target(path, version(ROOT), ROOT)
            pair = (row['metadata']['target'], row['metadata']['variant'])
            if pair in seen or pair not in expected:
                raise ValueError('Duplicate/unplanned collected ABI: ' + repr(pair))
            seen.add(pair);rows.append(row)
            complete = complete and row['metadata']['gameplay_bot_runtime']
        elif path.suffix in ('.json', '.log'):
            reports.append(path)
    if len(sources) != 1:
        raise ValueError('Require exactly one separate source artifact')
    promote = seen == expected
    complete = complete and promote
    args.out.mkdir(parents=True, exist_ok=True)
    stage = args.out / 'bundle';stage.mkdir(exist_ok=False)
    records = []
    def record(path):
        records.append({'path': path.relative_to(args.out).as_posix(), 'sha256': digest(path),
                        'bytes': path.stat().st_size})
    for path in reports:
        destination = stage / 'reports' / path.relative_to(args.input)
        destination.parent.mkdir(parents=True, exist_ok=True);shutil.copy2(path, destination);record(destination)
    packages = {}
    if complete:
        # Validate original runtime/core/native evidence before relocating files.
        for row in rows:
            result = validate_archive(row['path'])
            if not result['complete_runtime']:
                raise ValueError('Incomplete gameplay bot runtime')
        for variant in sorted(variants):
            selected = [row for row in rows if row['metadata']['variant'] == variant]
            frozen = client_assets(version(ROOT), variant, ROOT)
            data, metadata = canonical_client(selected, set(target_table(ROOT)), version(ROOT), variant, ROOT, assets=frozen)
            bundle, bundle_metadata = build_bundle(selected, data, metadata, version(ROOT), variant, ROOT, assets=frozen)
            if check_bundle(bundle, version(ROOT), variant, ROOT, assets=frozen) != bundle_metadata:
                raise ValueError('Final mod/server bundle failed round-trip')
            label = 'Game' if variant == 'game' else 'Editor'
            destination = args.out / f'TCE2-{version(ROOT)}-{label}.zip';destination.write_bytes(bundle)
            checksum = destination.with_suffix('.zip.sha256')
            checksum.write_text(digest(destination) + '  ' + destination.name + '\n', encoding='ascii')
            record(destination);record(checksum)
            packages[variant] = {'archive': destination.name, 'sha256': digest(destination),
                                 'pk3_sha256': sha(data), 'targets': metadata['targets']}
    else:
        # Failed/incomplete runs never publish a partial same-name mod PK3.
        for row in rows:
            destination = stage / 'inputs' / row['path'].name
            destination.parent.mkdir(parents=True, exist_ok=True);shutil.copy2(row['path'], destination);record(destination)
    manifest = {'schema': 2, 'version': version(ROOT), 'complete': complete,
                'jobs': {name: row.get('result') for name, row in needs.items()},
                'github_sha': os.environ.get('GITHUB_SHA'), 'files': records,
                'mod_packages': packages, 'client_inputs_promoted': bool(packages),
                'expected_targets': sorted(expected), 'collected_targets': sorted(seen),
                'source_artifact': {'path': str(sources[0].resolve()), 'sha256': digest(sources[0])}}
    (args.out / 'release-manifest.json').write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf8', newline='\n')
    (args.out / 'release-notes.md').write_text(
        f'TCE2 {version(ROOT)}: manual build from {os.environ.get("GITHUB_SHA", "local review")}.\n\n'
        'Each selected variant has exactly one ZIP with one mod-update PK3, all advertised client ABIs inside it, '
        'and loose server/QGame modules for every advertised ABI. Game and Editor remain separate.\n'
        'The original TCE 0.49b pak0..4 base installation is required and is not bundled.\n'
        'Source and optional NavStudio desktop are separate workflow artifacts. Target transfers are internal inputs.\n'
        'No engine is bundled. Compilation and package closure do not imply original-binary behavioral parity.\n',
        encoding='utf8', newline='\n')
    print(json.dumps({'complete': complete, 'packages': packages}))
    return 0 if complete else 1


if __name__ == '__main__':
    raise SystemExit(main())
