#include "../cgame/cg_local.h"
#include "../cgame/tce_weapon_media.h"
#include "../cgame/tce_weapon_frames.h"
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <setjmp.h>
#undef _vsnprintf

cg_t cg;
vmCvar_t cg_debugAnim, cg_animSpeed, cg_noPlayerAnims;
static jmp_buf errorJump;
static int setCases,clearCases,partCases,errors,prints,runCases,raiseCases,weaponCases;
static char message[128];
static int startAnimCases,continueAnimCases;
static void require(int ok,const char *what){if(!ok){fprintf(stderr,"Weapon frames %d/%d/%d: %s\n",setCases,clearCases,partCases,what);exit(1);}}
void QDECL CG_Error(const char *fmt,...){va_list ap;va_start(ap,fmt);_vsnprintf(message,sizeof(message),fmt,ap);va_end(ap);++errors;longjmp(errorJump,1);}
void QDECL CG_Printf(const char *fmt,...){va_list ap;va_start(ap,fmt);_vsnprintf(message,sizeof(message),fmt,ap);va_end(ap);++prints;}
typedef struct{unsigned char *address,saved[5];} hook_t;
static void hook(hook_t *h,unsigned char *base,int off,void *fn){DWORD p,u;h->address=base+off;memcpy(h->saved,h->address,5);require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&p),"hook");h->address[0]=0xe9;*(int *)(h->address+1)=(unsigned char *)fn-h->address-5;FlushInstructionCache(GetCurrentProcess(),h->address,5);VirtualProtect(h->address,5,p,&u);}
static void unhook(hook_t *h){DWORD p,u;require(VirtualProtect(h->address,5,PAGE_EXECUTE_READWRITE,&p),"unhook");memcpy(h->address,h->saved,5);FlushInstructionCache(GetCurrentProcess(),h->address,5);VirtualProtect(h->address,5,p,&u);}
static void transition(unsigned char *base,int clear,int number,int time,int frameTime,int debug,int variant,int sdk){
    static tce_weaponInfo_t media,savedMedia;
    static weaponInfo_t sdkMedia,savedSdk;
    static lerpFrame_t actual,expected;
    char expectedMessage[128];int expectedErrors,expectedPrints,i;
    void (__cdecl *original)(void *,void *,int)=(void *)(base+(clear?0x6f400:0x6f440));
    memset(&media,0x5a,sizeof(media));memset(&sdkMedia,0x5a,sizeof(sdkMedia));
    for(i=0;i<13;++i){media.animations[i].firstFrame=i*37-31;media.animations[i].mdxFile=variant?0:101+i;media.animations[i].initialLerp=variant==0?0:variant==1?-137:250+i;}
    memcpy(sdkMedia.weapAnimations,media.animations,sizeof(media.animations));savedMedia=media;savedSdk=sdkMedia;
    memset(&actual,0x5a,sizeof(actual));actual.frameTime=frameTime;actual.animation=&media.animations[0];expected=actual;
    cg.time=*(int *)(base+0x407dea4)=time;cg_debugAnim.integer=*(int *)(base+0x4034f4c)=debug;
    errors=prints=0;memset(message,0,sizeof(message));
    if(!setjmp(errorJump))original(&media,&expected,number);
    expectedErrors=errors;expectedPrints=prints;memcpy(expectedMessage,message,sizeof(message));
    errors=prints=0;memset(message,0,sizeof(message));
    if(!setjmp(errorJump)){
        if(clear){if(sdk)CG_ClearWeapLerpFrame(&sdkMedia,&actual,number);else TCE_CG_ClearWeapLerpFrame(media.animations,&actual,number);}
        else TCE_CG_SetWeapLerpFrameAnimation(sdk?sdkMedia.weapAnimations:media.animations,&actual,number);
    }
    if(sdk && !expectedErrors)expected.animation=&sdkMedia.weapAnimations[number&~ANIM_TOGGLEBIT];
    require(errors==expectedErrors&&prints==expectedPrints&&!memcmp(message,expectedMessage,sizeof(message)),"error/debug behavior");
    require(!memcmp(&actual,&expected,sizeof(actual)),"complete frame state including untouched fields");
    require(!memcmp(&media,&savedMedia,sizeof(media))&&!memcmp(&sdkMedia,&savedSdk,sizeof(sdkMedia)),"media changed");
    if(clear)++clearCases;else ++setCases;
}
static void parts(unsigned char *base,int number,int toggle,int partId,int flags,int history,int variant,int sdk,int alias){
    static tce_weaponInfo_t media,savedMedia;
    static weaponInfo_t sdkMedia,savedSdk;
    static centity_t cent,savedCent;
    static unsigned char raw[0xa94],savedRaw[0xa94];
    refEntity_t actual,expected,parent,savedParent,originalParent;
    int i,a,b;unsigned bit=1u<<((unsigned)partId&31),hide=1u<<(((unsigned)partId+8)&31);
    int (__cdecl *original)(void *,void *,void *,int,void *)=(void *)(base+0x6f350);
    memset(&media,0x5a,sizeof(media));memset(&sdkMedia,0x5a,sizeof(sdkMedia));memset(&cent,0x5a,sizeof(cent));memset(raw,0x5a,sizeof(raw));
    for(i=0;i<13;++i){media.animations[i].numFrames=(variant?-3:7)+i;media.animations[i].firstFrame=i*31-40;
        media.animations[i].moveSpeed=(history==0?0:history==1?bit:i&1?bit:0);}
    media.animations[number].moveSpeed=(flags&1?bit:0)|(flags&2?hide:0);
    memcpy(sdkMedia.weapAnimations,media.animations,sizeof(media.animations));
    cent.pe.weap.animationNumber=number|(toggle?ANIM_TOGGLEBIT:0);
    cent.pe.weap.animation=sdk?&sdkMedia.weapAnimations[number]:&media.animations[number];
    memcpy(raw+0x364,&cent.pe.weap,sizeof(cent.pe.weap));*(void **)(raw+0x398)=&media.animations[number];
    savedCent=cent;memcpy(savedRaw,raw,sizeof(raw));savedMedia=media;savedSdk=sdkMedia;
    memset(&actual,0x35,sizeof(actual));expected=actual;memset(&parent,0x72,sizeof(parent));
    parent.frame=variant?-21:79;parent.oldframe=variant?18:-13;parent.backlerp=variant?-0.25f:0.75f;
    if(alias){actual=parent;expected=parent;}savedParent=parent;originalParent=parent;
    a=original(raw,&expected,alias?&expected:&originalParent,partId,&media);
    b=sdk?CG_GetPartFramesFromWeap(&cent,&actual,alias?&actual:&parent,partId,&sdkMedia):
        TCE_CG_GetPartFramesFromWeap(&cent.pe.weap,&actual,alias?&actual:&parent,partId,media.animations);
    require(a==b&&!memcmp(&actual,&expected,sizeof(actual)),"part result/complete render state");
    require(!memcmp(&parent,&savedParent,sizeof(parent))&&!memcmp(&originalParent,&savedParent,sizeof(parent)),"parent changed");
    require(!memcmp(&cent,&savedCent,sizeof(cent))&&!memcmp(raw,savedRaw,sizeof(raw)),"cent changed");
    require(!memcmp(&media,&savedMedia,sizeof(media))&&!memcmp(&sdkMedia,&savedSdk,sizeof(sdkMedia)),"part media changed");++partCases;
}

