"""Check shared geometry against actual native plant-query/contact statements.

These are the production C statements, not a mirrored C implementation. The
independent AABB fixture only supplies EntitiesInBox; native hints/aim rays,
animation and map traversal remain live checks.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
from goalgen import capture_contact,plant_contact


ROOT=(Path(__file__).resolve().parents[1] / "src")


def program():
    missile=(ROOT/'game/g_missile.c').read_text()
    start=missile.index('            VectorCopy(self->r.currentOrigin,origin);SnapVector(origin);')
    end=missile.index(';',missile.index('num=trap_EntitiesInBox',start))+1
    plant=missile[start:end]
    active=(ROOT/'game/g_active.c').read_text()
    start=active.index('\t\tif ((hit->r.contents & CONTENTS_TRIGGER) &&')
    end=active.index('\n\t\t}',start)+5
    contact=active[start:end]
    snap=next(line for line in (ROOT/'game/q_shared.h').read_text().splitlines()
              if re.match(r'#define\s+SnapVector\(',line))
    return r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef float vec3_t[3];
enum {MAX_GENTITIES=1024,CONTENTS_TRIGGER=1,AXIS_OBJECTIVE=1,ALLIED_OBJECTIVE=2,qtrue=1};
typedef struct {struct {vec3_t origin,mins;}ps;int tceObjectiveContact;}client_t;
typedef struct {struct {vec3_t currentOrigin,maxs;int contents;}r;
 struct {struct {vec3_t trBase;}pos;}s;client_t *client;int spawnflags;const char *classname;}entity_t;
#define VectorCopy(a,b) memcpy((b),(a),sizeof(vec3_t))
#define VectorSet(a,x,y,z) do{(a)[0]=(x);(a)[1]=(y);(a)[2]=(z);}while(0)
''' + snap + r'''
static vec3_t triggerMin,triggerMax;
static int trap_EntitiesInBox(const vec3_t mins,const vec3_t maxs,int *touch,int limit){
 int i;(void)limit;for(i=0;i<3;i++)if(mins[i]>=triggerMax[i]||maxs[i]<=triggerMin[i])return 0;
 touch[0]=7;return 1;
}
int main(int argc,char**argv){
 entity_t actor={0},objective={0},*self=&actor,*ent=&actor,*hit=&objective;
 client_t client={0};vec3_t origin,mins,maxs;int touch[MAX_GENTITIES],num,i;
 if(argc!=11)return 2;
 actor.client=&client;
 for(i=0;i<3;i++){
  actor.r.currentOrigin[i]=actor.client->ps.origin[i]=(float)atof(argv[i+1]);
  triggerMin[i]=(float)atof(argv[i+5]);triggerMax[i]=(float)atof(argv[i+8]);
  objective.r.maxs[i]=triggerMax[i];
 }
 actor.client->ps.mins[2]=-(float)atof(argv[4]);
 objective.r.contents=CONTENTS_TRIGGER;objective.spawnflags=AXIS_OBJECTIVE;objective.classname="trigger_objective_info";
''' + plant + '\n' + contact + r'''
 printf("%d\n",num>0&&client.tceObjectiveContact);return 0;
}
'''


class PlantContactTests(TestCase):
    def test_usable_perimeter_and_unusable_roof_or_solid_target(self):
        trigger=[[96,8,8],[160,56,52]];crate=[[112,8,8],[144,56,52]]
        self.assertTrue(plant_contact([88,32,0],30,trigger,target_bounds=crate))
        self.assertFalse(plant_contact([128,32,0],30,trigger,target_bounds=crate))
        self.assertFalse(plant_contact([128,32,52.25],30,trigger,target_bounds=crate))
        self.assertFalse(plant_contact([80,32,0],30,trigger,target_bounds=crate))
        tall_trigger=[[96,8,8],[160,56,96]]
        self.assertFalse(plant_contact([128,32,52.25],30,tall_trigger,target_bounds=crate))

    def test_capture_capsule_side_corner_and_other_storey_for_both_tc_hulls(self):
        trigger=[[96,8,8],[160,56,64]]
        for radius,height in ((14,70),(16,87)):
            with self.subTest(radius=radius,height=height):
                self.assertTrue(capture_contact([88,32,0],trigger,radius,height))
                self.assertTrue(capture_contact([168,32,0],trigger,radius,height))
                self.assertFalse(capture_contact([80,32,0],trigger,radius,height))
                self.assertFalse(capture_contact([84,-4,0],trigger,radius,height))
                self.assertFalse(capture_contact([128,32,80],trigger,radius,height))
                self.assertFalse(capture_contact([128,32,-height-8],trigger,radius,height))
        self.assertFalse(capture_contact([88,32,0],None))
        self.assertFalse(capture_contact([88,float('nan'),0],trigger))


@skipUnless(os.name=='nt'and shutil.which('cmake'),'Windows CMake/MSVC required')
class NativePlantGeometryTests(TestCase):
    def test_exact_actual_snapped_origin_box_and_touch_height_contract(self):
        trigger=[[96,8,32],[160,56,76]]
        feet=([88,32,0],[84,32,0],[84.999,32,0],[85,32,0],[160,32,0],
              [128,32,-21],[128,32,-19],[128,32,71.9],[128,32,72],
              [128,32,76.25],[88.9,32.5,.25],[-8.9,32,0])
        with tempfile.TemporaryDirectory(prefix='tce-plant-contact-')as folder:
            directory=Path(folder);(directory/'main.c').write_text(program())
            (directory/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(PlantContact LANGUAGES C)\nadd_executable(plant-contact main.c)\n')
            options=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake','-S',folder,'-B',str(directory/'build'),'-A','Win32'],
                            ['cmake','--build',str(directory/'build'),'--config','Release']):
                result=subprocess.run(command,**options);self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            for point in feet:
                with self.subTest(feet=point):
                    origin=[point[0],point[1],point[2]+30]
                    values=origin+[30]+trigger[0]+trigger[1]
                    result=subprocess.run([str(directory/'build/Release/plant-contact.exe'),*map(str,values)],**options)
                    self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                    self.assertEqual(bool(int(result.stdout.strip())),plant_contact(point,30,trigger))


if __name__=='__main__':main()
