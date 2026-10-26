import importlib.util
from pathlib import Path
import unittest
import hashlib
import tempfile

spec = importlib.util.spec_from_file_location('triage', Path(__file__).parents[1] / 'tools/tce_triage.py')
triage = importlib.util.module_from_spec(spec)
spec.loader.exec_module(triage)


def row(name, calls=(), **kwargs):
    result = dict(id=name, name=name, program='qagame_mp_x86.dll', module='qagame',
                  status='pending', integration='pending', calls=list(calls),
                  graph_available=True, decompiled=True, indirect_calls=0, themes=[])
    result.update(kwargs)
    return result


class TriageTests(unittest.TestCase):
    def run_rows(self, *rows):
        return triage.triage({'functions': list(rows)})

    def test_cycle_indivisible_and_dependency_first(self):
        report = self.run_rows(row('a', ['b']), row('b', ['a', 'c']), row('c'))
        self.assertEqual([u['members'] for u in report['units']], [['c'], ['a', 'b']])
        self.assertTrue(report['units'][1]['cyclic'])

    def test_verified_integrated_callee_not_blocker(self):
        report = self.run_rows(row('a', ['b']), row('b', status='verified', integration='integrated'))
        self.assertEqual(report['summary']['open_functions'], 1)
        self.assertEqual(report['units'][0]['dependencies'], [])
        self.assertEqual(report['units'][0]['verified_integrated_callees'], ['b'])

    def test_verified_but_not_integrated_still_blocks(self):
        report = self.run_rows(row('a', ['b']), row('b', status='verified'))
        self.assertEqual([u['id'] for u in report['units']], ['b', 'a'])

    def test_missing_and_indirect_edges_not_claimed_done(self):
        unit = self.run_rows(row('a', ['external'], graph_available=False,
                                 decompiled=False, indirect_calls=2))['units'][0]
        self.assertEqual(unit['unknown_callees'], ['external'])
        self.assertEqual(unit['risks'], ['unknown_edges', 'missing_graph', 'indirect_calls', 'missing_cache'])
        self.assertTrue(unit['initial_known_dependency_ready'])
        self.assertEqual(unit['integration_blockers'], ['a'])

    def test_priority_propagates_to_prerequisites(self):
        report = self.run_rows(row('a'), row('z', ['y'], status='in_progress', themes=['weapons']), row('y'))
        self.assertEqual([u['id'] for u in report['units']], ['y', 'z', 'a'])
        self.assertTrue(report['units'][0]['priority_gameplay'])
        self.assertEqual(report['units'][0]['caller_unlocks'], 1)

    def test_order_and_duplicate_edges_deterministic(self):
        rows = [row('a', ['b', 'b']), row('b', ['a']), row('c')]
        self.assertEqual(self.run_rows(*rows), self.run_rows(*reversed(rows)))

    def test_self_cycle(self):
        self.assertTrue(self.run_rows(row('a', ['a']))['units'][0]['cyclic'])

    def test_unavailable_indirect_count(self):
        unit = self.run_rows(row('a', graph_available=False, indirect_calls=None))['units'][0]
        self.assertIn('missing_graph', unit['risks'])

    def test_long_chain_no_recursion(self):
        rows = [row(str(i), [str(i+1)] if i < 2499 else []) for i in range(2500)]
        report = self.run_rows(*rows)
        self.assertEqual(report['units'][0]['id'], '2499')
        self.assertEqual(report['units'][-1]['dependency_depth'], 2499)

    def test_empty_and_linux_filtered(self):
        self.assertEqual(self.run_rows(row('linux', program='qagame.mp.i386.so'))['units'], [])

    def test_duplicate_id_rejected(self):
        with self.assertRaises(ValueError):
            self.run_rows(row('a'), row('a'))

    def test_runtime_not_applicable_and_excluded(self):
        report = self.run_rows(row('a', ['b', 'c']),
            row('b', status='verified', integration='not_applicable'), row('c', status='excluded'))
        self.assertEqual(report['summary']['open_functions'], 1)
        self.assertEqual(report['excluded'], ['c'])
        self.assertEqual(report['units'][0]['verified_integrated_callees'], ['b'])
        self.assertEqual(report['units'][0]['excluded_callees'], ['c'])
        self.assertEqual(report['units'][0]['dependencies'], [])

    def test_output_boundary(self):
        root = Path(__file__).resolve().parents[2]
        self.assertEqual(triage.under_build(root, root / 'build/test.json'), root / 'build/test.json')
        for name in ('src/output.json', 'build/../tcetest/output.json', 'build'):
            with self.assertRaises(ValueError):
                triage.under_build(root, root / name)

    def test_stale_ledger_and_source(self):
        with tempfile.TemporaryDirectory(dir=triage.ROOT / 'build') as folder:
            root = Path(folder)
            ledger = root / 'src/reconstruction/functions.json'
            ledger.parent.mkdir(parents=True)
            ledger.write_bytes(b'ledger')
            source = root / 'src/source.c'
            source.write_bytes(b'source')
            index = dict(ledger_sha256=hashlib.sha256(b'ledger').hexdigest(),
                         sdk_hashes={'src/source.c': hashlib.sha256(b'source').hexdigest()})
            self.assertEqual(triage.freshness(root, index)['hashed_inputs_checked'], 1)
            ledger.write_bytes(b'new ledger')
            with self.assertRaisesRegex(ValueError, 'ledger changed'):
                triage.freshness(root, index)
            ledger.write_bytes(b'ledger')
            source.write_bytes(b'new source')
            with self.assertRaisesRegex(ValueError, 'changed/missing input'):
                triage.freshness(root, index)


if __name__ == '__main__':
    unittest.main()
