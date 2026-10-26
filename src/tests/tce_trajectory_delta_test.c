#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <stddef.h>
#include "../game/q_shared.h"
#include "../game/bg_public.h"
#include "../game/tce_trajectory.h"

typedef struct{int kind,args[3];float values[8];}event_t;
static event_t events[16],expectedEvents[16];static int count,cases,mode,currentType;
static splinePath_t paths[2],savedPaths[2];
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"case %d type %d mode %d: %s\n",cases,currentType,mode,why);exit(1);}}
static event_t *event(int kind){event_t *e;require(count<16,"event capacity");e=&events[count++];memset(e,0,sizeof(*e));e->kind=kind;return e;}
void QDECL Com_Error(int level,const char *fmt,...){va_list ap;event_t *e=event(5);require(!strcmp(fmt,"BG_EvaluateTrajectoryDelta: unknown trType: %i"),"error text");va_start(ap,fmt);e->args[0]=level;e->args[1]=va_arg(ap,int);va_end(ap);}
void QDECL Com_Printf(const char *fmt,...){(void)fmt;}
typedef struct{unsigned char *p,saved[5];}hook_t;
static void hook(hook_t *h,unsigned char *base,int off,void *fn){DWORD p,u;h->p=base+off;memcpy(h->saved,h->p,5);require(VirtualProtect(h->p,5,PAGE_EXECUTE_READWRITE,&p),"hook");h->p[0]=0xe9;*(int *)(h->p+1)=(unsigned char *)fn-h->p-5;FlushInstructionCache(GetCurrentProcess(),h->p,5);VirtualProtect(h->p,5,p,&u);}
static void unhook(hook_t *h){DWORD p,u;require(VirtualProtect(h->p,5,PAGE_EXECUTE_READWRITE,&p),"unhook");memcpy(h->p,h->saved,5);FlushInstructionCache(GetCurrentProcess(),h->p,5);VirtualProtect(h->p,5,p,&u);}
static unsigned seed=382917;
static float sample(void){seed=seed*1664525u+1013904223u;return ((int)(seed>>16)-32768)*.03125f;}
static int nearFloat(float a,float b){return _finite(a)&&_finite(b)&&fabs(a-b)<=.0003+fabs(b)*.000005;}
static void compare(unsigned char *base,trajectory_t *tr,int time,int angle,int path,float gravity){
    struct{unsigned before;vec3_t out;unsigned after;}actual,expected;
    trajectory_t saved=*tr;int n,i,j;
    void (__cdecl *original)(const trajectory_t *,int,float *,int,int,float)=(void *)(base+0x4f90);
    currentType=tr->trType;
    memset(&actual,0x35,sizeof(actual));expected=actual;count=0;
    original(tr,time,expected.out,angle,path,gravity);n=count;memcpy(expectedEvents,events,n*sizeof(event_t));
    count=0;TCE_BG_EvaluateTrajectoryDelta(tr,time,actual.out,angle,path,gravity);
    require(actual.before==expected.before&&actual.after==expected.after,"output guards");
    for(i=0;i<3;++i)if(!nearFloat(actual.out[i],expected.out[i])){
        fprintf(stderr,"axis %d time %d start %d duration %d angle %d path %d gravity %g base %g,%g,%g delta %g,%g,%g: %.9g vs %.9g\n",i,time,tr->trTime,tr->trDuration,angle,path,gravity,tr->trBase[0],tr->trBase[1],tr->trBase[2],tr->trDelta[0],tr->trDelta[1],tr->trDelta[2],actual.out[i],expected.out[i]);require(0,"trajectory output");}
    require(n==count,"dependency count");
    for(i=0;i<n;++i){require(events[i].kind==expectedEvents[i].kind&&!memcmp(events[i].args,expectedEvents[i].args,sizeof(events[i].args)),"dependency identity/arguments");for(j=0;j<8;++j)require(nearFloat(events[i].values[j],expectedEvents[i].values[j]),"dependency floats");}
    require(!memcmp(tr,&saved,sizeof(saved)),"trajectory mutated");require(!memcmp(paths,savedPaths,sizeof(paths)),"paths mutated");++cases;
}
int main(int argc,char **argv) {
    HMODULE dll; unsigned char *base; hook_t h; trajectory_t tr;
    int a,b,c,d,e,i,time;
    const int durations[]={-500,0,1,500,1000,10000};
    const int offsets[]={-10000,-1000,-1,0,1,499,500,501,999,1000,1001,10000};
    const int starts[]={-301,0,17000001,2147483600,(-2147483647-1)};
    const float gravities[]={-2,0,.25f,1,3};
    require(argc==2,"DLL path"); dll=LoadLibraryA(argv[1]); require(dll!=NULL,"load original"); base=(unsigned char *)dll;
    hook(&h,base,0x47c30,Com_Error);
    for(a=-1;a<=18;++a)for(b=0;b<6;++b)for(c=0;c<12;++c)for(d=0;d<5;++d)for(e=0;e<30;++e){
        if(a==5&&durations[b]==0)continue;
        memset(&tr,0,sizeof(tr));tr.trType=a;tr.trTime=starts[e%5];tr.trDuration=durations[b];
        for(i=0;i<3;++i){tr.trBase[i]=sample();tr.trDelta[i]=e==0?0:sample();}
        time=(int)((unsigned)tr.trTime+(unsigned)offsets[c]);
        compare(base,&tr,time,e&1,e%3-1,gravities[d]);
    }
    unhook(&h);FreeLibrary(dll);
    printf("%d complete EvaluateTrajectoryDelta original comparisons passed.\n",cases);return 0;
}
