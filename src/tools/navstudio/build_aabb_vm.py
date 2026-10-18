"""Build a small offline VM with actual native Omni AABB bindings.

No engine, game process, network, installation or existing VM is touched.
Only entity coordinates and drawing are independent fixture boundaries.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess
from xml.etree import ElementTree
from test_door_runtime_native import body


def main():
    root=Path(__file__).resolve().parents[3]
    common=root/'src/vendor/omni-bot/source/Omnibot/Common'
    output=root/'build/navstudio-aabb-gmvm';output.mkdir(exist_ok=True)
    library=root/'build/omnibot-win32/Common/Release/omnibot-common.lib'
    vs=Path('C:/Program Files/Microsoft Visual Studio/2022/Community')
    vcvars=vs/'VC/Auxiliary/Build/vcvarsall.bat'
    versions=sorted((vs/'VC/Tools/MSVC').iterdir(),key=lambda p:tuple(map(int,p.name.split('.'))))
    compiler=versions[-1]/'bin/Hostx64/x86/cl.exe'
    setup=subprocess.run('call '+subprocess.list2cmdline([str(vcvars),'x86'])+' >nul && set',shell=True,
                         capture_output=True,text=True,creationflags=subprocess.CREATE_NO_WINDOW)
    if setup.returncode:raise RuntimeError(setup.stderr)
    env=os.environ.copy()
    for line in setup.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key,value=line.split('=',1);env[key]=value
    project=ElementTree.parse(root/'build/omnibot-win32/Common/omnibot-common.vcxproj')
    includes=[]
    for tag in project.iter():
        if tag.tag.endswith('AdditionalIncludeDirectories'):
            includes.extend(x for x in (tag.text or '').split(';') if x and not x.startswith('%'))
    includes=list(dict.fromkeys(includes))
    quiet=Path(__file__).with_name('gm_quiet_vm.cpp').read_text()
    quiet=quiet.replace('gmMachineLib(&machine);',
        'gmMachineLib(&machine);fixtureMachine=&machine;gmAABB::Initialise(&machine,false);machine.RegisterLibrary(nativeLibrary,sizeof(nativeLibrary)/sizeof(nativeLibrary[0]));')
    fixture=r'''
#include "PrecompCommon.h"
#include "gmAABB.cpp"
static gmMachine*fixtureMachine=nullptr;
namespace Utils{void OutlineAABB(const AABB&,const obColor&,float,AABB::Direction){}}
namespace EngineFuncs{bool EntityWorldAABB(GameEntity e,AABB&box){
 float origin[3]={0.f,e.GetIndex()==3?0.f:32.125f,30.f};float half=16.f;
 auto*actors=fixtureMachine->GetGlobals()->Get(fixtureMachine,"Actors").GetTableObjectSafe();
 if(actors){auto*actor=actors->Get(gmVariable(e.GetIndex())).GetTableObjectSafe();
  if(!actor)return false;
  actor->Get(fixtureMachine,"Pos").GetVector(origin[0],origin[1],origin[2]);
  actor->Get(fixtureMachine,"Half").GetFloatSafe(half,16.f);}
 for(int i=0;i<3;++i){box.m_Mins[i]=origin[i]+(i==2?-29.f:-half-1);
  box.m_Maxs[i]=origin[i]+(i==2?60.f:half+1);}return true;}}
// Only integer fixture entity decoding differs from production. The actual
// OPTIONAL output-argument binding and return-value path execute unchanged.
#undef GM_CHECK_GAMEENTITY_FROM_PARAM
#define GM_CHECK_GAMEENTITY_FROM_PARAM(VAR,N) GM_CHECK_INT_PARAM(fixtureId,N);VAR.FromInt(fixtureId)
NATIVE_WORLD_BINDING
static int NativeGC(gmThread*t){t->GetMachine()->CollectGarbage(true);return GM_OK;}
static gmFunctionEntry nativeLibrary[]={{"GetEntWorldAABB",NativeWorldAABB},{"NativeGC",NativeGC}};
'''
    binding=body((common/'gmBotLibrary.cpp').read_text(),'static int gmfGetEntityAABB(')
    fixture=fixture.replace('NATIVE_WORLD_BINDING',binding.replace('gmfGetEntityAABB','NativeWorldAABB'))
    source=output/'aabb-vm.cpp';source.write_text(fixture+'\n'+quiet,encoding='utf8')
    executable=output/'gm-aabb-vm.exe'
    command=[str(compiler),'/nologo','/MT','/EHsc','/std:c++17','/DWIN32','/DNDEBUG',
        '/DOMNIBOT_USE_STD_FILESYSTEM','/D_HAS_AUTO_PTR_ETC=1',*('/I'+p for p in includes),
        str(source),str(library),'/Fe:'+str(executable),'/Fo:'+str(output/'aabb-vm.obj')]
    run=subprocess.run(command,env=env,cwd=output,capture_output=True,text=True,errors='replace',
                       creationflags=subprocess.CREATE_NO_WINDOW)
    (output/'build.log').write_text(run.stdout+run.stderr,encoding='utf8')
    if run.returncode:raise RuntimeError(run.stdout+run.stderr)
    digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    (output/'build.json').write_text(json.dumps({'source':str(source),'source_sha256':digest(source),
        'executable':str(executable),'executable_sha256':digest(executable),
        'native_sources':{str(p.relative_to(root)):digest(p) for p in
            (common/'gmAABB.cpp',common/'gmAABB.h',common/'gmBind.h',common/'gmBotLibrary.cpp')},
        'library_sha256':digest(library),'game_or_api_used':False},indent=2)+'\n')
    print(executable)


if __name__=='__main__':main()
