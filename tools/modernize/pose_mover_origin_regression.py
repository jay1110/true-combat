#!/usr/bin/env python3
"""Finite actual-CGame defect reproducer for the affected-pose mover design."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess
from compat_validate import msvc_environment
from weapon_regression import function
ROOT=Path(__file__).resolve().parents[2]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--optimized',action='store_true')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--compiler',default='cc')
    parser.add_argument('--require-canonical',action='store_true')
    parser.add_argument('--verify-fixed',action='store_true')
    args=parser.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    groups={'src/game/q_math.c':['TCE_RotationTrigValue','AngleVectors','vectoangles','LerpAngle',
        'VectorInverse','VectorNormalize2','TCE_VectorLengthSquaredValue','TCE_VectorLengthValue'],
        'src/game/bg_misc.c':['BG_RaySphereIntersection','BG_PathFractionInSegment','BG_LinearPathOrigin2',
        'BG_CreateRotationMatrix','BG_TransposeMatrix','BG_RotatePoint'],
        'src/cgame/cg_ents.c':['CG_AddLinkedEntity','CG_AdjustPositionForMover','CG_EntityTrajectory',
        'CG_InterpolateEntityOrigin','CG_InterpolateEntityMotion','CG_InterpolateEntityPosition','CG_CalcEntityLerpPositions']}
    units=[];hashes={}
    for name,names in groups.items():
        data=(ROOT/name).read_bytes();hashes[name]=hashlib.sha256(data).hexdigest()
        units.extend(function(data.decode('utf-8'),f) for f in names)
    template=ROOT/'tools/modernize/pose_mover_origin_fixture.c';hashes[str(template.relative_to(ROOT))]=hashlib.sha256(template.read_bytes()).hexdigest()
    source=out/'generated.c';source.write_text(template.read_text(encoding='utf-8').replace('/* ACTUAL_FUNCTIONS */','\n\n'.join(units)),encoding='utf-8',newline='\n')
    push_source=out/'actual-push.c'
    push_template=ROOT/'tools/modernize/pose_mover_push_fixture.c'
    push_source.write_text(push_template.read_text().replace('/* ACTUAL_PUSH */',
        function((ROOT/'src/game/g_mover.c').read_text(),'G_TryPushingEntity')),encoding='utf-8',newline='\n')
    for name in ('src/game/g_mover.c','src/game/bg_tce_pose_origin.c','src/game/bg_tce_pose_origin.h',
                 'compat/compat_pose_origin.c','compat/compat_pose_origin.h','tools/modernize/pose_mover_push_fixture.c'):
        hashes[name]=hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
    names=['src/game/tce_trajectory.c','src/game/tce_trajectory_delta.c','src/game/tce_spline_calculate.c',
        'src/game/tce_spline_traverse.c','src/game/tce_spline_lookup.c']
    sources=[source,push_source,ROOT/'src/game/bg_tce_pose_origin.c',ROOT/'compat/compat_pose_origin.c']+[ROOT/n for n in names]
    for name in names:hashes[name]=hashlib.sha256((ROOT/name).read_bytes()).hexdigest()
    if os.name=='nt':
        env,compiler=msvc_environment(out,'x64' if args.sanitize else 'x86','14.44' if args.sanitize else '14.39')
        exe=out/'test.exe';command=[compiler,'/nologo','/TC','/W3','/fp:precise','/O2' if args.optimized else '/Od',
            '/DWIN32','/DCGAMEDLL','/D_CRT_SECURE_NO_WARNINGS','/I'+str(ROOT),'/I'+str(ROOT/'src/game')]
        if args.sanitize:command+=['/fsanitize=address','/Zi']
        command+=list(map(str,sources))+['/Fo:'+str(out)+os.sep,'/Fe:'+str(exe)]
    else:
        env=os.environ.copy();compiler=args.compiler;exe=out/'test'
        command=[compiler,'-std=c99','-O2' if args.optimized else '-O0','-ffp-contract=off','-DCGAMEDLL','-I'+str(ROOT),'-I'+str(ROOT/'src/game')]
        if args.sanitize:command+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
        command+=list(map(str,sources))+['-lm','-o',str(exe)]
    built=subprocess.run(command,cwd=out,env=env,capture_output=True,text=True)
    (out/'compile.log').write_text(built.stdout+built.stderr,encoding='utf-8')
    report={'sources':hashes,'command':command,'build_exit':built.returncode,
        'generated_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'require_canonical':args.require_canonical}
    if not built.returncode:
        run=subprocess.run([str(exe)]+(['--require-canonical'] if args.require_canonical else [])+
            (['--verify-fixed'] if args.verify_fixed else []),env=env,capture_output=True,text=True)
        report.update(run_exit=run.returncode,stdout=run.stdout,stderr=run.stderr)
        print(run.stdout+run.stderr,end='')
    else:print(built.stdout+built.stderr)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return report.get('run_exit',built.returncode)
if __name__=='__main__':raise SystemExit(main())
