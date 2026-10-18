"""Actual standing-only C landing proof against analytic plane/hull geometry.

The fixture distinguishes a square foot box from the rounded capsule and
includes the collision skin. It is a focused native regression, not engine
parity or a live jump certificate.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless

from test_standing_jump_native import SHIM, program as standing_program
from test_traversal_reset_native import function


GEOMETRY = r'''
static float slope_x=.056f,slope_y=.080f;
static float fixture_floor(const vec3_t p) {
 float z=slope_x*p[0]+slope_y*p[1];
 if(scenario==6&&p[0]>8.f)z+=.25f;
 return z;
}
static int fixture_hole(const vec3_t p) {
 return (scenario==3&&fabsf(p[0])<2.f&&fabsf(p[1])<2.f)||
        (scenario==4&&p[0]>8.f&&p[1]>8.f)||
        (scenario==19&&p[0]<-8.f&&p[1]<-8.f);
}
static void fixture_trace(trace_t *tr,const vec3_t a,const vec3_t mins,
 const vec3_t maxs,const vec3_t b,int capsule) {
 float r=mins?fabsf(mins[0]):0.f;
 float scale=sqrtf(1.f+slope_x*slope_x+slope_y*slope_y);
 float shape=capsule?r*(scale-1.f):r*(fabsf(slope_x)+fabsf(slope_y));
 float start_floor=fixture_floor(a)+shape,end_floor=fixture_floor(b)+shape;
 float d1=a[2]-start_floor,d2=b[2]-end_floor,skin=.125f*scale;
 int body=capsule&&maxs&&maxs[2]>2.f*r;
 if(capsule)body_calls++;else if(mins)foot_calls++;else point_calls++;
 memset(tr,0,sizeof(*tr));tr->fraction=1.f;tr->entityNum=ENTITYNUM_NONE;
 VectorCopy(b,tr->endpos);
 VectorSet(tr->plane.normal,-slope_x/scale,-slope_y/scale,1.f/scale);
 if((!mins&&fixture_hole(a))||(body&&scenario==16))return;
 if((body&&scenario==7)||d1<-.00001f||
    (scenario==20&&body&&a[0]<-4.f&&b[0]>=-4.f)) {
  tr->startsolid=tr->allsolid=1;tr->fraction=0;VectorCopy(a,tr->endpos);return;
 }
 if(d2<d1&&d2<=skin) {
  int i;float t=(d1-skin)/(d1-d2);if(t<0)t=0;if(t>1)t=1;
  tr->fraction=t;tr->entityNum=scenario==9?100:ENTITYNUM_WORLD;
  tr->surfaceFlags=scenario==10?SURF_SLICK:0;
  for(i=0;i<3;i++)tr->endpos[i]=a[i]+t*(b[i]-a[i]);
  if(!mins&&scenario==5&&a[0]>8.f)tr->plane.normal[0]-=.01f;
 }
}
'''


def program():
    original = function(SHIM, 'fixture_trace')
    shim = SHIM.replace(original, GEOMETRY)
    shim = shim.replace(function(shim, 'trap_PointContents'), r'''
static int trap_PointContents(const vec3_t p,int entity) {
 (void)p;(void)entity;
 return scenario==11?CONTENTS_WATER:scenario==12?CONTENTS_SLIME:scenario==13?CONTENTS_LAVA:0;
}
''')
    shim = shim.replace(function(shim, 'trap_EntitiesInBox'), r'''
static int trap_EntitiesInBox(const vec3_t mins,const vec3_t maxs,int *entities,int size) {
 (void)mins;(void)maxs;if(scenario==14&&size){entities[0]=100;return 1;}return 0;
}
''')
    code = standing_program().split('int main(', 1)[0].replace(SHIM, shim)
    return code + r'''
int main(int argc,char **argv) {
 vec3_t feet,from,down;trace_t contact;int accepted,old,positive;
 float scale=sqrtf(1.f+slope_x*slope_x+slope_y*slope_y),gap;
 scenario=argc>1?atoi(argv[1]):0;
 ngWidth=14;ngStand=70;ngDuck=49;ngOrigin=24;ngTraceBudget=2000000;
 g_newbbox.integer=0;g_gravity.value=800;
 g_entities[100].inuse=1;g_entities[100].classname="trigger_hurt";
 if(scenario==1||scenario==17)slope_x=slope_y=0;
 if(scenario==2){slope_x=.2f;slope_y=0;}
 scale=sqrtf(1.f+slope_x*slope_x+slope_y*slope_y);
 VectorSet(feet,0,0,ngWidth*(fabsf(slope_x)+fabsf(slope_y))+.125f*scale+.125f);
 if(scenario==8)feet[2]+=.1f;
 if(scenario==17)feet[2]=1.5f;
 VectorCopy(feet,down);down[2]-=3.f;
 if(scenario==0) {
  REQUIRE(NG_Trace(&contact,feet,down,ngStand)&&contact.fraction<1.f,"analytic full capsule support");
  gap=feet[2]-contact.endpos[2];
  REQUIRE(gap>1.95f&&gap<1.98f,"square-foot versus actual capsule gap with skin");
  REQUIRE(fabsf(contact.endpos[2]-(ngWidth*(scale-1.f)+.125f*scale))<.0001f,"actual rounded support height");
 }
 old=NG_TrStableLanding(feet);
 if(scenario==15)ngTraces=NG_TRACE_BUDGET-1;
 accepted=NG_TrStandingLanding(feet);
 positive=scenario==0||scenario==1||scenario==18;
 printf("slope fixture%d old%d standing%d body%d foot%d point%d traces%d\n",
        scenario,old,accepted,body_calls,foot_calls,point_calls,ngTraces);
 REQUIRE(accepted==positive,"typed landing outcome");
 if(scenario==0||scenario==18) {
  REQUIRE(!old,"unchanged flat proof rejects square-foot slope offset");
  REQUIRE(body_calls>2&&foot_calls>0&&point_calls>=11,"real capsule, ordinary self-walk and all ring rays");
  VectorSet(from,-8,0,feet[2]-8*slope_x);
  REQUIRE(!NG_TrExitContinuation(from,feet),"untyped drop and ladder exit still rejects slope");
  REQUIRE(NG_TrStandingExitContinuation(from,feet),"typed full ordinary supported continuation");
  if(scenario==18) {
   scenario=20;
   REQUIRE(!NG_TrStandingExitContinuation(from,feet),"blocked continuation never inherits landing success");
  }
 }
 if(scenario==1)REQUIRE(old&&body_calls==0,"existing flat path remains first and unchanged");
 if(scenario==15)REQUIRE(ngTraces<=NG_TRACE_BUDGET,"no trace budget extension");
 return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class StandingSlopeNativeTests(TestCase):
    def test_actual_capsule_and_nine_ray_plane_contract(self):
        fixtures = ('gentle capsule slope', 'unchanged flat', 'steep', 'centre hole',
                    'ring hole', 'normal seam', 'height seam', 'body startsolid',
                    'support beyond two', 'dynamic', 'slick', 'water', 'slime',
                    'lava', 'hurt', 'budget', 'no capsule support',
                    'no lifted-flat waiver', 'continuation obstruction', 'deep ring hole')
        with tempfile.TemporaryDirectory(prefix='tce-standing-slope-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\n'
                'project(StandingSlopeRegression LANGUAGES C)\nadd_executable(slope main.c)\n')
            options = dict(capture_output=True, text=True, timeout=90,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            executable = directory / 'build/Release/slope.exe'
            for number, label in enumerate(fixtures):
                with self.subTest(fixture=label):
                    result = subprocess.run([str(executable), str(number)], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    main()
