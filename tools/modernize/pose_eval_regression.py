#!/usr/bin/env python3
"""Build only the mod-owned pose evaluator and generated asset/capture fixture."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from compat_validate import msvc_environment

ROOT = Path(__file__).resolve().parents[2]

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--optimized', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', help='Native GCC/Clang on non-Windows hosts')
    parser.add_argument('--asset', type=Path, action='append', default=[], help='Also validate a complete original MDX/MDM file')
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    sources = [ROOT/'src/game/tce_pose_math.c', ROOT/'tools/modernize/pose_eval_test.c']
    if os.name == 'nt':
        env, compiler = msvc_environment(out, 'x64' if args.sanitize else 'x86', '14.44' if args.sanitize else '14.39')
        exe = out/'pose_eval_test.exe'
        cmd = [compiler, '/nologo', '/TC', '/W4', '/WX', '/fp:precise', '/O2' if args.optimized else '/Od',
               '/I'+str(ROOT/'src/game'), '/I'+str(args.fixtures.resolve()), '/D_CRT_SECURE_NO_WARNINGS']
        if args.sanitize: cmd += ['/fsanitize=address', '/Zi']
        cmd += [str(s) for s in sources]+['/Fo:'+str(out)+os.sep, '/Fe:'+str(exe)]
    else:
        env = os.environ.copy(); compiler = args.compiler or 'cc'; exe = out/'pose_eval_test'
        cmd = [compiler, '-std=c99', '-Wall', '-Wextra', '-Werror', '-ffp-contract=off', '-O2' if args.optimized else '-O0',
               '-I'+str(ROOT/'src/game'), '-I'+str(args.fixtures.resolve())]
        if args.sanitize: cmd += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
        cmd += [str(s) for s in sources]+['-lm', '-o', str(exe)]
    built = subprocess.run(cmd, cwd=out, env=env, capture_output=True, text=True)
    (out/'compile.log').write_text(built.stdout+built.stderr, encoding='utf-8')
    report = {'command':cmd, 'build_exit':built.returncode,
              'sources':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest()
                         for p in sources+[ROOT/'src/game/tce_pose_math.h']},
              'fixture_sha256':hashlib.sha256((args.fixtures/'pose_eval_fixtures.inc').read_bytes()).hexdigest()}
    if not built.returncode:
        run = subprocess.run([str(exe)]+[str(p.resolve()) for p in args.asset], cwd=out, env=env, capture_output=True, text=True)
        report.update(run_exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
        if not run.returncode:
            report['metrics'] = json.loads(run.stdout)
        report['full_asset_sha256'] = {str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in args.asset}
        print(run.stdout, end=''); print(run.stderr, end='')
    else: print(built.stdout+built.stderr)
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return report.get('run_exit', built.returncode)

if __name__ == '__main__':
    raise SystemExit(main())
