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
static bool separatedByWorld=false;
static int blockedSide=0;
static AABB observedHull;
static Vector3f observedStep;
struct Engine {void GetPlayerInfo(obPlayerInfo&i){++infos;for(int n=0;n<64;++n)if(players[n].active){i.m_Players[n].m_Team=players[n].team;i.m_Players[n].m_Controller=players[n].human?obPlayerInfo::Human:obPlayerInfo::Bot;}}GameEntity EntityFromID(int id){return players[id].active?GameEntity(id,players[id].serial):GameEntity();}};
static Engine engine,*g_EngineFuncs=&engine;
namespace InterfaceFuncs {
 bool GetEntityCategory(GameEntity e,Flags&f){f.bits=players[e.id].category?1u<<ENT_CAT_PLAYER:0;return true;}
 bool GetEntityFlags(GameEntity e,Flags&f){f=players[e.id].flags;return true;}
 bool IsAlive(GameEntity e){return players[e.id].alive;}
}
namespace EngineFuncs {
 bool EntityPosition(GameEntity e,Vector3f&p){p=players[e.id].pos;return true;}
 bool EntityVelocity(GameEntity e,Vector3f&v){v=players[e.id].vel;return true;}
 bool TraceLine(obTraceResult&t,const Vector3f&,const Vector3f&end,const AABB*h,int mask,int,int){++traces;assert(mask==15);observedHull=*h;observedStep=end;for(int i=0;i<64;++i)if(players[i].active&&(players[i].pos-end).SquaredLength()<.001f){t.m_StartSolid=false;t.m_Fraction=.5f;t.m_HitEntity=separatedByWorld?GameEntity():GameEntity(i,players[i].serial);return true;}t.m_StartSolid=traceStartSolid;t.m_Fraction=traceClear?1.f:0.f;return true;}
}
class Client {
public:
 int id=2,team=1;bool tce=true,jump=false;Flags flags;Vector3f pos,move;FollowPath follow;
 Client(){flags.bits=1u<<ENT_FLAG_ONGROUND;}
 bool UseValidatedWalkEntries()const{return tce;}int GetGameID()const{return id;}int GetTeam()const{return team;}
 bool HasEntityFlag(int f)const{return flags.CheckFlag(f);}bool IsButtonDown(int)const{return jump;}
 const Vector3f&GetPosition()const{return pos;}FollowPath*GetFollowPath(){return &follow;}
 bool IsAllied(GameEntity e)const{return players[e.id].allied;}
 bool GetNavigationHull(AABB&h,NavFlags)const{h.m_Mins[0]=h.m_Mins[1]=-14;h.m_Mins[2]=-24;h.m_Maxs[0]=h.m_Maxs[1]=14;h.m_Maxs[2]=46;return hullValid;}
 bool CanWalkTo(const Vector3f&p)const{++walks;return groundSafe&&!(blockedSide>0&&p.y>0)&&!(blockedSide<0&&p.y<0);}
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
static void reset(Client&c,Steering&s){c=Client();s=Steering();s.Attach(&c);for(int i=0;i<64;++i)players[i]=Player();groundSafe=traceClear=hullValid=true;traceStartSolid=separatedByWorld=false;blockedSide=0;walks=traces=infos=0;gameTime=1000;s.SetTarget(Vector3f(500,0,0),32);}
static void player(int id,float x,float y,bool human){players[id].active=true;players[id].pos=Vector3f(x,y,0);players[id].human=human;players[id].flags.bits=1u<<ENT_FLAG_ONGROUND;}
int main(){
 Client c;Steering s;Vector3f v;
 reset(c,s);s.UpdateSteering();CHECK(s.GetMoveVector().x==1&&walks==0);
 player(3,64,0,true);s.UpdateSteering();v=s.GetMoveVector();CHECK(std::fabs(v.y)>.5f&&v.x>0&&traces==2&&walks==1);CHECK(observedHull.m_Mins[2]==-24&&observedHull.m_Maxs[2]==46);
 int side=s.Side();gameTime+=50;s.UpdateSteering();CHECK(s.Side()==side&&s.Until()==1750);
 reset(c,s);player(3,60,0,true);blockedSide=-1;s.UpdateSteering();CHECK(s.GetMoveVector().y>0&&walks==2&&traces==2);
 reset(c,s);player(3,60,0,true);groundSafe=false;s.UpdateSteering();CHECK(s.GetMoveVector().SquaredLength()==0&&walks==3&&traces==1);
 reset(c,s);player(3,60,0,true);traceClear=false;s.UpdateSteering();CHECK(s.GetMoveVector().SquaredLength()==0&&traces==4);
 reset(c,s);player(3,60,0,true);traceStartSolid=true;s.UpdateSteering();CHECK(s.GetMoveVector().SquaredLength()==0);
 reset(c,s);player(3,60,0,true);hullValid=false;s.UpdateSteering();CHECK(s.GetMoveVector().SquaredLength()==0&&walks==0);
 reset(c,s);player(3,60,0,true);s.SetNoAvoidTime(250);s.UpdateSteering();CHECK(s.GetMoveVector().x==1&&infos==0);gameTime=1251;s.UpdateSteering();CHECK(std::fabs(s.GetMoveVector().y)>.5f);
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
 printf("steering avoidance: %d checks, 0 failures\n",checks);return 0;
}
