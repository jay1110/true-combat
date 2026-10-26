#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <stdio.h>
#include "../cgame/cg_local.h"
#include "../cgame/tce_fragment_sound.h"

static int cases, volumeCases, randomValue, randomCalls, soundCalls;
static int soundArgs[4];
static vec3_t soundOrigin;
static char order[8];
static int orderCount;
static void require(int ok, const char *message) {
    if (!ok) { fprintf(stderr, "case %d volume %d: %s\n", cases, volumeCases, message); exit(1); }
}
int rand(void) { ++randomCalls; order[orderCount++]='R'; return randomValue; }
void trap_S_StartSoundVControl(vec3_t origin, int entity, int channel, sfxHandle_t sound, int volume) {
    ++soundCalls; order[orderCount++]='S';
    memcpy(soundOrigin, origin, sizeof(vec3_t));
    soundArgs[0]=entity; soundArgs[1]=channel; soundArgs[2]=sound; soundArgs[3]=volume;
}
typedef struct { unsigned char *entry, saved[5]; } hook_t;
static void hook(hook_t *h, unsigned char *base, int offset, void *fn) {
    DWORD p,u; h->entry=base+offset; memcpy(h->saved,h->entry,5);
    require(VirtualProtect(h->entry,5,PAGE_EXECUTE_READWRITE,&p),"hook protection");
    h->entry[0]=0xe9; *(int *)(h->entry+1)=(unsigned char *)fn-h->entry-5;
    FlushInstructionCache(GetCurrentProcess(),h->entry,5); VirtualProtect(h->entry,5,p,&u);
}
static void unhook(hook_t *h) {
    DWORD p,u; require(VirtualProtect(h->entry,5,PAGE_EXECUTE_READWRITE,&p),"unhook protection");
    memcpy(h->entry,h->saved,5); FlushInstructionCache(GetCurrentProcess(),h->entry,5);
    VirtualProtect(h->entry,5,p,&u);
}
static void setContext(unsigned char *base, const tce_fragmentSoundContext_t *c) {
    int i,j;
    memcpy(base+0x407f01c,c->listener,12);
    *(int *)(base+0x3fdf380)=c->distanceVariant;
    *(float *)(base+0x40a49cc)=c->attenuation;
    *(int *)(base+0x40a4a1c)=c->disabled;
    *(int *)(base+0x40a49bc)=c->soundVariant;
    memcpy(base+0x25888a8,c->brass,sizeof(c->brass));
    memcpy(base+0x25888e4,c->rubble,sizeof(c->rubble));
    memcpy(base+0x258890c,c->type6,sizeof(c->type6));
    for(i=0;i<5;++i)for(j=0;j<2;++j)
        memcpy(base+0x2588938+i*0x50+j*0x20,c->casing[i][j],16);
    *(int *)(base+0x2588cd8)=c->type12;
}
static void resetCapture(void) {
    randomCalls=soundCalls=orderCount=0; memset(order,0,sizeof(order));
    memset(soundArgs,0,sizeof(soundArgs)); memset(soundOrigin,0,sizeof(soundOrigin));
}
static void compareBounce(unsigned char *base, tce_fragmentSoundContext_t *c,
    int type, unsigned material, int variant) {
    localEntity_t le,expected;
    trace_t trace,savedTrace;
    tce_fragmentSoundContext_t savedContext;
    unsigned char raw[448],expectedRaw[448];
    int nr,ns,no,args[4]; char calls[8]; vec3_t origin;
    void (__cdecl *original)(void *,void *)=(void *)(base+0x44c10);
    memset(&le,0x5a,sizeof(le)); le.leBounceSoundType=type; expected=le;
    memset(raw,0x5a,sizeof(raw)); *(int *)(raw+0x98)=type; memcpy(expectedRaw,raw,sizeof(raw));
    memset(&trace,0x35,sizeof(trace));
    trace.surfaceFlags=(int)((material<<24)|(variant?0xabcdef:0));
    trace.endpos[0]=variant%3?c->listener[0]+91.25f:32000;
    trace.endpos[1]=c->listener[1]-17.5f; trace.endpos[2]=c->listener[2]+33.25f;
    setContext(base,c); savedContext=*c; savedTrace=trace;
    randomValue=variant; resetCapture(); original(raw,&trace);
    nr=randomCalls;ns=soundCalls;no=orderCount; memcpy(args,soundArgs,sizeof(args));
    memcpy(origin,soundOrigin,sizeof(origin));memcpy(calls,order,sizeof(calls));
    resetCapture();TCE_CG_FragmentBounceSound(&le,&trace,c);
    require(nr==randomCalls&&ns==soundCalls&&no==orderCount&&!memcmp(calls,order,sizeof(calls)),"call/RNG order");
    require(!memcmp(args,soundArgs,sizeof(args))&&!memcmp(origin,soundOrigin,sizeof(origin)),"complete sound arguments");
    expected.leBounceSoundType=*(int *)(raw+0x98);
    require(!memcmp(&le,&expected,sizeof(le)),"complete SDK local entity");
    *(int *)(expectedRaw+0x98)=le.leBounceSoundType;
    require(!memcmp(raw,expectedRaw,sizeof(raw)),"complete original local entity");
    require(!memcmp(&trace,&savedTrace,sizeof(trace))&&!memcmp(c,&savedContext,sizeof(*c)),"input mutation");
    ++cases;
}
static void compareVolume(unsigned char *base,tce_fragmentSoundContext_t *c,
    const vec3_t origin,float volume,float range,int linear) {
    int actual,expected;
    int (__cdecl *original)(const float *,float,float,int)=(void *)(base+0x44b50);
    vec3_t saved; tce_fragmentSoundContext_t savedContext=*c;
    memcpy(saved,origin,sizeof(saved));setContext(base,c);
    expected=original(origin,volume,range,linear);
    actual=TCE_CG_SoundVolume(origin,volume,range,linear,c);
    if(actual!=expected)fprintf(stderr,"volume %g range %g linear %d distflag %d attenuation %g origin %g,%g,%g: %d != %d\n",volume,range,linear,c->distanceVariant,c->attenuation,origin[0],origin[1],origin[2],actual,expected);
    require(actual==expected,"exact volume");
    require(!memcmp(origin,saved,sizeof(saved))&&!memcmp(c,&savedContext,sizeof(*c)),"volume inputs");
    ++volumeCases;
}
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *base;hook_t h[2];
    tce_fragmentSoundContext_t c;
    vec3_t origin;
    const float attenuation[]={0,.25f,1,1.5f,-.5f};
    const float volumes[]={0,1,64,127,255,-8};
    const float ranges[]={1,80,800,4000};
    const int seeds[]={0,1,2,3,4,5,127,32767};
    int i,j,k,a,b,d,t,m,v;
    unsigned seed=91327;
    require(argc==2,"DLL path");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load original");base=(unsigned char *)dll;
    memset(&c,0,sizeof(c));
    for(i=0;i<4;++i)for(j=0;j<3;++j)c.brass[i][j]=100+i*3+j;
    for(i=0;i<3;++i){c.rubble[i]=200+i;c.type6[i]=210+i;}
    for(i=0;i<5;++i)for(j=0;j<2;++j)for(k=0;k<4;++k)c.casing[i][j][k]=300+i*100+j*10+k;
    c.type12=901;c.listener[0]=17.25f;c.listener[1]=-100.5f;c.listener[2]=31.125f;
    /* Full real original math: no hooks in SoundVolume or its dependencies. */
    for(a=0;a<5;++a)for(b=0;b<6;++b)for(d=0;d<4;++d)for(i=0;i<2;++i)for(j=0;j<2;++j)for(k=0;k<257;++k){
        c.attenuation=attenuation[a];c.distanceVariant=j;
        memcpy(origin,c.listener,12);origin[0]+=(k-1)*ranges[d]*.00390625f;
        compareVolume(base,&c,origin,volumes[b],ranges[d],i);
    }
    for(i=0;i<50000;++i){
        for(j=0;j<3;++j){seed=seed*1664525u+1013904223u;origin[j]=((int)(seed>>16)-32768)*.0625f;}
        c.attenuation=attenuation[i%5];c.distanceVariant=i%3-1;
        compareVolume(base,&c,origin,volumes[i%6],ranges[i%4],i&1);
    }
    hook(&h[0],base,0x885b6,rand);hook(&h[1],base,0x64fe0,trap_S_StartSoundVControl);
    for(t=-1;t<=14;++t)for(m=0;m<256;++m)for(d=0;d<2;++d)for(v=0;v<3;++v)for(a=0;a<5;++a)for(b=0;b<8;++b){
        c.disabled=d;c.soundVariant=v;c.attenuation=attenuation[a];c.distanceVariant=b&1;
        compareBounce(base,&c,t,m,seeds[b]);
    }
    unhook(&h[1]);unhook(&h[0]);FreeLibrary(dll);
    printf("%d exact SoundVolume and %d complete FragmentBounceSound original comparisons passed.\n",volumeCases,cases);
    return 0;
}
