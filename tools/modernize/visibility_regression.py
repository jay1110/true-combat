"""Compile actual player-visibility decisions against bounded glass/wall rays."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import os,re,shutil

from compat_validate import msvc_environment
from weapon_regression import function

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--source', type=Path, default=ROOT / 'src/cgame/cg_players.c')
    parser.add_argument('--arch', choices=('x86', 'x64'), default='x86')
    parser.add_argument('--sanitize', action='store_true')
    parser.add_argument('--compiler')
    parser.add_argument('--without-origin-scope',action='store_true',help='Expected failing pre-integration visibility/attachment call sequence')
    args = parser.parse_args()
    source = args.source.read_text(encoding='utf-8-sig')
    units = []
    for name in ('CG_TCETransparentGlass', 'CG_TCEVisibilityTrace', 'CG_CalcEntityVisibility'):
        if name + '(' in source:
            units.append(function(source, name))
    units.append(re.search(r'typedef struct\s*\{[^}]*\}\s*shadowPart_t;',source,re.S).group(0))
    for name in ('CG_PlayerFloatSprite','CG_PlayerShadow','CG_PlayerSplash'):units.append(function(source,name))
    presentation=(ROOT/'src/cgame/cg_tce_pose_presentation.c').read_text()
    for name in ('CG_TCEPresentationBeginOrigin','CG_TCEPresentationEndOrigin'):units.append(function(presentation,name))
    generated = (ROOT / 'tools/modernize/visibility_regression_fixture.c').read_text().replace(
        '/* PRODUCTION_FUNCTIONS */', '\n\n'.join(units))
    generated='#define TCE_TEST_ORIGIN_SCOPE '+str(int(not args.without_origin_scope))+'\n'+generated
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    path = out / 'fixture.c'; path.write_text(generated, encoding='utf-8', newline='\n')
    if os.name=='nt':
        env, compiler = msvc_environment(out, args.arch, '14.44' if args.sanitize else None)
        executable = out / 'fixture.exe'
        command = [compiler, '/nologo', '/TC', '/O2', '/Gy', '/DWIN32', '/D_CRT_SECURE_NO_WARNINGS',
                   '/I' + str(ROOT), str(path), str(ROOT / 'src/game/q_math.c'), '/Fe:' + str(executable)]
        if args.sanitize:command.insert(1, '/fsanitize=address')
        command += ['/link', '/OPT:REF']
    else:
        env=os.environ.copy();compiler=args.compiler or shutil.which('clang') or shutil.which('gcc')
        executable=out/'fixture'
        command=[compiler,'-std=c99','-O2','-ffunction-sections','-fdata-sections','-DCGAMEDLL',
                 '-I'+str(ROOT),str(path),str(ROOT/'src/game/q_math.c'),'-lm','-Wl,--gc-sections','-o',str(executable)]
        if args.sanitize:command[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer']
    built = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out / 'compile.log').write_text(built.stdout + built.stderr, encoding='utf-8')
    report = {'source': str(args.source), 'sha256': hashlib.sha256(source.encode()).hexdigest(),
              'arch': args.arch, 'asan': args.sanitize, 'command': command, 'build_returncode': built.returncode,
              'origin_scope':not args.without_origin_scope,
              'source_hashes':{name:hashlib.sha256((ROOT/name).read_bytes()).hexdigest() for name in
                  ('src/cgame/cg_players.c','src/cgame/cg_tce_pose_presentation.c','src/cgame/cg_tce_pose_presentation.h',
                   'src/game/q_math.c','tools/modernize/visibility_regression.py','tools/modernize/visibility_regression_fixture.c')},
              'limits': ['Analytic collision services, no renderer/BSP/live game',
                         'Full actual visibility decision functions extracted without branch rewriting']}
    if built.returncode:
        print(built.stdout + built.stderr)
    else:
        run = subprocess.run([str(executable)], cwd=out, env=env, capture_output=True, text=True, timeout=20)
        report.update(returncode=run.returncode, stdout=run.stdout, stderr=run.stderr)
        print(run.stdout + run.stderr, end='')
    (out / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8', newline='\n')
    return int(bool(built.returncode or report.get('returncode')))


if __name__ == '__main__':
    raise SystemExit(main())
