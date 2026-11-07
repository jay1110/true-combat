"""Compile the real grid and viewport bodies against bounded draw traps."""
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
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    paths = ['src/cgame/cg_commandmap.c', 'src/cgame/cg_drawtools.c',
             'src/cgame/cg_hud_viewport.c', 'src/ui/tce_viewport.c', 'src/ui/tce_ui_coordinates.c']
    sources = {p: (ROOT/p).read_text(encoding='utf-8-sig') for p in paths}
    grid = sources[paths[0]]
    units = [function(sources[paths[1]], n) for n in ['CG_TCAdjustFrom640', 'CG_AdjustFrom640']]
    units += [function(grid, n) for n in ['CG_DrawGridLine', 'CG_DrawGrid']]
    assert function(grid, 'CG_DrawGrid').count('CG_DrawGridLine(') == 4
    assert 'cgs.screenXScale' not in function(grid, 'CG_DrawGrid')
    template = Path(__file__).with_name('commandmap_grid_fixture.c')
    code = template.read_text().replace('/* ACTUAL_HUD_CONTEXT */',
            re.sub(r'^#include[^\n]*\n', '', sources[paths[2]], flags=re.M))
    code = code.replace('/* ACTUAL_FUNCTIONS */', '\n\n'.join(units))
    generated = out/'grid.c'; generated.write_text(code, encoding='utf-8', newline='\n')
    if os.name != 'nt':
        raise RuntimeError('This receipt targets Windows native numeric branches; use the Linux compatibility bundle for Linux branches')
    env, compiler = msvc_environment(out, 'x64' if args.sanitize else 'x86', '14.44')
    exe = out/'grid.exe'
    command = [compiler, '/nologo', '/TC', '/W3', '/O2' if args.optimized else '/Od',
               '/D_CRT_SECURE_NO_WARNINGS', '/I'+str(ROOT), str(generated),
               str(ROOT/paths[3]), str(ROOT/paths[4]), '/Fe:'+str(exe), '/Fo:'+str(out)+os.sep]
    if args.sanitize:
        command += ['/fsanitize=address', '/Zi', '/Fd:'+str(out/'fixture.pdb'), '/link', '/DEBUG']
    result = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out/'compile.log').write_text(result.stdout+result.stderr, encoding='utf-8')
    report = {'source_sha256': {p: hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},
              'template_sha256': hashlib.sha256(template.read_bytes()).hexdigest(),
              'command': command, 'build_exit': result.returncode,
              'limits': ['Actual Windows grid and actual HUD/menu/legacy TC/SDK projection bodies; controlled font/trap services.',
                         'No real renderer, native OS input, engine clipping, Linux numeric branch or live map is simulated.']}
    if result.returncode:
        print(result.stdout+result.stderr)
    else:
        run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True, timeout=30)
        report.update(run_exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
        print(run.stdout+run.stderr, end='')
    (out/'report.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf-8', newline='\n')
    return report.get('run_exit', result.returncode)


if __name__ == '__main__':
    raise SystemExit(main())
