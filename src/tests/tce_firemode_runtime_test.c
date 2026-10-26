/* Compile the actual production movement unit, including static PM_Weapon. */
#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
static int modeEvents, events, bodies, bodyContinue, bodyForce, bodyAnim;
static void hook(unsigned char *p,void *fn) {
 DWORD old,tmp;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))abort();
 p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
 FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
static int body(void *ps,void *model,int anim,int continued,int force) {++bodies;bodyContinue=continued;bodyForce=force;bodyAnim=anim;return 0;}
static void eventOriginal(int e) {if(e==40||e==42)++events;if(e==135)++modeEvents;}
static void eventSource(int e,int parm,playerState_t *ps) {if(e==EV_FIRE_WEAPON||e==EV_FIRE_WEAPON_LASTSHOT)++events;if(e==EV_TCE_FIREMODE)++modeEvents;}
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base,raw[0x700],rawpm[0x200],rawext[0x200],rawchar[0x900];
 int skills[8]={0},i,expectedModeEvents,expectedEvents,expectedBodies,expectedContinue,expectedForce,expectedAnim;
 int weapons[]={3,8,10,45},delays[]={0,1,32,100},intervals[]={50,80,150},ticks[]={8,16,33};
 int mode,w,d,n,t,pattern,weapon,prone,clip,recoilCase,frames=0; tce_weaponDef_t *defs;void (__cdecl *original)(void),(__cdecl *originalAdjust)(void);
 playerState_t ps;pmove_t move;pmoveExt_t ext;bg_character_t character;
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;
 original=(void *)(base+0xfc80);originalAdjust=(void *)(base+0x9c30);
 hook(base+0x2520,body);hook(base+0x8680,eventOriginal);
 hook((unsigned char *)BG_AnimScriptEvent,body);hook((unsigned char *)BG_AddPredictableEventToPlayerstate,eventSource);
 for(mode=0;mode<3;++mode)for(w=0;w<4;++w)for(d=0;d<4;++d)for(n=0;n<3;++n)for(t=0;t<3;++t)for(pattern=0;pattern<4;++pattern)for(prone=0;prone<2;++prone)for(clip=1;clip<=30;clip+=29)for(recoilCase=0;recoilCase<4;++recoilCase) {
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
 defs[weapon].semiauto=1;defs[weapon].fullauto=weapon!=45;defs[weapon].burst=weapon==45?3:0;
 *(int *)(raw+0x138)=ps.persistant[10]=mode;
 memcpy(weaponDef,defs,sizeof(weaponDef));gearDef.parsed=1;
 pm=&move;move.ps=&ps;move.pmext=&ext;move.character=&character;move.skill=skills;
 move.cmd.serverTime=1000;move.cmd.buttons=BUTTON_ATTACK;move.cmd.weapon=weapon;
 ps.stats[STAT_HEALTH]=100;ps.persistant[PERS_TEAM]=TEAM_ALLIES;ps.weapon=weapon;ps.ammo[weapon]=3;ps.ammoclip[weapon]=clip;ps.eFlags=prone?EF_PRONE:0;
 pml.msec=ticks[t];pml.frametime=ticks[t]*.001f;
 for(i=0;i<100;++i) {
  *(int *)(rawpm+0x28)=move.oldcmd.serverTime=move.cmd.serverTime-ticks[t];
  originalAdjust();PM_AdjustAimSpreadScale();
  if (*(int *)(raw+0xf0)!=ps.stats[STAT_TCE_WEAPON_FLAGS] ||
      *(int *)(raw+0xfc)!=ps.stats[STAT_TCE_MOVEMENT_INSTABILITY] ||
      *(int *)(raw+0x104)!=ps.stats[STAT_TCE_AIM_PHASE] ||
      *(int *)(raw+0x100)!=ps.stats[STAT_TCE_SHOT_INSTABILITY] ||
      *(int *)(raw+0x48c)!=ps.aimSpreadScale ||
      *(float *)(raw+0x488)!=ps.aimSpreadScaleFloat) {
   fprintf(stderr,"Aim w%d frame%d: flags %x/%x move%d/%d phase%d/%d shot%d/%d aim%d/%d float%g/%g\n",weapon,i,*(int *)(raw+0xf0),ps.stats[8],*(int *)(raw+0xfc),ps.stats[11],*(int *)(raw+0x104),ps.stats[13],*(int *)(raw+0x100),ps.stats[12],*(int *)(raw+0x48c),ps.aimSpreadScale,*(float *)(raw+0x488),ps.aimSpreadScaleFloat);return 1;
  }
  for(int axis=0;axis<3;++axis) {
   ps.viewangles[axis]=(float)(i*3-axis*17+recoilCase*11);
   *(float *)(raw+0xb0+axis*4)=ps.viewangles[axis];
  }
  rawpm[0x10]=move.cmd.buttons=(pattern==0 || (pattern==1 ? i%40<2 : i%40<20))?BUTTON_ATTACK:0;
  rawpm[0x12]=move.cmd.weapon=(pattern==3 && i>=60 && i<63)?55:weapon;
  modeEvents=events=bodies=bodyContinue=bodyForce=bodyAnim=0;original();
  expectedModeEvents=modeEvents;
  expectedEvents=events;expectedBodies=bodies;expectedContinue=bodyContinue;expectedForce=bodyForce;expectedAnim=bodyAnim;
  modeEvents=events=bodies=bodyContinue=bodyForce=bodyAnim=0;PM_Weapon();
  if (*(int *)(raw+0x138)!=ps.persistant[10] || *(int *)(raw+0x3bc)!=ps.holdable[11] ||
      *(int *)(raw+0xc)!=ps.pm_flags || rawpm[0x10]!=move.cmd.buttons ||
      *(int *)(raw+0x2c)!=ps.weaponTime || *(int *)(raw+0x30)!=ps.weaponDelay ||
      *(int *)(raw+0xa8)!=ps.weaponstate || *(int *)(raw+0x290+weapon*4)!=ps.ammoclip[weapon] ||
      expectedModeEvents!=modeEvents || expectedEvents!=events || *(int *)(raw+0x480)!=ps.weapAnim ||
      expectedBodies!=bodies || expectedContinue!=bodyContinue || expectedForce!=bodyForce || (bodies && bodyAnim!=(expectedAnim==30?ANIM_ET_FIREWEAPONPRONE:ANIM_ET_FIREWEAPON))) {
   fprintf(stderr,"mode%d counter%d/%d flags%x/%x buttons%d/%d\n", mode,*(int *)(raw+0x3bc),ps.holdable[11],*(int *)(raw+0xc),ps.pm_flags,rawpm[0x10],move.cmd.buttons);
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
  if (*(int *)(raw+0x390)!=ps.holdable[0] || *(int *)(raw+0x394)!=ps.holdable[1] ||
      memcmp(rawext+0x64,ext.tceShotAngles,12)) {
   fprintf(stderr,"Shot snapshot mismatch weapon%d frame%d\n",weapon,i);return 1;
  }
  ++frames;if(!ps.ammoclip[weapon])break;
  *(int *)(rawpm+0xc)+=ticks[t];move.cmd.serverTime+=ticks[t];
 }
 }
 printf("%d production PM_Weapon frames matched original timing, ammo, attack animation, hip recoil and instability recovery\n",frames);
 FreeLibrary(dll);return 0;
}

