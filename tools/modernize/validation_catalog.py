"""Finite release gate catalog. Paths/toolchains are supplied, never installed."""
from __future__ import annotations

import json
import os
from pathlib import Path
import sys

from validation_bundle import Job
from source_review import proof_input_paths

MODES = ('debug', 'release', 'asan')
CAPTURES = ('pose-32152-1', 'pose-control-crouch', 'pose-control-prone',
            'pose-control-lean-left', 'pose-control-lean-right', 'pose-control-prone-aim')
# name, runner, argument convention, Windows-only, extra arguments
FIXTURES = (
    ('weapon', 'weapon_regression', 'configuration', False, []),
    ('movement', 'movement_regression', 'configuration', False, ['--footsteps']),
    ('objective', 'objective_lifecycle_regression', 'plain', False, []),
    ('bomb-lifecycle', 'bomb_lifecycle_regression', 'configuration', False, []),
    ('spawn-distance', 'spawn_distance_regression', 'optimized', False, []),
    ('hitbox', 'hitbox_regression', 'optimized', False, []),
    ('hitbox-bullets', 'hitbox_regression', 'optimized', False, ['--bullets']),
    ('door', 'door_reversal_regression', 'configuration', False, []),
    ('effects', 'effect_regression', 'plain', True, []),
    ('visibility', 'visibility_regression', 'plain', False, []),
    ('weather-retry', 'weather_retry_regression', 'plain', True, []),
    ('weather-precision', 'weather_precision_probe', 'configuration', True, []),
    ('server-lifecycle', 'server_lifecycle_probe', 'configuration', False, []),
    ('stats-public', 'stats_popup_regression', 'plain', True, []),
    ('stats-editor', 'stats_popup_regression', 'plain', True, ['--editor']),
    ('ui-binding', 'ui_binding_regression', 'plain', True, []),
    ('ui-reload', 'ui_reload_regression', 'plain', True, []),
    ('scoreboard-prefix', 'scoreboard_prefix_regression', 'configuration', False, []),
    ('pose-clock', 'pose_clock_regression', 'debug-flag', True, []),
    ('pose-compact', 'pose_compact_regression', 'optimized', False, []),
    ('pose-presentation', 'pose_presentation_regression', 'optimized', False, []),
    ('pose-runtime', 'pose_presentation_runtime_regression', 'optimized', False, []),
    ('pose-ordinary', 'pose_ordinary_regression', 'optimized', False, []),
    ('pose-mover-origin', 'pose_mover_origin_regression', 'optimized', False, ['--verify-fixed']),
    ('pose-eval', 'pose_eval_regression', 'optimized', False, []),
    ('pose-skin', 'pose_skin_regression', 'optimized', False, []),
    ('pose-server', 'pose_server_regression', 'optimized', False, []),
    ('compat-golden', 'compat_validate', 'configuration', False, []),
    ('compat-word', 'compat_word_probe', 'plain', True, []),
)


