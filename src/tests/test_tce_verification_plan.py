import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from tce_verification_plan import build_plan, check_plan

class VerificationPlanTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.write('src/cgame/sample.c', 'void DrawThing(void) {}')
        self.write('src/tests/compare.c', 'DrawThing();')
        self.write('build/live.py', '# owned fixture')
        self.write('original.dll', 'immutable reference')
        self.ledger = {'functions': [
            {'id': 'cg:1', 'name': 'DrawThing', 'status': 'ported', 'source': ['src/cgame/sample.c'], 'evidence': []},
            {'id': 'cg:2', 'name': 'Other', 'status': 'ported', 'source': [], 'evidence': []},
            {'id': 'cg:3', 'name': 'Done', 'status': 'verified', 'source': [], 'evidence': []}]}
        self.manifest = {'schema': 1, 'runtime_inputs': ['original.dll'], 'scenarios': [
            {'id': 'one-start', 'setup': {'map': 'obj_delta'}, 'fixture': 'build/live.py',
             'live_boundary': 'No whole renderer inference', 'functions': [
                 {'id': 'cg:1', 'live_observation': 'Visible item', 'tests': [
                     {'path': 'src/tests/compare.c', 'boundary': 'Callees mocked'}]}]}]}
        self.persist()

    def write(self, name, text):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def persist(self):
        self.write('src/reconstruction/functions.json', json.dumps(self.ledger))
        self.write('src/reconstruction/verification_scenarios.json', json.dumps(self.manifest))

    def test_groups_and_no_promotion_or_writes(self):
        before = (self.root / 'src/reconstruction/functions.json').read_bytes()
        plan = build_plan(self.root)
        self.assertEqual([f['id'] for f in plan['scenarios'][0]['functions']], ['cg:1'])
        self.assertEqual([f['id'] for f in plan['unassigned']], ['cg:2'])
        self.assertFalse(plan['automatic_promotion'])
        self.assertTrue(check_plan(self.root, plan)['fresh'])
        self.assertEqual(before, (self.root / 'src/reconstruction/functions.json').read_bytes())

    def test_changed_evidence_runtime_and_new_source_invalidate(self):
        for path in ['src/tests/compare.c', 'original.dll', 'src/cgame/new.h']:
            with self.subTest(path=path):
                plan = build_plan(self.root)
                self.write(path, 'changed ' + path)
                result = check_plan(self.root, plan)
                self.assertFalse(result['fresh'])
                self.assertIn(path, result['changed_inputs'])

    def test_missing_input_never_fresh(self):
        self.manifest['runtime_inputs'].append('missing.dll')
        self.persist()
        plan = build_plan(self.root)
        self.assertIn('missing.dll', plan['missing_inputs'])
        self.assertFalse(check_plan(self.root, plan)['fresh'])

    def test_reject_unknown_duplicate_and_escaping_paths(self):
        for fault in ['unknown', 'duplicate', 'escape']:
            with self.subTest(fault=fault):
                original = json.loads(json.dumps(self.manifest))
                entry = self.manifest['scenarios'][0]['functions'][0]
                if fault == 'unknown': entry['id'] = 'cg:999'
                elif fault == 'duplicate': self.manifest['scenarios'][0]['functions'].append(dict(entry))
                else: entry['tests'][0]['path'] = '../escape'
                self.persist()
                with self.assertRaises(ValueError): build_plan(self.root)
                self.manifest = original

    def test_unassigned_lexical_hits_are_only_candidates(self):
        self.manifest['scenarios'] = []
        self.persist()
        plan = build_plan(self.root)
        self.assertEqual(plan['unassigned'][0]['candidate_tests_only'], ['src/tests/compare.c'])
        self.assertEqual(plan['scenarios'], [])

    def test_added_runtime_pack_invalidates(self):
        self.write('mod/pak0.pk3', 'first')
        self.manifest['runtime_input_globs'] = ['mod/*.pk3']
        self.persist()
        plan = build_plan(self.root)
        self.write('mod/pak1.pk3', 'new asset override')
        self.assertIn('glob:mod/*.pk3', check_plan(self.root, plan)['changed_inputs'])

if __name__ == '__main__':
    unittest.main()