static snapshot_t snapshot;
static unsigned char rawSnapshot[0x500];
static void runOnce(unsigned char *base,tce_weaponInfo_t *media,lerpFrame_t *actual,
                    int number,int nextWeapon,int now,int enabled,int debug,float speed) {
    static lerpFrame_t expected;
    static tce_weaponInfo_t saved;
    char expectedMessage[128];int expectedErrors,expectedPrints;
    void (__cdecl *original)(void *,void *,void *,int,float)=(void *)(base+0x730d0);
    cg.time=*(int *)(base+0x407dea4)=now;
    cg_animSpeed.integer=*(int *)(base+0x40120ec)=enabled;
    cg_debugAnim.integer=*(int *)(base+0x4034f4c)=debug;
    snapshot.ps.nextWeapon=*(int *)(rawSnapshot+0x448)=nextWeapon;
    cg.snap=&snapshot;*(void **)(base+0x40352c4)=rawSnapshot;
    expected=*actual;saved=*media;
    errors=prints=0;memset(message,0,sizeof(message));
    if(!setjmp(errorJump))original(NULL,media,&expected,number,speed);
    expectedErrors=errors;expectedPrints=prints;memcpy(expectedMessage,message,sizeof(message));
    errors=prints=0;memset(message,0,sizeof(message));
    if(!setjmp(errorJump))TCE_CG_RunWeapLerpFrame(media->animations,actual,number,speed,TCE_PM_RaiseAnimForWeapon(nextWeapon));
    require(errors==expectedErrors&&prints==expectedPrints&&!memcmp(message,expectedMessage,sizeof(message)),"run error/debug");
    require(fabs(actual->backlerp-expected.backlerp)<0.000002,"run interpolation");
    expected.backlerp=actual->backlerp;
    if(memcmp(actual,&expected,sizeof(expected))){
        fprintf(stderr,"run=%d number=%d weapon=%d time=%d speed=%g frame=%d/%d frameTime=%d/%d\n",runCases,number,nextWeapon,now,speed,actual->frame,expected.frame,actual->frameTime,expected.frameTime);
        require(0,"run complete frame state");
    }
    require(!memcmp(media,&saved,sizeof(saved)),"run media mutation");++runCases;
}
static unsigned testRandom=0x12345678;
static unsigned nextRandom(void){testRandom=testRandom*1664525u+1013904223u;return testRandom;}
static void runMatrix(unsigned char *base){
    const int weapons[]={0,28,55,56,62,63,-1},offsets[]={-10000,-201,-200,-1,0,1,199,200,201,10000};
    const int lerps[]={0,1,7,50,200,-7},counts[]={0,1,7,13},loops[]={0,1,3,-1};
    const float speeds[]={-0.5f,0,0.1f,0.5f,1,1.25f,2};
    const int invalid[]={-1,13,525,1024};
    static tce_weaponInfo_t media;static lerpFrame_t frame;
    int a,b,c,d,e,f,g,i,j,num,now,kind;
    int (__cdecl *originalRaise)(int)=(void *)(base+0x88c0);
    for(i=-4096;i<=4096;++i){require(TCE_PM_RaiseAnimForWeapon(i)==originalRaise(i),"raise mapping");++raiseCases;}
    require(TCE_PM_RaiseAnimForWeapon(2147483647)==originalRaise(2147483647),"raise INT_MAX");++raiseCases;
    require(TCE_PM_RaiseAnimForWeapon(-2147483647-1)==originalRaise(-2147483647-1),"raise INT_MIN");++raiseCases;
    for(a=0;a<26;++a)for(b=0;b<3;++b)for(c=0;c<7;++c)for(d=0;d<7;++d)for(e=0;e<10;++e)for(f=0;f<5;++f){
        num=a%13|(a>=13?ANIM_TOGGLEBIT:0);now=e&1?-101:1000;
        memset(&media,0x5a,sizeof(media));
        for(i=0;i<13;++i){media.animations[i].firstFrame=i*37-31;media.animations[i].mdxFile=101+i;
            media.animations[i].initialLerp=offsets[e];media.animations[i].frameLerp=lerps[e%6];
            media.animations[i].numFrames=counts[e%4];media.animations[i].loopFrames=loops[(e/2)%4];}
        memset(&frame,0x5a,sizeof(frame));frame.animation=b==0?NULL:&media.animations[b==1?a%13:(a+1)%13];
        frame.animationNumber=b==1?num:(num^ANIM_TOGGLEBIT);frame.frameTime=now+offsets[e];frame.oldFrameTime=now+offsets[(e+3)%10];
        frame.animationTime=now+offsets[(e+6)%10];frame.backlerp=0.375f;
        runOnce(base,&media,&frame,num,weapons[c],now,f==4?0:1,f%4,speeds[d]);
    }
    /* Independent values avoid correlations in the systematic matrix. */
    for(g=0;g<65536;++g){
        memset(&media,0,sizeof(media));num=nextRandom()%13;now=(int)(nextRandom()%200001)-100000;
        for(i=0;i<13;++i){media.animations[i].firstFrame=(int)(nextRandom()%301)-150;
            media.animations[i].mdxFile=nextRandom()%99;media.animations[i].initialLerp=(int)(nextRandom()%1001)-500;
            media.animations[i].frameLerp=lerps[nextRandom()%6];media.animations[i].numFrames=counts[nextRandom()%4];media.animations[i].loopFrames=loops[nextRandom()%4];}
        memset(&frame,0x5a,sizeof(frame));frame.frame=(int)(nextRandom()%101)-50;frame.frameModel=nextRandom()%999;
        frame.frameTime=now+(int)(nextRandom()%2001)-1000;frame.oldFrameTime=now+(int)(nextRandom()%2001)-1000;
        frame.animationTime=now+(int)(nextRandom()%2001)-1000;frame.backlerp=-0.75f;
        frame.animationNumber=num;kind=nextRandom()%3;frame.animation=kind?&media.animations[num]:NULL;
        if(kind==2)num^=ANIM_TOGGLEBIT;
        runOnce(base,&media,&frame,num,nextRandom()%64,now,(int)(nextRandom()%3)-1,nextRandom()%4,speeds[nextRandom()%7]);
    }
    /* Stateful time progression through all animations and toggle transitions. */
    memset(&media,0,sizeof(media));
    for(i=0;i<13;++i){media.animations[i].firstFrame=100*i;media.animations[i].mdxFile=i+1;
        media.animations[i].initialLerp=31;media.animations[i].frameLerp=17+i;media.animations[i].numFrames=13;media.animations[i].loopFrames=i%4;}
    for(a=0;a<7;++a){memset(&frame,0,sizeof(frame));for(j=0;j<2048;++j)
        runOnce(base,&media,&frame,(j/59)%13|((j/59)&1?ANIM_TOGGLEBIT:0),weapons[a],j*11,1,j%4,speeds[a]);}
    /* Integer quotient above float's exact range; original FILD keeps it exact. */
    for(a=0;a<2;++a)for(b=0;b<7;++b){
        memset(&media,0,sizeof(media));media.animations[0].frameLerp=1;media.animations[0].numFrames=2147483647;
        memset(&frame,0,sizeof(frame));frame.animation=&media.animations[0];frame.frameTime=a?-16777218:16777216;
        runOnce(base,&media,&frame,0,0,a?-16777217:16777217,1,0,speeds[b]);
    }
    for(a=0;a<4;++a)for(b=0;b<2;++b){memset(&frame,0,sizeof(frame));frame.animation=b?&media.animations[0]:NULL;
        runOnce(base,&media,&frame,invalid[a],28,1000,1,3,1);}
}


