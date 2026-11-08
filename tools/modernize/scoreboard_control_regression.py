"""Compile the real scoreboard command/input state transitions with engine traps."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess
from weapon_regression import function
from compat_validate import msvc_environment
ROOT=Path(__file__).resolve().parents[2]
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--optimized',action='store_true')
    parser.add_argument('--sanitize',action='store_true')
    args=parser.parse_args();out=args.out.resolve();out.mkdir(parents=True,exist_ok=True)
    paths=['src/cgame/cg_consolecmds.c','src/cgame/cg_newDraw.c','src/cgame/cg_scoreboard.c','src/cgame/cg_local.h','src/cgame/cg_public.h']
    sources={p:(ROOT/p).read_text(encoding='utf-8-sig') for p in paths}
    units=[function(sources[paths[0]],n) for n in ['CG_ScoresDown_f','CG_ScoresUp_f','CG_ClosePinnedScoreboard','CG_ToggleScoreboard_f']]
    units += [function(sources[paths[1]],n) for n in ['CG_EventHandling','CG_KeyEvent']]
    assert 'if (cg.tceScoreboardPinned) CG_ScoresDown_f();' in function(sources[paths[2]],'CG_DrawScoreboard')
    assert '{ "toggleScoreboard",' in sources[paths[0]]
    template=Path(__file__).with_name('scoreboard_control_fixture.c')
    generated=out/'fixture.c';generated.write_text(template.read_text().replace('/* ACTUAL_FUNCTIONS */','\n\n'.join(units)),encoding='utf-8',newline='\n')
    env=os.environ.copy();exe=out/('fixture.exe' if os.name=='nt' else 'fixture')
    if os.name=='nt':
        env,compiler=msvc_environment(out,'x64' if args.sanitize else 'x86','14.44' if args.sanitize else '14.39')
        command=[compiler,'/nologo','/TC','/W3','/O2' if args.optimized else '/Od','/I'+str(ROOT),str(generated),'/Fe:'+str(exe)]
        if args.sanitize:command+=['/fsanitize=address','/Zi','/link','/DEBUG']
    else:
        compiler=shutil.which('clang') or shutil.which('gcc')
        command=[compiler,'-std=c99','-O2' if args.optimized else '-O0','-I'+str(ROOT),str(generated),'-lm','-o',str(exe)]
        if args.sanitize:command+=['-fsanitize=address,undefined','-g']
    compiled=subprocess.run(command,cwd=out,env=env,capture_output=True,text=True)
    (out/'compile.log').write_text(compiled.stdout+compiled.stderr,encoding='utf-8')
    report={'source_sha256':{p:hashlib.sha256((ROOT/p).read_bytes()).hexdigest() for p in paths},'command':command,'build_exit':compiled.returncode,'limits':['Actual production command and event bodies; controlled clock/input/client-command services.','No real renderer or OS key dispatch is simulated; native Escape routing must be checked in-game.']}
    if compiled.returncode:print(compiled.stdout+compiled.stderr)
    else:
        run=subprocess.run([str(exe)],cwd=out,env=env,capture_output=True,text=True,timeout=30)
        report.update(run_exit=run.returncode,stdout=run.stdout,stderr=run.stderr);print(run.stdout+run.stderr,end='')
    (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    return report.get('run_exit',compiled.returncode)
if __name__=='__main__':raise SystemExit(main())
