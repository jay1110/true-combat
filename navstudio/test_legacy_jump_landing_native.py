"""Complete Update: actual Delta legacy JUMP -> ordinary landing -> walk.

UID493 is a plain JUMP, not a generated walk-drop. Independent falling samples
exercise the real controller and full native walk; this is not live physics.
"""
import os
import re
import shutil
from unittest import TestCase, main, skipUnless
from test_door_runtime_native import COMMON, compile_run
from test_action_landing_order_native import program as action_program


def program():
    source = action_program().split('bool logged(const char*s)', 1)[0]
    jump_bit = re.search(r'F_NAV_JUMP\s*=\s*\(NavFlags\(1\)<<([0-9]+)\)',
        (COMMON / 'NavigationFlags.h').read_text()).group(1)
    assert jump_bit == '27'  # Actual saved Delta WAY flag 134217728.
    source = source.replace('F_NAV_JUMP=2,', f'F_NAV_JUMP=(1<<{jump_bit}),')
    source = source.replace('void AddAimRequest(int,void*,int){}',
        'int priority=-1;void AddAimRequest(int p,void*,int){priority=p;}')
    return source + r'''
bool logged(const char*s){for(const auto&l:logs)if(l.find(s)!=String::npos)return true;return false;}
void legacy(FollowPath&f,User&u,bool previous=true){
 f.m_Query.m_User=&u;f.client.debug=false;f.client.grounded=false;
 f.client.position={1186.676f,2438.697f,62.125f};f.client.velocity={-90,0,0};
 if(previous)f.m_CurrentPath.AddPt({1260,2438.7275f,62.25f},8).PlayerOrigin().NavId(493).Flags(F_NAV_JUMP);
 f.m_CurrentPath.AddPt({1152,2438.7275f,40.25f},35).PlayerOrigin().NavId(56);
 f.m_CurrentPath.AddPt({1156,2344,40.25f},35).PlayerOrigin().NavId(420);
 if(previous)f.m_CurrentPath.NextPt();
}
int main(){
 static FollowPath jump;User owner;legacy(jump,owner);const int before=bodyQueries;frame(jump);
 assert(jump.failures==0&&jump.m_TceAirWaitActive&&jump.m_CurrentPath.GetCurrentPtIndex()==1&&bodyQueries==before);
 assert(jump.client.steer.target.x==1152&&jump.client.steer.target.y==2438.7275f&&jump.client.steer.radius==0);
 assert(jump.client.aimer.priority==Priority::High);
 assert(!(jump.client.buttons&BOT_BUTTON_JUMP)&&!logged("BLOCKED_ORDINARY"));
 // Body clears the upper ledge under continuing horizontal flight input.
 // Do not consume the wide-radius landing or begin the next Y turn midair.
 for(int i=0;i<6;++i){
  jump.client.position.x-=4.5f;jump.client.velocity.z-=40;
  jump.client.position.z+=jump.client.velocity.z*.05f;
  if(jump.client.position.z<=40.125f){jump.client.position.z=40.125f;jump.client.grounded=true;jump.client.velocity={};}
  frame(jump);assert(jump.failures==0);
  if(!jump.client.grounded){assert(jump.m_CurrentPath.GetCurrentPtIndex()==1&&bodyQueries==before);assert(jump.client.steer.target.x==1152&&jump.client.steer.target.y==2438.7275f);}
 }
 assert(jump.client.grounded&&jump.m_CurrentPath.GetCurrentPtIndex()==2&&bodyQueries>before&&owner.successes==0);

 // Query entry directly at the landing has no proved preceding jump. It
 // waits without driving, still cannot consume/validate from an air origin.
 logs.clear();static FollowPath direct;User directOwner;legacy(direct,directOwner,false);
 const int queries=bodyQueries;frame(direct);
 assert(direct.failures==0&&direct.m_TceAirWaitActive&&direct.m_CurrentPath.GetCurrentPtIndex()==0&&bodyQueries==queries);
 assert((direct.client.steer.target-direct.client.position).SquaredLength()==0);
 direct.client.grounded=true;direct.client.position={1160,2438.7f,40.125f};frame(direct);
 assert(direct.failures==0&&direct.m_CurrentPath.GetCurrentPtIndex()==1&&bodyQueries>queries);

 // A grounded observation outside the arrival radius ends the old episode.
 // A subsequent fall has its own full deadline, rather than a stale timeout.
 static FollowPath renewed;User renewedOwner;legacy(renewed,renewedOwner,false);frame(renewed);
 const int oldStart=renewed.m_TceAirWaitStart;IGame::time+=700;
 renewed.client.grounded=true;renewed.client.position={1250,2438.7f,40.125f};frame(renewed);
 assert(!renewed.m_TceAirWaitActive&&renewed.failures==0);
 renewed.client.grounded=false;renewed.client.position={1186.676f,2438.697f,62.125f};frame(renewed);
 assert(renewed.m_TceAirWaitActive&&renewed.m_TceAirWaitStart>oldStart&&renewed.failures==0);

 // Door metadata can also describe an ordinary retreat, not an active
 // passage. Its eventual native walk proof has the same ground contract.
 static FollowPath retreat;User retreatOwner;legacy(retreat,retreatOwner,false);
 auto*retreatPoint=const_cast<Path::PathPoint*>(retreat.m_CurrentPath.GetPt(0));
 retreatPoint->m_NavFlags=F_NAV_DOOR;retreatPoint->m_TceDoor.marked=true;retreatPoint->m_TceDoor.role=3;
 const int retreatQueries=bodyQueries;frame(retreat);
 assert(retreat.failures==0&&retreat.m_TceAirWaitActive&&bodyQueries==retreatQueries);

 // Unknown support / indefinite flight remains bounded, and an asserted
 // landing with a vanished outgoing floor remains a real proof failure.
 logs.clear();static FollowPath timeout;User timeoutOwner;legacy(timeout,timeoutOwner);frame(timeout);
 IGame::time+=750;frame(timeout);assert(timeout.failures==1&&logged("airborne_timeout")&&timeoutOwner.successes==0);
 assert(logged("user=fixture current_radius=35.000 next_radius=35.000 previous=493 previous_flags=134217728 destination_volume=0"));
 assert(logged("current_target=(1152.000 2438.728 40.250)"));
 assert((timeout.client.steer.target-timeout.client.position).SquaredLength()==0);
 logs.clear();static FollowPath lost;User lostOwner;legacy(lost,lostOwner);frame(lost);
 lost.client.grounded=true;lost.client.position={1160,2438.7f,40.125f};collisionScenario=3;frame(lost);
 assert(lost.failures==0&&lostOwner.successes==0&&lost.m_TceAirWaitActive);
 const int lostStart=lost.m_TceAirWaitStart;
 for(int i=0;i<15&&lost.failures==0;++i)frame(lost);
 assert(lost.failures==1&&lostOwner.successes==0&&logged("unsupported_ground_timeout"));
 assert(IGame::GetTime()-lostStart==750);
 assert(logged("user=fixture current_radius=35.000 next_radius=35.000 previous=493 previous_flags=134217728 destination_volume=0"));
 assert(logged("current_target=(1152.000 2438.728 40.250)"));
 printf("actual Delta legacy JUMP metadata: bounded landing air-control, no premature next-leg proof, direct-entry hold, fresh ground proof, timeout and missing-floor rejection passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class LegacyJumpLandingTests(TestCase):
    def test_legacy_jump_and_direct_entry_share_ground_gate_without_losing_air_control(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
