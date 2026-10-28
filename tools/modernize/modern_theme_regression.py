"""Compile the exact optional theme include under controlled engine services."""
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
    p.add_argument('--optimized',action='store_true');p.add_argument('--sanitize',action='store_true')
    a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    implementation=ROOT/'src/ui/tce_modern_theme.inc';fixture=Path(__file__).with_name('modern_theme_fixture.c')
    shared=ROOT/'src/ui/ui_shared.c'
    text=fixture.read_text().replace('/* ACTUAL_THEME */',implementation.read_text())
    text=text.replace('/* ACTUAL_WINDOW_PAINT */',function(shared.read_text(encoding='utf8'),'Window_Paint'))
    generated=out/'theme-fixture.c';generated.write_text(text,encoding='utf8',newline='\n')
    if os.name=='nt':env,compiler=msvc_environment(out,'x64' if a.sanitize else 'x86','14.44' if a.sanitize else '14.39')
    else:env=os.environ.copy();compiler=shutil.which('clang') or shutil.which('gcc')
    report={'source_sha256':{str(path.relative_to(ROOT)):hashlib.sha256(path.read_bytes()).hexdigest() for path in (implementation,fixture,shared,Path(__file__),ROOT/'src/game/q_shared.h',ROOT/'src/ui/ui_shared.h')},'runs':{},
            'limits':['Actual optional theme code and cvar/image services; does not prove native raster output, real input or VFS font ordering.']}
    for mode in ('ui','cgame'):
        directory=out/mode;directory.mkdir(exist_ok=True);exe=directory/('theme.exe' if os.name=='nt' else 'theme')
        if os.name=='nt':
            command=[compiler,'/nologo','/TC','/W3','/O2' if a.optimized else '/Od','/I'+str(ROOT),str(generated),'/Fe:'+str(exe)]
            if mode=='cgame':command+=['/DCGAMEDLL']
            if a.sanitize:command+=['/fsanitize=address','/Zi','/link','/DEBUG']
        else:
            command=[compiler,'-std=c99','-O2' if a.optimized else '-O0','-I'+str(ROOT),str(generated),'-lm','-o',str(exe)]
            if mode=='cgame':command+=['-DCGAMEDLL']
            if a.sanitize:command+=['-fsanitize=address,undefined','-g']
        built=subprocess.run(command,cwd=directory,env=env,capture_output=True,text=True);(directory/'compile.log').write_text(built.stdout+built.stderr)
        row={'command':command,'build_exit':built.returncode}
        if not built.returncode:
            result=subprocess.run([str(exe)],cwd=directory,env=env,capture_output=True,text=True,timeout=20)
            row.update(run_exit=result.returncode,stdout=result.stdout,stderr=result.stderr)
        report['runs'][mode]=row;print(mode+': '+str(row.get('stdout',built.stdout))+str(row.get('stderr',built.stderr)))
    report['passed']=all(row.get('run_exit',1)==0 for row in report['runs'].values())
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8');return int(not report['passed'])


if __name__=='__main__':raise SystemExit(main())
