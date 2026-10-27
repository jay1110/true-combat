#!/usr/bin/env python3
"""Run complete server pose source with real file and allocation adapters."""
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
    parser.add_argument('--fixtures', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--optimized', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler', help='Native GCC/Clang compiler on non-Windows hosts')
    parser.add_argument('--full-assets', type=Path, help='Directory with the ten original .mdx clips and body.mdm')
    parser.add_argument('--adapter-fixtures',type=Path,help='Generated actual character-family/ray assets')
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    data = out/'fixture-files'; data.mkdir(exist_ok=True)
    for name in ('model-0.mdx', 'model-1.mdx', 'mesh-0.mdm'):
        shutil.copyfile(args.fixtures/name, data/name)
    (data/'bad.mdx').write_bytes(bytes(256))
    shutil.copyfile(data/'model-0.mdx', data/'unloaded.mdx')
    if args.full_assets:
        models=data/'animations/human/base'; models.mkdir(parents=True, exist_ok=True)
        mesh=data/'models/players/temperate/allied/engineer'; mesh.mkdir(parents=True, exist_ok=True)
        for path in args.full_assets.glob('*.mdx'): shutil.copyfile(path, models/path.name)
        shutil.copyfile(args.full_assets/'body.mdm', mesh/'body.mdm')
    if args.adapter_fixtures:
        if not args.full_assets:parser.error('--adapter-fixtures requires --full-assets')
        manifest=json.loads((args.adapter_fixtures/'manifest.json').read_text())
        for name,row in manifest['files'].items():
            asset=args.adapter_fixtures/name
            if hashlib.sha256(asset.read_bytes()).hexdigest()!=row['sha256']:raise ValueError('Changed adapter fixture '+name)
            if name.startswith('assets/'):
                destination=data/Path(name).relative_to('assets');destination.parent.mkdir(parents=True,exist_ok=True)
                shutil.copyfile(asset,destination)
        for name in ('adapter-head-copy.md3','adapter-head-overcap.md3'):
            shutil.copyfile(data/'models/players/hud/head.md3',data/name)
    fixture = (ROOT/'tools/modernize/pose_server_test.c').read_text()
    actual = function((ROOT/'src/game/bg_animation.c').read_text(), 'BG_PlayAnim')
    character_lookup=function((ROOT/'src/game/bg_character.c').read_text(),'BG_GetCharacter')
    source = out/'pose_server_generated.c'
    source.write_text(fixture.replace('/* ACTUAL_BG_PLAYANIM */', actual).replace('/* ACTUAL_BG_GETCHARACTER */',character_lookup), encoding='utf-8', newline='\n')
    sources = [source, ROOT/'src/game/tce_pose_math.c', ROOT/'src/game/bg_tce_pose_clock.c',
               ROOT/'src/game/bg_tce_pose_presentation.c']
    if os.name == 'nt':
        env, compiler = msvc_environment(out, 'x64' if args.sanitize else 'x86', '14.44' if args.sanitize else '14.39')
        exe = out/'pose_server_test.exe'
        cmd = [compiler, '/nologo', '/TC', '/W4', '/WX', '/fp:precise', '/O2' if args.optimized else '/Od',
               '/I'+str(ROOT/'src/game'), '/I'+str(ROOT), '/DWIN32', '/DGAMEDLL', '/D_CRT_SECURE_NO_WARNINGS']
        if args.sanitize: cmd += ['/fsanitize=address', '/Zi']
        if args.adapter_fixtures:cmd += ['/DG_TCE_POSE_ADAPTER_FIXTURES','/I'+str(args.adapter_fixtures.resolve())]
        cmd += [str(s) for s in sources]+['/Fo:'+str(out)+os.sep, '/Fe:'+str(exe)]
    else:
        env = os.environ.copy(); compiler = args.compiler or 'cc'; exe = out/'pose_server_test'
        cmd = [compiler, '-std=c99', '-Wall', '-Wextra', '-Werror', '-ffp-contract=off', '-DGAMEDLL',
               '-O2' if args.optimized else '-O0', '-I'+str(ROOT/'src/game'), '-I'+str(ROOT)]
        if args.sanitize: cmd += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
        if args.adapter_fixtures:cmd += ['-DG_TCE_POSE_ADAPTER_FIXTURES','-I'+str(args.adapter_fixtures.resolve())]
        cmd += [str(s) for s in sources]+['-lm', '-o', str(exe)]
    built = subprocess.run(cmd, cwd=out, env=env, capture_output=True, text=True)
    (out/'compile.log').write_text(built.stdout+built.stderr, encoding='utf-8')
    tracked = ['src/game/g_tce_pose.c', 'src/game/g_tce_pose.h', 'src/game/tce_pose_math.c',
               'src/game/bg_tce_pose_clock.c', 'src/game/bg_tce_pose_clock.h', 'src/game/bg_animation.c',
               'src/game/bg_tce_pose_presentation.c','src/game/bg_tce_pose_presentation.h',
               'src/game/g_tce_pose_presentation.h',
               'src/game/bg_character.c',
               'tools/modernize/pose_server_test.c', 'tools/modernize/pose_server_regression.py']
    report = {'command':cmd, 'build_exit':built.returncode,
              'sources':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in tracked},
              'limits':['Engine filesystem services are adapted to native stdio; this is not a live engine/map result.',
                        'Complete actual server module and BG_PlayAnim functions are executed; no damage provider is enabled.']}
    if args.adapter_fixtures:report['adapter_manifest_sha256']=hashlib.sha256((args.adapter_fixtures/'manifest.json').read_bytes()).hexdigest()
    if not built.returncode:
        run = subprocess.run([str(exe), str(data)]+(['full'] if args.full_assets else []), cwd=out, env=env, capture_output=True, text=True)
        report.update(run_exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
        if not run.returncode: report['metrics'] = json.loads(run.stdout)
        layout = subprocess.run([str(exe), '--layout'], cwd=out, env=env, capture_output=True, text=True)
        if not layout.returncode:
            report['layout'] = json.loads(layout.stdout)
            (out/'layout.json').write_text(json.dumps(report['layout'], indent=2)+'\n', encoding='utf-8')
        print(run.stdout+run.stderr, end='')
    else: print(built.stdout+built.stderr)
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8')
    return report.get('run_exit', built.returncode)

if __name__ == '__main__':
    raise SystemExit(main())
