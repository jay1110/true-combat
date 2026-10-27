#!/usr/bin/env python3
"""Run actual scoreboard row/prefix/name bodies with controlled renderer services."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from compat_validate import msvc_environment
from weapon_regression import function

ROOT = Path(__file__).resolve().parents[2]
WRAPPERS = ('TCE_ScoreLegacyClientRow', 'TCE_ScoreClientRow')
HELPERS = ('TCE_ScoreFill', 'TCE_ScoreText', 'TCE_ScoreTranslate', 'TCE_ScoreCountry',
           'TCE_ScoreCountryFlag', 'TCE_ScoreName', 'TCE_ScorePic', 'TCE_ScoreLocateMerged')


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--source', type=Path, default=ROOT / 'src/cgame/cg_scoreboard.c')
    parser.add_argument('--configuration', choices=('Debug', 'Release'), default='Debug')
    parser.add_argument('--arch', choices=('x86', 'x64'))
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', default='auto', help='Native GCC/Clang executable on non-Windows hosts')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = args.source.resolve()
    actual = source.read_text(encoding='utf-8-sig')
    helper_source = ROOT / 'src/cgame/cg_scoreboard.c'
    helpers = helper_source.read_text(encoding='utf-8-sig')
    bodies = {name: function(actual, name) for name in WRAPPERS}
    helper_bodies = {name: function(helpers, name) for name in HELPERS}
    if re.search(r'\bTCE_ScoreSnapshotState\([^;{}]*\)\s*\{', helpers):
        helper_bodies['TCE_ScoreSnapshotState'] = function(helpers, 'TCE_ScoreSnapshotState')
    fixture = Path(__file__).with_name('scoreboard_prefix_regression_fixture.c')
    generated = out / 'scoreboard_prefix.c'
    generated.write_text(fixture.read_text(encoding='utf-8').replace('/* ACTUAL_HELPERS */', '\n\n'.join(helper_bodies.values()))
                         .replace('/* ACTUAL_WRAPPERS */', '\n\n'.join(bodies.values())), encoding='utf-8', newline='\n')
    arch = args.arch or ('x64' if args.sanitize else 'x86')
    production = ROOT / 'src/cgame/tce_client_score.c'
    if os.name == 'nt':
        env, compiler = msvc_environment(out, arch, '14.44')
        executable = out / 'scoreboard_prefix.exe'
        command = [compiler, '/nologo', '/TC', '/W3', '/fp:precise', '/Od' if args.configuration == 'Debug' else '/O2',
                   '/D_CRT_SECURE_NO_WARNINGS', '/I' + str(ROOT), str(generated), str(production),
                   '/Fo:' + str(out) + os.sep, '/Fe:' + str(executable)]
        if args.sanitize:
            command[1:1] = ['/fsanitize=address', '/Zi', '/Fd:' + str(out / 'fixture.pdb')]
            command += ['/link', '/DEBUG']
    else:
        env = os.environ.copy()
        compiler = (shutil.which('clang') or shutil.which('gcc')) if args.compiler == 'auto' else shutil.which(args.compiler)
        if not compiler:
            raise RuntimeError('No native GCC/Clang compiler found')
        executable = out / 'scoreboard_prefix'
        command = [compiler, '-std=c99', '-Wall', '-Wextra', '-O0' if args.configuration == 'Debug' else '-O2',
                   '-ffp-contract=off', '-I', str(ROOT), str(generated), str(production), '-lm', '-o', str(executable)]
        if args.arch:
            command.insert(1, '-m32' if arch == 'x86' else '-m64')
        if args.sanitize:
            command[1:1] = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-g']
    built = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out / 'compile.log').write_text(built.stdout + built.stderr, encoding='utf-8')
    report = {'source': str(source), 'source_sha256': digest(source),
              'wrapper_sha256': {name: hashlib.sha256(body.encode()).hexdigest() for name, body in bodies.items()},
              'helper_source': str(helper_source), 'helper_source_sha256': digest(helper_source),
              'helper_sha256': {name: hashlib.sha256(body.encode()).hexdigest() for name, body in helper_bodies.items()},
              'inputs_sha256': {str(path.relative_to(ROOT)): digest(path) for path in
                               (Path(__file__), fixture, production, ROOT / 'src/cgame/tce_client_score.h',
                                ROOT / 'src/game/tce_score.h', ROOT / 'src/game/tce_countryflags.h',
                                ROOT / 'src/game/tce_numeric.h', ROOT / 'compat/compat_wrap.h')},
              'generated_sha256': digest(generated), 'command': command,
              'configuration': args.configuration, 'arch': arch if os.name == 'nt' or args.arch else 'native',
              'sanitizer': args.sanitize, 'build_exit': built.returncode,
              'limits': ['Actual two row wrappers, current country/name helpers and production client score renderer are compiled unchanged.',
                         'Renderer text width is a controlled six-pixels-per-byte service; tests assert drawing arguments, clipping and column positions.',
                         'Historical --source changes only the wrappers; current helpers/context/core isolate wrapper differences.',
                         'Malformed inputs are country configstring values; row client/index validity is the parser precondition.',
                         'No renderer, engine, map, network or live scoreboard execution.']}
    if built.returncode:
        print(built.stdout + built.stderr)
    else:
        result = subprocess.run([str(executable)], cwd=out, env=env, capture_output=True, text=True, timeout=30)
        report.update(exit_code=result.returncode, stdout=result.stdout, stderr=result.stderr, executable_sha256=digest(executable))
        summary = re.search(r'scoreboard prefix cases=(\d+) checks=(\d+) failures=(\d+) route_failures=(\d+),(\d+),(\d+)', result.stdout)
        if summary:
            report.update(cases=int(summary[1]), checks=int(summary[2]), failures=int(summary[3]),
                          route_failures=list(map(int, summary.group(4, 5, 6))))
        print(result.stdout + result.stderr, end='')
    report['passed'] = built.returncode == 0 and report.get('exit_code') == 0 and report.get('failures') == 0
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
