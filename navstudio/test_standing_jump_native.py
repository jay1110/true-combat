"""Run the actual C standing-jump integrator against independent geometry.

These plane/brush sweep fixtures exercise production physics and body-query
paths. They are not an ET collision implementation or live jump certification.
No game instance is started and no fabricated Pmove/client state is invoked.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
try:
    from .test_traversal_reset_native import function
except ImportError:
    from test_traversal_reset_native import function


ROOT = (Path(__file__).resolve().parents[1] / "src")
SHIM = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
typedef int qboolean;
typedef float vec3_t[3];
enum {qfalse=0,qtrue=1,MAX_GENTITIES=1024,ENTITYNUM_NONE=1023,
 ENTITYNUM_WORLD=1022,CONTENTS_SOLID=1,CONTENTS_PLAYERCLIP=2,
 CONTENTS_WATER=4,CONTENTS_LAVA=8,CONTENTS_SLIME=16,SURF_SLICK=1};
typedef struct {float normal[3];} plane_t;
typedef struct {qboolean startsolid,allsolid;float fraction;int entityNum,
 surfaceFlags,contents;vec3_t endpos;plane_t plane;} trace_t;
struct {int integer;float value;} g_newbbox,g_developer,g_gravity;
typedef struct {int inuse;const char *classname;} gentity_t;
static gentity_t g_entities[MAX_GENTITIES];
static int scenario,body_calls,foot_calls,point_calls;
static float target_height=32.f,platform_edge=32.f;
static float water_depth;
#define VectorCopy(a,b) memcpy((b),(a),sizeof(vec3_t))
#define VectorSet(a,x,y,z) do{(a)[0]=(x);(a)[1]=(y);(a)[2]=(z);}while(0)
#define VectorClear(a) memset((a),0,sizeof(vec3_t))
#define VectorCompare(a,b) ((a)[0]==(b)[0]&&(a)[1]==(b)[1]&&(a)[2]==(b)[2])
#define VectorSubtract(a,b,c) do{int vi;for(vi=0;vi<3;vi++)(c)[vi]=(a)[vi]-(b)[vi];}while(0)
#define VectorScale(a,s,c) do{int vi;float vs=(s);for(vi=0;vi<3;vi++)(c)[vi]=(a)[vi]*vs;}while(0)
#define VectorMA(a,s,b,c) do{int vi;float vs=(s);for(vi=0;vi<3;vi++)(c)[vi]=(a)[vi]+vs*(b)[vi];}while(0)
#define DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
static float VectorLength(const vec3_t v){return sqrtf(DotProduct(v,v));}
static float VectorNormalize(vec3_t v){float l=VectorLength(v);if(l){int i;for(i=0;i<3;i++)v[i]/=l;}return l;}
#define Q_stricmp _stricmp
static void G_Printf(const char *format,...){va_list args;va_start(args,format);vprintf(format,args);va_end(args);}
static int TCE_NavGroundKind(int entity){return entity==ENTITYNUM_WORLD?1:0;}
static int trap_PointContents(const vec3_t p,int entity){(void)entity;return (scenario==4&&p[0]>8)||(scenario==6&&p[2]<water_depth)?CONTENTS_WATER:0;}
static int trap_EntitiesInBox(const vec3_t mins,const vec3_t maxs,int *entities,int size){
 (void)maxs;if(scenario==3&&size&&mins[0]<80&&maxs[0]>24){entities[0]=100;return 1;}return 0;
}
// Independent horizontal floor and vertical wall/ceiling brush planes.
// All sweep boundaries use the supplied full hull; no integrator decisions
// or accepted trajectory are mirrored by this collision fixture.
static float support(float x,float halfwidth){
 if(x+halfwidth>=platform_edge)return target_height;
 if(scenario==5&&x>-32&&x<20)return -1000.f;
 return 0.f;
}
static void fixture_trace(trace_t *tr,const vec3_t a,const vec3_t mins,
 const vec3_t maxs,const vec3_t b,int capsule){
 const float width=mins?fabsf(mins[0]):0,height=maxs?maxs[2]:0;
 const float low=mins?mins[2]:0,ceiling=scenario==2?96.f:10000.f;
 float previous=0.f;int sample,hit=0;vec3_t p,normal;
 if(capsule)body_calls++;else if(mins)foot_calls++;else point_calls++;
 memset(tr,0,sizeof(*tr));tr->fraction=1.f;tr->entityNum=ENTITYNUM_NONE;
 VectorCopy(b,tr->endpos);VectorSet(tr->plane.normal,0,0,1);
 if(a[2]+low<support(a[0],width)-.001f||a[2]+height>ceiling+.001f||
    (scenario==1&&a[0]+width>28.f+.001f)){
  tr->startsolid=tr->allsolid=1;tr->fraction=0.f;VectorCopy(a,tr->endpos);return;
 }
 // Sampling only locates a bracket for the fixture's plane crossing, then
 // bisection computes its time. Production controls its own dt and velocity.
 for(sample=1;sample<=32;sample++){
  float t=(float)sample/32;int i;for(i=0;i<3;i++)p[i]=a[i]+t*(b[i]-a[i]);
  if(p[2]+low<support(p[0],width)-.000001f){hit=1;VectorSet(normal,0,0,1);}
  else if(p[2]+height>ceiling+.000001f){hit=2;VectorSet(normal,0,0,-1);}
  else if(scenario==1&&p[0]+width>28.f+.000001f){hit=3;VectorSet(normal,-1,0,0);}
  if(hit){
   float lower=previous,upper=t;int iteration;
   for(iteration=0;iteration<24;iteration++){
    float m=(lower+upper)*.5f;for(i=0;i<3;i++)p[i]=a[i]+m*(b[i]-a[i]);
    if((hit==1&&p[2]+low<support(p[0],width))||
       (hit==2&&p[2]+height>ceiling)||(hit==3&&p[0]+width>28))upper=m;else lower=m;
   }
   tr->fraction=lower;tr->entityNum=ENTITYNUM_WORLD;VectorCopy(normal,tr->plane.normal);
   for(i=0;i<3;i++)tr->endpos[i]=a[i]+lower*(b[i]-a[i]);return;
  }
  previous=t;
 }
}
static void trap_Trace(trace_t *tr,const vec3_t a,const vec3_t mins,const vec3_t maxs,
 const vec3_t b,int entity,int mask){(void)entity;(void)mask;fixture_trace(tr,a,mins,maxs,b,0);}
static void trap_TraceCapsule(trace_t *tr,const vec3_t a,const vec3_t mins,const vec3_t maxs,
 const vec3_t b,int entity,int mask){(void)entity;(void)mask;fixture_trace(tr,a,mins,maxs,b,1);}
static void require(int good,const char *name){if(!good){fprintf(stderr,"STANDING REGRESSION: %s\n",name);exit(9);}}
#define REQUIRE(good,name) require((good),(name))
'''


def program():
    core = (ROOT / 'game/tce_navgen.c').read_text()
    traversal = (ROOT / 'game/tce_navgen_traversal.inc').read_text()
    source = core + '\n' + traversal
    names = ('NG_Trace', 'NG_Hurt', 'NG_StaticGround', 'NG_ProbeBodyRepresentations',
             'NG_OrdinarySurface', 'NG_Ground', 'NG_Walk', 'NG_TrClear',
             'NG_TrGravity', 'NG_TrStableLanding', 'NG_TrGentlePlaneSupport', 'NG_TrPhysicalGroundAlias',
             'NG_TrStandingLanding', 'NG_TrStandingPhysicalWalk', 'NG_TrCentreSupported',
             'NG_TrExitContinuation', 'NG_TrStandingExitContinuation', 'NG_TrDropGroundClip', 'NG_TrStandSpeed', 'NG_TrStandingJump', 'NG_TrCommitManualStandingJump',
             'NG_TrJumpImpulse', 'NG_TrGroundedLaunch', 'NG_TrJumpArc', 'NG_TrJump', 'NG_TrProbeJumps')
    functions = [function(source, name) for name in names]
    needed = set(re.findall(r'\bng\w+\b', '\n'.join(functions))) | {'ngDropSpeeds', 'ngTraceBudget'}
    declarations = [line for line in source.splitlines()
                    if re.match(r'^static [^;()]+;', line)
                    and needed.intersection(re.findall(r'\bng\w+\b', line))]
    structs = re.findall(r'typedef struct\s*\{[^}]+\}\s*\w+;', source, re.S)
    structures = [text for text in structs if text.rsplit('}', 1)[1].strip(' ;') in ('ngDoorRecipe', 'ngNode', 'ngEdge', 'ngJumpProof')]
    defines = '\n'.join(line for line in source.splitlines() if re.match(r'^#define NG_\w+\s', line))
    header = (ROOT / 'game/tce_nav_jump.h').read_text()
    header += (ROOT / 'game/tce_nav_water.h').read_text()
    return SHIM + defines + '\n' + header + '\n' + '\n'.join(structures + declarations + functions) + r'''
int main(int argc,char **argv){
 ngNode a,b;ngJumpProof proof;int positive;
 scenario=argc>1?atoi(argv[1]):0;
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
 g_newbbox.integer=1;ngWidth=16;ngStand=87;ngDuck=61;ngOrigin=30;
 g_gravity.value=800;g_developer.integer=0;ngTraceBudget=2000000;
 VectorSet(a.feet,0,0,.125f);VectorSet(b.feet,64,0,32.125f);
 a.accepted=b.accepted=qtrue;a.id=1;b.id=2;
 g_entities[100].inuse=1;g_entities[100].classname="trigger_hurt";
 if(scenario==7){
  scenario=2;ngProbe=qtrue;ngTraces=17;ngStepDiagnostics=2;ngJumpArcFailure=6;
  VectorSet(ngJumpArcFailurePoint,9,8,7);ngConditionalGroundHits[100]=19;
  NG_TrProbeJumps(&a,&b,2000000);
  REQUIRE(ngProbe&&ngTraces==17&&ngStepDiagnostics==2&&ngJumpArcFailure==6,"quiet diagnostic restores all counters and probe state");
  REQUIRE(ngJumpArcFailurePoint[0]==9&&ngJumpArcFailurePoint[1]==8&&ngJumpArcFailurePoint[2]==7&&ngConditionalGroundHits[100]==19,"quiet diagnostic restores points and conditional hits");
  printf("scenario7 real quiet probe emitted bounded failed contact\n");return 0;
 }
 if(scenario==6){
  vec3_t floor;
  water_depth=32;
  REQUIRE(!NG_Ground(a.feet,floor,87.f),"enlarged standing waterlevel1 at depth32");
  REQUIRE(NG_Ground(a.feet,floor,61.f)!=NULL,"enlarged crouched swimming at depth32");
  water_depth=42;REQUIRE(NG_Ground(a.feet,floor,87.f)!=NULL,"enlarged standing swimming at depth42");
  ngOrigin=24;ngWidth=14;ngStand=70;ngDuck=49;g_newbbox.integer=0;
  water_depth=32;REQUIRE(!NG_Ground(a.feet,floor,70.f),"normal standing waterlevel1 at depth32");
  water_depth=23;REQUIRE(NG_Ground(a.feet,floor,49.f)!=NULL,"normal crouched swimming at depth23");
  water_depth=20;REQUIRE(!NG_Ground(a.feet,floor,49.f),"normal crouched waterlevel1 at depth20");
  printf("scenario 6 actual NG_Ground water posture gates passed\n");return 0;
 }
 REQUIRE(TCE_NavStandingSpeed(36,0,0)&&TCE_NavStandingSpeed(40,0,0)&&TCE_NavStandingSpeed(44,0,0),"allowed primed speeds");
 REQUIRE(!TCE_NavStandingSpeed(35,0,0)&&!TCE_NavStandingSpeed(45,0,0)&&!TCE_NavStandingSpeed(40,0,40),"total launch-speed gate");
 REQUIRE(fabs(TCE_NavStandingImpulse(0)-270)<.001f&&fabs(TCE_NavStandingImpulse(1)-337.5f)<.001f,"actual standing impulse scale");
 positive=NG_TrStandingJump(&a,&b,2000000,&proof);
 printf("scenario %d accepted %d failure %d traces %d body %d foot %d point %d\n",scenario,positive,ngJumpArcFailure,ngTraces,body_calls,foot_calls,point_calls);
 if(!scenario){
  REQUIRE(positive,"32-unit platform standing jump");
  REQUIRE(proof.standing&&proof.duration>0&&proof.duration<1.44f,"standing receipt");
  REQUIRE(proof.crash<=38.f&&proof.impact>0,"safe tested impact");
  REQUIRE(body_calls>100&&point_calls>9&&foot_calls>0,"real production body/foot/landing paths");
  REQUIRE(!NG_TrJump(&a,&b),"running impulse cannot certify the 32-unit ascent");
  {ngEdge edge,originalEdge;ngNode originalNodes[2];int i;
   ngCount=2;ngNodes[0]=a;ngNodes[1]=b;memcpy(originalNodes,ngNodes,sizeof(originalNodes));
   memset(&edge,0,sizeof(edge));edge.from=0;edge.to=1;originalEdge=edge;
   REQUIRE(NG_TrCommitManualStandingJump(&edge,&proof),"authored standing receipt");
   REQUIRE(ngJumpProofCount==1&&ngJumpProofs[0].standing&&ngJumpProofs[0].launch==0&&
    ngJumpProofs[0].landing==1&&ngJumpProofs[0].duration==proof.duration,"exact production receipt retained");
   REQUIRE(NG_TrCommitManualStandingJump(&edge,&proof)&&ngJumpProofCount==1,"same directed receipt reused");
   edge.to=0;REQUIRE(!NG_TrCommitManualStandingJump(&edge,&proof)&&ngJumpProofCount==1,"conflicting target retained");edge=originalEdge;
   ngJumpProofCount=64;for(i=0;i<64;i++)ngJumpProofs[i].launch=i+2;
   REQUIRE(!NG_TrCommitManualStandingJump(&edge,&proof)&&ngJumpProofCount==64,"bounded receipt array fails explicitly");
   REQUIRE(!memcmp(ngNodes,originalNodes,sizeof(originalNodes))&&!memcmp(&edge,&originalEdge,sizeof(edge)),"receipt bookkeeping leaves graph exact");
  }
 }else REQUIRE(!positive,"obstructed/hazardous fixture must reject");
 return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class StandingJumpNativeTests(TestCase):
    def test_actual_integrator_platform_and_negative_body_support_hazards(self):
        with tempfile.TemporaryDirectory(prefix='tce-stand-regression-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\n'
                'project(StandingJumpRegression LANGUAGES C)\nadd_executable(standing-jump main.c)\n')
            options = dict(capture_output=True, text=True, timeout=90,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            executable = directory / 'build/Release/standing-jump.exe'
            for number, name in enumerate(('clear platform32', 'wall', 'low ceiling', 'hurt', 'water', 'no launch ground', 'water postures', 'quiet failure contact')):
                with self.subTest(fixture=name):
                    result = subprocess.run([str(executable), str(number)], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    if number==7:
                        self.assertEqual(result.stdout.count('"part":"standing"'), 1)
                        self.assertIn('"phase":"air"', result.stdout)
                        self.assertIn('"normal":[0.0000000,0.0000000,-1.0000000]', result.stdout)
                        self.assertEqual(result.stdout.count('navgen probe jump '), 3)
                    elif number==6:self.assertIn('actual NG_Ground water posture gates passed',result.stdout)
                    else:self.assertIn(f'scenario {number} accepted {int(number == 0)}', result.stdout)


if __name__ == '__main__':
    main()
