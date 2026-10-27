#!/usr/bin/env python3
"""Compile the actual G_SendScore body with controlled score transport services."""
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


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--source', type=Path, default=ROOT / 'src/game/g_cmds.c')
    parser.add_argument('--configuration', choices=('Debug', 'Release'), default='Debug')
    parser.add_argument('--arch', choices=('x86', 'x64'))
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', default='auto')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = args.source.resolve()
    body = function(source.read_text(encoding='utf-8-sig'), 'G_SendScore')
    fixture = Path(__file__).with_name('score_state_regression_fixture.c')
    generated = out / 'score_state.c'
    generated.write_text(fixture.read_text(encoding='utf-8').replace('/* ACTUAL_SEND_SCORE */', body),
                         encoding='utf-8', newline='\n')
    arch = args.arch or ('x64' if args.sanitize else 'x86')
    shared = ROOT / 'src/game/q_shared.c'
    if os.name == 'nt':
        env, compiler = msvc_environment(out, arch, '14.44')
        exe = out / 'score_state.exe'
        command = [compiler, '/nologo', '/TC', '/W3', '/DWIN32', '/D_CRT_SECURE_NO_WARNINGS',
                   '/Od' if args.configuration == 'Debug' else '/O2', '/I' + str(ROOT),
                   str(generated), str(shared), '/Fo:' + str(out) + os.sep, '/Fe:' + str(exe)]
        if args.sanitize:
            command[1:1] = ['/fsanitize=address', '/Zi', '/Fd:' + str(out / 'fixture.pdb')]
            command += ['/link', '/DEBUG']
    else:
        env = os.environ.copy()
        compiler = (shutil.which('clang') or shutil.which('gcc')) if args.compiler == 'auto' else shutil.which(args.compiler)
        if not compiler:
            raise RuntimeError('No native GCC/Clang compiler found')
        exe = out / 'score_state'
        command = [compiler, '-std=c99', '-Wall', '-Wextra',
                   '-O0' if args.configuration == 'Debug' else '-O2', '-I', str(ROOT),
                   str(generated), str(shared), '-lm', '-o', str(exe)]
        if args.arch:
            command.insert(1, '-m32' if arch == 'x86' else '-m64')
        if args.sanitize:
            command[1:1] = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all',
                            '-fno-omit-frame-pointer', '-g']
    built = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out / 'compile.log').write_text(built.stdout + built.stderr, encoding='utf-8')
    report = {'source': str(source), 'source_sha256': digest(source),
              'function_sha256': hashlib.sha256(body.encode()).hexdigest(),
              'inputs_sha256': {str(p.relative_to(ROOT)): digest(p) for p in
                               (Path(__file__), fixture, shared, ROOT / 'src/game/g_local.h',
                                ROOT / 'src/game/bg_public.h', ROOT / 'src/game/q_shared.h',
                                ROOT / 'src/game/tce_bg.h', ROOT / 'src/game/tce_numeric.h',
                                ROOT / 'compat/legacy/game/tce_numeric.h')},
              'generated_sha256': digest(generated), 'command': command,
              'configuration': args.configuration, 'arch': arch if os.name == 'nt' or args.arch else 'native',
              'sanitizer': args.sanitize, 'build_exit': built.returncode,
              'limits': ['Actual unchanged extracted G_SendScore body and production q_shared helpers.',
                         'Controlled server transport and class conversion; actual SDK structures/constants.',
                         'No client/PVS state is supplied; server packet fields are asserted directly.',
                         'No engine, map, network, renderer or live game execution.']}
    if built.returncode:
        print(built.stdout + built.stderr)
    else:
        result = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True, timeout=30)
        report.update(exit_code=result.returncode, stdout=result.stdout, stderr=result.stderr,
                      executable_sha256=digest(exe))
        summary = re.search(r'score state cases=(\d+) checks=(\d+) failures=(\d+) failed_cases=(\d+)', result.stdout)
        if summary:
            report.update(dict(zip(('cases', 'checks', 'failures', 'failed_cases'), map(int, summary.groups()))))
        print(result.stdout + result.stderr, end='')
    report['passed'] = built.returncode == 0 and report.get('exit_code') == 0 and report.get('failures') == 0
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
