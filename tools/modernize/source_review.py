"""Exact review decisions for baseline-indexed function and file differences."""
import collections
import hashlib
from pathlib import Path, PurePosixPath
import re
from risk_scan import scan

SHA = re.compile(r'^[0-9a-f]{64}$')


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def relative_name(value):
    return (isinstance(value, str) and bool(value) and '\\' not in value and ':' not in value
            and not PurePosixPath(value).is_absolute() and all(p not in ('', '.', '..') for p in value.split('/')))


def proof_input_paths(root, document):
    """Return the explicit proof closure for the bundle's global input freeze."""
    if not isinstance(document, dict) or not isinstance(document.get('proofs', {}), dict):
        raise ValueError('Review proofs must be a mapping')
    paths = set()
    for proof in document.get('proofs', {}).values():
        if not isinstance(proof, dict) or not isinstance(proof.get('inputs', []), list):
            raise ValueError('Proof inputs must be a list')
        for entry in proof.get('inputs', []):
            if not isinstance(entry, dict) or not relative_name(entry.get('path')):
                raise ValueError('Invalid proof input path')
            paths.add(root / entry['path'])
    return sorted(paths)


def review_policy(root, document, baseline_sha256):
    errors, functions, removals = [], {}, {}
    if document is None:
        return functions, removals, errors
    if not isinstance(document, dict) or document.get('schema') != 1 or document.get('baseline_inventory_sha256') != baseline_sha256:
        return {}, {}, ['Review schema or exact baseline inventory SHA256 mismatch']
    proofs = document.get('proofs', {})
    if not isinstance(proofs, dict):
        return {}, {}, ['Review proofs must be a mapping']
    for proof_id, proof in proofs.items():
        if (not isinstance(proof_id, str) or not proof_id or not isinstance(proof, dict)
                or not isinstance(proof.get('description'), str) or not proof['description'].strip()):
            errors.append('Invalid proof descriptor: ' + str(proof_id))
            continue
        inputs = proof.get('inputs', [])
        if not isinstance(inputs, list):
            errors.append('Proof inputs must be a list: ' + proof_id)
            continue
        for entry in inputs:
            if not isinstance(entry, dict):
                errors.append('Invalid proof input: ' + proof_id)
                continue
            name, expected = entry.get('path'), entry.get('sha256')
            if (not relative_name(name) or not isinstance(expected, str) or not SHA.fullmatch(expected)
                    or not (root / name).is_file() or digest(root / name) != expected):
                errors.append('Missing or changed proof input: ' + str(proof_id) + '/' + str(name))
    for kind, destination in (('functions', functions), ('removed_files', removals)):
        entries = document.get(kind, [])
        if not isinstance(entries, list):
            errors.append(kind + ' must be a list')
            continue
        for entry in entries:
            if not isinstance(entry, dict):
                errors.append('Invalid review entry in ' + kind)
                continue
            name, ids = entry.get('path'), entry.get('proof_ids')
            valid = (relative_name(name) and isinstance(entry.get('reason'), str) and bool(entry['reason'].strip())
                     and isinstance(ids, list) and bool(ids) and all(isinstance(i, str) and i in proofs for i in ids))
            if kind == 'functions':
                old, new = entry.get('baseline_normalized_sha256'), entry.get('current_normalized_sha256')
                valid = (valid and 'current_normalized_sha256' in entry
                         and isinstance(entry.get('name'), str) and bool(entry['name'])
                         and type(entry.get('ordinal')) is int and entry['ordinal'] >= 0
                         and isinstance(old, str) and bool(SHA.fullmatch(old))
                         and (new is None or isinstance(new, str) and bool(SHA.fullmatch(new)))
                         and old != new and relative_name(entry.get('current_path')))
                key = (name, entry.get('name'), entry.get('ordinal'))
            else:
                old = entry.get('baseline_sha256')
                valid = valid and isinstance(old, str) and bool(SHA.fullmatch(old))
                key = name
            if not valid or key in destination:
                errors.append('Invalid or duplicate ' + kind + ' review: ' + str(key))
            else:
                destination[key] = entry
    return functions, removals, errors


def compare(root, before, baseline_sha256, reviews=None):
    approved, removed, review_errors = review_policy(root, reviews, baseline_sha256)
    if not any(row.get('functions') for row in before['files']):
        review_errors.append('Baseline contains no indexed functions; no function-identity proof')
    changes, equal, missing, reviewed_missing, unreviewed_missing = [], 0, [], [], []
    used_functions, used_removals = set(), set()
    for old_file in before['files']:
        name = old_file['path']
        path = root / name
        if Path(name).parts[0] == 'src':
            relocated = root / 'compat/legacy' / Path(name).relative_to('src')
            if relocated.is_file():
                path = relocated
        if not path.is_file():
            missing.append(name)
            decision = removed.get(name)
            if not review_errors and decision and decision['baseline_sha256'] == old_file['sha256']:
                reviewed_missing.append({'path': name, 'baseline_sha256': old_file['sha256'],
                                         'reviewed_reason': decision['reason'], 'proof_ids': decision['proof_ids']})
                used_removals.add(name)
            else:
                unreviewed_missing.append(name)
            continue
        new_file = scan(path, root)
        old_groups, new_groups = collections.defaultdict(list), collections.defaultdict(list)
        for item in old_file['functions']:
            old_groups[item['name']].append(item)
        for item in new_file['functions']:
            new_groups[item['name']].append(item)
        for function_name, items in old_groups.items():
            current = new_groups.get(function_name, [])
            for ordinal, item in enumerate(items):
                actual = current[ordinal]['normalized_sha256'] if ordinal < len(current) else None
                if item['normalized_sha256'] == actual:
                    equal += 1
                    continue
                change = {'path': name, 'current_path': path.relative_to(root).as_posix(),
                          'name': function_name, 'ordinal': ordinal, 'score': item['score'], 'line': item['line'],
                          'baseline_normalized_sha256': item['normalized_sha256'],
                          'current_normalized_sha256': actual, 'reviewed_reason': None}
                key = (name, function_name, ordinal)
                decision = approved.get(key)
                if (not review_errors and decision and all(decision.get(k) == change[k] for k in
                        ('current_path', 'baseline_normalized_sha256', 'current_normalized_sha256'))):
                    change.update(reviewed_reason=decision['reason'], proof_ids=decision['proof_ids'])
                    used_functions.add(key)
                changes.append(change)
    unmatched = [{'path': key[0], 'name': key[1], 'ordinal': key[2]} for key in sorted(set(approved) - used_functions)]
    unmatched += [{'removed_file': name} for name in sorted(set(removed) - used_removals)]
    unreviewed = sum(not item['reviewed_reason'] for item in changes)
    return {'normalized_unchanged_functions': equal, 'changes': changes, 'missing_files': missing,
            'unreviewed_missing_files': unreviewed_missing, 'reviewed_removed_files': reviewed_missing,
            'baseline_inventory_sha256': baseline_sha256, 'review_errors': review_errors,
            'unmatched_reviews': unmatched, 'unreviewed_functions': unreviewed,
            'passed': not (unreviewed or unreviewed_missing or review_errors or unmatched),
            'limits': 'Baseline-indexed lexical functions only; new functions/files need separate risk/fixture review. Not complete AST, generated-code, ABI or runtime parity.'}
