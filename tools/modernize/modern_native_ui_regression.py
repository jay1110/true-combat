"""Source-bound optional native UI model/scoreboard with controlled rendering."""
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
    p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True)
    p.add_argument('--optimized',action='store_true');p.add_argument('--sanitize',action='store_true')
    p.add_argument('--compiler',default='auto');a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    source=ROOT/'src/ui/tce_modern_ui.c';score=ROOT/'src/cgame/cg_modern_scoreboard.inc';state=ROOT/'src/cgame/cg_scoreboard.c'
    fixture=Path(__file__).with_name('modern_native_ui_fixture.c')
    names=('TCE_ModernMarkerValid','TCE_ModernProgress','TCE_ModernRosterPages','TCE_ModernHiddenStats','TCE_ModernUIEnabled')
    text=fixture.read_text().replace('/* ACTUAL_MODEL */','\n'.join(function(source.read_text(),n) for n in names))
    text=text.replace('/* ACTUAL_SNAPSHOT_STATE */',function(state.read_text(),'TCE_ScoreSnapshotState'))
    text=text.replace('/* ACTUAL_SCOREBOARD */',score.read_text())
    connects=[ROOT/'src/cgame/cg_modern_loadpanel.inc',ROOT/'src/ui/ui_modern_loadpanel.inc']
    text=text.replace('/* ACTUAL_CONNECT */',connects[0].read_text()+
                      '\n#define trap_TranslateString FixtureUITranslateString\n'+connects[1].read_text()+
                      '\n#undef trap_TranslateString\n')
    unit=out/'native_modern_ui.c';unit.write_text(text,encoding='utf-8',newline='\n')
    if os.name=='nt':
        env,cc=msvc_environment(out,'x64' if a.sanitize else 'x86','14.44');exe=out/'native_modern_ui.exe'
        cmd=[cc,'/nologo','/TC','/W3','/DWIN32','/D_CRT_SECURE_NO_WARNINGS','/I'+str(ROOT),'/O2' if a.optimized else '/Od',str(unit),'/Fo:'+str(out)+os.sep,'/Fe:'+str(exe)]
        if a.sanitize:cmd[1:1]=['/fsanitize=address','/Zi','/Fd:'+str(out/'fixture.pdb')];cmd+=['/link','/DEBUG']
    else:
        env=os.environ.copy();cc=(shutil.which('clang') or shutil.which('gcc')) if a.compiler=='auto' else shutil.which(a.compiler)
        if not cc:raise RuntimeError('Native compiler unavailable')
        exe=out/'native_modern_ui';cmd=[cc,'-std=c99','-Wall','-Wextra','-I',str(ROOT),'-O2' if a.optimized else '-O0',str(unit),'-lm','-o',str(exe)]
        if a.sanitize:cmd[1:1]=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-g']
    r=subprocess.run(cmd,cwd=out,env=env,capture_output=True,text=True);(out/'compile.log').write_text(r.stdout+r.stderr,encoding='utf-8')
    headers=[ROOT/x for x in ('src/ui/tce_modern_ui.h','src/game/q_shared.h','src/game/bg_public.h',
                             'src/cgame/cg_local.h','src/ui/ui_public.h','src/game/tce_numeric.h',
                             'compat/legacy/game/tce_numeric.h')]
    report={'build_exit':r.returncode,'command':cmd,'inputs':{str(x.relative_to(ROOT)):hashlib.sha256(x.read_bytes()).hexdigest() for x in (source,score,state,fixture,Path(__file__),*connects,*headers)},
            'limits':['Actual new scoreboard and source-bound snapshot-state/model functions; controlled engine/draw services.',
                      'Does not prove native fonts, VFS ordering, pixels or live connect/download; parent performs actual module/runtime checks.']}
    if not r.returncode:
        r=subprocess.run([str(exe)],cwd=out,env=env,capture_output=True,text=True);report.update(run_exit=r.returncode,stdout=r.stdout,stderr=r.stderr)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps(report));return r.returncode


if __name__=='__main__':raise SystemExit(main())
