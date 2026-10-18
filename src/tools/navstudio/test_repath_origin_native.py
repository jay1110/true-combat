"""Actual FollowPath Repath/QueryPath versus raw player-origin destinations.

Planner output is supplied independently. The production path-coordinate and
destination code runs unchanged, including projection, flags and path storage.
This catches the real final-target minus-eight-units floor failure.
"""
from pathlib import Path
import os
import shutil
from unittest import TestCase, main, skipUnless

from test_door_runtime_native import COMMON, RUNTIME_SHIM, body, compile_run


def program():
    path_source = (COMMON / 'Path.cpp').read_text()
    path = '\n'.join(body(path_source, marker) for marker in
        ('Path::Path()', 'void Path::Clear()', 'Path::PathPoint &Path::AddPt(',
         'bool Path::NextPt()', 'void Path::RemoveLastPt()', 'bool Path::GetCurrentPt(',
         'bool Path::GetNextPt(', 'bool Path::GetFirstPt(', 'bool Path::GetLastPt(', 'bool Path::GetPt('))
    follow = (COMMON / 'BotBaseStates.cpp').read_text()
    functions = '\n'.join(body(follow, marker) for marker in
        ('bool FollowPath::Repath()', 'bool FollowPath::QueryPath(', 'Vector3f RawWpPos('))
    return RUNTIME_SHIM + (COMMON/'Path.h').read_text() + path + r'''
enum{F_NAV_PRONE=8192,BOT_DEBUG_PLANNER=1};float g_fTopWaypointOffset=72,g_fBottomWaypointOffset=-56;
float Length(const Vector3f&a,const Vector3f&b){return(a-b).Length();}
template<class T>T MaxT(T a,T b){return a>b?a:b;}template<class T>T MinT(T a,T b){return a<b?a:b;}
namespace Utils {constexpr float FloatMax=1e30f;
 float ClosestPtOnLine(const Vector3f&a,const Vector3f&b,const Vector3f&p,Vector3f&out){
  const Vector3f d=b-a,q=p-a;const float denominator=d.x*d.x+d.y*d.y+d.z*d.z;
  const float t=denominator?(q.x*d.x+q.y*d.y+q.z*d.z)/denominator:0;
  out={a.x+t*d.x,a.y+t*d.y,a.z+t*d.z};return t;}}
struct Destination{Vector3f m_Position;float m_Radius;};using DestinationVector=std::vector<Destination>;
struct User{int m_DestinationIndex=0,resets=0;void ResetPathUser(){++resets;}};
struct MapGoal{String GetName(){return"fixture";}};struct State{MapGoal*GetMapGoalPtr(){return nullptr;}String GetName(){return"fixture";}};
struct HighLevel{State*GetActiveState(){return nullptr;}};
struct Client{HighLevel high;bool IsDebugEnabled(int){return false;}HighLevel*GetHighLevel(){return&high;}
 bool UseValidatedWalkEntries(){return true;}
 Vector3f GetPosition(){return{0,0,30.125f};}const char*GetName(){return"probe";}int GetTeamFlag(){return 1;}void ResetStuckTime(){}};
struct PathPlannerBase {Path path;bool found=true,ordinaryEntry=false;
 int PlanPathToNearest(Client*,const Vector3f&,const DestinationVector&,int){return 0;}bool FoundGoal(){return found;}
 int PlanPathToNearestWithEntryPolicy(Client*c,const Vector3f&p,const DestinationVector&d,int team,bool ordinary){ordinaryEntry=ordinary;return PlanPathToNearest(c,p,d,team);}
 void GetPath(Path&out){out=path;}}planner;
struct IGameManager{static IGameManager*GetInstance(){static IGameManager m;return&m;}PathPlannerBase*GetNavSystem(){return&planner;}};
struct IGame{static float GetTimeSecs(){return 1;}};
namespace EngineFuncs{void ConsoleMessage(const char*){}}
const char*va(const char*f,...){return f;}void MapDebugPrint(const char*){}
struct FollowPathUser{enum{NoPath=1};};
class FollowPath{public:enum{PathInProgress=0,PathFinished=1,PathNotFound=2};
 struct{User*m_User=nullptr;DestinationVector m_Destination;bool m_SkipLastPt=false;}m_Query;
 Client client;Path m_CurrentPath;int m_PathStatus=PathFinished,m_PathThroughPtIndex=0,m_PassThroughState=0,failures=0,doorResets=0;
 bool m_IgnorePathNotFound=true;Client*GetClient(){return&client;}void ResetTceDoor(){++doorResets;}
 bool m_TceReentryPlanning=false;
 void ResetTceFreeClimb(){} void ResetTceStandingJump(){} void ResetTceCentreApproach(){}
 void NotifyUserFailed(int){++failures;}bool Repath();bool QueryPath(Path&,const DestinationVector&,bool,float);};
''' + functions + r'''
void plannerPath(){planner.path.Clear();planner.path.AddPt({0,0,38},12).NavId(1);
 planner.path.AddPt({128,0,38},12).NavId(2).Flags(F_NAV_CROUCH);planner.found=true;}
int main(){User user;FollowPath f;f.m_Query.m_User=&user;f.m_Query.m_Destination={{{128,0,30.25f},4}};plannerPath();
 assert(f.Repath()&&f.failures==0&&f.m_CurrentPath.GetNumPts()==3);
 assert(!planner.ordinaryEntry); // Ordinary Goto retains normal start semantics.
 const Path::PathPoint*last=f.m_CurrentPath.GetPt(2);assert(last->m_IsPlayerOrigin&&last->m_NavId==0);
 assert(RawWpPos(*last).z==30.25f&&last->m_NavFlags==F_NAV_CROUCH&&last->m_TceDestinationVolume);
 assert(RawWpPos(*f.m_CurrentPath.GetPt(1)).z==30.f);
 Path::PathPoint legacy=*last;legacy.PlayerOrigin(false);assert(RawWpPos(legacy).z==22.25f); // real former below-floor coordinate
 f.m_Query.m_Destination={{{105,4,30.25f},5}};plannerPath();assert(f.Repath());
 assert(f.m_CurrentPath.GetNumPts()==3);const Path::PathPoint*projected=f.m_CurrentPath.GetPt(1);
 assert(std::fabs(projected->m_Pt.x-105.f)<.01f&&RawWpPos(*projected).z==30.f&&!projected->m_IsPlayerOrigin);
 assert(RawWpPos(*f.m_CurrentPath.GetPt(2)).z==30.25f);
 const int activeCount=f.m_CurrentPath.GetNumPts();const float activeX=f.m_CurrentPath.GetPt(2)->m_Pt.x;
 Path queried;DestinationVector goals={{{256,0,30.25f},8}};plannerPath();f.m_Query.m_SkipLastPt=true;
 assert(f.QueryPath(queried,goals,false,Utils::FloatMax)&&queried.GetNumPts()==3);
 assert(queried.GetPt(2)->m_IsPlayerOrigin&&RawWpPos(*queried.GetPt(2)).z==30.25f);
 assert(f.m_CurrentPath.GetNumPts()==activeCount&&f.m_CurrentPath.GetPt(2)->m_Pt.x==activeX);
 f.m_Query.m_SkipLastPt=false;assert(f.QueryPath(queried,goals,true,Utils::FloatMax)&&queried.GetNumPts()==2);
 f.m_Query.m_SkipLastPt=true;assert(f.Repath()&&f.m_CurrentPath.GetNumPts()==2);
 planner.path.Clear();f.m_Query.m_SkipLastPt=false;assert(f.Repath()&&f.m_CurrentPath.GetNumPts()==1&&f.m_CurrentPath.GetPt(0)->m_IsPlayerOrigin);
 planner.found=false;assert(!f.Repath()&&f.failures==1);assert(!f.QueryPath(queried,goals,false,Utils::FloatMax));
 Path reused;reused.AddPt({1,2,30},3).PlayerOrigin();reused.Clear();assert(!reused.AddPt({0,0,38},12).m_IsPlayerOrigin);
 printf("actual Repath/QueryPath/RawWpPos destination, projection, posture, independent output, skip argument, empty/failure and reused-storage fixtures passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class RepathOriginTests(TestCase):
    def test_actual_repath_and_query_destinations_do_not_move_below_the_floor(self):
        compile_run(self, program())

    def test_raw_destination_runs_the_actual_complete_body_floor_gate(self):
        from test_runtime_walk_native import SHIM, CLIENT, extract, ROOT
        shim = SHIM.replace('struct Vector3f {', 'struct Vector3f { static const Vector3f ZERO;')
        source = CLIENT.read_text()
        helpers = '\n'.join(extract(source, name) for name in
                            ('NavigationGroundContentsSafe', 'NavigationGroundSurfaceSafe', 'TraceWalkTo', 'CanWalkTo', 'ResolveWalkDestination'))
        compile_run(self, shim + r'''
#include <vector>
using obuint32=unsigned;using obColor=int;using Vector3List=std::vector<Vector3f>;
namespace COLOR{constexpr int GREEN=1;}const Vector3f Vector3f::ZERO{};
float g_fTopWaypointOffset=72,g_fBottomWaypointOffset=-56;
''' + (COMMON/'Path.h').read_text() + (ROOT/'game/tce_nav_water.h').read_text() +
                    helpers + body((COMMON/'BotBaseStates.cpp').read_text(), 'Vector3f RawWpPos(') + r'''
int main(){scenario=0;Client client;client.position={128,0,30.125f};
 Path::PathPoint raw;raw.Position({128,0,30.25f}).PlayerOrigin();
 REQUIRE(client.CanWalkTo(RawWpPos(raw)),"real final player origin is supported by complete body/floor checks");
 raw.PlayerOrigin(false);
 REQUIRE(!client.CanWalkTo(RawWpPos(raw)),"former minus-eight final origin actually fails native body/floor acceptance");
 REQUIRE(bodyqueries>0,"actual full-body collision queries ran for both coordinate cases");
 printf("actual raw origin is accepted; actual former waypoint-offset destination below floor is rejected\n");
}
''')


if __name__ == '__main__':
    main()
