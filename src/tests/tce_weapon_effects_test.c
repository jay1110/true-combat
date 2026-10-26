#include "../cgame/cg_local.h"
#include "../cgame/tce_weapon_effects.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>

static int calls,flameCases,renderCases,adapterCases,mutate;
static void *expectedCent;
static float *expectedOrigin,*expectedAngles;
static const refEntity_t *expectedGun;
static refEntity_t rendered;
static void require(int ok,const char *why) {
    if(!ok){fprintf(stderr,"weapon effects %d/%d/%d: %s\n",flameCases,renderCases,adapterCases,why);exit(1);}
}
/* Dependency boundary only: validate every argument. FireFlameChunks itself
 * is not replaced in production or claimed verified by this test. */
static void flame(void *cent,float *origin,float *angles,float speed,int firing) {
    require(cent==expectedCent && origin==expectedOrigin && angles==expectedAngles,"flame pointer identity");
    require(speed==1.0f && firing==1,"flame speed/firing arguments");
    ++calls;
    if(mutate){if(origin)origin[0]=137.5f;angles[2]=-31.25f;}
}
void CG_FireFlameChunks(centity_t *cent,vec3_t origin,vec3_t angles,float speed,qboolean firing) {
    flame(cent,origin,angles,speed,firing);
}
void trap_R_AddRefEntityToScene(const refEntity_t *gun) {
    require(gun==expectedGun,"renderer pointer identity");++calls;
    if(gun)rendered=*gun;
}
typedef struct {unsigned char *entry,saved[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int offset,void *fn) {
    DWORD protection,unused;h->entry=base+offset;memcpy(h->saved,h->entry,5);
    require(VirtualProtect(h->entry,5,PAGE_EXECUTE_READWRITE,&protection),"hook protection");
    h->entry[0]=0xe9;*(int *)(h->entry+1)=(unsigned char *)fn-h->entry-5;
    FlushInstructionCache(GetCurrentProcess(),h->entry,5);VirtualProtect(h->entry,5,protection,&unused);
}
static void unhook(hook_t *h) {
    DWORD protection,unused;require(VirtualProtect(h->entry,5,PAGE_EXECUTE_READWRITE,&protection),"unhook protection");
    memcpy(h->entry,h->saved,5);FlushInstructionCache(GetCurrentProcess(),h->entry,5);VirtualProtect(h->entry,5,protection,&unused);
}
static void compareFlame(unsigned char *base,int weapon,int originMode,int mutation,int sdk) {
    unsigned char raw[0xa94],savedRaw[0xa94];centity_t cent,savedCent;
    vec3_t originalOrigin={1.5f,-2.5f,3.5f},portOrigin={1.5f,-2.5f,3.5f};int expectedCalls;
    void (__cdecl *original)(void *,float *)=(void *)(base+0x72320);
    memset(raw,0x35,sizeof(raw));memset(&cent,0x35,sizeof(cent));
    *(int *)(raw+0xe8)=weapon;cent.currentState.weapon=weapon;
    VectorSet(cent.lerpAngles,47.5f,-91.5f,133.75f);memcpy(raw+0x71c,cent.lerpAngles,12);
    savedCent=cent;memcpy(savedRaw,raw,sizeof(raw));mutate=mutation;
    expectedCent=raw;expectedAngles=(float *)(raw+0x71c);
    expectedOrigin=originMode==0?originalOrigin:originMode==1?expectedAngles:NULL;
    calls=0;original(raw,expectedOrigin);expectedCalls=calls;
    require(expectedCalls==(weapon==66),"original flame gate");
    expectedCent=&cent;expectedAngles=cent.lerpAngles;
    expectedOrigin=originMode==0?portOrigin:originMode==1?expectedAngles:NULL;
    calls=0;
    if(sdk)CG_FlamethrowerFlame(&cent,expectedOrigin);else TCE_CG_FlamethrowerFlame(&cent,weapon,expectedOrigin);
    require(calls==expectedCalls,"flame call count");
    require(!memcmp(originalOrigin,portOrigin,12)&&!memcmp(raw+0x71c,cent.lerpAngles,12),"flame forwarded writes/aliasing");
    memcpy(savedRaw+0x71c,raw+0x71c,12);VectorCopy(cent.lerpAngles,savedCent.lerpAngles);
    require(!memcmp(raw,savedRaw,sizeof(raw))&&!memcmp(&cent,&savedCent,sizeof(cent)),"wrapper changed entity");
    if(sdk)++adapterCases;else ++flameCases;
}
static void compareRender(unsigned char *base,int powerups,int mode,int pattern) {
    refEntity_t gun,savedGun,expectedRendered;centity_t cent,savedCent;playerState_t ps,savedPs;
    unsigned char *bytes=(unsigned char *)&gun;int i;
    void (__cdecl *original)(void *,int,void *,void *)=(void *)(base+0x72350);
    for(i=0;i<sizeof(gun);++i)bytes[i]=(unsigned char)(i*37+pattern);
    memset(&cent,0x35,sizeof(cent));memset(&ps,0x5a,sizeof(ps));savedCent=cent;savedPs=ps;savedGun=gun;
    expectedGun=mode&4?NULL:&gun;memset(&rendered,0,sizeof(rendered));calls=0;
    original((void *)expectedGun,powerups,mode&1?&ps:NULL,mode&2?&cent:NULL);
    require(calls==1,"original render count");expectedRendered=rendered;
    calls=0;memset(&rendered,0,sizeof(rendered));
    CG_AddWeaponWithPowerups((refEntity_t *)expectedGun,powerups,mode&1?&ps:NULL,mode&2?&cent:NULL);
    require(calls==1&&!memcmp(&rendered,&expectedRendered,sizeof(rendered)),"renderer argument bytes/count");
    require(!memcmp(&gun,&savedGun,sizeof(gun))&&!memcmp(&cent,&savedCent,sizeof(cent))&&!memcmp(&ps,&savedPs,sizeof(ps)),"render wrapper input mutation");
    ++renderCases;
}
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *base;hook_t hooks[2];int w,m,b,p;
    const int flags[]={0,1,2,0x100,0x200,0x800000,0x7fffffff,(-2147483647-1),-1};
    require(sizeof(refEntity_t)==236,"render ABI");require(argc==2,"DLL argument");
    dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load original");base=(unsigned char *)dll;
    hook(&hooks[0],base,0x3ce00,flame);hook(&hooks[1],base,0x65290,trap_R_AddRefEntityToScene);
    for(w=-128;w<256;++w)for(m=0;m<3;++m)for(b=0;b<2;++b)compareFlame(base,w,m,b,0);
    /* The adapter receives TC protocol IDs, including ordinary slot6 and flame66. */
    for(w=0;w<128;++w)for(m=0;m<3;++m)for(b=0;b<2;++b)
        compareFlame(base,w,m,b,1);
    for(w=0;w<9;++w)for(m=0;m<8;++m)for(p=0;p<16;++p)compareRender(base,flags[w],m,p*17);
    unhook(&hooks[1]);unhook(&hooks[0]);FreeLibrary(dll);
    printf("%d flame-dispatch, %d SDK-adapter and %d weapon-render original comparisons passed.\n",flameCases,adapterCases,renderCases);return 0;
}
