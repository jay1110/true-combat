#!/usr/bin/env python3
"""Exercise real character resets, presentation policy and ordinary bullet selection."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from compat_validate import msvc_environment
from weapon_regression import function

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--optimized', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', help='Native GCC/Clang compiler on non-Windows hosts')
    parser.add_argument('--publisher-source', type=Path, help='Explicit comparison source; default is current production')
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    publisher = args.publisher_source or ROOT/'src/game/g_tce_pose_presentation.c'
    pose = (ROOT/'src/game/g_tce_pose.c').read_text()
    weapon = (ROOT/'src/game/g_weapon.c').read_text()
    private = pose[pose.index('typedef struct {'):pose.index('/* Deliberately module-private')]
    names = ('PoseGeneration', 'PoseCharacterIndex', 'PoseClientNumber', 'PoseIdentity',
             'G_TCEPoseResetClient', 'G_TCEPoseCharacterChanged', 'G_TCEPoseStats',
             'G_TCEPoseAssetInfo', 'G_TCEPosePresentationIdentity',
             'G_TCEPosePresentationMetadata', 'G_TCEPosePresentationProfiles')
    context = re.search(r'typedef struct\s*\{\s*qboolean hit\[MAX_CLIENTS\];[\s\S]*?\}\s*tceBulletTraceContext_t;', weapon)
    if context is None:
        raise ValueError('Actual bullet context was not found')
    fixture = (ROOT/'tools/modernize/pose_ordinary_test.c').read_text()
    replacements = {
        '@PRIVATE@': private,
        '@CHARACTER@': function((ROOT/'src/game/bg_character.c').read_text(), 'BG_GetCharacter'),
        '@POSE@': '\n'.join(function(pose, name) for name in names),
        '@PUBLISHER@': publisher.read_text().replace('#include "g_local.h"', ''),
        '@CONTEXT@': context.group(),
        '@TRACE@': function(weapon, 'G_TCETraceBullet'),
    }
    for token, value in replacements.items():
        if fixture.count(token) != 1:
            raise ValueError('Missing or repeated extraction token '+token)
        fixture = fixture.replace(token, value)
    source = out/'ordinary_generated.c'; source.write_text(fixture, encoding='utf-8', newline='\n')
    sources = [source, ROOT/'src/game/bg_tce_pose_clock.c', ROOT/'src/game/bg_tce_pose_presentation.c']
    if os.name == 'nt':
        env, compiler = msvc_environment(out, 'x64' if args.sanitize else 'x86', '14.44' if args.sanitize else '14.39')
        exe = out/'test.exe'
        cmd = [compiler, '/nologo', '/TC', '/W4', '/WX', '/wd4505', '/fp:precise', '/O2' if args.optimized else '/Od',
               '/DWIN32', '/DGAMEDLL', '/D_CRT_SECURE_NO_WARNINGS', '/I'+str(ROOT), '/I'+str(ROOT/'src/game')]
        if args.sanitize:
            cmd += ['/fsanitize=address', '/Zi']
        cmd += list(map(str, sources))+['/Fo:'+str(out)+os.sep, '/Fe:'+str(exe)]
    else:
        env = os.environ.copy(); compiler = args.compiler or 'cc'; exe = out/'test'
        cmd = [compiler, '-std=c99', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
               '-ffp-contract=off', '-O2' if args.optimized else '-O0', '-DGAMEDLL',
               '-I'+str(ROOT), '-I'+str(ROOT/'src/game')]
        if args.sanitize:
            cmd += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
        cmd += list(map(str, sources))+['-lm', '-o', str(exe)]
    built = subprocess.run(cmd, cwd=out, env=env, text=True, capture_output=True)
    (out/'compile.log').write_text(built.stdout+built.stderr, encoding='utf-8')
    paths = ['src/game/g_tce_pose.c', 'src/game/g_tce_pose.h', 'src/game/g_tce_pose_presentation.c',
             'src/game/g_tce_pose_presentation.h', 'src/game/g_weapon.c', 'src/game/g_tce_bullet_damage.h',
             'src/game/tce_damage_trace.h', 'src/game/tce_damage.h', 'src/game/bg_character.c',
             'src/game/bg_tce_pose_clock.c', 'src/game/bg_tce_pose_clock.h',
             'src/game/bg_tce_pose_presentation.c', 'src/game/bg_tce_pose_presentation.h',
             'src/game/g_local.h', 'src/game/bg_public.h', 'src/game/q_shared.h',
             'tools/modernize/pose_ordinary_test.c', 'tools/modernize/pose_ordinary_regression.py',
             'tools/modernize/compat_validate.py', 'tools/modernize/weapon_regression.py']
    report = {'command': cmd, 'build_exit': built.returncode,
              'publisher_source': str(publisher), 'publisher_sha256': hashlib.sha256(publisher.read_bytes()).hexdigest(),
              'sources': {p: hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},
              'generated_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'limits': ['Actual reset/metadata, complete publisher/query and bullet selection bodies run.',
                         'Registration and engine services are controlled fixture inputs, not a live spawn capture.',
                         'Unexpected affected skin evaluation aborts; affected unavailable poses must remain excluded.']}
    if built.returncode:
        print(built.stdout+built.stderr)
    else:
        run = subprocess.run([str(exe)], cwd=out, env=env, text=True, capture_output=True, timeout=30)
        report.update(run_exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
        print(run.stdout+run.stderr, end='')
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8', newline='\n')
    return report.get('run_exit', built.returncode)


if __name__ == '__main__':
    raise SystemExit(main())
