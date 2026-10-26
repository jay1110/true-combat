"""Real complete drop Update with fresh two-body traces and real steering.

Independent moving body/floor observations drive the actual production contact
lease. The VM regression separately executes the actual TrafficYield movement;
these offline fixtures do not assert ET PMove or live Railhouse certification.
"""
import os
import re
import shutil
from unittest import TestCase, main, skipUnless
from test_corner_steering_deadzone_native import program as steering_program
from test_door_runtime_native import COMMON,body,compile_run


def program():
    # The fixture's compact bitset values are independent, but every added
    # production enum spelling must actually exist in the shared engine ABI.
    abi=(COMMON/'Omni-Bot_Types.h').read_text()
    for name in ('TR_MASK_SOLID','TR_MASK_PLAYERCLIP','TR_MASK_PLAYER','TR_MASK_NAVCAPSULE',
                 'ENT_CAT_PLAYER','ENT_FLAG_DEAD','ENT_FLAG_ONGROUND','ENT_FLAG_ONLADDER'):
        assert re.search(r'\b'+name+r'\b',abi),name
    source=steering_program().split('bool logged(',1)[0]
    source=source.replace('int UpdateTceDropTraffic(const Path::PathPoint&){return 0;}',
                          'int UpdateTceDropTraffic(const Path::PathPoint&);')
    source=source.replace('bool MaintainTceDropTrafficLease(const Path::PathPoint&){return false;}',
                          'bool MaintainTceDropTrafficLease(const Path::PathPoint&);')
    source=source.replace('StateStatus Update(float);',
        'int stopCalls=0;void Stop(bool){++stopCalls;m_CurrentPath.Clear();m_PathStatus=PathFinished;}StateStatus Update(float);')
    source=source.replace('bool grounded=true', 'bool grounded=true')
    source=source.replace('bool CanWalkTo(const Vector3f&p,NavFlags=0)'+body(source,'bool CanWalkTo(const Vector3f&p,NavFlags=0)').split(')',1)[1],
                          'bool CanWalkTo(const Vector3f&,NavFlags=0){return !wall;}')
    # Bind independent posture/velocity/floor observations. Contact flags alone
    # never manufacture an unchanged supported world-body pose.
    source=source.replace('struct Client {','struct Client {bool wall=false,supported=true;')
    source=source.replace('Vector3f GetVelocity(){return{};}', 'Vector3f GetVelocity(){return velocity;}')
    source=source.replace(body(source,'bool ResolveWalkDestination('),
                          'bool ResolveWalkDestination(const Vector3f&p,float,NavFlags f,Vector3f&out){out=p;return supported&&CanWalkTo(p,f);}')
    source=source.replace('GameEntity EntityFromID(int n){return{n};}',
                          'GameEntity EntityFromID(int n){return{n};}')
    source=source.replace('struct GameEntity {int id;bool IsValid()const{return id>=0;}};',
                          'struct GameEntity {int id=-1;bool IsValid()const{return id>=0;}};')
    declaration=r'''
enum{TR_MASK_SOLID=1,TR_MASK_PLAYERCLIP=2,TR_MASK_PLAYER=4,TR_MASK_NAVCAPSULE=8,
 ENT_CAT_PLAYER=1,ENT_FLAG_DEAD=3};constexpr bool False=false;
struct BitFlag32{unsigned bits=0;bool CheckFlag(int n)const{return(bits&(1u<<n))!=0;}};
using BitFlag64=BitFlag32;
struct obTraceResult{int m_StartSolid=0;float m_Fraction=1;GameEntity m_HitEntity;};
Vector3f otherPosition{25.625f,0,30};bool otherAlive=true,otherGrounded=true,otherPlayer=true;
int otherTeam=1,otherId=5,contactTraces=0;
namespace InterfaceFuncs{
 bool GetEntityCategory(GameEntity,BitFlag32&out){out.bits=otherPlayer?(1u<<ENT_CAT_PLAYER):0;return true;}
 int GetEntityTeam(GameEntity){return otherTeam;}
 bool IsAlive(GameEntity){return otherAlive;}
 bool GetEntityFlags(GameEntity,BitFlag64&out){out.bits=otherGrounded?(1u<<ENT_FLAG_ONGROUND):0;
  if(!otherAlive)out.bits|=1u<<ENT_FLAG_DEAD;return true;}
}
namespace EngineFuncs{bool TraceLine(obTraceResult&tr,const Vector3f&a,const Vector3f&b,
 const AABB*,int mask,int,bool){
 ++contactTraces;tr=obTraceResult{};
 assert(mask==(TR_MASK_SOLID|TR_MASK_PLAYERCLIP|TR_MASK_PLAYER|TR_MASK_NAVCAPSULE));
 // Equal-height independent player bodies. Moving the other actor, rather
 // than a timed boolean, removes its actual overlap with this sweep.
 if(std::fabs(otherPosition.y-a.y)<32.125f&&std::fabs(otherPosition.z-a.z)<61.f&&
  otherPosition.x-32.125f<b.x&&otherPosition.x+32.125f>a.x){
  tr.m_Fraction=.5f;tr.m_HitEntity.id=otherId;
 }return true;}}
'''
    marker='State::StateStatus FollowPath::Update('
    source=source.replace(marker,declaration+'\n'+marker,1)
    follow=(COMMON/'BotBaseStates.cpp').read_text()
    stop=body(follow,'void FollowPath::Stop(')
    assert 'ResetTceDropTrafficLease' not in stop and 'm_TceDropTrafficStart =' not in stop
    client=(COMMON/'Client.cpp').read_text().split('HANDLER(MESSAGE_SPAWN)',1)[1]
    spawn=body(client,'if(UseValidatedWalkEntries())').replace('UseValidatedWalkEntries()',
        'f.client.UseValidatedWalkEntries()').replace('GetFollowPath()->','f.')
    return source+'\n'+''.join(body(follow,marker) for marker in (
        'void FollowPath::ResetTceDropTrafficLease(',
        'bool FollowPath::MaintainTceDropTrafficLease(',
        'int FollowPath::UpdateTceDropTraffic('))+'\nvoid spawnEvent(FollowPath&f){'+spawn+'}\n'+r'''
bool logged(const char*s){for(const auto&l:logs)if(l.find(s)!=String::npos)return true;return false;}
void setupDrop(FollowPath&f,User&u,float distance=6.498f){
 f.client.debug=false;f.client.steer.client=&f.client;f.client.position={-distance,0,30};
 f.m_Query.m_User=&u;
 f.m_CurrentPath.AddPt({0,0,30},2).PlayerOrigin().NavId(684).TceWalkDrop(true);
 f.m_CurrentPath.AddPt({128,0,10},6).PlayerOrigin().NavId(685);
}
void dropFrame(FollowPath&f){frame(f);f.client.steer.UpdateSteering();}
void expectFail(FollowPath&f,const char*reason){
 assert(f.failures==1&&f.m_PathStatus==FollowPath::PathFinished&&logged(reason));
 assert(f.m_CurrentPath.GetCurrentPtIndex()==0);
}
int main(){
 // Both actors keep their measured16u hulls. The independent high-priority
 // stationary actor is held by a real obstructing body past the old3s bound.
 // Actual VM TrafficYield is separately tested to move that body36.25u and
 // stop32.25u aside with its real4u tolerance, clearing this exact lane.
 static FollowPath pair;User owner;setupDrop(pair,owner);const int begin=IGame::time;
 for(int i=0;i<64;++i){dropFrame(pair);assert(pair.failures==0&&owner.successes==0);}
 assert(pair.m_TceDropTrafficCredit>=3050&&contactTraces>=64);
 assert(pair.client.steer.m_MoveVec.SquaredLength()>0);
 // Three actual displacement increments from the GM VM fixture. No trace
 // hit flag is toggled; full-body overlap is recomputed from both positions.
 for(float side:{10.2f,20.4f,30.6f,32.25f}){
  otherPosition.y=-side;dropFrame(pair);assert(pair.failures==0);
 }
 assert(logged("TRAFFIC_CLEAR")&&pair.m_TceDropTrafficCredit<4000);
 pair.client.position={-1.9f,0,30};pair.client.velocity={1,0,0};
 dropFrame(pair);assert(pair.m_CurrentPath.GetCurrentPtIndex()==0);
 pair.client.velocity={};dropFrame(pair);dropFrame(pair);dropFrame(pair);
 assert(pair.m_CurrentPath.GetCurrentPtIndex()==1&&owner.successes==0&&pair.failures==0&&logged("TCE2WALKDROP LAUNCH"));
 assert(IGame::time-begin<4000);
 assert(pair.m_CurrentPath.GetPt(0)->m_Radius==2&&pair.m_CurrentPath.GetPt(0)->m_Pt.x==0);

 // The same physical contact is detected even below the GM movement .1
 // threshold. Its taper/input/launch radius remain entirely unchanged.
 logs.clear();otherPosition={29.125f,0,30};static FollowPath micro;User mu;setupDrop(micro,mu,3.f);
 for(int i=0;i<65;++i){dropFrame(micro);assert(micro.failures==0);}
 assert(micro.client.steer.m_MoveVec.Length()<.1f&&micro.m_TceDropTrafficCredit>3000);
 for(int i=0;i<30&&micro.failures==0;++i)dropFrame(micro);
 expectFail(micro,"traffic_deadline");const int lease=micro.m_TceDropTrafficStart;
 // Repeated Update, changing the blocking actor and discarding/rebuilding
 // the local phase cannot replace the original immutable contact deadline.
 micro.failures=0;micro.m_PathStatus=FollowPath::PathInProgress;otherId=6;
 micro.m_TceDropLaunchNavId=micro.m_TceDropSettleStart=micro.m_TceDropStableSince=0;
 dropFrame(micro);expectFail(micro,"traffic_deadline");assert(micro.m_TceDropTrafficStart==lease);

 // No player observation can pause a wall, absent support, dead/enemy actor,
 // or no player. Every case retains the original3s geometry failure.
 for(int mode=0;mode<5;++mode){
  logs.clear();static FollowPath guards[5];User u;auto&f=guards[mode];setupDrop(f,u);
  otherPosition={25.625f,0,30};otherAlive=true;otherGrounded=true;otherTeam=1;otherPlayer=true;
  if(mode==0)f.client.wall=true;if(mode==1)f.client.supported=false;
  if(mode==2)otherAlive=false;if(mode==3)otherTeam=2;if(mode==4)otherPlayer=false;
  const int start=IGame::time;for(int i=0;i<65&&f.failures==0;++i)dropFrame(f);
  expectFail(f,"settle_deadline");assert(f.m_TceDropTrafficCredit==0&&IGame::time-start<=3150);
 }
 otherAlive=otherGrounded=otherPlayer=true;otherTeam=1;
 // Contact flicker and blocker changes consume one absolute4s episode.
 logs.clear();static FollowPath flicker;User fu;setupDrop(flicker,fu);
 for(int i=0;i<90&&flicker.failures==0;++i){otherId=5+i%2;otherPosition.y=i%3?0:64;dropFrame(flicker);}
 assert(flicker.failures==1&&flicker.m_TceDropTrafficStart>0&&flicker.m_TceDropTrafficCredit<=4000);

 // Entering the unchanged2u circle clears contact EVERY settle frame,
 // even while velocity/alignment still prevents launch. A later fresh hit
 // must not credit this independently observed contact-free interval.
 logs.clear();static FollowPath gap;User gu;setupDrop(gap,gu,3.f);
 otherPosition={29.125f,0,30};dropFrame(gap);dropFrame(gap);
 const int credited=gap.m_TceDropTrafficCredit;assert(credited==50);
 gap.client.position={-1.9f,0,30};gap.client.velocity={1,0,0};otherPosition.y=64;
 for(int i=0;i<10;++i)dropFrame(gap);
 assert(!gap.m_TceDropTrafficContact&&gap.m_TceDropTrafficCredit==credited&&gap.failures==0);
 gap.client.position={-3,0,30};gap.client.velocity={};otherPosition.y=0;
 dropFrame(gap);assert(gap.m_TceDropTrafficCredit==credited);
 dropFrame(gap);assert(gap.m_TceDropTrafficCredit==credited+50);

 // Intervening ordinary movement is observed while AWAY, so a later
 // genuine return to the SAME drop UID can receive a new bounded lease.
 // Replanning without this physical progress was already rejected above.
 static FollowPath journey;User ju;setupDrop(journey,ju);
 otherPosition={25.625f,0,30};dropFrame(journey);const int firstLease=journey.m_TceDropTrafficStart;
 journey.m_CurrentPath.Clear();journey.m_CurrentPath.AddPt({256,0,30},35).PlayerOrigin().NavId(77);
 journey.m_CurrentPath.AddPt({512,0,30},35).PlayerOrigin().NavId(78);
 journey.client.position={256,0,30};dropFrame(journey);
 assert(journey.m_TceDropTrafficStart==0&&journey.failures==0);
 journey.m_CurrentPath.Clear();setupDrop(journey,ju);dropFrame(journey);
 assert(journey.m_TceDropTrafficStart>firstLease&&journey.m_TceDropTrafficNavId==684&&journey.failures==0);
 // A far pose with no fresh support cannot replenish even one lease.
 const int renewed=journey.m_TceDropTrafficStart;journey.client.position={256,0,30};journey.client.supported=false;
 Path::PathPoint jp;journey.m_CurrentPath.GetCurrentPt(jp);
 assert(!journey.MaintainTceDropTrafficLease(jp)&&journey.m_TceDropTrafficStart==renewed);
 journey.client.supported=true;
 // Execute the exact new production Client MESSAGE_SPAWN handler block:
 // Stop(true) invalidates the old path, then the lease is explicitly reset.
 spawnEvent(journey);assert(journey.stopCalls==1&&journey.m_CurrentPath.GetNumPts()==0&&journey.m_TceDropTrafficStart==0);
 journey.client.position={-6.498f,0,30};setupDrop(journey,ju);dropFrame(journey);
 assert(journey.m_TceDropTrafficStart>0);
 const int oldClock=IGame::time;IGame::time-=5000;
 assert(journey.Update(.05f)==State::State_Finished&&journey.m_TceDropTrafficStart==0&&journey.stopCalls==2);
 assert(journey.m_CurrentPath.GetNumPts()==0);IGame::time=oldClock+50;
 printf("full Update: moving pair body clearance, original2u/100ms/velocity/alignment, microinput, immutable4s lease and3s static guards passed\n");
}
'''


@skipUnless(os.name=='nt' and shutil.which('cmake'),'Windows CMake/MSVC required')
class DropTrafficUpdateNativeTests(TestCase):
    def test_actual_update_two_body_contact_has_one_bounded_lease(self):
        compile_run(self,program())


if __name__=='__main__':main()
