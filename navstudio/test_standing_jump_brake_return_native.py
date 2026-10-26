"""Actual Update/Client walk replays Stadtrand's missed2u landing circle.

Observed grounded positions are independent inputs; the real controller and
full-body walk run against independent static planes. No PMove/live claim.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless

from test_door_runtime_native import compile_run
from test_standing_jump_ground_continue_native import program as ground_program


def program():
    source = ground_program().split('int main(){', 1)[0]
    source = source.replace('const Vector3f n{-.0556942f,-.0795398f,.9952746f};',
                            'const Vector3f n{0,0,1};')
    source = source.replace('const Vector3f hoodContact{2100.9531f,1970.2024f,37.1101f};',
                            'const Vector3f hoodContact{-246.2368f,101.0325f,84.f};')
    source = source.replace('b.x>=2090&&b.x<=2096', 'b.y>=100&&b.y<=104')
    source = source.replace('(scenario==3?80.f:100.f)', '(scenario==3?120.f:200.f)')
    source = source.replace('if(scenario==1)clip({-1,0,0},-2089.f);',
                            'if(scenario==1)clip({0,-1,0},-98.f);')
    # Observe the complete real Update's actual steering strength too. This
    # checks braking admission, without inventing physics from input strength.
    source = source.replace('Vector3f target;void SetTarget(const Vector3f&p,float=0,int=0,bool=false,float=1){target=p;}',
                            'Vector3f target;float scale=1;void SetTarget(const Vector3f&p,float=0,int=0,bool=false,float input=1){target=p;scale=input;}')
    return source + r'''
int main(){
 Waypoint launch{704,{-240.2368f,173.0325f,64.25f}},roof{408,{-246.2368f,101.0325f,84.25f}};
 launch.props.p={{"tce_stand_jump","1"},{"tce_stand_jump_target","408"}};
 launch.flags=F_NAV_JUMP;launch.radius=roof.radius=2;planner.m_WaypointList={&launch,&roof};
 Vector3f dir=(roof.pos-launch.pos).Flatten();const float length=dir.Normalize();
 auto setup=[&](FollowPath&f,User&u){
  f.client.passable=true;planner.m_Client=&f.client;planner.m_Solution={&roof,&launch};
  planner.GetPath(f.m_CurrentPath,0);f.m_Query.m_User=&u;
  f.m_TceStandNavId=704;f.m_TceStandPhase=2;f.m_TceStandAttempt=IGame::time;
  f.client.position={-240.408f,171.445f,76.377f};f.client.velocity={-3,-41,225};f.client.onGround=false;
  frame(f);assert(f.m_CurrentPath.GetCurrentPtIndex()==1&&f.failures==0&&u.successes==0);
 };
 FollowPath f;User u;setup(f,u);IGame::time+=450;f.client.onGround=true;
 f.client.position={-243.020f,138.310f,84.125f};f.client.velocity={-7,-87,0};frame(f);
 assert(f.m_TceStandPhase==3&&f.failures==0&&u.successes==0);
 // Last pre-overshoot sample is2.075u from the target; it cannot consume
 // an authored2u arrival. Input tapers rather than pushing full ground speed.
 f.client.position={-246.059f,103.100f,84.125f};f.client.velocity={-12,-142,0};frame(f);
 assert(f.failures==0&&u.successes==0&&f.client.steer.scale<.07f);
 // Exact observed5.069u overshoot remains a pending physically supported
 // ordinary return. Reaching it is NEVER a relaxed-radius successful LAND.
 f.client.position={-246.674f,95.983f,84.125f};frame(f);
 assert(f.failures==0&&u.successes==0&&f.m_CurrentPath.GetCurrentPtIndex()==1);
 assert(f.client.steer.scale>.15f&&f.client.steer.scale<.17f);
 assert((f.client.steer.target-roof.pos).SquaredLength()<.00001f);
 assert(!(f.client.buttons&BOT_BUTTON_JUMP)&&!(f.client.buttons&BOT_BUTTON_TCE_STAND_LAUNCH));
 const int before=bodyqueries;
 f.client.position={-246.450f,98.500f,84.125f};f.client.velocity={3,35,0};frame(f);
 assert(f.failures==0&&u.successes==0&&bodyqueries>before);
 f.client.position={-246.300f,100.100f,84.125f};f.client.velocity={3,25,0};frame(f);
 assert(f.failures==0&&u.successes==1);
 // The endcap is finite. 15.9u can brake back only with a fresh full-body
 // proof;16.1u cannot continue, even on an otherwise empty safe floor.
 for(int past=0;past<2;++past){
  FollowPath end;User owner;setup(end,owner);IGame::time+=500;end.client.onGround=true;
  end.client.position=roof.pos+dir*(past?16.1f:15.9f);end.client.position.z=84.125f;
  frame(end);assert(owner.successes==0&&end.failures==past);
 }
 // A return through wall, missing actual support, insufficient headroom,
 // lava or swimming is still refused by the complete production Client walk.
 for(int bad=1;bad<=5;++bad){
  FollowPath blocked;User owner;scenario=0;setup(blocked,owner);IGame::time+=500;
  blocked.client.onGround=true;blocked.client.position={-246.674f,95.983f,84.125f};
  scenario=bad;frame(blocked);assert(blocked.failures==1&&owner.successes==0);
 }
 scenario=0;
 FollowPath timeout;User timeoutOwner;setup(timeout,timeoutOwner);IGame::time+=2000;
 timeout.client.onGround=true;timeout.client.position={-246.674f,95.983f,84.125f};frame(timeout);
 assert(timeout.failures==1&&timeoutOwner.successes==0);
 printf("real full Update + Client: observed Stad5.069u miss brakes/returns without enlarged arrival;16u cap, actual wall/support/headroom/hazards and original deadline remain mandatory\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class StandingJumpBrakeReturnTests(TestCase):
    def test_observed_overshoot_requires_bounded_supported_return_to_original_radius(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
