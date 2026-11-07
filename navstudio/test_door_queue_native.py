"""Compile actual door FIFO and its real FollowPath/controller integration.

Actor observations and collision responses are independent fixture services.
No BSP, waypoint edits, game installation or live-mover claim is involved.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless

from test_door_runtime_native import COMMON, body, compile_run
from test_door_update_native import program as controller_program


def program():
    source = controller_program().split('int main(){', 1)[0]
    source = source.replace('struct GameEntity {int id;bool IsValid()const{return id>=0;}};', r'''
struct GameEntity {int id=-1,serial=0;bool IsValid()const{return id>=0;}
 int AsInt()const{return (serial<<16)|id;}void FromInt(int n){id=n&65535;serial=n>>16;}};
''')
    source = source.replace('struct Steering {Vector3f target;',
                            'struct Steering {Vector3f target;int noAvoid=0;void SetNoAvoidTime(int n){noAvoid=n;}')
    source = source.replace('int buttons=0,resets=0,team=1;',
                            'int buttons=0,resets=0,team=1,gameId=1,serial=0;bool allWalkBlocked=false;')
    source = source.replace('int GetGameID()const{return 1;}',
                            'int GetGameID()const{return gameId;}')
    source = source.replace('GameEntity GetGameEntity()const{return{1};}',
                            'GameEntity GetGameEntity()const{return{gameId,serial};}')
    source = source.replace('bool CanWalkTo(const Vector3f&p,NavFlags=0){',
                            'bool CanWalkTo(const Vector3f&p,NavFlags=0){if(allWalkBlocked)return false;')
    source = source.replace('struct Engine{GameEntity EntityFromID(int n){return{n};}}engine;', r'''
struct obPlayerInfo{enum{MaxPlayers=64};enum Controller{Bot,Human,Both};
 struct PInfo{int m_Team=0;Controller m_Controller=Bot;};PInfo m_Players[64];};
obPlayerInfo population;std::map<int,bool>dead;std::map<int,Vector3f>actors;int moverSerial=0;
struct Engine{GameEntity EntityFromID(int n){return{n,n==40?moverSerial:0};}
 void GetPlayerInfo(obPlayerInfo&out){out=population;}}engine;
''')
    source = source.replace('namespace InterfaceFuncs{obUserData',
                            'namespace InterfaceFuncs{bool IsAlive(GameEntity e){return e.IsValid()&&!dead[e.AsInt()];}obUserData')
    source = source.replace('namespace EngineFuncs{void ConsoleMessage',
        'namespace EngineFuncs{bool EntityPosition(GameEntity e,Vector3f&out){auto it=actors.find(e.id);if(it==actors.end())return false;out=it->second;return true;}void ConsoleMessage')
    source = source.replace('namespace EngineFuncs{bool EntityPosition', r'''
enum{TR_MASK_SOLID=1,TR_MASK_PLAYERCLIP=2,TR_MASK_PLAYER=4,TR_MASK_NAVCAPSULE=8,False=0};
struct obTraceResult{bool m_StartSolid=false;float m_Fraction=1.f;};
bool allTrafficBlocked=false;int trafficChecks=0;
namespace EngineFuncs{
 bool TraceLine(obTraceResult&trace,const Vector3f&start,const Vector3f&end,const AABB*hull,int mask,int user,int){
  ++trafficChecks;assert(hull&&mask==15);trace=obTraceResult();
  if(allTrafficBlocked){trace.m_Fraction=.1f;return true;}
  const Vector3f direction=end-start;const float n=direction.Dot(direction);
  for(const auto&actor:actors){if(actor.first==user||dead[actor.first]||std::fabs(actor.second.z-start.z)>64)continue;
   const float t=n?ClampT((actor.second-start).Dot(direction)/n,0.f,1.f):0;
   if(SquaredLength2d(actor.second,start+direction*t)<32.f*32.f){trace.m_Fraction=MinT(t,.99f);return true;}}
  return true;
 }
 bool EntityPosition''')
    source = source.replace('void ResetTceDoorQueue(){}int UpdateTceDoorQueue(const Path::PathPoint&){return 0;}', r'''
 int m_TceDoorQueueBot=-1,m_TceDoorQueueStart=0,m_TceDoorQueueWait=0,m_TceDoorQueueBlocked=0;
 void ResetTceDoorQueue();int UpdateTceDoorQueue(const Path::PathPoint&);
''')
    queue = (COMMON / 'TceDoorQueue.h').read_text()
    source = source.replace('class FollowPath:public State{public:', queue + '\nstatic TceDoorQueue tceDoorQueue;\nclass FollowPath:public State{public:')
    follow = (COMMON / 'BotBaseStates.cpp').read_text()
    source += '\n'.join(body(follow, marker) for marker in
        ('void FollowPath::ResetTceDoorQueue(', 'static bool TceDoorHumanTraffic(',
         'int FollowPath::UpdateTceDoorQueue('))
    return '#include <cstddef>\n' + source + r'''
void clear(){tceDoorQueue=TceDoorQueue();dead.clear();actors.clear();population=obPlayerInfo();
 moverSerial=0;sweepObservation=1;allTrafficBlocked=false;trafficChecks=0;IGame::time=1000;logs.clear();}
int queue(FollowPath&f){Path::PathPoint p;assert(f.m_CurrentPath.GetCurrentPt(p));return f.UpdateTceDoorQueue(p);}
void keep(TceDoorQueue&q,int now,int first,int second){q.Request(now,first,first,40,7,true);q.Request(now,second,second,40,7,true);}
int main(){
 assert(sizeof(TceDoorQueue::Ticket)==32&&sizeof(TceDoorQueue)==2052);
 assert(offsetof(TceDoorQueue::Ticket,active)==28&&offsetof(TceDoorQueue::Ticket,owner)==29);
 // Same-frame tie order is client ID, not update order. No preemption during
 // traversal; younger arrivals cannot steal an established passage lease.
 {TceDoorQueue q;assert(!q.Request(1000,9,9,40,7,true).owner);
  assert(!q.Request(1000,2,2,40,7,true).owner);
  assert(!q.Request(1050,9,9,40,7,true).owner);
  assert(q.Request(1050,2,2,40,7,true).owner);
  assert(!q.Request(1050,1,1,40,7,true).owner);
  assert(q.Request(1100,2,2,40,7,true).owner);
  q.Release(2);assert(q.Request(1100,9,9,40,7,true).owner);
  q.Release(9);assert(q.Request(1100,1,1,40,7,true).owner);}
 // Human priority retains the head's position in line; other doors are
 // independent, including differently serialized movers in the same slot.
 {TceDoorQueue q;q.Request(1000,1,1,40,7,false);q.Request(1000,2,2,40,7,true);
  assert(!q.Request(1050,1,1,40,7,false).owner&&!q.Request(1050,2,2,40,7,true).owner);
  q.Request(1050,3,3,41,7,true);assert(q.Request(1100,3,3,41,7,true).owner);
  assert(q.Request(1100,1,1,40,7,true).owner);
  q.Request(1100,4,4,40+(1<<16),7,true);assert(q.Request(1150,4,4,40+(1<<16),7,true).owner);}
 // Abandoned tickets expire, explicit release works, backwards time resets
 // the entire map's leases, and active waiting cannot become an endless jam.
 {TceDoorQueue q;q.Request(1000,1,1,40,7,true);q.Request(1050,1,1,40,7,true);
  q.Request(2000,2,2,40,7,true);assert(q.Request(3100,2,2,40,7,true).owner);
  assert(!q.Request(100,3,3,40,7,true).owner);assert(q.Request(150,3,3,40,7,true).owner);
  for(int t=1100;t<45150;t+=1000)assert(q.Request(t,3,3,40,7,true).owner);
  assert(q.Request(45150,3,3,40,7,true).rank==-1);
  TceDoorQueue human;q=human;q.Request(100,1,1,40,7,false);
  for(int t=1100;t<60100;t+=1000)assert(!q.Request(t,1,1,40,7,false).owner);
  assert(q.Request(60100,1,1,40,7,false).rank==-1);}
 // Capacity bounded; no array growth, pointer ownership or overwritten turn.
 {TceDoorQueue q;for(int n=0;n<64;++n)assert(q.Request(1000,n,n,40,7,true).rank==n);
  assert(q.Request(1000,65,65,40,7,true).rank==-1);q.Release(20);
  assert(q.Request(1050,65,65,40,7,true).rank>=0);}

 clear();FollowPath first,second;setup(first,false);setup(second,false);
 first.client.gameId=2;second.client.gameId=9;
 assert(queue(second)==1&&queue(first)==1);IGame::time+=50;
 assert(queue(second)==1&&second.client.steer.target.x<=-144);
 assert(std::fabs(second.client.steer.target.y)==64&&second.client.steer.noAvoid==250);
 assert(queue(first)==0);assert(tick(first)==1&&first.m_TceDoorPhase==2);
 // No USE or action activation by a waiting bot; it keeps the entrance clear.
 assert(second.m_TceDoorPhase==0&&!(second.client.buttons&BOT_BUTTON_USE));
 first.ResetTceDoor();assert(queue(second)==0);

 // Native static walk geometry deliberately excludes player traffic. A live
 // full-hull sweep rejects an occupied first bay and chooses the other side;
 // if all bay sweeps are blocked the bot stays still, despite static clearance.
 clear();static FollowPath occupied;setup(occupied,false);occupied.client.gameId=9;
 population.m_Players[5].m_Controller=obPlayerInfo::Human;population.m_Players[5].m_Team=1;
 actors[5]={32,0,30};actors[8]={-96,64,30};
 assert(queue(occupied)==1&&occupied.client.steer.target.y==-64&&trafficChecks==2);
 allTrafficBlocked=true;IGame::time+=50;assert(queue(occupied)==1&&occupied.client.steer.target.x==0);

 // Every out-of-band traversal contract stops queue lookahead. No setting of
 // steering suppression may get ahead of a scripted path-through or landing.
 for(int kind=0;kind<7;++kind){clear();static FollowPath script;script=FollowPath();
  auto&lead=script.m_CurrentPath.AddPt({-128,0,38},35).NavId(10);
  switch(kind){case 0:lead.m_OnPathThrough=1;break;case 1:lead.m_TceAscentRelease=true;break;
   case 2:lead.m_TceAscentAlignment=true;break;case 3:lead.m_TceWalkDrop=true;break;
   case 4:lead.m_TceStandJump=true;break;case 5:lead.m_TceFreeClimb=true;break;case 6:lead.m_NavFlags=F_NAV_SCRIPT;break;}
  script.m_CurrentPath.AddPt({0,0,38},35).Flags(F_NAV_DOOR).NavId(1).TceDoor(recipe(false));
  script.m_CurrentPath.AddPt({128,0,38},35).NavId(2);state(0);
  assert(queue(script)==0&&script.client.steer.noAvoid==0&&script.m_TceDoorQueueBot==-1);}
 for(int kind=0;kind<4;++kind){clear();static FollowPath action;action=FollowPath();setup(action,false);
  queue(action);assert(action.m_TceDoorQueueBot!=-1);
  switch(kind){case 0:action.m_PassThroughState=1;break;case 1:action.m_TceDropNavId=1;break;
   case 2:action.m_TceStandPhase=1;break;case 3:action.m_TceFreePhase=1;break;}
  assert(queue(action)==0&&action.m_TceDoorQueueBot==-1);}

 // A newly constructed controller must not inherit a static owner ticket
 // even when both index and serial were reused with a monotonic game clock.
 clear();tceDoorQueue.Request(900,1,1,40,7,true);assert(tceDoorQueue.Request(950,1,1,40,7,true).owner);
 static FollowPath fresh;setup(fresh,false);assert(queue(fresh)==1);
 IGame::time+=50;assert(queue(fresh)==0);

 // Live allied human in either approach has priority. Side corridors, other
 // floors, enemies, spectators and dead players cannot hold this doorway.
 clear();FollowPath human;setup(human,false);population.m_Players[5].m_Controller=obPlayerInfo::Human;
 population.m_Players[5].m_Team=1;actors[5]={64,0,30};
 assert(queue(human)==1);IGame::time+=50;assert(queue(human)==1&&human.client.steer.target.x<=-96);
 actors[5]={64,80,30};IGame::time+=50;assert(queue(human)==0);
 actors[5]={64,0,130};assert(queue(human)==0);
 actors[5]={64,0,30};population.m_Players[5].m_Team=2;assert(queue(human)==0);
 population.m_Players[5].m_Team=1;dead[5]=true;assert(queue(human)==0);
 dead[5]=false;human.m_TceDoorPhase=4;human.m_TceDoorEntry=*human.m_CurrentPath.GetPt(0);
 human.m_TceDoorExit=*human.m_CurrentPath.GetPt(1);human.client.position={32,0,30};
 assert(queue(human)==0); // do not halt a bot already clearing the threshold

 // Dead owner, recycled player/mover, path reset and clock rollback never
 // retain a former life/map's exclusive access. Raw recipe must still match.
 clear();FollowPath a,b;setup(a,false);setup(b,false);b.client.gameId=2;
 queue(a);queue(b);IGame::time+=50;assert(queue(a)==0&&queue(b)==1);
 dead[a.client.GetGameEntity().AsInt()]=true;assert(queue(b)==0);
 assert(queue(a)==1&&a.m_TceDoorQueueBot==-1);
 b.ResetTceDoor();assert(b.m_TceDoorQueueBot==-1);dead[1]=false;a.client.serial=1;
 assert(queue(a)==1);IGame::time+=50;assert(queue(a)==0);
 moverSerial=1;assert(queue(a)==1);IGame::time+=50;assert(queue(a)==0);
 stats[{40,"tce_nav_model"}]=99;assert(queue(a)==-1);

 // Unknown/negative sweep observations and blocked walks cause no guessed
 // move; the bounded failure gets dispatched by the actual full Update.
 clear();FollowPath blocked;setup(blocked,false);blocked.client.allWalkBlocked=true;
 population.m_Players[5].m_Controller=obPlayerInfo::Human;population.m_Players[5].m_Team=1;actors[5]={32,0,30};
 blocked.Update(.05f);assert(blocked.failures==0&&blocked.client.steer.target.x==0);
 IGame::time+=2000;blocked.Update(.05f);assert(blocked.failures==1&&blocked.m_TceDoorQueueBot==-1);
 clear();FollowPath unknown;setup(unknown,false);queue(unknown);IGame::time+=50;
 population.m_Players[5].m_Controller=obPlayerInfo::Human;population.m_Players[5].m_Team=1;actors[5]={32,0,30};
 sweepObservation=-1;assert(queue(unknown)==1&&unknown.client.steer.target.x==0);
 clear();static FollowPath crouched;setup(crouched,false);
 Path::PathPoint low=*crouched.m_CurrentPath.GetPt(0);low.Flags(F_NAV_DOOR|F_NAV_CROUCH);
 crouched.m_CurrentPath.Clear();crouched.m_CurrentPath.AddPt(low.m_Pt,35).Flags(low.m_NavFlags).NavId(1).TceDoor(recipe(false));
 crouched.m_CurrentPath.AddPt({128,0,38},35).NavId(2);
 population.m_Players[5].m_Controller=obPlayerInfo::Human;population.m_Players[5].m_Team=1;actors[5]={32,0,30};
 assert(queue(crouched)==1&&(crouched.client.buttons&BOT_BUTTON_CROUCH));

 // Full controller hook actually delays USE; it cannot be bypassed by the
 // previous action controller merely being able to open this door.
 clear();FollowPath whole;setup(whole,false);whole.Update(.05f);
 assert(whole.m_TceDoorPhase==0&&!(whole.client.buttons&BOT_BUTTON_USE));
 IGame::time+=50;whole.Update(.05f);assert(whole.m_TceDoorPhase==2);
 IGame::time+=500;whole.Update(.05f);assert(whole.client.buttons&BOT_BUTTON_USE);
 printf("TceDoorQueue bytes=%zu Ticket bytes=%zu active=%zu owner=%zu lastTime=%zu\n",
  sizeof(TceDoorQueue),sizeof(TceDoorQueue::Ticket),offsetof(TceDoorQueue::Ticket,active),
  offsetof(TceDoorQueue::Ticket,owner),64*sizeof(TceDoorQueue::Ticket));
 printf("door queue: FIFO, human priority, checked waiting, lifecycle, timeout and actual Update passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Native Windows C++ toolchain required')
class DoorQueueTests(TestCase):
    def test_production_queue_and_full_controller(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
