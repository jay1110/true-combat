"""One bundled, logged mod-only validation pass; never deploys a game.

Run from a clean output directory. Unsupported SDK/toolchains are reported as
unavailable, never successful builds. Native game smoke remains an explicit
separate invocation against the one existing installation.

Final source-bound acceptance is opt-in: --strict-release --help. Its explicit
existing-tree build map preserves normal Omni-bot gameplay in Game builds.
"""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]


def main():
    # Preserve the historical command/output contract. Final acceptance uses an
    # explicit strict mode with existing-tree maps and fail-closed required gates.
    if '--strict-release' in sys.argv[1:]:
        from validation_bundle import main as strict_main
        try:
            raise SystemExit(strict_main([arg for arg in sys.argv[1:] if arg != '--strict-release']))
        except (OSError, ValueError, KeyError) as error:
            print(str(error), file=sys.stderr)
            raise SystemExit(2)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/modernization-validation')
    parser.add_argument('--cross', action='store_true', help='Also build available Zig Linux and Emscripten targets')
    parser.add_argument('--resume', action='store_true', help='Explicitly reuse this bundle after fixing a reported failure')
    parser.add_argument('--parallel', type=int, default=4)
    parser.add_argument('--baseline-dir', type=Path, help='Local baseline containing inventory/risk-inventory.json and symbol-baseline.json')
    args = parser.parse_args()
    out = args.out.resolve()
    if out.exists() and any(out.iterdir()) and not args.resume:
        parser.error('Output is not empty; select a clean directory or explicitly --resume')
    out.mkdir(parents=True, exist_ok=True)
    if args.resume:
        previous = out / ('previous-run-' + str(time.time_ns()))
        previous.mkdir()
        for path in [*out.glob('*.log'), out / 'validation.json', out / 'validation.md']:
            if path.is_file():
                shutil.copy2(path, previous / path.name)
    report = {'schema': 1, 'started': time.time(), 'steps': [], 'unavailable': []}
    env = os.environ.copy()
    asan = Path('C:/Program Files/Microsoft Visual Studio/2022/Community/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64')
    if asan.is_dir():
        env['PATH'] = str(asan) + os.pathsep + env['PATH']

    def save():
        (out / 'validation.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
        rows = ['# Mod validation bundle', '', '| Check | Result | Log |', '|---|---|---|']
        for step in report['steps']:
            rows.append(f"| {step['name']} | {'PASS' if step['returncode'] == 0 else 'FAIL'} | {step['log']} |")
        rows += ['', 'Unavailable / not run:', *['- ' + item for item in report['unavailable']], '',
                 'Build success and smoke tests do not establish complete behavioral parity.']
        (out / 'validation.md').write_text('\n'.join(rows) + '\n', encoding='utf-8')

    def run(name, command):
        started = time.time()
        log = out / (name + '.log')
        with log.open('w', encoding='utf-8') as stream:
            result = subprocess.run([str(item) for item in command], cwd=ROOT, env=env, stdout=stream, stderr=subprocess.STDOUT)
        report['steps'].append({'name': name, 'command': [str(item) for item in command], 'returncode': result.returncode,
                                'seconds': round(time.time() - started, 2), 'log': log.name})
        save()
        print(f"{name}: {'PASS' if result.returncode == 0 else 'FAIL'} ({time.time() - started:.1f}s)", flush=True)
        return result.returncode == 0

    def native(name, arch, sanitizers=False):
        build = out / name
        command = ['cmake', '-S', ROOT, '-B', build, '-G', 'Visual Studio 17 2022', '-A', arch,
                   '-DBUILD_TESTING=ON', '-DTCE_BUILD_REFERENCE_TESTS=OFF', '-DFEATURE_NAVSTUDIO=OFF', '-DFEATURE_OMNIBOT=OFF']
        if sanitizers:
            if asan.is_dir():
                command += ['-T', 'v143,version=14.44.35207']
            command += ['-DTCE_ENABLE_SANITIZERS=ON', '-DTCE_SANITIZERS=address']
        if not run(name + '-configure', command):
            return
        for configuration in (['Debug'] if sanitizers else ['Debug', 'Release']):
            label = name + '-' + configuration.lower()
            if run(label + '-build', ['cmake', '--build', build, '--config', configuration, '--parallel', args.parallel]):
                run(label + '-ctest', ['ctest', '--test-dir', build, '-C', configuration, '--output-on-failure'])

    def msvc_sanitizer():
        # VS's generator can silently resolve an older registered toolset even
        # when -T version is supplied. Use one explicit matching compiler, CRT
        # and ASAN runtime through the developer environment instead.
        nonlocal env
        from compat_validate import msvc_environment
        saved_env = env
        env, compiler = msvc_environment(out, 'x64', '14.44')
        env = {key.upper(): value for key, value in env.items()}
        build = out / 'windows-x64-asan1444'
        if run('windows-x64-asan-configure', ['cmake', '-S', ROOT, '-B', build, '-G', 'Ninja',
                    '-DCMAKE_C_COMPILER=' + compiler, '-DCMAKE_BUILD_TYPE=Debug', '-DBUILD_TESTING=ON',
                    '-DTCE_ENABLE_SANITIZERS=ON', '-DTCE_SANITIZERS=address', '-DFEATURE_NAVSTUDIO=OFF', '-DFEATURE_OMNIBOT=OFF']):
            if run('windows-x64-asan-debug-build', ['cmake', '--build', build, '--parallel', args.parallel]):
                run('windows-x64-asan-debug-ctest', ['ctest', '--test-dir', build, '--output-on-failure'])
        env = saved_env

    run('risk-scan', [sys.executable, ROOT / 'tools/modernize/risk_scan.py', '--out', out / 'risk'])
    if os.name == 'nt':
        native('windows-x86', 'Win32')
        msvc_sanitizer()
    else:
        for compiler in ('clang', 'gcc'):
            if not shutil.which(compiler):
                report['unavailable'].append(compiler + ': not installed')
                continue
            build = out / ('linux-' + compiler)
            if run(compiler + '-configure', ['cmake', '-S', ROOT, '-B', build, '-G', 'Ninja',
                                            '-DCMAKE_BUILD_TYPE=Debug', '-DCMAKE_C_COMPILER=' + compiler,
                                            '-DBUILD_TESTING=ON', '-DTCE_ENABLE_SANITIZERS=ON']):
                if run(compiler + '-build', ['cmake', '--build', build, '--parallel', args.parallel]):
                    run(compiler + '-ctest', ['ctest', '--test-dir', build, '--output-on-failure'])
    if args.cross:
        zig = ROOT / 'build/linux-toolchain/ziglang/zig.exe'
        if zig.is_file():
            for arch in ('i386', 'x86_64', 'armv7', 'aarch64'):
                toolchain = ROOT / ('cmake/toolchains/linux-zig-' + arch + '.cmake')
                if not toolchain.is_file():
                    report['unavailable'].append('Zig Linux ' + arch + ': no toolchain input')
                    continue
                name = 'linux-' + arch
                build = out / name
                if run(name + '-configure', ['cmake', '-S', ROOT, '-B', build, '-G', 'Ninja',
                                            '-DCMAKE_TOOLCHAIN_FILE=' + str(toolchain), '-DTCE_ZIG_EXECUTABLE=' + str(zig),
                                            '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=OFF']):
                    run(name + '-build', ['cmake', '--build', build, '--parallel', args.parallel])
        else:
            report['unavailable'].append('Zig Linux subset: compiler not installed')
        emscripten = Path('C:/emsdk/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake')
        if emscripten.is_file():
            build = out / 'wasm32'
            if run('wasm32-configure', ['cmake', '-S', ROOT, '-B', build, '-G', 'Ninja',
                                       '-DCMAKE_TOOLCHAIN_FILE=' + str(emscripten), '-DCMAKE_BUILD_TYPE=Release', '-DBUILD_TESTING=ON']):
                if run('wasm32-build', ['cmake', '--build', build, '--parallel', args.parallel]):
                    run('wasm32-ctest', ['ctest', '--test-dir', build, '--output-on-failure'])
        else:
            report['unavailable'].append('WASM32: Emscripten SDK not installed')
    run('compat-body-hashes', [sys.executable, ROOT / 'tools/modernize/compat_validate.py', '--hash-only', '--out', out / 'compat-hashes'])
    run('vendor-input-hashes', [sys.executable, ROOT / 'tools/modernize/vendor_check.py', '--out', out / 'vendor-lock-check.json'])
    if args.baseline_dir:
        baseline = args.baseline_dir.resolve()
        run('source-body-diff', [sys.executable, ROOT / 'tools/modernize/source_diff.py', '--baseline', baseline / 'inventory/risk-inventory.json', '--out', out / 'source-diff.json'])
        native_bins = out / 'windows-x86/bin/Release'
        if native_bins.is_dir():
            original = json.loads((baseline / 'symbol-baseline.json').read_text())
            original['binaries'] = {key: value for key, value in original['binaries'].items() if key.startswith(('cgame_', 'ui_', 'qagame_'))}
            reduced = out / 'symbol-baseline-modules.json'
            reduced.write_text(json.dumps(original), encoding='utf-8')
            run('native-exports-signatures', [sys.executable, ROOT / 'tools/modernize/symbol_diff.py', '--baseline', reduced,
                     '--out', out / 'symbol-diff.json', *[native_bins / name for name in original['binaries']]])
    report['unavailable'] += ['Android ARMv7/ARM64: requires separately installed NDK and external matching engine host',
                              'macOS universal: requires Apple SDK/toolchain and external matching engine host',
                              'GCC runtime validation on this Windows host: use the Linux CI job',
                              'Cross-built modules require an external host; no cross-target gameplay parity claimed']
    report['finished'] = time.time()
    report['passed'] = all(item['returncode'] == 0 for item in report['steps'])
    save()
    raise SystemExit(not report['passed'])


if __name__ == '__main__':
    main()
