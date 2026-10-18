"""Run the actual complete FollowPath::Update over successive door frames.

Actor positions, static walk outcomes and door state observations are supplied
independently. This tests controller integration, not simulated game physics.
The real path storage/lookahead, directed metadata, action functions, complete
Update and final notification code execute unchanged.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_door_runtime_native import COMMON, body, compile_run, runtime_program


def program():
    source = runtime_program().split('int main(){', 1)[0]
    source = source.replace('namespace COLOR{constexpr int GREEN=1;}',
                            'namespace COLOR{constexpr int GREEN=1,RED=2;}')
    source = source.replace('Vector3f Flatten()const', r'''
 float Dot(const Vector3f&b)const{return x*b.x+y*b.y+z*b.z;}float SquaredLength()const{return x*x+y*y+z*z;}
 void ToSpherical(float&h,float&p,float&r)const{r=Length();h=std::atan2(y,x);p=r?std::asin(z/r):0;}
 void FromSpherical(float h,float p,float r){x=r*std::cos(p)*std::cos(h);y=r*std::cos(p)*std::sin(h);z=r*std::sin(p);}
 Vector3f Flatten()const''')
    source = source.replace('float Sqrt(float x){return std::sqrt(x);}',
                            'float Sqrt(float x){return std::sqrt(x);}float Sqr(float x){return x*x;}float DegToRad(float x){return x*.01745329252f;}')
    source = source.replace('namespace Priority {enum{High=1};}', r'''
 enum{F_NAV_SNEAK=16384,F_NAV_PRONE=8192,F_NAV_UNDERWATER=32768,ENT_FLAG_ONLADDER=2,
 BOT_BUTTON_AIM=4,BOT_BUTTON_JUMP=8,BOT_BUTTON_LEANLEFT=16,BOT_BUTTON_LEANRIGHT=32,BOT_BUTTON_SPRINT=64,
 BOT_BUTTON_PRONE=128,BOT_BUTTON_MOVEUP=256,BOT_BUTTON_MOVEDN=512,BOT_BUTTON_WALK=1024,
 BOT_BUTTON_TCE_STAND_LAUNCH=2048,BOT_BUTTON_TCE_STAND_PRIME=4096,BOT_BUTTON_TCE_FREECLIMB=8192};
 namespace Priority {enum{High=1,Idle=0};}
 #define Prof(x)
 #define OBASSERT(x,y) assert(x)
 ''')
    source = source.replace('struct Steering {', r'''
 struct Destination{Vector3f m_Position;float m_Radius;};using DestinationVector=std::vector<Destination>;
 struct User{int successes=0;unsigned GetFollowUserName(){return 1;}void OnPathSucceeded(){++successes;}bool GetNextDestination(DestinationVector&,bool&,bool&){return false;}};
 struct State{enum StateStatus{State_Busy,State_Finished};bool OnPathThrough(const String&){return false;}};
 struct LowLevel{State*FindState(unsigned){return nullptr;}};
 struct Box3f{float Extent[3]={16,16,43.5f};Vector3f Center;};
 struct Steering {''')
    source = source.replace('Vector3f GetEyePosition(){return position.AddZ(52);}', r'''
 bool ResolveWalkDestination(const Vector3f&p,float,NavFlags f,Vector3f&out){out=p;return CanWalkTo(p,f);}
 Vector3f GetEyePosition(){return position.AddZ(52);}Vector3f GetFacingVector(){return{1,0,0};}
 Vector3f GetVelocity(){return{};}void ProcessGotoNode(const Path&){}int GetStuckTime(){return 0;}
 LowLevel*GetLowLevel(){static LowLevel l;return&l;}Box3f bounds;const Box3f&GetWorldBounds(){bounds.Center=position.AddZ(13.5f);return bounds;}
 ''')
    source = source.replace('class IGame{public:static int time;static int GetTime(){return time;}}',
                            'class IGame{public:static int time;static int GetTime(){return time;}static int GetFrameNumber(){return time/50;}}')
    source = source.replace('class FollowPath{public:Client client;struct{int m_MoveMode=0;}m_Query;', r'''
 class FollowPath:public State{public:Client client;
 struct{int m_MoveMode=0;bool m_Final=true;User*m_User=nullptr;}m_Query,m_SavedQuery;
 enum{PathPending,PathInProgress,PathFinished,PathNotFound};int m_PathStatus=PathInProgress;
 int m_PassThroughState=0,m_PathThroughPtIndex=0,m_LastStuckTime=0,m_LadderDirection=0;
 int m_TceExitStartTime=0,m_TceExitNavId=0,m_TceDropStartTime=0,m_TceDropNavId=0;
 int m_TceDropLaunchNavId=0,m_TceDropSettleStart=0,m_TceDropStableSince=0;
 int m_TceDropTrafficStart=0,m_TceDropTrafficLast=0,m_TceDropTrafficCredit=0,m_TceDropTrafficNavId=0;
 bool m_TceDropTrafficContact=false;Vector3f m_TceDropTrafficPosition;
 int m_TceStandNavId=0,m_TceStandPhase=0,m_TceStandStart=0,m_TceStandStable=0,m_TceStandAttempt=0;
 int m_TceFreeNavId=0,m_TceFreePhase=0,m_TceFreeStart=0;
 int m_TceReentryAttempts=0;bool m_TceReentryPlanning=false,m_TceReentryPendingProgress=false;Vector3f m_TceReentryPosition;
 Vector3f m_TceReentryBudgetPosition,m_TceReentryDestination;float m_TceReentryBestRemaining=-1;int m_TceReentryCheckpointId=0;
 bool m_TceAirWaitActive=false;int m_TceAirWaitStart=0,m_TceOrdinaryLadderStart=0;
 Vector3f m_PtOnPath;static bool m_OldLadderStyle;int failures=0;
 void NotifyUserFailed(int){++failures;}void NotifyUserSuccess();bool Repath(){return false;}
 bool Goto(User*,const DestinationVector&,int,bool,bool){return false;}
 bool CheckForMover(const Vector3f&){return false;}void CheckForLowJumps(const Vector3f&){}void CheckForGapJumps(const Vector3f&){}
 StateStatus Update(float);bool DebugDrawingEnabled(){return false;}
 bool TryTceReentry(const Path::PathPoint&){return false;}
 void ReplenishTceReentryBudget(const Path::PathPoint&){}
 bool WaitForTceGround(const Path::PathPoint&);
 int UpdateTceOrdinaryLadderContact(const Path::PathPoint&){return 0;}
 void ResetTceOrdinaryLadderContact(){m_TceOrdinaryLadderStart=0;}
 int UpdateTceDropTraffic(const Path::PathPoint&){return 0;}
 bool MaintainTceDropTrafficLease(const Path::PathPoint&){return false;}
 void ResetTceDropTrafficLease();
 ''')
    source = source.replace('namespace EngineFuncs{void ConsoleMessage(const char*s){logs.emplace_back(s);}}',
                            'namespace EngineFuncs{void ConsoleMessage(const char*s){logs.emplace_back(s);}void DrawLine(const Vector3f&,const Vector3f&,int,float){}}')
    source = source.replace('namespace Utils {float ClosestPtOnLine',
                            'namespace Utils {constexpr float FloatMax=1e30f;String HashToString(unsigned){return "fixture";}float ClosestPtOnLine')
    prelude = r'''
struct FollowPathUser{enum{Blocked=2};};bool FollowPath::m_OldLadderStyle=false;
float SquaredLength(const Vector3f&a,const Vector3f&b){return(a-b).SquaredLength();}
Vector3f Interpolate(const Vector3f&a,const Vector3f&b,float f){return a+(b-a)*f;}
float PointToSegmentDistance(const Vector3f&p,const Vector3f&a,const Vector3f&b,Vector3f&out,float&t){
 const Vector3f d=b-a;const float n=d.SquaredLength();t=n?ClampT((p-a).Dot(d)/n,0.f,1.f):0;
 out=a+d*t;return(p-out).Length();}
'''
    path = (COMMON/'Path.cpp').read_text()
    funcs = '\n'.join(body(path, marker) for marker in
                     ('bool Path::GetFirstPt(', 'bool Path::GetLastPt(', 'bool Path::GetPt(', 'Vector3f Path::FindNearestPtOnPath('))
    follow = (COMMON/'BotBaseStates.cpp').read_text()
    return source + prelude + funcs + body(follow, 'void FollowPath::NotifyUserSuccess()') + \
        body(follow, 'bool FollowPath::WaitForTceGround(') + \
        body(follow, 'State::StateStatus FollowPath::Update(') + r'''
void frame(FollowPath&f){IGame::time+=50;f.client.buttons=0;f.Update(.05f);}
void arriveCurrent(FollowPath&f){Path::PathPoint p;f.m_CurrentPath.GetCurrentPt(p);f.client.position=RawWpPos(p);}
int main(){Waypoint a{1,{0,0,30}},exit{2,{128,0,30}},bend{3,{0,128,30}},button{4,{128,128,30}},goal{5,{256,0,30}};
 a.props.p={{"tce_door_entity","40"},{"tce_door_use_entity","41"},{"tce_door_target","2"},
 {"tce_door_mode","button"},{"tce_door_team","3"},{"tce_door_role","entry"},{"tce_door_model","7"},
 {"tce_door_use_model","8"},{"tce_door_use_nav","4"},{"tce_door_entry","1"},{"tce_door_aim","32 0 60"}};
 a.m_Connections={{&exit},{&bend}};bend.m_Connections={{&a},{&button}};button.m_Connections={{&bend}};
 exit.props=a.props;exit.props.p["tce_door_role"]="exit";exit.props.p["tce_door_target"]="0";
 a.flags=exit.flags=F_NAV_DOOR;planner.m_WaypointList={&a,&exit,&bend,&button,&goal};
 static FollowPath f;User owner;state(0,true);planner.m_Client=&f.client;planner.m_Solution={&goal,&exit,&a};
 planner.GetPath(f.m_CurrentPath,0);assert(f.m_CurrentPath.GetPt(1)->m_TceDoor.role==3&&f.m_CurrentPath.GetPt(1)->m_TceDoor.Valid());
 f.m_Query.m_User=&owner;frame(f);
 assert(f.m_TceDoorPhase==1&&owner.successes==0);
 for(int i=0;i<6&&f.m_TceDoorPhase==1;++i){arriveCurrent(f);frame(f);assert(owner.successes==0);}
 assert(f.m_TceDoorPhase==2);state(1,true);frame(f);assert(owner.successes==0);
 state(2,true);f.client.passable=true;frame(f);assert(f.m_TceDoorPhase==3);
 for(int i=0;i<6&&f.m_TceDoorPhase==3;++i){arriveCurrent(f);frame(f);assert(owner.successes==0);}
 assert(f.m_TceDoorPhase==4&&f.m_CurrentPath.GetCurrentPtIndex()==0);
 frame(f);assert(f.m_CurrentPath.GetCurrentPtIndex()==1&&f.client.steer.target.x==128);
 f.client.position={64,0,30};frame(f);assert(f.m_CurrentPath.GetCurrentPtIndex()==1&&owner.successes==0);
 f.client.position={128,0,30};frame(f);
 assert(f.m_TceDoorPhase==0&&f.m_CurrentPath.GetCurrentPtIndex()==2&&f.client.steer.target.x==256&&owner.successes==0);
 frame(f);assert(f.m_CurrentPath.GetCurrentPtIndex()==2&&f.client.steer.target.x==256&&owner.successes==0);
 f.client.position={256,0,30};frame(f);assert(owner.successes==1&&f.m_PathStatus==FollowPath::PathFinished&&f.failures==0);
 int trips=0,egress=0;for(const auto&s:logs){trips+=s.find("TCE2DOOR TO_SWITCH")!=String::npos;egress+=s.find("TCE2DOOR EGRESS ")!=String::npos;}
 assert(trips==1&&egress==1);
 // Insufficient actual remaining time selects the accepted ordinary bypass,
 // never another activation trip, and retains the original final destination.
 button.m_Connections.push_back({&exit});static FollowPath bypass;User bypassOwner;
 state(2,true);stats[{40,"tce_door_open_remaining"}]=100;bypass.client.passable=true;
 planner.m_Client=&bypass.client;planner.m_Solution={&goal,&exit,&a};planner.GetPath(bypass.m_CurrentPath,0);
 bypass.m_Query.m_User=&bypassOwner;frame(bypass);assert(bypass.m_TceDoorPhase==5&&bypass.m_TceDoorBypassCount==1);
 for(int i=0;i<12&&bypass.m_TceDoorPhase==5;++i){arriveCurrent(bypass);frame(bypass);assert(bypassOwner.successes==0);}
 assert(bypass.m_TceDoorPhase==4&&bypass.m_CurrentPath.GetCurrentPtIndex()==1);
 // The safe EXIT is outside the rotor sweep; the following ordinary goal is
 // inside it. The actual read-only sweep outcome must hold movement until the
 // door has naturally closed; it cannot be ignored just because WALK is clear.
 stats[{40,"tce_door_open_remaining"}]=30000;sweepObservation=1;
 frame(bypass);assert(bypass.m_TceDoorPhase==4&&bypassOwner.successes==0);
 assert(bypass.client.steer.target.x==128&&bypass.m_CurrentPath.GetCurrentPtIndex()==1);
 IGame::time+=9000;frame(bypass);assert(bypass.m_TceDoorPhase==4&&bypass.failures==0);
 sweepObservation=0;state(3,true);frame(bypass);assert(bypass.m_TceDoorPhase==4);
 state(0,true);frame(bypass);assert(bypass.m_TceDoorPhase==0&&bypass.client.steer.target.x==256);
 arriveCurrent(bypass);frame(bypass);assert(bypassOwner.successes==1&&bypass.failures==0);
 int windows=0;trips=0;for(const auto&s:logs){windows+=s.find("TCE2DOOR WINDOW_BYPASS")!=String::npos;trips+=s.find("TCE2DOOR TO_SWITCH")!=String::npos;}
 assert(windows==1&&trips==1);
 // A remote switch really opened, but the checked return took longer than its
 // timer. Initial closed state still activates once; closing state waits;
 // already closed after return bypasses once rather than repeating USE trips.
 static FollowPath expired;User expiredOwner;const size_t expiredLog=logs.size();
 state(0,true);planner.m_Client=&expired.client;planner.m_Solution={&goal,&exit,&a};
 planner.GetPath(expired.m_CurrentPath,0);expired.m_Query.m_User=&expiredOwner;frame(expired);
 assert(expired.m_TceDoorPhase==1&&expired.m_TceDoorBypassCount==0);
 for(int i=0;i<12&&expired.m_TceDoorPhase==1;++i){arriveCurrent(expired);frame(expired);}
 assert(expired.m_TceDoorPhase==2);state(1,true);frame(expired);assert(expired.m_TceDoorPhase==3);
 IGame::time+=5000;state(3,true);expired.client.passable=true;
 for(int i=0;i<12&&expired.m_TceDoorPhase==3;++i){arriveCurrent(expired);frame(expired);}
 assert(expired.m_TceDoorPhase==4);frame(expired);
 assert(expired.m_TceDoorPhase==4&&expired.m_TceDoorBypassCount==0&&expired.m_TceDoorCycles==1);
 state(0,true);frame(expired);assert(expired.m_TceDoorPhase==5&&expired.m_TceDoorBypassCount==1);
 for(int i=0;i<12&&expired.m_TceDoorPhase==5;++i){arriveCurrent(expired);frame(expired);}
 assert(expired.m_TceDoorPhase==4);frame(expired);
 assert(expired.m_TceDoorPhase==0&&expired.client.steer.target.x==256);
 arriveCurrent(expired);frame(expired);assert(expiredOwner.successes==1&&expired.failures==0);
 int expiredTrips=0,expiredUse=0,expiredBypass=0,expiredPhysical=0;
 for(size_t i=expiredLog;i<logs.size();++i){const auto&s=logs[i];
  expiredTrips+=s.find("TCE2DOOR TO_SWITCH")!=String::npos;expiredUse+=s.find("TCE2DOOR USE ")!=String::npos;
  expiredBypass+=s.find("TCE2DOOR BYPASS_EGRESS ")!=String::npos;
  expiredPhysical+=s.find("TCE2DOOR PASSAGE ")!=String::npos||s.find("TCE2DOOR EGRESS ")!=String::npos;}
 assert(expiredTrips==1&&expiredUse==1&&expiredBypass==1&&expiredPhysical==0);

 // Local separated USE has a different contract: wait for state2 at the
 // safe side, then align ENTRY and finish the original destination once.
 a.props.p["tce_door_mode"]="3";a.props.p["tce_door_use_entity"]="40";
 a.props.p["tce_door_use_model"]="7";a.props.p["tce_door_use_nav"]="4";
 button.pos={-32,0,30};button.flags=F_NAV_DOOR;button.props=a.props;
 button.props.p["tce_door_role"]="1";button.props.p["tce_door_target"]="1";
 exit.props=a.props;exit.props.p["tce_door_role"]="3";exit.props.p["tce_door_target"]="0";
 a.m_Connections={{&exit},{&button}};button.m_Connections={{&a}};
 static FollowPath local;User localOwner;state(0);sweepObservation=1;
 local.client.position=button.pos;local.client.passable=true;planner.m_Client=&local.client;
 planner.m_Solution={&goal,&exit,&a,&button};planner.GetPath(local.m_CurrentPath,0);
 local.m_Query.m_User=&localOwner;frame(local);assert(local.m_TceDoorPhase==1);
 frame(local);assert(local.m_TceDoorPhase==2&&(local.client.buttons&BOT_BUTTON_USE));
 state(1);for(int i=0;i<4;++i){frame(local);assert(local.m_TceDoorPhase==2&&local.client.steer.target.x==-32);}
 assert(localOwner.successes==0&&local.failures==0);
 state(2);stats[{40,"tce_door_open_remaining"}]=100;frame(local);
 assert(local.m_TceDoorPhase==2&&local.client.steer.target.x==-32);
 stats[{40,"tce_door_open_remaining"}]=-1;frame(local);assert(local.m_TceDoorPhase==3);
 for(int i=0;i<6&&local.m_TceDoorPhase==3;++i){arriveCurrent(local);frame(local);assert(localOwner.successes==0);}
 assert(local.m_TceDoorPhase==4&&local.m_CurrentPath.GetCurrentPtIndex()==1);
 frame(local);assert(local.m_CurrentPath.GetCurrentPtIndex()==2&&local.client.steer.target.x==128);
 local.client.position={64,0,30};frame(local);assert(localOwner.successes==0&&local.m_TceDoorPhase==4);
 local.client.position=exit.pos;frame(local);
 assert(local.m_TceDoorPhase==0&&local.m_CurrentPath.GetCurrentPtIndex()==3&&local.client.steer.target.x==256);
 local.client.position=goal.pos;frame(local);assert(localOwner.successes==1&&local.failures==0);
 printf("real complete Update: physical crossing, timed bypass, closed-after-return bypass and precise logging retain the original goal exactly once\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class DoorUpdateTests(TestCase):
    def test_actual_complete_update_finishes_ordinary_goal_after_remote_crossing(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