def make_plan(root, out, config, baseline, parallel):
    if config.get('schema') != 1 or parallel < 1:
        raise ValueError('Expected build-map schema 1 and positive parallelism')
    jobs, inputs = [], []
    root, out = Path(root).resolve(), Path(out).resolve()

    def expand(value):
        return str(value).replace('{root}', str(root)).replace('{out}', str(out)).replace('{python}', sys.executable)

    def path(value):
        result = Path(expand(value))
        return str((root / result).resolve() if not result.is_absolute() else result.resolve())

    def add(name, category, command=(), outputs=(), needs=(), reason='', env=None, evidence=(), artifact_manifest=''):
        jobs.append(Job(name, category, list(map(str, command)), list(map(str, outputs)),
                        list(needs), reason, env or {}, True, list(map(str, evidence)), str(artifact_manifest)))

    def py(script, *args):
        return [sys.executable, str(root / ('tools/' + script + '.py')), *map(str, args)]

    baseline = Path(path(baseline or config['baseline_dir'])) if baseline or config.get('baseline_dir') else None
    inputs += [Path(path(value)) for value in config.get('tool_inputs', [])]
    add('toolchain-inputs', 'toolchain-identity', py('modernize/validation_gates', 'tools',
        '--out', out / 'tool-inputs.json', *[arg for value in config.get('tool_inputs', []) for arg in ('--input', path(value))]),
        [out / 'tool-inputs.json'], reason='' if config.get('tool_inputs') else 'Explicit compiler/linker/SDK identity inputs required')

    add('risk', 'risk/classification', py('modernize/risk_scan', '--out', out / 'risk'),
        [out / 'risk/risk-inventory.json', out / 'risk/risk-summary.md'])
    add('compat-hashes', 'normalized-source-hash', py('modernize/compat_validate', '--hash-only', '--out', out / 'compat-hashes'),
        [out / 'compat-hashes/compat-hashes.json'])
    add('vendor', 'vendor-lock', py('modernize/vendor_check', '--out', out / 'vendor.json'), [out / 'vendor.json'])
    add('release-source', 'package/public-boundary', py('release/validate', 'source', '--expect-version', '1.0.2'))
    add('release-tests', 'package/public-boundary', [sys.executable, '-m', 'unittest', 'discover', '-s', 'tools/release', '-p', 'test_*.py'])
    add('orchestration-tests', 'bundle-contract', [sys.executable, '-m', 'unittest', 'discover', '-s', 'tools/modernize', '-p', 'test_validation_*.py'])
    if baseline:
        baseline = Path(baseline).resolve()
        source_base = baseline / 'inventory/risk-inventory.json'
        inputs += [source_base, baseline / 'symbol-baseline.json']
        source_reviews = config.get('source_reviews')
        if source_reviews:
            review_path = Path(path(source_reviews))
            inputs.append(review_path)
            if review_path.is_file():
                inputs += proof_input_paths(root, json.loads(review_path.read_text(encoding='utf-8')))
        add('source-diff', 'normalized-source-hash', py('modernize/source_diff', '--baseline', source_base,
            '--out', out / 'source-diff.json', *(['--reviews', path(source_reviews)] if source_reviews else [])),
            [out / 'source-diff.json'], ['risk'],
            '' if source_base.is_file() else 'Missing baseline function inventory')
    else:
        add('source-diff', 'normalized-source-hash', reason='--baseline-dir required')

    build_names = []
    builds = config.get('builds', [])
    def abi_key(row):
        abi = row.get('abi', {})
        return (abi.get('system'), abi.get('architecture'), abi.get('pointer_bytes'))
    native_abis = {abi_key(row) for row in builds if row.get('role') != 'matrix'}
    for role in ('debug', 'release', 'asan', 'matrix'):
        if not any(row.get('role') == role for row in builds):
            add('missing-build-' + role, 'build/' + role, reason='Explicit existing ' + role + ' build tree required')
    if not any(row.get('role') == 'release' for row in builds):
        add('missing-exports', 'exports/signatures', reason='Release modules and symbol/signature baseline required')
    if not any(row.get('role') != 'matrix' or row.get('runnable') for row in builds):
        add('missing-sdk-abi', 'SDK-ABI/goldens', reason='Runnable native build and seven portable tests required')
    for row in builds:
        name, role = row['name'], row['role']
        if role not in ('debug', 'release', 'asan', 'matrix'):
            raise ValueError('Unknown build role: ' + role)
        tree = path(row['directory'])
        configuration = row.get('configuration', 'Release' if role in ('release', 'matrix') else 'Debug')
        if (role in ('debug', 'asan') and configuration != 'Debug') or (role == 'release' and configuration != 'Release'):
            raise ValueError('Build role/configuration mismatch: ' + name)
        args = row.get('configure_args')
        reason = '' if Path(tree, 'CMakeCache.txt').is_file() and isinstance(args, list) else 'Existing CMakeCache and explicit configure_args required'
        env = row.get('env', {})
        abi = row.get('abi', {})
        if not (abi.get('system') and abi.get('architecture') and abi.get('pointer_bytes') in (4, 8)):
            reason = reason or 'Explicit actual system/architecture/pointer_bytes ABI descriptor required'
        elif role == 'matrix' and abi_key(row) in native_abis:
            raise ValueError('Matrix target ABI must differ from the native build subset: ' + name)
        prefix = 'build-' + name
        command = ['cmake', '--fresh', '-S', str(root), '-B', tree, *(args or []),
                   '-DBUILD_TESTING=ON', '-DFEATURE_OMNIBOT=ON', '-DTCE_BUILD_OMNIBOT_CORE=ON',
                   '-DFEATURE_NAVSTUDIO=OFF', '-DTCE_BUILD_REFERENCE_TESTS=OFF']
        if role == 'asan':
            command += ['-DTCE_ENABLE_SANITIZERS=ON', '-DTCE_SANITIZERS=' + row.get('sanitizers', 'address' if os.name == 'nt' else 'address,undefined')]
        add(prefix + '-configure', 'clean-configure/' + role, command, needs=['vendor', 'toolchain-inputs'], reason=reason, env=env)
        cache_report = out / (prefix + '-cache.json')
        add(prefix + '-cache', 'public-build-policy', py('modernize/validation_gates', 'cache', '--tree', tree,
            '--root', root, '--role', role, '--configuration', configuration, '--abi', json.dumps(abi),
            '--out', cache_report), [cache_report], [prefix + '-configure'], env=env, artifact_manifest=cache_report)
        add(prefix, 'build/' + role, ['cmake', '--build', tree, '--config', configuration,
            '--clean-first', '--parallel', str(parallel)], needs=[prefix + '-cache'], env=env)
        # The emitted target-path manifest avoids bin/Release guesses.
        modules = Path(tree) / ('tce-modules-' + configuration + '.json')
        report = out / (prefix + '-modules.json')
        add(prefix + '-modules', 'binary/public-boundary', py('modernize/validation_gates', 'modules',
            '--manifest', modules, '--configuration', configuration, '--out', report),
            [report], [prefix], env=env, artifact_manifest=report)
        build_names.append(prefix + '-modules')
        if role != 'matrix' or row.get('runnable', False):
            ctest_report = out / (prefix + '-ctest.json')
            add(prefix + '-ctest', 'SDK-ABI/goldens/' + role, py('modernize/validation_gates', 'ctest',
                '--tree', tree, '--configuration', configuration, '--out', ctest_report),
                [ctest_report], [prefix], env=env, artifact_manifest=ctest_report)
        core = row.get('core')
        core_report = out / (prefix + '-boundary.json')
        add(prefix + '-bot-boundary', 'public-core-boundary', py('modernize/validation_gates', 'core',
            '--manifest', Path(tree) / ('omnibot-runtime-' + configuration + '.json'),
            '--configuration', configuration, '--abi', json.dumps(abi), '--out', core_report,
            *(['--core', path(core)] if core else []),
            *(['--map', path(row['linker_map'])] if row.get('linker_map') else []),
            *(['--nm', path(row['nm'])] if row.get('nm') else [])),
            [core_report], [prefix], env=env, artifact_manifest=core_report)
        if role == 'release':
            add(prefix + '-exports', 'exports/signatures', py('modernize/validation_gates', 'exports',
                '--manifest', modules, '--baseline', (baseline / 'symbol-baseline.json') if baseline else 'missing',
                '--root', root, '--out', out / (prefix + '-exports.json'),
                *(['--nm', path(row['nm'])] if row.get('nm') else [])),
                [out / (prefix + '-exports.json')], [prefix + '-modules'],
                '' if baseline and (baseline / 'symbol-baseline.json').is_file() else 'Symbol/signature baseline required', env=env)

    assets = config.get('assets', {})
    asset_reason = '' if all(assets.get(key) for key in ('mod_pk3', 'base_pk3', 'captures', 'full_assets')) else 'Explicit local PK3/captures/full_assets map required'
    mod, base = path(assets.get('mod_pk3', 'missing-mod')), path(assets.get('base_pk3', 'missing-base'))
    captures = path(assets.get('captures', 'missing-captures'))
    bound = [path(value) for value in assets.get('bound_captures', [])]
    if not asset_reason:
        inputs += [Path(mod), Path(base), Path(path(assets['full_assets']))]
        inputs += [Path(captures) / (name + '.jsonl') for name in CAPTURES] + list(map(Path, bound))
    generated = out / 'generated'
    for kind, script in [('eval', 'pose_eval_fixtures'), ('skin', 'pose_skin_fixtures'), ('adapter', 'pose_adapter_fixtures')]:
        destination = generated / kind
        extra = [] if kind == 'adapter' else ['--captures', captures]
        if kind == 'skin':
            extra += [arg for item in bound for arg in ('--bound-capture', item)]
        add('generate-' + kind, 'fixture-inputs', py('modernize/' + script, '--mod-pk3', mod,
            '--base-pk3', base, '--out', destination, *extra), [destination / 'manifest.json', destination], reason=asset_reason)
    add('bound-inputs', 'fixture-inputs', py('modernize/pose_skin_bound_inputs_test', '--mod-pk3', mod,
        '--base-pk3', base, '--out', out / 'bound-inputs.json',
        *[arg for item in bound for arg in ('--capture', item)]), [out / 'bound-inputs.json'],
        reason=asset_reason or ('' if bound else 'Explicit actual bound normal-CGame captures required'))
    add('carrier-audit', 'integration/source', py('modernize/pose_carrier_audit', '--out', out / 'carrier'), [out / 'carrier/carrier-audit.json'])
    installation = assets.get('installation')
    if installation:
        install = Path(path(installation))
        inputs += sorted((install / 'tce2').glob('*.pk3')) + sorted((install / 'etmain').glob('*.pk3'))
    add('presentation-hook-audit', 'integration/source-assets', py('modernize/pose_presentation_hook_audit',
        '--out', out / 'hooks', '--installation', path(installation or 'missing-installation')),
        [out / 'hooks/report.json'], reason='' if installation else 'Explicit sole existing installation required (read-only asset scan)')
    for mode in MODES:
        for name, script, convention, windows, extra in FIXTURES:
            # Some existing runners intentionally have one fixed optimization
            # profile. Do not relabel or rerun that profile as Debug and O2.
            if convention == 'plain' and mode == 'release':
                continue
            fixture_mode = 'default' if convention == 'plain' and mode == 'debug' else mode
            destination = out / 'fixtures' / fixture_mode / name
            flags = list(extra)
            if convention == 'configuration':
                flags += ['--configuration', 'Release' if mode == 'release' else 'Debug']
            elif convention == 'optimized' and mode == 'release':
                flags += ['--optimized']
            elif convention == 'debug-flag' and mode != 'release':
                flags += ['--debug']
            if mode == 'asan':
                flags += ['--sanitize']
                if os.name == 'nt' and name in ('weapon', 'movement', 'bomb-lifecycle'):
                    flags += ['--msvc-version', '14.44', '--arch', 'x86']
                elif os.name == 'nt' and name == 'compat-golden':
                    flags += ['--msvc-version', '14.44', '--msvc-arch', 'x86']
            if name.startswith('hitbox'):
                flags += ['--path', 'windows' if os.name == 'nt' else 'linux']
            needs, reason = [], ''
            if windows and os.name != 'nt':
                reason = 'Runner requires Windows MSVC; native Linux is not fabricated with preprocessor flags'
            if name == 'pose-compact':
                flags += ['--mod-pk3', mod, '--base-pk3', base]
                reason = reason or asset_reason
            if name in ('pose-eval', 'pose-skin', 'pose-server'):
                kind = 'eval' if name in ('pose-eval', 'pose-server') else 'skin'
                flags += ['--fixtures', str(generated / kind)]
                needs = ['generate-' + kind]
                if name == 'pose-server':
                    flags += ['--full-assets', path(assets.get('full_assets', 'missing-full-assets')),
                              '--adapter-fixtures', str(generated / 'adapter')]
                    needs += ['generate-adapter']
            filename = ('result.json' if name == 'server-lifecycle' else
                        'compat-validation.json' if name == 'compat-golden' else
                        'compat-word-probe.json' if name == 'compat-word' else 'report.json')
            add(name + '-' + fixture_mode, 'deterministic/' + fixture_mode, py('modernize/' + script, '--out', destination, *flags),
                [destination / filename, destination], needs, reason)
    # Explicit commands are reviewed by the caller; no launcher, SSH, download,
    # original DLL or credentials are discovered or invoked automatically.
    for name, category in [('critical-ir-review', 'risk/critical-IR-review'),
                           ('native-compiler-matrix', 'native/GCC-Clang-ABI'),
                           ('package-closure', 'package/full-client-closure'),
                           ('runtime-smoke', 'runtime/startup-map-game')]:
        row = config.get('gates', {}).get(name, {})
        inputs += [Path(path(value)) for value in row.get('inputs', [])]
        add(name, category, [expand(arg) for arg in row.get('command', [])], [path(p) for p in row.get('outputs', [])],
            build_names if name != 'critical-ir-review' else ['risk', 'source-diff'] +
                [job.name for job in jobs if job.category.startswith('deterministic/')],
            '' if row.get('command') and row.get('outputs') and row.get('inputs') else
            'Explicit reviewed command, inputs and regenerated outputs required',
            {key: expand(value) for key, value in row.get('env', {}).items()},
            evidence=[path(p) for p in row.get('evidence', [])])
    return jobs, inputs
