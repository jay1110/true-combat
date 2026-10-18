"""Run the actual C front scheduler across its portal/fallback boundary.

Sampling, arc advancement, SCCs and commits are deliberately controlled stubs.
This is a scheduling and capacity regression, not a collision/physics or live
bot certification. A second portal commit exhausts one real production limit
or invalidates the SCC result while the first portal still has a pending
receipt. The first portal must never resume after that stop condition.
An executable with just the second-pass guards removed must fail identically.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless

try:
    from .test_traversal_reset_native import function, ROOT
except ImportError:
    from test_traversal_reset_native import function, ROOT


def program(unguarded_control=False):
    core = (ROOT / 'game/tce_navgen.c').read_text()
    traversal = (ROOT / 'game/tce_navgen_traversal.inc').read_text()
    body = function(traversal, 'NG_TrFrontJumps')
    if unguarded_control:
        start = body.index('  /* No portal ordering result is a rejection.')
        end = body.index('  /* All candidates remain scheduled.', start)
        fallback = body[start:end]
        stop_guards = ('&&\n      strcmp(ngFrontJumpStop,"graph_budget")'
                       '&&strcmp(ngFrontJumpStop,"directed_components_unavailable")')
        if fallback.count(stop_guards) != 1:
            raise AssertionError('Actual fallback stop guard changed; review negative control')
        fallback = fallback.replace(stop_guards, '', 1)
        capacity_guard = (r'   if\(ngCount>=NG_NODES-4\|\|ngEdgeCount>=NG_EDGES-8\|\|'
                          r'ngJumpProofCount>=64\|\|ngDropProofCount>=32\)'
                          r'\{ngFrontJumpStop="graph_budget";break;\}\n')
        fallback, count = re.subn(capacity_guard, '', fallback)
        if count != 1:
            raise AssertionError('Actual fallback capacity guard changed; review negative control')
        body = body[:start] + fallback + body[end:]
    definitions = []
    for source, names in ((core, ('NG_NODES', 'NG_EDGES')),
                          (traversal, ('NG_FRONT_JUMP_CASES', 'NG_FRONT_JUMP_TRACES',
                                       'NG_FRONT_JUMP_ARCS', 'NG_FRONT_JUMP_QUANTUM'))):
        for name in names:
            match = re.search(r'^#define ' + name + r' .+$', source, re.M)
            if not match:
                raise AssertionError('Actual production limit missing: ' + name)
            definitions.append(match.group())
    case_type = re.search(r'typedef struct\s*\{[^}]+\}\s*ngFrontJumpCase;', traversal)
    if not case_type:
        raise AssertionError('Actual front report structure missing')
    return r'''
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int qboolean;
typedef float vec3_t[3];
enum {qfalse=0,qtrue=1,NG_JUMP_FAILURES=14,NG_DROP_FAILURES=16};
''' + '\n'.join(definitions) + r'''
#define NG_TRACE_BUDGET 2000000
#define VectorSubtract(a,b,c) do{int v;for(v=0;v<3;v++)(c)[v]=(a)[v]-(b)[v];}while(0)
#define VectorCopy(a,b) memcpy((b),(a),sizeof(vec3_t))
#define VectorScale(a,n,b) do{int v;for(v=0;v<3;v++)(b)[v]=(a)[v]*(n);}while(0)
#define DotProduct(a,b) ((a)[0]*(b)[0]+(a)[1]*(b)[1]+(a)[2]*(b)[2])
#define G_Printf printf
typedef struct {qboolean accepted,crouch,topologyProtected;unsigned int navFlags;vec3_t feet;} ngNode;
typedef struct {int low,high;float cost;} ngFrontArcPair;
''' + case_type.group() + r'''
/* Only fields consumed by the actual scheduler and the controlled stubs are
 * represented. No claim about production ngNode ABI or movement is made. */
typedef struct {
 ngNode low[21],high[21];ngFrontArcPair arcs[21*21];
 qboolean highLaunch[21],done,found,runningOnly;
 int lc,hc,arcCount,pendingArc;
} ngFrontState;
static ngNode ngNodes[NG_NODES];
static unsigned char ngSpawnReachable[NG_NODES];
static ngFrontJumpCase ngFrontJumpCases[NG_FRONT_JUMP_CASES];
static int ngCount,ngEdgeCount,ngTraces,ngJumpProofCount,ngDropProofCount;
static int ngExternalSpatialImport=1,ngFrontJumpCaseCount,ngFrontJumpTraces;
static int ngFrontJumpArcChecks,ngFrontJumpConnected,ngJumpTraversalTraces;
static const char *ngFrontJumpStop;
static struct {int integer;} g_newbbox={1};
enum {LIMIT_JUMP=1,LIMIT_DROP,LIMIT_NODES,LIMIT_EDGES,SCC_FAILURE,LIMIT_TRACES,LIMIT_ARCS};
static int mode,firstAdvances,secondAdvances,commits,expansions,sccCalls;
static void require(int ok,const char *message){
 if(!ok){fprintf(stderr,"FRONT PHASE LIMIT %s\n",message);exit(9);}
}
static float VectorNormalize(vec3_t d){float n=sqrtf(DotProduct(d,d));int i;if(n)for(i=0;i<3;i++)d[i]/=n;return n;}
static float NG_TrGravity(void){return 1000.f;}
static float TCE_NavStandingImpulse(int profile){return profile?337.5f:270.f;}
static int NG_TrStrongComponents(int *groups){
 int i;sccCalls++;
 if(mode==SCC_FAILURE&&commits)return -1;
 for(i=0;i<ngCount;i++)groups[i]=i;
 return ngCount;
}
static qboolean NG_TrAccepted(int a,int b){(void)a;(void)b;return qfalse;}
static qboolean NG_TrSpecialInput(int a,int b){(void)a;(void)b;return qfalse;}
static qboolean NG_TrFrontPortal(int low,int high,int limit,int *lane){
 (void)low;(void)high;require(ngTraces<limit-512,"portal inside trace reserve");
 *lane=0;ngTraces++;return qtrue;
}
static int NG_TrFrontStrip(int base,const vec3_t direction,float side,float inward,
                          ngNode points[3],int limit,ngFrontJumpCase *entry){
 (void)direction;(void)side;(void)inward;(void)entry;
 require(ngTraces<limit-512,"sample inside trace reserve");
 points[0]=ngNodes[base];ngTraces++;return 1;
}
static qboolean NG_TrLaunchSeparated(const ngNode *point){(void)point;return qtrue;}
static int NG_TrFrontPairs(const ngNode *low,int lc,const ngNode *high,int hc,
 const qboolean *launch,const ngNode *lowBase,const ngNode *highBase,ngFrontArcPair *pairs){
 (void)low;(void)lc;(void)high;(void)hc;(void)launch;(void)lowBase;(void)highBase;
 pairs[0].low=pairs[0].high=0;return 1;
}
static void NG_TrFrontExpand(ngFrontState *state,ngFrontJumpCase *entry,int limit){
 (void)entry;(void)limit;expansions++;state->runningOnly=qfalse;state->done=qfalse;
}
static qboolean NG_TrFrontAdvance(ngFrontState *state,ngFrontJumpCase *entry,int limit,int quantum){
 require(quantum==8,"unchanged eight-action quantum");
 require(ngTraces<limit-512&&ngFrontJumpArcChecks<256,"advance inside original budgets");
 ngTraces++;ngFrontJumpArcChecks++;entry->arcChecks++;
 if(entry->low==0){
  firstAdvances++;
  if(firstAdvances==1){state->pendingArc=0;entry->upAccepted++;return qfalse;}
  /* If a missing guard resumes the pending first receipt, make a commit
   * possible without ever writing a real proof array out of bounds. */
  state->found=state->done=qtrue;return qtrue;
 }
 require(entry->low==2,"only two independent input fronts");
 secondAdvances++;state->found=state->done=qtrue;return qtrue;
}
static void NG_TrFrontCommit(ngFrontState *state,ngFrontJumpCase *entry,int *maxid){
 (void)maxid;require(state->found,"commit requires accepted scheduling receipt");
 commits++;ngFrontJumpConnected++;
 if(entry->low==0)return;
 require(entry->low==2&&commits==1,"second portal performs the sole allowed commit");
 switch(mode){
  case LIMIT_JUMP:ngJumpProofCount=64;break;
  case LIMIT_DROP:ngDropProofCount=32;break;
  case LIMIT_NODES:ngCount=NG_NODES-4;break;
  case LIMIT_EDGES:ngEdgeCount=NG_EDGES-8;break;
  case SCC_FAILURE:break;
  case LIMIT_TRACES:ngTraces=NG_FRONT_JUMP_TRACES-512;break;
  case LIMIT_ARCS:ngFrontJumpArcChecks=NG_FRONT_JUMP_ARCS;break;
  default:require(0,"known fixture mode");
 }
}
''' + body + r'''
int main(int argc,char **argv){int i,maxid=4;
 mode=argc>1?atoi(argv[1]):LIMIT_JUMP;
 require(NG_NODES==16384&&NG_EDGES==16384*16,"unchanged production graph capacities");
 require(NG_FRONT_JUMP_TRACES==24000&&NG_FRONT_JUMP_CASES==16&&
         NG_FRONT_JUMP_ARCS==256&&NG_FRONT_JUMP_QUANTUM==8,"unchanged production front budgets");
 ngCount=4;
 for(i=0;i<4;i++){
  ngNodes[i].accepted=qtrue;ngNodes[i].feet[0]=(i/2)*1000.f+(i%2)*80.f;
  ngNodes[i].feet[2]=(i%2)*20.f;
  ngNodes[i].topologyProtected=(i%2)!=0;ngSpawnReachable[i]=(unsigned char)(!(i%2));
 }
 NG_TrFrontJumps(4,&maxid);
 require(firstAdvances==1,"pending first portal must not resume after capacity/SCC stop");
 require(secondAdvances==1&&commits==1,"only second portal may finish before stop");
 require(expansions==0,"fallback must not resample after capacity/SCC stop");
 require(sccCalls==2,"initial and post-commit SCC calls only");
 require(ngFrontJumpCaseCount==2&&ngFrontJumpCaseCount<=16&&
         ngFrontJumpTraces<=24000&&ngFrontJumpArcChecks<=256,"all original scheduling bounds retained");
 if(mode==SCC_FAILURE)require(!strcmp(ngFrontJumpStop,"directed_components_unavailable"),"SCC failure retained");
 else if(mode==LIMIT_TRACES)require(!strcmp(ngFrontJumpStop,"trace_budget"),"trace exhaustion retained");
 else if(mode==LIMIT_ARCS)require(!strcmp(ngFrontJumpStop,"arc_budget"),"arc exhaustion retained");
 else require(!strcmp(ngFrontJumpStop,"graph_budget"),"capacity stop retained");
 printf("actual front phase limit mode %d passed\n",mode);return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class FrontPhaseLimitsNativeTests(TestCase):
    def test_actual_scheduler_stops_fallback_after_portal_exhaustion_or_scc_failure(self):
        with tempfile.TemporaryDirectory(prefix='tce-front-phase-limits-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'unguarded.c').write_text(program(unguarded_control=True))
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(FrontPhaseLimits LANGUAGES C)\n'
                'add_executable(front-phase-limits main.c)\n'
                'add_executable(unguarded-phase-control unguarded.c)\n')
            options = dict(capture_output=True, text=True, timeout=90,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for mode in range(1, 8):
                with self.subTest(mode=mode):
                    result = subprocess.run(
                        [str(directory / 'build/Release/front-phase-limits.exe'), str(mode)], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertIn('actual front phase limit mode', result.stdout)
                    if mode <= 5:
                        control = subprocess.run(
                            [str(directory / 'build/Release/unguarded-phase-control.exe'), str(mode)], **options)
                        self.assertNotEqual(control.returncode, 0, control.stdout + control.stderr)
                        self.assertIn('pending first portal must not resume', control.stderr)


if __name__ == '__main__':
    main()
