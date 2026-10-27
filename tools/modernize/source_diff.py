"""Compare baseline lexical functions; approvals are exact, explicit review inputs.

New functions/files require the separate risk and fixture gates. This is not
complete AST, generated-code, exported-signature or runtime parity proof.
"""
import argparse
import json
from pathlib import Path
from source_review import compare, digest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--baseline', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2])
    parser.add_argument('--reviews', type=Path, help='Root-reviewed exact-hash decisions; no inferred approvals')
    args = parser.parse_args()
    before = json.loads(args.baseline.read_text(encoding='utf-8'))
    reviews = json.loads(args.reviews.read_text(encoding='utf-8')) if args.reviews else None
    report = compare(args.root.resolve(), before, digest(args.baseline), reviews)
    if args.reviews:
        report['review_manifest_sha256'] = digest(args.reviews)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({'unchanged': report['normalized_unchanged_functions'], 'changed': len(report['changes']),
                      'unreviewed': report['unreviewed_functions'], 'missing': len(report['unreviewed_missing_files']),
                      'review_errors': len(report['review_errors']), 'unmatched_reviews': len(report['unmatched_reviews'])}))
    return int(not report['passed'])


if __name__ == '__main__':
    raise SystemExit(main())
