"""Actual NG_Walk C tests for separate capsule support, with analytic floor fixtures.

These are independent plane/rounded ledge regression fixtures, not ET collision
parity or live map certification. No game/API or production module build occurs.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
from test_standing_jump_native import SHIM
from test_traversal_reset_native import function

ROOT = (Path(__file__).resolve().parents[1] / "src")


def program():
    source = (ROOT / 'game/tce_navgen.c').read_text()
    names = ('NG_Trace', 'NG_Hurt', 'NG_StaticGround', 'NG_ProbeBodyRepresentations',
             'NG_OrdinarySurface', 'NG_Ground', 'NG_Walk')
    functions = [function(source, name) for name in names]
    needed = set(re.findall(r'\bng\w+\b', '\n'.join(functions))) | {'ngTraceBudget'}
    declarations = [line for line in source.splitlines()
                    if re.match(r'^static [^;()]+;', line)
                    and needed.intersection(re.findall(r'\bng\w+\b', line))]
    structures = [text for text in re.findall(r'typedef struct\s*\{[^}]+\}\s*\w+;', source, re.S)
                  if text.rsplit('}', 1)[1].strip(' ;') in ('ngDoorRecipe', 'ngNode')]
    defines = '\n'.join(line for line in source.splitlines() if re.match(r'^#define NG_\w+\s', line))
    shim = SHIM
    start = shim.index('static float support(')
    end = shim.index('static void fixture_trace', start)
    shim = shim[:start] + r'''
// A sloped plane's full capsule support is r*(sqrt(1+slope^2)-1)
// above its centre floor. The flat foot box instead reaches slope*r higher.
static float support(float x,float radius,int capsule){
 if(scenario>=12&&scenario<=15){
  float lower=scenario==15?10.f:26.f,lip=-1000.f;
  if(!capsule){if(x<=radius)lip=40.f;}
  else if(x<=0.f)lip=40.f;
  else if(x<radius)lip=40.f-radius+sqrtf(radius*radius-x*x);
  return lip>lower?lip:lower;
 }
 float slope=(scenario==1||scenario==2||scenario==7||scenario==8||scenario==10)?.2f:0.f;
 float floor=slope*x+(capsule?radius*(sqrtf(1+slope*slope)-1):slope*radius);
 if(scenario==4)return 9.f*floorf((x+radius)/32.f);
 if(scenario==10)return floor+(x>=16.f?16.f:0.f);
 if(scenario==5||scenario==11){
  float ramp=.38f*x+9.6f+(capsule?radius*(sqrtf(1.1444f)-1):.38f*radius);
  float lip=-1000.f;
  if(!capsule){if(x-radius<0.f)lip=40.f;}
  else if(x<=0.f)lip=40.f;
  else if(x<radius)lip=40.f-radius+sqrtf(radius*radius-x*x);
  return ramp>lip?ramp:lip;
 }
 if(scenario==6&&x>24.f&&x<40.f)return -1000.f;
 return floor;
}
''' + shim[end:]
    shim=shim.replace('support(a[0],width)', 'support(a[0],width,capsule)')
    shim=shim.replace('support(p[0],width)', 'support(p[0],width,capsule)')
    shim=shim.replace('ceiling=scenario==2?96.f:10000.f',
                      'ceiling=scenario==3?63.f:(scenario>=12&&scenario<=15?102.f:10000.f)')
    shim=shim.replace('(scenario==1&&a[0]+width>28.f+.001f)', '(scenario==99&&a[0]+width>28.f+.001f)')
    shim=shim.replace('scenario==1&&p[0]+width>28.f+.000001f', 'scenario==99&&p[0]+width>28.f+.000001f')
    shim=shim.replace('if(scenario==3&&size&&mins[0]<80&&maxs[0]>24)', 'if(scenario==9&&size&&mins[0]<80&&maxs[0]>24)')
    shim=shim.replace('return (scenario==4&&p[0]>8)||(scenario==6&&p[2]<water_depth)?CONTENTS_WATER:0;',
                      'return scenario==7&&p[2]<water_depth?CONTENTS_WATER:0;')
    shim=shim.replace('VectorCopy(b,tr->endpos);VectorSet(tr->plane.normal,0,0,1);',
                      'VectorCopy(b,tr->endpos);VectorSet(tr->plane.normal,0,0,1);if(scenario==8)tr->surfaceFlags=SURF_SLICK;')
    # Floor normals reflect the independent sloped plane, including slick policy.
    shim=shim.replace('tr->entityNum=ENTITYNUM_WORLD;VectorCopy(normal,tr->plane.normal);',
                      'tr->entityNum=ENTITYNUM_WORLD;VectorCopy(normal,tr->plane.normal);if(hit==1&&(scenario==1||scenario==2||scenario==7||scenario==8)){tr->plane.normal[0]=-.196116f;tr->plane.normal[2]=.980581f;}')
    # The rounded ledge/ceiling above is geometric. These two additional
    # negative cases inject an engine sweep refusal, modelling a newly
    # blocking mover. They verify fail-closed handling of each orthogonal
    # leg and make no claim of static collision parity.
    shim=shim.replace('static int scenario,body_calls,foot_calls,point_calls;',
                      'static int scenario,body_calls,foot_calls,point_calls,refused_sweeps;')
    shim=shim.replace('(void)entity;(void)mask;fixture_trace(tr,a,mins,maxs,b,1);',r'''
 (void)entity;(void)mask;fixture_trace(tr,a,mins,maxs,b,1);
 if((scenario==13&&a[0]>=7.9f&&b[0]>a[0]+1.f&&fabsf(b[2]-a[2])<.001f&&a[2]<40.f)||
    (scenario==14&&a[0]>15.9f&&fabsf(a[0]-b[0])<.001f&&a[2]<40.f&&b[2]<a[2]-1.f)){
  int i;refused_sweeps++;tr->startsolid=tr->allsolid=0;tr->fraction=.5f;
  tr->entityNum=100;VectorSet(tr->plane.normal,-1,0,0);
  for(i=0;i<3;i++)tr->endpos[i]=(a[i]+b[i])*.5f;
 }
''')
    header=(ROOT/'game/tce_nav_water.h').read_text()
    return shim+defines+'\n'+header+'\n'+'\n'.join(structures+declarations+functions)+r'''
int main(int argc,char **argv){
 ngNode a,b,originalA,originalB;const char *why;float height=87.f;
 scenario=argc>1?atoi(argv[1]):0;
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
 g_newbbox.integer=1;ngWidth=16;ngStand=87;ngDuck=61;ngOrigin=30;ngTraceBudget=2000000;
 VectorSet(a.feet,0,0,.125f);VectorSet(b.feet,64,0,.125f);
 if(scenario==1||scenario==2||scenario==7||scenario==8||scenario==10){a.feet[2]=3.325f;b.feet[2]=16.125f;}
 if(scenario==2){ngNode swap=a;a=b;b=swap;}
 if(scenario==10)b.feet[2]+=16.f;
 if(scenario==3)height=61.f;
 if(scenario==4){a.feet[2]=.125f;b.feet[2]=18.125f;}
 if(scenario==5){VectorSet(a.feet,0,0,40.125f);VectorSet(b.feet,64,0,40.125f);height=61.f;}
 if(scenario==11){VectorSet(a.feet,64,0,40.125f);VectorSet(b.feet,0,0,40.125f);height=61.f;}
 if(scenario>=12&&scenario<=15){VectorSet(a.feet,0,0,40.125f);VectorSet(b.feet,16,0,40.125f);height=61.f;}
 if(scenario==7)water_depth=42.f;
 if(scenario==9){g_entities[100].inuse=1;g_entities[100].classname="trigger_hurt";}
 originalA=a;originalB=b;
 why=NG_Walk(&a,&b,height);
 printf("scenario%d result=%s traces=%d body=%d foot=%d\n",scenario,why?why:"accepted",ngTraces,body_calls,foot_calls);
 REQUIRE(!memcmp(&a,&originalA,sizeof(a))&&!memcmp(&b,&originalB,sizeof(b)),"exported anchors remain unchanged");
 if(scenario<=4||scenario==10||scenario==12){REQUIRE(!why,"flat/uphill/downhill/low-ceiling/stairs/descending lip remain ordinary");}
 else{REQUIRE(why,"capsule drop/gap/water/slippery slope/hurt stays rejected");}
 if(scenario==5){REQUIRE(!strcmp(why,"capsule_support_step_or_drop")||!strcmp(why,"capsule_support_gap_or_drop"),"ledge rejected by actual capsule support, not legacy foot box");}
 if(scenario==10){REQUIRE(!why,"safe real step survives higher flat-foot export anchor");}
 if(scenario==11){REQUIRE(why!=NULL,"real upward barrier remains blocked by full-body clearance or step bound");}
 if(scenario==12){REQUIRE(!why,"horizontal then down clears rounded lip without requesting ceiling headroom");}
 if(scenario==13||scenario==14){REQUIRE(refused_sweeps>0&&why!=NULL,"refused horizontal/down full capsule sweep stays blocked");}
 if(scenario==15){REQUIRE(why!=NULL,"larger drop stays blocked by full capsule support limits");}
 REQUIRE(body_calls>0,"full-body capsule geometry queried");
 return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class GeneratorCapsuleSupportNativeTests(TestCase):
    def test_actual_c_support_continuity(self):
        with tempfile.TemporaryDirectory(prefix='tce-generator-capsule-') as folder:
            directory=Path(folder)
            (directory/'main.c').write_text(program())
            (directory/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(CapsuleSupport LANGUAGES C)\nadd_executable(support main.c)\n')
            options=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake','-S',folder,'-B',str(directory/'build'),'-A','Win32'],
                            ['cmake','--build',str(directory/'build'),'--config','Release']):
                result=subprocess.run(command,**options)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            for scenario in range(16):
                with self.subTest(scenario=scenario):
                    result=subprocess.run([str(directory/'build/Release/support.exe'),str(scenario)],**options)
                    self.assertEqual(result.returncode,0,result.stdout+result.stderr)


if __name__ == '__main__':
    main()
