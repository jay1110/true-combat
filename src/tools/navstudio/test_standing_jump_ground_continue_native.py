"""Complete real Update and Client walk after the observed Delta touchdown.

Independent actor samples and analytic collision planes exercise admission,
per-frame capsule support, hazards, action bounds and unchanged final arrival.
These regressions do not simulate PMove or certify the live route.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless

from test_standing_jump_runtime_native import program as standing_program
from test_door_runtime_native import COMMON, compile_run
from test_runtime_walk_native import CLIENT, ROOT, extract


def program():
    source = standing_program().split('int main(){', 1)[0]
    source = source.replace('Vector3f(float a,float b,float c)',
                            'Vector3f(const float*p):x(p[0]),y(p[1]),z(p[2]){}Vector3f(float a,float b,float c)')
    old_walk = ('bool CanWalkTo(const Vector3f&p,NavFlags=0){if(approachBlocked&&p.x==0)return false;'
                'if(egressBlocked&&p.x==256)return false;return p.x<100||p.y>100||passable;}')
    assert old_walk in source
    source = source.replace(old_walk, '''
 bool CanWalkTo(const Vector3f&,NavFlags=0)const;
 bool TraceWalkTo(const Vector3f&,NavFlags,float,Vector3f*)const;
 float GetStepHeight()const{return 8;}
 ''')
    source = source.replace('bool ResolveWalkDestination(const Vector3f&p,float,NavFlags f,Vector3f&out){out=p;return CanWalkTo(p,f);}',
                            'bool ResolveWalkDestination(const Vector3f&,float,NavFlags,Vector3f&)const;')
    source = source.replace('bool GetNavigationHull(AABB&out,NavFlags){out=AABB{};return true;}',
                            'bool GetNavigationHull(AABB&out,NavFlags)const{out=AABB{};out.m_Mins[2]=-24;out.m_Maxs[2]=46;return true;}')
    source = source.replace('struct Client {', '''
 struct obTraceResult{float m_Fraction=1,m_Normal[3]={0,0,1},m_Endpos[3]={};int m_StartSolid=0,m_Surface=0;};
 enum{CONT_SLIME=1,CONT_LAVA=2,CONT_WATER=4,SURFACE_SLICK=1,
 TR_MASK_SOLID=2,TR_MASK_PLAYERCLIP=4,TR_MASK_NAVCAPSULE=8,BOT_DEBUG_PLANNER=2,False=0};
 int scenario=0,queries=0,bodyqueries=0,boxqueries=0;
 namespace EngineFuncs{bool TraceLine(obTraceResult&,const Vector3f&,const Vector3f&,const AABB*,int,int,int);}
 struct Client {''')
    source = source.replace('struct Engine{GameEntity EntityFromID', '''
 struct Engine{int GetPointContents(const Vector3f&){return scenario==4?CONT_LAVA:scenario==5?CONT_WATER:0;}
 GameEntity EntityFromID''')
    client = CLIENT.read_text()
    helpers = '\n'.join(extract(client, name) for name in
                        ('NavigationGroundContentsSafe', 'NavigationGroundSurfaceSafe',
                         'TraceWalkTo', 'CanWalkTo', 'ResolveWalkDestination'))
    trace = r'''
namespace EngineFuncs {
bool TraceLine(obTraceResult&r,const Vector3f&a,const Vector3f&b,const AABB*h,int mask,int,int){
 ++queries;const bool capsule=(mask&TR_MASK_NAVCAPSULE)!=0;
 if(capsule)++bodyqueries;else ++boxqueries;
 r=obTraceResult{};
 const Vector3f n{-.0556942f,-.0795398f,.9952746f};
 // Same gentle normal and rounded-body/flat-foot anchor difference as the
 // real hood. The last .125u remains PMove's resting separation, not a snap.
 const Vector3f hoodContact{2100.9531f,1970.2024f,37.1101f};
 float d=hoodContact.Dot(n)+(capsule?0.f:1.8358f*n.z);
 auto clip=[&](const Vector3f&normal,float plane){
  const float da=a.Dot(normal)-plane,db=b.Dot(normal)-plane;
  if(da<-.0005f){r.m_StartSolid=1;r.m_Fraction=0;}
  if(db<0&&db<da){const float t=da/(da-db);if(t<r.m_Fraction){
   r.m_Fraction=std::fmax(0.f,t);for(int j=0;j<3;j++)r.m_Normal[j]=(&normal.x)[j];}}
 };
 // Missing actual capsule support must reject even when the broad foot box
 // still proposes its valid-looking anchor over that section.
 if(!(scenario==2&&capsule&&b.x>=2090&&b.x<=2096))clip(n,d);
 clip({0,0,-1},-((scenario==3?80.f:100.f)-h->m_Maxs[2]));
 if(scenario==1)clip({-1,0,0},-2089.f);
 if(scenario==6)r.m_Surface=SURFACE_SLICK;
 for(int j=0;j<3;j++)r.m_Endpos[j]=(&a.x)[j]+r.m_Fraction*((&b.x)[j]-(&a.x)[j]);
 return true;
}
}
'''
    return source + (ROOT / 'game/tce_nav_water.h').read_text() + trace + helpers + r'''
int main(){
 Waypoint launch{519,{2063.4714f,1940.2170f,4.25f}},hood{518,{2100.9531f,1970.2024f,37.2351f}};
 launch.props.p={{"tce_stand_jump","1"},{"tce_stand_jump_target","518"}};
 launch.flags=F_NAV_JUMP;launch.radius=hood.radius=2;planner.m_WaypointList={&launch,&hood};
 auto setup=[&](FollowPath&f,User&u){
  f.client.passable=true;planner.m_Client=&f.client;planner.m_Solution={&hood,&launch};
  planner.GetPath(f.m_CurrentPath,0);f.m_Query.m_User=&u;
  f.m_TceStandNavId=519;f.m_TceStandPhase=2;f.m_TceStandAttempt=IGame::time;
  f.client.position={2064.572f,1941.062f,16.377f};f.client.velocity={31,25,225};f.client.onGround=false;
  frame(f);assert(f.m_CurrentPath.GetCurrentPtIndex()==1&&u.successes==0&&f.failures==0);
 };
 const Vector3f touch{2083.327f,1956.183f,35.125f};
 FollowPath f;User u;setup(f,u);
 IGame::time+=400;f.client.position=touch;f.client.velocity={63,50,0};f.client.onGround=true;
 scenario=0;queries=bodyqueries=boxqueries=0;frame(f);
 assert(f.m_TceStandPhase==3&&f.failures==0&&u.successes==0);
 assert(bodyqueries>0&&queries<100&&!(f.client.buttons&BOT_BUTTON_TCE_STAND_LAUNCH));
 assert(!(f.client.buttons&BOT_BUTTON_JUMP));
 assert((f.client.steer.target-hood.pos).SquaredLength()<.00001f);
 // Every later grounded observation re-proves the rest of the full-body
 // walk, even after the target's Z tolerance has already become satisfied.
 for(const Vector3f p:{Vector3f{2085.532f,1957.933f,35.267f},
                      Vector3f{2090.241f,1961.661f,35.828f},
                      Vector3f{2097.136f,1967.133f,36.651f}}){
  const int before=bodyqueries;f.client.position=p;f.client.velocity={63,50,8};frame(f);
  assert(f.failures==0&&u.successes==0&&bodyqueries>before);
  assert(f.m_CurrentPath.GetCurrentPtIndex()==1);
 }
 // Exact final authored radius and actual contact, not the first touchdown,
 // finish once. An actor still outside that radius must remain in motion.
 f.client.position={2100.780f,1970.037f,37.087f};f.client.velocity={73,58,9};frame(f);
 assert(f.failures==0&&u.successes==1);
 int continued=0,landed=0;for(const auto&s:logs){
  continued+=s.find("TCE2STANDJUMP GROUND_CONTINUE ")!=String::npos;
  landed+=s.find("TCE2STANDJUMP LAND ")!=String::npos;}
 assert(continued==1&&landed==1);
 // The real walk consumer rejects actual wall, missing body support, head
 // clearance, lava, swimming and slippery slope independently. No early LAND.
 for(int bad=1;bad<=6;++bad){
  FollowPath blocked;User owner;scenario=0;setup(blocked,owner);
  IGame::time+=400;blocked.client.position=touch;blocked.client.onGround=true;
  scenario=bad;queries=bodyqueries=boxqueries=0;frame(blocked);
  assert(blocked.failures==1&&owner.successes==0&&bodyqueries>0);
 }
 scenario=0;
 // A later dynamic wall invalidates a previously permitted continuation,
 // including a still-not-arrived contact already within2u of the target Z.
 FollowPath dynamic;User dynamicOwner;setup(dynamic,dynamicOwner);
 IGame::time+=400;dynamic.client.position=touch;dynamic.client.onGround=true;frame(dynamic);
 assert(dynamic.m_TceStandPhase==3&&dynamic.failures==0);
 dynamic.client.position={2085.532f,1957.933f,35.267f};scenario=1;frame(dynamic);
 assert(dynamic.failures==1&&dynamicOwner.successes==0);scenario=0;
 // No inherited/stale phase, ground at the original floor, contact outside
 // the proved shaft, unbounded action duration or re-jump after contact.
 for(int bad=0;bad<7;++bad){
  FollowPath rejected;User owner;setup(rejected,owner);IGame::time+=400;
  rejected.client.position=touch;rejected.client.onGround=true;
  if(bad==0)rejected.m_TceStandNavId=999;
  if(bad==1)rejected.client.position.z=4.125f;
  if(bad==2)rejected.client.position.y-=20;
  if(bad==3)IGame::time+=2000;
  if(bad==4){frame(rejected);assert(rejected.m_TceStandPhase==3);rejected.client.onGround=false;}
  if(bad==5){rejected.client.position=hood.pos;IGame::time+=2000;}
  if(bad==6)rejected.client.position.z+=10; // False ONGROUND lacks current body support.
  frame(rejected);assert(rejected.failures==1&&owner.successes==0);
 }
 printf("real full Update + Client capsule walk: observed bumper continuation, per-frame reproof, final contact and14 unsafe/stale/timeout cases passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class StandingJumpGroundContinueTests(TestCase):
    def test_actual_update_continues_only_over_fresh_supported_body_walk(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
