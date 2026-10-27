"""Small strict-bundle adapters around existing checks; no engine services."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import subprocess
import sys

from validation_bundle import file_hash

PORTABLE_TESTS = {'tce_compat_golden', 'tce_compat_wasm_vector_golden',
                  'tce_score_packet_test', 'tce_geoip_test',
                  'tce_client_presentation_test', 'tce_geoip_policy_test', 'tce_item_presentation_test'}


def cache_values(tree):
    values = {}
    for line in (Path(tree) / 'CMakeCache.txt').read_text(encoding='utf-8').splitlines():
        if line and not line.startswith(('#', '//')) and '=' in line and ':' in line.split('=', 1)[0]:
            key, value = line.split('=', 1)
            values[key.split(':', 1)[0]] = value
    return values


def check_cache(tree, root, role, configuration=None, abi=None):
    values = cache_values(tree)
    enabled = {'ON', 'TRUE', 'YES', '1'}
    errors = []
    for key in ('BUILD_TESTING', 'FEATURE_OMNIBOT', 'TCE_BUILD_OMNIBOT_CORE'):
        if values.get(key, '').upper() not in enabled:
            errors.append(key + ' must be enabled')
    for key in ('FEATURE_NAVSTUDIO', 'TCE_BUILD_REFERENCE_TESTS'):
        if values.get(key, '').upper() not in {'OFF', 'FALSE', 'NO', '0'}:
            errors.append(key + ' must be explicitly disabled')
    if Path(values.get('CMAKE_HOME_DIRECTORY', '')).resolve() != Path(root).resolve():
        errors.append('CMake source directory does not match this source freeze')
    if role == 'asan' and (values.get('TCE_ENABLE_SANITIZERS', '').upper() not in enabled or
                           'address' not in values.get('TCE_SANITIZERS', '').split(',')):
        errors.append('ASan build must explicitly instrument address checks')
    if configuration:
        configurations = values.get('CMAKE_CONFIGURATION_TYPES', '').split(';')
        if values.get('CMAKE_CONFIGURATION_TYPES'):
            if configuration not in configurations:
                errors.append('Declared configuration not available in the multi-config generator')
        elif values.get('CMAKE_BUILD_TYPE') != configuration:
            errors.append('Single-config CMAKE_BUILD_TYPE differs from declared ' + configuration)
    metadata_files = [p for pattern in ('*/CMakeSystem.cmake', '*/CMakeCCompiler.cmake')
                      for p in (Path(tree) / 'CMakeFiles').glob(pattern)]
    if abi:
        metadata = ''.join(p.read_text(encoding='utf-8') for p in metadata_files)
        ptr = re.findall(r'set\(CMAKE_C_SIZEOF_DATA_PTR\s+"?([48])"?\)', metadata)
        system = re.findall(r'set\(CMAKE_SYSTEM_NAME\s+"([^"]+)"\)', metadata)
        if not ptr or int(ptr[-1]) != abi.get('pointer_bytes') or not system or system[-1] != abi.get('system'):
            errors.append('Generated compiler/system ABI does not match declared target')
    return {'passed': not errors, 'errors': errors, 'cache_sha256': file_hash(Path(tree) / 'CMakeCache.txt'),
            'artifact_paths': [str(p.resolve()) for p in [Path(tree) / 'CMakeCache.txt', *metadata_files]],
            'policy': {key: value for key, value in values.items() if key.startswith(('FEATURE_', 'TCE_'))}}


def boundary_checker():
    from importlib import util
    root = Path(__file__).resolve().parents[2]
    spec = util.spec_from_file_location('bundle_boundary', root / 'tools/release/verify_omnibot_boundary.py')
    checker = util.module_from_spec(spec)
    sys.path.insert(0, str(root / 'tools/release'))
    spec.loader.exec_module(checker)
    return checker


def modules(manifest, configuration=None):
    checker = boundary_checker()
    data = json.loads(Path(manifest).read_text(encoding='utf-8'))
    if data.get('schema') != 1 or data.get('variant') != 'game' or set(data.get('modules', {})) != {'cgame', 'ui', 'qagame'}:
        raise ValueError('Expected exact Game target-path manifest')
    if configuration and data.get('configuration') != configuration:
        raise ValueError('Target manifest configuration does not match required build role')
    result = {}
    for role, name in data['modules'].items():
        path = Path(name)
        checker.verify_module_bytes(role, path.read_bytes(), 'game')
        result[role] = {'path': str(path.resolve()), 'sha256': file_hash(path), 'bytes': path.stat().st_size}
    return {'passed': True, 'manifest_sha256': file_hash(manifest), 'modules': result,
            'artifact_paths': [str(Path(manifest).resolve()), *[row['path'] for row in result.values()]]}


def core(manifest, configuration, abi, expected_core, linker_map, nm):
    data = json.loads(Path(manifest).read_text(encoding='utf-8'))
    if (Path(manifest).name != 'omnibot-runtime-' + configuration + '.json' or data.get('schema') != 1
            or data.get('variant') != 'game' or data.get('system') != abi.get('system')
            or data.get('architecture') != abi.get('architecture')):
        raise ValueError('Runtime manifest variant/configuration/ABI mismatch')
    library = Path(data['library']).resolve()
    if expected_core and library != Path(expected_core).resolve():
        raise ValueError('Mapped core is not this build target runtime library')
    result = boundary_checker().verify_core(library, 'game', linker_map, Path(nm) if nm else None)
    result['passed'] = True
    result['artifact_paths'] = [str(Path(manifest).resolve()), str(library)]
    if linker_map:
        result['artifact_paths'].append(str(Path(linker_map).resolve()))
    return result


def run_ctest(tree, configuration):
    base = ['ctest', '--test-dir', str(tree), '-C', configuration]
    found = subprocess.run(base + ['--show-only=json-v1'], capture_output=True, text=True, check=True)
    inventory = json.loads(found.stdout)
    names = {row['name'] for row in inventory['tests']}
    missing = sorted(PORTABLE_TESTS - names)
    if missing:
        return {'passed': False, 'missing_tests': missing, 'discovered': sorted(names)}
    result = subprocess.run(base + ['--output-on-failure', '--no-tests=error'])
    evidence = set(Path(tree).rglob('CTestTestfile.cmake'))
    for row in inventory['tests']:
        for argument in row.get('command', []):
            path = Path(argument)
            if path.is_absolute() and path.exists():
                evidence.add(path)
    return {'passed': result.returncode == 0, 'returncode': result.returncode, 'discovered': sorted(names),
            'artifact_paths': sorted(str(p.resolve()) for p in evidence),
            'scope': 'SDK/syscall compatibility and deterministic tests; not original-DLL or engine parity'}


def exports(manifest, baseline, root, nm):
    from symbol_diff import snapshot, compare
    data = json.loads(Path(manifest).read_text(encoding='utf-8'))
    before = json.loads(Path(baseline).read_text(encoding='utf-8'))
    names = {Path(value).name for value in data['modules'].values()}
    before['binaries'] = {key: value for key, value in before['binaries'].items() if key in names}
    if set(before['binaries']) != names or len(before.get('entry_signatures', {})) != 6:
        raise ValueError('Baseline must cover all three current module filenames and six SDK entry signatures')
    after = snapshot(Path(root), list(data['modules'].values()), nm)
    changes = compare(before, after)
    after.update(passed=not changes, differences=changes)
    return after


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('operation', choices=('cache', 'modules', 'ctest', 'exports', 'tools', 'core'))
    parser.add_argument('--tree', type=Path)
    parser.add_argument('--root', type=Path)
    parser.add_argument('--role')
    parser.add_argument('--manifest', type=Path)
    parser.add_argument('--baseline', type=Path)
    parser.add_argument('--nm')
    parser.add_argument('--core', type=Path)
    parser.add_argument('--map', type=Path)
    parser.add_argument('--abi', type=json.loads, default={})
    parser.add_argument('--input', type=Path, action='append', default=[])
    parser.add_argument('--configuration', default='Debug')
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args(argv)
    if args.operation == 'tools':
        result = {'passed': bool(args.input) and all(p.is_file() for p in args.input),
                  'inputs': {str(p): file_hash(p) if p.is_file() else None for p in args.input}}
    elif args.operation == 'cache':
        result = check_cache(args.tree, args.root, args.role, args.configuration, args.abi)
    elif args.operation == 'modules':
        result = modules(args.manifest, args.configuration)
    elif args.operation == 'core':
        result = core(args.manifest, args.configuration, args.abi, args.core, args.map, args.nm)
    elif args.operation == 'ctest':
        result = run_ctest(args.tree, args.configuration)
    else:
        result = exports(args.manifest, args.baseline, args.root, args.nm)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(result, indent=2))
    return int(not result['passed'])


if __name__ == '__main__':
    raise SystemExit(main())
