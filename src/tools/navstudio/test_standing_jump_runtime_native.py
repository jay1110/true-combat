"""Actual complete FollowPath Update for directed standing-jump recipes.

Independent actor observations test control and arrival, not game physics.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_door_update_native import program as update_program
from test_door_runtime_native import COMMON, body, compile_run


def program():
    source = update_program().split('int main(){', 1)[0]
    source = source.replace('bool debug=true,passable=false',
                            'bool onGround=true,onLadder=false,debug=true,passable=false')
    source = source.replace('return flag==ENT_FLAG_ONGROUND;',
                            'return flag==ENT_FLAG_ONGROUND?onGround:flag==ENT_FLAG_ONLADDER?onLadder:false;')
    source = source.replace('Vector3f GetVelocity(){return{};}',
                            'Vector3f velocity;Vector3f GetVelocity(){return velocity;}')
    source = source.replace('StateStatus Update(float);',
                            'void ResetTceStandingJump();StateStatus Update(float);')
    follow = (COMMON/'BotBaseStates.cpp').read_text()
    source += body(follow, 'void FollowPath::ResetTceStandingJump()')
    for marker in ('void FollowPath::Stop(', 'bool FollowPath::Repath()',
                   'bool FollowPath::Goto(FollowPathUser *_owner, const Path &_path,'):
        assert 'ResetTceStandingJump();' in body(follow, marker), marker
    return source + r'''
int main(){
 Waypoint launch{1,{0,0,30}},landing{2,{64,0,62}},retreat{3,{-64,0,30}};
 launch.props.p={{"tce_stand_jump","1"},{"tce_stand_jump_target","2"}};launch.flags=F_NAV_JUMP;
 launch.radius=2;landing.radius=6;planner.m_WaypointList={&launch,&landing,&retreat};
 FollowPath f;User owner;f.client.passable=true;planner.m_Client=&f.client;planner.m_Solution={&landing,&launch};
 planner.GetPath(f.m_CurrentPath,0);assert(f.m_CurrentPath.GetPt(0)->m_TceStandJump);
 f.m_Query.m_User=&owner;
 // The ordinary predecessor's radius permits a real lateral approach.
 // Reaching the precise braking point must precede launch-shaft checks.
 f.client.position={-24,20,30};frame(f);
 assert(f.failures==0&&f.m_TceStandPhase==0&&!(f.client.buttons&BOT_BUTTON_JUMP));
 assert(f.client.steer.target.x==-8&&f.client.steer.target.y==0);
 f.client.position={-8,0,30};frame(f);frame(f);frame(f);
 assert(f.m_TceStandPhase==1&&owner.successes==0);
 frame(f);assert(f.client.buttons&BOT_BUTTON_TCE_STAND_PRIME);
 f.client.position={0,0,30};f.client.velocity={35,0,0};frame(f);
 assert(f.m_TceStandPhase==1&&!(f.client.buttons&BOT_BUTTON_TCE_STAND_LAUNCH));
 f.client.velocity={45,0,0};frame(f);
 assert(f.m_TceStandPhase==1&&!(f.client.buttons&BOT_BUTTON_TCE_STAND_LAUNCH));
 f.client.position={0,0,30};f.client.velocity={40,0,0};frame(f);
 assert(f.m_TceStandPhase==2&&(f.client.buttons&BOT_BUTTON_TCE_STAND_LAUNCH));
 assert(!(f.client.buttons&BOT_BUTTON_TCE_STAND_PRIME)&&owner.successes==0);
 // Space is already committed: forward motion can pass the priming band's
 // +3 limit before the first airborne observation. It must transfer to the
 // checked landing instead of aborting the path while physically rising.
 f.client.onGround=false;f.client.position={4,0,40};f.client.velocity={40,0,320};frame(f);
 assert(f.m_CurrentPath.GetCurrentPtIndex()==1&&owner.successes==0);
 f.client.position={64,0,72};f.client.velocity={114,0,-40};frame(f);
 assert(owner.successes==0&&(f.client.buttons&BOT_BUTTON_TCE_STAND_LAUNCH));
 f.client.onGround=true;f.client.position={64,0,62};f.client.velocity={114,0,0};frame(f);
 assert(owner.successes==1&&f.failures==0);
 int launches=0,landings=0;for(const auto&s:logs){launches+=s.find("TCE2STANDJUMP LAUNCH ")!=String::npos;landings+=s.find("TCE2STANDJUMP LAND ")!=String::npos;}
 assert(launches==1&&landings==1);
 // Metadata is directed; the walking retreat from this exact launch must
 // not inherit Space or the special standing controller.
 Path ordinary;planner.m_Solution={&retreat,&launch};planner.GetPath(ordinary,0);
 assert(!ordinary.GetPt(0)->m_TceStandJump&&!(ordinary.GetPt(0)->m_NavFlags&F_NAV_JUMP));
 // Independently observed lower-floor contact is not a successful ascent.
 FollowPath low;User lowOwner;low.client.passable=true;planner.m_Client=&low.client;
 planner.m_Solution={&landing,&launch};planner.GetPath(low.m_CurrentPath,0);low.m_Query.m_User=&lowOwner;
 low.m_CurrentPath.NextPt();low.m_TceStandAttempt=IGame::time-200;
 low.m_TceStandPhase=2;low.m_TceStandNavId=1;low.client.position={64,0,30};frame(low);
 assert(lowOwner.successes==0&&low.failures==1);
 // Passing the correct coordinates in the air, including ladder contact,
 // cannot finish; an absent landing fails after the bounded observation.
 FollowPath air;User airOwner;air.client.passable=true;planner.m_Client=&air.client;
 planner.m_Solution={&landing,&launch};planner.GetPath(air.m_CurrentPath,0);air.m_Query.m_User=&airOwner;
 air.m_CurrentPath.NextPt();air.m_TceStandAttempt=IGame::time;air.client.position={64,0,62};
 air.client.onGround=false;air.client.onLadder=true;frame(air);
 assert(airOwner.successes==0&&air.failures==0);
 IGame::time+=2000;frame(air);assert(airOwner.successes==0&&air.failures==1);
 // The narrow launch band is still mandatory once priming has begun.
 FollowPath side;User sideOwner;side.client.passable=true;planner.m_Client=&side.client;
 planner.m_Solution={&landing,&launch};planner.GetPath(side.m_CurrentPath,0);side.m_Query.m_User=&sideOwner;
 side.m_TceStandNavId=1;side.m_TceStandPhase=1;side.client.position={0,20,30};frame(side);
 assert(side.failures==1&&sideOwner.successes==0);
 // A fresh query with the same launch ID cannot reuse a previous phase2
 // or old timer after its original path was interrupted or replanned.
 f.ResetTceStandingJump();assert(!f.m_TceStandNavId&&!f.m_TceStandPhase&&!f.m_TceStandStart&&!f.m_TceStandStable&&!f.m_TceStandAttempt);
 f.m_CurrentPath.Clear();f.m_PathStatus=FollowPath::PathInProgress;f.client.position={-24,20,30};
 f.client.velocity={0,0,0};planner.m_Client=&f.client;planner.m_Solution={&landing,&launch};planner.GetPath(f.m_CurrentPath,0);
 frame(f);assert(f.m_TceStandPhase==0&&f.failures==0&&!(f.client.buttons&BOT_BUTTON_TCE_STAND_LAUNCH));
 printf("actual full Update: lateral approach, braking, exact priming, airborne transfer and real-height arrival verified\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class StandingJumpRuntimeTests(TestCase):
    def test_actual_full_update_requires_checked_launch_and_actual_landing(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
