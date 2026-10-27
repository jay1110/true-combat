"""Executable orchestration regressions; no compiler, game or SDK required."""
from __future__ import annotations

from contextlib import redirect_stdout
import ast
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from validation_bundle import Executor, Job, source_paths
from validation_catalog import FIXTURES, make_plan
from validation_gates import check_cache, core, modules, PORTABLE_TESTS

ROOT = Path(__file__).resolve().parents[2]


class ExecutorTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.out = self.root / 'results'
        self.source = self.root / 'production.c'
        self.source.write_text('int behavior = 1;\n')
        self.worker = self.root / 'worker.py'
        self.worker.write_text('''import pathlib,sys
out=pathlib.Path(sys.argv[1]);mode=sys.argv[2]
out.parent.mkdir(parents=True,exist_ok=True)
counter=out.with_suffix('.counter')
n=int(counter.read_text())+1 if counter.exists() else 1
counter.write_text(str(n))
if mode=='fail' or (mode=='fail-once' and n==1):sys.exit(7)
if mode!='no-output':out.write_text(str(n))
if mode=='mutate':pathlib.Path(sys.argv[3]).write_text('changed during execution')
''', encoding='utf-8')

    def job(self, name='a', mode='write', needs=()):
        artifact = self.out / (name + '.result')
        command = [sys.executable, str(self.worker), str(artifact), mode, str(self.source)]
        return Job(name, 'test', command, [str(artifact)], list(needs))

    def executor(self, jobs, extra=()):
        return Executor(self.root, self.out, jobs, lambda: [self.source, self.worker, *extra])

    def run_bundle(self, jobs, **kwargs):
        with redirect_stdout(io.StringIO()):
            return self.executor(jobs).run(**kwargs)

    def test_real_process_pass_reuse_and_verify(self):
        jobs = [self.job(), self.job('b', needs=['a'])]
        first = self.run_bundle(jobs)
        self.assertTrue(first['passed'])
        second = self.run_bundle(jobs, resume=True)
        self.assertTrue(all(step['reused'] for step in second['steps']))
        third = self.run_bundle(jobs, verify=True)
        self.assertTrue(third['passed'])
        self.assertEqual((self.out / 'a.counter').read_text(), '1')
        self.assertEqual((self.out / 'b.counter').read_text(), '1')

    def test_altered_artifact_invalidates_dependent_results(self):
        jobs = [self.job(), self.job('b', needs=['a'])]
        self.run_bundle(jobs)
        (self.out / 'a.result').write_text('tampered')
        self.assertFalse(self.run_bundle(jobs, verify=True)['passed'])
        with self.assertRaisesRegex(ValueError, 'artifacts changed'):
            self.run_bundle(jobs, resume=True)
        self.assertEqual((self.out / 'a.counter').read_text(), '1')
        self.assertEqual((self.out / 'b.counter').read_text(), '1')

    def test_source_change_refuses_green_resume(self):
        jobs = [self.job()]
        self.run_bundle(jobs)
        self.source.write_text('int behavior = 2;\n')
        with self.assertRaisesRegex(ValueError, 'freeze changed'):
            self.run_bundle(jobs, resume=True)

    def test_command_change_refuses_green_resume(self):
        self.run_bundle([self.job()])
        with self.assertRaisesRegex(ValueError, 'freeze changed'):
            self.run_bundle([self.job(mode='no-output')], resume=True)

    def test_added_input_refuses_green_resume(self):
        self.run_bundle([self.job()])
        added = self.root / 'added.h'
        added.write_text('new source input')
        with self.assertRaisesRegex(ValueError, 'freeze changed'):
            self.executor([self.job()], [added]).run(resume=True)

    def test_log_tamper_is_not_reused(self):
        jobs = [self.job()]
        self.run_bundle(jobs)
        (self.out / 'a.log').write_text('forged green log')
        self.assertFalse(self.run_bundle(jobs, verify=True)['passed'])

    def test_external_binary_tamper_and_deletion_are_not_green_reports(self):
        binary = self.root / 'module.dll'
        binary.write_bytes(b'actual built binary')
        job = self.job()
        job.evidence = [str(binary)]
        self.assertTrue(self.run_bundle([job])['passed'])
        binary.write_bytes(b'replaced binary')
        self.assertFalse(self.run_bundle([job], verify=True)['passed'])
        with self.assertRaisesRegex(ValueError, 'artifacts changed'):
            self.run_bundle([job], resume=True)
        binary.unlink()
        self.assertFalse(self.run_bundle([job], verify=True)['passed'])

    def test_later_gate_cannot_mutate_earlier_artifact_and_leave_pass(self):
        first = self.job()
        second = self.job('b', mode='mutate', needs=['a'])
        second.command[-1] = first.outputs[0]
        result = self.run_bundle([first, second])
        self.assertTrue(result['source_stable'])
        self.assertEqual(result['steps'][0]['status'], 'STALE')
        self.assertFalse(result['passed'])

    def test_generated_artifact_manifest_binds_real_binary(self):
        binary = self.root / 'module.dll'
        binary.write_bytes(b'actual built binary')
        job = self.job()
        manifest = self.root / 'module-paths.json'
        manifest.write_text(json.dumps({'artifact_paths': [str(binary)]}))
        job.evidence = [str(manifest)]
        job.artifact_manifest = str(manifest)
        self.assertTrue(self.run_bundle([job])['passed'])
        binary.unlink()
        self.assertFalse(self.run_bundle([job], verify=True)['passed'])

    def test_missing_output_even_successful_process_fails(self):
        result = self.run_bundle([self.job(mode='no-output')])
        self.assertEqual(result['steps'][0]['returncode'], 0)
        self.assertEqual(result['status'], 'FAIL')

    def test_old_output_cannot_satisfy_noop(self):
        jobs = [self.job(mode='no-output')]
        self.out.mkdir()
        (self.out / 'a.result').write_text('old green')
        with self.assertRaisesRegex(ValueError, 'not empty'):
            self.run_bundle(jobs)

    def test_failed_dependency_never_runs_child(self):
        result = self.run_bundle([self.job(mode='fail'), self.job('b', needs=['a'])])
        self.assertFalse(result['passed'])
        self.assertEqual(result['steps'][0]['returncode'], 7)
        self.assertEqual(result['steps'][1]['status'], 'INCOMPLETE')
        self.assertFalse((self.out / 'b.counter').exists())

    def test_resume_retries_failed_step_without_repeating_valid_work(self):
        jobs = [self.job(), self.job('b', mode='fail-once', needs=['a']), self.job('c', needs=['b'])]
        self.assertFalse(self.run_bundle(jobs)['passed'])
        self.assertTrue(self.run_bundle(jobs, resume=True)['passed'])
        self.assertEqual((self.out / 'a.counter').read_text(), '1')
        self.assertEqual((self.out / 'b.counter').read_text(), '2')
        self.assertEqual((self.out / 'c.counter').read_text(), '1')

    def test_environment_change_invalidates_source_bound_resume(self):
        self.run_bundle([self.job()])
        changed = self.job()
        changed.env['TCE_TEST_CHANGED_FLAG'] = '1'
        with self.assertRaisesRegex(ValueError, 'freeze changed'):
            self.run_bundle([changed], resume=True)

    def test_unavailable_required_gate_is_incomplete(self):
        result = self.run_bundle([self.job(), Job('missing', 'asan', unavailable='No mapped SDK')])
        self.assertEqual(result['status'], 'INCOMPLETE')
        self.assertFalse(result['passed'])

    def test_missing_frozen_input_cannot_pass(self):
        with redirect_stdout(io.StringIO()):
            result = self.executor([self.job()], [self.root / 'missing.pk3']).run()
        self.assertFalse(result['passed'])
        self.assertEqual(result['status'], 'INCOMPLETE')

    def test_source_mutation_during_run_invalidates_all_green_steps(self):
        result = self.run_bundle([self.job(mode='mutate')])
        self.assertEqual(result['steps'][0]['status'], 'PASS')
        self.assertFalse(result['source_stable'])
        self.assertEqual(result['status'], 'FAIL')

    def test_no_accidental_unordered_dependency_or_log_escape(self):
        for jobs in ([self.job(needs=['future'])], [self.job('../escape')], [self.job(), self.job()]):
            with self.assertRaises(ValueError):
                self.executor(jobs)


