/* Differential test of the whole callback. Only trajectory, collision and
 * renderer dependencies are controlled; original visibility/timer logic runs.
 */
#include "../cgame/cg_local.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <limits.h>

cg_t cg;
static vec3_t trajectoryResult;
static float traceFraction;
static int comparisons;
typedef struct {
    char order[4]; int count;
    trajectory_t trajectory;
    int time, angle, spline;
    vec3_t start, end, corona;
    int skip, mask, id, visible;
    float red, green, blue, scale;
} observation_t;
static observation_t observed;
static void require(int ok, const char *what) {
    if (!ok) { fprintf(stderr,"DynamiteTrail case %d: %s\n",comparisons,what); exit(1); }
}
static void record(char call) {
    require(observed.count < 3,"extra dependency call");
    observed.order[observed.count++] = call;
}
void BG_EvaluateTrajectory(const trajectory_t *tr,int time,vec3_t result,
                           qboolean angle,int spline) {
    record('T'); observed.trajectory=*tr; observed.time=time;
    observed.angle=angle; observed.spline=spline;
    VectorCopy(trajectoryResult,result);
}
static void originalTrajectory(const trajectory_t *tr,int time,vec3_t result,
                                qboolean angle,int spline,float multiplier) {
    require(multiplier==1.0f,"trajectory multiplier");
    BG_EvaluateTrajectory(tr,time,result,angle,spline);
}
void CG_Trace(trace_t *result,const vec3_t start,const vec3_t mins,
              const vec3_t maxs,const vec3_t end,int skip,int mask) {
    record('R'); require(!mins && !maxs,"trace bounds");
    VectorCopy(start,observed.start); VectorCopy(end,observed.end);
    observed.skip=skip; observed.mask=mask;
    memset(result,0,sizeof(*result)); result->fraction=traceFraction;
}
void trap_R_AddCoronaToScene(const vec3_t org,float r,float g,float b,float scale,
                             int id,qboolean visible) {
    record('C'); VectorCopy(org,observed.corona);
    observed.red=r; observed.green=g; observed.blue=b; observed.scale=scale;
    observed.id=id; observed.visible=visible;
}
typedef struct { unsigned char *address, saved[5]; } hook_t;
static void patch(hook_t *h,unsigned char *base,int offset,void *replacement) {
    DWORD protection,unused;
    h->address=base+offset; memcpy(h->saved,h->address,5);
    require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&protection),"hook protection");
    h->address[0]=0xe9; *(int *)(h->address+1)=(unsigned char *)replacement-h->address-5;
    FlushInstructionCache(GetCurrentProcess(),h->address,5);
    require(VirtualProtect(h->address,5,protection,&unused),"restore protection");
}
static void unpatch(hook_t *h) {
    DWORD protection,unused;
    require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&protection),"unhook protection");
    memcpy(h->address,h->saved,5); FlushInstructionCache(GetCurrentProcess(),h->address,5);
    require(VirtualProtect(h->address,5,protection,&unused),"unhook restore");
}
static void compare(unsigned char *base,int time,int effect1,int effect3,int team,
                    int viewCase,float fraction) {
    static centity_t ent, saved;
    observation_t expected;
    void (__cdecl *original)(centity_t *,const weaponInfo_t *)=(void *)(base+0x6e6c0);
    memset(&ent,0xa5,sizeof(ent));
    ent.currentState.number=123; ent.currentState.teamNum=team;
    ent.currentState.effect1Time=effect1; ent.currentState.effect2Time=19;
    ent.currentState.effect3Time=effect3;
    ent.currentState.pos.trType=TR_STATIONARY;
    saved=ent;
    cg.time=time;
    VectorSet(cg.refdef.vieworg,11,23,37);
    VectorSet(cg.refdef.viewaxis[0],1,0,0);
    VectorSet(trajectoryResult,111,23,31); /* +6 -> horizontal unit direction */
    switch(viewCase) {
    case 1: cg.refdef.viewaxis[0][0]=-1; break;
    case 2: cg.refdef.viewaxis[0][0]=0.49999997f; break;
    case 3: cg.refdef.viewaxis[0][0]=0.5f; break;
    case 4: cg.refdef.viewaxis[0][0]=0.50000006f; break;
    case 5: trajectoryResult[0]=11; break; /* zero direction */
    case 6: VectorSet(trajectoryResult,41,63,81); break;
    case 7: VectorSet(trajectoryResult,41,63,81);VectorSet(cg.refdef.viewaxis[0],0,0,1);break;
    case 8: VectorSet(trajectoryResult,-15,-35,-22);VectorSet(cg.refdef.viewaxis[0],0,-1,0);break;
    case 9: VectorSet(trajectoryResult,111,23,137);break;
    }
    traceFraction=fraction;
    *(int *)(base+0x407dea4)=cg.time;
    memcpy(base+0x407f01c,cg.refdef.vieworg,sizeof(vec3_t));
    memcpy(base+0x407f028,cg.refdef.viewaxis[0],sizeof(vec3_t));
    memset(&observed,0,sizeof(observed)); original(&ent,NULL); expected=observed;
    require(!memcmp(&ent,&saved,sizeof(ent)),"original entity writes");
    memset(&observed,0,sizeof(observed)); TCE_CG_DynamiteTrail(&ent,NULL);
    require(!memcmp(&observed,&expected,sizeof(observed)),"dependency arguments/order or visibility differ");
    require(!memcmp(&ent,&saved,sizeof(ent)),"reconstruction entity writes");
    ++comparisons;
}
void testDynamiteTrail(unsigned char *base) {
    const int phases[]={INT_MIN,INT_MIN+1,-55001,-20001,-5001,-1001,-999,-199,-1,
        0,1,49,50,99,100,199,200,499,500,999,1000,1001,4999,5000,5001,
        19999,20000,20001,39999,40000,40001,54999,55000,55001,
        INT_MAX/2,INT_MAX/2+1,INT_MAX-1,INT_MAX};
    const float fractions[]={0.0f,0.99999994f,1.0f,1.01f};
    hook_t hooks[3]; int a,b,c,d,e,oldTime=*(int *)(base+0x407dea4);
    unsigned char oldView[24]; memcpy(oldView,base+0x407f01c,sizeof(oldView));
    require(offsetof(centity_t,currentState)==0 && offsetof(entityState_t,pos)==12 &&
        offsetof(entityState_t,teamNum)==0x108 && offsetof(entityState_t,effect1Time)==0x10c &&
        offsetof(entityState_t,effect2Time)==0x110 && offsetof(entityState_t,effect3Time)==0x114 &&
        offsetof(trace_t,fraction)==8,"shared ABI offsets");
    patch(&hooks[0],base,0x43f0,originalTrajectory);
    patch(&hooks[1],base,0x58420,CG_Trace);
    patch(&hooks[2],base,0x65350,trap_R_AddCoronaToScene);
    for(a=0;a<2;++a) for(b=0;b<sizeof(phases)/sizeof(phases[0]);++b)
    for(c=-1;c<=5;++c) for(d=0;d<10;++d) for(e=0;e<4;++e) {
        int phase=phases[b], time=a?(int)(100000u-(unsigned int)phase):phase;
        compare(base,time,0,a?100000:0,c,d,fractions[e]);
    }
    /* Both subtraction directions wrap across the signed clock boundary. */
    compare(base,INT_MIN,INT_MAX,0,0,0,1);
    compare(base,INT_MAX,INT_MIN,0,0,0,1);
    compare(base,INT_MIN,0,INT_MAX,0,0,1);
    compare(base,INT_MAX,0,INT_MIN,0,0,1);
    for(a=2;a>=0;--a)unpatch(&hooks[a]);
    *(int *)(base+0x407dea4)=oldTime;memcpy(base+0x407f01c,oldView,sizeof(oldView));
    memset(&cg,0,sizeof(cg));
    printf("%d dynamite-trail original comparisons passed.\n",comparisons);
}
