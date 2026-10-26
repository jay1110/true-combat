/* Compile the actual production movement unit, including static PM_Weapon. */
#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
static int switchEvent;
static int conditionCalls,conditionWeapon;
static void condition(int client,int which,int value,int convert) {++conditionCalls;conditionWeapon=value;}
static int events, bodies, bodyContinue, bodyForce, bodyAnim;
static void hook(unsigned char *p,void *fn) {
 DWORD old,tmp;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))abort();
 p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
 FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
static int body(void *ps,void *model,int anim,int continued,int force) {++bodies;bodyContinue=continued;bodyForce=force;bodyAnim=anim;return 0;}
static void eventOriginal(int e) {switchEvent=e;if(e==40||e==42)++events;}
static void eventSource(int e,int parm,playerState_t *ps) {switchEvent=e;if(e==EV_FIRE_WEAPON||e==EV_FIRE_WEAPON_LASTSHOT)++events;}
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base,raw[0x700],rawpm[0x200],rawext[0x200],rawchar[0x900];
 int skills[8]={0},i,expectedEvents,expectedBodies,expectedContinue,expectedForce,expectedAnim;
 int weapons[]={2,3,5,6,7,8,10,13,14,23,24,25,32,33,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54},delays[]={0,1,32,100},intervals[]={50,80,150},ticks[]={8,16,33};
 int w,d,n,t,pattern,weapon,prone,clip,recoilCase,frames=0; tce_weaponDef_t *defs;void (__cdecl *original)(void),(__cdecl *originalAdjust)(void);
 playerState_t ps;pmove_t move;pmoveExt_t ext;bg_character_t character;
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;
 original=(void *)(base+0xfc80);originalAdjust=(void *)(base+0x9c30);
 hook(base+0x2520,body);hook(base+0x8680,eventOriginal);
 hook((unsigned char *)BG_AnimScriptEvent,body);hook((unsigned char *)BG_AddPredictableEventToPlayerstate,eventSource);
 for(w=0;w<sizeof(weapons)/sizeof(weapons[0]);++w)for(d=0;d<4;++d)for(n=0;n<3;++n)for(t=0;t<3;++t)for(pattern=0;pattern<3;++pattern)for(prone=0;prone<2;++prone)for(clip=1;clip<=30;clip+=29)for(recoilCase=0;recoilCase<4;++recoilCase) {
 weapon=weapons[w];
 memset(raw,0,sizeof(raw));memset(rawpm,0,sizeof(rawpm));memset(rawext,0,sizeof(rawext));memset(rawchar,0,sizeof(rawchar));
 memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&ext,0,sizeof(ext));memset(&character,0,sizeof(character));
 *(void **)(base+0x4ba9edc)=rawpm;*(void **)rawpm=raw;*(void **)(rawpm+4)=rawext;*(void **)(rawpm+8)=rawchar;
 *(void **)(rawpm+0x118)=skills;*(int *)(rawpm+0xc)=1000;rawpm[0x10]=1;rawpm[0x12]=(unsigned char)weapon;
 *(int *)(raw+0x68)=prone?0x80000:0;
 *(int *)(raw+0xd0)=100;*(int *)(raw+0x11c)=2;*(int *)(raw+0xa4)=weapon;
 *(int *)(raw+0x190+weapon*4)=3;*(int *)(raw+0x290+weapon*4)=clip;*(int *)(base+0x4ba9e68)=ticks[t];
 defs=(void *)(base+0x4c1df20);memset(defs,0,64*sizeof(*defs));defs[weapon].parsed=1;defs[weapon].maxclip=30;defs[weapon].uses=1;defs[weapon].fireDelayTime=delays[d];defs[weapon].nextShotTime=intervals[n];
 defs[weapon].unknown_0f8[5]=recoilCase?1000:0;
 defs[weapon].unknown_0f8[6]=recoilCase?200:0;
 defs[weapon].unknown_0f8[7]=recoilCase==3?700:recoilCase?150:0;
 *(int *)(raw+0xec)=ps.stats[STAT_TCE_SHOT_SEED]=recoilCase*17331;
 *(int *)(raw+0x100)=ps.stats[STAT_TCE_SHOT_INSTABILITY]=recoilCase==2?900:0;
 *(int *)(raw+0xf8)=ps.stats[STAT_TCE_RECOIL_REMAINDER]=recoilCase==2?300:0;
 *(int *)(raw+0x44)=ps.delta_angles[0]=-190;
 *(int *)(raw+0x48)=ps.delta_angles[1]=57;
 memcpy(weaponDef,defs,sizeof(weaponDef));gearDef.parsed=1;
 pm=&move;move.ps=&ps;move.pmext=&ext;move.character=&character;move.skill=skills;
 move.cmd.serverTime=1000;move.cmd.buttons=BUTTON_ATTACK;move.cmd.weapon=weapon;
 ps.stats[STAT_HEALTH]=100;ps.persistant[PERS_TEAM]=TEAM_ALLIES;ps.weapon=weapon;ps.ammo[weapon]=3;ps.ammoclip[weapon]=clip;ps.eFlags=prone?EF_PRONE:0;
 pml.msec=ticks[t];pml.frametime=ticks[t]*.001f;
 for(i=0;i<40;++i) {
  *(int *)(rawpm+0x28)=move.oldcmd.serverTime=move.cmd.serverTime-ticks[t];
  originalAdjust();PM_AdjustAimSpreadScale();
  if (*(int *)(raw+0xf0)!=ps.stats[STAT_TCE_WEAPON_FLAGS] ||
      *(int *)(raw+0xfc)!=ps.stats[STAT_TCE_MOVEMENT_INSTABILITY] ||
      *(int *)(raw+0x104)!=ps.stats[STAT_TCE_AIM_PHASE] ||
      *(int *)(raw+0x100)!=ps.stats[STAT_TCE_SHOT_INSTABILITY] ||
      *(int *)(raw+0x48c)!=ps.aimSpreadScale ||
      *(float *)(raw+0x488)!=ps.aimSpreadScaleFloat) {
   fprintf(stderr,"Production aim adapter mismatch weapon%d frame%d\n",weapon,i);return 1;
  }
  rawpm[0x10]=move.cmd.buttons=(!pattern || i%10<5)?BUTTON_ATTACK:0;
  rawpm[0x12]=move.cmd.weapon=(unsigned char)(pattern==2?56:weapon);
  events=bodies=bodyContinue=bodyForce=bodyAnim=0;original();
  expectedEvents=events;expectedBodies=bodies;expectedContinue=bodyContinue;expectedForce=bodyForce;expectedAnim=bodyAnim;
  events=bodies=bodyContinue=bodyForce=bodyAnim=0;PM_Weapon();
  if (*(int *)(raw+0x2c)!=ps.weaponTime || *(int *)(raw+0x30)!=ps.weaponDelay ||
      *(int *)(raw+0xa8)!=ps.weaponstate || *(int *)(raw+0x290+weapon*4)!=ps.ammoclip[weapon] ||
      expectedEvents!=events || *(int *)(raw+0x480)!=ps.weapAnim ||
      expectedBodies!=bodies || expectedContinue!=bodyContinue || expectedForce!=bodyForce || (bodies && bodyAnim!=(expectedAnim==2?ANIM_ET_FIREWEAPON:expectedAnim==3?ANIM_ET_FIREWEAPON2:expectedAnim==30?ANIM_ET_FIREWEAPONPRONE:expectedAnim==31?ANIM_ET_FIREWEAPON2PRONE:-1))) {
   fprintf(stderr,"Mismatch weapon%d delay%d interval%d tick%d pattern%d frame%d: time%d/%d delay%d/%d state%d/%d clip%d/%d event%d/%d anim%d/%d body%d/%d\n",
    weapon,delays[d],intervals[n],ticks[t],pattern,i,*(int *)(raw+0x2c),ps.weaponTime,
    *(int *)(raw+0x30),ps.weaponDelay,*(int *)(raw+0xa8),ps.weaponstate,
    *(int *)(raw+0x290+weapon*4),ps.ammoclip[weapon],expectedEvents,events,
    *(int *)(raw+0x480),ps.weapAnim,expectedBodies,bodies);return 1;
  }
  if (*(int *)(raw+0x44)!=ps.delta_angles[0] || *(int *)(raw+0x48)!=ps.delta_angles[1] ||
      *(int *)(raw+0xec)!=ps.stats[STAT_TCE_SHOT_SEED] ||
      *(int *)(raw+0xf8)!=ps.stats[STAT_TCE_RECOIL_REMAINDER] ||
      *(int *)(raw+0x100)!=ps.stats[STAT_TCE_SHOT_INSTABILITY]) {
   fprintf(stderr,"Recoil mismatch w%d case%d frame%d tick%d: pitch%d/%d yaw%d/%d seed%d/%d rem%d/%d instability%d/%d\n",
    weapon,recoilCase,i,ticks[t],*(int *)(raw+0x44),ps.delta_angles[0],*(int *)(raw+0x48),ps.delta_angles[1],
    *(int *)(raw+0xec),ps.stats[STAT_TCE_SHOT_SEED],*(int *)(raw+0xf8),ps.stats[STAT_TCE_RECOIL_REMAINDER],
    *(int *)(raw+0x100),ps.stats[STAT_TCE_SHOT_INSTABILITY]);return 1;
  }
  ++frames;if(!ps.ammoclip[weapon])break;
  *(int *)(rawpm+0xc)+=ticks[t];move.cmd.serverTime+=ticks[t];
 }
 }
 printf("%d production PM_Weapon frames matched original timing, ammo, attack animation, hip recoil and instability recovery\n",frames);

 FreeLibrary(dll);return 0;
}
