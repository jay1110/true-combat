"""Actual complete Update replays the failed second Stadtrand hood launch.

Independent actor observations exercise admission to priming, the unchanged
physical launch gates and actual landing. This does not simulate PMove or claim
that the fixed runtime has passed a live route.
"""
import os
import shutil
from unittest import TestCase,main,skipUnless

from test_standing_jump_runtime_native import program as standing_program
from test_door_runtime_native import compile_run


def program():
    source=standing_program().split('int main(){',1)[0]
    return source+r'''
int main(){
 Waypoint middle{703,{-246.2368f,173.0325f,64.25f}},launch{704,{-240.2368f,173.0325f,64.25f}},roof{408,{-246.2368f,101.0325f,84.25f}};
 launch.props.p={{"tce_stand_jump","1"},{"tce_stand_jump_target","408"}};
 launch.flags=F_NAV_JUMP;middle.radius=launch.radius=roof.radius=2;
 planner.m_WaypointList={&middle,&launch,&roof};
 Vector3f direction=(roof.pos-launch.pos).Flatten();direction.Normalize();
 const Vector3f stop=launch.pos-direction*8.f;
 const Vector3f perpendicular{-direction.y,direction.x,0};
 auto setup=[&](FollowPath&f,User&u){
  f.client.passable=true;planner.m_Client=&f.client;planner.m_Solution={&roof,&launch};
  planner.GetPath(f.m_CurrentPath,0);f.m_Query.m_User=&u;
  assert(f.m_CurrentPath.GetPt(0)->m_TceStandJump);
 };
 // Exact observed stopping pose on the successful first jump's hood:
 // 0.97185u from stop, with0.91308u lateral error. Old admission<=1u
 // primed here although its later Space gate correctly required<=.5u.
 FollowPath observed;User observedUser;setup(observed,observedUser);
 observed.client.position={-240.510f,180.749f,64.125f};
 for(int i=0;i<5;++i)frame(observed);
 assert(observed.m_TceStandPhase==0&&observed.m_TceStandStable==0);
 assert(!(observed.client.buttons&BOT_BUTTON_TCE_STAND_PRIME)&&observed.failures==0);
 assert((observed.client.steer.target-stop).SquaredLength()<.00001f);
 // Admission is the exact2D braking circle. A .13u residual still centres;
 // actual current waypoint/goal radii remain2 and are not broadened.
 observed.client.position=stop+perpendicular*.13f;frame(observed);
 assert(observed.m_TceStandPhase==0&&observed.m_TceStandStable==0);
 assert(observed.m_CurrentPath.GetPt(0)->m_Radius==2.f);
 // A precise stopped observation first owns100ms of grounded braking.
 observed.client.position=stop+perpendicular*.10f;observed.client.velocity={0,0,0};
 frame(observed);assert(observed.m_TceStandPhase==0);
 frame(observed);assert(observed.m_TceStandPhase==0);
 frame(observed);assert(observed.m_TceStandPhase==1);
 frame(observed);assert(observed.client.buttons&BOT_BUTTON_TCE_STAND_PRIME);
 // Physical actor observations for the centred diagonal pass. Native
 // endpoint, speed, Z, real ground and lateral gates all remain mandatory.
 observed.client.position=launch.pos-direction*.2f+perpendicular*.1f;
 observed.client.position.z=64.125f;observed.client.velocity=direction*40.f;
 frame(observed);assert(observed.m_TceStandPhase==2&&observed.failures==0);
 assert(observed.client.buttons&BOT_BUTTON_TCE_STAND_LAUNCH);
 observed.client.onGround=false;observed.client.position=launch.pos+direction*4.f;
 observed.client.position.z+=10;observed.client.velocity=direction*40.f+Vector3f{0,0,300};
 frame(observed);assert(observed.m_CurrentPath.GetCurrentPtIndex()==1&&observedUser.successes==0);
 observed.client.position=roof.pos;observed.client.position.z+=5;observed.client.velocity={0,0,-30};
 frame(observed);assert(observedUser.successes==0);
 observed.client.onGround=true;observed.client.position=roof.pos;observed.client.position.z-=.125f;
 observed.client.velocity={0,0,0};frame(observed);assert(observedUser.successes==1&&observed.failures==0);
 // Replay the actual failed near-launch samples with the old phase already
 // active: these MUST still refuse Space, not be accepted by widening a gate.
 FollowPath drift;User driftUser;setup(drift,driftUser);
 drift.m_TceStandNavId=704;drift.m_TceStandPhase=1;drift.m_TceStandStart=IGame::time;
 drift.client.position={-240.841f,173.301f,64.125f};drift.client.velocity={-2,-40,0};frame(drift);
 assert(drift.m_TceStandPhase==1&&!(drift.client.buttons&BOT_BUTTON_TCE_STAND_LAUNCH));
 drift.client.position={-240.929f,171.319f,64.125f};frame(drift);
 assert(drift.m_TceStandPhase==1&&!(drift.client.buttons&BOT_BUTTON_TCE_STAND_LAUNCH));
 drift.client.position={-241.018f,169.336f,64.125f};frame(drift);
 assert(drift.failures==1&&driftUser.successes==0);
 // A moving or airborne actor inside the new circle cannot start priming.
 FollowPath moving;User movingUser;setup(moving,movingUser);
 moving.client.position=stop;moving.client.velocity={2,0,0};
 for(int i=0;i<4;++i)frame(moving);assert(moving.m_TceStandPhase==0);
 moving.client.velocity={0,0,0};moving.client.onGround=false;
 for(int i=0;i<4;++i)frame(moving);assert(moving.m_TceStandPhase==0);
 // No new unbounded centring retry: existing15s approach deadline still
 // stops an actor that never acquires the precise physical stopping point.
 FollowPath timeout;User timeoutUser;setup(timeout,timeoutUser);
 timeout.client.position={-240.510f,180.749f,64.125f};frame(timeout);
 IGame::time+=15001;frame(timeout);assert(timeout.failures==1&&timeoutUser.successes==0);
 printf("full Update: observed .913u side error cannot prime; centred diagonal launch/real landing, original side gate and bounded timeout passed\n");
}
'''


@skipUnless(os.name=='nt'and shutil.which('cmake'),'Windows CMake/MSVC required')
class StandingJumpCentreTests(TestCase):
    def test_actual_second_hood_launch_requires_precise_braking_before_priming(self):
        compile_run(self,program())


if __name__=='__main__':main()
