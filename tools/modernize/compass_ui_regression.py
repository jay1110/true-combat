#!/usr/bin/env python3
"""Check real compass projections/commands/draw bodies with controlled services."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
from compat_validate import msvc_environment
from weapon_regression import function

ROOT = Path(__file__).resolve().parents[2]
FUNCTIONS = {
    'src/cgame/cg_commandmap.c': ('CG_ExpandedMapSize', 'CG_TransformCommandMapEntities', 'CG_CompassRange', 'CG_CommandMapShader', 'CG_CurLayerForZ', 'CG_TransformAutomapEntity', 'CG_AdjustAutomapZoom'),
    'src/cgame/cg_consolecmds.c': ('CG_AutomapZoomIn_f', 'CG_AutomapZoomOut_f', 'CG_AutomapExpandDown_f', 'CG_AutomapExpandUp_f', 'CG_CommandMap_f'),
    'src/cgame/cg_drawtools.c': ('CG_TCAdjustFrom640', 'CG_AdjustFrom640'),
    'src/cgame/cg_draw.c': ('CG_CompassRadius', 'CG_CompassMapShader', 'CG_DrawCompassMap', 'CG_DrawCompassCardinals', 'CG_DrawCompassBackdrop', 'CG_RegisterTCENavigationMedia', 'CG_DrawTCENavigationPlayer', 'CG_TCEFlagMapShader', 'CG_TCEFlagItemHasHomeMarker', 'CG_DrawCompassContact', 'CG_DrawObjectiveLetter', 'CG_DrawTCECommandMapObjectives', 'CG_DrawTCECompassObjective'),
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out', type=Path, required=True)
    parser.add_argument('--optimized', action='store_true')
    parser.add_argument('--sanitize', action='store_true')
    args = parser.parse_args()
    out = args.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    template = Path(__file__).with_name('compass_ui_fixture.c')
    command_map = (ROOT / 'src/cgame/cg_commandmap.c').read_text(encoding='utf-8-sig')
    map_draw = function(command_map, 'CG_DrawMap')
    player_draw = function(command_map, 'CG_DrawMapEntity')
    draw_source = (ROOT / 'src/cgame/cg_draw.c').read_text(encoding='utf-8-sig')
    radar_draw = function(draw_source, 'CG_DrawRadar')
    frame_draw = function(draw_source, 'CG_Draw2D')
    registration = function((ROOT / 'src/cgame/cg_main.c').read_text(encoding='utf-8-sig'), 'CG_RegisterGraphics')
    if 'CG_DrawSpawnPointInfo(' in map_draw:
        raise RuntimeError('TCE map still draws unrelated ET reinforcement flags')
    if 'classInfo->icon' in player_draw or 'classInfo->arrow' in player_draw or 'cgs.media.medicIcon' in player_draw:
        raise RuntimeError('TCE map still draws legacy ET class/revive artwork')
    if player_draw.count('CG_DrawTCENavigationPlayer(') != 2 or 'CG_DrawTCECompassObjective(y, cent, kind, shader, above, below);' not in radar_draw:
        raise RuntimeError('Custom navigation renderer is not connected to real map/radar frame paths')
    if 'CG_RegisterTCENavigationMedia();' not in registration:
        raise RuntimeError('Navigation TGAs are not registered during real CGame media initialization')
    if 'CG_DrawCompassBackdrop(y, lastangle);' not in radar_draw:
        raise RuntimeError('Real radar does not call the tested compass backdrop')
    if 'if (cg_drawCompass.integer)' not in frame_draw or 'CG_DrawRadar();' not in frame_draw.split('if (cg_drawCompass.integer)', 1)[1].split('}', 1)[0]:
        raise RuntimeError('Compass Off is not guarded by the actual HUD frame dispatch')
    units = [function((ROOT / p).read_text(encoding='utf-8-sig'), n) for p, names in FUNCTIONS.items() for n in names]
    generated = out / 'compass.c'
    generated.write_text(template.read_text(encoding='utf-8').replace('/* ACTUAL_FUNCTIONS */', '\n\n'.join(units)), encoding='utf-8', newline='\n')
    production = ROOT / 'src/cgame/tce_compass_map.c'
    if os.name == 'nt':
        env, compiler = msvc_environment(out, 'x64' if args.sanitize else 'x86', '14.44')
        exe = out / 'compass.exe'
        command = [compiler, '/nologo', '/TC', '/W3', '/O2' if args.optimized else '/Od', '/D_CRT_SECURE_NO_WARNINGS', '/I'+str(ROOT), str(generated), str(production), '/Fe:'+str(exe), '/Fo:'+str(out)+os.sep]
        if args.sanitize: command += ['/fsanitize=address', '/Zi', '/Fd:'+str(out/'fixture.pdb'), '/link', '/DEBUG']
    else:
        env = os.environ.copy(); compiler = shutil.which('clang') or shutil.which('gcc')
        if not compiler: raise RuntimeError('No native compiler')
        exe = out/'compass'
        command = [compiler, '-std=c99', '-O2' if args.optimized else '-O0', '-I'+str(ROOT), str(generated), str(production), '-o', str(exe), '-lm']
        if args.sanitize: command += ['-fsanitize=address,undefined', '-fno-omit-frame-pointer', '-g']
    built = subprocess.run(command, cwd=out, env=env, capture_output=True, text=True)
    (out/'compile.log').write_text(built.stdout+built.stderr, encoding='utf-8')
    digest = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
    report = {'source_sha256': {p: digest(ROOT/p) for p in (*FUNCTIONS, 'src/cgame/cg_main.c')}, 'helper_sha256': digest(production), 'template_sha256': digest(template), 'command': command, 'build_exit': built.returncode,
              'limits': ['Controlled draw/filesystem/font services; no real renderer, engine input, server packets or live game.', 'Exact production projection/zoom/command/marker/backdrop/native TC-coordinate bodies are compiled. The new HUD viewport callback is disabled in this fixture; its own suite covers that adapter. Frame calls and the Off guard are source-checked.']}
    if built.returncode: print(built.stdout+built.stderr)
    else:
        run = subprocess.run([str(exe)], cwd=out, env=env, capture_output=True, text=True, timeout=30)
        report.update(run_exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
        print(run.stdout+run.stderr,end='')
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
    return report.get('run_exit',built.returncode)


if __name__ == '__main__':
    raise SystemExit(main())
