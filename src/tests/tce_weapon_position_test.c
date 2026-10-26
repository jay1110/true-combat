#include "../game/q_shared.h"
#include "../game/tce_bg.h"
#include "../cgame/tce_weapon_position.h"
#include <windows.h>
/* No hooks: execute the original position, AngleVectors and firemode code. */
void QDECL Com_Printf(const char *fmt,...) {(void)fmt;}
void QDECL Com_Error(int level,const char *fmt,...) {(void)level;fputs(fmt,stderr);exit(2);}
int Q_vsnprintf(char *out,int size,const char *fmt,va_list args) {
    int r;
    #undef _vsnprintf
    r=_vsnprintf(out,size-1,fmt,args);out[size-1]=0;return r;
}
typedef struct {size_t offset;unsigned address;size_t size;} mapping_t;
#define M(field,address) {offsetof(tce_weaponPosition_t,field),address,sizeof(((tce_weaponPosition_t *)0)->field)}
static const mapping_t mappings[]={
    M(time,0x3407dea4),M(weapon,0x3407df64),M(eFlags,0x3407df28),M(thirdPerson,0x3407deb8),M(weaponState,0x3407df68),
    M(mountedPitch,0x34087f28),M(viewAngles,0x3407f1b0),M(postureModified,0x340a4b0c),M(proneMovingTime,0x3408a3a4),
    M(flags_3407dfb0,0x3407dfb0),M(stanceTime,0x340a4950),M(pmFlags,0x3407decc),M(duckTime,0x340a4948),
    M(lean,0x3407defc),M(aiming,0x340a496c),M(leanTime,0x3407f1c8),M(shotTime,0x3407e6b4),M(count_3407dff8,0x3407dff8),
    M(scopeEnabled,0x34849e4c),M(firemodeTime,0x340a49f8),M(speed,0x34080fb4),M(bobSin,0x34080fa4),M(bobCycle,0x34080fa8),
    M(swayTime,0x340a4a04),M(priorForward,0x340a4a08),M(swayHorizontal,0x340a4a14),M(swayVertical,0x340a4a18),
    M(velocity,0x3407dee0),M(proneTime,0x340a4958),M(stepTime,0x3407ef64),M(scale_3407dfbc,0x3407dfbc),M(scale_3407dfc0,0x3407dfc0),
    M(landTime,0x3407ef6c),M(stepChange,0x3407ef60),M(landChange,0x3407ef68),M(developer,0x32576e2c),
    M(tacticalScale,0x34846e6c),M(tacticalPitch,0x3407e264),M(tacticalYaw,0x3407e268),M(kickAngles,0x340810f8)
};
static unsigned rng=0x29ab3471;static int cases;
static tce_weaponPosition_t *history;
static unsigned randomInt(void){rng=rng*1664525u+1013904223u;return rng>>8;}
static float sample(void){return ((int)(randomInt()%65536)-32768)/32768.0f;}
static void checkFloat(float actual,float expected,const char *label,int i) {
    if(!isfinite(actual)||!isfinite(expected)||fabs(actual-expected)>0.0002+fabs(expected)*0.00001) {
        fprintf(stderr,"case %d %s[%d]: %.9g expected %.9g\n",cases,label,i,actual,expected);exit(1);
    }
}
static void compare(unsigned char *base,tce_weaponPosition_t *state) {
    tce_weaponPosition_t original=*state,actual=*state,unchanged=*state;
    float refdef[18]={0},expectedOrigin[3],expectedAngles[3],origin[3],angles[3];size_t i;
    void (__cdecl *function)(float *,float *)=(void *)(base+0x73270);
    tce_weaponPosition_t *portState=state;
    if(history)state=history;
    original=*state;
    for(i=0;i<sizeof(mappings)/sizeof(mappings[0]);++i)memcpy(base+mappings[i].address-0x30000000,(char *)state+mappings[i].offset,mappings[i].size);
    memcpy(refdef+6,state->viewOrigin,12);memcpy(refdef+9,state->viewAxis,36);*(void **)(base+0x40863ac)=refdef;
    memcpy(base+0x484af50+state->weapon*0xfd0,state->gunViewAngles,12);
    memcpy(base+0x4c1df20+state->weapon*0x1cc,&weaponDef[state->weapon],sizeof(tce_weaponDef_t));
    *(float *)(base+0x48896c8)=state->developerAngles[0];*(float *)(base+0x4012c28)=state->developerAngles[1];*(float *)(base+0x488c968)=state->developerAngles[2];
    function(expectedOrigin,expectedAngles);
    for(i=0;i<sizeof(mappings)/sizeof(mappings[0]);++i)memcpy((char *)&original+mappings[i].offset,base+mappings[i].address-0x30000000,mappings[i].size);
    TCE_CG_CalculateWeaponPosition(&actual,origin,angles);
    for(i=0;i<3;++i){checkFloat(origin[i],expectedOrigin[i],"origin",i);checkFloat(angles[i],expectedAngles[i],"angles",i);checkFloat(actual.priorForward[i],original.priorForward[i],"priorForward",i);}
    checkFloat(actual.swayHorizontal,original.swayHorizontal,"swayH",0);checkFloat(actual.swayVertical,original.swayVertical,"swayV",0);
    if(actual.swayTime!=original.swayTime || actual.postureModified!=original.postureModified){fprintf(stderr,"case %d integer state mismatch\n",cases);exit(1);}
    unchanged.postureModified=actual.postureModified;unchanged.swayTime=actual.swayTime;unchanged.swayHorizontal=actual.swayHorizontal;unchanged.swayVertical=actual.swayVertical;VectorCopy(actual.priorForward,unchanged.priorForward);
    if(memcmp(&unchanged,&actual,sizeof(actual))){fputs("port changed input fields\n",stderr);exit(1);}
    if(history)*history=original;
    original.swayHorizontal=actual.swayHorizontal;original.swayVertical=actual.swayVertical;
    if(memcmp(&original,&actual,sizeof(actual))){fputs("original changed additional mapped fields\n",stderr);exit(1);}
    *portState=actual;++cases;
}
static void setup(tce_weaponPosition_t *s,int weapon,int n) {
    static const int boundaries[]={-1,0,1,49,50,149,150,199,200,201,249,250,299,300,449,450,499,500,749,750,751};
    int i,d=boundaries[n%21];tce_weaponDef_t *w=&weaponDef[weapon];
    memset(s,0,sizeof(*s));memset(w,0,sizeof(*w));s->time=10000;s->weapon=weapon;
    s->eFlags=((n&1)?0x8000:0)|((n&2)?0x100000:0)|((n&4)?0x80000:0);
    s->flags_3407dfb0=n&8?0x4000:0;s->pmFlags=n&16?4:0;
    s->proneMovingTime=(s->eFlags&0x100000)?s->time-d:d-s->time;
    s->stanceTime=s->flags_3407dfb0?s->time-d:d-s->time;s->duckTime=s->pmFlags?s->time-d:d-s->time;
    s->aiming=(n>>5)&1;s->scopeEnabled=(n>>6)&1;s->thirdPerson=(n>>7)&1;s->weaponState=(n>>8)%3;
    s->lean=(n%7)?sample()*17:0;s->leanTime=s->time-d;s->shotTime=s->time-d;s->firemodeTime=s->time-d;
    s->landTime=s->time-d;s->stepTime=s->time-d;s->proneTime=d-s->time;s->swayTime=s->time+((n%5)-2)*16;
    s->count_3407dff8=n%3-1;s->postureModified=17;s->mountedPitch=sample()*60;s->speed=sample()*300;s->bobSin=sample();s->bobCycle=n%2;
    s->swayHorizontal=sample()*5;s->swayVertical=sample()*5;s->stepChange=sample()*18;s->landChange=sample()*12;
    s->scale_3407dfbc=randomInt()%1800-400;s->scale_3407dfc0=randomInt()%1800-400;
    s->developer=n%2;s->tacticalScale=n%3?sample():0;s->tacticalPitch=2000+(int)(sample()*300);s->tacticalYaw=2000+(int)(sample()*300);
    for(i=0;i<3;++i){s->viewOrigin[i]=sample()*500;s->viewAngles[i]=sample()*90;s->gunViewAngles[i]=sample()*5;s->kickAngles[i]=sample()*3;s->developerAngles[i]=sample()*4;s->velocity[i]=sample()*200;}
    AnglesToAxis(s->viewAngles,s->viewAxis);
    for(i=0;i<3;++i)s->priorForward[i]=s->viewAxis[0][i]+sample()*.03f;
    w->scoped=(float)((n>>9)%3);w->usesRecoilAnimMod=(n>>1)&1;w->usesPistolAnimMod=n&1;w->pump=(n>>2)&1;
    w->semiauto=(n>>3)&1;w->fullauto=(n>>4)&1;w->bolt=(n>>10)&1;w->burst=n%3;w->noTacMode=(n>>6)&1;w->unknown_0f8[3]=randomInt()%2400;
}
int main(int argc,char **argv) {
    HMODULE dll;int w,n,j;tce_weaponPosition_t state,reference;
    if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
    for(w=0;w<64;++w)for(n=0;n<4096;++n){setup(&state,w,n);compare((unsigned char *)dll,&state);}
    /* Small targets exercise the dead zone and unclamped smoothing; the large
     * random velocities above deliberately cover the saturation branches. */
    for(n=0;n<4096;++n){
        setup(&state,n%64,n);state.speed=0;state.lean=0;state.landChange=0;state.stepChange=0;
        state.swayHorizontal=sample()*.1f;state.swayVertical=sample()*.1f;
        VectorCopy(state.viewAxis[0],state.priorForward);
        for(j=0;j<3;++j)state.velocity[j]=sample()*4;
        compare((unsigned char *)dll,&state);
    }
    for(n=0;n<256;++n){setup(&state,n%64,n*17);reference=state;history=&reference;
        for(j=0;j<120;++j){state.time+=16;state.viewAngles[1]+=.25f;AnglesToAxis(state.viewAngles,state.viewAxis);
            reference.time=state.time;VectorCopy(state.viewAngles,reference.viewAngles);memcpy(reference.viewAxis,state.viewAxis,sizeof(state.viewAxis));
            compare((unsigned char *)dll,&state);}}
    printf("%d full weapon-position original comparisons passed (including state histories).\n",cases);FreeLibrary(dll);return 0;
}
