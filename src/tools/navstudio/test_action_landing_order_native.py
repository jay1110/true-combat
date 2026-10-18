"""Full Update/native walk: an action must land before its outgoing walk gate.

Delta's actual UID493 -> UID56 -> UID49 coordinates reproduce the premature
ordinary failure. Collision planes and movement observations are independent;
this is a controller regression, not a live traversal/physics certification.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_door_runtime_native import compile_run
from test_destination_volume_native import program as destination_program


def program():
    source = destination_program().rsplit('void setup(FollowPath&', 1)[0]
    source = source.replace('int collisionScenario=0,bodyQueries=0;',
        'int collisionScenario=0,bodyQueries=0;float baseFloor=10.f;')
    source = source.replace('const float floor=-8.f;', 'const float floor=b.y>2460.f?6.f:baseFloor;')
    source = source.replace('Vector3f GetVelocity(){return{};}', 'Vector3f velocity;Vector3f GetVelocity(){return velocity;}')
    source = source.replace('bool HasEntityFlag(int flag)const{return flag==ENT_FLAG_ONGROUND;}',
        'bool grounded=true;bool HasEntityFlag(int flag)const{return flag==ENT_FLAG_ONGROUND&&grounded;}')
    return source + r'''
bool logged(const char*s){for(const auto&l:logs)if(l.find(s)!=String::npos)return true;return false;}
void dropPath(FollowPath&f,User&u){
 f.m_Query.m_User=&u;f.client.debug=false;f.client.grounded=false;
 f.client.position={1186.401f,2438.738f,62.125f};f.client.velocity={-109,0,0};
 f.m_CurrentPath.AddPt({1260,2438.7275f,62.25f},2).PlayerOrigin().NavId(493).TceWalkDrop(true);
 f.m_CurrentPath.AddPt({1152,2438.7275f,40.25f},35).PlayerOrigin().NavId(56);
 f.m_CurrentPath.AddPt({1088,2549.5789f,36.25f},10).PlayerOrigin().NavId(49);
 f.m_CurrentPath.NextPt();
}
void risingPath(FollowPath&f,User&u,bool climb){
 f.m_Query.m_User=&u;f.client.debug=false;f.client.grounded=false;
 f.client.position={1260,2438.7275f,83.125f};f.client.velocity={80,0,-50};
 f.m_CurrentPath.AddPt({1200,2438.7275f,40.25f},2).PlayerOrigin().NavId(500).TceStandJump(!climb).TceFreeClimb(climb);
 f.m_CurrentPath.AddPt({1280,2438.7275f,62.25f},35).PlayerOrigin().NavId(501);
 f.m_CurrentPath.AddPt({1350,2438.7275f,62.25f},10).PlayerOrigin().NavId(502);
 f.m_CurrentPath.NextPt();
 f.m_TceStandNavId=500;f.m_TceStandPhase=2;f.m_TceStandAttempt=IGame::GetTime();
 f.m_TceFreeNavId=500;f.m_TceFreePhase=1;f.m_TceFreeStart=IGame::GetTime();
}
int main(){
 static FollowPath drop;User dropOwner;dropPath(drop,dropOwner);
 assert(!drop.client.CanWalkTo(drop.m_CurrentPath.GetPt(1)->m_Pt));
 const int queries=bodyQueries;frame(drop);
 assert(drop.failures==0&&dropOwner.successes==0&&drop.m_CurrentPath.GetCurrentPtIndex()==1);
 assert(bodyQueries==queries&&!drop.m_TceAirWaitActive&&!logged("BLOCKED_ORDINARY"));
 assert(drop.client.resets==0); // Active drop owns flight, not the ordinary air wait.
 for(int i=0;i<3;++i){drop.client.position.x-=4;drop.client.position.z-=4;frame(drop);assert(drop.failures==0&&drop.m_CurrentPath.GetCurrentPtIndex()==1);}
 drop.client.position={1154,2438.7275f,40.125f};drop.client.grounded=true;drop.client.velocity={};frame(drop);
 assert(drop.failures==0&&drop.m_CurrentPath.GetCurrentPtIndex()==2&&bodyQueries>queries);
 drop.client.position={1088,2549.5789f,36.125f};frame(drop);assert(dropOwner.successes==1);

 // Actual action arrival still needs the full outgoing floor/body proof.
 logs.clear();static FollowPath lost;User lostOwner;dropPath(lost,lostOwner);frame(lost);
 collisionScenario=3;lost.client.position={1152,2438.7275f,40.125f};lost.client.grounded=true;frame(lost);
 // At the ordinary outgoing gate a false ground flag has the same bounded
 // support wait; it cannot consume the action landing or waive its walk.
 const int lostSupportWait=lost.m_TceAirWaitStart;
 for(int i=0;i<15&&lost.failures==0;++i)frame(lost);
 assert(lost.failures==1&&lostOwner.successes==0&&lost.m_CurrentPath.GetCurrentPtIndex()==1&&logged("BLOCKED_ORDINARY"));
 assert(IGame::GetTime()-lostSupportWait==750&&logged("unsupported_ground_timeout"));
 collisionScenario=0;static FollowPath timeout;User timeoutOwner;dropPath(timeout,timeoutOwner);frame(timeout);
 IGame::time+=4100;frame(timeout);assert(timeout.failures==1&&timeoutOwner.successes==0);

 // Standing jump and free climb have the same necessary ordering. Neither
 // their airborne arrival nor their supported outgoing leg is exempted.
 baseFloor=32;
 for(int climb=0;climb<2;++climb){
  logs.clear();static FollowPath action[2];User owner[2];auto&f=action[climb];risingPath(f,owner[climb],climb!=0);
  const int before=bodyQueries;frame(f);
  assert(f.failures==0&&owner[climb].successes==0&&bodyQueries==before&&f.m_CurrentPath.GetCurrentPtIndex()==1);
  assert(!f.m_TceAirWaitActive&&!logged("BLOCKED_ORDINARY"));
  f.client.position={1279,2438.7275f,62.125f};f.client.grounded=true;f.client.velocity={};frame(f);
  assert(f.failures==0&&f.m_CurrentPath.GetCurrentPtIndex()==2&&bodyQueries>before);
  f.client.position={1350,2438.7275f,62.125f};frame(f);assert(owner[climb].successes==1);
 }
 // Ground contact at the wrong elevation is still a standing-jump failure.
 logs.clear();static FollowPath bad;User badOwner;risingPath(bad,badOwner,false);
 IGame::time+=150;bad.client.grounded=true;bad.client.position.z=72.125f;frame(bad);
 assert(bad.failures==1&&badOwner.successes==0&&logged("unsupported_contact_or_timeout"));
 printf("complete Update + native walk: real Delta drop coordinates, supported outgoing proof, missing floor, timeout, stand-jump/free-climb landings and bad-height contact passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class ActionLandingOrderTests(TestCase):
    def test_action_landing_must_precede_ordinary_transition_proof(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