static void weaponOutput(unsigned char *base,int number,int weapon,int mode,int timeCase,int debug,int alias) {
    static tce_weaponInfo_t media,savedMedia;
    static unsigned char ps[0x500],savedPs[0x500];
    static lerpFrame_t initial,expected;
    lerpFrame_t *lf=&cg.predictedPlayerEntity.pe.weap;
    const int offsets[]={-1000,-1,0,1,199,200,201};
    int i,now=1001,originalOld=117,originalFrame=-39,actualOld=117,actualFrame=-39;
    int expectedErrors,expectedPrints;
    float originalBack=-0.375f,actualBack=-0.375f;
    char expectedMessage[128];
    void (__cdecl *original)(void *,void *,int *,int *,float *)=(void *)(base+0x73020);
    memset(&media,0,sizeof(media));memset(ps,0x5a,sizeof(ps));
    *(int *)(ps+0xa0)=7;*(int *)(ps+0x480)=number;memcpy(savedPs,ps,sizeof(ps));
    for(i=0;i<13;++i){media.animations[i].firstFrame=31*i-51;media.animations[i].numFrames=7+i;
        media.animations[i].loopFrames=timeCase%3?3:0;media.animations[i].frameLerp=timeCase==0?0:33;
        media.animations[i].initialLerp=offsets[timeCase];media.animations[i].mdxFile=211+i;}
    savedMedia=media;memset(&initial,0x5a,sizeof(initial));
    initial.frame=79;initial.oldFrame=78;initial.backlerp=0.75f;initial.frameModel=211;
    initial.animation=mode%3==0?NULL:&media.animations[0];initial.animationNumber=mode%3==1?number:number^ANIM_TOGGLEBIT;
    initial.frameTime=now+offsets[timeCase];initial.oldFrameTime=now-13;initial.animationTime=now-273;
    *lf=initial;memcpy(base+0x407e7d0,&initial,sizeof(initial));
    cg.time=*(int *)(base+0x407dea4)=now;cg_debugAnim.integer=*(int *)(base+0x4034f4c)=debug;
    cg_noPlayerAnims.integer=*(int *)(base+0x4849f6c)=mode>=6?(mode>=9?-1:1):0;
    cg_animSpeed.integer=*(int *)(base+0x40120ec)=mode>=3&&mode<6?0:1;
    snapshot.ps.nextWeapon=*(int *)(rawSnapshot+0x448)=weapon;cg.snap=&snapshot;*(void **)(base+0x40352c4)=rawSnapshot;
    errors=prints=0;memset(message,0,sizeof(message));
    if(!setjmp(errorJump))original(ps,&media,&originalOld,alias?&originalOld:&originalFrame,&originalBack);
    memcpy(&expected,base+0x407e7d0,sizeof(expected));expectedErrors=errors;expectedPrints=prints;memcpy(expectedMessage,message,sizeof(message));
    errors=prints=0;memset(message,0,sizeof(message));
    if(!setjmp(errorJump))TCE_CG_WeaponAnimation(number,media.animations,&actualOld,alias?&actualOld:&actualFrame,&actualBack,TCE_PM_RaiseAnimForWeapon(weapon));
    require(errors==expectedErrors&&prints==expectedPrints&&!memcmp(message,expectedMessage,sizeof(message)),"weapon animation debug/error output");
    require(originalOld==actualOld&&originalFrame==actualFrame,"weapon frame output/order/alias");
    require(fabs(originalBack-actualBack)<0.000002,"weapon interpolation output");
    require(fabs(lf->backlerp-expected.backlerp)<0.000002,"weapon frame interpolation");expected.backlerp=lf->backlerp;
    require(!memcmp(lf,&expected,sizeof(expected)),"weapon frame state/disabled preservation");
    require(!memcmp(&media,&savedMedia,sizeof(media))&&!memcmp(ps,savedPs,sizeof(ps)),"weapon media/playerState mutation");
    ++weaponCases;
}
static void weaponMatrix(unsigned char *base) {
    const int weapons[]={0,28,55,56,62,63},debugs[]={0,1,2,3,4,-1},invalid[]={-1,13,525};
    int a,b,c,d,e,f;
    for(a=0;a<26;++a)for(b=0;b<6;++b)for(c=0;c<12;++c)for(d=0;d<7;++d)for(e=0;e<6;++e)for(f=0;f<2;++f)
        weaponOutput(base,a%13|(a>=13?ANIM_TOGGLEBIT:0),weapons[b],c,d,debugs[e],f);
    for(a=0;a<3;++a)for(b=0;b<12;++b)for(c=0;c<2;++c)
        if(b%3!=1 || b>=3)weaponOutput(base,invalid[a],28,b,2,3,c);
}

