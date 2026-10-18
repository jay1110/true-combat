"""Actual complete Update + Client walk for a world-space final goal volume.

Independent capsule/ground observations reproduce Northport's item centre z=4
above floor z=-8, while the player origin is z=22.125. These are offline
controller/collision regressions, not a live-map or pickup attribution claim.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_door_runtime_native import COMMON, compile_run
from test_door_update_native import program as update_program
from test_runtime_walk_native import extract


def program():
    source = update_program().split('int main(){', 1)[0]
    source = source.replace('Vector3f operator-(const Vector3f&b)const',
        'Vector3f(const float*p):x(p[0]),y(p[1]),z(p[2]){}Vector3f operator-(const Vector3f&b)const')
    old = 'bool CanWalkTo(const Vector3f&p,NavFlags=0){if(approachBlocked&&p.x==0)return false;if(egressBlocked&&p.x==256)return false;return p.x<100||p.y>100||passable;}'
    assert old in source
    source = source.replace(old, '''bool CanWalkTo(const Vector3f&,NavFlags=0)const;
 bool TraceWalkTo(const Vector3f&,NavFlags,float,Vector3f*)const;
 bool ResolveWalkDestination(const Vector3f&,float,NavFlags,Vector3f&)const;
 float GetStepHeight()const{return 8.f;}''')
    source = source.replace('bool ResolveWalkDestination(const Vector3f&p,float,NavFlags f,Vector3f&out){out=p;return CanWalkTo(p,f);}', '')
    source = source.replace('bool GetNavigationHull(AABB&out,NavFlags){', 'bool GetNavigationHull(AABB&out,NavFlags)const{')
    source = source.replace('struct Engine{GameEntity EntityFromID', 'struct Engine{int GetPointContents(const Vector3f&);GameEntity EntityFromID')
    source = source.replace('struct Steering {Vector3f target;', 'struct Steering {Vector3f target;float radius=0;')
    source = source.replace('void SetTarget(const Vector3f&p,float=0,int=0,bool=false,float=1){target=p;}',
        'void SetTarget(const Vector3f&p,float r=0,int=0,bool=false,float=1){target=p;radius=r;}')
    fixture = r'''
enum{CONT_SLIME=1,CONT_LAVA=2,CONT_WATER=4,SURFACE_SLICK=1,TR_MASK_SOLID=2,
 TR_MASK_PLAYERCLIP=4,TR_MASK_NAVCAPSULE=8,BOT_DEBUG_PLANNER=2,False=0};
struct obTraceResult{float m_Fraction=1,m_Normal[3]={0,0,1},m_Endpos[3]={};int m_StartSolid=0,m_Surface=0;};
int collisionScenario=0,bodyQueries=0;
int Engine::GetPointContents(const Vector3f&p){return collisionScenario==4?CONT_LAVA:collisionScenario==5?CONT_WATER:0;}
namespace EngineFuncs{
bool TraceLine(obTraceResult&r,const Vector3f&a,const Vector3f&b,const AABB*h,int mask,int ignored,int){
 assert(ignored==1);if(mask&TR_MASK_NAVCAPSULE)++bodyQueries;
 r=obTraceResult{};const float lo=h?h->m_Mins[2]:0,hi=h?h->m_Maxs[2]:0;
 const float floor=-8.f;
 if(a.z+lo<floor-.001f||(collisionScenario==6&&a.z+hi>70.f)){r.m_StartSolid=1;r.m_Fraction=0;}
 else if(b.z+lo<floor&&b.z<a.z&&collisionScenario!=3){
  const float t=(a.z+lo-floor)/(a.z-b.z),x=a.x+t*(b.x-a.x);
  if(!(collisionScenario==2&&x>-1970.f&&x<-1950.f))r.m_Fraction=t;
 }
 if(collisionScenario==1&&a.x<=-1960.f&&b.x>-1960.f){r.m_Fraction=(-1960.f-a.x)/(b.x-a.x);r.m_Normal[0]=-1;r.m_Normal[2]=0;}
 for(int i=0;i<3;++i)r.m_Endpos[i]=(&a.x)[i]+r.m_Fraction*((&b.x)[i]-(&a.x)[i]);
 return true;
}}
'''
    client = (COMMON/'Client.cpp').read_text()
    helpers = '\n'.join(extract(client, name) for name in ('NavigationGroundContentsSafe',
        'NavigationGroundSurfaceSafe', 'TraceWalkTo', 'CanWalkTo', 'ResolveWalkDestination'))
    water = (COMMON.parents[4]/'game/tce_nav_water.h').read_text()
    marker = 'State::StateStatus FollowPath::Update('
    assert marker in source
    source = source.replace(marker, fixture + water + helpers + '\n' + marker, 1)
    return source + r'''
void setup(FollowPath&f,User&u,float radius=32.f,bool volume=true){
 f.client.position={-1984.f,-1441.0663f,22.125f};f.client.debug=false;
 f.m_CurrentPath.AddPt({-1984.f,-1441.0663f,22.25f},35).PlayerOrigin().NavId(80);
 f.m_CurrentPath.AddPt({-1946.f,-1391.f,4.f},radius).PlayerOrigin().TceDestinationVolume(volume);
 f.m_Query.m_User=&u;
}
bool logContains(const char*s){for(const auto&line:logs)if(line.find(s)!=String::npos)return true;return false;}
int main(){
 static FollowPath good;User owner;setup(good,owner);
 assert(!good.client.CanWalkTo({-1946,-1391,4})); // Strict anchor contract is unchanged.
 frame(good);assert(good.failures==0&&good.m_CurrentPath.GetCurrentPtIndex()==1&&owner.successes==0);
 assert(logContains("DESTINATION_VOLUME")&&logContains("raw=(-1946.000 -1391.000 4.000) supported=(-1946.000 -1391.000 22.125)"));
 good.client.position={-1968,-1415,22.125f};frame(good);
 assert(good.failures==0&&owner.successes==0&&good.client.steer.target.z==22.125f);
 good.client.position={-1955,-1405,22.125f};frame(good);
 assert(owner.successes==1&&good.failures==0&&bodyQueries>0);
 assert(good.m_CurrentPath.GetPt(1)->m_Pt.z==4&&good.m_CurrentPath.GetPt(1)->m_Radius==32); // Never rewrite authored values.

 // A body obstruction, gap, vanished floor, lava, submerged floor and low
 // ceiling must all fail the very same full walk, despite the generous radius.
 for(int kind=1;kind<=6;++kind){
  collisionScenario=kind;static FollowPath blocked[6];User owners[6];auto&f=blocked[kind-1];setup(f,owners[kind-1]);
  for(int i=0;i<25&&f.failures==0;++i)frame(f);
  assert(f.failures==1&&owners[kind-1].successes==0&&f.m_CurrentPath.GetCurrentPtIndex()==0);
 }
 collisionScenario=0;
 static FollowPath small;User smallOwner;setup(small,smallOwner,8.f);
 for(int i=0;i<25&&small.failures==0;++i)frame(small);
 assert(small.failures==1&&smallOwner.successes==0); // Supported body is outside caller radius.

 static FollowPath ordinary;User ordinaryOwner;setup(ordinary,ordinaryOwner,32.f,false);
 for(int i=0;i<25&&ordinary.failures==0;++i)frame(ordinary);
 assert(ordinary.failures==1&&ordinaryOwner.successes==0); // No implicit reinterpretation of ordinary coordinates.

 static FollowPath lost;User lostOwner;setup(lost,lostOwner);frame(lost);
 lost.client.position={-1955,-1405,22.125f};collisionScenario=3;frame(lost);
 for(int i=0;i<15&&lost.failures==0;++i)frame(lost);
 assert(lost.failures==1&&lostOwner.successes==0&&logContains("unsupported_ground_timeout"));

 collisionScenario=0;static FollowPath edge;User edgeOwner;setup(edge,edgeOwner);frame(edge);
 edge.client.position={-1976,-1391,22.125f};frame(edge);
 assert(edgeOwner.successes==0); // 30XY is within32 but actual sphere includes18.125Z.
 edge.client.position={-1972.32f,-1391,22.25f};frame(edge);
 assert(edgeOwner.successes==0&&edge.client.steer.radius<26.32f); // Actual actor height must not create an outside-sphere stopping annulus.
 edge.client.position={-1970,-1391,22.125f};frame(edge);assert(edgeOwner.successes==1);
 Vector3f out;assert(!edge.client.ResolveWalkDestination({-1946,-1391,4},-1,0,out));
 assert(!edge.client.ResolveWalkDestination({-1946,-1391,4},NAN,0,out));
 assert(!edge.client.ResolveWalkDestination({-1946,-1391,4},INFINITY,0,out));
 printf("complete Update + real native walk: bounded item-centre volume, actual sphere arrival, unchanged anchors, wall/gap/floor/headroom/water/hazard rejection and fresh final proof passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class DestinationVolumeTests(TestCase):
    def test_actual_update_and_body_walk_resolve_only_supported_final_goal_volume(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
