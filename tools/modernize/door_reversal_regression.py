#!/usr/bin/env python3
"""Compile actual mover reversal/state/team functions with a failed-push fixture."""
import argparse,hashlib,json,re,shutil,subprocess,sys
from pathlib import Path
from compat_validate import msvc_environment
from movement_regression import function
ROOT=Path(__file__).resolve().parents[2]
def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--source',type=Path,default=ROOT/'src/game/g_mover.c')
 p.add_argument('--out',type=Path,default=ROOT/'build/door-reversal-regression')
 p.add_argument('--arch',choices=['x86','x64'],default='x86')
 p.add_argument('--configuration',choices=['Debug','Release'],default='Release')
 p.add_argument('--sanitize',action='store_true')
 p.add_argument('--compiler',default='auto',help='Native GCC/Clang compiler; Windows uses MSVC')
 a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=True)
 text=a.source.read_text(encoding='utf-8-sig');names=['SetMoverState','Blocked_DoorRotate','G_MoverTeam']
 helpers=['G_TCERotationTrajectory','G_TCEReverseRotationTime'] if 'static void G_TCERotationTrajectory(' in text else []
 unit='\n\n'.join(function(text,n) for n in helpers+names)
 pushed=re.search(r'typedef struct\s*\{[^}]+\}\s*pushed_t;',text)[0]
 source=Path(__file__).with_suffix('.c').read_text(encoding='utf-8').replace('/* PRODUCTION_FUNCTIONS */',unit).replace('/* PRODUCTION_PUSHED_TYPE */',pushed).replace('/* FROZEN_SET_MOVER_STATE */',Path(__file__).with_name('door_reversal_baseline.inc').read_text(encoding='utf-8'))
 path=out/'door_reversal.c';path.write_text(source,encoding='utf-8',newline='\n')
 sources=[str(path),str(ROOT/'src/game/tce_trajectory.c'),str(ROOT/'src/game/q_math.c')]
 if sys.platform=='win32':
  env,cc=msvc_environment(out,a.arch,'14.44');exe=out/'test.exe'
  command=[cc,'/nologo','/TC','/O2' if a.configuration=='Release' else '/Od','/W3','/fp:precise','/DWIN32','/DGAMEDLL','/I'+str(ROOT),*sources,'/Fe:'+str(exe),'/Fo:'+str(out)+'\\']
  if helpers:command.insert(1,'/DTCE_ROTATION_REVERSE_HELPERS')
  if a.sanitize:command.insert(1,'/fsanitize=address')
  compiler_version='MSVC14.44 (selected developer environment)'
 else:
  env=None;exe=out/'test'
  cc=(shutil.which('clang') or shutil.which('gcc')) if a.compiler=='auto' else shutil.which(a.compiler)
  if not cc:raise RuntimeError('No native C compiler found')
  command=[cc,'-m32' if a.arch=='x86' else '-m64','-std=c99','-O2' if a.configuration=='Release' else '-O0','-fno-fast-math','-ffp-contract=off','-DGAMEDLL','-I',str(ROOT),*sources,'-lm','-o',str(exe)]
  if helpers:command.insert(1,'-DTCE_ROTATION_REVERSE_HELPERS')
  if a.sanitize:command[1:1]=['-fsanitize=address,undefined','-fno-sanitize-recover=all','-fno-omit-frame-pointer','-g']
  compiler_version=subprocess.run([cc,'--version'],capture_output=True,text=True,check=True).stdout
 build=subprocess.run(command,cwd=out,env=env,capture_output=True,text=True);(out/'compile.log').write_text(build.stdout+build.stderr,encoding='utf-8')
 report={'source_sha256':hashlib.sha256(a.source.read_bytes()).hexdigest(),'inputs_sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [Path(__file__),Path(__file__).with_suffix('.c'),Path(__file__).with_name('door_reversal_baseline.inc'),ROOT/'src/game/tce_trajectory.c',ROOT/'src/game/q_math.c']},'generated_source_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'platform':sys.platform,'compiler':cc,'compiler_version':compiler_version,'configuration':a.configuration,'arch':a.arch,'sanitizer':a.sanitize,'command':command,'build_exit':build.returncode,'limits':['Actual production SetMoverState/Blocked_DoorRotate/G_MoverTeam and full TCE trajectory evaluator','Synthetic failed-push/link/damage services; no native BSP collision or gameplay proof']}
 if not build.returncode:
  report['executable_sha256']=hashlib.sha256(exe.read_bytes()).hexdigest()
  if sys.platform!='win32':report['executable_file']=subprocess.run(['file',str(exe)],capture_output=True,text=True,check=True).stdout.strip()
  run=subprocess.run([str(exe)],cwd=out,env=env,capture_output=True,text=True,timeout=30);report.update(exit_code=run.returncode,stdout=run.stdout,stderr=run.stderr);print(run.stdout+run.stderr,end='')
 else:print(build.stdout+build.stderr)
 (out/'report.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');return build.returncode or report.get('exit_code',1)
if __name__=='__main__':raise SystemExit(main())
