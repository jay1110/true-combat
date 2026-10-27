"""Build only normal Game modules/core and stage exact target binaries."""
import argparse
from pathlib import Path

from build import execute
from loose_binaries import ROOT, stage_target, target_table


def build_commands(target, build, boost, parallel=4):
    row = target_table()[target]
    config = ['-DCMAKE_BUILD_TYPE=Release']
    config += ['-A', row['architecture']] if row.get('architecture') else ['-G', 'Ninja']
    if row.get('toolchain'):
        config.append('-DCMAKE_TOOLCHAIN_FILE=' + str(ROOT / 'cmake/toolchains' / row['toolchain']))
    config += row.get('flags', [])
    modules, core = build / 'modules', build / 'core'
    return [
        ('configure-modules', ['cmake', '-S', str(ROOT), '-B', str(modules), *config,
          '-DBUILD_TESTING=OFF', '-DTCE_BUILD_REFERENCE_TESTS=OFF', '-DFEATURE_OMNIBOT=ON',
          '-DTCE_BUILD_OMNIBOT_CORE=OFF', '-DFEATURE_NAVSTUDIO=OFF',
          '-DTCE_BUILD_NAVSTUDIO=OFF', '-DTCE_BUILD_NAVMESH_TOOL=OFF', '-DTCE_BUILD_TCE_HELPERS=OFF']),
        ('build-modules', ['cmake', '--build', str(modules), '--config', 'Release',
          '--target', 'tce_modules', '--parallel', str(parallel)]),
        ('configure-core', ['cmake', '-S', str(ROOT / 'omnibot'), '-B', str(core), *config,
          '-DFEATURE_NAVSTUDIO=OFF', '-DOMNIBOT_ET=ON', '-DOMNIBOT_RTCW=OFF',
          '-DOMNIBOT_STD_FILESYSTEM=ON', '-DOMNIBOT_BOOST_INCLUDEDIR=' + str(boost)]),
        ('build-core', ['cmake', '--build', str(core), '--config', 'Release',
          '--target', 'omnibot-et', '--parallel', str(parallel)]),
    ]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--target', required=True)
    parser.add_argument('--build', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--boost', type=Path, required=True)
    parser.add_argument('--parallel', type=int, default=4)
    args = parser.parse_args()
    build, output = args.build.resolve(), args.out.resolve()
    for name, command in build_commands(args.target, build, args.boost.resolve(), args.parallel):
        log = output.parent / 'diagnostics' / (name + '.log')
        code = execute(command, log)
        if code:
            print(log.read_text(encoding='utf-8', errors='replace')[-12000:])
            return code
    print(stage_target(build / 'modules/tce-modules-Release.json',
                       build / 'core/omnibot-runtime-Release.json', output, args.target))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
