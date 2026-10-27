"""Collect binary/source artifacts; incomplete runs stay explicitly incomplete."""
import argparse
import json
import os
from pathlib import Path
import shutil
import zipfile
from package import digest, version, zip_folder
from client_packages import (canonical_client, json_bytes, promote_target,
                             read_target, target_table, zip_bytes, sha as digest_bytes)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    needs = json.loads(os.environ['NEEDS_JSON'])
    complete = (needs['plan'].get('result') == 'success' and
                needs['binaries'].get('result') == 'success' and
                needs['navstudio-desktop'].get('result') in ('success', 'skipped'))
    matrix = json.loads(needs['plan']['outputs']['matrix'])['include']
    expected_pairs = {(row['target'], row['variant']) for row in matrix}
    variants = {variant for _, variant in expected_pairs}
    required_pairs = {(target, variant) for target in target_table() for variant in variants}
    if (not variants or not variants <= {'game', 'navstudio'} or
            expected_pairs != required_pairs or len(matrix) != len(expected_pairs)):
        raise ValueError('Release plan must cover every client ABI once per selected variant')
    selected, targets, target_pairs = [], [], set()
    for path in sorted(args.input.rglob('*')):
        if path.relative_to(args.input).parts[0] == f'TCE2-{version()}-Complete-Release':
            continue  # A job rerun must not collect its previous aggregate.
        if not path.is_file() or path.suffix not in ('.zip', '.json', '.log'):
            continue
        selected.append(path)
        if path.suffix == '.zip':
            with zipfile.ZipFile(path) as archive:
                if 'manifest.json' in archive.namelist():
                    metadata = json.loads(archive.read('manifest.json'))
                    if 'target' in metadata:
                        pair = (metadata['target'], metadata['variant'])
                        if pair in target_pairs:
                            raise ValueError('Duplicate collected target/variant: ' + repr(pair))
                        target_pairs.add(pair)
                        targets.append(read_target(path, version()))
                        complete = complete and metadata['gameplay_bot_runtime']
    if not any(path.name.endswith('-Source.zip') for path in selected):
        raise ValueError('Missing source release ZIP')
    complete = complete and target_pairs == expected_pairs
    promote = target_pairs == expected_pairs
    shared = {}
    if promote:
        for variant in sorted(variants):
            rows = [row for row in targets if row['metadata']['variant'] == variant]
            shared[variant] = canonical_client(rows, set(target_table()), version(), variant)
    stage = args.out / 'bundle'
    stage.mkdir(parents=True, exist_ok=False)
    records = []
    def record(path):
        records.append({'path': path.relative_to(stage).as_posix(), 'sha256': digest(path),
                        'bytes': path.stat().st_size})
        if path.suffix == '.zip':
            checksum = path.with_suffix('.zip.sha256')
            checksum.write_text(digest(path) + '  ' + path.name + '\n', encoding='ascii')
            record(checksum)
    target_inputs = {row['path']: row for row in targets}
    for path in selected:
        row = target_inputs.get(path)
        if row is not None:
            filename = path.name.replace('-Client-Inputs.zip', '.zip') if promote else path.name
            relative = Path('archives' if promote else 'inputs') / filename
        else:
            relative = Path('archives') / path.name if path.suffix == '.zip' else Path('reports') / path.relative_to(args.input)
        destination = stage / relative
        if destination.exists():
            raise ValueError('Ambiguous collected release filename: ' + destination.name)
        destination.parent.mkdir(parents=True, exist_ok=True)
        if row is not None and promote:
            data, metadata = shared[row['metadata']['variant']]
            promote_target(row, data, metadata, destination, version())
        else:
            shutil.copy2(path, destination)
        record(destination)
    client_packages = {}
    for variant, (data, metadata) in shared.items():
        shared_zip = stage / 'archives' / f'TCE2-{version()}-Shared-Clients-{variant}.zip'
        shared_zip.write_bytes(zip_bytes({
            f'tce2/tce2-{version()}.pk3': data,
            'client-package.json': json_bytes(metadata),
            'README.txt': (f'TCE2 {version()} {variant}: shared client download for every release ABI.\n'
                           'Every final target ZIP of this variant already includes these exact PK3 bytes.\n'
                           + ('Local authoring only; never deploy on public servers.\n' if variant == 'navstudio' else '')).encode()}))
        record(shared_zip)
        client_packages[variant] = {'sha256': digest_bytes(data), 'targets': metadata['targets'],
                                    'archive': shared_zip.relative_to(stage).as_posix()}
    if needs['navstudio-desktop'].get('result') == 'success':
        complete = complete and any(x['path'].endswith('-NavStudio-Desktop-Windows-x64.zip') for x in records)
    manifest = {'schema': 1, 'version': version(), 'complete': complete,
                'jobs': {name: row.get('result') for name, row in needs.items()},
                'github_sha': os.environ.get('GITHUB_SHA'), 'files': records,
                'client_packages': client_packages, 'client_inputs_promoted': promote}
    manifest['expected_targets'] = sorted(expected_pairs)
    manifest['collected_targets'] = sorted(target_pairs)
    text = json.dumps(manifest, indent=2) + '\n'
    (stage / 'release-manifest.json').write_text(text, encoding='utf-8', newline='\n')
    (args.out / 'release-manifest.json').write_text(text, encoding='utf-8', newline='\n')
    notes = (f'TCE2 {version()}: manual build from {os.environ.get("GITHUB_SHA", "local review")}.\n\n'
             'Contains per-target gameplay modules, matched bot runtimes and scripts, separate local editor modules, '
             'the selected NavStudio desktop package, and build sources. See the manifest for actual job results.\n'
             'Installable target ZIPs use one identical shared client PK3 per variant across every client ABI. '
             'Incomplete ABI collections remain client inputs and are not promoted to installable target ZIPs.\n'
             'No engine or BSP map files are bundled. Original-binary parity is not implied by compilation.\n')
    (args.out / 'release-notes.md').write_text(notes, encoding='utf-8', newline='\n')
    label = 'All-Binaries-And-Source' if complete else 'INCOMPLETE-Build-Artifacts'
    archive = args.out / f'TCE2-{version()}-{label}.zip'
    zip_folder(stage, archive)
    print(archive)
    return 0 if complete else 1


if __name__ == '__main__':
    raise SystemExit(main())
