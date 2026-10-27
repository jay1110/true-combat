"""Compile the pure pose clock and actual accepted-animation callback path."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

from compat_validate import msvc_environment
from weapon_regression import function

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--animation-source', type=Path, default=ROOT / 'src/game/bg_animation.c')
    parser.add_argument('--arch', choices=('x86', 'x64'), default='x86')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--debug', action='store_true', help='Disable optimization for the isolated fixture')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source = args.animation_source.read_text(encoding='utf-8-sig')
    fixture = (ROOT / 'tools/modernize/pose_clock_regression_fixture.c').read_text(encoding='utf-8')
    generated = fixture.replace('/* ACTUAL_BG_PLAYANIM */', function(source, 'BG_PlayAnim'))
    path = out / 'fixture.c'
    path.write_text(generated, encoding='utf-8', newline='\n')
    env, compiler = msvc_environment(out, args.arch, '14.44' if args.sanitize else None)
    exe = out / 'fixture.exe'
    command = [compiler, '/nologo', '/TC', '/Od' if args.debug else '/O2', '/W4', '/DWIN32', '/DGAMEDLL',
               '/D_CRT_SECURE_NO_WARNINGS', '/I' + str(ROOT), str(path),
               str(ROOT / 'src/game/bg_tce_pose_clock.c'), '/Fe:' + str(exe)]
    if args.sanitize:
        command[1:1] = ['/fsanitize=address', '/Z7']
    command += ['/link']
    if args.sanitize:
        command += ['/DEBUG']
    built = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out / 'compile.log').write_text(built.stdout + built.stderr, encoding='utf-8')
    files = ['src/game/bg_tce_pose_clock.c', 'src/game/bg_tce_pose_clock.h',
             'src/game/bg_public.h', 'tools/modernize/pose_clock_regression_fixture.c']
    report = {'arch': args.arch, 'asan': args.sanitize, 'debug': args.debug,
              'animation_source': str(args.animation_source),
              'animation_sha256': hashlib.sha256(source.encode()).hexdigest(),
              'source_hashes': {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest()
                                for name in files},
              'generated_sha256': hashlib.sha256(generated.encode()).hexdigest(),
              'command': command, 'build_returncode': built.returncode,
              'limits': ['Raw clip clock, not observer transition/swing or renderer parity',
                         'Actual BG_PlayAnim assignments execute against real game structs',
                         'Private observer is installed by fixture; game lifecycle integration is separate',
                         'No phase is enabled for damage, no engine/game/VM/network run']}
    if built.returncode:
        print(built.stdout + built.stderr)
    else:
        run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True, timeout=30)
        report.update(returncode=run.returncode, stdout=run.stdout, stderr=run.stderr)
        print(run.stdout + run.stderr, end='')
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    return int(bool(built.returncode or report.get('returncode')))


if __name__ == '__main__':
    raise SystemExit(main())
