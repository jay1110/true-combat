"""Complete Update/Repath/native entry search with independent collision input.

The tiny fixture graph uses an independent breadth-first solver; production
PlanPathToNearest's candidate selection, full Client capsule walk, GetPath,
Repath and bounded re-entry controller execute unchanged. No live claim.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_door_runtime_native import COMMON, body, compile_run
from test_destination_volume_native import program as destination_program


def program():
    source = destination_program().rsplit('void setup(FollowPath&', 1)[0]
    source = '#include <algorithm>\n' + source
    source = source.replace('struct User{int successes=0;',
        'struct User{int successes=0,m_DestinationIndex=0,resets=0;void ResetPathUser(){++resets;}')
    source = source.replace('struct State{enum',
        'struct MapGoal{String GetName(){return "fixture";}};struct State{MapGoal*GetMapGoalPtr(){return nullptr;}String GetName(){return "fixture";}enum')
    source = source.replace('struct Client {', 'struct HighLevel{State*GetActiveState(){return nullptr;}};struct Client {')
    source = source.replace('bool IsDebugEnabled(int)const{return debug;}',
        'HighLevel*GetHighLevel(){static HighLevel h;return &h;}const char*GetName(){return "fixture";}bool IsDebugEnabled(int)const{return debug;}')
    source = source.replace('static float GetDeltaTimeSecs(){return .05f;}',
        'static float GetDeltaTimeSecs(){return .05f;}static float GetTimeSecs(){return time*.001f;}')
    if 'static float GetTimeSecs()' not in source:
        source = source.replace('static int GetFrameNumber(){return time/50;}',
            'static int GetFrameNumber(){return time/50;}static float GetTimeSecs(){return time*.001f;}')
    source = source.replace('bool m_Final=true;User*m_User=nullptr;',
        'bool m_Final=true,m_SkipLastPt=false;DestinationVector m_Destination;User*m_User=nullptr;')
    source = source.replace('bool Repath(){return false;}',
        'bool Repath();bool m_IgnorePathNotFound=true;void ResetTceFreeClimb();void ResetTceStandingJump();')
    source = source.replace('bool TryTceReentry(const Path::PathPoint&){return false;}',
        'bool TryTceReentry(const Path::PathPoint&);')
    source = source.replace('void ReplenishTceReentryBudget(const Path::PathPoint&){}',
        'void ReplenishTceReentryBudget(const Path::PathPoint&);')
    source = source.replace('enum{PathPending,PathInProgress,PathFinished,PathNotFound};',
        'enum PathStatus{PathPending,PathInProgress,PathFinished,PathNotFound};')
    source = source.replace('int m_PathStatus=PathInProgress;', 'PathStatus m_PathStatus=PathInProgress;')
    source = source.replace('struct FollowPathUser{enum{Blocked=2};}', 'struct FollowPathUser{enum{NoPath=1,Blocked=2};}')
    source = source.replace('struct Waypoint {int uid;', 'struct Waypoint {int uid;')
    source = source.replace('unsigned OnPathThrough()const{return 0;}',
        'unsigned through=0;unsigned OnPathThrough()const{return through;}')
    source = source.replace('using ConnectionList=std::list<ConnectionInfo>;',
        'int m_Mark=0,m_GoalIndex=0;using ConnectionList=std::list<ConnectionInfo>;')
    source = source.replace('class PathPlannerBase{public:', r'''
enum{SKIP_NO_CONNECTIONS=0,NAV_FOUNDGOAL=1,WAYPOINT_ISDONE=2};
struct Flags{unsigned n=0;void SetFlag(int f){n|=f;}void ClearFlag(int f){n&=~f;}bool CheckFlag(int f){return (n&f)!=0;}};
struct ClosestLink{static const int NumWps=1;Waypoint*m_Wp[1];ClosestLink(Waypoint*w=nullptr){m_Wp[0]=w;}bool IsValid(){return m_Wp[0]!=nullptr;}};
void TcePrepareRoutes(Client*){}
class PathPlannerBase{public:
virtual int PlanPathToNearest(Client*,const Vector3f&,const DestinationVector&,const NavFlags&)=0;
virtual int PlanPathToNearestWithEntryPolicy(Client*,const Vector3f&,const DestinationVector&,const NavFlags&,bool)=0;
virtual bool FoundGoal()=0;virtual void GetPath(Path&,int=0)=0;
''')
    marker = 'Client*m_Client=nullptr;'
    assert marker in source
    source = source.replace(marker, marker + r'''
 Flags m_PlannerFlags;ClosestLink m_Start;int m_GoalIndex=0,m_WaypointMark=0,plans=0;std::vector<Waypoint*>m_OpenList;
 Waypoint*_GetClosestWaypoint(const Vector3f&p,NavFlags,int,void*){
  Waypoint*best=nullptr;float d=1e30f;for(auto*w:m_WaypointList){const float s=(w->pos-p).SquaredLength();if(s<d){best=w;d=s;}}return best;}
 bool FoundGoal(){return m_PlannerFlags.CheckFlag(NAV_FOUNDGOAL);}
 int PlanPathToNearest(Client*,const Vector3f&,const DestinationVector&,const NavFlags&);
 int PlanPathToNearestWithEntryPolicy(Client*,const Vector3f&,const DestinationVector&,const NavFlags&,bool);
 void _RunDijkstra(NavFlags){ // Independent fixture graph solver, not a walk/entry stub.
  ++plans;m_Solution.clear();Waypoint*start=m_Start.m_Wp[0];std::vector<Waypoint*>queue{start};std::map<Waypoint*,Waypoint*>parent;parent[start]=nullptr;
  for(size_t i=0;i<queue.size();++i){auto*w=queue[i];if(w->m_Mark==m_WaypointMark){m_GoalIndex=w->m_GoalIndex;
    for(auto*p=w;p;p=parent[p])m_Solution.push_back(p);m_PlannerFlags.SetFlag(NAV_FOUNDGOAL);return;}
   for(const auto&edge:w->m_Connections)if(!parent.count(edge.m_Connection)){parent[edge.m_Connection]=w;queue.push_back(edge.m_Connection);}}
 }
''')
    follow = (COMMON/'BotBaseStates.cpp').read_text()
    planner = (COMMON/'PathPlannerWaypoint.cpp').read_text()
    functions = '\n'.join(body(follow, marker) for marker in ('void FollowPath::ResetTceFreeClimb()',
        'void FollowPath::ResetTceStandingJump()', 'bool FollowPath::Repath()', 'bool FollowPath::TryTceReentry(',
        'void FollowPath::ReplenishTceReentryBudget('))
    functions = body(planner, 'int PathPlannerWaypoint::PlanPathToNearest(Client *_client, const Vector3f &_start, const DestinationVector &') + \
        body(planner, 'int PathPlannerWaypoint::PlanPathToNearestWithEntryPolicy(') + functions
    # Repath's preserved final-segment projection uses the real remove helper.
    functions = body((COMMON/'Path.cpp').read_text(), 'void Path::RemoveLastPt()') + functions
    source += '\nvoid MapDebugPrint(const char*){}\n' + functions
    return source + r'''
void setupReentry(FollowPath&f,User&u,Waypoint&bad,Waypoint&entry,Waypoint&goal){
 f.client.position={-1984,-1441,22.125f};f.client.debug=false;
 f.m_CurrentPath.AddPt(bad.pos,60).PlayerOrigin().NavId(bad.uid);
 f.m_CurrentPath.AddPt({-1900,-1400,22.25f},8).PlayerOrigin().NavId(81);
 f.m_Query.m_User=&u;f.m_Query.m_Destination={{{-2300,-1500,22.25f},2},{{-1984,-1300,22.25f},2}};
 u.m_DestinationIndex=1;
 bad.m_Connections={{&goal}};entry.m_Connections={{&goal}};goal.m_Connections={{&entry}};
 planner.m_WaypointList={&bad,&entry,&goal};planner.m_PlannerFlags.n=0;
}
int main(){
 Waypoint bad{80,{-1940,-1441,22.25f}},entry{100,{-1984,-1391,22.25f}},goal{110,{-1984,-1300,22.25f}};
 entry.radius=goal.radius=2;collisionScenario=1;
 static FollowPath f;User owner;setupReentry(f,owner,bad,entry,goal);
 assert(!f.client.CanWalkTo(bad.pos)&&f.client.CanWalkTo(entry.pos));
 frame(f);
 assert(f.failures==0&&owner.successes==0&&f.m_TceReentryAttempts==1&&f.m_TceReentryPendingProgress);
 assert(planner.m_Start.m_Wp[0]==&entry&&planner.m_Client==&f.client); // Real native candidate filter skipped nearer blocked80.
 assert(f.m_CurrentPath.GetPt(0)->m_NavId==100&&f.client.steer.target.y==-1391);
 assert(f.m_Query.m_Destination.size()==2&&owner.m_DestinationIndex==1&&owner.resets==0&&f.client.resets==0);
 assert(f.m_CurrentPath.GetPt(f.m_CurrentPath.GetNumPts()-1)->m_Pt.y==-1300);
 frame(f);assert(f.client.resets==0); // Planning and waiting are not movement.
 f.client.position.y+=20;frame(f);assert(!f.m_TceReentryPendingProgress&&f.client.resets==1);
 f.client.position=entry.pos;frame(f);f.client.position=goal.pos;frame(f);frame(f);
 assert(owner.successes==1&&f.failures==0);

 // A nearer, physically accessible action must not conceal the next safe
 // ordinary recovery entry. Both searches execute the actual native hull
 // checks; normal queries still deliberately permit the same action start.
 Waypoint jump{300,{-1984,-1425,22.25f}};jump.flags=F_NAV_JUMP;jump.radius=2;
 static FollowPath actionNear;User actionOwner;setupReentry(actionNear,actionOwner,bad,entry,goal);
 jump.m_Connections={{&entry}};planner.m_WaypointList.push_back(&jump);
 assert(actionNear.client.CanWalkTo(jump.pos)&&actionNear.client.CanWalkTo(entry.pos));
 DestinationVector direct{{goal.pos,2}};
 planner.PlanPathToNearest(&actionNear.client,actionNear.client.position,direct,actionNear.client.GetTeamFlag());
 assert(planner.FoundGoal()&&planner.m_Start.m_Wp[0]==&jump);
 frame(actionNear);
 assert(actionNear.failures==0&&actionNear.m_TceReentryAttempts==1&&planner.m_Start.m_Wp[0]==&entry);
 assert(actionNear.m_CurrentPath.GetPt(0)->m_NavId==100&&actionNear.client.resets==0&&actionOwner.resets==0);

 // Property-only actions and script-through starts obey the same policy.
 for(int kind=0;kind<4;++kind){
  jump.flags=0;jump.props.p.clear();jump.through=0;
  if(kind==0)jump.props.p["tce_walk_drop"]="1";
  if(kind==1)jump.props.p["tce_stand_jump"]="1";
  if(kind==2)jump.props.p["tce_free_climb"]="1";
  if(kind==3)jump.through=1;
  planner.PlanPathToNearestWithEntryPolicy(&actionNear.client,actionNear.client.position,direct,actionNear.client.GetTeamFlag(),true);
  assert(planner.FoundGoal()&&planner.m_Start.m_Wp[0]==&entry);
 }
 jump.props.p.clear();jump.through=0;jump.flags=F_NAV_JUMP;
 // Action-only alternatives cannot turn recovery into an unchecked launch.
 planner.m_WaypointList={&jump};
 planner.PlanPathToNearestWithEntryPolicy(&actionNear.client,actionNear.client.position,direct,actionNear.client.GetTeamFlag(),true);
 assert(!planner.FoundGoal()&&!planner.m_Start.IsValid());
 // A missing floor still defeats every ordinary candidate.
 planner.m_WaypointList={&jump,&entry,&goal};collisionScenario=3;
 planner.PlanPathToNearestWithEntryPolicy(&actionNear.client,actionNear.client.position,direct,actionNear.client.GetTeamFlag(),true);
 assert(!planner.FoundGoal()&&!planner.m_Start.IsValid());collisionScenario=1;

 // Lost current support uses the single bounded ground wait, preserving the
 // original query/path; no failed re-entry attempt or fake success is needed.
 static FollowPath missing;User missingOwner;setupReentry(missing,missingOwner,bad,entry,goal);collisionScenario=3;
 frame(missing);const int supportWait=missing.m_TceAirWaitStart;
 for(int i=0;i<15&&missing.failures==0;++i)frame(missing);
 assert(missing.failures==1&&missingOwner.successes==0&&missing.m_TceReentryAttempts==0);
 assert(IGame::GetTime()-supportWait==750);
 assert(missing.m_CurrentPath.GetPt(0)->m_NavId==80&&missingOwner.m_DestinationIndex==1&&missing.m_Query.m_Destination.size()==2);
 assert(missing.client.resets==0&&missingOwner.resets==0);

 // A planner result with the identical usable first point is not called recovery.
 collisionScenario=0;static FollowPath same;User sameOwner;setupReentry(same,sameOwner,bad,entry,goal);
 same.client.position=bad.pos.AddZ(-.125f);Path::PathPoint samePoint;same.m_CurrentPath.GetCurrentPt(samePoint);
 assert(!same.TryTceReentry(samePoint)&&same.m_TceReentryAttempts==1&&same.client.resets==0);

 // Repeated displacement cannot replenish the budget by invoking Repath.
 collisionScenario=1;static FollowPath budget;User budgetOwner;setupReentry(budget,budgetOwner,bad,entry,goal);
 Path::PathPoint blocked;budget.m_CurrentPath.GetCurrentPt(blocked);
 assert(budget.TryTceReentry(blocked));assert(budget.TryTceReentry(blocked));
 const int planned=planner.plans;assert(!budget.TryTceReentry(blocked)&&budget.m_TceReentryAttempts==2&&planner.plans==planned);
 assert(budget.client.resets==0&&budgetOwner.resets==0&&budgetOwner.m_DestinationIndex==1);

 // A long, genuinely advanced route can acquire another bounded allowance.
 // Neither 16u strafing nor consuming the fresh entry is enough. The original
 // destination and the never-increasing checkpoint survive every Repath.
 Waypoint longGoal{210,{-1984,-1000,22.25f}},middle{220,{-1984,-1220,22.25f}};
 middle.radius=longGoal.radius=2;static FollowPath progress;User progressOwner;
 setupReentry(progress,progressOwner,bad,entry,longGoal);
 progress.m_Query.m_Destination[1].m_Position=longGoal.pos;
 entry.m_Connections={{&middle}};middle.m_Connections={{&longGoal}};
 planner.m_WaypointList.push_back(&middle);
 assert(progress.TryTceReentry(blocked)&&progress.TryTceReentry(blocked));
 assert(progress.m_TceReentryAttempts==2);const float initialBest=progress.m_TceReentryBestRemaining;
 progress.client.position.y+=20;frame(progress);assert(progress.m_TceReentryAttempts==2);
 progress.client.position=entry.pos;frame(progress);assert(progress.m_TceReentryAttempts==2);
 progress.client.position=middle.pos;frame(progress);
 assert(progress.m_TceReentryAttempts==0&&progress.m_TceReentryCheckpointId==220);
 assert(progress.m_TceReentryBestRemaining<=initialBest-128&&progressOwner.successes==0);
 assert(progress.TryTceReentry(blocked)&&progress.TryTceReentry(blocked));
 assert(progress.m_TceReentryAttempts==2);
 const float checkpoint=progress.m_TceReentryBestRemaining;
 // A large detour and return, even across different reached UIDs, cannot
 // replenish attempts without new net progress toward the original goal.
 for(int i=0;i<2;++i){
  progress.m_CurrentPath.Clear();Vector3f p=i?middle.pos:Vector3f{-1984,-1400,22.25f};
  progress.m_CurrentPath.AddPt(p,2).PlayerOrigin().NavId(230+i);
  progress.m_CurrentPath.AddPt(longGoal.pos,2).PlayerOrigin().NavId(210);
  progress.client.position=p;frame(progress);
  assert(progress.m_TceReentryAttempts==2&&progress.m_TceReentryBestRemaining==checkpoint);
 }
 // Losing the floor at a later nominal waypoint remains a real failure.
 progress.m_CurrentPath.Clear();progress.client.position={-1984,-1050,22.25f};
 progress.m_CurrentPath.AddPt(progress.client.position,2).PlayerOrigin().NavId(240);
 progress.m_CurrentPath.AddPt(longGoal.pos,2).PlayerOrigin().NavId(210);collisionScenario=3;
 frame(progress);const int laterSupportWait=progress.m_TceAirWaitStart;
 for(int i=0;i<15&&progress.failures==0;++i)frame(progress);
 assert(progress.failures==1&&progress.m_TceReentryAttempts==2&&progressOwner.successes==0);
 assert(IGame::GetTime()-laterSupportWait==750);
 printf("complete Update + real Repath/PlanPathToNearest/GetPath/full native body entry: displaced recovery, same destination, actual progress, missing floor, identical entry and two-attempt budget passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class CombatReentryTests(TestCase):
    def test_actual_controller_replans_only_to_a_different_native_walk_entry_with_bounded_budget(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
