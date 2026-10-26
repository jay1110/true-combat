#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <stdio.h>
#include "../cgame/cg_local.h"

/* Compile the real reconstructed trajectory body independently of the
 * original DLL. Previously realMode forwarded to the original instead. */
#define TCE_BG_EvaluateTrajectoryDelta testRealDelta
#include "../game/tce_trajectory_delta.c"
#undef TCE_BG_EvaluateTrajectoryDelta
void QDECL Com_Error(int level,const char *fmt,...){fprintf(stderr,"unexpected trajectory error %s\n",fmt);exit(12);}
cg_t cg;
void CG_ReflectVelocity(localEntity_t *, trace_t *);
static int cases, realMode, calls, capturedTime, angleArg, splineArg;
static float gravityArg;
static trajectory_t capturedTrajectory;
static vec3_t suppliedVelocity;

static void require(int ok,const char *why) {
    if(!ok){fprintf(stderr,"case %d: %s\n",cases,why);exit(1);}
}
static void __cdecl captureDelta(const trajectory_t *tr,int time,float *result,int angle,int spline,float gravity) {
    ++calls;capturedTime=time;capturedTrajectory=*tr;angleArg=angle;splineArg=spline;gravityArg=gravity;
    memcpy(result,suppliedVelocity,12);
}
void TCE_BG_EvaluateTrajectoryDelta(const trajectory_t *tr,int time,vec3_t result,qboolean angle,int spline,float gravity) {
    if(realMode)testRealDelta(tr,time,result,angle,spline,gravity);
    else captureDelta(tr,time,result,angle,spline,gravity);
}
static unsigned seed=738291;
static float sample(void){seed=seed*1664525u+1013904223u;return ((int)(seed>>16)-32768)*.03125f;}
static void compare(unsigned char *base,int type,int mark,int solid,int entity,int frame,float fraction,float bounce,int normal,int velocity,int trajectory) {
    static const float velocities[]={-100,-40,-39.999f,0,39.999f,40,40.001f,100};
    localEntity_t le,expected;trace_t trace,savedTrace;unsigned char raw[448],projected[448];
    trajectory_t callTrajectory;int originalTime,originalCalls,i;
    void (__cdecl *original)(void *,void *)=(void *)(base+0x44f20);
    memset(&le,0x35,sizeof(le));memset(raw,0x35,sizeof(raw));memset(&trace,0,sizeof(trace));
    le.leType=type;le.leMarkType=mark;le.bounceFactor=bounce;
    le.pos.trType=trajectory;le.pos.trTime=100;le.pos.trDuration=400;
    for(i=0;i<3;++i){le.pos.trBase[i]=sample();le.pos.trDelta[i]=sample();trace.endpos[i]=sample();}
    trace.plane.normal[0]=normal==3?.6f:0;trace.plane.normal[1]=normal==3?.8f:0;
    trace.plane.normal[2]=normal==0?1:normal==1?-1:normal==4?.00001f:0;
    if(normal==5){trace.plane.normal[0]=.26726124f;trace.plane.normal[1]=.53452248f;trace.plane.normal[2]=.8017837f;}
    suppliedVelocity[0]=sample();suppliedVelocity[1]=sample();suppliedVelocity[2]=velocities[velocity];
    trace.allsolid=solid&1;trace.startsolid=(solid>>1)&1;trace.entityNum=entity;trace.fraction=fraction;
    if(realMode) { static const int times[]={-301,99,100,101,299,499,500,501,17000001}; cg.time=times[cases%9]; }
    else cg.time=cases&1?17000001:-301;
    *(int *)(base+0x407dea4)=cg.time;
    cg.frametime=*(int *)(base+0x407dea0)=frame;
    *(int *)(raw+8)=type;*(int *)(raw+0x94)=mark;*(float *)(raw+0x68)=bounce;
    memcpy(raw+0x20,&le.pos,sizeof(le.pos));expected=le;savedTrace=trace;
    calls=0;original(raw,&trace);originalTime=capturedTime;originalCalls=calls;callTrajectory=capturedTrajectory;
    if(!realMode)require(angleArg==0&&splineArg==-1&&gravityArg==1,"original dependency arguments");
    calls=0;CG_ReflectVelocity(&le,&trace);
    if(!realMode)require(calls==originalCalls&&calls==1&&capturedTime==originalTime&&angleArg==0&&splineArg==-1&&gravityArg==1&&!memcmp(&callTrajectory,&capturedTrajectory,sizeof(callTrajectory)),"trajectory call contract");
    expected.pos.trType=le.pos.trType;expected.pos.trTime=le.pos.trTime;
    memcpy(expected.pos.trBase,le.pos.trBase,12);memcpy(expected.pos.trDelta,le.pos.trDelta,12);
    require(!memcmp(&expected,&le,sizeof(le)),"unexpected SDK writes");
    memcpy(projected,raw,sizeof(raw));memcpy(projected+0x20,&le.pos,sizeof(le.pos));
    for(i=0;i<448;i+=4){
        if(i>=0x38&&i<=0x40){float a=*(float *)(projected+i),b=*(float *)(raw+i);
            require(_finite(a)&&_finite(b)&&fabs(a-b)<=.0001+fabs(b)*.000003,"reflected velocity");
        }else require(!memcmp(projected+i,raw+i,4),"trajectory type/time/base");
    }
    /* The only original writes may be type, time, base and delta. */
    memcpy(projected,raw,sizeof(raw));memcpy(projected+0x20,&expected.pos,sizeof(expected.pos));
    memset(projected,0x35,0x20);*(int *)(projected+8)=type;
    memset(projected+0x44,0x35,sizeof(projected)-0x44);*(int *)(projected+0x94)=mark;*(float *)(projected+0x68)=bounce;
    require(!memcmp(projected,raw,0x20)&&!memcmp(projected+0x44,raw+0x44,448-0x44),"original guards");
    require(!memcmp(&trace,&savedTrace,sizeof(trace)),"trace mutation");++cases;
}
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *base,*entry,saved[5];DWORD p,u;
    const int frames[]={-2,-1,0,1,16,1000},entities[]={-1,0,1021,1022,1023};
    const float fractions[]={0,.5f,.999f,1},bounces[]={0,.3f,.6f,1,1.5f};
    const int trajectories[]={0,1,2,3,5,6,7,8,10,11,12,13,14,15,16};
    int t,m,s,e,f,b,n,v,k;
    require(argc==2,"DLL path");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load original");base=(unsigned char *)dll;
    entry=base+0x4f90;memcpy(saved,entry,5);
    require(VirtualProtect(entry,5,PAGE_EXECUTE_READWRITE,&p),"hook protection");entry[0]=0xe9;*(int *)(entry+1)=(unsigned char *)captureDelta-entry-5;FlushInstructionCache(GetCurrentProcess(),entry,5);VirtualProtect(entry,5,p,&u);
    for(t=2;t<=4;++t)for(m=0;m<3;++m)for(s=0;s<4;++s)for(e=0;e<5;++e)for(f=0;f<6;++f)for(b=0;b<5;++b)for(n=0;n<6;++n)for(v=0;v<8;++v)
        compare(base,t,m,s,entities[e],frames[f],fractions[v%4],bounces[b],n,v,6);
    require(VirtualProtect(entry,5,PAGE_EXECUTE_READWRITE,&p),"restore protection");memcpy(entry,saved,5);FlushInstructionCache(GetCurrentProcess(),entry,5);VirtualProtect(entry,5,p,&u);
    realMode=1;
    for(k=0;k<15;++k)for(v=0;v<2048;++v)
        compare(base,2+v%3,v%3,v%4,entities[v%5],frames[v%6],fractions[v%4],bounces[v%5],v%6,v%8,trajectories[k]);
    FreeLibrary(dll);printf("%d complete ReflectVelocity original comparisons passed (30720 with independent real source/original trajectory evaluators).\n",cases);return 0;
}
