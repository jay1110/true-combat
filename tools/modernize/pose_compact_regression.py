#!/usr/bin/env python3
"""Focused protocol2 presentation tests; no game/process/network actions."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
import zipfile
from compat_validate import msvc_environment

ROOT = Path(__file__).resolve().parents[2]

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--out', type=Path, required=True)
    p.add_argument('--mod-pk3', type=Path, default=ROOT/'build/navgen-overkill/tce2/pak2.pk3')
    p.add_argument('--base-pk3', type=Path, default=ROOT/'build/navgen-overkill/etmain/pak0.pk3')
    p.add_argument('--optimized', action='store_true')
    p.add_argument('--sanitize', action='store_true')
    p.add_argument('--compiler')
    a = p.parse_args()
    out = a.out.resolve(); out.mkdir(parents=True, exist_ok=True)
    archives = [zipfile.ZipFile(a.mod_pk3), zipfile.ZipFile(a.base_pk3)]
    inputs = {}
    def read(name):
        for z in archives:
            try:
                data = z.read(name)
                inputs[name] = hashlib.sha256(data).hexdigest()
                return data.decode('latin1')
            except KeyError:
                pass
        raise ValueError('Missing '+name)
    rows = []
    for include in re.findall(r'#include\s+"([^"]+)"', read('animations/human_base.anim')):
        for line in read(include).splitlines():
            words = line.split('//',1)[0].split()
            if not words: continue
            if len(words)!=8: raise ValueError((include,line))
            name = words[0]; first,n,loop,fps,speed,transition,reverse = map(float,words[1:])
            frame = int(1000/(fps or 1))
            for initial in sorted({frame,40}):
                # Both raw and fire-event initialLerp alternatives are checked;
                # no heuristic claims that a name prefix is a script event.
                rows.append((name,[int(first),int(n),int(loop),frame,initial,int(reverse)]))
    generated = 'static const actualClip_t actualClips[]={\n'+',\n'.join(
        '{"'+name+'",{'+','.join(map(str,clip))+'}}' for name,clip in rows)+'\n};\n'
    fixture = (ROOT/'tools/modernize/pose_compact_test.c').read_text().replace('/* ACTUAL_CLIPS */',generated)
    source=out/'compact_generated.c';source.write_text(fixture,encoding='utf-8',newline='\n')
    sources=[source,ROOT/'src/game/bg_tce_pose_presentation.c',ROOT/'src/game/bg_tce_pose_clock.c']
    if os.name=='nt':
        env,cc=msvc_environment(out,'x64' if a.sanitize else 'x86','14.44' if a.sanitize else '14.39')
        exe=out/'compact_test.exe'
        command=[cc,'/nologo','/TC','/W4','/WX','/fp:precise','/O2' if a.optimized else '/Od',
                 '/DWIN32','/DGAMEDLL','/D_CRT_SECURE_NO_WARNINGS','/I'+str(ROOT/'src/game'),'/I'+str(ROOT)]
        if a.sanitize:command+=['/fsanitize=address','/Zi']
        command+=list(map(str,sources))+['/Fo:'+str(out)+os.sep,'/Fe:'+str(exe)]
    else:
        env=os.environ.copy();exe=out/'compact_test'
        command=[a.compiler or 'cc','-std=c99','-Wall','-Wextra','-Werror','-ffp-contract=off',
                 '-O2' if a.optimized else '-O0','-DGAMEDLL','-I'+str(ROOT/'src/game'),'-I'+str(ROOT)]
        if a.sanitize:command+=['-fsanitize=address,undefined','-fno-omit-frame-pointer','-g']
        command+=list(map(str,sources))+['-lm','-o',str(exe)]
    built=subprocess.run(command,cwd=out,env=env,text=True,capture_output=True)
    (out/'compile.log').write_text(built.stdout+built.stderr,encoding='utf-8')
    names=['src/game/bg_tce_pose_presentation.c','src/game/bg_tce_pose_presentation.h',
           'src/game/bg_tce_pose_clock.c','src/game/bg_tce_pose_clock.h',
           'tools/modernize/pose_compact_regression.py','tools/modernize/pose_compact_test.c']
    report=dict(command=command,build_exit=built.returncode,
                sources={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in names},
                inputs=inputs,clip_alternatives=len(rows),
                limits=['Actual clip metadata; not legacy observer phase parity',
                        'Pure callbacks and ABI assertions; no network/engine run'])
    if not built.returncode:
        run=subprocess.run([str(exe)],cwd=out,env=env,text=True,capture_output=True,timeout=60)
        report.update(run_exit=run.returncode,stdout=run.stdout,stderr=run.stderr)
        print(run.stdout+run.stderr,end='')
    else:print(built.stdout+built.stderr)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8',newline='\n')
    return report.get('run_exit',built.returncode)

if __name__=='__main__':raise SystemExit(main())
