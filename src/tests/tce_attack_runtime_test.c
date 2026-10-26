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
 int weapons[]={3,8,10},delays[]={0,1,32,100},intervals[]={50,80,150},ticks[]={8,16,33};
 int w,d,n,t,pattern,weapon,prone,clip,recoilCase,frames=0; tce_weaponDef_t *defs;void (__cdecl *original)(void),(__cdecl *originalAdjust)(void);
 playerState_t ps;pmove_t move;pmoveExt_t ext;bg_character_t character;
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;
 original=(void *)(base+0xfc80);originalAdjust=(void *)(base+0x9c30);
 /* Exercise the production selectors, not just the imported mapping helpers. */
 gearDef.parsed=1;
 for(i=-32;i<256;++i) {
  int (__cdecl *raise)(int)=(void *)(base+0x88c0);
  int (__cdecl *drop)(int)=(void *)(base+0x8930);
  if(PM_RaiseAnimForWeapon(i)!=raise(i) || PM_DropAnimForWeapon(i)!=drop(i))return 3;
 }
 hook(base+0x2520,body);hook(base+0x8680,eventOriginal);
 hook((unsigned char *)BG_AnimScriptEvent,body);hook((unsigned char *)BG_AddPredictableEventToPlayerstate,eventSource);
 for(w=0;w<3;++w)for(d=0;d<4;++d)for(n=0;n<3;++n)for(t=0;t<3;++t)for(pattern=0;pattern<3;++pattern)for(prone=0;prone<2;++prone)for(clip=1;clip<=30;clip+=29)for(recoilCase=0;recoilCase<4;++recoilCase) {
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
      expectedBodies!=bodies || expectedContinue!=bodyContinue || expectedForce!=bodyForce || (bodies && bodyAnim!=(expectedAnim==30?ANIM_ET_FIREWEAPONPRONE:ANIM_ET_FIREWEAPON))) {
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

 {
  void (__cdecl *begin)(int,int,int)=(void *)(base+0x8e90);
  int old,next,scenario,expectedNext,expectedState,expectedTime,expectedWeapAnim,expectedEvent,cases=0;
  for(old=0;old<64;++old)for(next=-1;next<66;++next)for(scenario=0;scenario<12;++scenario) {
   memset(raw,0,sizeof(raw));memset(rawpm,0,sizeof(rawpm));memset(rawext,0,sizeof(rawext));memset(rawchar,0,sizeof(rawchar));
   memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&ext,0,sizeof(ext));memset(&character,0,sizeof(character));
   *(void **)(base+0x4ba9edc)=rawpm;*(void **)rawpm=raw;*(void **)(rawpm+4)=rawext;*(void **)(rawpm+8)=rawchar;
   pm=&move;move.ps=&ps;move.pmext=&ext;move.character=&character;move.skill=skills;gearDef.parsed=1;
   ps.pm_flags=*(int *)(raw+0xc)=scenario==1?PMF_RESPAWNED:0;
   ps.weaponstate=scenario==2?WEAPON_DROPPING:scenario==3?WEAPON_DROPPING_TORELOAD:WEAPON_READY;
   *(int *)(raw+0xa8)=scenario==2?3:scenario==3?4:0;
   ps.weaponDelay=*(int *)(raw+0x30)=scenario==4?1:0;
   ps.grenadeTimeLeft=*(int *)(raw+0x34)=scenario==5?10:-1;
   ps.eFlags=*(int *)(raw+0x68)=scenario==6?EF_PRONE:0;
   move.waterlevel=*(int *)(rawpm+0x110)=scenario==7?3:0;
   ps.weaponTime=*(int *)(raw+0x2c)=19;
   ps.nextWeapon=*(int *)(raw+0x41c)=12;
   ps.weapAnim=*(int *)(raw+0x480)=2;
   weapAlts[old]=((int *)(base+0x106e78))[old]=scenario>=9?next:0;
   if(next>0 && next<64 && scenario!=8) {
    COM_BitSet(ps.weapons,next);((unsigned *)(raw+0x3d4))[next/32]|=1u<<(next%32);
    ps.ammoclip[next]=*(int *)(raw+0x290+next*4)=1;
   }
   switchEvent=-1;begin(old,next,scenario==10);
   expectedNext=*(int *)(raw+0x41c);expectedState=*(int *)(raw+0xa8);
   expectedTime=*(int *)(raw+0x2c);expectedWeapAnim=*(int *)(raw+0x480);expectedEvent=switchEvent;
   switchEvent=-1;PM_BeginWeaponChange(old,next,scenario==10);
   if(ps.nextWeapon!=expectedNext || ps.weaponTime!=expectedTime || ps.weapAnim!=expectedWeapAnim ||
     (ps.weaponstate==WEAPON_DROPPING?3:ps.weaponstate==WEAPON_DROPPING_TORELOAD?4:0)!=expectedState ||
     switchEvent!=expectedEvent || ps.grenadeTimeLeft!=*(int *)(raw+0x34)) {
    printf("begin mismatch old%d next%d scenario%d time%d/%d anim%d/%d event%d/%d\n",old,next,scenario,ps.weaponTime,expectedTime,ps.weapAnim,expectedWeapAnim,switchEvent,expectedEvent);return 4;
   }
   ++cases;
  }
  printf("%d production begin-switch states matched original\n",cases);
 }

 {
  void (__cdecl *finish)(void)=(void *)(base+0x12bd0);
  int old,next,scenario,cases=0,expectedWeapon,expectedState,expectedTime,expectedAnim,expectedSide,expectedSpread;
  int expectedBody,expectedBodyCount,expectedConditionCount,expectedConditionWeapon;
  hook(base+0x25f0,condition);hook((unsigned char *)BG_UpdateConditionValue,condition);
  for(old=0;old<64;++old)for(next=-1;next<66;++next)for(scenario=0;scenario<8;++scenario) {
   memset(raw,0,sizeof(raw));memset(rawpm,0,sizeof(rawpm));memset(rawext,0,sizeof(rawext));memset(rawchar,0,sizeof(rawchar));
   memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&ext,0,sizeof(ext));memset(&character,0,sizeof(character));
   *(void **)(base+0x4ba9edc)=rawpm;*(void **)rawpm=raw;*(void **)(rawpm+4)=rawext;*(void **)(rawpm+8)=rawchar;
   pm=&move;move.ps=&ps;move.pmext=&ext;move.character=&character;move.skill=skills;gearDef.parsed=1;
   ps.weapon=*(int *)(raw+0xa4)=old;ps.nextWeapon=*(int *)(raw+0x41c)=next;
   ps.weaponstate=scenario&1?WEAPON_DROPPING_TORELOAD:WEAPON_DROPPING;*(int *)(raw+0xa8)=scenario&1?4:3;
   ps.eFlags=*(int *)(raw+0x68)=scenario&2?EF_PRONE:0;
   ps.weaponTime=*(int *)(raw+0x2c)=19;ps.weapAnim=*(int *)(raw+0x480)=2;
   ext.silencedSideArm=*(int *)(rawext+0xc)=7;
   ps.aimSpreadScale=*(int *)(raw+0x48c)=13;ps.aimSpreadScaleFloat=*(float *)(raw+0x488)=13;
   weapAlts[old]=((int *)(base+0x106e78))[old]=scenario&4?next:0;
   if(next>=0 && next<64) {COM_BitSet(ps.weapons,next);((unsigned *)(raw+0x3d4))[next/32]|=1u<<(next%32);}
   memset(ps.ammoclip,scenario&1?1:0,sizeof(ps.ammoclip));memset(raw+0x290,scenario&1?1:0,256);
   bodies=conditionCalls=0;finish();expectedWeapon=*(int *)(raw+0xa4);expectedState=*(int *)(raw+0xa8);
   expectedTime=*(int *)(raw+0x2c);expectedAnim=*(int *)(raw+0x480);expectedSide=*(int *)(rawext+0xc);expectedSpread=*(int *)(raw+0x48c);
   expectedBody=bodyAnim;expectedBodyCount=bodies;expectedConditionCount=conditionCalls;expectedConditionWeapon=conditionWeapon;
   bodies=conditionCalls=0;PM_FinishWeaponChange();
   if(ps.weapon!=expectedWeapon || (ps.weaponstate==WEAPON_RAISING_TORELOAD?2:1)!=expectedState ||
      ps.weaponTime!=expectedTime || ps.weapAnim!=expectedAnim || ext.silencedSideArm!=expectedSide || ps.aimSpreadScale!=expectedSpread ||
      ps.aimSpreadScaleFloat!=*(float *)(raw+0x488) || bodies!=expectedBodyCount || conditionCalls!=expectedConditionCount ||
      (conditionCalls && conditionWeapon!=expectedConditionWeapon)) {
    printf("finish mismatch old%d next%d scenario%d time%d/%d anim%d/%d bodies%d/%d\n",old,next,scenario,ps.weaponTime,expectedTime,ps.weapAnim,expectedAnim,bodies,expectedBodyCount);return 5;
   }
   if(bodies) {
    int mapped=expectedBody==26?ANIM_ET_DO_ALT_WEAPON_MODE:expectedBody==28?ANIM_ET_DO_ALT_WEAPON_MODE_PRONE:expectedBody==32?ANIM_ET_RAISEWEAPONPRONE:ANIM_ET_RAISEWEAPON;
    if(bodyAnim!=mapped)return 6;
   }
   ++cases;
  }
  printf("%d production finish-switch transitions matched original\n",cases);
 }
 FreeLibrary(dll);return 0;
}