class CatalogTests(unittest.TestCase):
    def test_review_manifest_and_ignored_proof_bytes_invalidate_global_freeze(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            baseline = root / 'baseline'
            (baseline / 'inventory').mkdir(parents=True)
            (baseline / 'inventory/risk-inventory.json').write_text('{}')
            (baseline / 'symbol-baseline.json').write_text('{}')
            proof = root / 'reconstruction/proof.json'
            proof.parent.mkdir()
            proof.write_text('actual measured output')
            reviews = root / 'review.json'
            reviews.write_text(json.dumps({'proofs': {'measured': {'description': 'Bound proof',
                'inputs': [{'path': 'reconstruction/proof.json', 'sha256': '0' * 64}]}}}))
            config = {'schema': 1, 'baseline_dir': 'baseline', 'source_reviews': 'review.json'}
            jobs, inputs = make_plan(root, root / 'unused', config, None, 1)
            self.assertIn(proof, inputs)
            self.assertIn(reviews, inputs)
            source_diff = next(j for j in jobs if j.name == 'source-diff')
            self.assertIn(str(baseline / 'inventory/risk-inventory.json'), source_diff.command)
            worker = root / 'worker.py'
            worker.write_text('import pathlib,sys;pathlib.Path(sys.argv[1]).write_text("receipt")')
            output = root / 'results'
            job = Job('checked', 'test', [sys.executable, str(worker), str(output / 'receipt')],
                      [str(output / 'receipt')])
            executor = Executor(root, output, [job], lambda: inputs + [worker])
            with redirect_stdout(io.StringIO()):
                self.assertTrue(executor.run()['passed'])
                proof.write_text('proof changed after the source-diff gate')
                with self.assertRaisesRegex(ValueError, 'freeze changed'):
                    executor.run(resume=True)

    def test_review_proof_path_escape_is_rejected_before_execution(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            reviews = root / 'review.json'
            for entry in ({'path': '../outside'}, {'path': 'C:/outside'}, {'path': 'safe/../outside'}):
                reviews.write_text(json.dumps({'proofs': {'bad': {'inputs': [entry]}}}))
                with self.assertRaisesRegex(ValueError, 'proof input path'):
                    make_plan(root, root / 'out', {'schema': 1, 'source_reviews': 'review.json'}, root / 'baseline', 1)

    def test_all_bugfix_regression_runners_registered(self):
        registered = {row[1] + '.py' for row in FIXTURES}
        actual = {path.name for path in (ROOT / 'tools/modernize').glob('*_regression.py')}
        self.assertEqual(actual - registered, set())
        self.assertGreaterEqual(len(registered), 20)

    def test_required_visibility_gate_uses_current_origin_scope_not_baseline(self):
        jobs, _ = make_plan(ROOT, ROOT / 'build/test-plan-only', {'schema': 1}, None, 2)
        visibility = [job for job in jobs if job.name.startswith('visibility-')]
        self.assertEqual({job.name for job in visibility}, {'visibility-default', 'visibility-asan'})
        for job in visibility:
            self.assertNotIn('--without-origin-scope', job.command)
            self.assertFalse(job.unavailable)
            self.assertTrue(job.required)
        runner = (ROOT / 'tools/modernize/visibility_regression.py').read_text()
        self.assertIn("'CG_TCEPresentationBeginOrigin'", runner)
        self.assertIn("'CG_TCEPresentationEndOrigin'", runner)

    def test_critical_gate_waits_for_fresh_actual_deterministic_jobs(self):
        jobs, _ = make_plan(ROOT, ROOT / 'build/test-plan-only', {'schema': 1}, None, 2)
        critical = next(job for job in jobs if job.name == 'critical-ir-review')
        actual = {job.name for job in jobs if job.category.startswith('deterministic/')}
        self.assertTrue(actual <= set(critical.needs))

    def test_catalog_uses_real_runner_cli_flags(self):
        jobs, _ = make_plan(ROOT, ROOT / 'build/test-plan-only', {'schema': 1}, None, 2)
        for job in jobs:
            if not job.category.startswith('deterministic/'):
                continue
            source = Path(job.command[1])
            tree = ast.parse(source.read_text(encoding='utf-8-sig'))
            supported = {arg.value for node in ast.walk(tree) if isinstance(node, ast.Call)
                         and isinstance(node.func, ast.Attribute) and node.func.attr == 'add_argument'
                         for arg in node.args if isinstance(arg, ast.Constant) and isinstance(arg.value, str)}
            flags = {arg for arg in job.command[2:] if arg.startswith('--')}
            self.assertEqual(flags - supported, set(), job.name)

    def test_unmapped_release_is_explicitly_incomplete(self):
        jobs, _ = make_plan(ROOT, ROOT / 'build/test-plan-only', {'schema': 1}, None, 2)
        missing = {job.name for job in jobs if job.unavailable}
        self.assertTrue({'missing-build-debug', 'missing-build-release', 'missing-build-asan',
                         'missing-build-matrix', 'source-diff', 'package-closure',
                         'runtime-smoke', 'critical-ir-review'} <= missing)
        self.assertTrue(all(job.required for job in jobs))
        self.assertEqual(len(jobs), len({job.name for job in jobs}))

    def test_clean_builds_force_normal_gameplay_and_abi_tests(self):
        with tempfile.TemporaryDirectory() as temp:
            tree = Path(temp) / 'existing'
            tree.mkdir()
            (tree / 'CMakeCache.txt').write_text('FEATURE_OMNIBOT:BOOL=OFF\n')
            config = {'schema': 1, 'builds': [{'name': 'test', 'role': 'asan', 'directory': str(tree),
                       'configure_args': ['-G', 'Ninja'], 'configuration': 'Debug',
                       'abi': {'system': 'Windows', 'architecture': 'i386', 'pointer_bytes': 4}}]}
            jobs, _ = make_plan(ROOT, Path(temp) / 'out', config, None, 2)
            configure = next(j for j in jobs if j.name == 'build-test-configure')
            self.assertIn('--fresh', configure.command)
            self.assertIn('-DFEATURE_OMNIBOT=ON', configure.command)
            self.assertIn('-DTCE_BUILD_OMNIBOT_CORE=ON', configure.command)
            self.assertIn('-DTCE_ENABLE_SANITIZERS=ON', configure.command)
            self.assertFalse(configure.unavailable)
            build = next(j for j in jobs if j.name == 'build-test')
            self.assertIn('--clean-first', build.command)
            self.assertGreaterEqual(len(PORTABLE_TESTS), 7)

    def test_cache_rejects_old_off_build_and_uninstrumented_asan(self):
        with tempfile.TemporaryDirectory() as temp:
            tree = Path(temp)
            lines = [f'CMAKE_HOME_DIRECTORY:INTERNAL={ROOT}', 'BUILD_TESTING:BOOL=ON',
                     'FEATURE_OMNIBOT:BOOL=OFF', 'TCE_BUILD_OMNIBOT_CORE:BOOL=OFF',
                     'FEATURE_NAVSTUDIO:BOOL=OFF', 'TCE_BUILD_REFERENCE_TESTS:BOOL=OFF']
            (tree / 'CMakeCache.txt').write_text('\n'.join(lines))
            result = check_cache(tree, ROOT, 'asan')
            self.assertFalse(result['passed'])
            self.assertTrue(any('address' in item for item in result['errors']))
            lines[2:4] = ['FEATURE_OMNIBOT:BOOL=ON', 'TCE_BUILD_OMNIBOT_CORE:BOOL=ON']
            lines += ['TCE_ENABLE_SANITIZERS:BOOL=ON', 'TCE_SANITIZERS:STRING=address,undefined']
            (tree / 'CMakeCache.txt').write_text('\n'.join(lines))
            self.assertTrue(check_cache(tree, ROOT, 'asan')['passed'])

    def test_debug_role_and_actual_single_config_release_cannot_pass(self):
        with tempfile.TemporaryDirectory() as temp:
            row = {'name': 'wrong', 'role': 'debug', 'directory': temp, 'configuration': 'Release'}
            with self.assertRaisesRegex(ValueError, 'configuration mismatch'):
                make_plan(ROOT, Path(temp) / 'out', {'schema': 1, 'builds': [row]}, None, 1)
            tree = Path(temp)
            (tree / 'CMakeCache.txt').write_text('\n'.join([
                f'CMAKE_HOME_DIRECTORY:INTERNAL={ROOT}', 'CMAKE_BUILD_TYPE:STRING=Release',
                'BUILD_TESTING:BOOL=ON', 'FEATURE_OMNIBOT:BOOL=ON', 'TCE_BUILD_OMNIBOT_CORE:BOOL=ON',
                'FEATURE_NAVSTUDIO:BOOL=OFF', 'TCE_BUILD_REFERENCE_TESTS:BOOL=OFF']))
            self.assertFalse(check_cache(tree, ROOT, 'debug', 'Debug')['passed'])
            self.assertTrue(check_cache(tree, ROOT, 'release', 'Release')['passed'])

    def test_same_native_abi_cannot_be_relabelled_matrix(self):
        abi = {'system': 'Windows', 'architecture': 'i386', 'pointer_bytes': 4}
        rows = [{'name': 'native', 'role': 'debug', 'directory': '.', 'abi': abi},
                {'name': 'fake-cross', 'role': 'matrix', 'directory': '.', 'abi': abi}]
        with self.assertRaisesRegex(ValueError, 'must differ'):
            make_plan(ROOT, ROOT / 'build/test-plan-only', {'schema': 1, 'builds': rows}, None, 1)

    def test_stale_configuration_manifest_and_unrelated_core_rejected(self):
        with tempfile.TemporaryDirectory() as temp:
            directory = Path(temp)
            manifest = directory / 'tce-modules-Debug.json'
            manifest.write_text(json.dumps({'schema': 1, 'variant': 'game', 'configuration': 'Release',
                                           'modules': dict.fromkeys(('cgame', 'ui', 'qagame'), 'unused')}))
            with self.assertRaisesRegex(ValueError, 'configuration'):
                modules(manifest, 'Debug')
            runtime = directory / 'omnibot-runtime-Debug.json'
            abi = {'system': 'Windows', 'architecture': 'i386'}
            runtime.write_text(json.dumps({'schema': 1, 'variant': 'game', **abi,
                                           'library': str(directory / 'fresh-core.dll')}))
            with self.assertRaisesRegex(ValueError, 'not this build'):
                core(runtime, 'Debug', abi, directory / 'old-core.dll', None, None)

    def test_packaged_license_is_a_frozen_source_input(self):
        paths = source_paths(ROOT)
        self.assertIn(ROOT / 'EULA_TrueCombat_Elite.txt', paths)
        self.assertIn(ROOT / 'README.md', paths)
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            policy = root / 'tools/release/source_archive.py'
            policy.parent.mkdir(parents=True)
            policy.write_bytes((ROOT / 'tools/release/source_archive.py').read_bytes())
            license_file = root / 'EULA_TrueCombat_Elite.txt'
            license_file.write_text('original packaged license')
            worker = root / 'worker.py'
            worker.write_text('import pathlib,sys;pathlib.Path(sys.argv[1]).write_text("receipt")')
            subprocess.run(['git', 'init', '-q', str(root)], check=True)
            out = root / 'results'
            job = Job('package', 'test', [sys.executable, str(worker), str(out / 'receipt.json')],
                      [str(out / 'receipt.json')])
            executor = Executor(root, out, [job], lambda: source_paths(root) + [worker])
            with redirect_stdout(io.StringIO()):
                self.assertTrue(executor.run()['passed'])
                license_file.write_text('different packaged license')
                with self.assertRaisesRegex(ValueError, 'freeze changed'):
                    executor.run(resume=True)

    def test_source_closure_includes_new_untracked_helpers_and_no_evidence(self):
        paths = source_paths(ROOT)
        self.assertIn(ROOT / 'tools/modernize/validation_catalog.py', paths)
        self.assertIn(ROOT / 'src/game/g_tce_pose.c', paths)
        self.assertFalse(any('reconstruction' in path.parts for path in paths))


if __name__ == '__main__':
    unittest.main()
