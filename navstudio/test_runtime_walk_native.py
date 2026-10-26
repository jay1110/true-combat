"""Compile actual Omni Client::CanWalkTo against independent sweep fixtures.

The fixtures supply collisions; the production walk, grounding and acceptance
code runs unchanged. No game instance, ABI stubs pretending API success, or
manual waypoint edits are used. These are synthetic regressions, not live ET
collider/whole-map certification.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless


ROOT = (Path(__file__).resolve().parents[1] / "src")
CLIENT = ROOT.parent / 'vendor/omni-bot/source/Omnibot/Common/Client.cpp'
SHIM = r'''
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
struct Vector3f {
 float x,y,z;Vector3f():x(0),y(0),z(0){}Vector3f(float a,float b,float c):x(a),y(b),z(c){}
 Vector3f(const float *p):x(p[0]),y(p[1]),z(p[2]){}
 Vector3f operator+(const Vector3f&b)const{return{x+b.x,y+b.y,z+b.z};}
 Vector3f operator-(const Vector3f&b)const{return{x-b.x,y-b.y,z-b.z};}
 Vector3f operator*(float t)const{return{x*t,y*t,z*t};}
 float SquaredLength()const{return x*x+y*y+z*z;}float Length()const{return std::sqrt(SquaredLength());}
 float Dot(const Vector3f&b)const{return x*b.x+y*b.y+z*b.z;}
 Vector3f AddZ(float v)const{return{x,y,z+v};}
};
float SquaredLength(const Vector3f&a,const Vector3f&b){return(a-b).SquaredLength();}
namespace Mathf{float Sqrt(float x){return std::sqrt(x);}float FAbs(float x){return std::fabs(x);}}
template<class T>T MaxT(T a,T b){return a>b?a:b;}
using NavFlags=unsigned long long;
struct AABB{float m_Mins[3]={-16,-16,-30},m_Maxs[3]={16,16,57};};
struct obTraceResult{float m_Fraction=1,m_Normal[3]={0,0,1},m_Endpos[3]={};int m_StartSolid=0,m_Surface=0;};
enum{CONT_SLIME=1,CONT_LAVA=2,CONT_WATER=4,SURFACE_SLICK=1,TR_MASK_SOLID=2,
 TR_MASK_PLAYERCLIP=4,TR_MASK_NAVCAPSULE=8,BOT_DEBUG_PLANNER=1,False=0,ENT_FLAG_ONGROUND=1};
int scenario,queries,bodyqueries,boxqueries;
struct Engine{int GetPointContents(const Vector3f&p){
 float depth=scenario==5?100.f:(scenario==7||scenario==8||scenario==10)?32.f:
  scenario==9?42.f:scenario==11?23.f:scenario==12?20.f:0.f;
 return depth>0.f&&p.z<depth?CONT_WATER:0;
}}engine;
Engine*g_EngineFuncs=&engine;
const char*va(const char*format,...){static char b[2048];va_list a;va_start(a,format);vsnprintf(b,sizeof(b),format,a);va_end(a);return b;}
namespace EngineFuncs {
void ConsoleMessage(const char*text){printf("%s\n",text);}
bool TraceLine(obTraceResult&r,const Vector3f&a,const Vector3f&b,const AABB*h,int mask,int,int){
 ++queries;if(mask&TR_MASK_NAVCAPSULE)++bodyqueries;else ++boxqueries;
 r=obTraceResult{};r.m_Surface=scenario==1||scenario==6?SURFACE_SLICK:0;
 const float lower=h?h->m_Mins[2]:0;
 const float slope=scenario==6?.142492f:0;
 const float normal=1/std::sqrt(1+slope*slope);
 r.m_Normal[0]=-slope*normal;r.m_Normal[2]=normal;
 for(int i=0;i<3;i++)r.m_Endpos[i]=(&b.x)[i];
 auto floor=[&](float x){return slope*x;};
 if(a.z+lower<floor(a.x)-.001f){r.m_StartSolid=1;r.m_Fraction=0;return true;}
 const float fa=a.z+lower-floor(a.x),fb=b.z+lower-floor(b.x);
 if(fb<0&&fb<fa){
  const float t=fa/(fa-fb),x=a.x+t*(b.x-a.x);
  if(!(scenario==3&&x>40&&x<88)&&scenario!=4){r.m_Fraction=t;}
 }
 if(scenario==2&&a.x<=40&&b.x>40){r.m_Fraction=(40-a.x)/(b.x-a.x);r.m_Normal[0]=-1;r.m_Normal[2]=0;}
 for(int i=0;i<3;i++)r.m_Endpos[i]=(&a.x)[i]+r.m_Fraction*((&b.x)[i]-(&a.x)[i]);
 return true;
}
}
class Client {
public:bool validated=true;Vector3f position{0,0,30.125f};
 bool GetNavigationHull(AABB&out,NavFlags)const{
  out=AABB{};
  if(scenario==8)out.m_Maxs[2]=31.f;
  if(scenario>=10){out.m_Mins[2]=-24.f;out.m_Maxs[2]=scenario==11||scenario==12?25.f:46.f;}
  return true;
 }
 bool UseValidatedWalkEntries()const{return validated;}float GetStepHeight()const{return 8;}
 bool HasEntityFlag(int flag)const{return flag==ENT_FLAG_ONGROUND;}
 Vector3f GetPosition()const{return position;}int GetGameID()const{return 4;}
 bool IsDebugEnabled(int)const{return true;}
 bool CanWalkTo(const Vector3f&,NavFlags=0)const;
 bool TraceWalkTo(const Vector3f&,NavFlags,float,Vector3f*)const;
 bool ResolveWalkDestination(const Vector3f&,float,NavFlags,Vector3f&)const;
};
void require(bool good,const char*name){if(!good){fprintf(stderr,"WALK REGRESSION: %s\n",name);exit(9);}}
#define REQUIRE(good,name) require((good),(name))
'''


def extract(source, name):
    match = re.search(r'^(?:static )?bool\s+(?:Client::)?' + name + r'\([^;]*?\)\s*(?:const)?\s*\{', source, re.M)
    if not match:
        raise AssertionError('Actual function missing: ' + name)
    opening = source.index('{', match.start()); depth = 1; end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}'); end += 1
    return source[match.start():end]


def program():
    source = CLIENT.read_text()
    helpers = [extract(source, name) for name in ('NavigationGroundContentsSafe',
               'NavigationGroundSurfaceSafe', 'TraceWalkTo', 'CanWalkTo', 'ResolveWalkDestination')]
    water = (ROOT/'game/tce_nav_water.h').read_text()
    return SHIM + water + '\n'.join(helpers) + r'''
int main(int argc,char**argv){
 scenario=argc>1?atoi(argv[1]):0;Client c;
 if(scenario>=10)c.position.z=24.125f;
 Vector3f goal(128,0,30.25f);if(scenario==6)goal.z+=128*.142492f;
 if(scenario>=10)goal.z=24.25f;
 const bool actual=c.CanWalkTo(goal);
 printf("fixture %d accepted %d traces %d body %d box %d\n",scenario,(int)actual,queries,bodyqueries,boxqueries);
 REQUIRE(actual==(scenario==0||scenario==1||scenario==7||scenario==10||scenario==12),"actual walk geometry result");
 REQUIRE(bodyqueries>0,"actual complete-body queries executed");
 REQUIRE(NavigationGroundSurfaceSafe(1,SURFACE_SLICK,true),"flat slick is real supported floor");
 REQUIRE(!NavigationGroundSurfaceSafe(.9998f,SURFACE_SLICK,true),"sloped slick requires a separate recipe");
 REQUIRE(!NavigationGroundSurfaceSafe(1,SURFACE_SLICK,false),"original ET policy remains unchanged");
 REQUIRE(!NavigationGroundSurfaceSafe(.69f,0,true),"steep ordinary floor rejects");
 if(scenario==1){c.validated=false;REQUIRE(!c.CanWalkTo(goal),"original ET full walk still rejects slick");}
 return 0;
}
'''


class ObjectiveCommandBindingTests(TestCase):
    def test_status_command_calls_actual_registered_global_and_member_apis(self):
        common = ROOT.parent / 'vendor/omni-bot/source/Omnibot/Common'
        library = (common / 'gmBotLibrary.cpp').read_text()
        globals = set(re.findall(r'\{\s*"(\w+)"\s*,\s*\w+\s*\}', library))
        command = (ROOT.parent / 'vendor/omni-bot/tce2-scripts/tce_commands.gm').read_text().split('Commands["tceobjectives"]', 1)[1]
        calls = set(re.findall(r'(?<![\w.])(\w+)\s*\(', command)) - {'function', 'if', 'foreach', 'print'}
        self.assertFalse(calls-globals, 'Unregistered global API names: ' + str(calls-globals))
        self.assertIn('GetGoals(goals,0,', command)
        self.assertNotIn('GetMapGoals(', command)
        bot = (common / 'gmBot.cpp').read_text(); goal = (common / 'MapGoal.cpp').read_text()
        for method in ('GetGameId', 'GetTeam', 'GetStat'):
            self.assertRegex(bot, r'GMBIND_FUNCTION\(\s*"'+method+r'"')
        for method in ('GetName', 'GetEntity'):
            self.assertIn('"'+method+'"', goal)


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class RuntimeWalkNativeTests(TestCase):
    def test_real_client_walk_accepts_flat_slick_but_not_slope_wall_gap_or_hazard(self):
        with tempfile.TemporaryDirectory(prefix='tce-walk-regression-') as folder:
            directory = Path(folder)
            (directory/'main.cpp').write_text(program())
            (directory/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(RuntimeWalk LANGUAGES CXX)\nadd_executable(runtime-walk main.cpp)\n')
            options = dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake','-S',folder,'-B',str(directory/'build'),'-A','Win32'],
                            ['cmake','--build',str(directory/'build'),'--config','Release']):
                result=subprocess.run(command,**options)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            for number,name in enumerate(('normal floor','flat slick','wall','gap','missing support','deep water','sloped slick',
                        'enlarged stand32 water','enlarged duck32 water','enlarged stand42 water',
                        'normal stand32 water','normal duck23 water','normal duck20 water')):
                with self.subTest(fixture=name):
                    result=subprocess.run([str(directory/'build/Release/runtime-walk.exe'),str(number)],**options)
                    self.assertEqual(result.returncode,0,result.stdout+result.stderr)


if __name__=='__main__':main()
