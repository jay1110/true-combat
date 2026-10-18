"""Actual complete Update with a position-dependent, strict direct USE ray.

Northport378's observed actor offset hits the right leaf beyond 38.4 units.
The independent ray fixture uses its measured leaf plane and saved aim, so
activation becomes available only through an actual position change. It never
increases the game's range or bypasses its identity gate. No live claim.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_door_runtime_native import COMMON, body, compile_run
from test_door_update_native import program as door_program


def program():
    source = door_program().split('int main(){', 1)[0]
    source = source.replace('struct Steering {Vector3f target;',
                            'struct Steering {Vector3f target;float radius=0,input=1;')
    source = source.replace('void SetTarget(const Vector3f&p,float=0,int=0,bool=false,float=1){target=p;}',
                            'void SetTarget(const Vector3f&p,float r=0,int=0,bool=false,float s=1){target=p;radius=r;input=s;}')
    source = source.replace('bool CanWalkTo(const Vector3f&p,NavFlags=0){',
                            'int walkChecks=0;bool blockUseWalk=false;bool CanWalkTo(const Vector3f&p,NavFlags=0){++walkChecks;if(blockUseWalk)return false;')
    source = source.replace('Vector3f Flatten()const',
        'Vector3f&operator*=(float s){x*=s;y*=s;z*=s;return*this;}Vector3f Flatten()const')
    source = source.replace('static int GetFrameNumber(){return time/50;}',
        'static int GetFrameNumber(){return time/50;}static float GetDeltaTimeSecs(){return .05f;}')
    marker = 'namespace InterfaceFuncs{obUserData GetEntityStat(GameEntity e,const char*n){'
    assert marker in source
    source = source.replace(marker, r'''
Client*rayActor=nullptr;bool permitIdentity=true;int rayChecks=0,rayDoor=378;
Vector3f useOrigin(-944.4844f,-1364.125f,22.25f),savedAim(-968.25f,-1347.3906f,74.25f);
float leafPlane=-1342.125f;
float directRayFraction(){
 Vector3f eye=rayActor->GetEyePosition(),direction=savedAim-eye;direction.Normalize();
 return (leafPlane-eye.y)/(96.f*direction.y);
}
namespace InterfaceFuncs{obUserData GetEntityStat(GameEntity e,const char*n){
 if(rayActor&&String(n)=="tce_use_identity"){
  ++rayChecks;const float fraction=directRayFraction();
  return{permitIdentity&&fraction>=0&&fraction<=.4f?rayDoor:-1,true};
 }
''')
    steering = body((COMMON/'BotSteeringSystem.cpp').read_text(), 'void SteeringSystem::UpdateSteering()')
    steering = steering.replace('SteeringSystem::', 'VerifySteering::')
    return source + r'''
enum{BOT_DEBUG_MOVEVEC=123,ENT_FLAG_ON_ICE=456};
namespace Utils{void DrawLine(const Vector3f&,const Vector3f&,int,float){}}
class VerifySteering{public:
 enum{Normal,Arrive};Client*client;bool m_bMoveEnabled=true,m_TargetVector3d=false;
 int m_MoveType=Arrive;Vector3f m_TargetVector,m_MoveVec;float m_DistanceToTarget=0,m_TargetRadius,m_InputScale;
 Client*GetClient(){return client;}void UpdateSteering();
 VerifySteering(Client*c):client(c),m_TargetVector(c->steer.target),m_TargetRadius(c->steer.radius),m_InputScale(c->steer.input){}
};
''' + steering + r'''
void state378(int s){
 stats[{rayDoor,"tce_nav_model"}]=48;stats[{rayDoor,"tce_nav_use_kind"}]=2;
 stats[{rayDoor,"tce_use_door:"+std::to_string(rayDoor)+":48"}]=1;stats[{rayDoor,"tce_door_locked"}]=0;
 stats[{rayDoor,"tce_door_team"}]=3;stats[{rayDoor,"tce_door_state"}]=s;
 stats[{rayDoor,"tce_door_open_remaining"}]=-1;stats[{rayDoor,"tce_door_force"}]=0;
 stats[{1,"tce_door_speed:0:0"}]=174;
}
void recipe378(Waypoint&p,int mode,int role,int target,int use,int entry){
 p.flags=F_NAV_DOOR;p.props.p={{"tce_door_entity",std::to_string(rayDoor)},{"tce_door_use_entity",std::to_string(rayDoor)},
 {"tce_door_target",std::to_string(target)},{"tce_door_mode",std::to_string(mode)},
 {"tce_door_team","3"},{"tce_door_role",std::to_string(role)},{"tce_door_model","48"},
 {"tce_door_use_model","48"},{"tce_door_use_nav",std::to_string(use)},
 {"tce_door_entry",std::to_string(entry)},{"tce_door_aim",rayDoor==378?"-968.25 -1347.3906 74.25":"-968.25 -892.6094 74.25"}};
}
int main(){
 static FollowPath actors[12];User owners[12];
 for(int door: {378,379}){
 rayDoor=door;
 useOrigin=door==378?Vector3f(-944.4844f,-1364.125f,22.25f):Vector3f(-944.4063f,-875.875f,22.25f);
 savedAim=door==378?Vector3f(-968.25f,-1347.3906f,74.25f):Vector3f(-968.25f,-892.6094f,74.25f);
 leafPlane=door==378?-1342.125f:-897.875f;
 for(int mode: {1,3}){
  for(int scenario=0;scenario<3;++scenario){
   const int index=(door==378?0:6)+(mode==1?0:3)+scenario;FollowPath&f=actors[index];User&owner=owners[index];
   Waypoint use{760,useOrigin},entry{757,mode==1?useOrigin:Vector3f(-992.4844f,-1364.125f,22.25f)},
    exit{759,{-992.4844f,-1317.875f,30.25f}},goal{800,{-1100,-1300,30.25f}};
   const int useId=mode==1?757:760;
   recipe378(entry,mode,2,759,useId,757);recipe378(exit,mode,3,0,useId,757);
   recipe378(use,mode,1,757,useId,757);
   use.m_Connections={{&entry}};entry.m_Connections={{&use},{&exit}};
   planner.m_WaypointList={&use,&entry,&exit,&goal};planner.m_Client=&f.client;
   planner.m_Solution={&goal,&exit,&entry};if(mode==3)planner.m_Solution.push_back(&use);
   planner.GetPath(f.m_CurrentPath,0);f.m_Query.m_User=&owner;
   f.client.position=door==378?Vector3f(-943.983f,-1363.654f,22.125f):Vector3f(-944.317f,-875.797f,22.125f);f.client.passable=true;
   rayActor=&f.client;permitIdentity=true;state378(0);sweepObservation=1;
   const float observedFraction=directRayFraction();
   assert(observedFraction>.4f&&observedFraction<.405f);
   for(int i=0;i<4&&f.m_TceDoorPhase!=2;++i)frame(f);
   frame(f);
   assert(f.m_TceDoorPhase==2&&!(f.client.buttons&BOT_BUTTON_USE));
   assert(f.client.steer.radius==0.f&&f.client.steer.input<.125f);
   assert(SquaredLength2d(f.client.steer.target,useOrigin)<.0001f);
   VerifySteering actual(&f.client);actual.UpdateSteering();
   assert(actual.m_MoveVec.SquaredLength()>0); // Execute the real steering dead-zone/scale code.
   if(door==379){
    assert(actual.m_DistanceToTarget<.125f&&actual.m_DistanceToTarget>.11f);
    VerifySteering former(&f.client);former.m_TargetRadius=.125f;former.UpdateSteering();
    assert(former.m_MoveVec.SquaredLength()==0); // Reproduces the actual former no-input failure.
    Vector3f forward(-.819f,-.574f,0);forward.Normalize();Vector3f right(-forward.y,forward.x,0);
    const int cmdForward=(signed char)(127.f*actual.m_MoveVec.Dot(forward));
    const int cmdRight=(signed char)(127.f*actual.m_MoveVec.Dot(right));
    assert(cmdForward!=0||cmdRight!=0); // The real input quantization still produces movement.
   }
   const int checks=f.client.walkChecks;frame(f);
   assert(!(f.client.buttons&BOT_BUTTON_USE)&&f.client.walkChecks==checks+1&&f.failures==0);
   if(scenario==2){
    f.client.blockUseWalk=true;frame(f);
    assert(f.failures==1&&!(f.client.buttons&BOT_BUTTON_USE)&&owner.successes==0);
    continue;
   }
   f.client.position=useOrigin.AddZ(-.125f);assert(directRayFraction()<.4f); // Actual ground height, not a fabricated actor Z.
   if(scenario==1){
    // Correct centre cannot authorize an unrelated/ineligible identity.
    permitIdentity=false;const int began=f.m_TceDoorPhaseStart;
    while(f.failures==0&&IGame::time-began<=8200){frame(f);assert(!(f.client.buttons&BOT_BUTTON_USE));}
    assert(f.failures==1&&owner.successes==0&&IGame::time-began<=8100);
    continue;
   }
   frame(f);assert((f.client.buttons&BOT_BUTTON_USE)&&f.failures==0);
   state378(1);frame(f);assert(!(f.client.buttons&BOT_BUTTON_USE)&&f.m_TceDoorPhase==2);
   state378(2);frame(f);
   if(mode==3){
    assert(f.m_TceDoorPhase==3);
    for(int i=0;i<8&&f.m_TceDoorPhase==3;++i){arriveCurrent(f);frame(f);}
   }
   assert(f.m_TceDoorPhase==4);
   for(int i=0;i<12&&owner.successes==0;++i){arriveCurrent(f);frame(f);}
   assert(owner.successes==1&&f.failures==0&&f.m_PathStatus==FollowPath::PathFinished);
  }
 }
 }
 printf("actual Update + Steering modes1/3: Northport378/379 strict eye rays, real .118u dead-zone failure and nonzero quantized input, complete passages, immutable identity and eight-second failure bound passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class DoorUseCentreUpdateTests(TestCase):
    def test_complete_update_reaches_checked_use_before_strict_identity_can_authorize_activation(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
