#!/usr/bin/env python3
"""Compile real effect/cover controllers against bounded collision fixtures.

No engine, map generation or live game is launched. --source-root accepts the
local pre-fix baseline to demonstrate regressions without restoring sources.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess

from compat_validate import msvc_environment
from weapon_regression import function

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, default=ROOT / 'build/effect-regression')
    parser.add_argument('--source-root', type=Path, default=ROOT)
    parser.add_argument('--arch', choices=['x86', 'x64'], default='x86')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    def source(relative):
        path = args.source_root / relative
        if not path.exists():
            path = args.source_root / Path(relative).name
        return path.read_text(encoding='utf-8-sig')
    sources = {name: source(name) for name in ('src/game/g_combat.c', 'src/game/g_weapon.c',
                'src/game/g_missile.c', 'src/game/tce_bullet_material.c', 'src/cgame/tce_shotgun_prediction.c', 'src/cgame/cg_marks.c')}
    units = []
    for name in ('CanDamage', 'G_AdjustedDamageVec', 'G_RadiusDamage', 'etpro_RadiusDamage'):
        units.append(function(sources['src/game/g_combat.c'], name))
    weapons = sources['src/game/g_weapon.c']
    units.append(function(weapons, 'SnapVectorTowards'))
    if 'static void G_TCETraceBullet(' in weapons:
        units += [re.search(r'typedef struct\s*\{\s*qboolean hit\[MAX_CLIENTS\];\s*\} tceBulletTraceContext_t;', weapons)[0],
                  function(weapons, 'G_TCETraceBullet'), function(weapons, 'G_TCEBulletFireExtended')]
    units.append(function(weapons, 'Bullet_Fire_Extended'))
    units += [function(sources['src/game/tce_bullet_material.c'], name) for name in ('TCE_SnapWallPoint', 'TCE_BulletWallExit')]
    units += [function(sources['src/cgame/tce_shotgun_prediction.c'], name) for name in ('SnapTowards', 'TCE_CG_BulletFireExtended')]
    units.append(function(sources['src/game/g_missile.c'], 'G_MissileImpact'))
    if 'static void G_TCEMissileTrace(' in sources['src/game/g_missile.c']:
        units.append(function(sources['src/game/g_missile.c'], 'G_TCEMissileTrace'))
    else:
        # Exact baseline trace-selection block from G_RunMissile.
        body = sources['src/game/g_missile.c']
        start = body.index('\ttrap_Trace(&tr, ent->r.currentOrigin', body.index('void G_RunMissile('))
        end = body.index('\n\tif (ent->s.weapon == 60', start)
        units.append('static void G_TCEMissileTrace(gentity_t *ent, const vec3_t origin, trace_t *result) { trace_t tr;\n'
                     + body[start:end] + '\n*result = tr; }')
    bg = (ROOT / 'src/game/tce_bg.c').read_text(encoding='utf-8')
    units.insert(0, function(bg, 'BG_SurfaceFlag2Type'))
    units.insert(0, re.search(r'const tce_pierceMaterial_t tcePierceTable\[37\] =\s*\{.*?\n\};', bg, re.S)[0])
    game = (ROOT / 'tools/modernize/effect_regression_game.c').read_text(encoding='utf-8').replace('/* PRODUCTION_FUNCTIONS */', '\n\n'.join(units))
    mark_units = [function(sources['src/cgame/cg_marks.c'], name) for name in ('CG_InitMarkPolys', 'CG_FreeMarkPoly', 'CG_AllocMark')]
    mark_units[1] = mark_units[1].replace('void CG_FreeMarkPoly(', 'static void freeMarkImpl(', 1)
    marks = (ROOT / 'tools/modernize/effect_regression_marks.c').read_text(encoding='utf-8').replace('/* PRODUCTION_FUNCTIONS */', '\n\n'.join(mark_units))
    env, compiler = msvc_environment(out, args.arch, '14.44' if args.sanitize else None)
    report = {'arch': args.arch, 'sanitizer': args.sanitize, 'source_root': str(args.source_root),
              'source_hashes': {name: hashlib.sha256(text.encode()).hexdigest() for name, text in sources.items()},
              'runs': [], 'limits': ['Synthetic collision/renderer services', 'No BSP, renderer FPS or network reproduction',
                                     'Actual production decision bodies extracted without rewriting', 'Mark pool guard prevents baseline sentinel-corruption hang']}
    for name, text in [('game', game), ('marks', marks)]:
        generated = out / (name + '.c')
        generated.write_text(text, encoding='utf-8', newline='\n')
        executable = out / (name + '.exe')
        command = [compiler, '/nologo', '/TC', '/O2', '/Gy', '/DWIN32', '/D_CRT_SECURE_NO_WARNINGS',
                   '/I' + str(ROOT), str(generated), '/Fe:' + str(executable)]
        if name == 'game':
            command += [str(ROOT / 'src/game/q_math.c')]
        if args.sanitize:
            command.insert(1, '/fsanitize=address')
        command += ['/link', '/OPT:REF']
        built = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
        (out / (name + '-compile.log')).write_text(built.stdout + built.stderr, encoding='utf-8')
        row = {'name': name, 'command': command, 'build_returncode': built.returncode}
        if built.returncode:
            print(built.stdout + built.stderr)
        else:
            run = subprocess.run([str(executable)], cwd=out, env=env, capture_output=True, text=True, timeout=30)
            row.update(returncode=run.returncode, stdout=run.stdout, stderr=run.stderr)
            print(run.stdout + run.stderr, end='')
        report['runs'].append(row)
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    return int(any(row['build_returncode'] or row.get('returncode') for row in report['runs']))


if __name__ == '__main__':
    raise SystemExit(main())
