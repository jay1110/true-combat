"""Compile production door queries and FollowPath action functions.

Independent fixtures supply mover states, eye trace hits and walking outcomes.
The real recipe parser, graph search and action state machine run unchanged.
This is an offline regression, not a live mover or whole-map certificate.
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


def body(source, marker):
    begin = source.index(marker)
    opening = source.index('{', begin)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[begin:end]


def compile_run(test, program):
    with tempfile.TemporaryDirectory(prefix='tce-door-unit-') as folder:
        directory = Path(folder)
        (directory / 'main.cpp').write_text(program, encoding='utf-8')
        (directory / 'CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.15)\nproject(DoorTests LANGUAGES CXX)\n'
            'set(CMAKE_CXX_STANDARD 17)\nadd_executable(door-test main.cpp)\n')
        options = dict(capture_output=True, text=True, timeout=90,
                       creationflags=subprocess.CREATE_NO_WINDOW)
        for command in (['cmake', '-S', folder, '-B', str(directory/'build'), '-A', 'Win32'],
                        ['cmake', '--build', str(directory/'build'), '--config', 'Release'],
                        [str(directory/'build/Release/door-test.exe')]):
            result = subprocess.run(command, **options)
            test.assertEqual(result.returncode, 0, result.stdout + result.stderr)


RUNTIME_SHIM = r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdarg>
#include <string>
#include <vector>
#include <map>
#include <list>
#include <sstream>
using String=std::string;using NavFlags=unsigned long long;using obuint32=unsigned;using obint32=int;using obColor=int;
using StringStr=std::ostringstream;
namespace COLOR{constexpr int GREEN=1;}
struct Vector3f {float x,y,z;Vector3f():x(0),y(0),z(0){}Vector3f(float a,float b,float c):x(a),y(b),z(c){}
 Vector3f operator-(const Vector3f&b)const{return{x-b.x,y-b.y,z-b.z};}
 Vector3f operator+(const Vector3f&b)const{return{x+b.x,y+b.y,z+b.z};}Vector3f operator*(float s)const{return{x*s,y*s,z*s};}
 Vector3f Flatten()const{return{x,y,0};}float Normalize(){const float n=Length();if(n>0){x/=n;y/=n;z/=n;}return n;}
 Vector3f AddZ(float d)const{return{x,y,z+d};}float Length()const{return std::sqrt(x*x+y*y+z*z);}static const Vector3f ZERO;};
const Vector3f Vector3f::ZERO{};using Vector3List=std::vector<Vector3f>;
float SquaredLength2d(const Vector3f&a,const Vector3f&b){return(a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y);}
namespace Mathf {float FAbs(float x){return std::fabs(x);}float Sqrt(float x){return std::sqrt(x);}}
enum {F_NAV_DOOR=128,F_NAV_CLIMB=1,F_NAV_JUMP=2,F_NAV_JUMPLOW=4,F_NAV_JUMPGAP=8,F_NAV_TELEPORT=16,
 F_NAV_ELEVATOR=32,F_NAV_SCRIPT=64,F_NAV_CLOSED=256,F_NAV_TEAMONLY=512,F_NAV_CROUCH=1024,F_LNK_CLOSED=1,
 BOT_BUTTON_USE=1,BOT_BUTTON_CROUCH=2,ENT_FLAG_ONGROUND=1,NAVID_WP=1,BOT_DEBUG_FPINFO=1};
enum MoveMode{Run=0,Walk=1};namespace Priority {enum{High=1};}
struct GameEntity {int id;bool IsValid()const{return id>=0;}};
struct obUserData {int value;bool typed=true;bool IsInt()const{return typed;}int GetInt()const{return value;}};
struct PropertyMap {std::map<String,String> p;String GetProperty(const char*n)const{auto it=p.find(n);return it==p.end()?"":it->second;}};
'''


def runtime_program():
    header = (COMMON / 'Path.h').read_text()
    path_source = (COMMON / 'Path.cpp').read_text()
    path = '\n'.join(body(path_source, marker) for marker in
        ('Path::Path()', 'void Path::Clear()', 'Path::PathPoint &Path::AddPt(',
         'bool Path::NextPt()', 'bool Path::GetCurrentPt(', 'bool Path::GetNextPt(', 'bool Path::GetPreviousPt('))
    planner = (COMMON / 'PathPlannerWaypoint.cpp').read_text()
    follow = (COMMON / 'BotBaseStates.cpp').read_text()
    functions = '\n'.join(body(planner, marker) for marker in
        ('Path::TceDoorRecipe PathPlannerWaypoint::ReadDoorRecipe(',
         'bool PathPlannerWaypoint::BuildDoorRoute(', 'void PathPlannerWaypoint::GetPath('))
    actions = '\n'.join(body(follow, marker) for marker in
        ('Vector3f RawWpPos(', 'bool FollowPath::GetAimPosition(', 'void FollowPath::ResetTceDoor()', 'void FollowPath::ResetTceCentreApproach()', 'static int TceDoorStat(',
         'static int TceDoorPointSweep(', 'static void TceDoorPathLog(',
         'bool FollowPath::TceDoorRoute(', 'int FollowPath::UpdateTceDoor('))
    return RUNTIME_SHIM + header + path + r'''
float g_fTopWaypointOffset=72,g_fBottomWaypointOffset=-56,g_fPathLevelOffset=.5f;
template<class T>T MinT(T a,T b){return a<b?a:b;}
template<class T>T MaxT(T a,T b){return a>b?a:b;}
template<class T>T ClampT(T x,T a,T b){return x<a?a:x>b?b:x;}
struct Waypoint {int uid;Vector3f pos;NavFlags flags=0;float radius=35;PropertyMap props;
 struct ConnectionInfo{Waypoint*m_Connection;unsigned m_ConnectionFlags=0;};using ConnectionList=std::list<ConnectionInfo>;
 ConnectionList m_Connections;const PropertyMap&GetPropertyMap()const{return props;}const Vector3f&GetPosition()const{return pos;}
 NavFlags GetNavigationFlags()const{return flags;}bool IsFlagOn(NavFlags f)const{return(flags&f)!=0;}int GetUID()const{return uid;}float GetRadius()const{return radius;}
 unsigned OnPathThrough()const{return 0;}unsigned OnPathThroughParam()const{return 0;}};
struct Steering {Vector3f target;void SetTarget(const Vector3f&p,float=0,int=0,bool=false,float=1){target=p;}};
struct Aimer {int releases=0;void AddAimRequest(int,void*,int){}void ReleaseAimRequest(int){++releases;}};
struct AABB{float m_Mins[3]={-16,-16,-30},m_Maxs[3]={16,16,57};};
struct Client {Vector3f position{0,0,30};bool debug=true,passable=false,callbackAllowed=true,approachBlocked=false,egressBlocked=false;int buttons=0,resets=0,team=1;Steering steer;Aimer aimer;
 bool IsDebugEnabled(int)const{return debug;}
 bool UseValidatedWalkEntries()const{return true;}Vector3f GetPosition()const{return position;}
 bool CanWalkTo(const Vector3f&p,NavFlags=0){if(approachBlocked&&p.x==0)return false;if(egressBlocked&&p.x==256)return false;return p.x<100||p.y>100||passable;}
 Vector3f GetEyePosition(){return position.AddZ(52);}
 int GetTeam()const{return team;}int GetTeamFlag()const{return 2048;}int GetGameID()const{return 1;}
 GameEntity GetGameEntity()const{return{1};}void ReleaseButton(int b){buttons&=~b;}void PressButton(int b){buttons|=b;}
 bool GetNavigationHull(AABB&out,NavFlags){out=AABB{};return true;}
 float NavCallback(NavFlags,Waypoint*,Waypoint*){return callbackAllowed?1.f:0.f;}
 void ResetStuckTime(){++resets;}Steering*GetSteeringSystem(){return&steer;}Aimer*GetAimer(){return&aimer;}
 bool HasEntityFlag(int flag)const{return flag==ENT_FLAG_ONGROUND;}};
class PathPlannerBase{public:virtual int GetPlannerType(){return NAVID_WP;}virtual~PathPlannerBase(){}};
class PathPlannerWaypoint:public PathPlannerBase{public:std::vector<Waypoint*>m_WaypointList,m_Solution;NavFlags m_CallbackFlags=4096;Client*m_Client=nullptr;
 Waypoint*GetWaypointByGUID(int id){for(auto*w:m_WaypointList)if(w->uid==id)return w;return nullptr;}
 static Path::TceDoorRecipe ReadDoorRecipe(const Waypoint*);bool BuildDoorRoute(Client*,int,int,Path&);void GetPath(Path&,int);};
PathPlannerWaypoint planner;
class IGameManager{public:static IGameManager*GetInstance(){static IGameManager m;return&m;}PathPlannerBase*GetNavSystem(){return&planner;}};
struct Engine{GameEntity EntityFromID(int n){return{n};}}engine;Engine*g_EngineFuncs=&engine;
std::map<std::pair<int,String>,int>stats;bool statsTyped=true;int sweepObservation=1;
namespace InterfaceFuncs{obUserData GetEntityStat(GameEntity e,const char*n){if(String(n).find("tce_door_sweep:")==0)return{sweepObservation,statsTyped};auto it=stats.find({e.id,n});return{it==stats.end()?-1:it->second,statsTyped};}}
std::vector<String>logs;namespace EngineFuncs{void ConsoleMessage(const char*s){logs.emplace_back(s);}}
const char*va(const char*format,...){static char b[1024];va_list a;va_start(a,format);vsnprintf(b,sizeof(b),format,a);va_end(a);return b;}
class IGame{public:static int time;static int GetTime(){return time;}};int IGame::time=1000;
struct TceRouteReservation{int owner,team,created,expires;std::vector<unsigned>nodes;};std::vector<TceRouteReservation>tceRoutes;
float Length(const Vector3f&a,const Vector3f&b){return(a-b).Length();}
namespace Utils {float ClosestPtOnLine(const Vector3f&a,const Vector3f&b,const Vector3f&p,Vector3f&out){
 const Vector3f d=b-a,q=p-a;const float n=d.x*d.x+d.y*d.y+d.z*d.z;
 const float t=n?(d.x*q.x+d.y*q.y+d.z*q.z)/n:0;out={a.x+t*d.x,a.y+t*d.y,a.z+t*d.z};return t;}}
class FollowPath{public:Client client;struct{int m_MoveMode=0;}m_Query;Path m_CurrentPath,m_TceDoorSavedPath;
 int m_TceCentreNavId=0,m_TceCentreNextId=0,m_TceCentreAttempts=0,m_TceCentreStart=0,m_TceCentreStillSince=0;
 int m_TceCentreLastProgress=0;float m_TceCentreBestDistance=0;
 Path::PathPoint m_TceDoorEntry,m_TceDoorExit,m_TceDoorUse;int m_TceDoorPhase=0,m_TceDoorCycles=0,m_TceDoorStart=0,m_TceDoorPhaseStart=0,m_TceDoorPulse=0;
 int m_TceDoorWaitState=-1;bool m_TceDoorPassageLogged=false;
 int m_TceDoorEgressStart=0,m_TceDoorSweepWaitUntil=0;Vector3f m_LookAheadPt;
 int m_TceDoorTraceEntity=-1,m_TceDoorTraceUntil=0,m_TceDoorTraceTime=0;
 int m_TceDoorBypassCount=0,m_TceDoorSweepNavId=-1,m_TceDoorSweepResult=-1;
 Client*GetClient(){return&client;}int GetNameHash(){return 9;}void ResetTceDoor();void ResetTceCentreApproach();bool TceDoorRoute(int,int,int);int UpdateTceDoor(const Path::PathPoint&);bool GetAimPosition(Vector3f&);};
''' + functions + actions + r'''
Path::TceDoorRecipe recipe(bool remote){Path::TceDoorRecipe r;r.entity=40;r.useEntity=remote?41:40;r.target=2;r.mode=remote?2:1;r.team=3;
 r.role=2;r.model=7;r.useModel=remote?8:7;r.useNav=remote?4:1;r.entry=1;r.aim={32,0,60};r.marked=true;return r;}
void state(int s,bool remote=false){stats[{40,"tce_nav_model"}]=7;stats[{40,"tce_nav_use_kind"}]=1;
 stats[{40,"tce_door_locked"}]=0;stats[{40,"tce_door_team"}]=3;stats[{40,"tce_door_state"}]=s;
 stats[{41,"tce_nav_model"}]=8;stats[{41,"tce_nav_use_kind"}]=3;stats[{1,"tce_use_identity"}]=remote?41:40;
 stats[{40,"tce_door_open_remaining"}]=-1;stats[{1,"tce_door_speed:0:0"}]=174;
 stats[{40,"tce_door_force"}]=0;
 stats[{40,"tce_use_door:40:7"}]=1;stats[{41,"tce_use_door:40:7"}]=1;}
void setup(FollowPath&f,bool remote){f.m_CurrentPath.AddPt({0,0,38},35).Flags(F_NAV_DOOR).NavId(1).TceDoor(recipe(remote));
 f.m_CurrentPath.AddPt({128,0,38},35).NavId(2);f.m_CurrentPath.AddPt({256,0,38},35).NavId(5);state(0,remote);}
int tick(FollowPath&f){Path::PathPoint p;assert(f.m_CurrentPath.GetCurrentPt(p));return f.UpdateTceDoor(p);}
void finishRoute(FollowPath&f){while(f.m_CurrentPath.NextPt()){}Path::PathPoint p;f.m_CurrentPath.GetCurrentPt(p);f.client.position=RawWpPos(p);}
int main(){
 {const auto previousLogs=logs.size();
  for(int clear=-1;clear<=1;clear++){FollowPath forced;setup(forced,false);stats[{40,"tce_door_force"}]=1;
   sweepObservation=clear;assert(tick(forced)==1&&forced.m_TceDoorPhase==2);
   const int result=tick(forced);assert(result==(clear==1?1:-1));
   assert(bool(forced.client.buttons&BOT_BUTTON_USE)==(clear==1));
  }
  FollowPath unknownForce;setup(unknownForce,false);stats[{40,"tce_door_force"}]=-1;
  assert(tick(unknownForce)==1&&tick(unknownForce)==-1&&!(unknownForce.client.buttons&BOT_BUTTON_USE));
  sweepObservation=1;logs.resize(previousLogs);
 }
 Waypoint a{1,{0,0,30}},exit{2,{128,0,30}},bend{3,{0,128,30}},button{4,{128,128,30}};
 a.props.p={{"tce_door_entity","40"},{"tce_door_use_entity","41"},{"tce_door_target","2"},
 {"tce_door_mode","button"},{"tce_door_team","3"},{"tce_door_role","entry"},{"tce_door_model","7"},
 {"tce_door_use_model","8"},{"tce_door_use_nav","4"},{"tce_door_entry","1"},{"tce_door_aim","32 0 60"}};
 assert(PathPlannerWaypoint::ReadDoorRecipe(&a).Valid());a.props.p["tce_door_target"]="0";
 assert(!PathPlannerWaypoint::ReadDoorRecipe(&a).Valid());a.props.p["tce_door_role"]="exit";
 assert(PathPlannerWaypoint::ReadDoorRecipe(&a).Valid());a.props.p["tce_door_role"]="entry";a.props.p["tce_door_target"]="2";
 a.props.p["tce_door_aim"]="nan 0 60";
 assert(!PathPlannerWaypoint::ReadDoorRecipe(&a).Valid());a.props.p["tce_door_aim"]="32 0 60 extra";
 assert(!PathPlannerWaypoint::ReadDoorRecipe(&a).Valid());a.props.p["tce_door_aim"]="32 0 60";
 a.m_Connections={{&exit},{&bend}};exit.m_Connections={{&button}};bend.m_Connections={{&a},{&button}};button.m_Connections={{&bend}};
 planner.m_WaypointList={&a,&exit,&bend,&button};
 Path p;Client c;assert(planner.BuildDoorRoute(&c,1,4,p)&&p.GetNumPts()==3);
 c.debug=false;const auto quiet=logs.size();TceDoorPathLog("QUIET",&c,p);assert(logs.size()==quiet);c.debug=true;
 a.flags=F_NAV_DOOR;planner.m_Client=&c; // actual directed GetPath mapping
 planner.m_Solution={&exit,&a};Path mapped;planner.GetPath(mapped,0);
 assert(mapped.GetPt(0)->m_TceDoor.Valid()&&mapped.GetPt(0)->m_TceDoor.role==2&&mapped.GetPt(0)->m_TceDoor.target==2);
 assert(mapped.GetPt(0)->m_NavFlags&F_NAV_DOOR);assert(RawWpPos(*mapped.GetPt(0)).z==30);
 exit.props=a.props;exit.props.p["tce_door_target"]="1";exit.props.p["tce_door_entry"]="2";exit.flags=F_NAV_DOOR;
 planner.m_Solution={&a,&exit};mapped.Clear();planner.GetPath(mapped,0);
 assert(mapped.GetPt(0)->m_TceDoor.role==2&&mapped.GetPt(0)->m_TceDoor.target==1);
 assert(mapped.GetPt(1)->m_TceDoor.role==0&&!(mapped.GetPt(1)->m_NavFlags&F_NAV_DOOR));
 planner.m_Solution={&bend,&a};mapped.Clear();planner.GetPath(mapped,0);
 assert(mapped.GetPt(0)->m_TceDoor.role==0&&!(mapped.GetPt(0)->m_NavFlags&F_NAV_DOOR));
 exit.props.p.clear();exit.flags=0;
 assert(p.GetPt(1)->m_NavId==3);bend.flags=F_NAV_CLIMB;assert(!planner.BuildDoorRoute(&c,1,4,p));bend.flags=0;
 c.callbackAllowed=false;assert(planner.BuildDoorRoute(&c,1,4,p)); // actual ET normal callback result is zero
 bend.flags=4096;assert(!planner.BuildDoorRoute(&c,1,4,p));bend.flags=0;c.callbackAllowed=true;
 bend.m_Connections.front().m_ConnectionFlags=F_LNK_CLOSED;
 assert(!planner.BuildDoorRoute(&c,4,1,p));bend.m_Connections.front().m_ConnectionFlags=0;
 static FollowPath direct;setup(direct,false);assert(tick(direct)==1&&direct.m_TceDoorPhase==2);
 stats[{1,"tce_use_identity"}]=41;++IGame::time;assert(tick(direct)==1&&!(direct.client.buttons&BOT_BUTTON_USE));
 stats[{1,"tce_use_identity"}]=40;IGame::time+=500;assert(tick(direct)==1&&(direct.client.buttons&BOT_BUTTON_USE));
 ++IGame::time;assert(tick(direct)==1&&!(direct.client.buttons&BOT_BUTTON_USE));state(1);assert(tick(direct)==1);assert(tick(direct)==1);
 state(2);assert(tick(direct)==1&&direct.m_TceDoorPhase==4);assert(tick(direct)==1);
 direct.client.passable=true;assert(tick(direct)==0);assert(direct.m_CurrentPath.GetCurrentPtIndex()==0);
 Vector3f aim;direct.client.position={5,1,30};assert(direct.GetAimPosition(aim)&&aim.x>105&&aim.z==82);
 direct.m_CurrentPath.NextPt();assert(tick(direct)==1);direct.client.position={128,0,30};assert(tick(direct)==1&&direct.m_TceDoorPhase==0&&direct.client.aimer.releases==1);
 assert(direct.m_CurrentPath.GetCurrentPtIndex()==2&&direct.client.steer.target.x==256);
 direct.m_CurrentPath.NextPt();assert(tick(direct)==0);
 static FollowPath egress;setup(egress,false);egress.client.passable=true;state(2);assert(tick(egress)==0);egress.m_CurrentPath.NextPt();
 egress.client.position={128,0,30};egress.client.egressBlocked=true;assert(tick(egress)==1&&egress.m_TceDoorEgressStart>0);
 state(0);assert(tick(egress)==1);egress.client.egressBlocked=false;
 assert(tick(egress)==1&&egress.m_TceDoorPhase==0); // native closed door is irrelevant after physical crossing
 static FollowPath approach;setup(approach,false);approach.client.position={-16,0,30};approach.client.approachBlocked=true;state(2);
 assert(tick(approach)==1&&approach.m_TceDoorPhase==0);state(0);assert(tick(approach)==-1);
 state(2);IGame::time+=10001;assert(tick(approach)==-1);
 static FollowPath pushed;setup(pushed,true);pushed.client.passable=true;state(2,true);assert(tick(pushed)==0);
 pushed.m_CurrentPath.NextPt();pushed.client.position={1,0,30};pushed.client.passable=false;state(0,true);
 assert(tick(pushed)==1&&pushed.m_TceDoorPhase==1&&pushed.m_TceDoorCycles==1);
 assert(pushed.m_CurrentPath.GetCurrentPtIndex()==0&&pushed.m_CurrentPath.GetPt(0)->m_NavId==1);
 static FollowPath outside;setup(outside,false);outside.client.passable=true;state(2);assert(tick(outside)==0);
 outside.m_CurrentPath.NextPt();outside.client.position={120,0,30};state(0);
 assert(tick(outside)==1&&outside.m_TceDoorPhase==4&&!(outside.client.buttons&BOT_BUTTON_USE));
 outside.client.position={128,0,30};assert(tick(outside)==1&&outside.m_TceDoorPhase==0);
 static FollowPath blocked;setup(blocked,false);stats[{40,"tce_nav_model"}]=99;assert(tick(blocked)==-1);
 setup(blocked,false);stats[{40,"tce_door_locked"}]=1;assert(tick(blocked)==-1);
 static FollowPath wrongTeam;setup(wrongTeam,false);wrongTeam.client.team=2;stats[{40,"tce_door_team"}]=1;assert(tick(wrongTeam)==-1);
 static FollowPath retargeted;setup(retargeted,true);stats[{41,"tce_use_door:40:7"}]=0;assert(tick(retargeted)==-1);
 static FollowPath remote;setup(remote,true);assert(tick(remote)==1&&remote.m_TceDoorPhase==1);
 button.m_Connections.push_back({&exit}); // a real ordinary alternative around this fixture's leaf
 assert(remote.m_CurrentPath.GetNumPts()==3&&remote.m_CurrentPath.GetPt(1)->m_NavId==3);
 finishRoute(remote);assert(tick(remote)==1&&remote.m_TceDoorPhase==2&&(remote.client.buttons&BOT_BUTTON_USE));
 state(2,true);assert(tick(remote)==1&&remote.m_TceDoorPhase==3);finishRoute(remote);
 remote.client.position.x+=1;assert(remote.GetAimPosition(aim)&&aim.x>100&&aim.z==82);
 state(0,true);assert(tick(remote)==1&&remote.m_TceDoorPhase==4);
 assert(remote.m_CurrentPath.GetNumPts()==3&&remote.m_CurrentPath.GetCurrentPtIndex()==0);
 assert(tick(remote)==1&&remote.m_TceDoorPhase==5&&remote.m_TceDoorCycles==1);
 finishRoute(remote);assert(tick(remote)==1&&remote.m_TceDoorPhase==4);
 remote.client.passable=true;assert(tick(remote)==1&&remote.m_TceDoorPhase==0); // timer expiry uses one ordinary bypass
 static FollowPath missing;setup(missing,false);statsTyped=false;assert(tick(missing)==-1);statsTyped=true;
 static FollowPath timeout;setup(timeout,false);assert(tick(timeout)==1);IGame::time+=45001;assert(tick(timeout)==-1);
 int waits=0,passages=0,returns=0,switches=0;for(const auto&line:logs){waits+=line.find("TCE2DOOR WAIT ")!=String::npos;
 passages+=line.find("TCE2DOOR PASSAGE")!=String::npos;returns+=line.find("TCE2DOOR RETURN")!=String::npos;
 switches+=line.find("TCE2DOOR TO_SWITCH")!=String::npos;}
 assert(waits==1&&passages==4&&returns==1&&switches==2);
 // Mode3 keeps a full-sweep-safe local USE separate from the unsafe ENTRY.
 a.props.p["tce_door_mode"]="local";a.props.p["tce_door_use_entity"]="40";
 a.props.p["tce_door_use_model"]="7";a.props.p["tce_door_use_nav"]="4";
 button.pos={-32,0,30};button.props=a.props;button.props.p["tce_door_role"]="use";
 button.props.p["tce_door_target"]="1";button.flags=F_NAV_DOOR;
 a.m_Connections={{&exit},{&button}};button.m_Connections={{&a}};
 assert(PathPlannerWaypoint::ReadDoorRecipe(&a).Valid());
 a.props.p["tce_door_use_entity"]="41";assert(!PathPlannerWaypoint::ReadDoorRecipe(&a).Valid());
 a.props.p["tce_door_use_entity"]="40";a.props.p["tce_door_use_nav"]="1";
 assert(!PathPlannerWaypoint::ReadDoorRecipe(&a).Valid());a.props.p["tce_door_use_nav"]="4";
 auto localSetup=[&](FollowPath&f){state(0);sweepObservation=1;f.client.position=button.pos;f.client.passable=true;
  planner.m_Client=&f.client;planner.m_Solution={&exit,&a,&button};planner.GetPath(f.m_CurrentPath,0);};
 FollowPath local;localSetup(local);
 assert(local.m_CurrentPath.GetPt(0)->m_TceDoor.role==1&&(local.m_CurrentPath.GetPt(0)->m_NavFlags&F_NAV_DOOR));
 assert(tick(local)==1&&local.m_TceDoorPhase==1&&local.client.steer.target.x==-32);
 assert(tick(local)==1&&local.m_TceDoorPhase==2&&(local.client.buttons&BOT_BUTTON_USE));
 state(1);assert(tick(local)==1&&local.m_TceDoorPhase==2&&!(local.client.buttons&BOT_BUTTON_USE));
 assert(local.client.steer.target.x==-32);
 state(2);stats[{40,"tce_door_open_remaining"}]=100;
 assert(tick(local)==1&&local.m_TceDoorPhase==2&&local.client.steer.target.x==-32);
 stats[{40,"tce_door_open_remaining"}]=-1;
 assert(tick(local)==1&&local.m_TceDoorPhase==3);
 assert(local.m_CurrentPath.GetNumPts()==2&&local.m_CurrentPath.GetPt(0)->m_NavId==4);
 finishRoute(local);assert(tick(local)==1&&local.m_TceDoorPhase==4&&local.m_CurrentPath.GetCurrentPtIndex()==1);
 assert(tick(local)==0);local.m_CurrentPath.NextPt();local.client.position=exit.pos;
 assert(tick(local)==0&&local.m_TceDoorPhase==0);
 FollowPath localSweep;localSetup(localSweep);sweepObservation=0;
 assert(tick(localSweep)==-1&&!(localSweep.client.buttons&BOT_BUTTON_USE));
 FollowPath localIdentity;localSetup(localIdentity);assert(tick(localIdentity)==1);
 stats[{40,"tce_nav_use_kind"}]=3;assert(tick(localIdentity)==-1);
 FollowPath localClosing;localSetup(localClosing);assert(tick(localClosing)==1&&tick(localClosing)==1);
 state(2);assert(tick(localClosing)==1&&localClosing.m_TceDoorPhase==3);
 finishRoute(localClosing);state(3);assert(tick(localClosing)==1&&localClosing.m_TceDoorPhase==1);
 assert(localClosing.client.steer.target.x==-32&&localClosing.m_TceDoorCycles==1);
 FollowPath localReplan;localSetup(localReplan);localReplan.m_CurrentPath.NextPt();localReplan.client.position=a.pos;
 assert(tick(localReplan)==1&&localReplan.m_TceDoorPhase==1&&localReplan.client.steer.target.x==-32);
 planner.m_Client=&local.client;planner.m_Solution={&a,&button};mapped.Clear();planner.GetPath(mapped,0);
 assert(mapped.GetPt(0)->m_TceDoor.role==0&&!(mapped.GetPt(0)->m_NavFlags&F_NAV_DOOR));

 printf("real door recipe, ordinary graph detour, exact USE identity, release frames, collision gate, return, timer retry, lock/team/model and timeout fixtures passed\n");
}
'''


QUERY_SHIM = r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cstring>
#include <cmath>
#include <cstdio>
enum qboolean{qfalse=0,qtrue=1};using vec3_t=float[3];constexpr int MAX_GENTITIES=64,ENTITYNUM_WORLD=63;
enum{STATE_INVISIBLE=1,STATE_UNDERCONSTRUCTION=2,TR_STATIONARY=0,TEAM_SPECTATOR=3,EF_MOUNTEDTANK=4,
 MOVER_POS1=0,MOVER_1TO2=1,MOVER_POS2=2,MOVER_2TO1=3,MOVER_POS1ROTATE=4,MOVER_1TO2ROTATE=5,MOVER_POS2ROTATE=6,MOVER_2TO1ROTATE=7,
 CONTENTS_SOLID=1,CONTENTS_MISSILECLIP=2,CONTENTS_BODY=4,CONTENTS_CORPSE=8,CONTENTS_TRIGGER=16,SURF_NOIMPACT=1,ET_ITEM=1,ET_ALARMBOX=2,ET_MOVER=3};
struct Traj{int trType=0;};struct State{int weapon=0,number=0,eFlags=0,eType=0,modelindex=1;Traj apos,pos;};
enum{PMF_DUCKED=1,BUTTON_WALKING=0x10,STAT_TCE_WEAPON_FLAGS=0,TCE_MAX_WEAPONS=64};
struct playerState_t{int eFlags=0,pm_flags=0,speed=300,weapon=0,stats[16]={},holdable[16]={};
 float runSpeedScale=.55f,sprintSpeedScale=1.1f,crouchSpeedScale=.3f;
 float origin[3]={1,2,30},viewangles[3]={0,0,0},viewheight=52,leanf=0;};
struct Client{struct{int sessionTeam=1;}sess;playerState_t ps;struct{int sprintTime=0;}pmext;};
struct{int integer=7;}g_gametype;struct{int integer=127;}g_movespeed;
struct{int loadoutWeight=4;}weaponDef[TCE_MAX_WEAPONS];
struct gentity_t{int inuse=1,entstate=0,spawnflags=0,key=0,moverState=MOVER_POS1,allowteams=0,nextthink=0,active=0,health=100;
 float wait=0;void*use=(void*)1;const char*classname="func_door";const char*scriptName=nullptr;State s;
 struct{int linked=1;}r;gentity_t*teammaster=nullptr,*teamchain=nullptr;Client*client=nullptr;};
gentity_t g_entities[MAX_GENTITIES];struct{int time=1000,num_entities=63;}level;
struct trace_t{int entityNum=ENTITYNUM_WORLD,surfaceFlags=0;float fraction=1;};trace_t traces[2];int calls=0;float lastEye[3],lastEnd[3];
int Q_stricmp(const char*a,const char*b){return std::strcmp(a,b);}
qboolean G_AllowTeamsAllowed(gentity_t*e,gentity_t*a){return !e->allowteams||(e->allowteams&a->client->sess.sessionTeam)?qtrue:qfalse;}
void AngleVectors(const vec3_t,float*f,float*r,float*u){f[0]=1;f[1]=f[2]=0;r[0]=r[2]=0;r[1]=1;u[0]=u[1]=0;u[2]=1;}
void VectorCopy(const float*a,float*b){memcpy(b,a,sizeof(vec3_t));}
void VectorMA(const float*a,float n,const float*b,float*c){for(int i=0;i<3;++i)c[i]=a[i]+n*b[i];}
void trap_Trace(trace_t*out,const float*a,void*,void*,const float*b,int,int){memcpy(lastEye,a,sizeof(vec3_t));memcpy(lastEnd,b,sizeof(vec3_t));*out=traces[calls<1?0:1];++calls;}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class NativeDoorRuntimeTests(TestCase):
    def test_real_followpath_action_and_real_graph_search(self):
        compile_run(self, runtime_program())
        follow = (COMMON / 'BotBaseStates.cpp').read_text()
        update = body(follow, 'State::StateStatus FollowPath::Update(')
        self.assertLess(update.index('UpdateTceDoor(pt)'), update.index('NotifyUserSuccess()'))
        self.assertIn('!pt.m_TceDoor.marked', update)
        self.assertIn('GetClient()->CanWalkTo(RawWpPos(m_TceDoorExit)', follow)

    def test_read_only_native_mover_and_eye_trace_queries(self):
        helper = (ROOT / 'game/etbot/tce_nav_door_query.h').read_text()
        movement = (ROOT/'game/tce_movement.h').read_text() + (ROOT/'game/tce_movement.c').read_text().replace('#include "tce_movement.h"', '')
        compile_run(self, QUERY_SHIM + movement + helper + r'''
int main(){Client client;gentity_t&actor=g_entities[1];actor.client=&client;actor.s.number=1;
 assert(TceNavDoorForwardSpeed(&actor,0,0)==165);assert(TceNavDoorForwardSpeed(&actor,1,0)==90);
 assert(TceNavDoorForwardSpeed(&actor,0,1)==137);client.ps.stats[0]=4;
 assert(TceNavDoorForwardSpeed(&actor,0,0)==90);client.ps.stats[0]=0;
 gentity_t&door=g_entities[20];door.s.modelindex=7;door.teammaster=&door;gentity_t&leaf=g_entities[21];
 door.teamchain=&leaf;leaf.teammaster=&door;leaf.s.modelindex=8;leaf.allowteams=2;
 assert(TceNavDoorGroupStat(&door,2)==2);assert(TceNavDoorGroupStat(&door,0)==0);
 assert(TceNavDoorGroupStat(&door,4)==0);door.classname="func_door_rotating";door.moverState=MOVER_POS1ROTATE;
 door.spawnflags=32;assert(TceNavDoorGroupStat(&door,4)==1);
 door.spawnflags=0;leaf.spawnflags=32;assert(TceNavDoorGroupStat(&door,4)==0);leaf.spawnflags=0;
 door.classname="func_door";door.moverState=MOVER_POS1;
 door.moverState=leaf.moverState=MOVER_POS2;door.nextthink=1200;leaf.nextthink=1500;
 assert(TceNavDoorGroupStat(&door,0)==2&&TceNavDoorGroupStat(&door,3)==200);
 leaf.moverState=MOVER_2TO1;assert(TceNavDoorGroupStat(&door,0)==3);leaf.key=-1;
 assert(TceNavDoorGroupStat(&door,1)==1);leaf.key=0;leaf.allowteams=0;door.moverState=leaf.moverState=MOVER_POS1;
 door.spawnflags=1;assert(TceNavDoorSingleState(&door)==-1);door.classname="func_door_rotating";door.moverState=MOVER_POS1ROTATE;
 assert(TceNavDoorSingleState(&door)==0&&!TceNavUseReady(&actor,&door,.1f));door.spawnflags=0;door.classname="func_door";door.moverState=MOVER_POS1;
 traces[0]={20,0,.39f};calls=0;client.ps.leanf=3;gentity_t before[MAX_GENTITIES];memcpy(before,g_entities,sizeof(before));
 assert(TceNavUseIdentity(&actor)==20&&calls==1);assert(lastEye[2]==82&&lastEye[1]==5&&lastEnd[0]-lastEye[0]==96);
 assert(memcmp(before,g_entities,sizeof(before))==0);traces[0].fraction=.41f;traces[1]=traces[0];calls=0;assert(TceNavUseIdentity(&actor)==-1);
 gentity_t&button=g_entities[22];button.classname="func_button";traces[0]={ENTITYNUM_WORLD,0,1};traces[1]={22,0,.8f};calls=0;
 assert(TceNavUseIdentity(&actor)==22&&calls==2);button.active=1;calls=0;assert(TceNavUseIdentity(&actor)==-1);button.active=0;
 traces[0]={23,0,.2f};g_entities[23].classname="item_weapon";g_entities[23].s.eType=ET_ITEM;calls=0;assert(TceNavUseIdentity(&actor)==-1&&calls==1);
 button.classname="func_invisible_user";button.wait=999;traces[0]={22,0,.8f};calls=0;assert(TceNavUseIdentity(&actor)==22);
 button.wait=1000;calls=0;assert(TceNavUseIdentity(&actor)==-1);button.wait=999;button.scriptName="script_switch";
 calls=0;assert(TceNavUseIdentity(&actor)==-1);button.scriptName=nullptr;actor.s.weapon=60;calls=0;assert(TceNavUseIdentity(&actor)==-1&&calls==0);
 actor.s.weapon=0;actor.health=0;assert(TceNavUseIdentity(&actor)==-1);actor.health=100;
 door.s.eType=ET_MOVER;door.spawnflags=128;assert(!TceNavUseReady(&actor,&door,.1f));
 door.r.linked=0;assert(TceNavDoorSingleState(&door)==-1);
 printf("real native mover-group, team/key, rotating remote-only, exact eye range/lean, button readiness and read-only fixtures passed\n");
}
''')
        self.assertNotRegex(helper, r'\b(?:G_TryDoor|G_UseTargets|Use_BinaryMover|G_UseEntity|G_Script_ScriptEvent|trap_LinkEntity)\s*\(')


if __name__ == '__main__':
    main()
