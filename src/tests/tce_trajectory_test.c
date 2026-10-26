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
splinePath_t *BG_GetSplineData(int number,qboolean *backwards){event_t *e=event(1);e->args[0]=number;e->args[1]=*backwards;*backwards=number<0;if(number==0)return NULL;return &paths[0];}
void BG_CalculateSpline_r(splinePath_t *path,vec3_t a,vec3_t b,float fraction){event_t *e=event(2);int i;e->args[0]=(int)(path-paths);e->values[0]=fraction;for(i=0;i<3;++i){a[i]=(i-1)*37.5f+(path==paths?0:100);b[i]=a[i]+(i+1)*19.25f+fraction*(i+1);}}
qboolean BG_TraverseSpline(float *fraction,splinePath_t **path){event_t *e=event(3);e->args[0]=(int)(*path-paths);e->values[0]=*fraction;if(!mode)return qfalse;*path=&paths[1];*fraction=.375f;return qtrue;}
void BG_LinearPathOrigin2(float radius,splinePath_t **path,float *fraction,vec3_t result,qboolean backwards){event_t *e=event(4);e->args[0]=(int)(*path-paths);e->args[1]=backwards;e->values[0]=radius;e->values[1]=*fraction;memcpy(e->values+2,result,12);result[0]+=radius;result[1]-=radius*.5f;result[2]+=radius*.25f;*path=&paths[1];*fraction=.625f;}
void QDECL Com_Error(int level,const char *fmt,...){va_list ap;event_t *e=event(5);require(!strcmp(fmt,"BG_EvaluateTrajectory: unknown trType: %i"),"error text");va_start(ap,fmt);e->args[0]=level;e->args[1]=va_arg(ap,int);va_end(ap);}
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
    void (__cdecl *original)(const trajectory_t *,int,float *,int,int,float)=(void *)(base+0x43f0);
    currentType=tr->trType;
    memset(&actual,0x35,sizeof(actual));expected=actual;count=0;
    original(tr,time,expected.out,angle,path,gravity);n=count;memcpy(expectedEvents,events,n*sizeof(event_t));
    count=0;TCE_BG_EvaluateTrajectory(tr,time,actual.out,angle,path,gravity);
    require(actual.before==expected.before&&actual.after==expected.after,"output guards");
    for(i=0;i<3;++i)if(!nearFloat(actual.out[i],expected.out[i])){
        fprintf(stderr,"axis %d time %d start %d duration %d angle %d path %d gravity %g base %g,%g,%g delta %g,%g,%g: %.9g vs %.9g\n",i,time,tr->trTime,tr->trDuration,angle,path,gravity,tr->trBase[0],tr->trBase[1],tr->trBase[2],tr->trDelta[0],tr->trDelta[1],tr->trDelta[2],actual.out[i],expected.out[i]);require(0,"trajectory output");}
    require(n==count,"dependency count");
    for(i=0;i<n;++i){require(events[i].kind==expectedEvents[i].kind&&!memcmp(events[i].args,expectedEvents[i].args,sizeof(events[i].args)),"dependency identity/arguments");for(j=0;j<8;++j)require(nearFloat(events[i].values[j],expectedEvents[i].values[j]),"dependency floats");}
    require(!memcmp(tr,&saved,sizeof(saved)),"trajectory mutated");require(!memcmp(paths,savedPaths,sizeof(paths)),"paths mutated");++cases;
}
int main(int argc,char **argv){
    HMODULE dll;unsigned char *base;hook_t h[5];trajectory_t tr;int a,b,c,d,e,f,g,i,j;
    const int offsets[]={-1000,-1,0,1,499,500,501,1000,10000};
    const int durations[]={1,500,1000,10000,-500};
    const float gravities[]={-2,0,.25f,1,3};
    const float rolls[]={-11001,-11000,-10001,-10000,-1001,-1000,-17,0,17,999,1000,1001,9999,10000,11000,11001};
    const float look[]={-350,-1,0,1,350};
    require(offsetof(splinePath_t,segments)==0x1c8&&offsetof(splinePath_t,length)==0x388,"spline layout");
    for(i=0;i<2;++i){paths[i].length=175.5f;for(j=0;j<16;++j){paths[i].segments[j].length=13.25f+j;paths[i].segments[j].start[0]=j*25.25f;paths[i].segments[j].start[1]=-j*16.5f;paths[i].segments[j].start[2]=j*7.25f;paths[i].segments[j].v_norm[0]=.26726124f;paths[i].segments[j].v_norm[1]=.53452248f;paths[i].segments[j].v_norm[2]=.8017837f;}}
    memcpy(savedPaths,paths,sizeof(paths));require(argc==2,"DLL path");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load original");base=(unsigned char *)dll;
    hook(&h[0],base,0x5c30,BG_GetSplineData);hook(&h[1],base,0x3cd0,BG_CalculateSpline_r);hook(&h[2],base,0x3e00,BG_TraverseSpline);hook(&h[3],base,0x3f70,BG_LinearPathOrigin2);hook(&h[4],base,0x47c30,Com_Error);
    for(a=-1;a<=18;++a)for(b=0;b<5;++b)for(c=0;c<9;++c)for(d=0;d<5;++d)for(e=0;e<12;++e){
        memset(&tr,0,sizeof(tr));tr.trType=a;tr.trTime=e&1?17000001:-301;tr.trDuration=durations[b];
        for(i=0;i<3;++i){tr.trBase[i]=sample();tr.trDelta[i]=e==0?0:sample();}
        mode=e&1;compare(base,&tr,tr.trTime+offsets[c],e&1,e%3-1,gravities[d]);
    }
    for(a=0;a<2;++a)for(b=0;b<16;++b)for(c=0;c<5;++c)for(d=0;d<35;++d)for(e=0;e<3;++e)for(f=0;f<2;++f)for(g=0;g<2;++g){
        memset(&tr,0,sizeof(tr));tr.trType=a?16:12;tr.trTime=1000;tr.trDuration=d==0?0:1600;tr.trBase[0]=look[c];tr.trBase[1]=rolls[b];tr.trBase[2]=37.5f;
        mode=f;compare(base,&tr,950+d*50,g,e-1,1);
    }
    for(a=4;a>=0;--a)unhook(&h[a]);FreeLibrary(dll);printf("%d complete EvaluateTrajectory original comparisons passed.\n",cases);return 0;
}
