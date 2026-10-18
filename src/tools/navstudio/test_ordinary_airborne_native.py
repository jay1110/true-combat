"""Actual complete FollowPath Update and native body-walk during ordinary falls.

Northport's recorded position/waypoints feed an independent plane collision
fixture and gravity samples. The historic log lacks Ground/velocity: this is
the reproducible airborne class, not proof of that old frame's exact flags.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_door_runtime_native import compile_run
from test_destination_volume_native import program as destination_program


def program():
    source = destination_program().rsplit('void setup(FollowPath&', 1)[0]
    source = source.replace('const float floor=-8.f;', 'const float floor=-136.f;')
    source = source.replace('Vector3f GetVelocity(){return{};}', 'Vector3f velocity;Vector3f GetVelocity(){return velocity;}')
    source = source.replace('bool HasEntityFlag(int flag)const{return flag==ENT_FLAG_ONGROUND;}',
        'bool grounded=true,ladder=false;bool HasEntityFlag(int flag)const{return flag==ENT_FLAG_ONGROUND?grounded:flag==ENT_FLAG_ONLADDER?ladder:false;}')
    return source + r'''
void setupAir(FollowPath&f,User&u){
 f.client.position={-1179.378f,-2637.717f,-86.590f};f.client.grounded=false;f.client.velocity={0,0,-100};
 f.m_Query.m_User=&u;
 f.m_CurrentPath.AddPt({-1148.8f,-2634.3999f,-105.75f},35).PlayerOrigin().NavId(616);
 f.m_CurrentPath.AddPt({-1152,-2616,-105.75f},35).PlayerOrigin().NavId(546).Flags(F_NAV_CROUCH);
}
bool logged(const char*s){for(const auto&l:logs)if(l.find(s)!=String::npos)return true;return false;}
int main(){
 static FollowPath falling;User owner;setupAir(falling,owner);
 assert(!falling.client.CanWalkTo(falling.m_CurrentPath.GetPt(0)->m_Pt));
 assert(!falling.client.CanWalkTo(falling.m_CurrentPath.GetPt(1)->m_Pt,F_NAV_CROUCH));
 const int queried=bodyQueries;
 falling.client.buttons=BOT_BUTTON_JUMP;IGame::time+=50;falling.Update(.05f);
 assert(falling.failures==0&&owner.successes==0&&falling.m_CurrentPath.GetCurrentPtIndex()==0);
 assert(falling.m_TceAirWaitActive&&bodyQueries==queried&&falling.client.resets==0);
 assert(!(falling.client.buttons&BOT_BUTTON_JUMP));
 assert((falling.client.steer.target-falling.client.position).SquaredLength()==0);
 // Independent falling observations; no planner or authored point is changed.
 for(int i=0;i<5&&!falling.client.grounded;++i){
  falling.client.velocity.z-=1000.f*.05f;falling.client.position.z+=falling.client.velocity.z*.05f;
  if(falling.client.position.z<=-105.875f){falling.client.position.z=-105.875f;falling.client.velocity={};falling.client.grounded=true;}
  frame(falling);if(!falling.client.grounded)assert(falling.client.resets==0&&falling.m_CurrentPath.GetCurrentPtIndex()==0);
 }
 assert(falling.client.grounded&&falling.failures==0&&!falling.m_TceAirWaitActive&&bodyQueries>queried);
 assert(logged("AIR_WAIT")&&logged("AIR_LANDED")&&owner.successes==0);
 falling.client.position={-1152,-2616,-105.875f};frame(falling);
 assert(owner.successes==1&&falling.failures==0);

 // No floor / indefinitely airborne is a bounded fatal, never a fake arrival.
 logs.clear();static FollowPath noFloor;User none;setupAir(noFloor,none);collisionScenario=3;
 for(int i=0;i<20&&noFloor.failures==0;++i)frame(noFloor);
 assert(noFloor.failures==1&&none.successes==0&&noFloor.client.resets==0&&logged("reason=airborne_timeout"));
 assert(IGame::GetTime()-noFloor.m_TceAirWaitStart==750);
 // A reported landing does not bypass the fresh full floor proof.
 logs.clear();static FollowPath falseGround;User falseOwner;setupAir(falseGround,falseOwner);frame(falseGround);
 falseGround.client.grounded=true;falseGround.client.position.z=-105.875f;frame(falseGround);
 assert(falseGround.failures==0&&falseOwner.successes==0&&falseGround.m_TceAirWaitActive);
 const int originalWait=falseGround.m_TceAirWaitStart;
 for(int i=0;i<20&&falseGround.failures==0;++i)frame(falseGround);
 assert(falseGround.failures==1&&falseOwner.successes==0&&logged("unsupported_ground_timeout")&&logged("ground=1"));
 assert(falseGround.m_TceAirWaitStart==originalWait&&IGame::GetTime()-originalWait==750);

 // A future launch is not an active action. Nor are phase counters left by
 // an already consumed landing: current path metadata determines ownership.
 collisionScenario=0;
 for(int kind=0;kind<6;++kind){
  logs.clear();static FollowPath approach[6];User owners[6];auto&f=approach[kind];setupAir(f,owners[kind]);
  auto*next=const_cast<Path::PathPoint*>(f.m_CurrentPath.GetPt(1));
  if(kind==0)next->m_TceWalkDrop=true;
  if(kind==1)next->m_TceStandJump=true;
  if(kind==2)next->m_TceFreeClimb=true;
  if(kind==3)next->m_NavFlags=F_NAV_CLIMB;
  if(kind==4)next->m_NavFlags=F_NAV_DOOR;
  if(kind==5){f.m_TceDropNavId=40;f.m_TceStandPhase=2;f.m_TceFreePhase=1;}
  const int before=bodyQueries;frame(f);
  assert(f.failures==0&&f.m_TceAirWaitActive&&logged("AIR_WAIT")&&bodyQueries==before);
  assert(f.m_CurrentPath.GetCurrentPtIndex()==0&&f.client.resets==0&&owners[kind].successes==0);
 }

 // Authored traversal handlers retain ownership, including previous launch
 // metadata on their landing. The new ordinary wait must never intercept.
 collisionScenario=0;
 for(int kind=0;kind<5;++kind){
  logs.clear();static FollowPath action[5];User actionOwner[5];auto&f=action[kind];setupAir(f,actionOwner[kind]);
  if(kind==0)const_cast<Path::PathPoint*>(f.m_CurrentPath.GetPt(0))->m_NavFlags=F_NAV_JUMP;
  if(kind==1)const_cast<Path::PathPoint*>(f.m_CurrentPath.GetPt(0))->m_NavFlags=F_NAV_CLIMB;
  if(kind==2)const_cast<Path::PathPoint*>(f.m_CurrentPath.GetPt(0))->m_TceWalkDrop=true;
  if(kind==3){const_cast<Path::PathPoint*>(f.m_CurrentPath.GetPt(0))->m_TceWalkDrop=true;f.m_CurrentPath.NextPt();}
  if(kind==4)f.client.ladder=true;
  frame(f);assert(!f.m_TceAirWaitActive&&!logged("AIR_WAIT"));
 }
 printf("actual complete Update/native capsule walk: transient ordinary fall, checked landing, no-floor timeout, false grounded rejection and action ownership passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class OrdinaryAirborneTests(TestCase):
    def test_actual_update_waits_boundedly_for_ground_before_rechecking_ordinary_walk(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
