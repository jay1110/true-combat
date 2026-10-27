#!/usr/bin/env python3
"""Compile current GT7 spawn selection and admission with a finite fixture."""
from pathlib import Path
import argparse
import hashlib
import json
import os
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
    parser.add_argument('--source-root', type=Path, default=ROOT)
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    paths = ['src/game/g_client.c', 'src/game/g_team.c']
    sources = {p: (args.source_root / p).read_text(encoding='utf-8-sig') for p in paths}
    client, team = (sources[p] for p in paths)
    units = [function(client, name) for name in ('SpotWouldTelefrag', 'G_SpawnOriginWorldClear')]
    units += [re.search(r'^#define MAX_TEAM_SPAWN_POINTS[^\n]+', team, re.M)[0]]
    units += [function(team, 'TCE_SpawnDistance')]
    if re.search(r'\bG_TCESelectCustomDMSpawn\([^;{}]*\)\s*\{', team):
        units.append(function(team, 'G_TCESelectCustomDMSpawn'))
    units.append(function(team, 'SelectRandomTeamSpawnPoint'))
    template = Path(__file__).with_name('spawn_distance_fixture.c')
    generated = out / 'spawn-distance.c'
    generated.write_text(template.read_text(encoding='utf-8').replace('/* ACTUAL_FUNCTIONS */', '\n\n'.join(units)), encoding='utf-8', newline='\n')
    if os.name == 'nt':
        env, compiler = msvc_environment(out, 'x64' if args.sanitize else 'x86', '14.44' if args.sanitize else '14.39')
        exe = out / 'spawn-distance.exe'
        command = [compiler, '/nologo', '/TC', '/W3', '/O2' if args.optimized else '/Od', '/DWIN32', '/D_CRT_SECURE_NO_WARNINGS', '/I'+str(ROOT), '/I'+str(ROOT/'src/game'), str(generated), '/Fe:'+str(exe), '/Fo:'+str(out/'fixture.obj')]
        if args.sanitize:
            command += ['/fsanitize=address', '/Zi', '/Fd:'+str(out/'fixture.pdb'), '/link', '/DEBUG']
    else:
        env = os.environ.copy(); compiler = 'cc'; exe = out/'spawn-distance'
        command = [compiler, '-std=c99', '-O2' if args.optimized else '-O0', '-DGAMEDLL', '-I'+str(ROOT), '-I'+str(ROOT/'src/game'), str(generated), '-o', str(exe), '-lm']
        if args.sanitize: command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
    built = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out/'compile.log').write_text(built.stdout+built.stderr, encoding='utf-8')
    sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    report = {'source_sha256': {p: sha(args.source_root/p) for p in paths}, 'header_sha256': sha(ROOT/'src/game/g_local.h'), 'template_sha256': sha(template), 'runner_sha256': sha(Path(__file__)), 'generated_sha256': sha(generated), 'command': command, 'build_exit': built.returncode, 'limits': ['Controlled engine collision/lookup/randomness, no real BSP, mapscript load or game runtime.', 'Strict nearest-enemy distance priority intentionally differs from CQB pressure-only scoring.']}
    if built.returncode: print(built.stdout+built.stderr)
    else:
        run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True)
        report.update(run_exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
        print(run.stdout+run.stderr, end='')
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8', newline='\n')
    return report.get('run_exit', built.returncode)


if __name__ == '__main__':
    raise SystemExit(main())
