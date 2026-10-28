"""Compile actual browser display-list state machine with controlled services."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
from compat_validate import msvc_environment
from weapon_regression import function

ROOT=Path(__file__).resolve().parents[2]


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--source',type=Path,default=ROOT/'src/ui/ui_main.c')
    p.add_argument('--optimized',action='store_true');p.add_argument('--sanitize',action='store_true');a=p.parse_args()
    out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    source=a.source.resolve();fixture=Path(__file__).with_name('server_browser_fixture.c')
    body=function(source.read_text(encoding='utf8'),'UI_BuildServerDisplayList')
    unit=out/'browser.c';unit.write_text(fixture.read_text().replace('/* ACTUAL_BROWSER */',body),encoding='utf8',newline='\n')
    if os.name=='nt':
        env,cc=msvc_environment(out,'x64' if a.sanitize else 'x86','14.44' if a.sanitize else '14.39');exe=out/'browser.exe'
        cmd=[cc,'/nologo','/TC','/W3','/D_CRT_SECURE_NO_WARNINGS','/O2' if a.optimized else '/Od',str(unit),'/Fe:'+str(exe),'/Fo:'+str(out)+os.sep]
        if a.sanitize:cmd+=['/fsanitize=address','/Zi','/link','/DEBUG']
    else:
        env=os.environ.copy();cc=shutil.which('clang') or shutil.which('gcc');exe=out/'browser'
        cmd=[cc,'-std=c99','-O2' if a.optimized else '-O0',str(unit),'-o',str(exe)]
        if a.sanitize:cmd+=['-fsanitize=address,undefined']
    built=subprocess.run(cmd,cwd=out,env=env,capture_output=True,text=True);(out/'compile.log').write_text(built.stdout+built.stderr)
    report={'build_exit':built.returncode,'command':cmd,'function_sha256':hashlib.sha256(body.encode()).hexdigest(),'inputs':{str(x.relative_to(ROOT)):hashlib.sha256(x.read_bytes()).hexdigest() for x in (source,fixture,Path(__file__))},'limits':['Actual display-list body with controlled engine reply state and filters; no real master/DNS/UDP connectivity or public server count is proved.']}
    if not built.returncode:
        run=subprocess.run([str(exe)],cwd=out,env=env,capture_output=True,text=True);report.update(run_exit=run.returncode,stdout=run.stdout,stderr=run.stderr)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');print(json.dumps(report));return report.get('run_exit',built.returncode)


if __name__=='__main__':raise SystemExit(main())
