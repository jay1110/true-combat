#include "../cgame/cg_local.h"
#include "../cgame/tce_weapon_media.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>

cg_t cg;
cgs_t cgs;
tce_weaponInfo_t tce_cg_weapons[64];
static snapshot_t snapshot;
static int cases, count, satchelCases, artilleryCases, markerCases, hiddenMineCases;
static void *activeCent, *activeMedia, *activeTrajectory;
typedef struct { int kind, a, b, c; float f[8]; refEntity_t ent; } call_t;
static call_t calls[12], expected[12];
static void require(int ok,const char *what) {
    if(!ok){fprintf(stderr,"Missile case %d: %s\n",cases,what);exit(1);}
}
static call_t *record(int kind) {
    call_t *c;require(count<12,"call overflow");c=&calls[count++];memset(c,0,sizeof(*c));c->kind=kind;return c;
}
static void trail(void *cent,void *media,int kind) {
    require(cent==activeCent,"trail cent");require(!media||media==activeMedia,"trail media");record(kind);
}
static void grenade(void *cent,void *media){trail(cent,media,1);}
static void rocket(void *cent,void *media){trail(cent,media,2);}
static void pyro(void *cent,void *media){trail(cent,media,3);}
static void dynamite(void *cent,void *media){trail(cent,media,4);}
void TCE_CG_GrenadeTrail(centity_t *c,const weaponInfo_t *w){grenade(c,(void *)w);}
void CG_RocketTrail(centity_t *c,const weaponInfo_t *w){rocket(c,(void *)w);}
void CG_PyroSmokeTrail(centity_t *c,const weaponInfo_t *w){pyro(c,(void *)w);}
void TCE_CG_DynamiteTrail(centity_t *c,const weaponInfo_t *w){dynamite(c,(void *)w);}
void trap_R_AddLightToScene(const vec3_t origin,float radius,float intensity,float r,float g,float b,qhandle_t shader,int flags) {
    call_t *c=record(5);VectorCopy(origin,c->f);c->f[3]=radius;c->f[4]=intensity;c->f[5]=r;c->f[6]=g;c->f[7]=b;c->a=shader;c->b=flags;
}
void BG_EvaluateTrajectoryDelta(const trajectory_t *tr,int time,vec3_t result,qboolean angle,int spline) {
    call_t *c=record(6);require((void *)tr==activeTrajectory,"trajectory pointer");c->a=time;c->b=angle;c->c=spline;VectorSet(result,3.25f,-7.5f,19.0f);
}
static void originalDelta(const trajectory_t *tr,int time,vec3_t result,qboolean angle,int spline,float scale) {
    require(scale==1.0f,"original trajectory scale");BG_EvaluateTrajectoryDelta(tr,time,result,angle,spline);
}
void TCE_BG_EvaluateTrajectoryDelta(const trajectory_t *tr,int time,vec3_t result,qboolean angle,int spline,float scale) {
    originalDelta(tr,time,result,angle,spline,scale);
}
void trap_S_AddLoopingSound(const vec3_t origin,const vec3_t velocity,sfxHandle_t sound,int volume,int time) {
    call_t *c=record(7);VectorCopy(origin,c->f);VectorCopy(velocity,c->f+3);c->a=sound;c->b=volume;c->c=time;
}
void trap_S_AddRealLoopingSound(const vec3_t origin,const vec3_t velocity,sfxHandle_t sound,int range,int volume,int time) {
    trap_S_AddLoopingSound(origin,velocity,sound,volume,time);calls[count-1].kind=8;calls[count-1].f[6]=(float)range;
}
void CG_AddRefEntityWithPowerups(refEntity_t *ent,int powerups,int team,entityState_t *es,const vec3_t dir) {
    call_t *c=record(9);require((void *)es==activeCent,"powerups state");c->ent=*ent;c->a=powerups;c->b=team;VectorCopy(dir,c->f);
}
void TCE_CG_DrawSmokeGrenade(centity_t *cent){require((void *)cent==activeCent,"smoke cent");record(10);}
typedef struct {unsigned char *address,saved[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int offset,void *replacement) {
    DWORD protection,unused;h->address=base+offset;memcpy(h->saved,h->address,5);
    require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&protection),"hook protection");
    h->address[0]=0xe9;*(int *)(h->address+1)=(unsigned char *)replacement-h->address-5;
    FlushInstructionCache(GetCurrentProcess(),h->address,5);VirtualProtect(h->address,5,protection,&unused);
}
static void unhook(hook_t *h) {
    DWORD p,u;require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&p),"unhook");memcpy(h->address,h->saved,5);
    FlushInstructionCache(GetCurrentProcess(),h->address,5);VirtualProtect(h->address,5,p,&u);
}
static void compare(unsigned char *base,int weapon,int etype,int mode,int variant,int flytime) {
    static centity_t cent,savedCent;
    static unsigned char raw[0xa94],savedRaw[0xa94],snap[0x500];
    tce_weaponInfo_t *media=&tce_cg_weapons[weapon<64?weapon:0],savedMedia;
    unsigned char *originalMedia=base+0x484a080+(weapon<64?weapon:0)*0xfd0;
    void (__cdecl *original)(void *)=(void *)(base+0x30a10);
    void *callbacks[]={NULL,grenade,rocket,pyro,dynamite};
    int n,i,j,k,client=variant&1?2:3,team=mode%4,now=variant&2?-101:1001;
    memset(&cent,0,sizeof(cent));memset(raw,0,sizeof(raw));memset(snap,0,sizeof(snap));memset(media,0,sizeof(*media));
#define FIELD(field,off,value) cent.currentState.field=*(int *)(raw+(off))=(value)
    FIELD(number,0,7);FIELD(weapon,0xe8,weapon);FIELD(eType,4,etype);
    FIELD(clientNum,0xac,client);FIELD(teamNum,0x108,mode<12?mode-2:2);
    FIELD(otherEntityNum2,0x90,variant%3);FIELD(otherEntityNum,0x8c,variant&4?17:0);
    FIELD(modelindex,0xa4,5);FIELD(modelindex2,0xa8,mode==12);FIELD(density,0xf4,mode==13?3:0);
    FIELD(effect1Time,0x10c,variant&8);FIELD(powerups,0xe4,3);FIELD(time,0x54,-13);
    FIELD(pos.trType,0xc,variant%2?TR_STATIONARY:TR_LINEAR);FIELD(pos.trTime,0x10,now-flytime);FIELD(frame,0xb0,variant);
#undef FIELD
    VectorSet(cent.currentState.angles,7,8,-11);memcpy(raw+0x74,cent.currentState.angles,12);
    VectorSet(cent.lerpOrigin,64,32,16);memcpy(raw+0x710,cent.lerpOrigin,12);
    if(variant%3==1)VectorCopy(cent.lerpOrigin,cent.rawOrigin);
    if(variant%3==2)VectorSet(cent.rawOrigin,17,-32,48);
    memcpy(raw+0x6f8,cent.rawOrigin,12);
    if(variant&1)VectorSet(cent.currentState.pos.trDelta,12,-5,8);
    memcpy(raw+0x24,cent.currentState.pos.trDelta,12);
    VectorSet(cent.currentState.pos.trBase,15,7,9);memcpy(raw+0x18,cent.currentState.pos.trBase,12);
    cent.lerpFrame.frame=*(int *)(raw+0x7c0)=19;cent.lerpFrame.frameTime=*(int *)(raw+0x7c4)=now-51;
    cent.lerpFrame.oldFrame=*(int *)(raw+0x7b4)=18;cent.lerpFrame.oldFrameTime=*(int *)(raw+0x7b8)=now-101;
    cg.time=*(int *)(base+0x407dea4)=now;cg.clientFrame=*(int *)(base+0x40352a0)=variant;
    cg.clientNum=*(int *)(base+0x40352a4)=1;
    snapshot.ps.clientNum=*(int *)(snap+0xcc)=2;cg.snap=&snapshot;*(void **)(base+0x40352c4)=snap;
    cgs.clientinfo[1].team=*(int *)(base+0x2580a98+0x1c0)=team;
    cgs.clientinfo[2].team=*(int *)(base+0x2580a98+2*0x1c0)=variant%3;
    cgs.clientinfo[2].skill[SK_BATTLE_SENSE]=*(int *)(base+0x2580af0+2*0x1c0)=mode&1?4:3;
    VectorCopy(cent.lerpOrigin,cg.predictedPlayerEntity.lerpOrigin);
    cg.predictedPlayerEntity.lerpOrigin[0]+=variant%3==0?255:variant%3==1?256:257;
    memcpy(base+0x407eb7c,cg.predictedPlayerEntity.lerpOrigin,12);
    cgs.gameModels[5]=*(int *)(base+0x257e54c+20)=123;
    cgs.media.flamebarrel=*(int *)(base+0x2588498)=124;
    cgs.media.genericConstructionShader=*(int *)(base+0x2588584)=125;
    cg.satchelCharge=NULL;*(void **)(base+0x408adb8)=NULL;
    memset(cg.artilleryRequestPos,0,sizeof(cg.artilleryRequestPos));memset(base+0x40a2468,0,sizeof(cg.artilleryRequestPos));
    memset(cg.artilleryRequestTime,0,sizeof(cg.artilleryRequestTime));memset(base+0x40a2768,0,sizeof(cg.artilleryRequestTime));
    media->missileModel=variant&1?0:101;media->missileAlliedSkin=102;media->missileAxisSkin=103;
    media->modModels[0]=104;media->modModels[1]=105;media->missileSound=variant&2?0:106;media->spindownSound=107;
    media->missileRenderfx=variant&8?0x123400:0;media->missileTrail=variant%5;
    media->missileDlight=variant&2?0:57;VectorSet(media->missileDlightColor,0.2f,0.3f,0.4f);
    savedMedia=*media;memcpy(originalMedia,media,sizeof(*media));*(void **)(originalMedia+0xea0)=callbacks[media->missileTrail];
    savedCent=cent;memcpy(savedRaw,raw,sizeof(raw));
    activeCent=raw;activeMedia=originalMedia;activeTrajectory=raw+0xc;count=0;
    original(raw);n=count;memcpy(expected,calls,sizeof(calls));
    activeCent=&cent;activeMedia=media;activeTrajectory=&cent.currentState.pos;count=0;
    TCE_CG_Missile(&cent);require(count==n,"call count");
    for(i=0;i<count;++i){
        for(j=0;j<3;++j)for(k=0;k<3;++k){require(fabs(calls[i].ent.axis[j][k]-expected[i].ent.axis[j][k])<0.000003,"render axis");calls[i].ent.axis[j][k]=expected[i].ent.axis[j][k];}
        require(fabs(calls[i].ent.backlerp-expected[i].ent.backlerp)<0.000002,"marker backlerp");calls[i].ent.backlerp=expected[i].ent.backlerp;
        if(memcmp(&calls[i],&expected[i],sizeof(call_t))){fprintf(stderr,"weapon=%d type=%d mode=%d variant=%d fly=%d call=%d kind=%d/%d volume=%d/%d\n",weapon,etype,mode,variant,flytime,i,calls[i].kind,expected[i].kind,calls[i].b,expected[i].b);require(0,"call contents/order");}
    }
#define STATE(field,off) require(!memcmp(&cent.field,raw+(off),sizeof(cent.field)),#field); savedCent.field=cent.field;memcpy(savedRaw+(off),raw+(off),sizeof(cent.field))
    STATE(currentState.weapon,0xe8);
    STATE(lerpFrame.oldFrame,0x7b4);STATE(lerpFrame.oldFrameTime,0x7b8);STATE(lerpFrame.frame,0x7c0);STATE(lerpFrame.frameTime,0x7c4);
#undef STATE
    require(fabs(cent.lerpFrame.backlerp-*(float *)(raw+0x7cc))<0.000002,"frame backlerp");savedCent.lerpFrame.backlerp=cent.lerpFrame.backlerp;memcpy(savedRaw+0x7cc,raw+0x7cc,4);
    require(!memcmp(cent.rawOrigin,raw+0x6f8,12)&&!memcmp(cent.lerpAngles,raw+0x71c,12),"origins/angles");
    VectorCopy(cent.rawOrigin,savedCent.rawOrigin);VectorCopy(cent.lerpAngles,savedCent.lerpAngles);memcpy(savedRaw+0x6f8,raw+0x6f8,12);memcpy(savedRaw+0x71c,raw+0x71c,12);
    require(!memcmp(&cent,&savedCent,sizeof(cent))&&!memcmp(raw,savedRaw,sizeof(raw)),"extra entity writes");
    if(cg.satchelCharge==&cent)++satchelCases;
    if(cg.artilleryRequestTime[client])++artilleryCases;
    if(weapon==26 && count==0)++hiddenMineCases;
    for(i=0;i<count;++i)if(calls[i].kind==9 && (calls[i].ent.hModel==104||calls[i].ent.hModel==105))++markerCases;
    require((cg.satchelCharge==&cent)==(*(void **)(base+0x408adb8)==raw),"satchel tracking");
    require(!memcmp(cg.artilleryRequestPos,base+0x40a2468,sizeof(cg.artilleryRequestPos)),"artillery position");
    require(!memcmp(cg.artilleryRequestTime,base+0x40a2768,sizeof(cg.artilleryRequestTime)),"artillery time");
    require(!memcmp(media,&savedMedia,sizeof(*media)),"media mutation");++cases;
}
int main(int argc,char **argv) {
    const int types[]={3,21,22,23,24,25,27},times[]={-1,0,299,300,301,302,374,375,376,1000};
    const int special[]={15,26,27,30,55,56,60,63,65};
    hook_t hooks[7];HMODULE dll;unsigned char *base;int w,t,m,v,f;
    require(argc==2,"DLL argument");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load original");base=(unsigned char *)dll;
    hook(&hooks[0],base,0x6ca10,rocket);hook(&hooks[1],base,0x65310,trap_R_AddLightToScene);
    hook(&hooks[2],base,0x4f90,originalDelta);hook(&hooks[3],base,0x650c0,trap_S_AddLoopingSound);
    hook(&hooks[4],base,0x650f0,trap_S_AddRealLoopingSound);hook(&hooks[5],base,0x53ec0,CG_AddRefEntityWithPowerups);
    hook(&hooks[6],base,0x310a0,TCE_CG_DrawSmokeGrenade);
    for(w=0;w<64;++w)for(t=0;t<7;++t)for(m=0;m<18;++m)for(v=0;v<16;++v)compare(base,w,types[t],m,v,375);
    for(w=0;w<9;++w)for(t=0;t<7;++t)for(m=0;m<18;++m)for(v=0;v<16;++v)for(f=0;f<10;++f)compare(base,special[w],types[t],m,v,times[f]);
    require(satchelCases && artilleryCases && markerCases && hiddenMineCases,"state/visibility branch coverage");
    {
        const int invalid[]={-1,64,(-2147483647-1)};centity_t cent;
        memset(&tce_cg_weapons[0],0,sizeof(tce_cg_weapons[0]));
        for(w=0;w<3;++w){memset(&cent,0,sizeof(cent));cent.currentState.weapon=invalid[w];count=0;
            activeCent=&cent;activeTrajectory=&cent.currentState.pos;activeMedia=&tce_cg_weapons[0];
            TCE_CG_Missile(&cent);require(cent.currentState.weapon==0 && !count,"malformed weapon clamp");}
    }
    for(w=6;w>=0;--w)unhook(&hooks[w]);FreeLibrary(dll);
    printf("%d missile original comparisons passed (dependency calls captured, original mine/math executed).\n",cases);return 0;
}
