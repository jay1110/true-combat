"""Actual strict entry search inside the caller's original goal sphere.

Complete Planner/GetPath/Repath/FollowPath/Client walking and real steering
execute unchanged. An independent swept static corner and small safe body
steps supply movement, rather than teleporting to proposed entry points.
This offline fixture is not an ET PMove or live-map qualification.
"""
import os
import shutil
from unittest import TestCase,main,skipUnless
from test_combat_reentry_native import program as reentry_program
from test_door_runtime_native import COMMON,body,compile_run


def program():
    source=reentry_program().split('void setupReentry(',1)[0]
    source=source.replace('enum MoveMode{Run=0,Walk=1};','using MoveMode=int;enum{Run=0,Walk=1};')
    source=source.replace('Vector3f operator*(float s)const{return{x*s,y*s,z*s};}',
        'Vector3f operator*(float s)const{return{x*s,y*s,z*s};}Vector3f&operator*=(float s){x*=s;y*=s;z*=s;return*this;}')
    old=body(source,'struct Steering {')
    source=source.replace(old,r'''struct Client;
struct Steering{Client*client=nullptr;Client*GetClient(){return client;}
 Vector3f m_TargetVector,m_MoveVec;float m_TargetRadius=32,m_InputScale=1,m_DistanceToTarget=0;
 bool m_TargetVector3d=false,m_bMoveEnabled=true;MoveMode m_MoveMode=0;
 enum{Normal,Arrive};int m_MoveType=Normal;
 bool SetTarget(const Vector3f&,float=32,MoveMode=0,bool=false,float=1);
 void UpdateSteering();Vector3f GetTarget(){return m_TargetVector;}}''',1)
    source=source.replace('struct Client {','struct Client {int stuck=0;')
    source=source.replace('int GetStuckTime(){return 0;}','int GetStuckTime(){return stuck;}')
    old='if(collisionScenario==1&&a.x<=-1960.f&&b.x>-1960.f){r.m_Fraction=(-1960.f-a.x)/(b.x-a.x);r.m_Normal[0]=-1;r.m_Normal[2]=0;}'
    assert old in source
    source=source.replace(old,r'''
 if(collisionScenario==1){
  // Independent full-box corner. The actual player body is never shrunk.
  const float worldMin[3]={-1960,-1425,-8},worldMax[3]={-1900,-1365,90};
  float enter=0,leave=1;int hitAxis=-1;float hitSign=0;bool possible=true;
  for(int axis=0;axis<3;++axis){
   const float minimum=worldMin[axis]-(h?h->m_Maxs[axis]:0);
   const float maximum=worldMax[axis]-(h?h->m_Mins[axis]:0);
   const float begin=(&a.x)[axis],d=(&b.x)[axis]-begin;
   if(std::fabs(d)<.000001f){if(begin<=minimum||begin>=maximum)possible=false;}
   else{float x=(minimum-begin)/d,y=(maximum-begin)/d,sign=-1;
    if(x>y){std::swap(x,y);sign=1;}
    if(x>enter){enter=x;hitAxis=axis;hitSign=sign;}leave=MinT(leave,y);
   }
  }
  if(possible&&enter<leave&&leave>0&&enter<r.m_Fraction){
   r.m_Fraction=MaxT(0.f,enter);r.m_StartSolid=enter==0&&hitAxis<0;
   r.m_Normal[0]=r.m_Normal[1]=r.m_Normal[2]=0;
   if(hitAxis>=0)r.m_Normal[hitAxis]=hitSign;
  }
 }
''')
    steering=(COMMON/'BotSteeringSystem.cpp').read_text()
    source+='\nenum{BOT_DEBUG_MOVEVEC=100,ENT_FLAG_ON_ICE=200};\nnamespace Utils{void DrawLine(const Vector3f&,const Vector3f&,int,float){}}\n'
    source+='\n'.join(body(steering,marker).replace('SteeringSystem::','Steering::')for marker in
                      ('bool SteeringSystem::SetTarget(','void SteeringSystem::UpdateSteering()'))
    return source+r'''
bool logged(const char*s){for(const auto&l:logs)if(l.find(s)!=String::npos)return true;return false;}
void setup(FollowPath&f,User&u,Waypoint&bad,Waypoint&entry,Waypoint&goal){
 f.client.position={-1976.125f,-1425.f,22.125f};f.client.debug=false;f.client.steer.client=&f.client;
 f.m_CurrentPath.AddPt(bad.pos,35).PlayerOrigin().NavId(bad.uid);
 f.m_CurrentPath.AddPt(goal.pos,35).PlayerOrigin().NavId(goal.uid);
 f.m_Query.m_User=&u;f.m_Query.m_Destination={{{-4000,-4000,22.25f},2},{goal.pos,32}};u.m_DestinationIndex=1;
 bad.m_Connections={{&goal}};entry.m_Connections={{&bad}};goal.m_Connections={{&entry}};
 planner.m_WaypointList={&bad,&entry,&goal};planner.m_PlannerFlags.n=0;planner.plans=0;
}
void steerFrame(FollowPath&f){frame(f);f.client.steer.UpdateSteering();}
float safeMovementFrame(FollowPath&f){
 steerFrame(f);if(f.m_PathStatus==FollowPath::PathFinished)return 0;
 const Vector3f before=f.client.position,d=f.client.steer.m_TargetVector-before;
 const float remaining=d.Flatten().Length();float speed=204.f*.05f;
 Vector3f move=f.client.steer.m_MoveVec*speed;const float distance=move.Length();
 if(distance>remaining&&distance>0)move*=remaining/distance;
 const Vector3f after=before+move;
 if(move.SquaredLength()>0){
  AABB hull;f.client.GetNavigationHull(hull,0);obTraceResult trace;
  EngineFuncs::TraceLine(trace,before,after,&hull,TR_MASK_SOLID|TR_MASK_PLAYERCLIP|TR_MASK_NAVCAPSULE,1,False);
  assert(!trace.m_StartSolid&&trace.m_Fraction==1&&f.client.CanWalkTo(after));
  f.client.position=after;f.client.stuck=0;
 }else f.client.stuck+=50;
 return move.Length();
}
int main(){
 Waypoint bad{80,{-1952,-1441.125f,22.25f}},entry{100,{-1976.125f,-1460,22.25f}},goal{110,{-1960,-1441.125f,22.25f}};
 entry.radius=goal.radius=2;bad.radius=35;collisionScenario=1;
 static FollowPath f;User owner;setup(f,owner,bad,entry,goal);
 const Vector3f initial=f.client.position;
 assert(Length(initial,goal.pos)<32&&!f.client.CanWalkTo(bad.pos)&&!f.client.CanWalkTo(goal.pos));
 assert(f.client.CanWalkTo(entry.pos));
 // Normal callers retain their original goal-volume early completion.
 const int priorQueries=bodyQueries;
 planner.PlanPathToNearest(&f.client,initial,{{goal.pos,32}},f.client.GetTeamFlag());
 assert(planner.FoundGoal()&&planner.m_Solution.empty()&&planner.plans==0&&bodyQueries==priorQueries);
 // Full Update reaches the actual failed old corner and requests strict
 // recovery. It MUST search, despite being inside exactly the same32sphere.
 steerFrame(f);
 assert(f.failures==0&&owner.successes==0&&f.m_TceReentryAttempts==1&&planner.plans==1);
 assert(planner.m_Start.m_Wp[0]==&entry&&f.m_CurrentPath.GetPt(0)->m_NavId==100);
 // GetPath considered its first-leg projection; the actual body cannot
 // reach that chord, so the unchanged real entry is retained.
 assert(SquaredLength(RawWpPos(*f.m_CurrentPath.GetPt(0)),entry.pos)<.0001f);
 assert(f.client.steer.m_MoveVec.SquaredLength()>0&&logged("radius=32.000"));
 assert(f.m_Query.m_Destination.size()==2&&owner.m_DestinationIndex==1&&owner.resets==0);
 float moved=0;bool reachedEntry=false;
 for(int i=0;i<80&&owner.successes==0&&f.failures==0;++i){
  moved+=safeMovementFrame(f);
  if(Length(f.client.position,entry.pos)<2.1f)reachedEntry=true;
 }
 assert(owner.successes==1&&f.failures==0&&reachedEntry&&moved>30);
 // The original goal is close to the start; the real route goes away to
 // the entry and comes back. Require witnessed entry and physical path
 // length above, rather than pretending its final displacement is that path.
 assert(f.m_TceReentryAttempts==1);
 assert(f.m_Query.m_Destination[1].m_Radius==32&&goal.radius==2&&bad.radius==35&&entry.radius==2);

 // Strict entry cannot use a wall-blocked/action/unsupported alternative.
 for(int mode=0;mode<7;++mode){
  logs.clear();static FollowPath rejects[7];User u;auto&r=rejects[mode];setup(r,u,bad,entry,goal);
  collisionScenario=1;entry.flags=0;entry.props.p.clear();entry.through=0;
  if(mode==0)entry.flags=F_NAV_JUMP;
  if(mode==1)entry.flags=F_NAV_DOOR;
  if(mode==2)entry.props.p["tce_walk_drop"]="1";
  if(mode==3)entry.props.p["tce_stand_jump"]="1";
  if(mode==4)entry.props.p["tce_free_climb"]="1";
  if(mode==5)entry.through=1;
  if(mode==6)collisionScenario=3;
  Path::PathPoint blocked;r.m_CurrentPath.GetCurrentPt(blocked);
  assert(!r.TryTceReentry(blocked)&&r.m_TceReentryAttempts==1&&r.m_CurrentPath.GetPt(0)->m_NavId==80);
  assert(u.successes==0&&logged("REENTRY_REJECT")&&logged("radius=32.000"));
 }
 entry.flags=0;entry.props.p.clear();entry.through=0;
 // Missing water/ground clearance is still determined by actual Client walk.
 for(int hazard:{4,5}){
  static FollowPath hazards[2];User u;auto&r=hazards[hazard-4];setup(r,u,bad,entry,goal);collisionScenario=hazard;
  Path::PathPoint blocked;r.m_CurrentPath.GetCurrentPt(blocked);
  assert(!r.TryTceReentry(blocked)&&r.m_TceReentryAttempts==1&&u.successes==0);
 }
 // Same ordinary entry (including a near-identical projected first point)
 // is refused. Being inside the goal sphere cannot bypass this contract.
 collisionScenario=0;logs.clear();static FollowPath same;User su;setup(same,su,bad,entry,goal);
 same.client.position=bad.pos.AddZ(-.125f);Path::PathPoint bp;same.m_CurrentPath.GetCurrentPt(bp);
 assert(!same.TryTceReentry(bp)&&same.m_TceReentryAttempts==1&&logged("same_blocked_entry"));
 // Failed immobile recovery remains bounded by the original stuck gate;
 // no entry movement or goal-progress checkpoint is fabricated by planning.
 collisionScenario=1;static FollowPath stalled;User tu;setup(stalled,tu,bad,entry,goal);steerFrame(stalled);
 assert(stalled.m_TceReentryPendingProgress&&stalled.m_TceReentryAttempts==1);
 for(int i=0;i<50&&stalled.failures==0;++i){stalled.client.stuck+=50;steerFrame(stalled);}
 assert(stalled.failures==1&&tu.successes==0&&stalled.m_TceReentryAttempts==1);
 // Existing shared two-attempt budget and original-goal identity remain.
 static FollowPath budget;User bu;setup(budget,bu,bad,entry,goal);budget.m_CurrentPath.GetCurrentPt(bp);
 assert(budget.TryTceReentry(bp)&&budget.TryTceReentry(bp));const int plans=planner.plans;
 budget.m_Query.m_Destination[1].m_Position={-1976.125f,-1700,22.25f};
 assert(!budget.TryTceReentry(bp)&&budget.m_TceReentryAttempts==2&&planner.plans==plans);
 printf("inside32sphere: real strict native entry search, blocked projection, swept body movement, original normal volume arrival, hazards/actions/same-entry/stuck and shared budget passed\n");
}
'''


@skipUnless(os.name=='nt'and shutil.which('cmake'),'Windows CMake/MSVC required')
class ReentryInsideGoalTests(TestCase):
    def test_strict_real_entry_search_is_not_skipped_inside_original_goal_sphere(self):
        compile_run(self,program())


if __name__=='__main__':main()
