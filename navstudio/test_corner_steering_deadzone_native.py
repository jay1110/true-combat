"""Production FollowPath Update and SteeringSystem over Snow's .111u stop.

Walk/ground outcomes and actor positions are independent observations. Real
native probes separately establish actual->centre and centre->next clearance.
This test executes the production steering and exact signed-char command
projection; it does not replace ET's movement physics with a live claim.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_corner_centre_update_native import program as corner_program
from test_door_runtime_native import COMMON, body, compile_run


def program():
    source = corner_program().split('void setupCorner(', 1)[0]
    source = source.replace('enum MoveMode{Run=0,Walk=1};', 'using MoveMode=int;enum{Run=0,Walk=1};')
    source = source.replace('Vector3f operator*(float s)const{return{x*s,y*s,z*s};}',
        'Vector3f operator*(float s)const{return{x*s,y*s,z*s};}Vector3f&operator*=(float s){x*=s;y*=s;z*=s;return*this;}')
    old = body(source, 'struct Steering {')
    replacement = r'''
struct Client;
struct Steering {
 Client*client=nullptr;Client*GetClient(){return client;}
 Vector3f m_TargetVector,m_MoveVec;float m_TargetRadius=32,m_InputScale=1,m_DistanceToTarget=0;
 bool m_TargetVector3d=false,m_bMoveEnabled=true;MoveMode m_MoveMode=0;
 enum{Normal,Arrive};int m_MoveType=Normal;
 bool SetTarget(const Vector3f&,float=32,MoveMode=0,bool=false,float=1);
 void UpdateSteering();Vector3f GetTarget(){return m_TargetVector;}
}'''
    source = source.replace(old, replacement, 1)
    # FollowPath's controller fixture supplies these fresh walk observations.
    old_walk = body(source, 'bool CanWalkTo(const Vector3f&p,NavFlags=0)')
    source = source.replace(old_walk, r'''
bool CanWalkTo(const Vector3f&p,NavFlags=0){
 if(p.y==-95.047698974609375f){++centreChecks;return centreClear;}
 ++nextChecks;
 return nextClear||(requireExactCentre&&
  (position-Vector3f(1037.0472412109375f,-95.047698974609375f,-1.75f)).Flatten().SquaredLength()<=.001f*.001f);
}''', 1)
    steering = (COMMON/'BotSteeringSystem.cpp').read_text()
    functions = '\n'.join(body(steering, marker).replace('SteeringSystem::', 'Steering::') for marker in
        ('bool SteeringSystem::SetTarget(', 'void SteeringSystem::UpdateSteering()'))
    return source + '\nenum{BOT_DEBUG_MOVEVEC=100,ENT_FLAG_ON_ICE=200};\nnamespace Utils{void DrawLine(const Vector3f&,const Vector3f&,int,float){}}\n' + functions + r'''
bool logged(const char*s){for(const auto&l:logs)if(l.find(s)!=String::npos)return true;return false;}
void setupSnow(FollowPath&f,User&u){
 f.client.position={1036.936f,-95.054f,-1.875f};f.client.debug=false;f.client.requireExactCentre=true;
 f.client.steer.client=&f.client;f.m_Query.m_User=&u;
 f.m_CurrentPath.AddPt({1037.0472412109375f,-95.047698974609375f,-1.75f},10).PlayerOrigin().NavId(690);
 f.m_CurrentPath.AddPt({1037.0472412109375f,-87.047698974609375f,-1.75f},35).PlayerOrigin().NavId(461);
}
void steerFrame(FollowPath&f){frame(f);f.client.steer.UpdateSteering();}
int main(){
 static FollowPath observed;User owner;setupSnow(observed,owner);
 // Exact reported stationary .111u residual. Full Update requests movement
 // instead of manufacturing a settled centre from its old .125u dead zone.
 for(int i=0;i<5;++i){
  steerFrame(observed);
  assert(observed.failures==0&&owner.successes==0&&observed.m_CurrentPath.GetCurrentPtIndex()==0);
  assert(observed.client.steer.m_TargetRadius==0&&observed.client.steer.m_MoveVec.SquaredLength()>0);
  assert(observed.client.steer.m_InputScale>=1.5f/64.f&&observed.client.steer.m_InputScale<=.125f);
 }
 assert(observed.client.centreChecks==5&&observed.client.nextChecks==5&&!logged("BLOCKED_ORDINARY"));
 const auto movement=observed.client.steer.m_MoveVec;
 // This is the actual bridge's truncation, for every integer yaw plus a
 // fractional offset. A .01 minimum produced TWO zero commands at45deg.
 for(int maximum:{64,127})for(int i=0;i<1440;++i){
  const float yaw=(i*.25f+.013f)*.01745329252f;
  const Vector3f forward{std::cos(yaw),std::sin(yaw),0},right{-std::sin(yaw),std::cos(yaw),0};
  const signed char fwd=(signed char)(forward.Dot(movement)*maximum);
  const signed char side=(signed char)(right.Dot(movement)*maximum);
  assert(fwd!=0||side!=0);
 }
 // An independent physically corrected observation permits the next leg
 // only after another fresh walk call, with the authored points unchanged.
 observed.client.position={1037.0472412109375f,-95.047698974609375f,-1.875f};steerFrame(observed);
 assert(observed.m_CurrentPath.GetCurrentPtIndex()==1&&observed.failures==0&&logged("CENTRE_CLEAR"));
 assert(observed.client.nextChecks==6&&observed.client.centreChecks==5);
 assert(observed.m_CurrentPath.GetPt(0)->m_Radius==10&&observed.m_CurrentPath.GetPt(1)->m_Radius==35);
 assert(observed.m_CurrentPath.GetPt(0)->m_NavFlags==0&&observed.m_CurrentPath.GetPt(0)->m_Pt.z==-1.75f);
 observed.client.position=observed.m_CurrentPath.GetPt(1)->m_Pt;steerFrame(observed);
 assert(owner.successes==1&&observed.failures==0);

 // No unchanged-position or oscillation loop can renew an episode. No
 // outgoing proof is waived even at the exact centre or after a deadline.
 logs.clear();static FollowPath frozen;User frozenOwner;setupSnow(frozen,frozenOwner);
 for(int i=0;i<35&&frozen.failures==0;++i)steerFrame(frozen);
 assert(frozen.failures==1&&frozenOwner.successes==0&&logged("centre_timeout"));
 assert(frozen.client.resets==0&&frozen.m_CurrentPath.GetCurrentPtIndex()==0);
 logs.clear();static FollowPath oscillating;User oscillatingOwner;setupSnow(oscillating,oscillatingOwner);
 for(int i=0;i<45&&oscillating.failures==0;++i){
  oscillating.client.position.x=1037.0472412109375f-(i%2?.10f:.11f);steerFrame(oscillating);
 }
 assert(oscillating.failures==1&&oscillatingOwner.successes==0&&logged("centre_timeout"));
 assert(oscillating.client.resets==0&&oscillating.m_CurrentPath.GetCurrentPtIndex()==0);
 logs.clear();static FollowPath exact;User exactOwner;setupSnow(exact,exactOwner);
 exact.client.requireExactCentre=false;exact.client.position={1037.0472412109375f,-95.047698974609375f,-1.875f};
 for(int i=0;i<4&&exact.failures==0;++i)steerFrame(exact);
 assert(exact.failures==1&&exactOwner.successes==0&&logged("centre_settled"));
 assert(exact.m_CurrentPath.GetCurrentPtIndex()==0&&exact.client.resets==0);
 logs.clear();static FollowPath blocked;User blockedOwner;setupSnow(blocked,blockedOwner);
 blocked.client.centreClear=false;steerFrame(blocked);
 assert(blocked.failures==1&&blockedOwner.successes==0&&logged("centre_unreachable"));
 printf("real FollowPath Update/SteeringSystem: Snow .111u correction, nonzero signed commands at2880 facing/speed pairs, fresh next proof, exact-centre failure and bounded frozen/oscillating episodes passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class CornerSteeringDeadzoneTests(TestCase):
    def test_actual_update_and_steering_complete_only_freshly_walkable_corner(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
