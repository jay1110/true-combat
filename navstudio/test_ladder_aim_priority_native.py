"""Complete FollowPath Update and real Aimer arbitration during a TCE climb.

The enemy submits the production AttackTarget priority; actual request storage,
replacement and winner selection execute from BotBaseStates.cpp. This verifies
controller ownership, not physical ladder traversal or the historical fall.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_door_runtime_native import COMMON, body, compile_run
from test_door_update_native import program as update_program


def program():
    source = update_program().split('int main(){', 1)[0]
    header = (COMMON/'BotBaseStates.h').read_text()
    cpp = (COMMON/'BotBaseStates.cpp').read_text()
    priorities = body((COMMON/'Utilities.h').read_text(), 'namespace Priority')
    source = source.replace('namespace Priority {enum{High=1,Idle=0};}', priorities)
    source = source.replace('static const Vector3f ZERO;', 'static const Vector3f ZERO,UNIT_X;')
    source = source.replace('const Vector3f Vector3f::ZERO{};',
                            'const Vector3f Vector3f::ZERO{};const Vector3f Vector3f::UNIT_X{1,0,0};')
    aimer = r'''
struct StateChild{enum StateStatus{State_Busy,State_Finished};StateChild(const char*){}unsigned GetNameHash(){return 99;}};
''' + body(header, 'class AimerUser') + ';\n' + body(header, 'class Aimer :') + ';\n'
    aimer += '\n'.join(body(cpp, marker) for marker in (
        'Aimer::AimRequest::AimRequest()', 'void Aimer::AimRequest::Reset()',
        'Aimer::Aimer()', 'Aimer::AimRequest *Aimer::FindAimRequest(',
        'bool Aimer::AddAimRequest(', 'void Aimer::ReleaseAimRequest(',
        'Aimer::AimRequest *Aimer::GetHighestAimRequest('))
    old = 'struct Aimer {int releases=0;void AddAimRequest(int,void*,int){}void ReleaseAimRequest(int){++releases;}};'
    assert old in source
    source = source.replace(old, aimer)
    source = source.replace('class FollowPath:public State{', 'class FollowPath:public State,public AimerUser{')
    source = source.replace('Client*GetClient(){return&client;}', 'void OnTarget(){}Client*GetClient(){return&client;}')
    source = source.replace('bool UseValidatedWalkEntries()const{return true;}',
        'bool tce=true;bool UseValidatedWalkEntries()const{return tce;}')
    source = source.replace('bool HasEntityFlag(int flag)const{return flag==ENT_FLAG_ONGROUND;}',
        'bool grounded=true,ladder=false;bool HasEntityFlag(int flag)const{return flag==ENT_FLAG_ONGROUND?grounded:flag==ENT_FLAG_ONLADDER?ladder:false;}')
    # Keep the opponent's priority bound to the actual weapon-state source.
    attack = body((COMMON/'BotWeaponSystem.cpp').read_text(), 'State::StateStatus AttackTarget::Update(')
    assert 'AddAimRequest(Priority::LowMed, this, GetNameHash())' in attack
    return source + r'''
struct Enemy:AimerUser{bool GetAimPosition(Vector3f&p){p={-100,100,80};return true;}void OnTarget(){}}enemy;
void submitEnemy(FollowPath&f){assert(f.client.aimer.AddAimRequest(Priority::LowMed,&enemy,77));}
void expectOwner(FollowPath&f,unsigned owner,Priority::ePriority priority){
 auto*r=f.client.aimer.GetHighestAimRequest(false);assert(r->m_Owner==owner&&r->m_Priority==priority);
 assert(r->m_AimerUser==(owner==77?static_cast<AimerUser*>(&enemy):static_cast<AimerUser*>(&f)));
}
void climb(FollowPath&f,User&u){f.m_Query.m_User=&u;f.client.debug=false;
 f.client.position={2277,260,120.125f};
 f.m_CurrentPath.AddPt({2274,278.25f,120.25f},8).PlayerOrigin().NavId(481).Flags(F_NAV_CLIMB);
 f.m_CurrentPath.AddPt({2274,264.375f,-39.75f},18).PlayerOrigin().NavId(480).Flags(F_NAV_CLIMB);
 f.m_CurrentPath.AddPt({2274,278.375f,-39.75f},8).PlayerOrigin().NavId(479).Flags(F_NAV_CLIMB);
}
int main(){
 static FollowPath descent;User owner;climb(descent,owner);submitEnemy(descent);frame(descent);
 expectOwner(descent,9,Priority::High);assert(descent.m_CurrentPath.GetCurrentPtIndex()==0&&!descent.client.ladder);
 // Mounted and temporarily airborne descent frames retain the same ownership.
 descent.client.grounded=false;descent.client.ladder=true;descent.client.position={2274,278.25f,120.125f};
 submitEnemy(descent);frame(descent);expectOwner(descent,9,Priority::High);
 descent.client.ladder=false;descent.client.position={2274,290,90};
 submitEnemy(descent);frame(descent);expectOwner(descent,9,Priority::High);
 assert(!descent.m_TceAirWaitActive&&descent.failures==0&&owner.successes==0);
 // Existing request is downgraded as soon as the current path is ordinary.
 descent.m_CurrentPath.Clear();descent.client.grounded=true;descent.client.position={0,0,30};
 descent.m_CurrentPath.AddPt({64,0,30},8).PlayerOrigin().NavId(12);
 descent.m_CurrentPath.AddPt({64,0,160},8).PlayerOrigin().NavId(13).Flags(F_NAV_CLIMB);
 frame(descent);expectOwner(descent,77,Priority::LowMed); // Future CLIMB does not own aim.
 descent.client.aimer.ReleaseAimRequest(77);expectOwner(descent,9,Priority::Idle);

 static FollowPath otherGame;User otherOwner;climb(otherGame,otherOwner);otherGame.client.tce=false;
 submitEnemy(otherGame);frame(otherGame);expectOwner(otherGame,77,Priority::LowMed);
 otherGame.client.aimer.ReleaseAimRequest(77);expectOwner(otherGame,9,Priority::Idle);

 // Marked ascent release and its dismount retain their existing High ownership.
 static FollowPath ascent;User ascentOwner;ascent.m_Query.m_User=&ascentOwner;
 ascent.client.position={0,0,70};ascent.client.grounded=false;ascent.client.ladder=true;
 ascent.m_CurrentPath.AddPt({0,0,104.25f},6).PlayerOrigin().NavId(483).Flags(F_NAV_CLIMB).TceAscentRelease(true);
 ascent.m_CurrentPath.AddPt({0,36,120.25f},8).PlayerOrigin().NavId(482).Flags(F_NAV_CLIMB);
 submitEnemy(ascent);frame(ascent);expectOwner(ascent,9,Priority::High);
 ascent.m_CurrentPath.NextPt();ascent.client.ladder=false;ascent.client.position={0,10,124};
 frame(ascent);expectOwner(ascent,9,Priority::High);assert(ascent.failures==0);
 printf("complete Update + actual Aimer: current TCE climb wins combat aim before/on/after ladder contact; ordinary/future/non-TCE remain unchanged; ascent release/dismount preserved\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class LadderAimPriorityTests(TestCase):
    def test_current_tce_climb_owns_real_aim_arbitration(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
