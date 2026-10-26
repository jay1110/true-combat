/* Run hash-pinned Windows originals. Rendering, trajectory and world queries
 * are captured; wind, normalization, angles and the smoke logic run normally. */
#include "../cgame/cg_local.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>

cg_t cg;
cgs_t cgs;
vmCvar_t cg_brassTime;
vec3_t ejectBrassCasingOrigin;
static localEntity_t brassEntity;
static unsigned char originalBrass[512];
static int useOriginalBrass;
typedef struct { char kind; int integers[5]; float values[12]; } call_t;
static call_t calls[256], expected[256];
static int callCount, contents, cases;
static int rocketMode, previousContents, contentsQueries, rocketCases;
static void require(int ok,const char *why) {
    if(!ok){fprintf(stderr,"Pyro case %d: %s\n",cases,why);exit(1);}
}
static call_t *record(char kind) {
    call_t *c;require(callCount<256,"call buffer overflow");c=&calls[callCount++];
    memset(c,0,sizeof(*c));c->kind=kind;return c;
}
localEntity_t *CG_AllocLocalEntity(void) {record('A');return useOriginalBrass ? (localEntity_t *)originalBrass : &brassEntity;}
void QDECL Com_Error(int code,const char *fmt,...) {(void)code;(void)fmt;exit(2);}
void QDECL Com_Printf(const char *fmt,...) {(void)fmt;}
void BG_EvaluateTrajectory(const trajectory_t *tr,int time,vec3_t result,qboolean angle,int spline) {
    call_t *c=record('T');int i;
    c->integers[0]=time;c->integers[1]=angle;c->integers[2]=spline;
    c->integers[3]=tr->trType;c->integers[4]=tr->trTime;
    memcpy(c->values,tr->trBase,sizeof(vec3_t));memcpy(c->values+3,tr->trDelta,sizeof(vec3_t));
    for(i=0;i<3;++i)result[i]=tr->trBase[i]+time*0.125f*(i+1);
}
static void originalTrajectory(const trajectory_t *tr,int time,vec3_t result,
                               qboolean angle,int spline,float scale) {
    require(scale==1.0f,"trajectory scale");BG_EvaluateTrajectory(tr,time,result,angle,spline);
}
int CG_PointContents(const vec3_t point,int skip) {
    call_t *c=record('P');VectorCopy(point,c->values);c->integers[0]=skip;
    return rocketMode && contentsQueries++ ? previousContents : contents;
}
void CG_BubbleTrail(vec3_t start,vec3_t end,float size,float spacing) {
    call_t *c=record('B');VectorCopy(start,c->values);VectorCopy(end,c->values+3);
    c->values[6]=size;c->values[7]=spacing;
}
void CG_ParticleExplosion(char *name,vec3_t origin,vec3_t velocity,int duration,
                           int sizeStart,int sizeEnd,qboolean dlight) {
    call_t *c=record('E');VectorCopy(origin,c->values);VectorCopy(velocity,c->values+3);
    require(!strcmp(name,"twiltb2") || !strcmp(name,"blacksmokeanim"),"particle name");
    c->integers[0]=duration;c->integers[1]=sizeStart;c->integers[2]=sizeEnd;
    c->integers[3]=dlight;c->integers[4]=!strcmp(name,"blacksmokeanim");
}
void trap_S_StartSound(vec3_t origin,int entity,int channel,sfxHandle_t sound) {
    call_t *c=record('S');require(!origin,"sound origin");
    c->integers[0]=entity;c->integers[1]=channel;c->integers[2]=sound;
}
localEntity_t *CG_SmokePuff(const vec3_t origin,const vec3_t velocity,float radius,
    float r,float g,float b,float a,float duration,int start,int fade,int flags,qhandle_t shader) {
    call_t *c=record('F');VectorCopy(origin,c->values);VectorCopy(velocity,c->values+3);
    c->values[6]=radius;c->values[7]=r;c->values[8]=g;c->values[9]=b;c->values[10]=a;c->values[11]=duration;
    c->integers[0]=start;c->integers[1]=fade;c->integers[2]=flags;c->integers[3]=shader;return NULL;
}
typedef struct {unsigned char *address,saved[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int offset,void *replacement) {
    DWORD protection,unused;h->address=base+offset;memcpy(h->saved,h->address,5);
    require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&protection),"hook protection");
    h->address[0]=0xe9;*(int *)(h->address+1)=(unsigned char *)replacement-h->address-5;
    FlushInstructionCache(GetCurrentProcess(),h->address,5);
    require(VirtualProtect(h->address,5,protection,&unused),"hook restore");
}
static void unhook(hook_t *h) {
    DWORD protection,unused;
    require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&protection),"unhook protection");
    memcpy(h->address,h->saved,5);FlushInstructionCache(GetCurrentProcess(),h->address,5);
    require(VirtualProtect(h->address,5,protection,&unused),"unhook restore");
}
static int closeFloat(float a,float b) {
    return fabs((double)a-b)<=0.00002+fabs((double)b)*0.000002;
}
static void compare(unsigned char *base,int weapon,int team,int density,int other,
                     int now,int trail,int misc,float wind) {
    static centity_t ent,originalEnt;
    void (__cdecl *original)(centity_t *,const weaponInfo_t *)=(void *)(base+0x6c670);
    int count,next,i,j;unsigned int seed=cases*171u+5;
    memset(&ent,0,sizeof(ent));
    ent.currentState.number=79;ent.currentState.weapon=weapon;
    ent.currentState.teamNum=team;ent.currentState.density=density;
    ent.currentState.otherEntityNum2=other;ent.currentState.effect2Time=17;
    ent.currentState.pos.trType=TR_LINEAR;ent.currentState.pos.trTime=-15;
    VectorSet(ent.currentState.pos.trBase,13,-27,8);
    VectorSet(ent.currentState.pos.trDelta,1,2,3);
    VectorSet(ent.currentState.apos.trBase,17,-29,63);
    ent.trailTime=trail;ent.miscTime=misc;originalEnt=ent;
    cg.time=now;cgs.smokeWindDir=wind;
    *(int *)(base+0x407dea4)=now;*(float *)(base+0x3fb7824)=wind;
    *(int *)(base+0x2588188)=cgs.media.smokePuffShader=703;
    *(int *)(base+0x2588dc0)=cgs.media.minePrimedSound=809;
    contents=(cases%3==0)?CONTENTS_WATER:(cases%3==1)?CONTENTS_LAVA:CONTENTS_SOLID;
    callCount=0;srand(seed);original(&originalEnt,NULL);next=rand();
    count=callCount;memcpy(expected,calls,count*sizeof(call_t));
    callCount=0;srand(seed);CG_PyroSmokeTrail(&ent,NULL);
    require(rand()==next,"random consumption differs");require(callCount==count,"call count differs");
    require(!memcmp(&ent,&originalEnt,sizeof(ent)),"entity writes differ");
    for(i=0;i<count;++i) {
        require(calls[i].kind==expected[i].kind,"call order differs");
        require(!memcmp(calls[i].integers,expected[i].integers,sizeof(calls[i].integers)),"integer argument differs");
        for(j=0;j<12;++j) if(!closeFloat(calls[i].values[j],expected[i].values[j])) {
            fprintf(stderr,"call %d (%c) value %d: %.9g vs %.9g\n",i,calls[i].kind,j,calls[i].values[j],expected[i].values[j]);
            require(0,"float argument differs");
        }
    }
    ++cases;
}
static void compareRocket(unsigned char *base,int type,int stationary,int now,int trail,
                          int currentContents,int oldContents,int shape,unsigned int seed) {
    static centity_t ent,saved;
    static unsigned char reference[8192],before[8192];
    void (__cdecl *original)(void *,void *)=(void *)(base+0x6ca10);
    int count,next,i,j;
    memset(&ent,0,sizeof(ent));memset(reference,0,sizeof(reference));
    ent.currentState.eType=type;ent.currentState.number=79;
    ent.currentState.pos.trType=stationary?TR_STATIONARY:TR_LINEAR;
    ent.currentState.effect2Time=17;ent.currentState.pos.trTime=-15;
    VectorSet(ent.currentState.pos.trBase,13,-27,8);
    VectorSet(ent.currentState.pos.trDelta,1,2,3);
    VectorSet(ent.currentState.apos.trBase,17,-29,63);
    VectorSet(ent.lerpOrigin,211,-307,89);
    if(shape==1) {ent.currentState.density=8;VectorSet(ent.currentState.angles,321.75f,13.5f,27.9f);}
    if(shape==2) {ent.currentState.density=-12;VectorSet(ent.currentState.angles,-9.9f,-2.5f,-8.9f);}
    if(shape==3) {ent.currentState.density=3;VectorSet(ent.currentState.angles,0.9f,-0.9f,0);}
    ent.trailTime=trail;saved=ent;
    memcpy(reference,&ent.currentState,sizeof(entityState_t));
    *(int *)(reference+0x25c)=trail;
    /* Project original lerpOrigin explicitly; do not assume the full centity layout. */
    memcpy(reference+0x710,ent.lerpOrigin,sizeof(vec3_t));memcpy(before,reference,sizeof(reference));
    cg.time=*(int *)(base+0x407dea4)=now;
    contents=currentContents;previousContents=oldContents;
    callCount=contentsQueries=0;srand(seed);original(reference,NULL);next=rand();
    count=callCount;memcpy(expected,calls,count*sizeof(call_t));
    callCount=contentsQueries=0;srand(seed);CG_RocketTrail(&ent,NULL);
    if(rand()!=next || callCount!=count) {fprintf(stderr,"Rocket case %d type %d\n",rocketCases,type);require(0,"rocket random/call count");}
    saved.trailTime=now;*(int *)(before+0x25c)=now;
    require(!memcmp(&ent,&saved,sizeof(ent)),"rocket SDK entity state");
    require(!memcmp(reference,before,sizeof(reference)),"rocket original entity state");
    for(i=0;i<count;++i) {
        require(calls[i].kind==expected[i].kind,"rocket call order");
        for(j=0;j<5;++j)if(calls[i].integers[j]!=expected[i].integers[j]) {
            fprintf(stderr,"Rocket case %d type %d seed %u call %d arg %d: %d vs %d\n",
                rocketCases,type,seed,i,j,calls[i].integers[j],expected[i].integers[j]);
            require(0,"rocket integer argument");
        }
        for(j=0;j<12;++j)require(closeFloat(calls[i].values[j],expected[i].values[j]),"rocket vector/float argument");
    }
    ++rocketCases;
}
static void testRocket(unsigned char *base) {
    const int types[]={ET_MISSILE,ET_FLAMEBARREL,ET_FP_PARTS,ET_FIRE_COLUMN,ET_FIRE_COLUMN_SMOKE,ET_RAMJET};
    const int timing[][2]={{0,0},{9,0},{10,0},{29,0},{30,0},{49,0},{50,0},
        {51,0},{301,0},{10,30},{-1,-60},{0,-30}};
    const int masks[]={0,CONTENTS_WATER,CONTENTS_SLIME,CONTENTS_LAVA,CONTENTS_WATER|CONTENTS_SLIME};
    hook_t hooks[2];int a,b,c,d,e,f;
    require(ET_FLAMEBARREL==21 && ET_FP_PARTS==22 && ET_FIRE_COLUMN==23 &&
        ET_FIRE_COLUMN_SMOKE==24 && ET_RAMJET==25 && sizeof(entityState_t)==0x120 &&
        offsetof(entityState_t,angles)==0x74,"rocket ABI");
    rocketMode=1;
    hook(&hooks[0],base,0x51f70,CG_ParticleExplosion);hook(&hooks[1],base,0x2b2d0,CG_BubbleTrail);
    for(a=0;a<6;++a)for(b=0;b<2;++b)for(c=0;c<12;++c)
    for(d=0;d<5;++d)for(e=0;e<5;++e)for(f=0;f<4;++f)
        compareRocket(base,types[a],b,timing[c][0],timing[c][1],masks[d],masks[e],f,rocketCases*171u+5);
    /* Many seeds exercise exact integer thresholds in extended-precision products. */
    for(a=0;a<6;++a)for(b=0;b<32768;++b)
        compareRocket(base,types[a],0,150,149,0,0,0,b);
    unhook(&hooks[1]);unhook(&hooks[0]);rocketMode=0;
    printf("%d rocket-trail original comparisons passed.\n",rocketCases);
}
static void testPanzer(unsigned char *base) {
    const int lifetimes[]={-1,0,1,250,2500,10000};
    const int masks[]={0,CONTENTS_WATER,CONTENTS_SLIME,CONTENTS_LAVA,CONTENTS_WATER|CONTENTS_LAVA};
    const vec3_t angles[]={{0,0,0},{90,0,0},{0,90,0},{17,-29,63},{-111,237,-35}};
    hook_t allocationHook;int n=0,a,b,c,seed,i,j,count,next;
    static unsigned char entity[8192],savedEntity[8192];
    static centity_t cent,savedCent;
    static unsigned char projected[512];
    static localEntity_t allowed;
    void (__cdecl *original)(void *)=(void *)(base+0x6e480);
    hook(&allocationHook,base,0x44900,CG_AllocLocalEntity);
    for(a=0;a<6;++a)for(b=0;b<5;++b)for(c=0;c<5;++c)for(seed=0;seed<256;++seed) {
        memset(&cent,0,sizeof(cent));memset(entity,0,sizeof(entity));
        VectorSet(cent.lerpOrigin,211,-307,89);VectorCopy(angles[c],cent.lerpAngles);
        cent.currentState.apos.trBase[1]=37.5f;
        memcpy(entity,&cent.currentState,sizeof(entityState_t));
        memcpy(entity+0x710,cent.lerpOrigin,sizeof(vec3_t));
        memcpy(entity+0x71c,cent.lerpAngles,sizeof(vec3_t));
        memcpy(savedEntity,entity,sizeof(entity));savedCent=cent;
        cg.time=*(int *)(base+0x407dea4)=seed%2?17000001:-301;
        cg_brassTime.integer=*(int *)(base+0x4b3292c)=lifetimes[a];
        cgs.media.panzerfaustBrassModel=*(int *)(base+0x25880cc)=917;
        contents=masks[b];
        memset(&brassEntity,seed%2?0x5a:0,sizeof(brassEntity));
        memset(originalBrass,seed%2?0x5a:0,sizeof(originalBrass));
        useOriginalBrass=1;callCount=0;srand(seed);original(entity);next=rand();
        useOriginalBrass=0;
        count=callCount;memcpy(expected,calls,count*sizeof(call_t));
        memset(&brassEntity,seed%2?0x5a:0,sizeof(brassEntity));
        callCount=0;srand(seed);CG_PanzerFaustEjectBrass(&cent);
        require(rand()==next,"panzer random consumption");require(callCount==count,"panzer call count");
        require(!memcmp(&cent,&savedCent,sizeof(cent)) && !memcmp(entity,savedEntity,sizeof(entity)),"panzer input writes");
        for(i=0;i<count;++i) {
            require(calls[i].kind==expected[i].kind && !memcmp(calls[i].integers,expected[i].integers,sizeof(calls[i].integers)),"panzer calls");
            for(j=0;j<12;++j)require(closeFloat(calls[i].values[j],expected[i].values[j]),"panzer query origin");
        }
        memset(projected,seed%2?0x5a:0,sizeof(projected));
        memset(&allowed,seed%2?0x5a:0,sizeof(allowed));
#define PROJECT(field,offset) memcpy(projected+(offset),&brassEntity.field,sizeof(brassEntity.field)); memcpy(&allowed.field,&brassEntity.field,sizeof(brassEntity.field))
        PROJECT(leType,8);PROJECT(leFlags,12);PROJECT(startTime,16);PROJECT(endTime,20);
        PROJECT(pos.trType,0x20);PROJECT(pos.trTime,0x24);PROJECT(pos.trBase,0x2c);PROJECT(pos.trDelta,0x38);
        PROJECT(angles.trType,0x44);PROJECT(angles.trTime,0x48);PROJECT(angles.trBase,0x50);PROJECT(angles.trDelta,0x5c);
        PROJECT(bounceFactor,0x68);PROJECT(leMarkType,0x94);PROJECT(leBounceSoundType,0x98);
        PROJECT(refEntity.hModel,0xa4);PROJECT(refEntity.axis,0xb8);PROJECT(refEntity.origin,0x104);PROJECT(sizeScale,0x1bc);
#undef PROJECT
        require(!memcmp(&allowed,&brassEntity,sizeof(allowed)),"panzer unexpected SDK writes");
        for(i=0;i<sizeof(projected);i+=4) {
            int isFloat=(i>=0x2c && i<=0x40)||(i>=0x50 && i<=0x68)||
                (i>=0xb8 && i<=0xd8)||(i>=0x104 && i<=0x10c)||i==0x1bc;
            if(isFloat)require(closeFloat(*(float *)(projected+i),*(float *)(originalBrass+i)),"panzer float field");
            else if(memcmp(projected+i,originalBrass+i,4)) {
                fprintf(stderr,"Panzer case %d offset %x: %d vs %d\n",n,i,*(int *)(projected+i),*(int *)(originalBrass+i));
                require(0,"panzer integer/untouched field");
            }
        }
        ++n;
    }
    unhook(&allocationHook);printf("%d panzer-brass original comparisons passed.\n",n);
}
static void compareMachineGun(unsigned char *base,int kind,int weapon,int route,int lifetime,int mask,int oldMask,int angle,unsigned int seed) {
    const vec3_t angles[]={{0,0,0},{17,-29,63},{-111,237,-35}};
    static snapshot_t snapshot;
    static unsigned char originalSnapshot[1024],entity[8192],savedEntity[8192],projected[512];
    static centity_t cent,savedCent;
    static localEntity_t allowed;
    int next,count,i,j;
    void (__cdecl *original)(void *)=(void *)(base+(kind?0x6c0d0:0x6bdd0));
        memset(&cent,0,sizeof(cent));memset(entity,0,sizeof(entity));
        VectorSet(cent.lerpOrigin,211,-307,89);VectorCopy(angles[angle],cent.lerpAngles);
        cent.currentState.apos.trBase[1]=37.5f;
        cent.currentState.clientNum=route==1?22:21;
        cent.currentState.weapon=weapon;
        cent.currentState.eFlags=route==3?EF_MG42_ACTIVE:route==4?EF_AAGUN_ACTIVE:route==5?(EF_MG42_ACTIVE|EF_AAGUN_ACTIVE):0;
        snapshot.ps.clientNum=21;snapshot.ps.persistant[PERS_HWEAPON_USE]=route==2?1:0;
        cg.snap=&snapshot;
        *(int *)(originalSnapshot+0xcc)=21;*(int *)(originalSnapshot+0x16c)=route==2?1:0;
        *(void **)(base+0x40352c4)=originalSnapshot;
        VectorSet(ejectBrassCasingOrigin,99,45,-67);memcpy(base+0x18531b0,ejectBrassCasingOrigin,sizeof(vec3_t));
        cgs.media.smallgunBrassModel=*(int *)(base+0x25880d0)=925;
        cgs.media.machinegunBrassModel=*(int *)(base+0x25880c8)=924;
        memcpy(entity,&cent.currentState,sizeof(entityState_t));
        memcpy(entity+0x710,cent.lerpOrigin,sizeof(vec3_t));
        memcpy(entity+0x71c,cent.lerpAngles,sizeof(vec3_t));
        memcpy(savedEntity,entity,sizeof(entity));savedCent=cent;
        cg.time=*(int *)(base+0x407dea4)=seed%2?17000001:-301;
        cg_brassTime.integer=*(int *)(base+0x4b3292c)=lifetime;
        cgs.media.panzerfaustBrassModel=*(int *)(base+0x25880cc)=917;
        contents=mask;previousContents=oldMask;rocketMode=1;
        memset(&brassEntity,seed%2?0x5a:0,sizeof(brassEntity));
        memset(originalBrass,seed%2?0x5a:0,sizeof(originalBrass));
        useOriginalBrass=1;callCount=contentsQueries=0;srand(seed);original(entity);next=rand();
        useOriginalBrass=0;
        count=callCount;memcpy(expected,calls,count*sizeof(call_t));
        memset(&brassEntity,seed%2?0x5a:0,sizeof(brassEntity));
        callCount=contentsQueries=0;srand(seed);if(kind)CG_MachineGunEjectBrass(&cent);else CG_MachineGunEjectBrassNew(&cent);
        require(rand()==next,"machinegun random consumption");require(callCount==count,"machinegun call count");
        require(!memcmp(&cent,&savedCent,sizeof(cent)) && !memcmp(entity,savedEntity,sizeof(entity)),"machinegun input writes");
        for(i=0;i<count;++i) {
            require(calls[i].kind==expected[i].kind && !memcmp(calls[i].integers,expected[i].integers,sizeof(calls[i].integers)),"machinegun calls");
            for(j=0;j<12;++j)if(!closeFloat(calls[i].values[j],expected[i].values[j])) {
                fprintf(stderr,"MG kind=%d weapon=%d route=%d life=%d masks=%x/%x angle=%d seed=%u call=%d field=%d actual=%g original=%g\n",kind,weapon,route,lifetime,mask,oldMask,angle,seed,i,j,calls[i].values[j],expected[i].values[j]);
                require(0,"machinegun query origin");
            }
        }
        memset(projected,seed%2?0x5a:0,sizeof(projected));
        memset(&allowed,seed%2?0x5a:0,sizeof(allowed));
#define PROJECT(field,offset) memcpy(projected+(offset),&brassEntity.field,sizeof(brassEntity.field)); memcpy(&allowed.field,&brassEntity.field,sizeof(brassEntity.field))
        PROJECT(leType,8);PROJECT(leFlags,12);PROJECT(startTime,16);PROJECT(endTime,20);
        PROJECT(pos.trType,0x20);PROJECT(pos.trTime,0x24);PROJECT(pos.trBase,0x2c);PROJECT(pos.trDelta,0x38);
        PROJECT(angles.trType,0x44);PROJECT(angles.trTime,0x48);PROJECT(angles.trBase,0x50);PROJECT(angles.trDelta,0x5c);
        PROJECT(bounceFactor,0x68);PROJECT(leMarkType,0x94);PROJECT(leBounceSoundType,0x98);
        PROJECT(refEntity.hModel,0xa4);PROJECT(refEntity.axis,0xb8);PROJECT(refEntity.origin,0x104);PROJECT(sizeScale,0x1bc);
#undef PROJECT
        require(!memcmp(&allowed,&brassEntity,sizeof(allowed)),"machinegun unexpected SDK writes");
        for(i=0;i<sizeof(projected);i+=4) {
            int isFloat=(i>=0x2c && i<=0x40)||(i>=0x50 && i<=0x68)||
                (i>=0xb8 && i<=0xd8)||(i>=0x104 && i<=0x10c)||i==0x1bc;
            if(isFloat)require(closeFloat(*(float *)(projected+i),*(float *)(originalBrass+i)),"machinegun float field");
            else if(memcmp(projected+i,originalBrass+i,4)) {
                fprintf(stderr,"Machinegun kind %d offset %x: %d vs %d\n",kind,i,*(int *)(projected+i),*(int *)(originalBrass+i));
                require(0,"machinegun integer/untouched field");
            }
        }
    rocketMode=0;
}
static void testMachineGun(unsigned char *base) {
    const int lifetimes[]={-1,0,1,5,2500};
    const int masks[]={0,CONTENTS_WATER,CONTENTS_SLIME,CONTENTS_LAVA,CONTENTS_WATER|CONTENTS_LAVA};
    hook_t allocationHook;int kind,weapon,route,a,b,c,d,seed,n;
    void *oldSnapshot=*(void **)(base+0x40352c4);
    hook(&allocationHook,base,0x44900,CG_AllocLocalEntity);
    for(kind=0;kind<2;++kind) {
        n=0;
        for(weapon=0;weapon<(kind?64:1);++weapon)for(route=0;route<(kind?6:1);++route)
        for(a=0;a<5;++a)for(b=0;b<5;++b)for(c=0;c<5;++c)for(d=0;d<3;++d)
        for(seed=0;seed<(kind?2:256);++seed) {
            compareMachineGun(base,kind,weapon,route,lifetimes[a],masks[b],masks[c],d,seed);++n;
        }
        printf("%d machinegun-brass %s original comparisons passed.\n",n,kind?"main":"first-person");
    }
    *(void **)(base+0x40352c4)=oldSnapshot;cg.snap=NULL;unhook(&allocationHook);
}
int main(int argc,char **argv) {
    const int teams[]={-1,0,1,2,7,8,9,11,12,16},weapons[]={0,26,27},density[]={0,-12,8};
    const int timing[][3]={{0,0,0},{29,0,0},{30,0,0},{31,0,0},{301,0,0},
        {1000,970,0},{1001,970,0},{1002,970,1},{1001,970,1},{999,970,1},
        {300,330,100},{301,270,301},{301,270,302},{-1,-60,-31},{0,-30,0},{60,29,1}};
    const float winds[]={-1.0f,0,0.25f,5.0f};
    HMODULE dll;unsigned char *base;hook_t hooks[5];int a,b,c,d,e,f,i,j,n=0,next;
    void (__cdecl *originalWind)(vec3_t);
    require(argc==2,"DLL argument");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load DLL");base=(unsigned char *)dll;
    require(offsetof(centity_t,trailTime)==0x25c && offsetof(centity_t,miscTime)==0x260 &&
        offsetof(entityState_t,weapon)==0xe8 && offsetof(entityState_t,density)==0xf4 &&
        offsetof(entityState_t,otherEntityNum2)==0x90,"entity ABI");
    hook(&hooks[0],base,0x885b6,rand);
    hook(&hooks[1],base,0x43f0,originalTrajectory);
    hook(&hooks[2],base,0x587d0,CG_PointContents);
    hook(&hooks[3],base,0x64fb0,trap_S_StartSound);
    hook(&hooks[4],base,0x2b570,CG_SmokePuff);
    originalWind=(void *)(base+0x6c610);
    for(i=0;i<4096;++i)for(j=0;j<4;++j) {
        vec3_t actual,reference;
        cgs.smokeWindDir=*(float *)(base+0x3fb7824)=winds[j];
        srand(i);originalWind(reference);next=rand();srand(i);CG_GetWindVector(actual);
        require(rand()==next,"wind random consumption");
        for(a=0;a<3;++a)require(closeFloat(actual[a],reference[a]),"wind component");++n;
    }
    printf("%d wind-vector original comparisons passed.\n",n);
    for(a=0;a<3;++a)for(b=0;b<10;++b)for(c=0;c<3;++c)
    for(d=0;d<16;++d)for(e=0;e<2;++e)for(f=0;f<4;++f)
        compare(base,weapons[a],teams[b],density[c],e,timing[d][0],timing[d][1],timing[d][2],winds[f]);
    testRocket(base);
    testPanzer(base);
    testMachineGun(base);
    for(i=4;i>=0;--i)unhook(&hooks[i]);FreeLibrary(dll);
    printf("%d pyro-smoke original comparisons passed.\n",cases);return 0;
}
