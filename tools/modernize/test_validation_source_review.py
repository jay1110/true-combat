"""Exact review inputs cannot approve drift, another function or a removal."""
from copy import deepcopy
import json
from pathlib import Path
import tempfile
import unittest

from risk_scan import scan
from source_review import compare, digest


class SourceReviewTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / 'src/game/sample.c'
        self.path.parent.mkdir(parents=True)
        self.original = 'int alpha(void)\n{ return 1; }\nint beta(void)\n{ return 2; }\n'
        self.path.write_text(self.original, newline='\n')
        self.baseline = {'files': [scan(self.path, self.root)]}
        inventory = self.root / 'inventory.json'
        inventory.write_text(json.dumps(self.baseline))
        self.inventory_hash = digest(inventory)
        self.proof = self.root / 'tests/proof.c'
        self.proof.parent.mkdir()
        self.proof.write_text('proof fixture bytes')

    def document(self, changes):
        fields = ('path', 'current_path', 'name', 'ordinal',
                  'baseline_normalized_sha256', 'current_normalized_sha256')
        return {'schema': 1, 'baseline_inventory_sha256': self.inventory_hash,
                'proofs': {'regression': {'description': 'Explicit synthetic test decision',
                           'inputs': [{'path': 'tests/proof.c', 'sha256': digest(self.proof)}]}},
                'functions': [dict({k: row[k] for k in fields}, reason='Reviewed exact intentional delta',
                                   proof_ids=['regression']) for row in changes], 'removed_files': []}

    def compare(self, reviews=None):
        return compare(self.root, self.baseline, self.inventory_hash, reviews)

    def changed(self):
        self.path.write_text(self.original.replace('return 1', 'return 3'), newline='\n')
        return self.document(self.compare()['changes'])

    def test_no_changes_requires_no_inferred_allowance(self):
        self.assertTrue(self.compare()['passed'])

    def test_exact_review_passes_and_keeps_evidence_in_report(self):
        doc = self.changed()
        result = self.compare(doc)
        self.assertTrue(result['passed'])
        self.assertEqual(result['changes'][0]['proof_ids'], ['regression'])

    def test_unreviewed_change_and_name_only_approval_fail(self):
        doc = self.changed()
        self.assertFalse(self.compare()['passed'])
        del doc['functions'][0]['current_normalized_sha256']
        # Absence cannot mean "any hash": the actual function still exists.
        self.assertFalse(self.compare(doc)['passed'])

    def test_stale_current_hash_does_not_approve_future_edit(self):
        doc = self.changed()
        self.path.write_text(self.original.replace('return 1', 'return 9'), newline='\n')
        self.assertFalse(self.compare(doc)['passed'])

    def test_tampered_old_hash_and_inventory_fail(self):
        doc = self.changed()
        for key in ('baseline_normalized_sha256', 'current_normalized_sha256'):
            altered = deepcopy(doc)
            altered['functions'][0][key] = '0' * 64
            self.assertFalse(self.compare(altered)['passed'])
        doc['baseline_inventory_sha256'] = 'f' * 64
        self.assertFalse(self.compare(doc)['passed'])

    def test_new_changed_function_not_in_review_is_blocking(self):
        doc = self.changed()
        self.path.write_text(self.path.read_text().replace('return 2', 'return 7'), newline='\n')
        result = self.compare(doc)
        self.assertFalse(result['passed'])
        self.assertEqual(result['unreviewed_functions'], 1)

    def test_function_removal_exact_and_reappearance_stale(self):
        self.path.write_text('int beta(void)\n{ return 2; }\n', newline='\n')
        doc = self.document(self.compare()['changes'])
        self.assertIsNone(doc['functions'][0]['current_normalized_sha256'])
        self.assertTrue(self.compare(doc)['passed'])
        self.path.write_text(self.original, newline='\n')
        self.assertFalse(self.compare(doc)['passed'])

    def test_removed_file_needs_exact_original_hash(self):
        self.path.unlink()
        self.assertFalse(self.compare()['passed'])
        doc = self.document([])
        doc['removed_files'] = [{'path': 'src/game/sample.c', 'baseline_sha256': self.baseline['files'][0]['sha256'],
                                 'reason': 'Proven unused file with archived bytes', 'proof_ids': ['regression']}]
        self.assertTrue(self.compare(doc)['passed'])
        doc['removed_files'][0]['baseline_sha256'] = '0' * 64
        self.assertFalse(self.compare(doc)['passed'])

    def test_changed_proof_or_unknown_proof_id_fails(self):
        doc = self.changed()
        self.proof.write_text('different regression decision')
        self.assertFalse(self.compare(doc)['passed'])
        doc = self.document(self.compare()['changes'])
        doc['functions'][0]['proof_ids'] = ['unregistered-proof']
        self.assertFalse(self.compare(doc)['passed'])

    def test_duplicate_reviews_and_path_escape_fail(self):
        doc = self.changed()
        duplicate = deepcopy(doc)
        duplicate['functions'].append(deepcopy(duplicate['functions'][0]))
        self.assertFalse(self.compare(duplicate)['passed'])
        doc['proofs']['regression']['inputs'][0]['path'] = '../outside.c'
        self.assertFalse(self.compare(doc)['passed'])

    def test_ListLocal_has_no_special_name_permission(self):
        self.path.write_text('int ListLocal(void)\n{ return 1; }\n', newline='\n')
        self.baseline = {'files': [scan(self.path, self.root)]}
        self.path.write_text('int ListLocal(void)\n{ return 9; }\n', newline='\n')
        result = self.compare()
        self.assertFalse(result['passed'])
        self.assertIsNone(result['changes'][0]['reviewed_reason'])

    def test_empty_function_inventory_is_not_identity_proof(self):
        self.baseline = {'files': [{'path': 'src/game/sample.c', 'sha256': digest(self.path), 'functions': []}]}
        self.assertFalse(self.compare()['passed'])

    def test_identical_duplicate_branches_require_each_ordinal(self):
        source = '#if PLATFORM\nint alpha(void)\n{ return 1; }\n#else\nint alpha(void)\n{ return 1; }\n#endif\n'
        self.path.write_text(source, newline='\n')
        self.baseline = {'files': [scan(self.path, self.root)]}
        inventory = self.root / 'inventory.json'
        inventory.write_text(json.dumps(self.baseline))
        self.inventory_hash = digest(inventory)
        self.path.write_text(source.replace('return 1', 'return 3'), newline='\n')
        changes = self.compare()['changes']
        self.assertEqual([c['ordinal'] for c in changes], [0, 1])
        self.assertFalse(self.compare(self.document(changes[:1]))['passed'])
        self.assertTrue(self.compare(self.document(changes))['passed'])


if __name__ == '__main__':
    unittest.main()