/* Original functions run without hooks, including Continue -> Start. The raw
 * TC player-state layout differs from SDK, so compare the explicitly mapped
 * animation output and byte-check both complete input regions separately. */
static int animationTransition(unsigned char *base,int continuing,int pmType,
    int timer,int weapon,int current,int requested) {
    unsigned char before[0x500],expected[0x500],timerBefore[12];
    playerState_t savedState;pmoveExt_t savedExt;
    int result;
    void (__cdecl *original)(int)=(void *)(base+(continuing?0x72fb0:0x72fe0));
    memset(base+0x407dec0,0x35,sizeof(before));
    memset(base+0x4087eec,0x5a,sizeof(timerBefore));
    *(int *)(base+0x407dec4)=pmType;*(int *)(base+0x407df64)=weapon;
    *(int *)(base+0x407e340)=current;*(int *)(base+0x4087ef0)=timer;
    memcpy(before,base+0x407dec0,sizeof(before));memcpy(expected,before,sizeof(before));
    memcpy(timerBefore,base+0x4087eec,sizeof(timerBefore));
    memset(&cg.predictedPlayerState,0x35,sizeof(cg.predictedPlayerState));
    memset(&cg.pmext,0x5a,sizeof(cg.pmext));
    cg.predictedPlayerState.pm_type=pmType;cg.predictedPlayerState.weapon=weapon;
    cg.predictedPlayerState.weapAnim=current;cg.pmext.weapAnimTimer=timer;
    savedState=cg.predictedPlayerState;savedExt=cg.pmext;
    original(requested);result=*(int *)(base+0x407e340);
    if(continuing)CG_ContinueWeaponAnim(requested);else CG_StartWeaponAnim(requested);
    require(cg.predictedPlayerState.weapAnim==result,"start/continue animation output");
    savedState.weapAnim=result;memcpy(expected+0x480,&result,sizeof(result));
    require(!memcmp(&savedState,&cg.predictedPlayerState,sizeof(savedState)),"start/continue changed other player state");
    require(!memcmp(&savedExt,&cg.pmext,sizeof(savedExt)),"start/continue changed pmext");
    require(!memcmp(expected,base+0x407dec0,sizeof(expected)),"original changed other player state");
    require(!memcmp(timerBefore,base+0x4087eec,sizeof(timerBefore)),"original changed timer context");
    if(continuing)++continueAnimCases;else ++startAnimCases;
    return result;
}
static void animationTransitionMatrix(unsigned char *base) {
    const int pmTypes[]={(-2147483647-1),-1,0,1,2,3,4,2147483647};
    const int timers[]={(-2147483647-1),-1,0,1,17,2147483647};
    const int weapons[]={-1,0,1,28,60,62,63,2147483647};
    int animations[34],a,b,c,d,e,f,current;
    for(a=0;a<26;++a)animations[a]=a%13|(a>=13?ANIM_TOGGLEBIT:0);
    animations[26]=-1;animations[27]=(-2147483647-1);animations[28]=2147483647;
    animations[29]=13;animations[30]=525;animations[31]=511;animations[32]=1024;animations[33]=1536;
    for(a=0;a<2;++a)for(b=0;b<8;++b)for(c=0;c<6;++c)for(d=0;d<8;++d)for(e=0;e<34;++e)for(f=0;f<34;++f)
        animationTransition(base,a,pmTypes[b],timers[c],weapons[d],animations[e],animations[f]);
    /* Repeated starts must toggle; continues of the same number must not. */
    for(a=0;a<34;++a){current=animations[a];for(b=0;b<128;++b)
        current=animationTransition(base,b%3!=0,0,b%17==0?1:0,28,current,animations[(a+b/8)%34]);}
}

