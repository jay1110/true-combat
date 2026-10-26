"""Build one release target/variant; preserve useful modules if core fails."""
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from package import ROOT, package_modules


def execute(arguments, log):
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open('w', encoding='utf-8') as stream:
        result = subprocess.run(arguments, stdout=stream, stderr=subprocess.STDOUT)
    print('exit', result.returncode, 'log', log, flush=True)
    return result.returncode


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', required=True)
    parser.add_argument('--variant', choices=('game', 'navstudio'), required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--boost', type=Path, required=True)
    parser.add_argument('--parallel', type=int, default=4)
    args = parser.parse_args()
    row = next(x for x in json.loads((ROOT / 'tools/release/targets.json').read_text())['targets']
               if x['id'] == args.target)
    build, output = args.build.resolve(), args.out.resolve()
    config = ['-DCMAKE_BUILD_TYPE=Release']
    if row.get('architecture'):
        config += ['-A', row['architecture']]
    else:
        config += ['-G', 'Ninja']
    if row.get('toolchain'):
        config += ['-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'cmake/toolchains' / row['toolchain'])]
    config += row.get('flags', [])
    editor = 'ON' if args.variant == 'navstudio' else 'OFF'
    steps = []
    def run(command, name):
        code = execute(command, output / (name + '.log'))
        steps.append({'step': name, 'command': command, 'exit_code': code})
        return code
    mod_build, core_build = build / 'modules', build / 'core'
    mod_args = ['cmake', '-S', str(ROOT), '-B', str(mod_build), *config,
                '-DBUILD_TESTING=OFF', '-DTCE_BUILD_REFERENCE_TESTS=OFF',
                '-DFEATURE_OMNIBOT=ON', '-DTCE_BUILD_OMNIBOT_CORE=OFF',
                '-DFEATURE_NAVSTUDIO=' + editor]
    code = run(mod_args, 'configure-modules')
    if code == 0:
        code = run(['cmake', '--build', str(mod_build), '--config', 'Release',
                    '--target', 'tce_modules', '--parallel', str(args.parallel)], 'build-modules')
    core_manifest = None
    core_args = ['cmake', '-S', str(ROOT / 'omnibot'), '-B', str(core_build), *config,
                 '-DFEATURE_NAVSTUDIO=' + editor, '-DOMNIBOT_ET=ON', '-DOMNIBOT_RTCW=OFF',
                 '-DOMNIBOT_STD_FILESYSTEM=ON', '-DOMNIBOT_BOOST_INCLUDEDIR=' + str(args.boost.resolve())]
    if run(core_args, 'configure-core') == 0 and run([
            'cmake', '--build', str(core_build), '--config', 'Release',
            '--target', 'omnibot-et', '--parallel', str(args.parallel)], 'build-core') == 0:
        core_manifest = core_build / 'omnibot-runtime-Release.json'
        if not core_manifest.is_file():
            raise ValueError('Core build did not generate TARGET_FILE discovery manifest')
    archive = None
    if code == 0:
        modules = mod_build / 'bin/Release'
        if not modules.is_dir():
            modules = mod_build / 'bin'
        archive = package_modules(modules, output, args.target, args.variant, core_manifest)
    complete = code == 0 and core_manifest is not None
    report = {'schema': 1, 'target': args.target, 'variant': args.variant,
              'complete_gameplay_runtime': complete, 'steps': steps,
              'archive': str(archive) if archive else None,
              'github_sha': os.environ.get('GITHUB_SHA'), 'engine_started': False}
    (output / 'build-report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    if not complete:
        print('Incomplete target: inspect concrete configure/build logs; modules-only output is not bot gameplay support.', file=sys.stderr)
    return 0 if complete else 1


if __name__ == '__main__':
    raise SystemExit(main())
