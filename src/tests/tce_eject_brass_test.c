#include "../cgame/cg_local.h"
#include "../game/tce_bg.h"
#include "../cgame/tce_eject_brass.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>

cg_t cg;cgs_t cgs;vmCvar_t cg_brassTime;vec3_t ejectBrassCasingOrigin;
static localEntity_t actual;
static unsigned char originalEntity[512];
static int originalMode,cases,contents,count,randomIndex;
static unsigned seed;
static char calls[16];static vec3_t query;
static orientation_t tag;
static const refEntity_t *expectedParent;
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"Eject brass case %d: %s\n",cases,why);exit(1);}}
static void record(char c){require(count<16,"call overflow");calls[count++]=c;}
int rand(void){record('R');++randomIndex;seed=seed*1664525u+1013904223u;return (int)(seed>>16);}
localEntity_t *CG_AllocLocalEntity(void){record('A');return originalMode?(localEntity_t *)originalEntity:&actual;}
int CG_PointContents(const vec3_t point,int skip){record('P');require(skip==-1,"contents skip");VectorCopy(point,query);return contents;}
int trap_R_LerpTag(orientation_t *out,const refEntity_t *parent,const char *name,int start){
    record('T');require(parent==expectedParent&&!strcmp(name,"tag_brass")&&start==0,"tag arguments");*out=tag;return 1;
}
void QDECL Com_Printf(const char *fmt,...) {(void)fmt;}
void QDECL Com_Error(int level,const char *fmt,...) {(void)level;fputs(fmt,stderr);exit(2);}
int Q_vsnprintf(char *out,int size,const char *fmt,va_list args){int r;
    #undef _vsnprintf
    r=_vsnprintf(out,size-1,fmt,args);out[size-1]=0;return r;
}
typedef struct{unsigned char *entry,saved[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int off,void *fn){DWORD p,u;h->entry=base+off;memcpy(h->saved,h->entry,5);require(VirtualProtect(h->entry,5,PAGE_EXECUTE_READWRITE,&p),"hook");h->entry[0]=0xe9;*(int *)(h->entry+1)=(unsigned char *)fn-h->entry-5;FlushInstructionCache(GetCurrentProcess(),h->entry,5);VirtualProtect(h->entry,5,p,&u);}
static void unhook(hook_t *h){DWORD p,u;require(VirtualProtect(h->entry,5,PAGE_EXECUTE_READWRITE,&p),"unhook");memcpy(h->entry,h->saved,5);FlushInstructionCache(GetCurrentProcess(),h->entry,5);VirtualProtect(h->entry,5,p,&u);}
static void checkFloat(float a,float b,int field){
    if(!_finite(a)||!_finite(b)||fabs(a-b)>0.0001+fabs(b)*0.000003){fprintf(stderr,"Case %d offset%x: %.9g expected %.9g\n",cases,field,a,b);exit(1);}
}
static unsigned inputSeed=0x38762519;
static float sample(void){inputSeed=inputSeed*1664525u+1013904223u;return ((int)(inputSeed>>16)-32768)/256.0f;}
static void compare(unsigned char *base,int weapon,int category,int lifetime,int local,int cached,int mask,int variant){
    static centity_t cent,savedCent;static snapshot_t snap;
    static unsigned char rawCent[0xa94],savedRaw[0xa94],rawSnap[512],savedSnap[512];
    unsigned char projected[512];localEntity_t allowed;tce_brassContext_t context,savedContext;
    refEntity_t parent,savedParent;vec3_t expectedQuery;int i,j,expectedCount,expectedRandom,enabled;
    char expectedCalls[16];unsigned initialSeed,expectedSeed;
    void (__cdecl *original)(void *,void *,int,int)=(void *)(base+0x71cb0);
    memset(&cent,0x35,sizeof(cent));memset(rawCent,0x35,sizeof(rawCent));memset(&snap,0,sizeof(snap));memset(rawSnap,0,sizeof(rawSnap));
    memset(&context,0,sizeof(context));context.weapon=weapon;context.sizeVariant=variant%3-1;
    cent.currentState.weapon=*(int *)(rawCent+0xe8)=weapon;
    memset(&parent,0x35,sizeof(parent));
    for(i=0;i<3;++i){cent.lerpAngles[i]=sample();context.entityVelocity[i]=sample()*2;snap.ps.velocity[i]=sample()*3;parent.origin[i]=sample()*4;tag.origin[i]=sample()*.03f;ejectBrassCasingOrigin[i]=sample()*5;}
    AnglesToAxis(cent.lerpAngles,parent.axis);AxisClear(tag.axis);
    memcpy(rawCent+0x71c,cent.lerpAngles,12);memcpy(rawCent+0xa84,context.entityVelocity,12);memcpy(rawSnap+0x4c,snap.ps.velocity,12);
    cg.snap=&snap;*(void **)(base+0x40352c4)=rawSnap;
    memcpy(base+0x18531b0,ejectBrassCasingOrigin,12);
    *(int *)(base+0x3fdf380)=context.sizeVariant;
    cg.time=*(int *)(base+0x407dea4)=variant&1?17000001:-301;
    cg_brassTime.integer=*(int *)(base+0x4b3292c)=lifetime;
    memset(&weaponDef[weapon],0x35,sizeof(tce_weaponDef_t));weaponDef[weapon].weaponCategory=category;
    memcpy(base+0x4c1df20+weapon*0x1cc,&weaponDef[weapon],sizeof(tce_weaponDef_t));
    cgs.media.smallgunBrassModel=*(int *)(base+0x25880d0)=variant%3?913:0;
    cgs.media.machinegunBrassModel=*(int *)(base+0x25880c8)=variant%3?914:0;
    cgs.media.shotgunBrassModel=*(int *)(base+0x25880d4)=variant%3?915:0;
    savedCent=cent;savedContext=context;savedParent=parent;memcpy(savedRaw,rawCent,sizeof(rawCent));memcpy(savedSnap,rawSnap,sizeof(rawSnap));
    memset(originalEntity,variant&1?0x5a:0,sizeof(originalEntity));memset(&actual,variant&1?0x5a:0,sizeof(actual));
    contents=mask;expectedParent=&parent;initialSeed=variant*9137u+weapon*173u+1u;
    originalMode=1;count=randomIndex=0;memset(calls,0,sizeof(calls));VectorClear(query);seed=initialSeed;
    original(&parent,rawCent,local,cached);expectedCount=count;expectedRandom=randomIndex;expectedSeed=seed;memcpy(expectedCalls,calls,sizeof(calls));VectorCopy(query,expectedQuery);
    originalMode=0;count=randomIndex=0;memset(calls,0,sizeof(calls));VectorClear(query);seed=initialSeed;
    TCE_CG_AddEjectBrass(&parent,&cent,&context,local,cached);
    enabled=lifetime>0&&category>0;
    require(count==expectedCount&&randomIndex==expectedRandom&&seed==expectedSeed&&!memcmp(calls,expectedCalls,sizeof(calls)),"dependency/RNG order");
    require(randomIndex==(enabled?7:0),"complete random consumption");
    for(i=0;i<3;++i)checkFloat(query[i],expectedQuery[i],i);
    memset(projected,variant&1?0x5a:0,sizeof(projected));memset(&allowed,variant&1?0x5a:0,sizeof(allowed));
#define PROJECT(field,off) memcpy(projected+(off),&actual.field,sizeof(actual.field));memcpy(&allowed.field,&actual.field,sizeof(actual.field))
    PROJECT(leType,8);PROJECT(leFlags,12);PROJECT(startTime,16);PROJECT(endTime,20);
    PROJECT(pos.trType,0x20);PROJECT(pos.trTime,0x24);PROJECT(pos.trBase,0x2c);PROJECT(pos.trDelta,0x38);
    PROJECT(angles.trType,0x44);PROJECT(angles.trTime,0x48);PROJECT(angles.trBase,0x50);PROJECT(angles.trDelta,0x5c);
    PROJECT(bounceFactor,0x68);PROJECT(leMarkType,0x94);PROJECT(leBounceSoundType,0x98);
    PROJECT(refEntity.hModel,0xa4);PROJECT(refEntity.axis,0xb8);PROJECT(refEntity.origin,0x104);
    PROJECT(onFireEnd,0x1a8);PROJECT(sizeScale,0x1bc);
#undef PROJECT
    require(!memcmp(&allowed,&actual,sizeof(actual)),"unexpected SDK entity writes");
    for(i=0;i<sizeof(projected);i+=4){
        int floating=(i>=0x2c&&i<=0x40)||(i>=0x50&&i<=0x68)||(i>=0xb8&&i<=0xd8)||(i>=0x104&&i<=0x10c)||i==0x1bc;
        if(floating)checkFloat(*(float *)(projected+i),*(float *)(originalEntity+i),i);
        else if(memcmp(projected+i,originalEntity+i,4)){fprintf(stderr,"Case%d offset%x: %d expected%d\n",cases,i,*(int *)(projected+i),*(int *)(originalEntity+i));require(0,"integer/untouched original field");}
    }
    require(!memcmp(&parent,&savedParent,sizeof(parent))&&!memcmp(&cent,&savedCent,sizeof(cent))&&!memcmp(&context,&savedContext,sizeof(context)),"port inputs changed");
    require(!memcmp(rawCent,savedRaw,sizeof(rawCent))&&!memcmp(rawSnap,savedSnap,sizeof(rawSnap)),"original inputs changed");
    require(!memcmp(base+0x4c1df20+weapon*0x1cc,&weaponDef[weapon],sizeof(tce_weaponDef_t)),"weapon definition changed");
    ++cases;
}
int main(int argc,char **argv){
    const int categories[]={-1,0,1,2,3,4,5,6,99},lifetimes[]={-1,0,1,3,4,4000},masks[]={0,CONTENTS_WATER,CONTENTS_SLIME,CONTENTS_LAVA,CONTENTS_WATER|CONTENTS_SLIME};
    HMODULE dll;unsigned char *base;hook_t h[4];int a,b,c,d,e,f,w;
    require(argc==2,"DLL argument");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load original");base=(unsigned char *)dll;
    hook(&h[0],base,0x44900,CG_AllocLocalEntity);hook(&h[1],base,0x885b6,rand);hook(&h[2],base,0x587d0,CG_PointContents);hook(&h[3],base,0x65510,trap_R_LerpTag);
    for(a=0;a<9;++a)for(b=0;b<6;++b)for(c=0;c<2;++c)for(d=0;d<2;++d)for(e=0;e<5;++e)for(f=0;f<6;++f)
        compare(base,33,categories[a],lifetimes[b],c,d,masks[e],f);
    for(w=0;w<64;++w)for(a=0;a<512;++a)compare(base,w,categories[a%9],lifetimes[(a/9)%6],a&1,(a>>1)&1,masks[a%5],a);
    for(a=3;a>=0;--a)unhook(&h[a]);FreeLibrary(dll);printf("%d complete AddEjectBrass original comparisons passed.\n",cases);return 0;
}
