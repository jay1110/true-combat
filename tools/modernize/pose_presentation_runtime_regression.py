#!/usr/bin/env python3
"""Run the complete publisher and CGame consumer with actual SDK converters."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
from compat_validate import msvc_environment
from weapon_regression import function
ROOT=Path(__file__).resolve().parents[2]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--optimized',action='store_true');p.add_argument('--sanitize',action='store_true');p.add_argument('--compiler')
    p.add_argument('--cgame-source',type=Path,help='Explicit frozen comparison source; default is current production')
    a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    bg=(ROOT/'src/game/bg_misc.c').read_text()
    macro=bg[bg.index('#define SETUP_MOUNTEDGUN_STATUS'):bg.index('/*',bg.index('#define SETUP_MOUNTEDGUN_STATUS'))]
    converters=macro+'\n'+'\n'.join(function(bg,n) for n in
        ('BG_TCEPlayerStateFields','BG_SnapshotSequenceAdd','BG_SnapshotVector',
         'BG_PlayerStateToEntityState','BG_PlayerStateToEntityStateExtraPolate'))
    selectors=function((ROOT/'src/game/bg_character.c').read_text(),'BG_GetCharacter')+'\n'+function(
        (ROOT/'src/cgame/cg_character.c').read_text(),'CG_CharacterForClientinfo')
    entities=(ROOT/'src/cgame/cg_ents.c').read_text()
    positions='\n'.join(function(entities,n) for n in
        ('CG_InterpolateEntityOrigin','CG_InterpolateEntityMotion','CG_InterpolateEntityPosition','CG_CalcEntityLerpPositions'))
    positions=function((ROOT/'src/game/q_math.c').read_text(),'LerpAngle')+'\n'+positions
    math_source=(ROOT/'src/game/q_math.c').read_text()
    mover_math='\n'.join(function(math_source,n) for n in
        ('TCE_RotationTrigValue','AngleVectors','vectoangles','VectorInverse','VectorNormalize2',
         'TCE_VectorLengthSquaredValue','TCE_VectorLengthValue'))
    mover_math+='\n'+'\n'.join(function(bg,n) for n in
        ('BG_RaySphereIntersection','BG_PathFractionInSegment','BG_LinearPathOrigin2',
         'BG_CreateRotationMatrix','BG_TransposeMatrix','BG_RotatePoint'))
    math_unit=out/'actual-origin-math.c'
    math_unit.write_text('#include "q_shared.h"\n#include "bg_public.h"\n#include "tce_numeric.h"\n#include "tce_rotate_precision.h"\n'
        'vec3_t vec3_origin;int numSplinePaths;splinePath_t splinePaths[MAX_SPLINE_PATHS];\n'
        'static volatile float tceSinRoll,tceCosRoll,tceSinPitch,tceCosYaw,tceCosPitch,tceSinYaw;\n'+mover_math,
        encoding='utf-8',newline='\n')
    report={'programs':{},'limits':['Complete production publisher/consumer; engine services adapted',
        'Authoritative own-body tuple plus remote tuple compared across separate native executables',
        'No game/network/runtime phase claim']}
    if os.name=='nt':
        env,compiler=msvc_environment(out,'x64' if a.sanitize else 'x86','14.44' if a.sanitize else '14.39')
    else:env=os.environ.copy();compiler=a.compiler or 'cc'
    for mode in ('server','cgame'):
        folder=out/mode;folder.mkdir(exist_ok=True)
        fixture=(ROOT/f'tools/modernize/pose_presentation_{mode}_test.c').read_text()
        fixture=fixture.replace('/* ACTUAL_PLAYER_CONVERTERS */',converters).replace('/* ACTUAL_CHARACTER_SELECTORS */',selectors)
        fixture=fixture.replace('/* ACTUAL_BG_GET_CHARACTER */',function((ROOT/'src/game/bg_character.c').read_text(),'BG_GetCharacter'))
        fixture=fixture.replace('/* ACTUAL_POSITION_FUNCTIONS */',positions)
        fixture=fixture.replace('/* ACTUAL_ENTITY_TRAJECTORY */',function(entities,'CG_EntityTrajectory'))
        production=(ROOT/'src/game/g_tce_pose_presentation.c' if mode=='server' else
                    a.cgame_source or ROOT/'src/cgame/cg_tce_pose_presentation.c')
        actual=production.read_text().replace('#include "g_local.h"','').replace('#include "cg_local.h"','')
        fixture=fixture.replace('/* ACTUAL_PRESENTATION */',actual)
        source=folder/'generated.c';source.write_text(fixture,encoding='utf-8',newline='\n')
        sources=[source,ROOT/'src/game/bg_tce_pose_presentation.c',ROOT/'src/game/bg_tce_pose_origin.c',
                 ROOT/'src/game/bg_tce_pose_clock.c',ROOT/'compat/compat_pose_origin.c',math_unit,ROOT/'src/game/tce_spline_calculate.c',
                 ROOT/'src/game/tce_spline_traverse.c',ROOT/'src/game/tce_spline_lookup.c',
                 ROOT/'src/game/tce_trajectory.c',ROOT/'src/game/tce_trajectory_delta.c']
        define='CGAMEDLL' if mode=='cgame' else 'GAMEDLL'
        if os.name=='nt':
            exe=folder/'test.exe'
            command=[compiler,'/nologo','/TC','/W4','/WX','/fp:precise','/O2' if a.optimized else '/Od',
                '/DWIN32','/D'+define,'/D_CRT_SECURE_NO_WARNINGS','/I'+str(ROOT/'src/cgame'),'/I'+str(ROOT/'src/game'),'/I'+str(ROOT)]
            if a.sanitize:command+=['/fsanitize=address','/Zi']
            command+=list(map(str,sources))+['/Fo:'+str(folder)+os.sep,'/Fe:'+str(exe)]
        else:
            exe=folder/'test'
            command=[compiler,'-std=c99','-Wall','-Wextra','-Werror','-ffp-contract=off','-O2' if a.optimized else '-O0',
                '-D'+define,'-I'+str(ROOT/'src/cgame'),'-I'+str(ROOT/'src/game'),'-I'+str(ROOT)]
            if a.sanitize:command+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
            command+=list(map(str,sources))+['-lm','-o',str(exe)]
        # Existing trajectory/math/spline sources retain classified legacy
        # local-variable and fixed-grid shift warnings. Compile those unchanged
        # TUs/extracted bodies with warnings logged;
        # the new modules/fixtures remain strict W4/WX or Wall/Wextra/Werror.
        legacy=[]
        for index,old in enumerate(sources[5:]):
            obj=folder/('trajectory%d.obj'%index if os.name=='nt' else 'trajectory%d.o'%index)
            prefix=command[:command.index(str(source))]
            prefix=[x for x in prefix if x not in ('/WX','-Werror')]
            oldcommand=prefix+(['/c',str(old),'/Fo:'+str(obj)] if os.name=='nt' else ['-c',str(old),'-o',str(obj)])
            oldbuild=subprocess.run(oldcommand,cwd=folder,env=env,text=True,capture_output=True)
            (folder/f'legacy-{index}.log').write_text(oldbuild.stdout+oldbuild.stderr,encoding='utf-8')
            legacy.append(dict(command=oldcommand,build_exit=oldbuild.returncode))
            if oldbuild.returncode:raise RuntimeError(oldbuild.stdout+oldbuild.stderr)
            command=[str(obj) if x==str(old) else x for x in command]
        if os.name=='nt':command=[x for x in command if x!='/TC'] # .c sources plus precompiled .obj inputs
        built=subprocess.run(command,cwd=folder,env=env,text=True,capture_output=True)
        (folder/'compile.log').write_text(built.stdout+built.stderr,encoding='utf-8')
        row=report['programs'][mode]={'command':command,'legacy_translation_units':legacy,'build_exit':built.returncode,
            'production_source':str(production),'production_sha256':hashlib.sha256(production.read_bytes()).hexdigest()}
        if not built.returncode:
            run=subprocess.run([str(exe),str(out/'tuple.bin')],cwd=folder,env=env,text=True,capture_output=True,timeout=30)
            row.update(run_exit=run.returncode,stdout=run.stdout,stderr=run.stderr);print(mode+': '+run.stdout+run.stderr,end='')
        else:print(built.stdout+built.stderr)
        if row.get('run_exit',built.returncode):break
    names=['src/game/g_tce_pose_presentation.c','src/game/g_tce_pose_presentation.h',
        'src/cgame/cg_tce_pose_presentation.c','src/cgame/cg_tce_pose_presentation.h',
        'src/game/bg_tce_pose_presentation.c','src/game/bg_tce_pose_presentation.h',
        'src/game/bg_tce_pose_origin.c','src/game/bg_tce_pose_origin.h',
        'compat/compat_pose_origin.c','compat/compat_pose_origin.h',
        'src/game/tce_spline_calculate.c','src/game/tce_spline_traverse.c','src/game/tce_spline_lookup.c',
        'src/game/bg_tce_pose_clock.c','src/game/bg_tce_pose_clock.h',
        'src/game/bg_misc.c','src/game/q_math.c','src/cgame/cg_character.c','src/game/bg_character.c',
        'src/game/tce_trajectory.c','src/game/tce_trajectory_delta.c','src/cgame/cg_ents.c',
        'tools/modernize/pose_presentation_trajectory_services.h',
        'tools/modernize/pose_presentation_runtime_cases.h','tools/modernize/pose_presentation_server_test.c',
        'tools/modernize/pose_presentation_cgame_test.c','tools/modernize/pose_presentation_runtime_regression.py']
    report['sources']={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in names}
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
    return row.get('run_exit',row['build_exit'])
if __name__=='__main__':raise SystemExit(main())
