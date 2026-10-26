#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../game/tce_bg.h"
#include "../cgame/tce_limbo_loadout.h"
#include <windows.h>
#include <stdio.h>
static char submitted[256];
static void commandSink(const char *s) { if(!strncmp(s,"team ",5))Q_strncpyz(submitted,s,sizeof(submitted)); }
static void popupSink(int type,const char *message,int shader) {}
static void soundSink(int sound,int channel) {}
static void patch(void *address,void *fn) {
    unsigned char *p=address;DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);
    p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
    FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
static int testCallbacks(void) {
    panel_button_t button;int i;
    puts("Starting production callback checks");
    memset(&gearDef,0,sizeof(gearDef));memset(&cgs,0,sizeof(cgs));memset(&cg,0,sizeof(cg));
    memset(weaponDef,0,sizeof(weaponDef));memset(&button,0,sizeof(button));
    gearDef.parsed=1;cgs.gametype=7;cgs.ccSelectedTeam=0;cgs.ccSelectedClass=2;
    cgs.ccSelectedWeaponNumber=1;cgs.clientinfo[0].team=TEAM_ALLIES;
    for(i=0;i<64;++i)gearDef.team[i]=1;
    strcpy(gearDef.weaponFile[8],"mp5");strcpy(gearDef.weaponFile[42],"ump45");
    strcpy(gearDef.weaponFile[2],"beretta");
    gearDef.requiredSkill[8][1]=gearDef.requiredSkill[42][1]=gearDef.requiredSkill[2][1]=1;
    patch(CG_AddPMItem,popupSink);patch(trap_SendClientCommand,commandSink);patch(trap_S_StartLocalSound,soundSink);
    button.rect.x=642;button.rect.y=346;button.rect.w=180;button.rect.h=30;
    cgDC.cursorx=700;cgDC.cursory=330;
    Init_Display(&cgDC);
    puts("Selecting UMP row");
    CG_LimboPanel_WeaponPanel_KeyDown(&button,K_MOUSE1);
    CG_LimboPanel_WeaponPanel_KeyUp(&button,K_MOUSE1);
    if(cgs.ccSelectedWeapon!=1 || submitted[0])return 1;
    CG_LimboPanel_SendSetupMsg(qfalse);
    if(strcmp(submitted,"team b 2 42 2 21 0\n"))return 1;
    /* The same row is unavailable at AA1 when it requires AA2. */
    cgs.ccSelectedWeapon=0;submitted[0]=0;gearDef.requiredSkill[42][1]=2;
    CG_LimboPanel_WeaponPanel_KeyDown(&button,K_MOUSE1);
    CG_LimboPanel_WeaponPanel_KeyUp(&button,K_MOUSE1);
    if(cgs.ccSelectedWeapon!=0)return 1;
    CG_LimboPanel_SendSetupMsg(qfalse);
    if(strcmp(submitted,"team b 2 8 2 21 0\n"))return 1;
    strcpy(gearDef.weaponFile[38],"akimbo_beretta");gearDef.requiredSkill[38][1]=2;
    cgs.clientinfo[0].tceScorePlayerClass=2;cgs.clientinfo[0].tceClassRating=1;
    cgs.ccSelectedWeaponNumber=0;
    CG_LimboPanel_WeaponPanel_KeyDown(&button,K_MOUSE1);
    CG_LimboPanel_WeaponPanel_KeyUp(&button,K_MOUSE1);
    if(cgs.ccSelectedWeapon2!=1 || strcmp(submitted,"team b 2 8 2 21 0\n"))return 1;
    CG_LimboPanel_SendSetupMsg(qfalse);
    if(strcmp(submitted,"team b 2 8 38 21 0\n"))return 1;
    strcpy(gearDef.weaponFile[10],"mac10");strcpy(gearDef.weaponFile[39],"glock");
    gearDef.team[10]=gearDef.team[39]=0;gearDef.requiredSkill[10][1]=gearDef.requiredSkill[39][1]=1;
    /* Sender does not clamp a stale sidearm index after a team change;
     * original selection returns0. Team-selection UI owns the reset. */
    cgs.ccSelectedTeam=1;cgs.clientinfo[0].team=TEAM_AXIS;
    CG_LimboPanel_SendSetupMsg(qfalse);
    if(strcmp(submitted,"team r 2 10 0 21 0\n"))return 1;
    puts("Production callbacks: primary/sidearm selection, locked row rejection, team change and TC loadout payloads passed");
    return 0;
}
int main(int argc,char **argv) {
    HMODULE dll;unsigned char *b;int (__cdecl *count)(int),(__cdecl *get)(int,int,int);
    tce_limboLoadout_t s;int team,cls,scenario,i,slot,n,a,e,cases=0;unsigned int seed=7;
    int (__cdecl *disabled)(int);
    setbuf(stdout,NULL);
    if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
    b=(unsigned char *)dll;count=(void *)(b+0x43590);get=(void *)(b+0x43790);disabled=(void *)(b+0x43d20);
    *(int *)(b+0x40352a4)=0;
    for(team=1;team<=2;++team)for(cls=0;cls<7;++cls)for(scenario=0;scenario<64;++scenario) {
        memset(&s,0,sizeof(s));s.team=team;s.playerClass=cls;
        s.rating=scenario%8-2;s.previousRating=scenario%5;s.scoreClass=scenario%5;
        s.lightSkill=scenario%6;s.heavySkill=(scenario/6)%6;s.gametype=scenario%2?7:2;
        *(int *)(b+0x3fde37c)=team==2?0:1;*(int *)(b+0x3fde384)=cls;
        *(int *)(b+0x2580bc4)=s.rating;*(int *)(b+0x2580bc8)=s.previousRating;
        *(int *)(b+0x2580bcc)=s.scoreClass;*(int *)(b+0x2580b00)=s.lightSkill;
        *(int *)(b+0x2580b04)=s.heavySkill;*(int *)(b+0x257e100)=s.gametype;
        for(i=0;i<64;++i) {
            int j;
            seed=seed*1664525u+1013904223u;
            gearDef.weaponFile[i][0]=i && (seed&7)?'x':0;
            gearDef.team[i]=(seed>>4)%3;
            for(j=0;j<3;++j)gearDef.requiredSkill[i][j]=(seed>>(7+j*4))%24;
            weaponDef[i].loadoutWeight=(seed>>22)%6;
            *(int *)(b+0x4c1e03c+i*0x1cc)=weaponDef[i].loadoutWeight;
        }
        memcpy(b+0x4ba9f00,&gearDef,sizeof(gearDef));
        gearDef.parsed=1;cg.clientNum=0;cgs.ccSelectedTeam=team==2?0:1;
        cgs.ccSelectedClass=cls;cgs.ccSelectedWeapon=0;cgs.gametype=s.gametype;
        cgs.clientinfo[0].tceClassRating=s.rating;cgs.clientinfo[0].tcePreviousClassRating=s.previousRating;
        cgs.clientinfo[0].tceScorePlayerClass=s.scoreClass;
        cgs.clientinfo[0].skill[SK_LIGHT_WEAPONS]=s.lightSkill;
        cgs.clientinfo[0].skill[SK_HEAVY_WEAPONS]=s.heavySkill;
        cgs.weaponRestrictions=0;*(float *)(b+0x257e210)=0;
        *(int *)(b+0x3fde388)=0;
        s.primary=get(0,1,1);
        /* Sidearm counting calls the selected primary; there are no heavy IDs
         * in the recovered candidate table, so no engine callback is needed. */
        for(slot=0;slot<3;++slot) {
            a=TCE_LimboWeaponCount(&s,slot);e=count(slot);
            if(a!=e){printf("count team%d class%d scenario%d slot%d: %d != %d\n",team,cls,scenario,slot,a,e);return 1;}
            if(CG_LimboPanel_WeaponCount_ForSlot(slot)!=e)return 1;
            ++cases;
            for(n=0;n<24;++n) {
                a=TCE_LimboWeaponForNumber(&s,n,slot);e=get(n,slot,1);
                if(a!=e){printf("get team%d class%d scenario%d slot%d n%d: %d != %d\n",team,cls,scenario,slot,n,a,e);return 1;}
                if(CG_LimboPanel_GetWeaponForNumber(n,slot,qtrue)!=e)return 1;
                ++cases;
            }
        }
        for(i=1;i<64;++i) {
            if(CG_LimboPanel_RealWeaponIsDisabled((weapon_t)i)!=disabled(i))return 1;
            ++cases;
        }
    }
    printf("%d original Windows limbo count/selection comparisons matched\n",cases);
    return testCallbacks();
}
