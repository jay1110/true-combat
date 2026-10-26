"""Run the complete production matcher with its real MSVC std::regex engine.

No game process or module is built. The small temporary native executable uses
the actual function body and the production common.h grammar definitions. It
also executes the old matcher as a failing control for the observed FLAG query.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless

ROOT = (Path(__file__).resolve().parents[1] / "src")
COMMON = ROOT.parent / 'vendor/omni-bot/source/Omnibot/Common'


def program():
    source = (COMMON / 'Utilities.cpp').read_text(encoding='utf8')
    start = source.index('\tbool RegexMatch(')
    body = source[start:source.index('\n\tfloat FloatMax', start)]
    common = (COMMON / 'common.h').read_text(encoding='utf8')
    options = re.search(r'#ifdef OMNIBOT_USE_STD_FILESYSTEM\s*\n#define REGEX_OPTIONS[^\n]*\n#else\n#define REGEX_OPTIONS[^\n]*\n#endif', common).group()
    return r'''
#include <regex>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <exception>
namespace obre = std;
typedef std::string String;
#define OMNIBOT_USE_STD_FILESYSTEM 1
''' + options + r'''
static int errors;
#define _UNUSED(value) (void)(value)
#define OBASSERT(condition, message) (void)0
#define LOGERR(message) do { ++errors; } while(0)
namespace Utils {
''' + body + r'''
}
// Exact pre-fix production function, not a simplified mock or another engine.
static bool BeforeFix(const char *exp,const char *str) {
 for(;;exp++,str++) {
  char e=*exp,s=*str;
  if(!e)return !s;
  if(e==s)continue;
  if(e>='A'&&e<='Z') { if(e+('a'-'A')==s)continue; }
  else if(e>='a'&&e<='z') { if(e-('a'-'A')==s)continue; }
  else if(e!='_'&&!(e>='0'&&e<='9')) {
   if(e=='*'||(e=='\\'&&exp[1]=='{'))exp--,str--;
   break;
  }
  e=exp[1];if(e!='*'&&e!='\\')return false;
  break;
 }
 if(exp[0]=='.'&&exp[1]=='*'&&exp[2]=='\0')return true;
 try { obre::regex expression(exp,REGEX_OPTIONS);return obre::regex_match(str,expression); }
 catch(const std::exception&){return false;}
}
struct Example {const char *pattern,*value;bool expected;};
int main() {
 const Example examples[]={
  {"FLAG_1","flag_1",true},{"FLAG_1","FLAG_10",false},
  {"FLAG_1","XFLAG_1",false},{"FLAG_1","FLAG_",false},
  {"","",true},{"","FLAG",false},{".*","",true},{".*","ANY_name",true},
  {"FLAG_.*","fLaG_",true},{"FLAG_.*","FLAG_A_more",true},
  {"FLAG_.*","FLAG",false},{"FLAG_.*","CAPPOINT_A",false},
  {"TCEOBJECTIVE_.*|FLAG_.*|CAPPOINT_.*","FLAG_allied_object",true},
  {"TCEOBJECTIVE_.*|FLAG_.*|CAPPOINT_.*","CAPPOINT_flagonly",true},
  {"TCEOBJECTIVE_.*|FLAG_.*|CAPPOINT_.*","TCEOBJECTIVE_bomb",true},
  {"TCEOBJECTIVE_.*|FLAG_.*|CAPPOINT_.*","DEFEND_flag",false},
  {"FLAG_A|FLAG_B","FLAG_A",true},{"FLAG_A|FLAG_B","FLAG_B",true},
  {"FLAG_A|FLAG_B","FLAG_AB",false},{"FLAG_A|FLAG_B","XFLAG_A",false},
  {"(FLAG|CAPPOINT)_.*","cappoint_A",true},
  {"(FLAG|CAPPOINT)_.*","FLAG_A",true},
  {"(FLAG|CAPPOINT)_.*","(FLAG|CAPPOINT)_A",false},
  {"^(TCEOBJECTIVE|FLAG|CAPPOINT)_.*$","FLAG_A",true},
  {"^(TCEOBJECTIVE|FLAG|CAPPOINT)_.*$","XFLAG_A",false},
  {"FLAG_.*|","",true},{"FLAG_.*|","CAPPOINT_A",false},
  {R"(goal_.*\.gm)","goal_flag.gm",true},
  {R"(goal_.*\.gm)","goal_tceobjective.GM",true},
  {R"(goal_.*\.gm)","goal_flagxgm",false},
  {R"(goal_.*\.gm)","goal_flag.gmx",false},
  {R"(C:\\mods\\tce2\\(cgame|qagame)_mp_x86\.dll)",R"(C:\mods\tce2\cgame_mp_x86.dll)",true},
  {R"(C:\\mods\\tce2\\(cgame|qagame)_mp_x86\.dll)",R"(c:\MODS\TCE2\QGAME_mp_x86.DLL)",false},
  {R"(C:\\mods\\tce2\\(cgame|qagame)_mp_x86\.dll)",R"(c:\MODS\TCE2\QAGAME_mp_x86.DLL)",true},
  {R"(C:\\mods\\tce2\\(cgame|qagame)_mp_x86\.dll)",R"(C:\mods\tce2\cgame_mp_x86Xdll)",false},
  {"A*","",true},{"A*","AAA",true},{"FLAG_F*","FLAG_",true},
  {"A*","AAB",false},{"FLAG_.*_end","FLAG_A_end",true},
  {"FLAG_.*_end","FLAG_A_end_extra",false},
  {R"(A\{2\})","AA",true},{R"(A\{2\})","A",false},
  // Actual stock commands.gm grouping/newline union must stay BRE-compatible.
  {"\\(CAMP.*\nDEFEND.*\nATTACK.*\nSNIPE.*\nROUTE.*\nMOBILE.*\nCAPPOINT.*\nPLANTMINE.*\n.*ART.*\\)","DEFEND_A",true},
  {"\\(CAMP.*\nDEFEND.*\nATTACK.*\nSNIPE.*\nROUTE.*\nMOBILE.*\nCAPPOINT.*\nPLANTMINE.*\n.*ART.*\\)","CAPPOINT_A",true},
  {"\\(CAMP.*\nDEFEND.*\\)","FLAG_A",false},
  {"[|()]","|",true},{"[|()]","(",true},
  {"[]|()]", "]",true},{"[^]|()]","A",true},
  {"[[:alpha:]|()]","|",true},{"[[:alpha:]|()]","A",true},
  {"[|()]\\(FLAG\\)","|FLAG",true},
  {"[[:alpha:]|()]\\(FLAG\\)","AFLAG",true},
  {"[^^]\\(FLAG\\)","AFLAG",true},{"[^^]\\(FLAG\\)","^FLAG",false},
  {"[", "[",false},{"FLAG_[", "FLAG_[",false}
 };
 for(size_t i=0;i<sizeof(examples)/sizeof(*examples);++i){
  const Example &e=examples[i];bool actual=Utils::RegexMatch(e.pattern,e.value);
  if(actual!=e.expected){fprintf(stderr,"case %u: /%s/ value <%s>: actual %d expected %d\n",
   (unsigned)i,e.pattern,e.value,(int)actual,(int)e.expected);return 3;}
 }
 if(errors!=2){fprintf(stderr,"invalid regex logging count %d expected 2\n",errors);return 4;}
 const char *unionQuery="TCEOBJECTIVE_.*|FLAG_.*|CAPPOINT_.*";
 if(BeforeFix(unionQuery,"FLAG_allied_object") || BeforeFix(unionQuery,"CAPPOINT_flagonly"))return 5;
 if(std::regex_match("FLAG_allied_object",std::regex(unionQuery,REGEX_OPTIONS)))return 6;
 printf("%u real native regex cases passed; original prefix and grep-union failures reproduced\n",
  (unsigned)(sizeof(examples)/sizeof(*examples)));
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class OmnibotRegexNativeTests(TestCase):
    def test_complete_production_function_real_engine_and_old_control(self):
        with tempfile.TemporaryDirectory(prefix='tce-regex-native-') as directory:
            folder = Path(directory)
            (folder / 'main.cpp').write_text(program(), encoding='utf8')
            (folder / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(OmniRegex LANGUAGES CXX)\n'
                'add_executable(omni-regex main.cpp)\n'
                'set_property(TARGET omni-regex PROPERTY CXX_STANDARD 17)\n', encoding='utf8')
            options = dict(capture_output=True, text=True, timeout=90,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', directory, '-B', str(folder / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(folder / 'build'), '--config', 'Release'],
                            [str(folder / 'build/Release/omni-regex.exe')]):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertNotRegex(result.stdout + result.stderr, r'(?im)\bwarning [CDL][0-9]+')
                if command[-1].endswith('.exe'):
                    print(result.stdout.strip())


if __name__ == '__main__':
    main()
