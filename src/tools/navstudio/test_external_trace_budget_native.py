"""Actual count-derived native budget and unchanged fail-closed publication.

The real C budget, trace dispatcher and final-graph gate execute with native
game types. Collision callbacks are controlled receipts, not live-map proof.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
from unittest import TestCase, main, skipUnless

from test_native_probe_context import compile_native
from test_traversal_reset_native import function

ROOT = Path(__file__).resolve().parents[2]


def program():
    core = (ROOT / 'game/tce_navgen.c').read_text()
    external = (ROOT / 'game/tce_navgen_external.inc').read_text()
    names = ('NG_NODES', 'NG_EDGES', 'NG_TRACE_BASE_BUDGET', 'NG_TRACE_MAX_BUDGET',
             'NG_TRACE_NODE_ALLOWANCE', 'NG_TRACE_EDGE_ALLOWANCE', 'NG_TRACE_BUDGET', 'NG_MASK')
    defines = '\n'.join(line for line in core.splitlines()
                        if any(line.startswith('#define '+name+' ') for name in names))
    structures = [s for s in re.findall(r'typedef struct\s*\{[^}]+\}\s*\w+;', core, re.S)
                  if s.rsplit('}', 1)[1].strip(' ;') in ('ngDoorRecipe', 'ngNode', 'ngEdge')]
    return r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "@HEADER@"
'''.replace('@HEADER@', (ROOT / 'game/g_local.h').as_posix()) + defines + '\n' + '\n'.join(structures) + r'''
static ngNode ngNodes[NG_NODES];static ngEdge ngEdges[NG_EDGES];
static int ngSimpleOwner[NG_NODES];
static int ngTraceBudget=NG_TRACE_BASE_BUDGET,ngTraces,ngCount,ngEdgeCount;
static int ngExternalSuppliedCount,ngExternalSuppliedEdges;
static qboolean ngTrCacheFailed,ngExternalInputComplete;
static float ngWidth=16;
static int physicalQueries,finalizeCalls;
void QDECL G_Printf(const char *format,...) {(void)format;}
int VectorCompare(const vec3_t a,const vec3_t b) {
 return a[0]==b[0]&&a[1]==b[1]&&a[2]==b[2];
}
vec_t VectorLength(const vec3_t v) {return (vec_t)sqrt(DotProduct(v,v));}
void trap_Trace(trace_t *r,const vec3_t a,const vec3_t mins,const vec3_t maxs,
                const vec3_t b,int entity,int mask) {
 assert(mins[0]==-16&&maxs[0]==16&&entity==ENTITYNUM_NONE&&mask==NG_MASK);
 physicalQueries++;memset(r,0,sizeof(*r));r->fraction=1;VectorCopy(b,r->endpos);
}
void trap_TraceCapsule(trace_t *r,const vec3_t a,const vec3_t mins,const vec3_t maxs,
                       const vec3_t b,int entity,int mask) {
 assert(maxs[2]==87);trap_Trace(r,a,mins,maxs,b,entity,mask);
}
static qboolean NG_LocalFinalize(void) {finalizeCalls++;return qtrue;}
static qboolean NG_TrSeedSpokesValid(qboolean actual) {assert(actual);return qtrue;}
static int NG_SimpleCached(int a,int b) {(void)a;(void)b;return 1;}
''' + function(core, 'NG_ExternalTraceBudget') + '\n' + function(core, 'NG_Trace') + '\n' + function(external, 'NG_ExternalFinalGraph') + r'''
static int expected(int n,int e) {
 long long value;
 if(n<0)n=0;else if(n>NG_NODES)n=NG_NODES;
 if(e<0)e=0;else if(e>NG_EDGES)e=NG_EDGES;
 value=(long long)NG_TRACE_BASE_BUDGET+(long long)n*NG_TRACE_NODE_ALLOWANCE+(long long)e*NG_TRACE_EDGE_ALLOWANCE;
 return value>NG_TRACE_MAX_BUDGET?NG_TRACE_MAX_BUDGET:(int)value;
}
static void ranges(void) {
 const int ns[]={INT_MIN,-1,0,1,100,519,15795,15872,16384,16385,INT_MAX};
 const int es[]={INT_MIN,-1,0,1,1410,18750,79774,93750,100000,253952,262144,262145,INT_MAX};
 unsigned int random=1234567;int i,j,n,e,count=0,last;
 assert(NG_TRACE_MAX_BUDGET==32000000&&NG_TRACE_BASE_BUDGET==2000000);
 assert(NG_TRACE_NODE_ALLOWANCE==16&&NG_TRACE_EDGE_ALLOWANCE==320);
 for(i=0;i<sizeof(ns)/sizeof(ns[0]);i++)for(j=0;j<sizeof(es)/sizeof(es[0]);j++) {
  assert(NG_ExternalTraceBudget(ns[i],es[j])==expected(ns[i],es[j]));count++;
 }
 for(i=0;i<65536;i++) {
  random=random*1664525u+1013904223u;n=(int)random;
  random=random*1664525u+1013904223u;e=(int)random;
  assert(NG_ExternalTraceBudget(n,e)==expected(n,e));count++;
 }
 assert(NG_ExternalTraceBudget(0,0)==2000000);
 assert(NG_ExternalTraceBudget(519,1410)==2459504);
 assert(NG_ExternalTraceBudget(15795,79774)==27780400);
 assert(NG_ExternalTraceBudget(NG_NODES,NG_EDGES)==32000000);
 for(n=0;n<=16384;n+=256) {
  last=0;
  for(e=0;e<=NG_EDGES;e+=256) {
   int budget=NG_ExternalTraceBudget(n,e);
   assert(budget>=last&&budget>=2000000&&budget<=32000000);last=budget;
   if((long long)n*16+(long long)e*320<=6000000)
    assert(budget==2000000+n*16+e*320); /* Every old uncapped case is identical. */
   count++;
  }
 }
 printf("range/clamp/INT-boundaries/random/monotonic/old-uncapped/Bloc/cap cases %d passed\n",count);
}
static void fixture(void) {
 int i;memset(ngNodes,0,sizeof(ngNodes));memset(ngEdges,0,sizeof(ngEdges));
 ngCount=ngExternalSuppliedCount=2;ngEdgeCount=ngExternalSuppliedEdges=2;
 ngExternalInputComplete=qtrue;ngTrCacheFailed=qfalse;finalizeCalls=0;
 ngTraceBudget=NG_ExternalTraceBudget(15795,79774);ngTraces=100;
 for(i=0;i<2;i++){ngNodes[i].id=i;ngNodes[i].accepted=qtrue;ngNodes[i].reason="external_native_validated_floor";}
 ngEdges[0].from=0;ngEdges[0].to=1;ngEdges[1].from=1;ngEdges[1].to=0;
 ngEdges[0].accepted=ngEdges[1].accepted=qtrue;
}
static void gates(void) {
 trace_t result;vec3_t a={0,0,0},b={1,0,0};
 fixture();assert(NG_ExternalFinalGraph()&&finalizeCalls==1);
 fixture();ngExternalInputComplete=qfalse;ngTraces=ngTraceBudget-1;
 assert(!NG_ExternalFinalGraph()&&!finalizeCalls); /* Partial input with a spare trace still refuses. */
 fixture();ngTraces=ngTraceBudget;assert(!NG_ExternalFinalGraph()&&!finalizeCalls);
 fixture();ngTrCacheFailed=qtrue;assert(!NG_ExternalFinalGraph()&&!finalizeCalls);
 fixture();ngEdges[1].accepted=qfalse;assert(!NG_ExternalFinalGraph());
 fixture();ngNodes[0].authored=qtrue;ngNodes[0].accepted=qfalse;assert(!NG_ExternalFinalGraph());
 fixture();ngNodes[0].authored=qtrue;ngEdges[0].accepted=qfalse;assert(!NG_ExternalFinalGraph());
 fixture();physicalQueries=0;ngTraces=ngTraceBudget-1;
 assert(NG_Trace(&result,a,b,87)&&physicalQueries==1&&ngTraces==ngTraceBudget);
 assert(!NG_Trace(&result,a,b,87)&&physicalQueries==1&&ngTraces==ngTraceBudget);
 fixture();physicalQueries=0;ngTraces=ngTraceBudget-1;
 assert(!NG_Trace(&result,a,a,87)&&!physicalQueries&&ngTraces==ngTraceBudget-1);
 fixture();physicalQueries=0;ngTraces=ngTraceBudget-2;
 assert(NG_Trace(&result,a,a,87)&&physicalQueries==2&&ngTraces==ngTraceBudget);
 assert(!NG_Trace(&result,a,a,87)&&physicalQueries==2&&ngTraces==ngTraceBudget);
 puts("actual trace dispatcher and final-graph partial/exhausted/cache/ordinary/authored fail-closed gates passed");
}
int main(int argc,char **argv) {assert(argc==2);if(atoi(argv[1]))gates();else ranges();return 0;}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows native compiler required')
class ExternalTraceBudgetNativeTests(TestCase):
    def test_actual_budget_range_and_partial_validation_remain_bounded(self):
        executable, options = compile_native(self, program(), 'input-budget')
        for scenario in ('0', '1'):
            result = subprocess.run([str(executable), scenario], **options)
            self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
            self.assertIn('passed', result.stdout)


if __name__ == '__main__':
    main()
