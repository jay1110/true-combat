#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include "../cgame/cg_local.h"

cg_t cg; cgs_t cgs; vmCvar_t cg_blood,cg_bloodTime;
typedef struct { int kind, args[8]; float values[20]; trajectory_t trajectory; } event_t;
static event_t events[4096], expectedEvents[4096];
static int count, randomIndex, seed, trailCases, markCases;
static void *expectedOwner;
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"trail %d mark %d: %s\n",trailCases,markCases,why);exit(1);}}
static event_t *event(int kind){event_t *e;require(count<4096,"event capacity");e=&events[count++];memset(e,0,sizeof(*e));e->kind=kind;return e;}
int rand(void){event_t *e=event(3);int value=(seed+randomIndex++*97)&32767;e->args[0]=value;return value;}
static void __cdecl trajectory(const trajectory_t *tr,int time,vec3_t out,int angle,int spline,float gravity){
    event_t *e=event(1);e->trajectory=*tr;e->args[0]=time;e->args[1]=angle;e->args[2]=spline;e->values[0]=gravity;
    out[0]=time*.125f;out[1]=tr->trBase[1];out[2]=tr->trDelta[2];
}
void BG_EvaluateTrajectory(const trajectory_t *tr,int time,vec3_t out,qboolean angle,int spline){trajectory(tr,time,out,angle,spline,1);}
int CG_AddTrailJunc(int head,void *owner,qhandle_t shader,int time,int type,vec3_t pos,int life,float a,float b,float w,float z,int flags,vec3_t c,vec3_t d,float ratio,float speed){
    event_t *e=event(2);require(owner==expectedOwner,"trail owner");
    e->args[0]=head;e->args[1]=shader;e->args[2]=time;e->args[3]=type;e->args[4]=life;e->args[5]=flags;
    memcpy(e->values,pos,12);e->values[3]=a;e->values[4]=b;e->values[5]=w;e->values[6]=z;
    memcpy(e->values+7,c,12);memcpy(e->values+10,d,12);e->values[13]=ratio;e->values[14]=speed;
    return (int)((unsigned)head+13u);
}
void trap_R_ProjectDecal(qhandle_t shader,int points,vec3_t *pos,vec4_t projection,vec4_t color,int life,int fade){
    event_t *e=event(4);e->args[0]=shader;e->args[1]=points;e->args[2]=life;e->args[3]=fade;
    memcpy(e->values,pos,12);memcpy(e->values+3,projection,16);memcpy(e->values+7,color,16);
}
typedef struct{unsigned char *entry,saved[5];}hook_t;
static void hook(hook_t *h,unsigned char *base,int off,void *fn){DWORD p,u;h->entry=base+off;memcpy(h->saved,h->entry,5);require(VirtualProtect(h->entry,5,PAGE_EXECUTE_READWRITE,&p),"hook");h->entry[0]=0xe9;*(int *)(h->entry+1)=(unsigned char *)fn-h->entry-5;FlushInstructionCache(GetCurrentProcess(),h->entry,5);VirtualProtect(h->entry,5,p,&u);}
static void unhook(hook_t *h){DWORD p,u;require(VirtualProtect(h->entry,5,PAGE_EXECUTE_READWRITE,&p),"unhook");memcpy(h->entry,h->saved,5);FlushInstructionCache(GetCurrentProcess(),h->entry,5);VirtualProtect(h->entry,5,p,&u);}
static void blood(unsigned char *base,int time,int frame,int enabled,float speed,int variant){
    localEntity_t le,expected;unsigned char raw[448],guard[448];int n;
    void (__cdecl *original)(void *)=(void *)(base+0x44970);
    memset(&le,0x35,sizeof(le));memset(raw,0x35,sizeof(raw));
    memset(&le.pos,0,sizeof(le.pos));le.pos.trType=TR_GRAVITY;le.pos.trTime=15;le.pos.trDuration=100;
    le.pos.trBase[1]=-37.25f;le.pos.trDelta[0]=speed;le.pos.trDelta[1]=variant==2?speed*.7f:0;le.pos.trDelta[2]=variant==2?-speed*.3f:0;
    le.headJuncIndex=variant==0?0:-19;expected=le;
    memcpy(raw+0x20,&le.pos,sizeof(le.pos));*(int *)(raw+0x190)=le.headJuncIndex;memcpy(guard,raw,sizeof(raw));
    cg.time=*(int *)(base+0x407dea4)=time;cg.frametime=*(int *)(base+0x407dea0)=frame;
    cg_blood.integer=*(int *)(base+0x40132ec)=enabled;
    cgs.media.bloodTrailShader=*(int *)(base+0x2588198)=variant?917:0;
    count=0;expectedOwner=raw;original(raw);n=count;memcpy(expectedEvents,events,n*sizeof(event_t));
    count=0;expectedOwner=&le;CG_BloodTrail(&le);
    require(n==count&&!memcmp(expectedEvents,events,n*sizeof(event_t)),"trail calls and arguments");
    expected.headJuncIndex=*(int *)(raw+0x190);require(!memcmp(&le,&expected,sizeof(le)),"SDK trail entity");
    *(int *)(guard+0x190)=le.headJuncIndex;require(!memcmp(raw,guard,sizeof(raw)),"original trail entity");++trailCases;
}
static void mark(unsigned char *base,int time,int last,int type,int life,int random,int wrapper){
    localEntity_t le,expected;trace_t trace,savedTrace;unsigned char raw[448],guard[448];int n,r,expectedLast,i;
    void (__cdecl *original)(void *,void *)=(void *)(base+0x44a60);
    memset(&le,0x35,sizeof(le));memset(raw,0x35,sizeof(raw));memset(&trace,0x35,sizeof(trace));
    le.leMarkType=type;expected=le;*(int *)(raw+0x94)=type;memcpy(guard,raw,sizeof(raw));
    trace.endpos[0]=time*.125f;trace.endpos[1]=-3.5f;trace.endpos[2]=12;savedTrace=trace;
    cg.time=*(int *)(base+0x407dea4)=time;cg_bloodTime.integer=*(int *)(base+0x401268c)=life;
    for(i=0;i<5;++i)cgs.media.bloodDotShaders[i]=*(int *)(base+0x2588454+i*4)=random&1?700+i:0;
    if(!wrapper)*(int *)(base+0x80e130)=last;
    seed=random;count=randomIndex=0;original(raw,&trace);n=count;r=randomIndex;expectedLast=*(int *)(base+0x80e130);memcpy(expectedEvents,events,n*sizeof(event_t));
    count=randomIndex=0;
    if(wrapper)CG_FragmentBounceMark(&le,&trace);else TCE_CG_FragmentBounceMark(&le,&trace,&last);
    require(n==count&&r==randomIndex&&!memcmp(expectedEvents,events,n*sizeof(event_t)),"mark calls/RNG/arguments");
    if(!wrapper)require(last==expectedLast,"mark limiter state");
    expected.leMarkType=*(int *)(raw+0x94);require(!memcmp(&le,&expected,sizeof(le)),"SDK mark entity");
    *(int *)(guard+0x94)=le.leMarkType;require(!memcmp(raw,guard,sizeof(raw)),"original mark entity");
    require(!memcmp(&trace,&savedTrace,sizeof(trace)),"trace mutation");++markCases;
}
int main(int argc,char **argv){
    const float speeds[]={0,FLT_EPSILON*.5f,FLT_EPSILON,FLT_EPSILON*2,FLT_EPSILON*11.7f,.01f,1,29.999f,30,30.001f,100,999,2999,3000,3001,1e6f};
    const int frames[]={-1,0,1,16,100,1000},times[]={-301,0,99,100,101,1000,17000001};
    const int lifeTimes[]={-1,0,1,30,2147484},offsets[]={-101,-100,-99,0,1,100};
    HMODULE dll;unsigned char *base;hook_t h[4];int a,b,c,d,e;
    require(argc==2,"DLL path");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load original");base=(unsigned char *)dll;
    hook(&h[0],base,0x43f0,trajectory);hook(&h[1],base,0x65c60,CG_AddTrailJunc);hook(&h[2],base,0x885b6,rand);hook(&h[3],base,0x64f70,trap_R_ProjectDecal);
    for(a=0;a<16;++a)for(b=0;b<6;++b)for(c=0;c<7;++c)for(d=0;d<3;++d)for(e=0;e<3;++e)blood(base,times[c],frames[b],d-1,speeds[a],e);
    for(a=0;a<7;++a)for(b=0;b<6;++b)for(c=-1;c<=2;++c)for(d=0;d<5;++d)for(e=0;e<160;++e)mark(base,times[a],times[a]+offsets[b],c,lifeTimes[d],e,0);
    /* Stateful production wrapper: repeated collisions, map-time rollback. */
    *(int *)(base+0x80e130)=0;
    for(a=0;a<4096;++a)mark(base,(a%1024)*17,0,a%4?1:0,30,a*7,1);
    for(a=3;a>=0;--a)unhook(&h[a]);FreeLibrary(dll);
    printf("%d complete BloodTrail and %d complete FragmentBounceMark original comparisons passed.\n",trailCases,markCases);return 0;
}
