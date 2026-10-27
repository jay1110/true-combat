#!/usr/bin/env python3
"""Compile/run the isolated full-surface fixture; never builds a game module."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/modernize'))
from compat_validate import msvc_environment

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--fixtures',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--optimized',action='store_true')
    parser.add_argument('--sanitize',action='store_true')
    parser.add_argument('--compiler',help='Native Linux GCC/Clang executable')
    args=parser.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    fixtures=args.fixtures.resolve();exe=out/('pose_skin_test.exe' if os.name=='nt' else 'pose_skin_test')
    if os.name=='nt':
        env,compiler=msvc_environment(out,'x64' if args.sanitize else 'x86','14.44' if args.sanitize else '14.39')
        command=[compiler,'/nologo','/TC','/W4','/WX','/fp:precise','/O2' if args.optimized else '/Od','/D_CRT_SECURE_NO_WARNINGS',
                 '/I'+str(ROOT/'src/game'),'/I'+str(fixtures),str(ROOT/'src/game/tce_pose_math.c'),str(ROOT/'tools/modernize/pose_skin_test.c'),
                 '/Fe:'+str(exe),'/Fo:'+str(out)+os.sep]
        if args.sanitize:command[1:1]=['/fsanitize=address','/Zi','/Fd:'+str(out/'fixture.pdb')]
    else:
        env=os.environ.copy();compiler=args.compiler or 'gcc'
        command=[compiler,'-std=c99','-Wall','-Wextra','-Werror','-ffp-contract=off','-O2' if args.optimized else '-O0',
                 '-I'+str(ROOT/'src/game'),'-I'+str(fixtures),str(ROOT/'src/game/tce_pose_math.c'),str(ROOT/'tools/modernize/pose_skin_test.c'),'-lm','-o',str(exe)]
        if args.sanitize:command[1:1]=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
    built=subprocess.run(command,cwd=out,env=env,capture_output=True,text=True)
    (out/'compile.log').write_text(built.stdout+built.stderr,encoding='utf-8')
    report={'command':command,'build_exit_code':built.returncode,
            'source_sha256':{str(path.relative_to(ROOT)):hashlib.sha256(path.read_bytes()).hexdigest() for path in
                             (ROOT/'src/game/tce_pose_math.c',ROOT/'src/game/tce_pose_math.h',ROOT/'tools/modernize/pose_skin_test.c')},
            'fixtures_manifest_sha256':hashlib.sha256((fixtures/'manifest.json').read_bytes()).hexdigest(),
            'limits':['Pure full-surface helper only; no active damage/provider or server phase proof.',
                      'Body/leg classification follows explicit surface-prefix/weighted thigh ancestry; no legacy damage-weight parity.']}
    if not built.returncode:
        run=subprocess.run([str(exe),str(fixtures)],cwd=out,env=env,capture_output=True,text=True)
        report.update(exit_code=run.returncode,stdout=run.stdout,stderr=run.stderr)
        print(run.stdout+run.stderr,end='')
    else:print((built.stdout+built.stderr)[-6000:])
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return report.get('exit_code',built.returncode)

if __name__=='__main__':raise SystemExit(main())
