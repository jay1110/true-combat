/* Actual complete SteeringSystem implementation with observed engine services.
 * No player physics or navigation algorithm is replaced in production. */
#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cstring>
typedef unsigned long long NavFlags;
typedef unsigned obuint32;
typedef int obint32;
#define __OMNIBOT_TYPES_H__
#include "vendor/omni-bot/source/Omnibot/Common/NavigationFlags.h"
#define Prof(x)
enum { False=0,BOT_BUTTON_WALK=1,BOT_BUTTON_JUMP=2,BOT_DEBUG_MOVEVEC=3 };
enum { ENT_FLAG_ONGROUND,ENT_FLAG_ONLADDER,ENT_FLAG_ON_ICE,ENT_FLAG_DEAD,ENT_FLAG_DISABLED,ENT_CAT_PLAYER };
enum { TR_MASK_SOLID=1,TR_MASK_PLAYERCLIP=2,TR_MASK_PLAYER=4,TR_MASK_NAVCAPSULE=8 };
enum MoveMode { Run,Walk };
struct Vector3f {
 float x,y,z;Vector3f(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
 static const Vector3f ZERO;
 Vector3f operator+(const Vector3f&v)const{return Vector3f(x+v.x,y+v.y,z+v.z);}
 Vector3f operator-(const Vector3f&v)const{return Vector3f(x-v.x,y-v.y,z-v.z);}
 Vector3f operator-()const{return Vector3f(-x,-y,-z);}
 Vector3f operator*(float s)const{return Vector3f(x*s,y*s,z*s);}
 Vector3f&operator*=(float s){x*=s;y*=s;z*=s;return*this;}
 float Dot(const Vector3f&v)const{return x*v.x+y*v.y+z*v.z;}
 float SquaredLength()const{return Dot(*this);}
 float Normalize(){float n=std::sqrt(SquaredLength());if(n>0){x/=n;y/=n;z/=n;}return n;}
 Vector3f Flatten()const{return Vector3f(x,y,0);}
};
const Vector3f Vector3f::ZERO;
struct GameEntity {int id,serial;GameEntity(int i=-1,int s=0):id(i),serial(s){}bool IsValid()const{return id>=0;}void Reset(){*this=GameEntity();}bool operator==(const GameEntity&e)const{return id==e.id&&serial==e.serial;}bool operator!=(const GameEntity&e)const{return !(*this==e);}};
struct Flags {unsigned bits=0;bool CheckFlag(int f)const{return(bits&(1u<<f))!=0;}};
typedef Flags BitFlag32;typedef Flags BitFlag64;
struct AABB {float m_Mins[3],m_Maxs[3];};
struct obTraceResult {bool m_StartSolid=false;float m_Fraction=1;GameEntity m_HitEntity;};
struct Path {
 struct PathPoint {NavFlags m_NavFlags=0;int m_OnPathThrough=0;bool m_TceAscentRelease=false,m_TceAscentAlignment=false,m_TceWalkDrop=false,m_TceStandJump=false,m_TceFreeClimb=false;struct{bool marked=false;}m_TceDoor;};
 PathPoint point,next;bool current=false,following=false;
 bool GetCurrentPt(PathPoint&p)const{p=point;return current;}
 bool GetNextPt(PathPoint&p)const{p=next;return following;}
};
struct FollowPath {Path path;int stops=0;const Path&GetCurrentPath()const{return path;}void Stop(){++stops;path.current=false;path.following=false;}};
struct obPlayerInfo {enum{MaxPlayers=64};enum Controller{Bot,Human,Both};struct PInfo{int m_Team=0;Controller m_Controller=Bot;}m_Players[64];};
struct Player {bool active=false,alive=true,allied=true,category=true;int serial=1,team=1;bool human=false;Vector3f pos,vel;Flags flags;};
static Player players[64];
static int gameTime=1000,walks,traces,infos;
static bool groundSafe=true,traceClear=true,hullValid=true,traceStartSolid=false;
static bool separatedByWorld=false,trafficGeometry=false,contactWallGeometry=false;
static int blockedSide=0;
static AABB observedHull;
static Vector3f observedStep;
struct Engine {void GetPlayerInfo(obPlayerInfo&i){++infos;for(int n=0;n<64;++n)if(players[n].active){i.m_Players[n].m_Team=players[n].team;i.m_Players[n].m_Controller=players[n].human?obPlayerInfo::Human:obPlayerInfo::Bot;}}GameEntity EntityFromID(int id){return players[id].active?GameEntity(id,players[id].serial):GameEntity();}int IDFromEntity(GameEntity e){return e.id>=0&&e.id<64&&players[e.id].active&&players[e.id].serial==e.serial?e.id:-1;}};
static Engine engine,*g_EngineFuncs=&engine;
namespace InterfaceFuncs {
 bool GetEntityCategory(GameEntity e,Flags&f){f.bits=players[e.id].category?1u<<ENT_CAT_PLAYER:0;return true;}
 bool GetEntityFlags(GameEntity e,Flags&f){f=players[e.id].flags;return true;}
 bool IsAlive(GameEntity e){return players[e.id].alive;}
}
namespace EngineFuncs {
 bool EntityPosition(GameEntity e,Vector3f&p){p=players[e.id].pos;return true;}
 bool EntityVelocity(GameEntity e,Vector3f&v){v=players[e.id].vel;return true;}
 bool TraceLine(obTraceResult&t,const Vector3f&start,const Vector3f&end,const AABB*h,int mask,int ignore,int){
  ++traces;assert(mask==15);observedHull=*h;observedStep=end;
  if(trafficGeometry){
   t.m_Fraction=1.f;t.m_StartSolid=false;t.m_HitEntity.Reset();
   for(int i=0;i<64;++i)if(i!=ignore&&players[i].active&&players[i].alive&&std::fabs(players[i].pos.z-start.z)<48){
    const Vector3f delta=end-start,relative=start-players[i].pos;
    float enter=0,leave=1;bool hit=true;const float width=contactWallGeometry?32.f:28.f;
    for(int axis=0;axis<2;++axis){const float d=axis?delta.y:delta.x,r=axis?relative.y:relative.x;if(std::fabs(d)<.0001f){if(std::fabs(r)>=width)hit=false;}else{float a=(-width-r)/d,b=(width-r)/d;if(a>b)std::swap(a,b);enter=std::max(enter,a);leave=std::min(leave,b);}}
    if(hit&&enter<=leave&&leave>0&&enter<t.m_Fraction){t.m_Fraction=std::max(0.f,enter);t.m_HitEntity=GameEntity(i,players[i].serial);t.m_StartSolid=std::fabs(relative.x)<width&&std::fabs(relative.y)<width;}
   }return true;
  }
  for(int i=0;i<64;++i)if(players[i].active&&(players[i].pos-end).SquaredLength()<.001f){t.m_StartSolid=false;t.m_Fraction=.5f;t.m_HitEntity=separatedByWorld?GameEntity():GameEntity(i,players[i].serial);return true;}t.m_StartSolid=traceStartSolid;t.m_Fraction=traceClear?1.f:0.f;return true;}
}
class Client {
public:
 int id=2,team=1;bool tce=true,jump=false;Flags flags;Vector3f pos,move;FollowPath follow;
 Client(){flags.bits=1u<<ENT_FLAG_ONGROUND;}
 bool UseValidatedWalkEntries()const{return tce;}int GetGameID()const{return id;}int GetTeam()const{return team;}
 GameEntity GetGameEntity()const{return GameEntity(id,players[id].serial);}
 bool HasEntityFlag(int f)const{return flags.CheckFlag(f);}bool IsButtonDown(int)const{return jump;}
 const Vector3f&GetPosition()const{return pos;}FollowPath*GetFollowPath(){return &follow;}
 bool IsAllied(GameEntity e)const{return players[e.id].allied;}
 bool GetNavigationHull(AABB&h,NavFlags)const{h.m_Mins[0]=h.m_Mins[1]=contactWallGeometry?-16.f:-14.f;h.m_Mins[2]=contactWallGeometry?-30.f:-24.f;h.m_Maxs[0]=h.m_Maxs[1]=contactWallGeometry?16.f:14.f;h.m_Maxs[2]=contactWallGeometry?57.f:46.f;return hullValid;}
 bool CanWalkTo(const Vector3f&p)const{++walks;if(contactWallGeometry){if(p.x<16.125f||std::fabs(p.y)>120.f)return false;}else if(trafficGeometry)for(int i=1;i<=8;++i){Vector3f q=pos+(p-pos)*(i/8.f);if(std::fabs(q.y)>(q.x<140.f?3.f:90.f))return false;}return groundSafe&&!(blockedSide>0&&p.y>0)&&!(blockedSide<0&&p.y<0);}
 bool IsDebugEnabled(int)const{return false;}Vector3f GetEyePosition()const{return pos;}
 void PressButton(int b){assert(b==BOT_BUTTON_WALK);}void SetMovementVector(Vector3f v){move=v;}
};
struct State {enum StateStatus{State_Busy};};
struct StateChild:State {Client*testClient;StateChild(const char*):testClient(0){}Client*GetClient()const{return testClient;}};
typedef std::vector<int> MemoryRecords;typedef std::vector<GameEntity> EntityList;
namespace COLOR {enum{GREEN};}namespace Utils {void DrawLine(Vector3f,Vector3f,int,float){}}
namespace Mathf {float FAbs(float x){return std::fabs(x);}}
template<class T>T ClampT(T v,T lo,T hi){return std::max(lo,std::min(hi,v));}
struct IGame {static int GetTime(){return gameTime;}static float GetDeltaTimeSecs(){return .05f;}};
#include "vendor/omni-bot/source/Omnibot/Common/BotSteeringSystem.h"
/* ACTUAL_STEERING */
struct Steering:AiState::SteeringSystem {void Attach(Client*c){testClient=c;}int Side()const{return m_AvoidSide;}int Until()const{return m_AvoidUntil;}GameEntity Avoided()const{return m_AvoidEntity;}};
static int checks;
#define CHECK(c) do{++checks;assert(c);}while(0)
static void reset(Client&c,Steering&s){static int clock=0;c=Client();s=Steering();s.Attach(&c);for(int i=0;i<64;++i)players[i]=Player();groundSafe=traceClear=hullValid=true;traceStartSolid=separatedByWorld=trafficGeometry=contactWallGeometry=false;blockedSide=0;walks=traces=infos=0;gameTime=(clock+=5000);s.SetTarget(Vector3f(500,0,0),32);}
static void player(int id,float x,float y,bool human){players[id].active=true;players[id].pos=Vector3f(x,y,0);players[id].human=human;players[id].flags.bits=1u<<ENT_FLAG_ONGROUND;}
int main(){
 Client c;Steering s;Vector3f v;
#ifndef TCE_TRAFFIC_CONTACT_ONLY
#ifndef TCE_TRAFFIC_CROWD_ONLY
 reset(c,s);s.UpdateSteering();CHECK(s.GetMoveVector().x==1&&walks==0);
 player(3,64,0,true);s.UpdateSteering();v=s.GetMoveVector();CHECK(std::fabs(v.y)>.5f&&v.x>0&&traces==2&&walks==1);CHECK(observedHull.m_Mins[2]==-24&&observedHull.m_Maxs[2]==46);
 int side=s.Side();gameTime+=50;s.UpdateSteering();CHECK(s.Side()==side&&s.Until()==gameTime+700);
 reset(c,s);player(3,60,0,true);blockedSide=-1;s.UpdateSteering();CHECK(s.GetMoveVector().y>0&&walks==2&&traces==2);
 reset(c,s);player(3,60,0,true);groundSafe=false;s.UpdateSteering();CHECK(s.GetMoveVector().SquaredLength()==0&&walks>0&&traces==1);
 reset(c,s);player(3,60,0,true);traceClear=false;s.UpdateSteering();CHECK(s.GetMoveVector().SquaredLength()==0&&traces>1);
 reset(c,s);player(3,60,0,true);traceStartSolid=true;s.UpdateSteering();CHECK(s.GetMoveVector().SquaredLength()==0);
 reset(c,s);player(3,60,0,true);hullValid=false;s.UpdateSteering();CHECK(s.GetMoveVector().SquaredLength()==0&&walks==0);
 reset(c,s);player(3,60,0,true);s.SetNoAvoidTime(250);s.UpdateSteering();CHECK(s.GetMoveVector().x==1&&infos==0);gameTime+=251;s.UpdateSteering();CHECK(std::fabs(s.GetMoveVector().y)>.5f);
 for(int kind=0;kind<9;++kind){reset(c,s);player(3,60,0,true);if(kind==0)c.tce=false;if(kind==1)c.flags.bits=0;if(kind==2)c.flags.bits|=1u<<ENT_FLAG_ONLADDER;if(kind==3)c.jump=true;if(kind==4)c.flags.bits|=1u<<ENT_FLAG_ON_ICE;if(kind==5)s.SetTarget(Vector3f(500,0,0),0);if(kind==6)s.SetTarget(Vector3f(500,0,0),32,Run,true);if(kind==7)s.SetTarget(Vector3f(500,0,0),32,Run,false,.5f);if(kind==8)c.flags.bits|=1u<<ENT_FLAG_DEAD;s.UpdateSteering();CHECK(infos==0&&traces==0);}
 const NavFlags exact[]={F_NAV_DOOR,F_NAV_JUMP,F_NAV_JUMPGAP,F_NAV_JUMPLOW,F_NAV_CLIMB,F_NAV_ELEVATOR,F_NAV_TELEPORT,F_NAV_PRONE,F_NAV_INWATER,F_NAV_UNDERWATER};
 for(int k=0;k<10;++k){reset(c,s);player(3,60,0,true);c.follow.path.current=true;c.follow.path.point.m_NavFlags=exact[k];s.UpdateSteering();CHECK(infos==0&&traces==0);}
 reset(c,s);player(3,60,0,true);c.follow.path.following=true;c.follow.path.next.m_TceStandJump=true;s.UpdateSteering();CHECK(infos==0);
 for(int k=0;k<6;++k){reset(c,s);player(3,60,0,true);if(k==0)players[3].team=2;if(k==1)players[3].allied=false;if(k==2)players[3].alive=false;if(k==3)players[3].category=false;if(k==4)players[3].flags.bits|=1u<<ENT_FLAG_DEAD;if(k==5)players[3].pos.z=80;s.UpdateSteering();CHECK(s.GetMoveVector().x==1&&walks==0);}
 reset(c,s);player(3,-60,0,true);s.UpdateSteering();CHECK(s.GetMoveVector().x==1&&walks==0);
 reset(c,s);player(3,60,20,true);separatedByWorld=true;s.UpdateSteering();CHECK(s.GetMoveVector().x==1&&walks==0);
 reset(c,s);player(3,60,80,true);s.UpdateSteering();CHECK(s.GetMoveVector().x==1&&walks==0);
 reset(c,s);player(3,60,0,false);players[3].vel=Vector3f(-100,0,0);s.UpdateSteering();CHECK(s.GetMoveVector().x==1&&walks==0);c.id=4;s.UpdateSteering();CHECK(std::fabs(s.GetMoveVector().y)>.5f);
 reset(c,s);player(3,60,0,false);s.UpdateSteering();CHECK(std::fabs(s.GetMoveVector().y)>.5f);
 reset(c,s);player(3,40,0,false);player(4,90,0,true);s.UpdateSteering();CHECK(s.Avoided().id==4);
 reset(c,s);s.SetTarget(Vector3f(0,0,0),32);player(3,80,0,true);players[3].vel=Vector3f(-100,0,0);s.UpdateSteering();CHECK(std::fabs(s.GetMoveVector().y)>.5f);players[3].vel=Vector3f(100,0,0);s.UpdateSteering();CHECK(s.GetMoveVector().SquaredLength()==0);
 reset(c,s);player(3,60,0,true);s.SetEnableMovement(false);s.UpdateSteering();CHECK(s.GetMoveVector().SquaredLength()==0&&infos==0);

 // A stationary ally must release a passage to a moving bot, independently
 // of the head-on ID tie-break used for two moving actors.
 reset(c,s);s.SetTarget(c.pos,32);player(3,60,0,false);players[3].vel=Vector3f(-100,0,0);s.UpdateSteering();CHECK(std::fabs(s.GetMoveVector().y)>.5f);
 // Body contact has already removed the human's forward velocity. Waiting
 // for a negative velocity dot product would freeze both actors forever.
 reset(c,s);s.SetTarget(c.pos,32);player(3,40,0,true);s.UpdateSteering();CHECK(s.GetMoveVector().SquaredLength()>.1f);
 // No lateral room: ordinary bot-vs-bot yielding has a supported retreat.
 reset(c,s);trafficGeometry=true;player(3,40,0,false);s.UpdateSteering();CHECK(s.GetMoveVector().x<-.5f&&std::fabs(s.GetMoveVector().y)<.01f);
#endif

 // Four idle bots fill a narrow corridor. The front bot cannot move until
 // the rear makes room; the blocked human has zero physical velocity.
 Client crowd[4];Steering steer[4];
 reset(c,s);trafficGeometry=true;player(1,-40,0,true);
 for(int i=0;i<4;++i){crowd[i]=Client();crowd[i].id=i+2;crowd[i].pos=Vector3f(i*40.f,0,0);player(i+2,crowd[i].pos.x,0,false);steer[i].Attach(&crowd[i]);steer[i].SetTarget(crowd[i].pos,32);}
 for(int i=0;i<4;++i)steer[i].UpdateSteering();
 CHECK(steer[0].GetMoveVector().SquaredLength()==0);CHECK(steer[3].GetMoveVector().x>.5f);
 for(int frame=0;frame<90;++frame){
  for(int i=0;i<4;++i){crowd[i].pos=players[i+2].pos;steer[i].UpdateSteering();}
  for(int i=3;i>=0;--i){Vector3f movement=steer[i].GetMoveVector()*5.f;AABB hull;crowd[i].GetNavigationHull(hull,0);obTraceResult trace;EngineFuncs::TraceLine(trace,players[i+2].pos,players[i+2].pos+movement,&hull,15,i+2,0);players[i+2].vel=trace.m_Fraction==1.f?movement*20.f:Vector3f::ZERO;if(trace.m_Fraction==1.f)players[i+2].pos=players[i+2].pos+movement;CHECK(!trace.m_StartSolid);}
  AABB humanHull;crowd[0].GetNavigationHull(humanHull,0);obTraceResult humanTrace;Vector3f humanStep(5,0,0);EngineFuncs::TraceLine(humanTrace,players[1].pos,players[1].pos+humanStep,&humanHull,15,1,0);players[1].vel=humanTrace.m_Fraction==1.f?Vector3f(100,0,0):Vector3f::ZERO;if(humanTrace.m_Fraction==1.f)players[1].pos=players[1].pos+humanStep;
  gameTime+=50;
  if(frame%30==29){printf("group frame %d human %.1f bots %.1f/%.1f %.1f/%.1f %.1f/%.1f %.1f/%.1f\n",frame,players[1].pos.x,players[2].pos.x,players[2].pos.y,players[3].pos.x,players[3].pos.y,players[4].pos.x,players[4].pos.y,players[5].pos.x,players[5].pos.y);fflush(stdout);}
 }
 CHECK(players[2].pos.x>24.f);CHECK(std::fabs(players[5].pos.y)>28.f);CHECK(players[1].pos.x>0.f);

 // A pending request must not survive actor reuse, a dead human, exact
 // traversal, or expiry. These are live identities, not durable bot IDs.
 for(int invalid=0;invalid<5;++invalid){
  reset(c,s);trafficGeometry=true;player(1,-40,0,true);player(2,0,0,false);player(3,40,0,false);c.id=2;s.SetTarget(c.pos,32);s.UpdateSteering();
  Client rear;Steering rearSteer;rear.id=3;rear.pos=players[3].pos;rearSteer.Attach(&rear);rearSteer.SetTarget(rear.pos,32);
  if(invalid==0)++players[3].serial;if(invalid==1)players[1].alive=false;if(invalid==2)rear.follow.path.current=true,rear.follow.path.point.m_NavFlags=F_NAV_JUMP;if(invalid==3)gameTime+=1201;if(invalid==4)players[1].human=false;
  rearSteer.UpdateSteering();CHECK(rearSteer.GetMoveVector().SquaredLength()==0);
 }
#endif
 // The native new-bbox contact gap is 16+16+.125, with the bot backed near
 // a wall. A +forward-biased sidestep enters the touching human. Pure lateral
 // input must still pass the unchanged full16u hull and world/body sweeps.
 for(int moving=0;moving<2;++moving){
  reset(c,s);trafficGeometry=contactWallGeometry=true;c.id=1;c.pos=Vector3f(27.33188f,0,30.125f);
  player(0,59.45723f,0,true);player(1,c.pos.x,0,false);players[0].pos.z=players[1].pos.z=c.pos.z;
  s.SetTarget(moving?Vector3f(500,0,c.pos.z):c.pos,32);s.UpdateSteering();v=s.GetMoveVector();
  printf("contact wall moving=%d step=(%.3f %.3f %.3f) gap=%.6f\n",moving,v.x,v.y,v.z,players[0].pos.x-c.pos.x);fflush(stdout);
  CHECK(std::fabs(v.y)>.5f&&std::fabs(v.x)<.01f);CHECK(observedHull.m_Mins[0]==-16&&observedHull.m_Maxs[0]==16);
  CHECK(!traceStartSolid&&players[0].pos.x-c.pos.x>32.f);
 }
 // A close human changes the relative basis after the first sideways move.
 // Alternating desired/idle directions must not reverse the world escape.
 for(int mode=0;mode<2;++mode){
  reset(c,s);trafficGeometry=contactWallGeometry=true;c.id=1;c.pos=Vector3f(200,0,30.125f);
  player(0,232.12535f,0,true);player(1,c.pos.x,0,false);players[0].pos.z=players[1].pos.z=c.pos.z;
  for(int frame=0;frame<8;++frame){
   s.SetTarget(mode&&frame%2==1?c.pos+Vector3f(-100,0,0):c.pos,32);s.UpdateSteering();v=s.GetMoveVector();
   CHECK(v.y>.5f&&std::fabs(v.x)<.01f);AABB hull;c.GetNavigationHull(hull,0);obTraceResult proof;
   EngineFuncs::TraceLine(proof,c.pos,c.pos+v*5.f,&hull,15,c.id,0);CHECK(!proof.m_StartSolid&&proof.m_Fraction==1.f);
   c.pos=c.pos+v*5.f;players[1].pos=c.pos;gameTime+=50;
  }
  CHECK(c.pos.y>25.f);printf("world escape mode=%d final=(%.3f %.3f)\n",mode,c.pos.x,c.pos.y);fflush(stdout);
 }
 // If a previous retreat reaches an obstruction near expiry, a newly safe
 // lateral direction receives a complete new750ms lease, not the old tail.
 reset(c,s);trafficGeometry=contactWallGeometry=true;c.id=1;c.pos=Vector3f(200,0,30.125f);
 player(0,232.12535f,0,true);player(1,c.pos.x,0,false);players[0].pos.z=players[1].pos.z=c.pos.z;
 s.SetTarget(c.pos,32);blockedSide=1;s.UpdateSteering();CHECK(s.GetMoveVector().y<-.5f);
 gameTime+=700;blockedSide=-1;s.UpdateSteering();CHECK(s.GetMoveVector().y>.5f&&s.Until()==gameTime+750);
 // A cached direction is never a collision exemption: newly missing ground
 // or live allies surrounding all escape directions still produce no input.
 for(int blocked=0;blocked<2;++blocked){
  reset(c,s);trafficGeometry=contactWallGeometry=true;c.id=1;c.pos=Vector3f(200,0,30.125f);
  player(0,232.12535f,0,true);player(1,c.pos.x,0,false);players[0].pos.z=players[1].pos.z=c.pos.z;
  s.SetTarget(c.pos,32);s.UpdateSteering();CHECK(s.GetMoveVector().y>.5f);gameTime+=50;
  if(blocked==0)groundSafe=false;
  else{player(2,200,40,false);player(3,200,-40,false);player(4,160,0,false);for(int id=2;id<=4;++id)players[id].pos.z=c.pos.z;}
  s.UpdateSteering();CHECK(s.GetMoveVector().SquaredLength()==0);
 }
 printf("steering avoidance: %d checks, 0 failures\n",checks);return 0;
}
