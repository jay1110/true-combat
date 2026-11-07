"""Compile complete production steering with deterministic engine observations."""
import argparse,hashlib,json,os,re,shutil,subprocess
from pathlib import Path
from compat_validate import msvc_environment
ROOT=Path(__file__).resolve().parents[2]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True);p.add_argument('--optimized',action='store_true');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    source=ROOT/'vendor/omni-bot/source/Omnibot/Common/BotSteeringSystem.cpp';header=source.with_suffix('.h');fixture=Path(__file__).with_name('steering_avoidance_fixture.cpp')
    actual=re.sub(r'^#include[^\n]*\n','',source.read_text(),flags=re.M)
    generated=out/'steering.cpp';generated.write_text(fixture.read_text().replace('/* ACTUAL_STEERING */',actual),newline='\n')
    if os.name=='nt':
        env,compiler=msvc_environment(out,'x64' if a.sanitize else 'x86','14.44');exe=out/'steering.exe';cmd=[compiler,'/nologo','/EHsc','/W3','/O2' if a.optimized else '/Od','/I'+str(ROOT),str(generated),'/Fe:'+str(exe),'/Fo:'+str(out)+os.sep]
        if a.sanitize:cmd+=['/fsanitize=address','/Zi','/link','/DEBUG']
    else:
        env=os.environ.copy();exe=out/'steering';cmd=[shutil.which('clang++') or shutil.which('g++'),'-std=c++11','-O2' if a.optimized else '-O0','-I'+str(ROOT),str(generated),'-o',str(exe)]
        if a.sanitize:cmd+=['-fsanitize=address,undefined','-g']
    built=subprocess.run(cmd,cwd=out,env=env,capture_output=True,text=True);(out/'compile.log').write_text(built.stdout+built.stderr)
    r={'source_sha256':{str(f.relative_to(ROOT)):hashlib.sha256(f.read_bytes()).hexdigest() for f in (source,header,fixture,Path(__file__))},'command':cmd,'build_exit':built.returncode,'limits':['Actual complete SteeringSystem source/header; controlled entity/player/nav/trace services, not engine movement physics.','CanWalkTo and body sweep rejection are separate observations; production Client retains existing full-body ground/water/slope validation.']}
    if built.returncode:print(built.stdout+built.stderr)
    else:
        run=subprocess.run([str(exe)],cwd=out,env=env,capture_output=True,text=True,timeout=20);r.update(run_exit=run.returncode,stdout=run.stdout,stderr=run.stderr);print(run.stdout+run.stderr,end='')
    r['passed']=r.get('run_exit',1)==0;(out/'report.json').write_text(json.dumps(r,indent=2)+'\n');return int(not r['passed'])
if __name__=='__main__':raise SystemExit(main())
