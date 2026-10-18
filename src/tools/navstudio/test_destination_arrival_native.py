"""Actual Delta final-volume arrival with full current body/support proof."""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_door_runtime_native import COMMON, body, compile_run
from test_destination_volume_native import program as destination_program


def program():
    source = destination_program().rsplit('void setup(FollowPath&', 1)[0]
    source = source.replace('int collisionScenario=0,bodyQueries=0;',
                            'int collisionScenario=0,bodyQueries=0;float groundHeight=-22.f;')
    source = source.replace('const float floor=-8.f;', 'const float floor=groundHeight;')
    source = source.replace('collisionScenario==6&&a.z+hi>70.f',
                            'collisionScenario==6&&a.z+hi>60.f')
    source = source.replace('bool HasEntityFlag(int flag)const{return flag==ENT_FLAG_ONGROUND;}',
                            'bool grounded=true;bool HasEntityFlag(int flag)const{return flag==ENT_FLAG_ONGROUND&&grounded;}')
    # Use the actual game bridge's stationary-capsule wrapper. The independent
    # collision fixture reproduces PMove's .125u clip epsilon: downward .01u
    # contact at a resting pose is support, not a clear stationary query.
    bridge = (COMMON.parents[4]/'game/etbot/tce_nav_capsule_trace.h').read_text()
    original_trace = body(source, 'bool TraceLine(')
    declarations = r'''
using vec3_t=float[3];using qboolean=int;enum{qtrue=1,qfalse=0,ENTITYNUM_WORLD=1022};
struct trace_t{float fraction=1,endpos[3]={};int startsolid=0,allsolid=0,entityNum=1023,surfaceFlags=0;struct{float normal[3]={0,0,1};}plane;};
bool VectorCompare(const float*a,const float*b){return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2];}
void VectorCopy(const float*a,float*b){memcpy(b,a,3*sizeof(float));}
void trap_TraceCapsule(trace_t*r,const float*a,const float*lo,const float*hi,const float*b,int ignored,int mask){
 assert(ignored==1);*r=trace_t{};r->plane.normal[2]=1;
 if(a[2]+lo[2]<groundHeight-.001f||(collisionScenario==6&&a[2]+hi[2]>60.f)||collisionScenario==8){r->startsolid=r->allsolid=1;r->fraction=0;}
 else if(b[2]<a[2]&&b[2]+lo[2]<groundHeight+.125f&&collisionScenario!=3){
  r->fraction=std::max(0.f,(a[2]+lo[2]-groundHeight-.125f)/(a[2]-b[2]));r->entityNum=1022;
 }
 if(collisionScenario==7&&a[1]>=1790.f&&b[1]<1790.f){r->fraction=(a[1]-1790.f)/(a[1]-b[1]);r->plane.normal[1]=1;r->plane.normal[2]=0;}
 for(int i=0;i<3;++i)r->endpos[i]=a[i]+r->fraction*(b[i]-a[i]);
}
'''
    source = '#include <algorithm>\n' + source
    source = source.replace('namespace EngineFuncs{\nbool TraceLine',
                            declarations + bridge + '\nnamespace EngineFuncs{\nbool TraceLine', 1)
    replacement = r'''
bool TraceLine(obTraceResult&r,const Vector3f&a,const Vector3f&b,const AABB*h,int mask,int ignored,int){
 assert(h);if(mask&TR_MASK_NAVCAPSULE)++bodyQueries;
 trace_t t;TCE_NavCapsuleTrace(&t,&a.x,h->m_Mins,h->m_Maxs,&b.x,ignored,mask);
 r=obTraceResult{};r.m_Fraction=t.fraction;r.m_StartSolid=t.startsolid;
 for(int i=0;i<3;++i){r.m_Endpos[i]=t.endpos[i];r.m_Normal[i]=t.plane.normal[i];}
 return true;
}'''
    assert original_trace in source
    source = source.replace(original_trace, replacement)
    return source + r'''
void setupArrival(FollowPath&f,User&u,bool volume=true){
 f.client.debug=false;f.client.position={1039.404f,1803.948f,8.126f};f.m_Query.m_User=&u;
 f.m_CurrentPath.AddPt({1024,1773.62f,8.25f},35).PlayerOrigin().NavId(41);
 f.m_CurrentPath.AddPt({1024,1773.62f,8.25f},35).PlayerOrigin().TceDestinationVolume(volume);
}
bool logContains(const char*s){for(const auto&line:logs)if(line.find(s)!=String::npos)return true;return false;}
int main(){
 collisionScenario=7;static FollowPath inside;User owner;setupArrival(inside,owner);
 assert(!inside.client.CanWalkTo({1024,1773.62f,8.25f}));
 assert(!inside.client.CanWalkTo(inside.client.GetPosition())); // Real zero-sweep wrapper rejects floor contact.
 const int before=bodyQueries;frame(inside);assert(inside.failures==0&&inside.m_CurrentPath.GetCurrentPtIndex()==1);
 frame(inside);assert(owner.successes==1&&inside.failures==0&&bodyQueries>before);
 assert(inside.m_CurrentPath.GetPt(1)->m_Pt.y==1773.62f&&inside.m_CurrentPath.GetPt(1)->m_Radius==35);

 // Ordinary anchors still require their actual centre, and a position just
 // outside the authored sphere cannot use the new already-arrived case.
 static FollowPath ordinary;User ordinaryOwner;setupArrival(ordinary,ordinaryOwner,false);frame(ordinary);
 assert(ordinary.failures==1&&ordinaryOwner.successes==0);
 static FollowPath outside;User outsideOwner;setupArrival(outside,outsideOwner);outside.m_CurrentPath.NextPt();
 outside.client.position={1041,1806,8.125f};frame(outside);assert(outside.failures==1&&outsideOwner.successes==0);

 // Even inside the goal: no support, lava, deep water, insufficient headroom
 // or an actually occupied hull must all reject. No goal-success shortcut.
 for(int scenario:{3,4,5,6,8}){
  static FollowPath failed[9];User u[9];auto&f=failed[scenario];setupArrival(f,u[scenario]);
  f.m_CurrentPath.NextPt();collisionScenario=scenario;frame(f);
  for(int i=0;i<15&&f.failures==0;++i)frame(f);
  assert(f.failures==1&&u[scenario].successes==0);
  assert(logContains("unsupported_ground_timeout"));
 }
 collisionScenario=7;static FollowPath falling;User fallOwner;setupArrival(falling,fallOwner);
 falling.client.grounded=false;falling.m_CurrentPath.NextPt();frame(falling);
 assert(fallOwner.successes==0&&falling.m_TceAirWaitActive);
 static FollowPath falseGround;User falseOwner;setupArrival(falseGround,falseOwner);
 falseGround.client.position.z=15.125f;falseGround.m_CurrentPath.NextPt();frame(falseGround);
 for(int i=0;i<15&&falseGround.failures==0;++i)frame(falseGround);
 assert(falseGround.failures==1&&falseOwner.successes==0);
 printf("actual Delta sphere: fresh supported current-body arrival; unchanged ordinary centres/radius; outside, air, missing floor, water, lava, headroom and occupied-hull rejection passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class DestinationArrivalTests(TestCase):
    def test_original_final_volume_accepts_only_freshly_supported_actual_arrival(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
