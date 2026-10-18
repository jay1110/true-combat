"""Full ordinary path Update/Steering and productive TCE ladder input adapter.

The recorded Delta pose feeds independent ladder contact and full-body floor
observations. Adapter commands drive small physical descent steps; this is an
offline collision/command contract, not an ET PMove or live-map qualification.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_reentry_inside_goal_native import program as entry_program
from test_door_runtime_native import COMMON, body, compile_run


def program():
    source = entry_program().split('bool logged(', 1)[0]
    source = source.replace('const float floor=-8.f;', 'const float floor=-64.f;')
    source = source.replace('out=AABB{};return true;', 'out=AABB{};out.m_Mins[2]=-24;out.m_Maxs[2]=48;return true;')
    source = source.replace('bool HasEntityFlag(int flag)const{return flag==ENT_FLAG_ONGROUND;}',
        'bool grounded=true,ladder=false;bool HasEntityFlag(int flag)const{return flag==ENT_FLAG_ONGROUND?grounded:flag==ENT_FLAG_ONLADDER?ladder:false;}')
    source = source.replace('Vector3f GetVelocity(){return{};}', 'Vector3f velocity;Vector3f GetVelocity(){return velocity;}')
    source = source.replace('int UpdateTceOrdinaryLadderContact(const Path::PathPoint&){return 0;}',
                            'int UpdateTceOrdinaryLadderContact(const Path::PathPoint&);')
    source = source.replace('struct User{int successes=0,', 'struct User{void OnPathFailed(int){}int successes=0,')
    source = source.replace('struct FollowPathUser{enum{NoPath=1,Blocked=2};}',
                            'struct FollowPathUser{enum{NoPath=1,Blocked=2,Interrupted=3};}')
    source = source.replace('bool Goto(User*,const DestinationVector&,int,bool,bool){return false;}',
        'bool Goto(User*,const DestinationVector&,int,bool,bool);void Stop(bool);void ClearUser(){m_Query.m_User=nullptr;}void SaveQuery(){m_SavedQuery=m_Query;}')
    follow = (COMMON/'BotBaseStates.cpp').read_text()
    # Stop, Goto and Repath are productive lifecycle functions, rather than
    # resetting a fixture timer while claiming that a real goal change did it.
    source += '\n' + body(follow, 'void FollowPath::Stop(')
    source += '\n' + body(follow, 'bool FollowPath::Goto(FollowPathUser *_owner, const DestinationVector &').replace('FollowPathUser *_owner','User *_owner')
    source += '\n' + body(follow, 'int FollowPath::UpdateTceOrdinaryLadderContact(')
    client = (COMMON/'Client.cpp').read_text().split('HANDLER(MESSAGE_SPAWN)', 1)[1]
    spawn = body(client, 'if(UseValidatedWalkEntries())').replace('UseValidatedWalkEntries()',
        'f.client.UseValidatedWalkEntries()').replace('GetFollowPath()->', 'f.')
    source += '\nvoid FollowPath::ResetTceDropTrafficLease(){m_TceDropTrafficStart=0;}\nvoid spawnEvent(FollowPath&f){' + spawn + '}\n'
    adapter_source = (COMMON.parents[4]/'game/etbot/g_etbot_interface.cpp').read_text()
    adapter = body(adapter_source, 'if (!_input.m_ButtonFlags.CheckFlag(TCE_NAV_FREECLIMB_BUTTON) && !cmd.upmove &&')
    ladder_header = (COMMON.parents[4]/'game/tce_nav_ladder.h').read_text()
    source += ladder_header + r'''
using vec3_t=float[3];
enum{PMF_LADDER=4,PMF_DUCKED=1,EF_PRONE=0x80000,EF_PRONE_MOVING=0x100000,
 ENTITYNUM_NONE=1023,ENTITYNUM_WORLD=1022,MASK_PLAYERSOLID=1,CONTENTS_BODY=2,SURF_LADDER=8,
 TCE_NAV_FREECLIMB_BUTTON=8192,PITCH=0,ROLL=2};
struct trace_t{float fraction=1;int startsolid=0,allsolid=0,entityNum=ENTITYNUM_WORLD,surfaceFlags=SURF_LADDER;struct{float normal[3]={0,1,0};}plane;};
void VectorCopy(const float*a,float*b){for(int i=0;i<3;++i)b[i]=a[i];}
void VectorScale(const float*a,float n,float*b){for(int i=0;i<3;++i)b[i]=a[i]*n;}
float DotProduct(const float*a,const float*b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
void vectoangles(const float*v,float*a){a[0]=-std::atan2(v[2],std::sqrt(v[0]*v[0]+v[1]*v[1]))*57.2957795f;a[1]=std::atan2(v[1],v[0])*57.2957795f;a[2]=0;}
void AngleVectors(const float*a,float*f,float*r,void*){const float yaw=a[1]*.01745329252f;f[0]=std::cos(yaw);f[1]=std::sin(yaw);f[2]=0;r[0]=std::sin(yaw);r[1]=-std::cos(yaw);r[2]=0;}
struct PlayerState{int pm_flags=0,eFlags=0,groundEntityNum=ENTITYNUM_NONE,stats[1]={100};float origin[3]={},mins[3]={-16,-16,-24},maxs[3]={16,16,48};};
struct GameClient{PlayerState ps;};struct GameBot{GameClient*client;};enum{STAT_HEALTH=0};
int nativeMountQueries=0;
void trap_TraceCapsule(trace_t*t,const float*,const float*mins,const float*maxs,const float*,int,int){
 assert(mins[2]==-24&&maxs[2]==48);++nativeMountQueries;t->fraction=.5f;
}
void SetClientViewAngle(GameBot*,float*){}
struct Command{signed char forwardmove=0,rightmove=0,upmove=0;};
Command tceLadderCommand(FollowPath&f,float&pitch){
 struct Input{struct{int bits=0;bool CheckFlag(int flag){return(bits&flag)!=0;}}m_ButtonFlags;float m_MoveDir[3];}_input;
 _input.m_ButtonFlags.bits=f.client.buttons;
 for(int i=0;i<3;++i)_input.m_MoveDir[i]=(&f.client.steer.m_MoveVec.x)[i];
 GameClient gc;gc.ps.pm_flags=f.client.ladder?PMF_LADDER:0;gc.ps.groundEntityNum=f.client.grounded?ENTITYNUM_WORLD:ENTITYNUM_NONE;
 for(int i=0;i<3;++i)gc.ps.origin[i]=(&f.client.position.x)[i];GameBot gb{&gc},*bot=&gb;
 Command cmd;float angles[3]={},bodyangles[3]={},forward[3]={},right[3]={};const int fMaxSpeed=127,_client=1;
''' + adapter + r'''
 pitch=angles[0];return cmd;
}
bool logged(const char*s){for(const auto&l:logs)if(l.find(s)!=String::npos)return true;return false;}
void setup(FollowPath&f,User&u){
 f.client.debug=false;f.client.steer.client=&f.client;f.client.position={2270.550f,278.640f,24.509f};
 f.client.grounded=false;f.client.ladder=true;f.client.velocity={2,-29,-72};f.m_Query.m_User=&u;
 f.m_CurrentPath.AddPt({2274,280,-39.75f},35).PlayerOrigin().NavId(472);
 f.m_CurrentPath.AddPt({2240,332.5538f,-39.75f},35).PlayerOrigin().NavId(157);
 f.m_Query.m_Destination={{{2240,332.5538f,-39.75f},35}};
}
void step(FollowPath&f){frame(f);f.client.steer.UpdateSteering();}
void descend(FollowPath&f){
 float pitch;const Command cmd=tceLadderCommand(f,pitch);
 assert(cmd.forwardmove==127&&pitch>50&&f.client.steer.m_MoveVec.z<-.5f);
 const Vector3f before=f.client.position;Vector3f after=before;after.z-=72.f*.05f;
 AABB hull;f.client.GetNavigationHull(hull,0);obTraceResult trace;
 EngineFuncs::TraceLine(trace,before,after,&hull,TR_MASK_SOLID|TR_MASK_PLAYERCLIP|TR_MASK_NAVCAPSULE,1,False);
 if(trace.m_Fraction<1){after.z=-39.875f;f.client.grounded=true;f.client.ladder=false;f.client.velocity={};}
 else f.client.velocity={0,0,-72};
 assert(!trace.m_StartSolid);f.client.position=after;
}
int main(){
 collisionScenario=0;static FollowPath f;User owner;setup(f,owner);
 const int queries=bodyQueries;assert(!f.client.CanWalkTo({2240,332.5538f,-39.75f}));const int before=bodyQueries;
 step(f);assert(f.failures==0&&owner.successes==0&&f.m_CurrentPath.GetCurrentPtIndex()==0);
 assert(f.m_TceOrdinaryLadderStart>0&&bodyQueries==before&&logged("LADDER_CONTACT"));
 assert(f.client.steer.m_TargetRadius==0&&f.client.steer.m_TargetVector3d&&f.client.steer.m_MoveVec.z<-.5f);
 assert(f.client.buttons&BOT_BUTTON_MOVEDN);
 const int start=f.m_TceOrdinaryLadderStart;int frames=0;
 while(!f.client.grounded&&frames++<30){descend(f);step(f);assert(f.failures==0&&owner.successes==0);}
 assert(f.client.grounded&&frames>=15&&frames<=20&&nativeMountQueries>0);
 assert(f.client.position.z==-39.875f&&f.m_TceOrdinaryLadderStart==0&&logged("LADDER_LANDED"));
 assert(bodyQueries>before&&f.m_CurrentPath.GetCurrentPtIndex()==1&&IGame::time-start<3000);
 assert(f.m_CurrentPath.GetPt(0)->m_Radius==35&&f.m_CurrentPath.GetPt(0)->m_Pt.z==-39.75f&&f.m_Query.m_Destination[0].m_Radius==35);
 // PMove can report both contact flags at a ladder's foot. Only an exact
 // fresh full-body floor proof permits the ordinary outgoing WALK there.
 static FollowPath both;User bu;setup(both,bu);step(both);
 both.client.position.z=-39.875f;both.client.grounded=true;step(both);
 assert(both.failures==0&&both.m_TceOrdinaryLadderStart==0&&both.m_CurrentPath.GetCurrentPtIndex()==1);
 // Fresh ordinary outgoing proof still fails after real landing if the
 // floor has become hazardous; actual body contact never waives that proof.
 for(int hazard:{3,4,5}){
  logs.clear();static FollowPath hazards[3];User u;auto&r=hazards[hazard-3];setup(r,u);step(r);
  collisionScenario=hazard;r.client.position.z=-39.875f;r.client.grounded=true;r.client.ladder=false;
  for(int i=0;i<20&&r.failures==0;++i)step(r);
  assert(r.failures==1&&u.successes==0&&r.m_CurrentPath.GetCurrentPtIndex()==0&&r.m_TceOrdinaryLadderStart>0);
 }
 // False ONGROUND on the ladder cannot manufacture actual unchanged support.
 collisionScenario=0;static FollowPath falseGround;User fu;setup(falseGround,fu);falseGround.client.grounded=true;
 step(falseGround);assert(falseGround.failures==0&&falseGround.m_CurrentPath.GetCurrentPtIndex()==0&&falseGround.m_TceOrdinaryLadderStart>0);
 // Contact loss returns to the unchanged750ms ground gate. It neither
 // consumes the point nor sends a fresh outgoing walk from this air pose.
 logs.clear();static FollowPath lost;User lu;setup(lost,lu);step(lost);const int lostLease=lost.m_TceOrdinaryLadderStart;
 lost.client.ladder=false;const int airQueries=bodyQueries;
 for(int i=0;i<17&&lost.failures==0;++i)step(lost);
 assert(lost.failures==1&&lu.successes==0&&lost.m_CurrentPath.GetCurrentPtIndex()==0&&bodyQueries==airQueries);
 assert(logged("airborne_timeout")&&lost.m_TceOrdinaryLadderStart==lostLease&&IGame::time-lost.m_TceAirWaitStart==750);
 // Actual Goto/Repath and Stop do not buy time by changing goal/point.
 logs.clear();static FollowPath repeated;User ru;setup(repeated,ru);step(repeated);const int lease=repeated.m_TceOrdinaryLadderStart;
 Waypoint entry{472,{2274,280,-39.75f}},goal{157,{2240,332.5538f,-39.75f}};entry.m_Connections={{&goal}};goal.m_Connections={{&entry}};
 planner.m_WaypointList={&entry,&goal};planner.m_PlannerFlags.n=0;
 for(int i=0;i<65&&repeated.failures==0;++i){
  if(i%3==0){assert(repeated.Goto(&ru,{{goal.pos,35}},0,false,true));assert(repeated.m_TceOrdinaryLadderStart==lease);}
  if(i==10){repeated.m_PathStatus=FollowPath::PathFinished;repeated.Stop(false);assert(repeated.m_TceOrdinaryLadderStart==lease);assert(repeated.Goto(&ru,{{goal.pos,35}},0,false,true));}
  repeated.client.ladder=i%4!=0;step(repeated);assert(repeated.m_TceOrdinaryLadderStart==lease);
 }
 assert(repeated.failures==1&&ru.successes==0&&logged("ladder_contact_timeout")&&IGame::time-lease==3000);
 // The original stuck2s guard remains active even before that hard deadline.
 static FollowPath stuck;User su;setup(stuck,su);stuck.client.stuck=2050;stuck.m_LastStuckTime=IGame::time;
 step(stuck);assert(stuck.failures==1&&su.successes==0);
 // Typed traversals and authored action landings retain their own controller.
 for(int mode=0;mode<9;++mode){
  static FollowPath typed[9];User u;auto&r=typed[mode];setup(r,u);auto*p=const_cast<Path::PathPoint*>(r.m_CurrentPath.GetPt(0));
  if(mode==0)p->m_NavFlags=F_NAV_CLIMB;if(mode==1)p->m_NavFlags=F_NAV_JUMP;
  if(mode==2)p->m_TceWalkDrop=true;if(mode==3)p->m_TceStandJump=true;if(mode==4)p->m_TceFreeClimb=true;
  if(mode==5)p->m_NavFlags=F_NAV_DOOR;if(mode==6)p->m_OnPathThrough=1;
  if(mode==7){p->m_NavFlags=F_NAV_JUMP;r.m_CurrentPath.NextPt();}
  if(mode==8){p->m_TceWalkDrop=true;r.m_CurrentPath.NextPt();}
  assert(r.UpdateTceOrdinaryLadderContact(*r.m_CurrentPath.GetPt(r.m_CurrentPath.GetCurrentPtIndex()))==0&&r.m_TceOrdinaryLadderStart==0);
  step(r);assert(r.m_TceOrdinaryLadderStart==0);
 }
 // Spawn and clock rollback end the old physical episode; no path survives
 // a new life or a clock from a previous game phase.
 static FollowPath spawn;User pu;setup(spawn,pu);step(spawn);assert(spawn.m_TceOrdinaryLadderStart>0);
 spawnEvent(spawn);assert(spawn.m_TceOrdinaryLadderStart==0&&spawn.m_CurrentPath.GetNumPts()==0);
 static FollowPath clock;User cu;setup(clock,cu);step(clock);IGame::time=clock.m_TceOrdinaryLadderStart-100;
 clock.Update(.05f);assert(clock.m_TceOrdinaryLadderStart==0&&clock.m_CurrentPath.GetNumPts()==0&&clock.m_PathStatus==FollowPath::PathFinished);
 printf("FullUpdate/Steering/TCE ladder adapter: true contact descends to exact body floor before ordinary WALK; immutable3s lifecycle and original750ms/2s/hazard/action guards passed\n");
}
'''
    return source


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class OrdinaryLadderContactTests(TestCase):
    def test_actual_contact_descends_before_ordinary_arrival_and_keeps_finite_deadlines(self):
        compile_run(self, program())

    def test_disabling_contact_contract_reproduces_recorded_premature_walk_failure(self):
        source = program().split('int main(){', 1)[0]
        actual = body(source, 'int FollowPath::UpdateTceOrdinaryLadderContact(')
        source = source.replace(actual, 'int FollowPath::UpdateTceOrdinaryLadderContact(const Path::PathPoint&){return 0;}')
        source += r'''
int main(){
 collisionScenario=0;static FollowPath f;User u;setup(f,u);step(f);
 assert(f.failures==1&&u.successes==0&&f.m_CurrentPath.GetCurrentPtIndex()==0);
 assert(logged("BLOCKED_ORDINARY")&&logged("reason=centre_unreachable")&&logged("ladder=1"));
 assert(!(f.client.buttons&BOT_BUTTON_MOVEDN));
}
'''
        compile_run(self, source)


if __name__ == '__main__':
    main()
