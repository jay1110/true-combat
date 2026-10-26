"""Actual 27-flight standing proof into a bumper/gentle-hood brush seam.

Independent convex-plane hull sweeps include rounded capsule versus square
foot expansion. Native regression only, not a claim of ET/live parity.
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
static float hood_slope=.096f,bumper_height=31.f,bumper_front=32.f,hood_cross=28.f;
static int slope_sweeps,hood_contacts;
static vec3_t last_a,last_b,last_normal;static float last_fraction;
static void fixture_brush(trace_t *tr,const vec3_t a,const vec3_t b,const vec3_t mins,
 const vec3_t maxs,int capsule,const float planes[][4],int count) {
 float enter=-1.f,leave=1.f;int i,outside=0;vec3_t hitnormal;
 for(i=0;i<count;i++) {
  const float *p=planes[i];float expansion=0,d1,d2;
  if(mins) {
   if(capsule) {
    float r=fabsf(mins[0]),half=(maxs[2]-mins[2])*.5f,centre=(maxs[2]+mins[2])*.5f;
    expansion=r+(half-r)*fabsf(p[2])-centre*p[2];
   }else expansion=fabsf(mins[0])*fabsf(p[0])+fabsf(mins[1])*fabsf(p[1])+
     (p[2]>=0?-mins[2]*p[2]:-maxs[2]*p[2]);
  }
  d1=DotProduct(p,a)-p[3]-expansion;d2=DotProduct(p,b)-p[3]-expansion;
  if(d1>0)outside=1;
  if(d1>0&&d2>=d1)return;
  if(d1<=0&&d2<=0)continue;
  if(d1>d2){float f=(d1-.125f)/(d1-d2);if(f>enter){enter=f;VectorCopy(p,hitnormal);}}
  else {float f=(d1+.125f)/(d1-d2);if(f<leave)leave=f;}
 }
 if(!outside){tr->startsolid=1;tr->allsolid=1;tr->fraction=0;VectorCopy(a,tr->endpos);return;}
 if(enter<leave&&enter>-1&&enter<tr->fraction) {
  if(enter<0)enter=0;tr->fraction=enter;tr->entityNum=scenario==7?100:ENTITYNUM_WORLD;
  tr->surfaceFlags=scenario==8?SURF_SLICK:0;VectorCopy(hitnormal,tr->plane.normal);
  for(i=0;i<3;i++)tr->endpos[i]=a[i]+enter*(b[i]-a[i]);
 }
}
static void fixture_trace(trace_t *tr,const vec3_t a,const vec3_t mins,const vec3_t maxs,const vec3_t b,int capsule) {
 float scale=sqrtf(1+hood_slope*hood_slope);
 float road[][4]={{0,0,1,0}};
 float bumper[][4]={{-1,0,0,-bumper_front},{0,0,1,bumper_height},{0,0,-1,0}};
 float hood[][4]={{-1,0,0,-bumper_front},{-hood_slope/scale,0,1/scale,(bumper_height-hood_slope*hood_cross)/scale},{0,0,-1,0}};
 int body=capsule&&maxs&&maxs[2]>28;
 if(capsule)body_calls++;else if(mins)foot_calls++;else point_calls++;
 memset(tr,0,sizeof(*tr));tr->fraction=1;tr->entityNum=ENTITYNUM_NONE;VectorCopy(b,tr->endpos);VectorSet(tr->plane.normal,0,0,1);
 if(!mins&&scenario==5&&a[0]>56)return;
 if(body&&scenario==6&&a[0]>34&&a[0]<38&&a[2]<36&&b[2]<a[2])return;
 fixture_brush(tr,a,b,mins,maxs,capsule,road,1);
 fixture_brush(tr,a,b,mins,maxs,capsule,bumper,3);
 fixture_brush(tr,a,b,mins,maxs,capsule,hood,3);
 if(scenario==1){float wall[][4]={{-1,0,0,-44},{1,0,0,46}};fixture_brush(tr,a,b,mins,maxs,capsule,wall,2);}
 if(scenario==2){float ceiling[][4]={{0,0,-1,-107},{0,0,1,111}};fixture_brush(tr,a,b,mins,maxs,capsule,ceiling,2);}
 if(body&&b[0]>a[0]&&b[2]>a[2]&&a[0]>26&&a[2]>30)slope_sweeps++;
 if(body&&tr->fraction<1&&tr->plane.normal[2]>.99f&&tr->plane.normal[2]<.99999f)hood_contacts++;
 if(body&&b[0]>a[0]){VectorCopy(a,last_a);VectorCopy(b,last_b);VectorCopy(tr->plane.normal,last_normal);last_fraction=tr->fraction;}
}
'''


def program():
    shim = SHIM.replace(function(SHIM, 'fixture_trace'), GEOMETRY)
    shim = shim.replace(function(shim, 'trap_PointContents'), r'''
static int trap_PointContents(const vec3_t p,int entity){(void)entity;return scenario==4&&p[0]>26?CONTENTS_WATER:0;}
''')
    source = standing_program().split('int main(', 1)[0].replace(SHIM, shim)
    # Disable only the new grounded slope-contact case, preserving the same
    # production trajectory and the unchanged old remainder/continuation.
    legacy = function(source, 'NG_TrStandSpeed').replace('NG_TrStandSpeed(', 'Legacy_StandSpeed(', 1)
    needle = 'if(TCE_NavGroundKind(tr.entityNum)!=1||tr.plane.normal[2]<.99f||\n         tr.plane.normal[2]>=.99999f'
    assert needle in legacy
    legacy = legacy.replace(needle, 'if(1||TCE_NavGroundKind(tr.entityNum)!=1||tr.plane.normal[2]<.99f||\n         tr.plane.normal[2]>=.99999f', 1)
    return source + legacy + r'''
int main(int argc,char **argv) {
 ngNode a,b;ngJumpProof proof;int accepted,old;float scale;
 scenario=argc>1?atoi(argv[1]):0;
 ngWidth=14;ngStand=70;ngDuck=49;ngOrigin=24;ngTraceBudget=2000000;
 g_newbbox.integer=0;g_gravity.value=800;
 g_entities[100].inuse=1;g_entities[100].classname="trigger_hurt";
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
 if(scenario==9)hood_slope=.2f;
 scale=sqrtf(1+hood_slope*hood_slope);
 VectorSet(a.feet,0,0,.25f);
 VectorSet(b.feet,48,0,bumper_height+hood_slope*(48-hood_cross)+ngWidth*(scale-1)+.125f*scale+.125f);
 if(scenario==10)ngTraces=NG_TRACE_BUDGET-1;
 memset(&proof,0,sizeof(proof));accepted=NG_TrStandingJump(&a,&b,16000,&proof);
 printf("fixture%d accepted%d failure%d phase%d normal%.5f traces%d slope%d contacts%d\n",scenario,accepted,ngJumpArcFailure,proof.failurePhase,proof.failureNormal[2],ngTraces,slope_sweeps,hood_contacts);
 if(!accepted)printf("last forward a %.9f %.9f b %.9f %.9f fraction %.9f normal %.9f %.9f\n",last_a[0],last_a[2],last_b[0],last_b[2],last_fraction,last_normal[0],last_normal[2]);
 if(!scenario) {
  REQUIRE(accepted,"all27 real flights and grounded gentle seam continuation");
  REQUIRE(ngTraces<16000-512,"complete27 remains within actual native probe budget");
  REQUIRE(slope_sweeps>27&&hood_contacts>27,"genuine ascending full-body continuation");
  REQUIRE(proof.standing&&proof.crash<=38,"existing standing full proof");
  ngTraces=0;memset(&proof,0,sizeof(proof));
  old=Legacy_StandSpeed(&a,&b,36,114,0,2000000,&proof);
  printf("old%d failure%d phase%d normal%.7f\n",old,ngJumpArcFailure,proof.failurePhase,proof.failureNormal[2]);
  REQUIRE(!old&&ngJumpArcFailure==9&&proof.failurePhase==2&&proof.failureNormal[2]>.99f,"retained horizontal branch reproduces real ground hood collision");
 }else {
  REQUIRE(!accepted,"negative geometry never inherits gentle landing success");
  if(scenario==2)REQUIRE(proof.failurePhase==1,"ceiling counterexample is a real air contact");
  if(scenario==6)REQUIRE(ngJumpArcFailure==13,"transient unsupported body fails real ground continuation");
 }
 return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class StandingGroundSlopeNativeTests(TestCase):
    def test_actual_twenty_seven_flights_and_grounded_remainder(self):
        with tempfile.TemporaryDirectory(prefix='tce-standing-ground-slope-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(StandingGroundSlope LANGUAGES C)\nadd_executable(slope main.c)\n')
            options = dict(capture_output=True, text=True, timeout=90, creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory/'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory/'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for scenario in range(11):
                with self.subTest(scenario=scenario):
                    result = subprocess.run([str(directory/'build/Release/slope.exe'), str(scenario)], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    if scenario==0:print(result.stdout.strip())


if __name__ == '__main__': main()
