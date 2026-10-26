"""Planner safety and dependency-selection regressions (no game/asset dependencies)."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location('tce_bulk', Path(__file__).parents[1] / 'tools/tce_bulk.py')
bulk = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bulk)
tracker_spec = importlib.util.spec_from_file_location('tracker', Path(__file__).parents[1] / 'tools/reconstruction_tracker.py')
tracker = importlib.util.module_from_spec(tracker_spec)
tracker_spec.loader.exec_module(tracker)


def row(key, module='cgame', status='pending', calls=(), name=None):
    return dict(id=key, module=module, name=name or key, status=status, calls=list(calls),
                callers=[], themes=['weapons'], graph_available=True, indirect_calls=0,
                sdk_candidates=[], linux_candidates=[], evidence=[], signature='void f(void)', globals=[])


def index(rows):
    return dict(functions=rows, named_families={}, ledger_sha256='', graph_files={})


class BulkTests(unittest.TestCase):
    def test_verification_packet_selects_only_ported_and_preserves_boundary(self):
        data=index([row('a',status='ported',calls=['b','c','d']),row('b',status='ported',calls=['a']),row('c',status='verified'),row('d',status='pending')])
        result=bulk.choose_packet(data,'weapons',24,['a'],purpose='verify')
        self.assertEqual([r['id'] for r in result['functions']],['a','b'])
        self.assertEqual(result['dependencies'],[dict(id='c',status='verified'),dict(id='d',status='pending')])
        self.assertEqual(data['functions'][0]['status'],'ported')
        self.assertEqual(bulk.choose_packet(data,'weapons',24,['a'])['functions'],[])

    def test_verification_automatic_selection_and_excluded(self):
        data=index([row('a',status='ported'),row('b',status='excluded'),row('c',status='pending')])
        self.assertEqual([r['id'] for r in bulk.choose_packet(data,'weapons',24,[],purpose='verify')['functions']],['a'])
        self.assertEqual([r['id'] for r in bulk.choose_packet(data,'weapons',24,[])['functions']],['c'])
        with self.assertRaises(ValueError):bulk.choose_packet(data,'weapons',24,[],purpose='invalid')

    def test_shared_tables_headers_and_new_inputs_invalidate_packet(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('src/game/table.inc', 'src/cgame/types.h', 'src/ui/commands.c',
                         'src/CMakeLists.txt', 'src/reconstruction/functions.json'):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('initial')
            hashes = bulk.source_inputs(root)
            self.assertEqual(len(hashes), 4)
            packet = dict(ledger_sha256=bulk.sha(root / 'src/reconstruction/functions.json'),
                          graph_files={}, source_hashes=hashes, complete_source_snapshot=True)
            self.assertEqual(bulk.check_packet(root, packet), [])
            for name in ('src/game/table.inc', 'src/cgame/types.h'):
                (root / name).write_text('changed')
                self.assertIn('changed/missing input: ' + name, bulk.check_packet(root, packet))
                (root / name).write_text('initial')
            extra = root / 'src/game/new.h'
            extra.write_text('new')
            self.assertTrue(bulk.check_packet(root, packet))
            extra.unlink()
            (root / 'src/game/table.inc').unlink()
            self.assertTrue(bulk.check_packet(root, packet))

    def test_wave_dependencies_precede_callers(self):
        rows = [row('a', calls=['b']), row('b', calls=['c']), row('c')]
        units = bulk.dependency_units(rows, 1)
        self.assertEqual([u['ids'] for u in units], [['c'], ['b'], ['a']])
        self.assertEqual(units[2]['requires_units'], [2])

    def test_wave_cycle_is_not_split_and_boundary_is_explicit(self):
        rows = [row('a', calls=['b']), row('b', calls=['a', 'outside'])]
        units = bulk.dependency_units(rows, 1)
        self.assertEqual(len(units), 1)
        self.assertTrue(units[0]['oversized'])
        self.assertEqual(units[0]['boundary'], ['outside'])
        self.assertEqual(units[0]['ids'], ['a', 'b'])

    def test_wave_large_graph_no_recursion_or_lost_functions(self):
        rows = [row(str(i), calls=[str(i+1)] if i < 3999 else []) for i in range(4000)]
        units = bulk.dependency_units(rows, 32)
        ids = [k for u in units for k in u['ids']]
        self.assertEqual(len(ids), 4000)
        self.assertEqual(len(set(ids)), 4000)
        self.assertTrue(all(d < u['number'] for u in units for d in u['requires_units']))
        self.assertEqual(units, bulk.dependency_units(list(reversed(rows)), 32))

    def test_wave_invalid_size(self):
        with self.assertRaises(ValueError): bulk.dependency_units([], 0)

    def test_ledger_batch_preserves_unrelated_records(self):
        data = {'functions':[{'id':'a','status':'pending'}, {'id':'b','status':'verified'}]}
        updated = tracker.apply_updates(data, {'ledger_sha256':'hash','updates':[{'id':'a','status':'in_progress'}]}, 'hash')
        self.assertEqual(updated['functions'][0]['status'], 'in_progress')
        self.assertEqual(updated['functions'][1], data['functions'][1])
        self.assertEqual(data['functions'][0]['status'], 'pending')

    def test_bad_ledger_batch_cannot_partially_apply(self):
        data = {'functions':[{'id':'a','status':'pending'}]}
        for updates in ([{'id':'a','status':'in_progress'},{'id':'missing','status':'verified'}],
                        [{'id':'a'},{'id':'a'}], [{'id':'a','source':'not-a-list'}]):
            with self.assertRaises(ValueError): tracker.apply_updates(data, {'ledger_sha256':'hash','updates':updates}, 'hash')
            self.assertEqual(data['functions'][0]['status'], 'pending')
        with self.assertRaises(ValueError): tracker.apply_updates(data, {'ledger_sha256':'stale','updates':[{'id':'a'}]}, 'hash')

    def test_cycle_bounded_and_deduplicated(self):
        data = index([row('a', calls=['b']), row('b', calls=['a', 'c']), row('c')])
        result = bulk.choose_packet(data, 'weapons', 2, ['a'])
        self.assertEqual([r['id'] for r in result['functions']], ['a', 'b'])
        self.assertEqual(result['dependencies'], [dict(id='c', status='pending')])

    def test_verified_dependency_is_not_reopened(self):
        data = index([row('a', calls=['b']), row('b', status='verified')])
        result = bulk.choose_packet(data, 'weapons', 20, ['a'])
        self.assertEqual(len(result['functions']), 1)
        self.assertEqual(result['dependencies'][0]['status'], 'verified')

    def test_three_module_seeds(self):
        data = index([row('a', 'ui'), row('b'), row('c', 'qagame')])
        self.assertEqual(len(bulk.choose_packet(data, 'weapons', 3, [])['functions']), 3)

    def test_missing_graph_and_indirect_calls_are_explicit(self):
        a, b = row('a', calls=['external']), row('b')
        a['graph_available'] = False; b['indirect_calls'] = 2
        result = bulk.choose_packet(index([a, b]), 'weapons', 5, [])
        self.assertEqual(result['incomplete_graph'], ['a', 'b'])
        self.assertEqual(result['dependencies'][0]['status'], 'external_or_uninventoried')

    def test_named_family_keeps_per_module_status(self):
        data = index([row('a', name='PM_Foo'), row('b', 'qagame', name='PM_Foo')])
        data['named_families'] = {'PM_Foo': ['a', 'b']}
        result = bulk.choose_packet(data, 'weapons', 5, ['a'])
        self.assertEqual(len(result['functions']), 2)
        self.assertEqual(data['functions'][0]['status'], 'pending')

    def test_unknown_seed_and_invalid_limit_fail(self):
        with self.assertRaises(ValueError): bulk.choose_packet(index([]), 'weapons', 4, ['bad'])
        with self.assertRaises(ValueError): bulk.choose_packet(index([]), 'weapons', 0, [])

    def test_shared_common_string_does_not_invent_match(self):
        matches = bulk.match_linux('FUN_1000', ['common'], 'cgame', [], {('cgame', 'common'): [0, 1]}, {})
        self.assertEqual(matches, [])

    def test_rare_string_and_exact_name_still_only_candidates(self):
        result = bulk.match_linux('FUN_1000', ['unique token'], 'cgame', [], {('cgame', 'unique token'): [0]}, {})
        self.assertIn('only', result[0]['reason'])
        result = bulk.match_linux('Known', [], 'cgame', [], {}, {('cgame', 'Known'): [0, 1]})
        self.assertEqual(len(result), 2)
        self.assertIn('unverified', result[0]['reason'])

    def test_source_scan_ignores_comment_and_string_functions(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); (root / 'src/game').mkdir(parents=True)
            (root / 'src/game/a.c').write_text('/* int Fake() { } */\nconst char *s="void Bad() { }";\nint Real(int x) { return x; }')
            self.assertEqual(set(bulk.sdk_symbols(root)), {'Real'})

    def test_changed_inputs_detected_and_packet_not_overwritten(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); ledger = root / 'src/reconstruction/functions.json'
            ledger.parent.mkdir(parents=True); ledger.write_text('{}')
            data = index([row('a')]); data['ledger_sha256'] = bulk.sha(ledger)
            result = bulk.write_packet(root, data, root / 'packet', 'weapons', 2, [])
            self.assertFalse(bulk.check_packet(root, result))
            with self.assertRaises(ValueError): bulk.write_packet(root, data, root / 'packet', 'weapons', 2, [])
            ledger.write_text('{"changed":true}')
            self.assertTrue(bulk.check_packet(root, result))

    def test_changed_binary_or_source_detected(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); ledger = root / 'src/reconstruction/functions.json'
            ledger.parent.mkdir(parents=True); ledger.write_text('{}')
            binary = root / 'reference.dll'; binary.write_bytes(b'original')
            source = root / 'source.c'; source.write_text('int f(void);')
            packet = dict(ledger_sha256=bulk.sha(ledger), graph_files={},
                          reference_hashes={'reference.dll':bulk.sha(binary)},
                          source_hashes={'source.c':bulk.sha(source)})
            binary.write_bytes(b'changed'); source.write_text('int f(int x);')
            self.assertEqual(len(bulk.check_packet(root, packet)), 2)

    def test_output_cannot_target_assets_or_source(self):
        for path in ('pak/cache.json', 'tcetest/cache.json', 'src/cache.json'):
            with self.assertRaises(ValueError): bulk.under_build(bulk.ROOT / path)

    def test_hydrate_reports_missing_without_fake_code(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp); ledger = root / 'src/reconstruction/functions.json'
            ledger.parent.mkdir(parents=True); ledger.write_text('{}')
            a = row('ui_mp_x86.dll:40001000', 'ui')
            a.update(program='ui_mp_x86.dll',address='40001000',byte_hash='example')
            data=index([a]);data['ledger_sha256']=bulk.sha(ledger)
            bulk.write_packet(root,data,root/'packet','weapons',2,[])
            self.assertEqual(bulk.hydrate(root,root/'packet/packet.json')[0]['status'],'missing')
            self.assertFalse((root/'packet/references').exists())


if __name__ == '__main__': unittest.main()
