"""Execute native front discovery and traversal proofs on independent brushes.

The production functions are extracted unchanged. The fixture supplies two
horizontal floors and finite wall planes; it is not a live ET certification.
No game, deployment or additional installation is started by this test.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
try:
    from .test_standing_jump_native import program as standing_program
    from .test_traversal_reset_native import function, ROOT
except ImportError:
    from test_standing_jump_native import program as standing_program
    from test_traversal_reset_native import function, ROOT


GEOMETRY = r'''
static float floor_end=0.f,high_begin=9.f,high_y_min=-1000.f,high_y_max=1000.f;
static float support(float x,float y,float halfwidth){
 float value=-1000.f;
 if(x-halfwidth<=floor_end)value=0.f;
 if(scenario!=3&&x+halfwidth>=high_begin&&y+halfwidth>=high_y_min&&y-halfwidth<=high_y_max)value=target_height;
 return value;
}
static int overlap(const vec3_t p,const vec3_t mins,const vec3_t maxs,vec3_t normal){
 float width=mins?fabsf(mins[0]):0.f,low=mins?mins[2]:0.f,top=maxs?maxs[2]:0.f;
 if(p[2]+low<support(p[0],p[1],width)-.000001f){VectorSet(normal,0,0,1);return 1;}
 if(p[2]+top>96.f&&scenario==2){VectorSet(normal,0,0,-1);return 2;}
 if(scenario==1&&p[0]+width>=high_begin&&p[2]+top>target_height){
  if(p[1]-width<high_y_min){VectorSet(normal,0,1,0);return 3;}
  if(p[1]+width>high_y_max){VectorSet(normal,0,-1,0);return 3;}
 }
 return 0;
}
static void fixture_trace(trace_t *tr,const vec3_t a,const vec3_t mins,const vec3_t maxs,const vec3_t b,int capsule){
 float previous=0.f;int sample,hit=0;vec3_t p,normal;
 if(capsule)body_calls++;else if(mins)foot_calls++;else point_calls++;
 memset(tr,0,sizeof(*tr));tr->fraction=1.f;tr->entityNum=ENTITYNUM_NONE;
 VectorCopy(b,tr->endpos);VectorSet(tr->plane.normal,0,0,1);
 if(overlap(a,mins,maxs,normal)){tr->startsolid=tr->allsolid=1;tr->fraction=0;VectorCopy(a,tr->endpos);return;}
 for(sample=1;sample<=32;sample++){
  float t=(float)sample/32;int i;for(i=0;i<3;i++)p[i]=a[i]+t*(b[i]-a[i]);
  if((hit=overlap(p,mins,maxs,normal))!=0){
   float lower=previous,upper=t;int iteration;
   for(iteration=0;iteration<24;iteration++){
    float m=(lower+upper)*.5f;for(i=0;i<3;i++)p[i]=a[i]+m*(b[i]-a[i]);
    if(overlap(p,mins,maxs,normal))upper=m;else lower=m;
   }
   tr->fraction=lower;tr->entityNum=ENTITYNUM_WORLD;VectorCopy(normal,tr->plane.normal);
   for(i=0;i<3;i++)tr->endpos[i]=a[i]+lower*(b[i]-a[i]);return;
  }
  previous=t;
 }
}
'''


def program(diagnostic_control=False):
    core = (ROOT / 'game/tce_navgen.c').read_text()
    traversal = (ROOT / 'game/tce_navgen_traversal.inc').read_text()
    source = core + '\n' + traversal
    text = standing_program().split('int main(int argc', 1)[0]
    if diagnostic_control:
        # Recreate the observed stale reason code at the dry under-height
        # return. The same executable assertion must reject this control.
        old = 'if(DotProduct(error,direction)>8.f||(velocity[2]<=0.f&&next[2]<b->feet[2]-18.f))NG_STAND_FAIL(12);'
        if old not in text:
            raise AssertionError('Actual standing diagnostic return missing')
        text = text.replace(old, old.replace('NG_STAND_FAIL(12)', 'NG_STAND_FAIL(10)'), 1)
    first = text.index('static float support(')
    last = text.index('static void trap_Trace(', first)
    text = text[:first] + GEOMETRY + '\n' + text[last:]
    names = ('NG_TrSafeDropHeight', 'NG_TrWalkDropSpeed', 'NG_TrWalkDrop',
             'NG_TrFrontSample', 'NG_TrFrontStrip', 'NG_TrFrontSamples', 'NG_TrFrontPortal', 'NG_TrLaunchSeparated', 'NG_TrFrontPairs')
    functions = [function(source, name) for name in names]
    structs = re.findall(r'typedef struct\s*\{[^}]+\}\s*\w+;', source, re.S)
    structures = [item for item in structs if item.rsplit('}', 1)[1].strip(' ;')
                  in ('ngDropProof', 'ngFrontJumpCase', 'ngFrontArcPair')]
    needed = set(re.findall(r'\bng\w+\b', '\n'.join(functions)))
    declarations = [line for line in source.splitlines()
                    if re.match(r'^static [^;()]+;', line) and line not in text
                    and needed.intersection(re.findall(r'\bng\w+\b', line))]
    return text + '\n'.join(structures + declarations + functions) + r'''
int main(int argc,char **argv){
 ngNode low[21],high[21];ngFrontJumpCase entry;ngJumpProof jump;ngDropProof drop;
 int lc,hc,a,b,c,d,pair,pairCount,found=0,misses=0;vec3_t direction,gap;
 qboolean lowLaunch[21];ngFrontArcPair pairs[21*21];
 scenario=argc>1?atoi(argv[1]):0;target_height=20.f;
 g_newbbox.integer=0;ngWidth=14;ngStand=70;ngDuck=49;ngOrigin=24;
 g_gravity.value=800;ngTraceBudget=2000000;ngCount=2;memset(&entry,0,sizeof(entry));
 memset(ngNodes,0,sizeof(ngNodes));
 VectorSet(ngNodes[0].feet,-76,0,.125f);VectorSet(ngNodes[1].feet,42,0,20.125f);
 ngNodes[0].accepted=ngNodes[1].accepted=1;ngNodes[0].id=1;ngNodes[1].id=2;
 ngNodes[0].radius=ngNodes[1].radius=20;
 if(scenario==1){high_y_min=-72;high_y_max=-28;ngNodes[1].feet[1]=-50;}
 if(scenario==4){/* Water is a real hazard, unlike a dry missed landing. */}
 if(scenario==5)high_begin=80;
 if(scenario==6){
  ngNode launch=ngNodes[0],landing=ngNodes[1];int failed;
  VectorSet(launch.feet,-40,0,.125f);VectorSet(landing.feet,100,0,20.125f);
  memset(&jump,0,sizeof(jump));failed=!NG_TrStandSpeed(&launch,&landing,36,114,0,24000,&jump);
  REQUIRE(failed,"dry under-height/landing failure");
  REQUIRE(ngJumpArcFailure==12,"dry failure must report landing continuation, not liquid or hurt");
  printf("dry miss rejected reason=%d\n",ngJumpArcFailure);return 0;
 }
 VectorSubtract(ngNodes[1].feet,ngNodes[0].feet,direction);direction[2]=0;VectorNormalize(direction);
 lc=NG_TrFrontSamples(0,direction,low,24000,&entry);VectorScale(direction,-1,direction);
 hc=NG_TrFrontSamples(1,direction,high,24000,&entry);
 REQUIRE(lc<=21&&hc<=21,"unchanged bounded output arrays");
 for(a=0;a<lc;a++){
  REQUIRE(fabs(low[a].feet[2]-.125f)<.01f&&NG_TrStableLanding(low[a].feet),"low samples on actual stable source floor");
  REQUIRE(!NG_Walk(&ngNodes[0],&low[a],ngStand)&&!NG_Walk(&low[a],&ngNodes[0],ngStand),"two real low approach walks");
 }
 for(b=0;b<hc;b++){
  REQUIRE(fabs(high[b].feet[2]-20.125f)<.01f&&NG_TrStableLanding(high[b].feet),"high samples on actual stable target floor");
  REQUIRE(!NG_Walk(&ngNodes[1],&high[b],ngStand)&&!NG_Walk(&high[b],&ngNodes[1],ngStand),"two real high approach walks");
 }
 for(a=0;a<lc;a++)lowLaunch[a]=NG_TrLaunchSeparated(&low[a]);
 pairCount=NG_TrFrontPairs(low,lc,high,hc,lowLaunch,&ngNodes[0],&ngNodes[1],pairs);
 REQUIRE(pairCount<=21*21,"unchanged maximum pair capacity");
 {float previous[7]={-1,-1,-1,-1,-1,-1,-1};unsigned int firstStrips=0;vec3_t forward,tangent;
 VectorSubtract(ngNodes[1].feet,ngNodes[0].feet,forward);forward[2]=0;VectorNormalize(forward);VectorSet(tangent,-forward[1],forward[0],0);
 for(pair=0;pair<pairCount;pair++){
  int strip;VectorSubtract(low[pairs[pair].low].feet,ngNodes[0].feet,gap);
  strip=(int)floor(DotProduct(gap,tangent)/16.f+.5f)+3;
  REQUIRE(pairs[pair].low>=0&&pairs[pair].low<lc&&pairs[pair].high>=0&&pairs[pair].high<hc,"pair array bounds");
  REQUIRE(lowLaunch[pairs[pair].low],"pairs never use an excluded launch");
  REQUIRE(strip>=0&&strip<7&&previous[strip]<=pairs[pair].cost,"cost order retained inside each strip");previous[strip]=pairs[pair].cost;
  if(pair<7&&lc==21){REQUIRE(!(firstStrips&(1u<<strip)),"first round covers different lateral approaches");firstStrips|=1u<<strip;}
 }
 }
 for(pair=0;pair<pairCount&&!found&&ngTraces<24000-512;pair++){
   a=pairs[pair].low;b=pairs[pair].high;
   memset(&jump,0,sizeof(jump));
   if(!NG_TrStandingJump(&low[a],&high[b],24000,&jump)){misses++;continue;}
   for(c=0;c<hc&&!found&&ngTraces<24000-512;c++)if(NG_TrLaunchSeparated(&high[c]))
    for(d=0;d<lc&&!found&&ngTraces<24000-512;d++){
     VectorSubtract(low[a].feet,low[d].feet,gap);if(DotProduct(gap,gap)<20*20)continue;
     if(NG_TrWalkDrop(&high[c],&low[d],24000,&drop)){
      printf("paired low %.3f %.3f high %.3f %.3f return %.3f %.3f -> %.3f %.3f crash %.3f/%.3f\n",
       low[a].feet[0],low[a].feet[1],high[b].feet[0],high[b].feet[1],
       high[c].feet[0],high[c].feet[1],low[d].feet[0],low[d].feet[1],jump.crash,drop.crashDelta);found=1;
     }
    }
 }
 printf("scenario %d paired %d low %d high %d traces %d misses %d failure %d\n",scenario,found,lc,hc,ngTraces,misses,ngJumpArcFailure);
 REQUIRE(ngTraces<24000,"unchanged trace budget even for negative geometry");
 if(scenario==0||scenario==1){
  REQUIRE(found,"actual standing ascent plus independent safe zero-impulse return");
  REQUIRE(ngTraces<24000,"unchanged trace budget");
  REQUIRE(jump.standing&&jump.crash<=38&&drop.crashDelta<=38,"unchanged physics and crash limits");
 }else REQUIRE(!found,"missing floor, blocked body or hazard never creates a route");
 return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class FrontJumpNativeTests(TestCase):
    def test_native_front_search_two_floor_and_negative_geometry(self):
        with tempfile.TemporaryDirectory(prefix='tce-front-regression-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'control.c').write_text(program(diagnostic_control=True))
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(FrontRegression LANGUAGES C)\n'
                'add_executable(front-jump main.c)\nadd_executable(diagnostic-control control.c)\n')
            options = dict(capture_output=True, text=True, timeout=90,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            executable = directory / 'build/Release/front-jump.exe'
            for number, name in enumerate(('rise20 gap9', 'lateral corridor', 'low ceiling',
                                           'missing upper floor', 'water', 'large gap', 'dry miss')):
                with self.subTest(fixture=name):
                    result = subprocess.run([str(executable), str(number)], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(directory / 'build/Release/diagnostic-control.exe'), '6'], **options)
            self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('dry failure must report landing continuation', result.stderr)


if __name__ == '__main__':
    main()
