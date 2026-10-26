#include "../cgame/cg_local.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <stddef.h>
cg_t cg;
cgs_t cgs;
centity_t cg_entities[MAX_GENTITIES];
static snapshot_t snapshot;
static int cases, count;
typedef struct { int kind; unsigned char render[236]; vec3_t origin,velocity; int sound,volume,time; } call_t;
static call_t calls[4], expected[4];
static void require(int ok,const char *why) { if(!ok){fprintf(stderr,"Powerups case %d: %s\n",cases,why);exit(1);} }
void QDECL Com_Error(int code,const char *fmt,...) {exit(2);}
void QDECL Com_Printf(const char *fmt,...) {}
/* RefEntity ABI is identical here; assertions below guard every modified field. */
static void pack(unsigned char *raw,const refEntity_t *ent) { memcpy(raw,ent,sizeof(*ent)); }
static void originalRender(const void *raw) {
    call_t *c;require(count<4,"call overflow");c=&calls[count++];memset(c,0,sizeof(*c));c->kind=1;memcpy(c->render,raw,236);
}
void trap_R_AddRefEntityToScene(const refEntity_t *ent) { unsigned char raw[236];pack(raw,ent);originalRender(raw); }
void trap_S_AddLoopingSound(const vec3_t origin,const vec3_t velocity,sfxHandle_t sound,int volume,int soundTime) {
    call_t *c;require(count<4,"sound overflow");c=&calls[count++];memset(c,0,sizeof(*c));c->kind=2;
    VectorCopy(origin,c->origin);VectorCopy(velocity,c->velocity);c->sound=sound;c->volume=volume;c->time=soundTime;
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

static void compare(unsigned char *base,int local,int now,int start,int end,int mode,int direction,int match) {
    refEntity_t ent,saved;
    entityState_t es,savedEs;
    static centity_t savedCent;
    static unsigned char savedOriginalCent[0xa94];
    snapshot_t savedSnapshot;
    unsigned char savedOriginalSnapshot[0x500];
    unsigned char raw[236],savedRaw[236],rawEs[0x200],savedRawEs[0x200],rawSnap[0x500];
    unsigned char *rawCent=base+0x488d800+7*0xa94;
    vec3_t dir={0,0,0};int n,i;
    void (__cdecl *original)(void *,int,int,void *,const float *)=(void *)(base+0x53ec0);
    memset(&ent,0x5a,sizeof(ent));memset(&es,0,sizeof(es));memset(rawEs,0,sizeof(rawEs));memset(rawSnap,0,sizeof(rawSnap));
    memset(&cg_entities[7],0,sizeof(cg_entities[7]));memset(rawCent,0,0xa94);
    es.number=*(int *)rawEs=7;es.onFireStart=*(int *)(rawEs+0xfc)=start;es.onFireEnd=*(int *)(rawEs+0x100)=end;
    cg_entities[7].currentState=es;
    *(int *)rawCent=7;*(int *)(rawCent+0xfc)=start;*(int *)(rawCent+0x100)=end;
    /* Vary renderer es independently of the authoritative on-fire entity. */
    if(mode==1){es.onFireStart=*(int *)(rawEs+0xfc)=now+100;es.onFireEnd=*(int *)(rawEs+0x100)=now-100;}
    if(mode==2){es.onFireStart=*(int *)(rawEs+0xfc)=now-2000;es.onFireEnd=*(int *)(rawEs+0x100)=now+4000;}
    snapshot.ps.clientNum=*(int *)(rawSnap+0xcc)=local?7:3;
    snapshot.ps.onFireStart=*(int *)(rawSnap+0x450)=start;
    cg.snap=&snapshot;*(void **)(base+0x40352c4)=rawSnap;
    cg.time=*(int *)(base+0x407dea4)=now;
    cgs.media.onFireShader=*(int *)(base+0x2588504)=301;
    cgs.media.onFireShader2=*(int *)(base+0x2588508)=302;
    cgs.media.flameCrackSound=*(int *)(base+0x2588cd4)=303;
    ent.hModel=match?101:102;cg_entities[7].pe.bodyRefEnt.hModel=*(int *)(rawCent+0x42c)=101;
    ent.reFlags=cases&1?8:16;ent.entityNum=-1;VectorSet(ent.origin,12,-23,45);
    if(direction==1)VectorSet(dir,1,-2,0.25f);if(direction==2)VectorSet(dir,-0.0f,0,-0.0f);
    pack(raw,&ent);saved=ent;saved.entityNum=7;pack(savedRaw,&saved);savedEs=es;memcpy(savedRawEs,rawEs,sizeof(rawEs));
    savedCent=cg_entities[7];memcpy(savedOriginalCent,rawCent,0xa94);
    savedSnapshot=snapshot;memcpy(savedOriginalSnapshot,rawSnap,sizeof(rawSnap));
    count=0;original(raw,cases,mode,rawEs,dir);n=count;memcpy(expected,calls,sizeof(expected));
    require(!memcmp(raw,savedRaw,sizeof(raw)),"original restore/entity number");
    count=0;CG_AddRefEntityWithPowerups(&ent,cases,mode,&es,dir);
    require(count==n,"render/sound count");
    for(i=0;i<count;++i)if(memcmp(&calls[i],&expected[i],sizeof(call_t))){
        fprintf(stderr,"local=%d now=%d start=%d end=%d mode=%d call=%d alpha=%d/%d volume=%d/%d\n",local,now,start,end,mode,i,calls[i].render[0xb7],expected[i].render[0xb7],calls[i].volume,expected[i].volume);
        require(0,"render/sound exact comparison");
    }
    require(!memcmp(&ent,&saved,sizeof(ent)),"SDK restore/entity number");
    require(!memcmp(&es,&savedEs,sizeof(es))&&!memcmp(rawEs,savedRawEs,sizeof(rawEs)),"entity state modified");
    require(!memcmp(&savedCent,&cg_entities[7],sizeof(savedCent)) && !memcmp(savedOriginalCent,rawCent,0xa94),"global entity modified");
    require(!memcmp(&savedSnapshot,&snapshot,sizeof(snapshot)) && !memcmp(savedOriginalSnapshot,rawSnap,sizeof(rawSnap)),"snapshot modified");
    ++cases;
}
int main(int argc,char **argv) {
    const int starts[]={-2001,0,1,100000,16777217,2000000000};
    const int offsets[]={-1,0,1,99,100,499,500,1499,1500,1501,1999,2000,2001,2999,3000,4499,4500,4501};
    const int ends[]={0,1000,1500,2000,4500,10000};
    HMODULE dll;unsigned char *base;hook_t hooks[2];int a,b,c,d,e,f,g;
    require(sizeof(refEntity_t)==236 && offsetof(refEntity_t,origin)==0x68 &&
            offsetof(refEntity_t,entityNum)==0xe8 && offsetof(refEntity_t,reFlags)==0xe4 &&
            offsetof(refEntity_t,fireRiseDir)==0xcc && offsetof(refEntity_t,shaderRGBA)==0xb4 &&
            offsetof(refEntity_t,customShader)==0xb0,"SDK render layout");
    require(argc==2,"DLL argument");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load original");base=(unsigned char *)dll;
    hook(&hooks[0],base,0x65290,originalRender);hook(&hooks[1],base,0x650c0,trap_S_AddLoopingSound);
    for(a=0;a<2;++a)for(b=0;b<6;++b)for(c=0;c<18;++c)for(d=0;d<6;++d)for(e=0;e<3;++e)for(f=0;f<3;++f)for(g=0;g<2;++g)
        compare(base,a,starts[b]+offsets[c],starts[b],starts[b]+ends[d],e,f,g);
    /* Sweep every millisecond to catch alpha-to-byte rounding transitions. */
    for(a=0;a<2;++a)for(b=0;b<2;++b)for(c=0;c<=6000;++c)
        compare(base,a,(b?16777217:100000)+c,b?16777217:100000,(b?16777217:100000)+4500,0,1,1);
    unhook(&hooks[1]);unhook(&hooks[0]);FreeLibrary(dll);
    printf("%d entity-powerups original comparisons passed.\n",cases);return 0;
}
