"""Exercise the actual NG_Trace body against deterministic engine responses.

Run from the repository root with a compiler environment, or pass --vcvars
pointing at VsDevCmd.bat. Only a tiny standalone test executable is built.
No game binary, installation, process, or navigation file is touched.
"""
import argparse
from pathlib import Path
import subprocess


PRELUDE = r'''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
typedef int qboolean;
typedef float vec3_t[3];
typedef struct { int startsolid, allsolid, entityNum, contents; float fraction; vec3_t endpos; } trace_t;
#define qfalse 0
#define qtrue 1
#define ENTITYNUM_NONE 1023
#define NG_MASK 65537
#define NG_TRACE_BUDGET 10
#define VectorSet(v,x,y,z) ((v)[0]=(x),(v)[1]=(y),(v)[2]=(z))
#define VectorCopy(a,b) memcpy((b),(a),sizeof(vec3_t))
#define VectorCompare(a,b) ((a)[0]==(b)[0]&&(a)[1]==(b)[1]&&(a)[2]==(b)[2])
static float ngWidth=16;
static int ngTraces, calls, boxCalls;
static trace_t replies[2];
static vec3_t starts[2],ends[2],hullMins[2],hullMaxs[2];
static void trap_TraceCapsule(trace_t *tr,const vec3_t a,const vec3_t mins,const vec3_t maxs,const vec3_t b,int pass,int mask) {
 int index=calls++;assert(index<2&&pass==ENTITYNUM_NONE&&mask==NG_MASK);
 VectorCopy(a,starts[index]);VectorCopy(b,ends[index]);VectorCopy(mins,hullMins[index]);VectorCopy(maxs,hullMaxs[index]);
 *tr=replies[index];VectorCopy(b,tr->endpos);
 /* Reproduce the observed broken engine zero-length special case. */
 if(VectorCompare(a,b)){tr->allsolid=tr->startsolid=1;tr->fraction=0;tr->contents=0;}
}
static void trap_Trace(trace_t *tr,const vec3_t a,const vec3_t mins,const vec3_t maxs,const vec3_t b,int pass,int mask) {
 assert(pass==ENTITYNUM_NONE&&mask==NG_MASK);assert(mins[2]==0&&maxs[2]==.25f);assert(a!=NULL);
 boxCalls++;*tr=replies[0];VectorCopy(b,tr->endpos);
}
'''

CASES = r'''
static void reset(void) {
 int i;calls=boxCalls=ngTraces=0;memset(replies,0,sizeof(replies));
 for(i=0;i<2;i++){replies[i].fraction=1;replies[i].entityNum=ENTITYNUM_NONE;}
}
int main(void) {
 vec3_t feet={-1158.5714f,982.596f,-103.75f},target;
 trace_t result;int direction,kind,checks=0;
 reset();assert(NG_Trace(&result,feet,feet,87));
 assert(calls==2&&boxCalls==0&&ngTraces==2&&!result.startsolid&&!result.allsolid&&result.fraction==1);
 assert(VectorCompare(result.endpos,feet));
 for(direction=0;direction<2;direction++) {
  assert(VectorCompare(starts[direction],feet));
  assert(hullMins[direction][0]==-16&&hullMins[direction][1]==-16&&hullMins[direction][2]==0);
  assert(hullMaxs[direction][0]==16&&hullMaxs[direction][1]==16&&hullMaxs[direction][2]==87);
  assert(fabsf(ends[direction][2]-feet[2]-(direction?-.01f:.01f))<.00002f);
 }
 checks++;
 for(direction=0;direction<2;direction++)for(kind=0;kind<4;kind++) {
  reset();replies[direction].entityNum=17;replies[direction].contents=NG_MASK;
  if(kind==0)replies[direction].fraction=.5f;
  if(kind==1)replies[direction].startsolid=1;
  if(kind==2)replies[direction].allsolid=1;
  if(kind==3)replies[direction].fraction=NAN;
  assert(NG_Trace(&result,feet,feet,87));
  assert(calls==2&&ngTraces==2&&result.startsolid&&result.allsolid&&result.fraction==0);
  assert(result.entityNum==17&&result.contents==NG_MASK&&VectorCompare(result.endpos,feet));checks++;
 }
 reset();ngTraces=NG_TRACE_BUDGET-1;assert(!NG_Trace(&result,feet,feet,87));
 assert(calls==0&&ngTraces==NG_TRACE_BUDGET-1);checks++;
 reset();ngTraces=NG_TRACE_BUDGET-2;assert(NG_Trace(&result,feet,feet,87));
 assert(calls==2&&ngTraces==NG_TRACE_BUDGET);checks++;
 reset();ngTraces=NG_TRACE_BUDGET;assert(!NG_Trace(&result,feet,feet,87));assert(calls==0);checks++;
 reset();assert(NG_Trace(&result,feet,feet,.25f));assert(boxCalls==1&&calls==0&&ngTraces==1);checks++;
 reset();VectorCopy(feet,target);target[0]+=8;replies[0].fraction=.5f;
 assert(NG_Trace(&result,feet,target,87));assert(calls==1&&ngTraces==1&&result.fraction==.5f&&!result.allsolid);checks++;
 reset();assert(NG_Trace(&result,feet,feet,61));assert(calls==2&&hullMaxs[0][2]==61);checks++;
 printf("NG_Trace production-body harness: %d cases passed\n",checks);return 0;
}
'''


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--vcvars', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[2]
    source = (root / 'src/game/tce_navgen.c').read_text()
    begin = source.index('static qboolean NG_Trace(')
    body = source[begin:source.index('static qboolean NG_Hurt(', begin)]
    directory = root / 'build/navgen-overkill/native-trace-unit'
    directory.mkdir(parents=True, exist_ok=True)
    fixture = directory / 'trace.c'
    fixture.write_text(PRELUDE + body + CASES)
    exe = directory / 'trace.exe'
    script = directory / 'run.cmd'
    lines = ['@echo off']
    if args.vcvars:
        lines += [f'call "{args.vcvars.resolve()}" -arch=x64 -host_arch=x64 >nul',
                  'if errorlevel 1 exit /b %errorlevel%']
    lines += [f'cl /nologo /W4 /WX /O2 /Fe"{exe}" /Fo"{directory / "trace.obj"}" "{fixture}"',
              'if errorlevel 1 exit /b %errorlevel%', f'"{exe}"']
    script.write_text('\n'.join(lines)+'\n')
    subprocess.run(['cmd.exe', '/d', '/c', str(script)], check=True, cwd=directory)


if __name__ == '__main__':
    main()
