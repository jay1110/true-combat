"""Parse exact generation-pause scripts with Omni-bot's real GM parser."""
import argparse
import json
from pathlib import Path
import subprocess
import sys


def main():
    parser=argparse.ArgumentParser();parser.add_argument("--vcvars",type=Path,required=True);args=parser.parse_args()
    root=Path(__file__).resolve().parents[2]
    sys.path.insert(0,str(root/"src/tools/navstudio"));import quiet
    directory=root/"build/navgen-overkill/native-ground-unit";directory.mkdir(parents=True,exist_ok=True)
    gm=root/"src/vendor/omni-bot/source/Omnibot/dependencies/gmscriptex/gmsrc_ex/src"
    sources=[quiet.file_source(builder("1"*16)) for builder in (quiet.begin_script,quiet.restore_script)]
    sources.append(quiet.file_source(quiet.count_script()))
    fixture=directory/"gmparse.cpp"
    fixture.write_text('#include "gmCodeTree.h"\n#include <stdio.h>\nint main(){gmLog log;int errors=0;\n'+
        ''.join('{const char* source='+json.dumps(source)+';int n=gmCodeTree::Get().Lock(source,&log);'
                'printf("script%d errors=%d\\n",'+str(i)+',n);bool first=true;const char* text;'
                'while((text=log.GetEntry(first)))printf("%s\\n",text);gmCodeTree::Get().Unlock();'
                'log.Reset();errors+=n;}\n' for i,source in enumerate(sources))+'return errors;}\n')
    files=[fixture]+[gm/"gm"/name for name in ("gmCodeTree.cpp","gmParser.cpp","gmScanner.cpp","gmMemChain.cpp","gmLog.cpp","gmUtil.cpp")]
    script=directory/"gmparse.cmd";exe=directory/"gmparse.exe"
    script.write_text('@echo off\ncall "'+str(args.vcvars.resolve())+'" -arch=x64 -host_arch=x64 >nul\n'
        'if errorlevel 1 exit /b %errorlevel%\n'
        'cl /nologo /EHsc /MD /W0 /DGM_PTR_SIZE_64 /DGM_DEFAULT_ALLOC_ALIGNMENT=16 /I"'+str(gm/"gm")+
        '" /I"'+str(gm/"platform/win32msvc")+'" /Fe"'+str(exe)+'" '+
        ' '.join('"'+str(path)+'"' for path in files)+'\nif errorlevel 1 exit /b %errorlevel%\n"'+str(exe)+'"\n')
    subprocess.run(["cmd.exe","/d","/c",str(script)],check=True,cwd=directory)


if __name__=="__main__":main()
