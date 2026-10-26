#include "../game/q_shared.h"
#include "../game/tce_bg.h"
#include "../cgame/tce_weapon_offset.h"
#include <windows.h>

/* Only supplies test-controlled stack contents; no original instruction or
 * function dependency is patched. This harness is never linked into cgame. */
__declspec(naked) static void callOriginal(void *fn,int weapon,float *offset,
    float *secondary,const float *axis,const float *legacy) {
    __asm {
        push ebp
        mov ebp,esp
        sub esp,128
        add esp,64
        push dword ptr [ebp+24]
        push dword ptr [ebp+20]
        push dword ptr [ebp+16]
        push dword ptr [ebp+12]
        mov eax,dword ptr [ebp+28]
        mov edx,dword ptr [eax]
        mov dword ptr [esp-16],edx
        mov edx,dword ptr [eax+4]
        mov dword ptr [esp-12],edx
        mov edx,dword ptr [eax+8]
        mov dword ptr [esp-8],edx
        call dword ptr [ebp+8]
        mov esp,ebp
        pop ebp
        ret
    }
}

void QDECL Com_Printf(const char *fmt,...) {(void)fmt;}
void QDECL Com_Error(int level,const char *fmt,...) {(void)level;fputs(fmt,stderr);exit(2);}
int Q_vsnprintf(char *out,int size,const char *fmt,va_list args) {
    int result;
    #undef _vsnprintf
    result=_vsnprintf(out,size-1,fmt,args);out[size-1]=0;return result;
}
static int cases,repairCases;
static unsigned rng=0x72e8a96b;
static unsigned randomInt(void) {rng=rng*1664525u+1013904223u;return rng>>8;}
static float sample(void) {return ((int)(randomInt()%65536)-32768)/4096.0f;}
static void require(int ok,const char *label) {
    if(!ok){fprintf(stderr,"Offset case %d: %s\n",cases,label);exit(1);}
}
static void checkFloat(float actual,float expected,const char *label,int index) {
    if(_isnan(actual)&&_isnan(expected))return;
    if(actual==expected)return;
    if(!_finite(actual)||!_finite(expected)||fabs(actual-expected)>0.00002+fabs(expected)*0.000002) {
        fprintf(stderr,"Offset case %d %s[%d]: %.9g expected %.9g\n",cases,label,index,actual,expected);exit(1);
    }
}
#define PUTI(address,value) (*(int *)(base+address-0x30000000)=(value))
#define PUTF(address,value) (*(float *)(base+address-0x30000000)=(value))
static void compare(unsigned char *base,int weapon,tce_weaponOffset_t *s,
    float axis[3][3],float legacy[3],int repair,int alias) {
    tce_weaponOffset_t savedState=*s;
    tce_weaponDef_t savedDefinition=weaponDef[weapon];
    float expected[10],actual[10],savedAxis[3][3],savedLegacy[3],zero[3]={0,0,0};
    float expectedScale,scale=999;int i;
    memcpy(savedAxis,axis,sizeof(savedAxis));memcpy(savedLegacy,legacy,sizeof(savedLegacy));
    PUTI(0x3407dea4,s->time);PUTI(0x3407f1c8,s->aimTime);PUTI(0x340a496c,s->aiming);
    PUTI(0x340a49fc,s->secondaryAiming);PUTI(0x340a4a00,s->secondaryAimTime);
    PUTI(0x32576e2c,s->developer);PUTI(0x3401484c,s->gunPosition);
    PUTF(0x34010168,s->developerOffset[0]);PUTF(0x34010288,s->developerOffset[1]);PUTF(0x340103a8,s->developerOffset[2]);
    memcpy(base+0x484af38+weapon*0xfd0,s->gunViewOffset,12);
    memcpy(base+0x484af44+weapon*0xfd0,s->gunAimOffset,12);
    memcpy(base+0x4c1df20+weapon*0x1cc,&weaponDef[weapon],sizeof(tce_weaponDef_t));
    PUTF(0x34846e6c,999);
    for(i=0;i<10;++i)expected[i]=actual[i]=12345.25f;
    callOriginal(base+0x72360,weapon,expected+1,expected+(alias?1:5),&axis[0][0],repair?zero:legacy);
    expectedScale=*(float *)(base+0x4846e6c);
    TCE_CG_EliteFPWeaponOffset(weapon,s,axis,actual+1,actual+(alias?1:5),&scale,repair?NULL:legacy);
    for(i=0;i<10;++i){
        checkFloat(actual[i],expected[i],"output/guard",i);
        if(repair)require(_finite(actual[i]),"zero-stack repair must produce finite output");
    }
    checkFloat(scale,expectedScale,"scale",0);
    require(!memcmp(s,&savedState,sizeof(*s))&&!memcmp(axis,savedAxis,sizeof(savedAxis))&&!memcmp(legacy,savedLegacy,sizeof(savedLegacy)),"input mutation");
    require(!memcmp(&weaponDef[weapon],&savedDefinition,sizeof(savedDefinition))&&!memcmp(base+0x4c1df20+weapon*0x1cc,&savedDefinition,sizeof(savedDefinition)),"weapon definition mutation");
    require(*(int *)(base+0x407dea4)==s->time&&*(int *)(base+0x407f1c8)==s->aimTime&&*(int *)(base+0x40a496c)==s->aiming&&*(int *)(base+0x40a49fc)==s->secondaryAiming&&*(int *)(base+0x40a4a00)==s->secondaryAimTime,"original state mutation");
    ++cases;if(repair)++repairCases;
}
static void setup(int weapon,int n,tce_weaponOffset_t *s,float axis[3][3],float legacy[3]) {
    const int boundaries[]={-200,-1,0,1,49,50,99,100,101,199,200,201,750};
    const int categories[]={0,74,75,76,84,85,86};
    float angles[3];int i,j;tce_weaponDef_t *w=&weaponDef[weapon];
    memset(s,0,sizeof(*s));memset(w,0,sizeof(*w));s->time=10000;
    s->aimTime=s->time-boundaries[n%13];s->secondaryAimTime=s->time-boundaries[(n/13)%13];
    s->aiming=n&1;s->secondaryAiming=(n>>1)&1;s->developer=(n>>2)&1;s->gunPosition=(n/8)%4;
    w->scoped=(float)((n/32)%3);w->unknown_1ac=categories[(n/96)%7];
    for(i=0;i<3;++i){s->developerOffset[i]=sample();s->gunViewOffset[i]=sample();s->gunAimOffset[i]=sample();legacy[i]=sample();angles[i]=sample()*40;}
    AnglesToAxis(angles,axis);
    if(n%5==0)AxisClear(axis);
    if(n%5==1)for(i=0;i<3;++i)for(j=0;j<3;++j)axis[i][j]=sample();
}
static void stackDependence(unsigned char *base) {
    tce_weaponOffset_t s;float axis[3][3],legacy[3],a[3],b[3],other[3];int i;
    setup(2,0,&s,axis,legacy);s.aiming=1;s.secondaryAiming=1;s.aimTime=s.secondaryAimTime=0;
    AxisClear(axis);
    /* Establish the dependency directly in the original before comparing
     * arbitrary legacy inputs and the intentional zero-stack repair. */
    compare(base,2,&s,axis,legacy,0,0);
    legacy[0]=legacy[1]=legacy[2]=0;
    callOriginal(base+0x72360,2,a,other,&axis[0][0],legacy);
    legacy[0]=10;legacy[1]=-20;legacy[2]=30;
    callOriginal(base+0x72360,2,b,other,&axis[0][0],legacy);
    for(i=0;i<3;++i)checkFloat(b[i]-a[i],legacy[i],"original stack dependence",i);
    for(i=0;i<3;++i){unsigned nanBits=0x7fc00000;legacy[0]=legacy[1]=legacy[2]=0;memcpy(&legacy[i],&nanBits,4);
        s.aiming=0;compare(base,2,&s,axis,legacy,0,0);
        s.aiming=1;compare(base,2,&s,axis,legacy,0,0);
        compare(base,2,&s,axis,legacy,1,0);}
}
int main(int argc,char **argv) {
    HMODULE dll;int w,n;tce_weaponOffset_t s;float axis[3][3],legacy[3];
    require(argc==2,"DLL argument");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"load original");
    stackDependence((unsigned char *)dll);
    for(w=0;w<64;++w)for(n=0;n<4096;++n){setup(w,n,&s,axis,legacy);compare((unsigned char *)dll,w,&s,axis,legacy,n%3==0,n%7==0);}
    printf("%d complete offset comparisons passed, including %d deterministic zero-stack comparisons.\n",cases,repairCases);
    FreeLibrary(dll);return 0;
}
