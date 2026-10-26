"""Collect binary/source artifacts; incomplete runs stay explicitly incomplete."""
import argparse
import json
import os
from pathlib import Path
import shutil
import zipfile
from package import digest, version, zip_folder


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    needs = json.loads(os.environ['NEEDS_JSON'])
    complete = (needs['plan'].get('result') == 'success' and
                needs['binaries'].get('result') == 'success' and
                needs['navstudio-desktop'].get('result') in ('success', 'skipped'))
    stage = args.out / 'bundle'
    stage.mkdir(parents=True, exist_ok=False)
    records, target_pairs = [], set()
    for path in sorted(args.input.rglob('*')):
        if not path.is_file() or path.suffix not in ('.zip', '.sha256', '.json', '.log'):
            continue
        relative = Path('archives') / path.name if path.suffix in ('.zip', '.sha256') else Path('reports') / path.relative_to(args.input)
        target = stage / relative
        if target.exists():
            raise ValueError('Ambiguous collected release filename: ' + path.name)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        records.append({'path': relative.as_posix(), 'sha256': digest(target), 'bytes': target.stat().st_size})
        if path.suffix == '.zip':
            with zipfile.ZipFile(path) as archive:
                if 'manifest.json' in archive.namelist():
                    metadata = json.loads(archive.read('manifest.json'))
                    target_pairs.add((metadata['target'], metadata['variant']))
                    complete = complete and metadata['gameplay_bot_runtime']
    if not any(x['path'].endswith('-Source.zip') for x in records):
        raise ValueError('Missing source release ZIP')
    matrix = json.loads(needs['plan']['outputs']['matrix'])['include']
    expected_pairs = {(row['target'], row['variant']) for row in matrix}
    complete = complete and target_pairs == expected_pairs
    if needs['navstudio-desktop'].get('result') == 'success':
        complete = complete and any(x['path'].endswith('-NavStudio-Desktop-Windows-x64.zip') for x in records)
    manifest = {'schema': 1, 'version': version(), 'complete': complete,
                'jobs': {name: row.get('result') for name, row in needs.items()},
                'github_sha': os.environ.get('GITHUB_SHA'), 'files': records}
    manifest['expected_targets'] = sorted(expected_pairs)
    manifest['collected_targets'] = sorted(target_pairs)
    text = json.dumps(manifest, indent=2) + '\n'
    (stage / 'release-manifest.json').write_text(text, encoding='utf-8', newline='\n')
    (args.out / 'release-manifest.json').write_text(text, encoding='utf-8', newline='\n')
    notes = (f'TCE2 {version()}: manual build from {os.environ.get("GITHUB_SHA", "local review")}.\n\n'
             'Contains per-target gameplay modules, matched bot runtimes and scripts, separate local editor modules, '
             'the selected NavStudio desktop package, and build sources. See the manifest for actual job results.\n'
             'No engine or BSP map files are bundled. Original-binary parity is not implied by compilation.\n')
    (args.out / 'release-notes.md').write_text(notes, encoding='utf-8', newline='\n')
    label = 'All-Binaries-And-Source' if complete else 'INCOMPLETE-Build-Artifacts'
    archive = args.out / f'TCE2-{version()}-{label}.zip'
    zip_folder(stage, archive)
    print(archive)
    return 0 if complete else 1


if __name__ == '__main__':
    raise SystemExit(main())
