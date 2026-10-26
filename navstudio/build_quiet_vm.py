"""Build the small offline VM driver against the existing Omni-bot Win32 library."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def main():
    root=Path(__file__).resolve().parents[1]
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--visual-studio',type=Path,default=Path('C:/Program Files/Microsoft Visual Studio/2022/Community'))
    parser.add_argument('--library',type=Path,default=root/'build/omnibot-win32/Common/Release/omnibot-common.lib')
    args=parser.parse_args();studio=args.visual_studio.resolve()
    vcvars=studio/'VC/Auxiliary/Build/vcvarsall.bat'
    versions=sorted((studio/'VC/Tools/MSVC').iterdir(),key=lambda p:tuple(int(v) for v in p.name.split('.')))
    compiler=versions[-1]/'bin/Hostx64/x86/cl.exe'
    if not vcvars.is_file() or not compiler.is_file() or not args.library.is_file():
        parser.error('Existing Visual Studio C++ tools and Omni-bot Win32 release library are required.')
    # Capture only this helper's compiler environment; never print env values.
    setup=subprocess.run('call '+subprocess.list2cmdline([str(vcvars),'x86'])+' >nul && set',shell=True,
                         capture_output=True,text=True,errors='replace',creationflags=subprocess.CREATE_NO_WINDOW)
    if setup.returncode:raise RuntimeError('Visual Studio environment setup failed: '+setup.stderr)
    environment=os.environ.copy()
    for line in setup.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key,value=line.split('=',1);environment[key]=value
    output=root/'build/navstudio-gmvm';output.mkdir(exist_ok=True)
    base=root/'vendor/omni-bot/source/Omnibot/dependencies/gmscriptex/gmsrc_ex/src'
    source=Path(__file__).with_name('gm_quiet_vm.cpp');executable=output/'gm-quiet-vm.exe'
    command=[str(compiler),'/nologo','/MT','/EHsc','/std:c++17','/DWIN32','/DNDEBUG',
             *('/I'+str(base/p) for p in ('gm','binds','3rdParty','platform/win32msvc')),
             str(source),str(args.library.resolve()),'/Fe:'+str(executable),'/Fo:'+str(output/'gm-quiet-vm.obj')]
    result=subprocess.run(command,env=environment,cwd=output,capture_output=True,text=True,errors='replace',
                          creationflags=subprocess.CREATE_NO_WINDOW)
    if result.returncode:raise RuntimeError(result.stdout+result.stderr)
    digest=lambda path:hashlib.sha256(Path(path).read_bytes()).hexdigest()
    (output/'build.json').write_text(json.dumps(dict(source_sha256=digest(source),library_sha256=digest(args.library),
        executable_sha256=digest(executable),source=str(source),library=str(args.library.resolve()),executable=str(executable)),indent=2),encoding='utf8')
    print('Offline GameMonkey VM driver built: '+str(executable))


if __name__=='__main__':main()
