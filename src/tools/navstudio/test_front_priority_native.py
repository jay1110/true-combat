"""Compile actual bounded front-pair selection; no jump acceptance is mocked."""
import os
from pathlib import Path
import shutil,subprocess,tempfile
from unittest import TestCase,main,skipUnless
from test_traversal_reset_native import function

ROOT=Path(__file__).resolve().parents[2]

def program():
    source=(ROOT/'game/tce_navgen_traversal.inc').read_text()
    body=function(source,'NG_TrFrontJumps')
    prefix=body[:body.index(' {\n  ngFrontState *states=')]
    return r'''
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
typedef float vec3_t[3];
typedef struct{int accepted,crouch,navFlags,topologyProtected;vec3_t feet;}ngNode;
enum{NG_NODES=256,NG_FRONT_JUMP_TRACES=24000,NG_TRACE_BUDGET=2000000};
static ngNode ngNodes[NG_NODES];static unsigned char ngSpawnReachable[NG_NODES];
static int ngCount,ngTraces,ngExternalSpatialImport=1;
static struct{int integer;}g_newbbox={1};static const char*ngFrontJumpStop;
static int selectedCount,selectedLow[64],selectedHigh[64],selectedPriority[64];
static int customGroups[NG_NODES],useGroups;
enum{qfalse=0,qtrue=1};typedef int qboolean;
#define VectorSubtract(a,b,c) do{int vi;for(vi=0;vi<3;vi++)(c)[vi]=(a)[vi]-(b)[vi];}while(0)
static float NG_TrGravity(void){return 1000;}
static float TCE_NavStandingImpulse(int profile){return profile?337.5f:270.f;}
static int NG_TrStrongComponents(int*groups){int i;for(i=0;i<ngCount;i++)groups[i]=useGroups?customGroups[i]:i;return ngCount;}
static qboolean NG_TrFrontPortal(int low,int high,int limit,int *lane){(void)low;(void)high;(void)limit;*lane=0;return qfalse;}
static int NG_TrAccepted(int a,int b){(void)a;(void)b;return 0;}
static int NG_TrSpecialInput(int a,int b){(void)a;(void)b;return 0;}
static void require(int ok,const char*message){if(!ok){fprintf(stderr,"FRONT PRIORITY %s\n",message);exit(9);}}
''' + prefix + r'''
 selectedCount=count;
 for(k=0;k<count;k++){selectedLow[k]=pairs[k].low;selectedHigh[k]=pairs[k].high;selectedPriority[k]=pairs[k].priority;}
}
static void node(int id,float x,float y,float z,int reachable,int protected){
 ngNodes[id].accepted=1;ngNodes[id].feet[0]=x;ngNodes[id].feet[1]=y;ngNodes[id].feet[2]=z;
 ngSpawnReachable[id]=(unsigned char)reachable;ngNodes[id].topologyProtected=protected;
}
int main(void){int i,maxid=0;
 ngCount=99;node(0,0,0,0,1,0);
 for(i=1;i<=96;i++)node(i,40.f+i*.1f,0,20,1,0);
 node(97,240,0,20,0,1);node(98,100,0,20,0,0);
 NG_TrFrontJumps(ngCount,&maxid);
 require(selectedCount==64,"exact bounded64 selection");
 require(selectedLow[0]==0&&selectedHigh[0]==97&&selectedPriority[0]==0,"far protected objective beats96 closer pairs");
 require(selectedHigh[1]==98&&selectedPriority[1]==1,"new region bridge before reachable shortcuts");
 for(i=2;i<64;i++)require(selectedPriority[i]==2,"remaining nearest reachable fronts");
 memset(ngNodes,0,sizeof(ngNodes));memset(ngSpawnReachable,0,sizeof(ngSpawnReachable));ngCount=10;
 node(0,0,0,0,1,0);node(1,240,0,20,0,1);
 node(2,1000,0,0,1,0);node(3,1080,0,20,0,0);
 node(4,2000,0,0,0,0);node(5,2060,0,20,1,0);
 node(6,3000,0,0,1,0);node(7,3040,0,20,1,0);
 node(8,4000,0,0,0,0);node(9,4040,0,20,0,1);
 NG_TrFrontJumps(ngCount,&maxid);
 require(selectedCount==5,"separate fixture five actual fronts");
 require(selectedHigh[0]==1&&selectedPriority[0]==0,"objective first");
 require(selectedHigh[1]==5&&selectedHigh[2]==3&&selectedPriority[1]==1&&selectedPriority[2]==1,"bridge ordering by distance inside same priority");
 require(selectedHigh[3]==7&&selectedPriority[3]==2,"both reachable before disconnected pairs");
 require(selectedHigh[4]==9&&selectedPriority[4]==3,"both unreachable last despite protected high");
 memset(ngNodes,0,sizeof(ngNodes));memset(ngSpawnReachable,0,sizeof(ngSpawnReachable));ngCount=4;useGroups=1;
 node(0,0,0,0,1,0);node(1,80,0,20,0,0);node(2,240,0,20,0,1);node(3,-60,0,20,0,0);
 customGroups[0]=0;customGroups[1]=customGroups[2]=1;customGroups[3]=2;
 NG_TrFrontJumps(ngCount,&maxid);
 require(selectedCount==3&&selectedHigh[0]==1&&selectedPriority[0]==0,"ordinary entrance inherits its mission SCC priority");
 require(selectedHigh[1]==2&&selectedPriority[1]==0&&selectedHigh[2]==3&&selectedPriority[2]==1,"mission entrance component before unrelated closer floor");
 puts("actual bounded native front-priority selection passed");return 0;
}
'''


@skipUnless(os.name=='nt'and shutil.which('cmake'),'Windows CMake/MSVC required')
class FrontPriorityNativeTests(TestCase):
    def test_real_bounded_selector_keeps_protected_objective_before_96_shortcuts(self):
        with tempfile.TemporaryDirectory(prefix='tce-front-priority-')as folder:
            path=Path(folder);(path/'main.c').write_text(program())
            (path/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(FrontPriority LANGUAGES C)\nadd_executable(front-priority main.c)\n')
            options=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake','-S',folder,'-B',str(path/'build'),'-A','Win32'],
                            ['cmake','--build',str(path/'build'),'--config','Release']):
                result=subprocess.run(command,**options);self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(path/'build/Release/front-priority.exe')],**options)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('actual bounded native front-priority selection passed',result.stdout)


if __name__=='__main__':main()
