#include "../cgame/cg_local.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>

cg_t cg;
cgs_t cgs;
centity_t cg_entities[MAX_GENTITIES];
static snapshot_t snapshot,savedSnapshot;
static unsigned char rawSnapshot[0x5dc+512*0x120],savedRawSnapshot[sizeof(rawSnapshot)];
static int cases,count,randomValue;
static char order[4];
static refEntity_t rendered;
static float light[8];static int lightShader,lightFlags;
static void require(int ok,const char *why){if(!ok){fprintf(stderr,"MG42 case %d: %s\n",cases,why);exit(1);}}
static void record(char kind){require(count<3,"extra effect call");order[count++]=kind;}
int rand(void){record('R');return randomValue;}
void trap_R_AddRefEntityToScene(const refEntity_t *ent){record('E');rendered=*ent;}
void trap_R_AddLightToScene(const vec3_t origin,float radius,float intensity,float r,float g,float b,qhandle_t shader,int flags){
    record('L');memcpy(light,origin,12);light[3]=radius;light[4]=intensity;light[5]=r;light[6]=g;light[7]=b;lightShader=shader;lightFlags=flags;
}
static unsigned rng=0x547db16c;
static float sample(void){rng=rng*1664525u+1013904223u;return ((int)(rng>>16)-32768)/128.0f;}
typedef struct{unsigned char *entry,saved[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int off,void *fn){DWORD p,u;h->entry=base+off;memcpy(h->saved,h->entry,5);require(VirtualProtect(h->entry,5,PAGE_EXECUTE_READWRITE,&p),"hook");h->entry[0]=0xe9;*(int *)(h->entry+1)=(unsigned char *)fn-h->entry-5;FlushInstructionCache(GetCurrentProcess(),h->entry,5);VirtualProtect(h->entry,5,p,&u);}
static void unhook(hook_t *h){DWORD p,u;require(VirtualProtect(h->entry,5,PAGE_EXECUTE_READWRITE,&p),"unhook");memcpy(h->entry,h->saved,5);FlushInstructionCache(GetCurrentProcess(),h->entry,5);VirtualProtect(h->entry,5,p,&u);}
static void checkFloat(float actual,float expected,const char *label){
    if(!_finite(actual)||!_finite(expected)||fabs(actual-expected)>0.0001+fabs(expected)*0.000002){fprintf(stderr,"MG42 case %d %s: %.9g expected %.9g\n",cases,label,actual,expected);exit(1);}
}
static void compare(unsigned char *base,int number,int pattern,int orientation,int model,int rv){
    static centity_t shooter,savedShooter,savedEntities[512];
    static unsigned char rawShooter[0xa94],savedRawShooter[0xa94],savedEntitiesRaw[512][0xa94];
    refEntity_t expectedRender;float expectedLight[8];int expectedCount,expectedShader,expectedFlags;
    int i,j,id,match=-1,active=number>0?number:0;char expectedOrder[4];
    void (__cdecl *original)(void *)=(void *)(base+0x757c0);
    memset(&snapshot,0x5a,sizeof(snapshot));memset(rawSnapshot,0x5a,sizeof(rawSnapshot));
    memset(&shooter,0x35,sizeof(shooter));memset(rawShooter,0x35,sizeof(rawShooter));
    shooter.currentState.number=*(int *)rawShooter=17;
    for(j=0;j<3;++j)shooter.lerpAngles[j]=orientation==0?0:orientation==1?(float)(90*(j+1)):sample();
    memcpy(rawShooter+0x71c,shooter.lerpAngles,12);
    snapshot.numEntities=*(int *)(rawSnapshot+0x5d8)=number;
    for(i=0;i<active;++i){
        unsigned char *raw;
        /* Non-contiguous, reverse entity IDs ensure snapshot order matters. */
        id=MAX_GENTITIES-1-i;raw=base+0x488d800+id*0xa94;
        memset(&cg_entities[id],0x72,sizeof(centity_t));memset(raw,0x72,0xa94);
        snapshot.entities[i].number=*(int *)(rawSnapshot+0x5dc+i*0x120)=id;
        cg_entities[id].currentState.eType=ET_GENERAL;*(int *)(raw+4)=0;
        cg_entities[id].currentState.otherEntityNum=*(int *)(raw+0x8c)=18;
        if(pattern==1||pattern==2||pattern==3||pattern==6){cg_entities[id].currentState.eType=ET_MG42_BARREL;*(int *)(raw+4)=44;}
        if(pattern==1||pattern==4||(pattern==2&&i==active-1)||(pattern==3&&i==active/2))
            cg_entities[id].currentState.otherEntityNum=*(int *)(raw+0x8c)=17;
        if(pattern==5){snapshot.entities[i].eType=ET_MG42_BARREL;snapshot.entities[i].otherEntityNum=17;*(int *)(rawSnapshot+0x5e0+i*0x120)=44;*(int *)(rawSnapshot+0x668+i*0x120)=17;}
        for(j=0;j<3;++j)cg_entities[id].currentState.pos.trBase[j]=sample()*4;
        memcpy(raw+0x18,cg_entities[id].currentState.pos.trBase,12);
        /* A stale interpolated origin must not replace pos.trBase. */
        VectorSet(cg_entities[id].lerpOrigin,10001,20002,30003);
        if(match<0&&cg_entities[id].currentState.eType==ET_MG42_BARREL&&cg_entities[id].currentState.otherEntityNum==17)match=i;
        savedEntities[i]=cg_entities[id];memcpy(savedEntitiesRaw[i],raw,0xa94);
    }
    savedShooter=shooter;memcpy(savedRawShooter,rawShooter,sizeof(rawShooter));savedSnapshot=snapshot;memcpy(savedRawSnapshot,rawSnapshot,sizeof(rawSnapshot));
    cg.snap=&snapshot;*(void **)(base+0x40352c4)=rawSnapshot;
    cgs.media.mg42muzzleflash=*(int *)(base+0x258849c)=model;randomValue=rv;
    count=0;memset(order,0,sizeof(order));memset(&rendered,0,sizeof(rendered));memset(light,0,sizeof(light));lightFlags=lightShader=-1;
    original(rawShooter);expectedCount=count;expectedRender=rendered;memcpy(expectedOrder,order,sizeof(order));memcpy(expectedLight,light,sizeof(light));expectedFlags=lightFlags;expectedShader=lightShader;
    count=0;memset(order,0,sizeof(order));memset(&rendered,0,sizeof(rendered));memset(light,0,sizeof(light));lightFlags=lightShader=-1;
    CG_MG42EFX(&shooter);
    require(count==expectedCount&&count==(match<0?0:3)&&!memcmp(order,expectedOrder,sizeof(order)),"effect/RNG sequence and first match");
    if(count){
        require(!memcmp(order,"ERL",3),"renderer, random, light order");
        for(i=0;i<3;++i){checkFloat(rendered.origin[i],expectedRender.origin[i],"origin");expectedRender.origin[i]=rendered.origin[i];
            for(j=0;j<3;++j){checkFloat(rendered.axis[i][j],expectedRender.axis[i][j],"axis");expectedRender.axis[i][j]=rendered.axis[i][j];}}
        for(i=0;i<8;++i)checkFloat(light[i],expectedLight[i],"light");
        require(light[4]==1.25f&&lightFlags==expectedFlags&&lightShader==expectedShader,"constant light arguments");
    }
    require(!memcmp(&rendered,&expectedRender,sizeof(rendered)),"remaining render bytes/zero initialization");
    require(!memcmp(&shooter,&savedShooter,sizeof(shooter))&&!memcmp(rawShooter,savedRawShooter,sizeof(rawShooter)),"shooter mutation");
    require(!memcmp(&snapshot,&savedSnapshot,sizeof(snapshot))&&!memcmp(rawSnapshot,savedRawSnapshot,sizeof(rawSnapshot)),"snapshot mutation");
    for(i=0;i<active;++i){id=MAX_GENTITIES-1-i;require(!memcmp(&cg_entities[id],&savedEntities[i],sizeof(centity_t))&&!memcmp(base+0x488d800+id*0xa94,savedEntitiesRaw[i],0xa94),"mount mutation");}
    ++cases;
}
int main(int argc,char **argv){
    const int numbers[]={-1,0,1,2,7,31,512},models[]={0,1,173,-1},randoms[]={0,1,31,32,127,32767,-1};
    HMODULE dll;unsigned char *base;hook_t hooks[3];int a,b,c,d,e;
    require(sizeof(refEntity_t)==236,"render ABI");require(argc==2,"DLL argument");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load DLL");base=(unsigned char *)dll;
    hook(&hooks[0],base,0x65290,trap_R_AddRefEntityToScene);hook(&hooks[1],base,0x65310,trap_R_AddLightToScene);hook(&hooks[2],base,0x885b6,rand);
    for(a=0;a<7;++a)for(b=0;b<7;++b)for(c=0;c<3;++c)for(d=0;d<4;++d)for(e=0;e<7;++e)compare(base,numbers[a],b,c,models[d],randoms[e]);
    for(a=0;a<8192;++a)compare(base,a%32,a%7,2,173,a);
    for(a=2;a>=0;--a)unhook(&hooks[a]);FreeLibrary(dll);printf("%d complete MG42-effect original comparisons passed.\n",cases);return 0;
}
