#!/usr/bin/env python3
"""Hash all original pak UI files, including unused/editor menus."""
import argparse
import json
from pathlib import Path
from semantic import inventory


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--out', type=Path, required=True)
    args = p.parse_args()
    if args.out.resolve().is_relative_to(args.source.resolve()):
        p.error('Inventory output must be outside the immutable original source')
    value = inventory(args.source)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(value, indent=2, ensure_ascii=True)+'\n', encoding='utf-8')
    print(json.dumps({k: value[k] for k in ('file_count','menu_count','semantic_file_count','counts')}))


if __name__ == '__main__':
    main()
