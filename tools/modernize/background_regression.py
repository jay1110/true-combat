"""Compile actual background selection/init; no engine or network is started."""
import argparse,hashlib,json,os,shutil,subprocess
from pathlib import Path
from compat_validate import msvc_environment
from weapon_regression import function
ROOT=Path(__file__).resolve().parents[2]


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--out',type=Path,required=True)
    p.add_argument('--optimized',action='store_true');p.add_argument('--sanitize',action='store_true')
    p.add_argument('--season-only',action='store_true',help='Run new seasonal cases and init smoke without repeating the unchanged46k picker grid');a=p.parse_args()
    out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
    source=ROOT/'src/ui/tce_modern_ui.c';text=source.read_text();fixture=Path(__file__).with_name('background_fixture.c')
    names=('TCE_ModernBackgroundPhase','TCE_ModernBackgroundPick','TCE_ModernBackgroundWinter',
           'TCE_ModernBackgroundCatalogAvailable','TCE_ModernBackgroundName',
           'TCE_ModernBackgroundConfigValue','TCE_ModernBackgroundPreferencesInit',
           'TCE_ModernBackgroundCvar','TCE_ModernBackgroundInit')
    bodies={name:function(text,name) for name in names}
    assert 'if (modernEnabled) TCE_ModernBackgroundInit();' in function(text,'TCE_ModernUIEnabled')
    assert 'modernBackgroundReady = 0;' in function(text,'TCE_ModernUIReset')
    assert 'ui_backgroundSession' not in function(text,'TCE_ModernUIReset')
    entry=function((ROOT/'src/ui/ui_main.c').read_text(),'_UI_Init')
    assert entry.index('TCE_ModernBackgroundPreferencesInit();') < entry.index('UI_RegisterCvars();')
    assert 'name = TCE_ModernBackgroundName();' in function(text,'TCE_ModernBackdrop')
    generated=out/'background.c';generated.write_text(fixture.read_text().replace('/* ACTUAL_BACKGROUND_FUNCTIONS */','\n'.join(bodies.values())),newline='\n')
    if os.name=='nt':
        env,cc=msvc_environment(out,'x64' if a.sanitize else 'x86','14.44' if a.sanitize else '14.39');exe=out/'background.exe'
        cmd=[cc,'/nologo','/TC','/W3','/D_CRT_SECURE_NO_WARNINGS','/O2' if a.optimized else '/Od',str(generated),'/Fe:'+str(exe),'/Fo:'+str(out)+os.sep]
        if a.sanitize:cmd+=['/fsanitize=address','/Zi','/link','/DEBUG']
        if a.season_only:cmd.insert(1,'/DBACKGROUND_SEASON_ONLY')
    else:
        env=os.environ.copy();cc=shutil.which('clang') or shutil.which('gcc');exe=out/'background'
        cmd=[cc,'-std=c99','-O2' if a.optimized else '-O0',str(generated),'-o',str(exe)]
        if a.sanitize:cmd+=['-fsanitize=address,undefined']
        if a.season_only:cmd.insert(1,'-DBACKGROUND_SEASON_ONLY')
    built=subprocess.run(cmd,cwd=out,env=env,capture_output=True,text=True);(out/'compile.log').write_text(built.stdout+built.stderr)
    report={'build_exit':built.returncode,'command':cmd,'inputs':{str(x.relative_to(ROOT)):hashlib.sha256(x.read_bytes()).hexdigest() for x in (source,ROOT/'src/ui/tce_modern_ui.h',ROOT/'src/ui/ui_main.c',fixture,Path(__file__))},'functions':{n:hashlib.sha256(v.encode()).hexdigest() for n,v in bodies.items()},'limits':['Actual native policy/init with controlled stock engine traps; no physical renderer/network/process-start proof.']}
    if not built.returncode:
        run=subprocess.run([str(exe)],cwd=out,env=env,capture_output=True,text=True);report.update(run_exit=run.returncode,stdout=run.stdout,stderr=run.stderr)
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report));return report.get('run_exit',built.returncode)


if __name__=='__main__':raise SystemExit(main())