int main(int argc,char **argv){
    const int times[]={-1001,-1,0,1,999,100001},frameTimes[]={-333,0,55,7777},debugs[]={0,1,2,3,-1};
    const int invalid[]={-1,-512,13,14,511,525,1024,2147483647,(-2147483647-1)};
    const int partIds[]={0,1,2,3,4,5,6,7,-1,8,23,24,31,32,39};
    HMODULE dll;unsigned char *base;hook_t hooks[2];int a,b,c,d,e,f,g,h;
    require(argc==2,"DLL argument");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load original");base=(unsigned char *)dll;
    hook(&hooks[0],base,0x47bf0,CG_Error);hook(&hooks[1],base,0x47b10,CG_Printf);
    for(a=0;a<2;++a)for(b=0;b<26;++b)for(c=0;c<6;++c)for(d=0;d<4;++d)for(e=0;e<5;++e)for(f=0;f<3;++f)for(g=0;g<2;++g)
        transition(base,a,b%13|(b>=13?ANIM_TOGGLEBIT:0),times[c],frameTimes[d],debugs[e],f,g);
    for(a=0;a<2;++a)for(b=0;b<9;++b)for(c=0;c<6;++c)for(d=0;d<2;++d)transition(base,a,invalid[b],times[c],55,3,1,d);
    for(a=0;a<13;++a)for(b=0;b<2;++b)for(c=0;c<15;++c)for(d=0;d<4;++d)for(e=0;e<3;++e)for(f=0;f<2;++f)for(g=0;g<2;++g)for(h=0;h<2;++h)
        parts(base,a,b,partIds[c],d,e,f,g,h);
    runMatrix(base);
    weaponMatrix(base);
    animationTransitionMatrix(base);
    unhook(&hooks[1]);unhook(&hooks[0]);FreeLibrary(dll);
    printf("%d weapon-animation original comparisons passed.\n",weaponCases);
    printf("%d start-animation and %d continue-animation original comparisons passed.\n",startAnimCases,continueAnimCases);
    printf("%d run-frame and %d raise-mapping original comparisons passed.\n",runCases,raiseCases);
    printf("%d set-animation, %d clear-frame, %d part-frame original comparisons passed.\n",setCases,clearCases,partCases);return 0;
}
