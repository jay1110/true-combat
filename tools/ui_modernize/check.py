#!/usr/bin/env python3
"""Compare a complete edited UI mirror against its immutable original inventory."""
import argparse
import json
from pathlib import Path
from semantic import compare


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--original', type=Path, required=True)
    p.add_argument('--edited', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--require-styled', action='store_true', help='Require actual non-comment edits in every original menu')
    args = p.parse_args()
    if any(args.out.resolve().is_relative_to(folder.resolve()) for folder in (args.original, args.edited)):
        p.error('Check output must be outside original and edited source folders')
    try:
        value = compare(json.loads(args.baseline.read_text(encoding='utf-8')), args.original, args.edited, args.require_styled)
    except (ValueError, OSError, KeyError, TypeError) as error:
        value = {'passed': False, 'errors': [str(error)]}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(value, indent=2, ensure_ascii=True)+'\n', encoding='utf-8')
    print(json.dumps({'passed': value['passed'], 'errors': value['errors']}))
    return 0 if value['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
