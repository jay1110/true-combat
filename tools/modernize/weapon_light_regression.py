#!/usr/bin/env python3
"""Exercise actual weapon flash/light emission with bounded renderer services."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

from compat_validate import msvc_environment
from weapon_regression import function

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--optimized', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', choices=('auto', 'gcc', 'clang'), default='auto')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    source_path = ROOT / 'src/cgame/cg_weapons.c'
    source = source_path.read_text(encoding='utf-8-sig')
    actual = function(source, 'CG_AddPlayerWeapon')
    start = actual.index('\t// weapons that don\'t need to go any further')
    end = actual.index('\n\t// Ridah, zombie fires from his head', start)
    tail = actual[start:end]
    call = 'CG_AddWeaponMuzzleFlash(&flash, weapon, cent, ps != NULL, weaponNum);'
    if tail.count(call) != 1 or actual.count(call) != 1:
        raise RuntimeError('Tested flash helper is not uniquely connected to actual weapon draw')
    if actual.index('CG_PositionRotatedEntityOnTag(&flash,') > start:
        raise RuntimeError('Flash/light no longer uses the actual positioned muzzle tag')
    fire = function(source, 'CG_FireWeapon')
    if 'cent->muzzleFlashTime = cg.time;' not in fire:
        raise RuntimeError('Flash event timestamp is no longer assigned by actual fire event')
    units = [function(source, n) for n in ('CG_AddWeaponMuzzleLight', 'CG_AddWeaponMuzzleFlash')]
    units.append('static void ActualWeaponFlashTail(refEntity_t flash, const weaponInfo_t *weapon, '
                 'const centity_t *cent, playerState_t *ps, int weaponNum) {\n' + tail + '\n}')
    template = Path(__file__).with_name('weapon_light_fixture.c')
    generated = out / 'weapon_light.c'
    generated.write_text(template.read_text(encoding='utf-8').replace('/* ACTUAL_FUNCTIONS */', '\n\n'.join(units)),
                         encoding='utf-8', newline='\n')
    if os.name == 'nt':
        env, compiler = msvc_environment(out, 'x86', '14.44' if args.sanitize else '14.39')
        exe = out / 'weapon_light.exe'
        command = [compiler, '/nologo', '/TC', '/W3', '/O2' if args.optimized else '/Od',
                   '/D_CRT_SECURE_NO_WARNINGS', '/I'+str(ROOT), str(generated),
                   '/Fo:'+str(out)+os.sep, '/Fe:'+str(exe)]
        if args.sanitize:
            command += ['/fsanitize=address', '/Zi', '/Fd:'+str(out/'fixture.pdb'), '/link', '/DEBUG']
    else:
        env = os.environ.copy()
        compiler = shutil.which(args.compiler) if args.compiler != 'auto' else (shutil.which('clang') or shutil.which('gcc'))
        if not compiler:
            raise RuntimeError('No native C compiler available')
        exe = out / 'weapon_light'
        command = [compiler, '-std=c99', '-O2' if args.optimized else '-O0', '-I'+str(ROOT),
                   str(generated), '-o', str(exe), '-lm']
        if args.sanitize:
            command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
    built = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out/'compile.log').write_text(built.stdout+built.stderr, encoding='utf-8')
    paths = ['src/cgame/cg_weapons.c', 'src/cgame/cg_local.h', 'src/cgame/cg_main.c',
             'src/cgame/tce_weapon_flash_color.h', 'src/game/tce_numeric.h',
             'pak/custom/default/weapons/mp5.weap', 'pak/custom/default/weapons/m4.weap',
             'pak/custom/default/weapons/m4sopmod.weap']
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    report = {'source_sha256': {p: digest(ROOT/p) for p in paths},
              'template_sha256': digest(template), 'generated_sha256': digest(generated),
              'runner_sha256': digest(Path(__file__)), 'command': command,
              'build_exit': built.returncode,
              'limits': ['Actual flash/light helpers and actual no-flash weapon exclusion tail; renderer calls recorded, no GPU or map.',
                         'Actual fire timestamp assignment and draw hook checked; complete weapon animation/model attachment not executed.',
                         'No renderer wall occlusion, visual brightness, FPS or live gameplay acceptance claim.']}
    if built.returncode:
        print(built.stdout+built.stderr)
    else:
        run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True, timeout=20)
        report.update(run_exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
        print(run.stdout+run.stderr, end='')
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8', newline='\n')
    return report.get('run_exit', built.returncode)


if __name__ == '__main__':
    raise SystemExit(main())
