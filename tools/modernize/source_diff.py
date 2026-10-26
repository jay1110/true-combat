"""Compare conservatively indexed function bodies against a risk-scan baseline.

Moved compatibility headers are resolved through an explicit relocation map.
This checks normalized source identity, not generated-code or runtime parity.
"""
import argparse
import json
from pathlib import Path
import collections

from risk_scan import scan


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    args = parser.parse_args()
    before = json.loads(args.baseline.read_text(encoding='utf-8'))
    changes, equal, missing = [], 0, []
    for old_file in before['files']:
        path = args.root / old_file['path']
        # The central compatibility relocation is intentionally one to one.
        relocated = args.root / 'compat/legacy' / Path(old_file['path']).relative_to('src')
        if relocated.is_file():
            path = relocated
        if not path.is_file():
            missing.append(old_file['path'])
            continue
        new_file = scan(path, args.root)
        old_groups, new_groups = collections.defaultdict(list), collections.defaultdict(list)
        for item in old_file['functions']:
            old_groups[item['name']].append(item)
        for item in new_file['functions']:
            new_groups[item['name']].append(item)
        for name, items in old_groups.items():
            current = new_groups.get(name, [])
            for i, item in enumerate(items):
                if i < len(current) and item['normalized_sha256'] == current[i]['normalized_sha256']:
                    equal += 1
                else:
                    changes.append({'path': old_file['path'], 'current_path': path.relative_to(args.root).as_posix(),
                                    'name': name, 'score': item['score'], 'line': item['line']})
    allowed = {('src/game/tce_lua_files.c', 'ListLocal'):
               'Class A: OS enumeration moved to platform/platform_io.h; filters and limits retained; directory golden tested.'}
    for change in changes:
        change['reviewed_reason'] = allowed.get((change['path'], change['name']))
    report = {'normalized_unchanged_functions': equal, 'changes': changes, 'missing_files': missing,
              'limits': 'Conservative lexical index and token hashes, not complete AST coverage or runtime parity.'}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'unchanged': equal, 'changed': len(changes), 'unreviewed': sum(not item['reviewed_reason'] for item in changes), 'missing': len(missing)}))
    raise SystemExit(bool(missing) or any(not item['reviewed_reason'] for item in changes))


if __name__ == '__main__':
    main()
