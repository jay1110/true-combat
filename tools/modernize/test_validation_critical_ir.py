"""Critical-gate negatives use real indexed C and real executor receipts."""
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest

from critical_ir_review import PAIRS, assembly_functions, assembly_units, emitted_closure, sha, validate_decisions, validate_fresh_fixtures
from risk_scan import scan
from validation_bundle import Executor, Job


class CriticalDecisionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.path = self.root / 'src/game/math.c'
        self.path.parent.mkdir(parents=True)
        self.path.write_text('float Critical(float x) { return x + 1; }\n', newline='\n')
        self.before = {'files': [scan(self.path, self.root)]}
        self.path.write_text('float Critical(float x) { return x + 2; }\nfloat PoseNew(float x) { return x * 3; }\n', newline='\n')
        self.current = {'files': [scan(self.path, self.root)]}
        old = self.before['files'][0]['functions'][0]['normalized_sha256']
        new = self.current['files'][0]['functions'][0]['normalized_sha256']
        self.entry = {'path': 'src/game/math.c', 'current_path': 'src/game/math.c', 'name': 'Critical',
                      'ordinal': 0, 'baseline_normalized_sha256': old, 'current_normalized_sha256': new,
                      'reason': 'Intentional checked behavior', 'proof_ids': ['oracle']}
        self.reviews = {'schema': 1, 'baseline_inventory_sha256': 'a' * 64,
                        'proofs': {'oracle': {'description': 'Independent fixture'}}, 'functions': [self.entry]}
        fields = {'proof_ids': ['oracle'], 'fixture_jobs': ['actual-oracle'],
                  'ir_rationale': 'Actual arithmetic pair comparison; no opcode parity claim',
                  'ir_mode': 'compare', 'ir_pairs': list(PAIRS)}
        new = self.current['files'][0]['functions'][1]
        self.policy = {'schema': 1, 'review_status': 'APPROVED', 'baseline_inventory_sha256': 'a' * 64,
                       'source_reviews_sha256': 'b' * 64, 'proofs': self.reviews['proofs'],
                       'functions': [dict(self.entry, **fields)],
                       'new_files': [dict(path='src/game/math.c', file_sha256=sha(self.path),
                            functions=[{'name': new['name'], 'ordinal': 0,
                                        'normalized_sha256': new['normalized_sha256']}], **fields)]}
        supplemental = self.root / 'src/game/tce_damage_controller.inc'
        supplemental.write_text('float SelectedDamage(float x) { return x * 2; }\n', newline='\n')
        indexed = scan(supplemental, self.root)['functions'][0]
        self.policy['supplemental_files'] = [dict(path='src/game/tce_damage_controller.inc',
            file_sha256=sha(supplemental), functions=[{'name': indexed['name'], 'ordinal': 0,
            'normalized_sha256': indexed['normalized_sha256']}], **fields)]

    def check(self):
        return validate_decisions(self.root, self.before, self.current, self.reviews,
                                  self.policy, 'a' * 64, 'b' * 64)

    def test_exact_decisions_cover_changed_and_new_bodies(self):
        result = self.check()
        self.assertTrue(result['passed'], result['errors'])
        self.assertEqual((result['changed'], result['new']), (1, 1))

    def test_missing_new_helper_and_unknown_policy_cannot_pass(self):
        self.policy['new_files'] = []
        self.assertIn('Missing new-function decisions', self.check()['errors'])
        self.policy['review_status'] = 'PENDING'
        self.assertFalse(self.check()['passed'])

    def test_changed_file_even_with_same_new_function_invalidates_exact_group(self):
        self.path.write_text(self.path.read_text() + '\n/* after review */\n', newline='\n')
        self.current = {'files': [scan(self.path, self.root)]}
        self.assertTrue(any('Stale or unmatched new-function' in e for e in self.check()['errors']))

    def test_deleted_inventory_function_and_omitted_new_file_fail(self):
        self.current['files'][0]['functions'].pop()
        self.assertFalse(self.check()['passed'])
        (self.path.parent / 'unlisted.c').write_text('int Unlisted(void) { return 1; }\n', newline='\n')
        self.assertIn('Current risk inventory omits/adds source files', self.check()['errors'])

    def test_not_applicable_requires_rationale_proof_and_fresh_fixture(self):
        row = self.policy['new_files'][0]
        row.update(ir_mode='not-applicable', ir_pairs=[], ir_rationale='No predecessor exists; asset oracle validates new geometry')
        self.assertTrue(self.check()['passed'])
        row['fixture_jobs'] = []
        self.assertFalse(self.check()['passed'])

    def test_tampered_review_digest_and_duplicate_new_function_fail(self):
        self.policy['source_reviews_sha256'] = 'c' * 64
        self.policy['new_files'][0]['functions'] *= 2
        self.assertFalse(self.check()['passed'])

    def test_unindexed_damage_include_cannot_be_omitted_or_changed(self):
        self.policy['supplemental_files'] = []
        self.assertFalse(self.check()['passed'])


class FreshFixtureTests(unittest.TestCase):
    def test_real_executor_receipt_rejects_missing_changed_and_added_artifacts(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            source = root / 'source.c'
            source.write_text('int value = 1;')
            worker = root / 'worker.py'
            worker.write_text('import pathlib,sys;p=pathlib.Path(sys.argv[1]);p.mkdir();(p/"report.json").write_text("measured")')
            out = root / 'out'
            generated = out / 'oracle'
            job = Job('actual-oracle', 'test', [sys.executable, str(worker), str(generated)],
                      [str(generated / 'report.json'), str(generated)])
            with redirect_stdout(io.StringIO()):
                self.assertTrue(Executor(root, out, [job], lambda: [source, worker]).run()['passed'])
            def check(names=['actual-oracle']):
                return validate_fresh_fixtures(out, names, root, ['source.c'])
            self.assertTrue(check()['passed'])
            self.assertFalse(check(['old-green-job'])['passed'])
            (generated / 'unreviewed-extra').write_text('extra artifact')
            self.assertFalse(check()['passed'])
            (generated / 'unreviewed-extra').unlink()
            (generated / 'report.json').write_text('different observation')
            self.assertFalse(check()['passed'])


class ListingTests(unittest.TestCase):
    def test_tail_call_comparison_contains_real_helper_body(self):
        lines, symbols, external = emitted_closure('_wrapper', {'_wrapper': ['jmp _actual'],
            '_actual': ['shr eax, 1', 'call _sqrt', 'ret 0']})
        self.assertIn('shr eax, 1', lines)
        self.assertEqual(symbols, ['_actual', '_wrapper'])
        self.assertEqual(external, ['_sqrt'])

    def test_actual_listing_wrapped_machine_bytes_do_not_become_instructions(self):
        text = '_ir_old_backlerp PROC\n  00000 66 0f 6e 4c 24\n 04    movd xmm1, DWORD PTR e$[esp-4]\n 00006 c3 ret 0\n_ir_old_backlerp ENDP\n'
        units = assembly_units(text)
        self.assertEqual(units['_ir_old_backlerp'], ['movd xmm1, DWORD PTR e$[esp-4]', 'ret 0'])
        with self.assertRaisesRegex(ValueError, 'Missing actual compiled wrapper'):
            assembly_functions(text)


if __name__ == '__main__':
    unittest.main()
