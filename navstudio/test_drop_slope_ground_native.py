"""Actual DROP integrator over a finite inclined slab and lower flat floor.

Independent rounded support and swept hull fixtures, not an engine/live proof.
The zero-Z ground branch remains a concrete negative control.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless

from test_standing_jump_native import SHIM
from test_walk_drop_native import program as drop_program
from test_traversal_reset_native import function


GEOMETRY = r'''
static float slope=-.096f,edge=24.f,roof=34.f;
static int tangent_moves,air_moves;
static float plane_height(float x,float radius,int capsule,vec3_t normal) {
 float scale=sqrtf(1.f+slope*slope),q=x;
 VectorSet(normal,-slope/scale,0,1.f/scale);
 if(!radius)return x<=edge?roof+slope*x:0.f;
 if(!capsule)return x-radius<=edge?roof+slope*(x-radius):0.f;
 q=x+radius*slope/scale;
 if(q<=edge)return roof+slope*x+radius*(scale-1.f);
 if(x-edge>=radius){VectorSet(normal,0,0,1);return 0.f;}
 {float dx=x-edge,z=sqrtf(radius*radius-dx*dx);
  VectorSet(normal,dx/radius,0,z/radius);
  return roof+slope*edge-radius+z;
 }
}
static float clearance(const vec3_t p,float radius,float height,int capsule,int *hit,vec3_t normal) {
 float floor=plane_height(p[0],radius,capsule,normal),d=p[2]-floor;
 if(floor<0){floor=0;VectorSet(normal,0,0,1);d=p[2];}
 *hit=1;
 if(scenario==1&&28.f-(p[0]+radius)<d){d=28.f-(p[0]+radius);VectorSet(normal,-1,0,0);*hit=2;}
 if(scenario==2&&p[0]>12.f&&100.f-(p[2]+height)<d){d=100.f-(p[2]+height);VectorSet(normal,0,0,-1);*hit=3;}
 return d;
}
static void fixture_trace(trace_t *tr,const vec3_t a,const vec3_t mins,const vec3_t maxs,const vec3_t b,int capsule) {
 float radius=mins?fabsf(mins[0]):0.f,height=maxs?maxs[2]:0.f;
 float skin=.125f,previous=0.f,initial;int hit,sample;vec3_t normal,p;
 int body=capsule&&height>2.f*radius;
 if(capsule)body_calls++;else if(mins)foot_calls++;else point_calls++;
 memset(tr,0,sizeof(*tr));tr->fraction=1.f;tr->entityNum=ENTITYNUM_NONE;VectorCopy(b,tr->endpos);
 VectorSet(tr->plane.normal,0,0,1);
 if(body&&scenario==12)return;
 if(!mins&&scenario==5&&a[0]>8.f&&a[1]>8.f)return;
 initial=clearance(a,radius,height,capsule,&hit,normal);
 if(initial<-.00001f){tr->startsolid=tr->allsolid=1;tr->fraction=0;VectorCopy(a,tr->endpos);return;}
 if(body&&b[0]>a[0]+.0001f) {
  if(a[0]<edge-4.f&&b[2]<a[2])tangent_moves++;
  if(a[0]>edge+radius&&b[2]<a[2])air_moves++;
 }
 for(sample=1;sample<=32;sample++) {
  float t=(float)sample/32,d;int i;
  for(i=0;i<3;i++)p[i]=a[i]+t*(b[i]-a[i]);
  d=clearance(p,radius,height,capsule,&hit,normal);
  if(d<initial-.000001f&&d<=skin) {
   float lower=previous,upper=t;int iteration;
   for(iteration=0;iteration<24;iteration++) {
    float mid=(lower+upper)*.5f;
    for(i=0;i<3;i++)p[i]=a[i]+mid*(b[i]-a[i]);
    if(clearance(p,radius,height,capsule,&hit,normal)<=skin)upper=mid;else lower=mid;
   }
   tr->fraction=lower;tr->entityNum=scenario==6?100:ENTITYNUM_WORLD;
   tr->surfaceFlags=scenario==7?SURF_SLICK:0;VectorCopy(normal,tr->plane.normal);
   for(i=0;i<3;i++)tr->endpos[i]=a[i]+lower*(b[i]-a[i]);return;
  }
  previous=t;
 }
}
'''


def program():
    shim = SHIM.replace(function(SHIM, 'fixture_trace'), GEOMETRY)
    shim = shim.replace(function(shim, 'trap_PointContents'), r'''
static int trap_PointContents(const vec3_t p,int entity){(void)entity;return scenario==4&&p[0]>8?CONTENTS_WATER:0;}
''')
    source = drop_program().split('int main(', 1)[0]
    # drop_program modifies the old SHIM's horizontal platform helper.
    old_shim = SHIM.replace('if(x+halfwidth>=platform_edge)return target_height;',
                           'if(x-halfwidth<platform_edge)return target_height;')
    source = source.replace(old_shim, shim)
    # Disable only the new typed activation: this executes the retained
    # zero-Z branch and unchanged flight/contact checks against the same hull.
    legacy = function(source, 'NG_TrWalkDropSpeed').replace(
        'NG_TrWalkDropSpeed(', 'Legacy_WalkDropSpeed(', 1).replace(
        'if(!step&&tr.plane.normal', 'if(0&&!step&&tr.plane.normal', 1)
    return source + legacy + r'''
int main(int argc,char **argv) {
 ngNode a,b;ngDropProof proof;int accepted,old;
 scenario=argc>1?atoi(argv[1]):0;
 ngWidth=14;ngStand=70;ngDuck=49;ngOrigin=24;ngTraceBudget=2000000;
 g_newbbox.integer=0;g_gravity.value=800;
 g_entities[100].inuse=1;g_entities[100].classname="trigger_hurt";
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
 if(scenario==8)roof=200;
 if(scenario==11)slope=-.2f;
 VectorSet(a.feet,0,0,roof+ngWidth*(sqrtf(1+slope*slope)-1)+.25f);
 VectorSet(b.feet,80,0,.25f);a.authored=1;a.editRole=9;a.editDropId=2;
 if(scenario==9)ngTraces=NG_TRACE_BUDGET-1;
 if(scenario==10)a.feet[2]+=.5f;
 accepted=NG_TrWalkDrop(&a,&b,2000000,&proof);
 printf("fixture%d accepted%d reason%s traces%d tangent%d air%d contacts%d\n",scenario,accepted,ngDropFailure,ngTraces,tangent_moves,air_moves,proof.intermediateContacts);
 if(!scenario) {
  REQUIRE(accepted,"all three command speeds depart gentle support and land safely");
  REQUIRE(tangent_moves>10&&air_moves>0,"actual descending supported movement then genuine falling");
  REQUIRE(proof.intermediateContacts==0&&proof.crashDelta<38,"no slope-intermediate-contact waiver");
  ngTraces=0;memset(&proof,0,sizeof(proof));
  old=Legacy_WalkDropSpeed(&a,&b,ngDropSpeeds[0],2000000,&proof);
  printf("legacy%d reason%s\n",old,ngDropFailure);
  REQUIRE(!old&&!strcmp(ngDropFailure,"ground_continuation"),"old zero-Z movement reproduces actual false slope impact");
 }else REQUIRE(!accepted,"obstruction hazard missing footprint or excessive impact stays rejected");
 return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class DropSlopeGroundNativeTests(TestCase):
    def test_actual_slope_departure_and_unchanged_hazards(self):
        with tempfile.TemporaryDirectory(prefix='tce-drop-slope-ground-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(DropSlope LANGUAGES C)\nadd_executable(drop main.c)\n')
            options = dict(capture_output=True, text=True, timeout=90, creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory/'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory/'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for scenario in range(13):
                with self.subTest(scenario=scenario):
                    result = subprocess.run([str(directory/'build/Release/drop.exe'), str(scenario)], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    main()
