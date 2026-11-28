"""Verify the exact vendored input snapshots without fetching dependencies."""
import argparse
import hashlib
import json
from pathlib import Path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    lock = json.loads((args.root / 'vendor/dependencies.lock.json').read_text(encoding='utf-8'))
    results = []
    for package in lock['packages']:
        directory = args.root / package['path']
        # Preserve the component order used to produce the existing Windows pins.
        # Keep original spelling in each record and break case ties deterministically.
        files = sorted((path.relative_to(directory) for path in directory.rglob('*') if path.is_file()),
                       key=lambda path: (tuple(part.lower() for part in path.parts), path.as_posix()))
        records = ''.join(path.as_posix() + '\0' +
                          hashlib.sha256((directory / path).read_bytes()).hexdigest() + '\n' for path in files)
        actual = hashlib.sha256(records.encode()).hexdigest()
        expected = package['snapshot']
        results.append({'package': package['path'], 'files': len(files), 'sha256': actual,
                        'matches': actual == expected['sha256'] and len(files) == expected['files']})
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'packages': len(results), 'passed': all(item['matches'] for item in results)}))
    raise SystemExit(not all(item['matches'] for item in results))


if __name__ == '__main__':
    main()
