"""Actual full FollowPath Update: directed free-climb launch/contact/landing.

Observations come from independent actor states; no physics is simulated here.
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
    source = source.replace('StateStatus Update(float);', 'void ResetTceFreeClimb();StateStatus Update(float);')
    follow = (COMMON/'BotBaseStates.cpp').read_text()
    source += body(follow, 'void FollowPath::ResetTceFreeClimb()')
    for marker in ('void FollowPath::Stop(', 'bool FollowPath::Repath()',
                   'bool FollowPath::Goto(FollowPathUser *_owner, const Path &_path,'):
        assert 'ResetTceFreeClimb();' in body(follow, marker), marker
    return source + r'''
int main(){
 Waypoint launch{1,{0,0,30}},landing{2,{64,0,132}},retreat{3,{-64,0,30}};
 launch.props.p={{"tce_free_climb","1"},{"tce_free_climb_target","2"}};launch.flags=F_NAV_CLIMB;
 launch.radius=2;landing.radius=6;planner.m_WaypointList={&launch,&landing,&retreat};
 FollowPath f;User owner;f.client.passable=true;planner.m_Client=&f.client;planner.m_Solution={&landing,&launch};
 planner.GetPath(f.m_CurrentPath,0);assert(f.m_CurrentPath.GetPt(0)->m_TceFreeClimb);
 stats[{1,"tce_free_climb_stamina"}]=2500;stats[{1,"tce_free_climb_ready"}]=0;
 f.m_Query.m_User=&owner;
 // A real normal waypoint can be consumed with its 35-unit tolerance,
 // leaving a side offset larger than the eight-unit climbing shaft.
 // The exact launch must still be approached instead of failing the path.
 f.client.position={-24,20,30};frame(f);
 assert(f.failures==0&&f.m_TceFreePhase==0&&!(f.client.buttons&BOT_BUTTON_JUMP));
 assert(f.client.steer.target.x==0&&f.client.steer.target.y==0);
 f.client.position={0,0,30};frame(f);assert(f.m_TceFreePhase==0&&!(f.client.buttons&BOT_BUTTON_JUMP));
 stats[{1,"tce_free_climb_ready"}]=1;frame(f);
 assert(f.m_TceFreePhase==1&&(f.client.buttons&BOT_BUTTON_JUMP)&&(f.client.buttons&BOT_BUTTON_TCE_FREECLIMB));
 assert(f.m_CurrentPath.GetCurrentPtIndex()==0&&owner.successes==0);
 f.client.onGround=false;f.client.onLadder=true;f.client.position={0,0,60};frame(f);
 assert(f.m_CurrentPath.GetCurrentPtIndex()==1&&(f.client.buttons&BOT_BUTTON_TCE_FREECLIMB)&&!(f.client.buttons&BOT_BUTTON_JUMP));
 f.client.onLadder=false;f.client.position={16,0,140};frame(f);
 assert(f.m_CurrentPath.GetCurrentPtIndex()==1&&owner.successes==0&&(f.client.buttons&BOT_BUTTON_TCE_FREECLIMB));
 f.client.onGround=true;f.client.position={24,0,132};stats[{1,"tce_free_climb_stamina"}]=1800;frame(f);
 assert(owner.successes==0&&!(f.client.buttons&BOT_BUTTON_TCE_FREECLIMB)&&f.client.steer.target.x==64);
 f.client.position={64,0,132};frame(f);assert(owner.successes==1&&f.failures==0);
 // A new path using the same launch UID must start from fresh physical
 // observations. Execute the exact lifecycle-reset helper then full Update.
 f.ResetTceFreeClimb();assert(f.m_TceFreeNavId==0&&f.m_TceFreePhase==0&&f.m_TceFreeStart==0);
 f.m_CurrentPath.Clear();f.m_PathStatus=FollowPath::PathInProgress;f.client.position={0,0,30};f.client.onGround=true;
 planner.m_Solution={&landing,&launch};planner.GetPath(f.m_CurrentPath,0);
 stats[{1,"tce_free_climb_stamina"}]=2500;stats[{1,"tce_free_climb_ready"}]=1;
 frame(f);assert(f.m_TceFreePhase==1&&(f.client.buttons&BOT_BUTTON_JUMP));
 // Replanning during actual ladder contact resumes the original physical
 // climb without issuing Space or retaining the abandoned path's phase.
 f.ResetTceFreeClimb();f.client.onGround=false;f.client.onLadder=true;f.client.position={0,0,62};
 frame(f);assert(f.m_CurrentPath.GetCurrentPtIndex()==1&&f.m_TceFreePhase==1);
 assert((f.client.buttons&BOT_BUTTON_TCE_FREECLIMB)&&!(f.client.buttons&BOT_BUTTON_JUMP));
 Path ordinary;planner.m_Solution={&retreat,&launch};planner.GetPath(ordinary,0);
 assert(!ordinary.GetPt(0)->m_TceFreeClimb&&!(ordinary.GetPt(0)->m_NavFlags&F_NAV_CLIMB));
 // A low/high position failure cannot become completion simply by entering
 // the endpoint's XY radius while airborne.
 FollowPath bad;User badOwner;bad.client.passable=true;bad.client.onGround=false;
 bad.client.position={64,0,30};planner.m_Client=&bad.client;planner.m_Solution={&landing,&launch};
 planner.GetPath(bad.m_CurrentPath,0);bad.m_Query.m_User=&badOwner;
 frame(bad);assert(badOwner.successes==0);
 // Once Space/contact engaged, that same side offset must still fail.
 FollowPath outside;User outsideOwner;outside.client.passable=true;
 planner.m_Client=&outside.client;planner.m_Solution={&landing,&launch};
 planner.GetPath(outside.m_CurrentPath,0);outside.m_Query.m_User=&outsideOwner;
 outside.client.position={0,0,30};frame(outside);assert(outside.m_TceFreePhase==1);
 outside.client.position={0,20,60};outside.client.onGround=false;outside.client.onLadder=true;
 frame(outside);assert(outside.failures==1&&outsideOwner.successes==0);
 printf("actual full Update: ready-gated Space, free-climb contact, actual-height landing and directed retreat verified\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class FreeClimbRuntimeTests(TestCase):
    def test_actual_full_update_uses_engine_ready_and_actual_landing(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
