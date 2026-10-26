"""Execute actual native DMS field handlers and role parsing without a game."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
from test_traversal_reset_native import function

ROOT=(Path(__file__).resolve().parents[1] / "src")

def compile_program(test,program,language='CXX'):
    with tempfile.TemporaryDirectory(prefix='tce-dms-regression-') as folder:
        root=Path(folder);name='main.cpp' if language=='CXX' else 'main.c';(root/name).write_text(program,encoding='utf8')
        (root/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(DmsTests LANGUAGES '+language+')\nset(CMAKE_CXX_STANDARD 17)\nadd_executable(dms-test '+name+')\n')
        options=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
        for command in (['cmake','-S',folder,'-B',str(root/'build'),'-A','Win32'],
                        ['cmake','--build',str(root/'build'),'--config','Release']):
            result=subprocess.run(command,**options);test.assertEqual(result.returncode,0,result.stdout+result.stderr)
        return subprocess.run([str(root/'build/Release/dms-test.exe')],**options)


def roles_program(control=False):
    text=(ROOT.parent/'vendor/omni-bot/source/Omnibot/Common/PropertyBinding.cpp').read_text()
    start=text.index('bool FromString(',text.index('class PropertyBitflag32'))
    opening=text.index('{',start);end=opening+1;depth=1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    actual=text[start:end]
    if control:
        assert 'm_Value = bf;\n\t\t\treturn true;' in actual
        actual=actual.replace('m_Value = bf;\n\t\t\treturn true;','m_Value = bf;\n\t\t\treturn false;',1)
    return r'''
#include <string>
#include <vector>
#include <stdio.h>
#include <string.h>
typedef unsigned int obuint32;typedef std::string String;typedef std::vector<String> StringVector;
struct IntEnum{const char*m_Key;int m_Value;};
struct BitFlag32{unsigned int bits=0;void SetFlag(int f,bool){bits|=1u<<f;}};
namespace Utils{
static void Tokenize(const String&s,const char*delimiters,StringVector&out){
 size_t cursor=0;while(cursor<s.size()){
  size_t end=s.find_first_of(delimiters,cursor);if(end==String::npos)end=s.size();
  if(end>cursor)out.push_back(s.substr(cursor,end-cursor));cursor=end+1;
 }
}
static int StringCompareNoCase(const char*a,const char*b){return _stricmp(a,b);}
}
struct ActualRoles{IntEnum*m_Enum;int m_EnumNum;BitFlag32 m_Value;
''' + actual + r'''
};
#define REQUIRE(x,n) if(!(x)){fprintf(stderr,"%s\n",n);return 12;}
int main(){
 IntEnum names[]={{"ATTACKER",1},{"DEFENDER",2},{"SNIPER",4}};
 ActualRoles roles={names,3,{}};
 REQUIRE(roles.FromString("ATTACKER"),"valid role must report success to MapGoal::SetProperty");
 REQUIRE(roles.m_Value.bits==2,"single native role");
 REQUIRE(roles.FromString("attacker|DEFENDER"),"multi role accepted case insensitive");
 REQUIRE(roles.m_Value.bits==6,"combined exact role bits");
 REQUIRE(!roles.FromString("ATTACKER|UNKNOWN"),"unknown role rejected");
 REQUIRE(roles.m_Value.bits==6,"bad input leaves old roles untouched");
 REQUIRE(roles.FromString(""),"empty role selection clears without false error");
 REQUIRE(roles.m_Value.bits==0,"clear role mask");
 puts("Actual native role parser: valid setters succeed; clear and invalid rollback passed.");return 0;
}
'''


def fields_program():
    ui=(ROOT/'ui/ui_atoms.c').read_text();cg=(ROOT/'cgame/cg_consolecmds.c').read_text()
    actual=function(ui,'UI_OmniNumericProperty')+'\n'+function(cg,'CG_OmniTextCommand')
    # Exact editor prefix of UI_ConsoleCommand; unrelated UI commands are not
    # needed to exercise these typed local VM handlers.
    actual+='\n'+ui[ui.index('qboolean UI_ConsoleCommand( int realTime )'):ui.index('    if(!Q_stricmp(cmd,"tce_mode"))')]+ '\nreturn qfalse;\n}\n'
    return r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <string>
typedef int qboolean;enum{qfalse=0,qtrue=1,EXEC_APPEND=2};
static std::string property,value,sent,generic,uiName,local="1",command;static int argc=2;
struct{struct{int frameTime,realTime;}uiDC;}uiInfo;
struct uiClientState_t{int connState;};enum{CA_ACTIVE=8,KEYCATCH_UI=2};
#define Q_stricmp _stricmp
static void trap_Argv(int,char*dst,int size){strncpy(dst,property.c_str(),size-1);dst[size-1]=0;}
static int trap_Argc(){return argc;}
static void trap_Cvar_VariableStringBuffer(const char*key,char*dst,int size){
 const std::string&s=!strcmp(key,"sv_running")?local:!strcmp(key,"cg_genericText")?generic:!strcmp(key,"ui_omnibotGoalName")?uiName:value;
 strncpy(dst,s.c_str(),size-1);dst[size-1]=0;
}
static const char*va(const char*fmt,...){static char text[1024];va_list args;va_start(args,fmt);vsnprintf(text,sizeof(text),fmt,args);va_end(args);return text;}
static void trap_Cmd_ExecuteText(int,const char*text){sent=text;}
static void trap_SendConsoleCommand(const char*text){sent=text;}
static void trap_Cvar_Set(const char*key,const char*text){if(!strcmp(key,"ui_omnibotGoalName"))uiName=text;}
static void CG_Printf(const char*){}
static char*UI_Argv(int){return const_cast<char*>(command.c_str());}
static float trap_Cvar_VariableValue(const char*){return (float)atof(local.c_str());}
static void trap_Print(const char*){}
static void trap_GetClientState(uiClientState_t*s){s->connState=CA_ACTIVE;}
static void Menus_CloseAll(){}
static int Menus_ActivateByName(const char*,int){return 1;}
static void trap_Key_SetCatcher(int){}
''' + actual + r'''
#define REQUIRE(x,n) if(!(x)){fprintf(stderr,"%s\n",n);return 12;}
int main(){
 property="timeout";value="1500";REQUIRE(UI_OmniNumericProperty(),"valid timeout");
 REQUIRE(sent=="bot goal_setproperty timeout 1500\n","exact bounded command");
 for(const char*bad:{"5001","1.5","nan","-1","1500;quit","1e3","9999999999999999999999"}){
  value=bad;sent.clear();REQUIRE(!UI_OmniNumericProperty(),"invalid timeout");REQUIRE(sent.empty(),"invalid input never queued");
 }
 property="radius";value="35.5";REQUIRE(UI_OmniNumericProperty(),"fractional radius allowed");
 property="quit";value="1";sent.clear();REQUIRE(!UI_OmniNumericProperty(),"unknown property");REQUIRE(sent.empty(),"unknown property not queued");
 generic="FrontDoor";local="0";sent.clear();CG_OmniTextCommand("omnibot_goalname");
 REQUIRE(sent.empty()&&uiName.empty(),"remote OB_DMS alias cannot mutate local field");
 local="1";CG_OmniTextCommand("omnibot_goalname");
 REQUIRE(sent=="omnibot_goalname\n"&&uiName=="FrontDoor","OB_DMS text copied to typed UI command");
 generic="name;quit";CG_OmniTextCommand("omnibot_goalgroup");
 REQUIRE(sent=="omnibot_goalgroup\n","text never interpolated into alias console command");
 command="omnibot_goalgroup";sent.clear();REQUIRE(UI_ConsoleCommand(10),"UI handled invalid legacy field");
 REQUIRE(sent.empty(),"invalid legacy field rejected before bot command");
 uiName="FrontDoor";command="omnibot_goalname";UI_ConsoleCommand(11);
 REQUIRE(sent=="bot goal_setproperty tagname FrontDoor\n","real UI goal rename handler");
 command="omnibot_goalgroup";UI_ConsoleCommand(12);
 REQUIRE(sent=="bot goal_setproperty group FrontDoor\n","real UI group handler");
 uiName="";UI_ConsoleCommand(12);REQUIRE(sent=="bot goal_setproperty group \n","empty group clears native group");
 command="omnibot_goalname";sent.clear();UI_ConsoleCommand(12);REQUIRE(sent.empty(),"empty goal name rejected");
 command="omnibot_waypointname";uiName="Front Door Switch";UI_ConsoleCommand(13);
 REQUIRE(sent=="bot waypoint_setname Front Door Switch\n","real UI waypoint name preserves spaces");
 local="0";sent.clear();UI_ConsoleCommand(14);REQUIRE(sent.empty(),"remote UI field blocked");
 puts("Actual native DMS fields: bounded numeric parser and local legacy aliases passed.");return 0;
}
'''


@skipUnless(os.name=='nt' and shutil.which('cmake'),'Windows CMake/MSVC required')
class DmsEditorNativeTests(TestCase):
    def test_role_parser_reports_real_success_and_old_control_fails(self):
        actual=compile_program(self,roles_program());self.assertEqual(actual.returncode,0,actual.stdout+actual.stderr)
        control=compile_program(self,roles_program(True));self.assertEqual(control.returncode,12,control.stdout+control.stderr)
        self.assertIn('valid role must report success',control.stderr)

    def test_actual_numeric_fields_and_legacy_local_aliases(self):
        result=compile_program(self,fields_program());self.assertEqual(result.returncode,0,result.stdout+result.stderr)


if __name__=='__main__':main()
