"""Complete production Update/native body support with temporary player perches.

Delta's real failed position is 49 units above the static floor. Independent
plane collisions and successive ground/air observations replay that class;
this regression does not claim to reproduce a live player's exact collision.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_door_runtime_native import compile_run
from test_ordinary_airborne_native import program as airborne_program


def program():
    source = airborne_program().split('void setupAir(', 1)[0]
    source = source.replace('const float floor=-136.f;', 'const float floor=16.125f;')
    old = 'bool GetNavigationHull(AABB&out,NavFlags)const{out=AABB{};return true;}'
    assert old in source
    source = source.replace(old,
        'bool GetNavigationHull(AABB&out,NavFlags)const{out=AABB{};out.m_Mins[2]=-24.f;out.m_Maxs[2]=46.f;return true;}')
    return source + r'''
void setupSupport(FollowPath&f,User&u){
 f.client.position={1151.211f,2406.491f,89.259f};f.client.grounded=true;f.client.velocity={-14,62,0};
 f.m_Query.m_User=&u;
 f.m_CurrentPath.AddPt({1152,2438.728f,40.25f},35).PlayerOrigin().NavId(56);
 f.m_CurrentPath.AddPt({1088,2549.579f,40.25f},10).PlayerOrigin().NavId(49);
 f.m_TceReentryAttempts=1;f.m_TceReentryBestRemaining=500;f.m_TceReentryCheckpointId=420;
 f.m_TceReentryPendingProgress=true;f.m_TceReentryPosition=f.client.position;
}
bool logged(const char*s){for(const auto&l:logs)if(l.find(s)!=String::npos)return true;return false;}
int main(){
 static FollowPath perched;User owner;setupSupport(perched,owner);
 Vector3f support;
 assert(!perched.client.ResolveWalkDestination(perched.client.position,.25f,0,support));
 assert(!perched.client.CanWalkTo(perched.m_CurrentPath.GetPt(0)->m_Pt));
 const int queried=bodyQueries;const int before=IGame::GetTime();
 perched.client.buttons=BOT_BUTTON_JUMP;perched.Update(.05f);
 assert(perched.failures==0&&owner.successes==0&&perched.m_TceAirWaitActive&&bodyQueries>queried);
 assert(perched.m_CurrentPath.GetCurrentPtIndex()==0&&perched.client.resets==0);
 assert(!(perched.client.buttons&BOT_BUTTON_JUMP));
 assert((perched.client.steer.target-perched.client.position).SquaredLength()==0);
 assert(logged("ground=1 ground_supported=0")&&!logged("BLOCKED_ORDINARY"));
 assert(perched.m_TceAirWaitStart==before&&perched.m_TceReentryAttempts==1);
 // Another ground-flag tick on a player does not restart/renew anything.
 for(int i=0;i<5;++i){frame(perched);assert(perched.failures==0&&perched.client.resets==0);}
 assert(perched.m_TceAirWaitStart==before&&perched.m_TceReentryBestRemaining==500);
 perched.client.grounded=false;perched.client.velocity={0,0,-20};
 for(int i=0;i<7;++i){
  perched.client.velocity.z-=1000.f*.05f;
  perched.client.position.z+=perched.client.velocity.z*.05f;
  if(perched.client.position.z<=40.25f){perched.client.position.z=40.25f;perched.client.velocity={};perched.client.grounded=true;}
  frame(perched);
  assert(perched.failures==0&&owner.successes==0);
  if(!perched.client.grounded)assert(perched.m_TceAirWaitActive&&perched.client.resets==0&&perched.m_CurrentPath.GetCurrentPtIndex()==0);
 }
 assert(perched.client.grounded&&!perched.m_TceAirWaitActive&&logged("AIR_LANDED"));
 assert(perched.m_TceAirWaitStart==before&&IGame::GetTime()-before==600);
 assert(perched.m_CurrentPath.GetCurrentPtIndex()==1); // Outgoing body walk re-proved only now.
 assert(perched.m_CurrentPath.GetPt(0)->m_Pt.z==40.25f&&perched.m_CurrentPath.GetPt(0)->m_Radius==35);
 assert(perched.m_TceReentryAttempts==1&&perched.m_TceReentryBestRemaining==500&&perched.m_TceReentryCheckpointId==420);
 perched.client.position=perched.m_CurrentPath.GetPt(1)->m_Pt;frame(perched);
 assert(owner.successes==1&&perched.failures==0);

 // Valid actual world contact proceeds immediately through the same walk.
 logs.clear();static FollowPath world;User worldOwner;setupSupport(world,worldOwner);
 world.client.position.z=40.25f;frame(world);
 assert(world.failures==0&&!world.m_TceAirWaitActive&&world.m_CurrentPath.GetCurrentPtIndex()==1&&!logged("AIR_WAIT"));

 // Neither a false ground flag nor toggling it can certify missing/hazardous
 // support. One shared 750ms allowance, no goal success, stuck or retry reset.
 for(int kind=0;kind<4;++kind){
  logs.clear();collisionScenario=kind==0?0:kind==1?3:kind==2?4:5;
  static FollowPath invalid[4];User owners[4];auto&f=invalid[kind];setupSupport(f,owners[kind]);
  if(kind)f.client.position.z=40.25f;
  frame(f);const int started=f.m_TceAirWaitStart;
  for(int i=0;i<15;++i){
   f.client.grounded=i%2==0;frame(f);
   assert(f.m_TceAirWaitStart==started&&f.client.resets==0&&f.m_TceReentryAttempts==1);
   assert(owners[kind].successes==0&&f.m_CurrentPath.GetCurrentPtIndex()==0);
  }
  assert(f.failures==1&&IGame::GetTime()-started==750&&logged("unsupported_ground_timeout"));
  assert(f.m_TceReentryBestRemaining==500&&f.m_TceReentryCheckpointId==420);
 }
 printf("production Update/native support: false-ground perch, bounded shared wait, real landing/outgoing proof, no floor/water/hazard waiver or retry renewal passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class OrdinaryGroundSupportTests(TestCase):
    def test_actual_update_requires_fresh_static_body_support_before_landing(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
