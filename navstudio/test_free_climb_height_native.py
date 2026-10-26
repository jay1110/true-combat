"""Compare original measured ledge height with the later landing-floor rise.

The fixture has a64-unit front rim and60-unit floor behind it. Production
PM_ClimbSlideMove and native contact gates see the rim; endpoint selection
sees the final floor. No production physics gate is changed by this test.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
try:
    from .test_free_climb_native import program as free_program
    from .test_traversal_reset_native import function, ROOT
except ImportError:
    from test_free_climb_native import program as free_program
    from test_traversal_reset_native import function, ROOT


GEOMETRY = r'''
static float rim_height=64.f;
static void fixture_box(trace_t *tr,const vec3_t a,const vec3_t mins,const vec3_t maxs,const vec3_t b,
 float x0,float x1,float y0,float y1,float z0,float z1){
 float boxMin[3]={x0,y0,z0},boxMax[3]={x1,y1,z1},enter=0.f,leave=1.f;int axis,inside=1,normalAxis=-1,normalSign=0;
 for(axis=0;axis<3;axis++){
  float lower=boxMin[axis]-(maxs?maxs[axis]:0),upper=boxMax[axis]-(mins?mins[axis]:0);
  if(a[axis]<=lower+.001f||a[axis]>=upper-.001f)inside=0;
 }
 if(inside){tr->startsolid=tr->allsolid=1;tr->fraction=0;VectorCopy(a,tr->endpos);return;}
 for(axis=0;axis<3;axis++){
  float lower=boxMin[axis]-(maxs?maxs[axis]:0),upper=boxMax[axis]-(mins?mins[axis]:0),delta=b[axis]-a[axis],near,far;int sign;
  if(fabsf(delta)<.0000001f){if(a[axis]<=lower||a[axis]>=upper)return;continue;}
  if(delta>0){near=(lower-a[axis])/delta;far=(upper-a[axis])/delta;sign=-1;}
  else{near=(upper-a[axis])/delta;far=(lower-a[axis])/delta;sign=1;}
  if(near>=enter){enter=near;normalAxis=axis;normalSign=sign;}
  if(far<leave)leave=far;if(enter>=leave)return;
 }
 if(normalAxis>=0&&enter>=0&&enter<tr->fraction&&enter<=1){
  tr->fraction=enter;tr->entityNum=ENTITYNUM_WORLD;VectorClear(tr->plane.normal);tr->plane.normal[normalAxis]=(float)normalSign;
  for(axis=0;axis<3;axis++)tr->endpos[axis]=a[axis]+enter*(b[axis]-a[axis]);
 }
}
static void fixture_trace(trace_t *tr,const vec3_t a,const vec3_t mins,const vec3_t maxs,const vec3_t b,int capsule){
 if(capsule)body_calls++;else if(mins)foot_calls++;else point_calls++;
 memset(tr,0,sizeof(*tr));tr->fraction=1;tr->entityNum=ENTITYNUM_NONE;VectorCopy(b,tr->endpos);
 fixture_box(tr,a,mins,maxs,b,-10000,10000,-1000,1000,-10000,0);
 fixture_box(tr,a,mins,maxs,b,20,28,-1000,1000,0,rim_height);
 fixture_box(tr,a,mins,maxs,b,28,10000,-1000,1000,0,60);
}
'''


def program():
    source = (ROOT / 'game/tce_navgen_freeclimb.inc').read_text()
    move = (ROOT / 'game/bg_pmove.c').read_text()
    text = free_program().split('int main(int argc', 1)[0]
    start = text.index('static void fixture_trace(')
    end = text.index('static void trap_Trace(', start)
    text = text[:start] + GEOMETRY + text[end:]
    portable = move[move.index('static qboolean PM_ClimbSlideMove(int unused,'):]
    actual = function(portable, 'PM_ClimbSlideMove')
    candidate = function(source, 'NG_TrFreeClimb')
    assert 'direction[2]<=18.25f' in candidate
    candidate = candidate.replace('NG_TrFreeClimb(', 'NG_TrFreeClimbLegacyEndpoint(', 1)
    candidate = candidate.replace('direction[2]<=18.25f', 'direction[2]<63.f', 1)
    context = r'''
enum{STAT_TCE_FLAGS=9};
typedef struct{vec3_t origin,velocity;int stats[16],clientNum;} fixturePlayer;
typedef struct{int sprintTime;} fixturePmExt;
typedef struct{fixturePlayer *ps;fixturePmExt *pmext;vec3_t mins,maxs;int tracemask;
 void(*trace)(trace_t*,const vec3_t,const vec3_t,const vec3_t,const vec3_t,int,int);} fixturePmove;
static fixturePlayer player;static fixturePmExt extension;static fixturePmove movement,*pm=&movement;
'''
    return text + context + actual + '\n' + candidate + r'''
int main(void){
 ngNode from,to,departure;ngFreeClimbProof proof;ngDropProof drop;vec3_t contact,normal;int originalGate,nativeGate,ordinaryCandidate,relaxedEndpoint;
 g_newbbox.integer=1;ngWidth=16;ngStand=87;ngDuck=61;ngOrigin=30;g_gravity.value=800;ngTraceBudget=2000000;
 memset(&from,0,sizeof(from));memset(&to,0,sizeof(to));from.accepted=to.accepted=1;
 VectorSet(from.feet,3.75f,0,.125f);VectorSet(to.feet,64,0,60.125f);
 pm->ps=&player;pm->pmext=&extension;pm->trace=trap_TraceCapsule;extension.sprintTime=2500;
 VectorSet(pm->mins,-16,-16,-30);VectorSet(pm->maxs,16,16,57);player.stats[STAT_TCE_FLAGS]=0x200;
 VectorCopy(from.feet,contact);contact[2]+=18;VectorCopy(contact,player.origin);player.origin[2]+=30;player.velocity[2]=100;
 VectorSet(normal,-1,0,0);originalGate=PM_ClimbSlideMove(1,normal,0);
 normal[0]=1;nativeGate=NG_TrFreeContact(contact,normal,100,0,2000000);
 REQUIRE(originalGate&&nativeGate,"original and native measured64 lip gates agree");
 REQUIRE(player.stats[STAT_TCE_FLAGS]&0x20,"original climb-possible bit set");
 ordinaryCandidate=NG_TrFreeClimb(&from,&to,2000000,&proof);
 relaxedEndpoint=NG_TrFreeClimbLegacyEndpoint(&from,&to,2000000,&proof);
 printf("rim64 floor60 original%d native%d currentEndpoint%d candidateEndpoint%d traces%d\n",originalGate,nativeGate,ordinaryCandidate,relaxedEndpoint,ngTraces);
 REQUIRE(!relaxedEndpoint,"old endpoint63 prefilter rejects floor60 before checking real rim");
 REQUIRE(ordinaryCandidate&&proof.climbSeconds>0&&proof.stamina>0&&proof.crash<=38,"full unchanged native three-speed proof accepts actual64 rim and floor60");
 departure=to;VectorSet(departure.feet,24,0,64.125f);
 REQUIRE(!NG_Walk(&to,&departure,ngStand),"native ordinary step onto4-unit return rim");
 VectorSet(from.feet,-64,0,.125f);
 REQUIRE(NG_TrWalkDrop(&departure,&from,2000000,&drop)&&drop.crashDelta<=38,"independent real three-speed zero-impulse return");
 rim_height=60;player.stats[STAT_TCE_FLAGS]=0x200;
 VectorSet(normal,-1,0,0);originalGate=PM_ClimbSlideMove(1,normal,0);normal[0]=1;
 nativeGate=NG_TrFreeContact(contact,normal,100,0,2000000);
 REQUIRE(!originalGate&&!nativeGate&&(player.stats[STAT_TCE_FLAGS]&0x40),"genuine measured60 lip remains blocked by original63 rule");
 VectorSet(from.feet,3.75f,0,.125f);
 REQUIRE(!NG_TrFreeClimb(&from,&to,2000000,&proof),"lower endpoint selection never bypasses actual60 lip rejection");
 puts("original measured lip gate, endpoint distinction, unchanged three-speed climb and independent return passed");return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class FreeClimbHeightNativeTests(TestCase):
    def test_actual_original_lip_height_and_native_landing_endpoint_distinction(self):
        with tempfile.TemporaryDirectory(prefix='tce-free-height-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(FreeHeight LANGUAGES C)\nadd_executable(free-height main.c)\n')
            options = dict(capture_output=True, text=True, timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release'],
                            [str(directory / 'build/Release/free-height.exe')]):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    main()
