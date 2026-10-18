"""Compile the production readonly TC free-climb/drop integrators.

Independent floor/vertical ledge sweeps exercise body contacts and safety
rejections. These are bounded collision regressions, not live map approval.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
try:
    from .test_standing_jump_native import SHIM
    from .test_traversal_reset_native import function
except ImportError:
    from test_standing_jump_native import SHIM
    from test_traversal_reset_native import function

ROOT = Path(__file__).resolve().parents[2]


def program():
    source = '\n'.join((ROOT / f'game/{name}').read_text() for name in
                       ('tce_navgen.c', 'tce_navgen_traversal.inc', 'tce_navgen_freeclimb.inc'))
    names = ('NG_Trace', 'NG_Hurt', 'NG_StaticGround', 'NG_ProbeBodyRepresentations',
             'NG_OrdinarySurface', 'NG_Ground', 'NG_Walk', 'NG_TrClear',
             'NG_TrGravity', 'NG_TrSafeDropHeight', 'NG_TrStableLanding',
             'NG_TrCentreSupported', 'NG_TrExitContinuation', 'NG_TrFreeContact', 'NG_TrFreeFinish',
             'NG_TrFreeSpeed', 'NG_TrFreeClimb', 'NG_TrWalkDropSpeed', 'NG_TrWalkDrop')
    functions = [function(source, name) for name in names]
    needed = set(re.findall(r'\bng\w+\b', '\n'.join(functions))) | {'ngDropSpeeds', 'ngTraceBudget'}
    declarations = [line for line in source.splitlines()
                    if re.match(r'^static [^;()]+;', line)
                    and needed.intersection(re.findall(r'\bng\w+\b', line))]
    structures = [text for text in re.findall(r'typedef struct\s*\{[^}]+\}\s*\w+;', source, re.S)
                  if text.rsplit('}', 1)[1].strip(' ;') in
                  ('ngDoorRecipe', 'ngNode', 'ngFreeClimbProof', 'ngDropProof')]
    defines = '\n'.join(line for line in source.splitlines() if re.match(r'^#define NG_\w+\s', line))
    shim = SHIM.replace('SURF_SLICK=1', 'SURF_LADDER=8,SURF_SLICK=1')
    start = shim.index('static void fixture_trace(')
    end = shim.index('static void trap_Trace(', start)
    shim = shim[:start] + r'''
/* Independent swept body against a floor and an axis-aligned platform
 * occupying x>=20,z<=height. Solve the intersection of X/Z slab intervals,
 * preserving a vertical face normal instead of calling it a raised floor. */
static void fixture_trace(trace_t *tr,const vec3_t a,const vec3_t mins,
 const vec3_t maxs,const vec3_t b,int capsule){
 float width=mins?fabsf(mins[0]):0,low=mins?mins[2]:0,head=maxs?maxs[2]:0;
 float height=scenario==1?10000.f:target_height,edge=20.f,hit=1.f;
 float x0=a[0]+width-edge,z0=a[2]+low-height,dx=b[0]-a[0],dz=b[2]-a[2];
 float enter=0,leave=1;int side=0,i;
 if(capsule)body_calls++;else if(mins)foot_calls++;else point_calls++;
 memset(tr,0,sizeof(*tr));tr->fraction=1;tr->entityNum=ENTITYNUM_NONE;VectorCopy(b,tr->endpos);
 if(a[2]+low<-.001f||(x0>.001f&&z0<-.001f)||
    (scenario==2&&a[2]+head>180.001f)){
  tr->startsolid=tr->allsolid=1;tr->fraction=0;VectorCopy(a,tr->endpos);return;
 }
 if(b[2]+low<0&&dz<0){hit=-(a[2]+low)/dz;side=1;}
 if(scenario==2&&b[2]+head>180&&dz>0){float t=(180-a[2]-head)/dz;if(t<hit){hit=t;side=2;}}
 if(dx==0){if(x0<=0)enter=2;}else if(dx>0){if(x0<0)enter=-x0/dx;}else {if(x0<=0)enter=2;else leave=-x0/dx;}
 if(dz==0){if(z0>=0)enter=2;}else if(dz<0){if(z0>0){float t=-z0/dz;if(t>enter)enter=t;}}
 else {if(z0>=0)enter=2;else {float t=-z0/dz;if(t<leave)leave=t;}}
 if(enter>=0&&enter<leave&&enter<hit&&enter<=1){
  hit=enter;side=(dx>0&&fabsf(x0+enter*dx)<.001f)?3:1;
 }
 if(side){tr->fraction=hit;tr->entityNum=scenario==5&&side==3?100:ENTITYNUM_WORLD;
  VectorSet(tr->plane.normal,side==3?-1:0,0,side==1?1:side==2?-1:0);
  for(i=0;i<3;i++)tr->endpos[i]=a[i]+hit*(b[i]-a[i]);}
}
''' + shim[end:]
    headers = (ROOT / 'game/tce_nav_jump.h').read_text() + (ROOT / 'game/tce_nav_water.h').read_text()
    return shim + defines + '\n' + headers + '\n' + '\n'.join(structures + declarations + functions) + r'''
int main(int argc,char **argv){
 ngNode a,b;ngFreeClimbProof proof;ngDropProof drop;int accepted;
 scenario=argc>1?atoi(argv[1]):0;
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
 g_newbbox.integer=1;ngWidth=16;ngStand=87;ngDuck=61;ngOrigin=30;
 g_gravity.value=800;ngTraceBudget=2000000;target_height=102;
 VectorSet(a.feet,3.75f,0,.125f);VectorSet(b.feet,64,0,102.125f);
 a.accepted=b.accepted=qtrue;g_entities[100].inuse=1;g_entities[100].classname="trigger_hurt";
 accepted=NG_TrFreeClimb(&a,&b,2000000,&proof);
 printf("freeclimb scenario%d accepted%d traces%d body%d\n",scenario,accepted,ngTraces,body_calls);
 if(!scenario){
  REQUIRE(accepted,"102-unit TC ledge");REQUIRE(proof.climbSeconds>0,"real free-climb contact required");
  REQUIRE(proof.stamina<2490&&proof.crash<=38&&proof.duration>0,"native stamina/landing receipt");
  REQUIRE(body_calls>100,"full body native collision paths");
  VectorCopy(b.feet,a.feet);VectorSet(b.feet,-64,0,.125f);memset(&drop,0,sizeof(drop));
  REQUIRE(NG_TrSafeDropHeight()>148&&NG_TrSafeDropHeight()<149,"PM_CrashLand-derived safe enlarged drop bound");
  accepted=NG_TrWalkDrop(&a,&b,2000000,&drop);printf("drop accepted%d reason%s at%.3f,%.3f,%.3f\n",accepted,ngDropFailure,ngDropFailurePoint[0],ngDropFailurePoint[1],ngDropFailurePoint[2]);
  REQUIRE(accepted,"102-unit truck zero-impulse descent");
  REQUIRE(drop.crashDelta<=38&&drop.impact>0,"safe actual impact samples");
  target_height=170;VectorSet(a.feet,64,0,170.125f);
  REQUIRE(!NG_TrWalkDrop(&a,&b,2000000,&drop),"170-unit damaging drop rejected");
 }else REQUIRE(!accepted,"obstructed/hazard/dynamic ledge rejected");
 return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class FreeClimbNativeTests(TestCase):
    def test_actual_integrators_ledge_safe_drop_and_negative_safety_gates(self):
        with tempfile.TemporaryDirectory(prefix='tce-free-climb-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(FreeClimbRegression LANGUAGES C)\n'
                'add_executable(free-climb main.c)\n')
            options = dict(capture_output=True, text=True, timeout=90, creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            executable = directory / 'build/Release/free-climb.exe'
            for number, name in enumerate(('clear102/drop102', 'no ledge', 'low ceiling', 'hurt', 'water', 'dynamic face')):
                with self.subTest(fixture=name):
                    result = subprocess.run([str(executable), str(number)], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    main()
