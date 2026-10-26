/* Actual production aim-input branch vs complete original PM_Weapon. */
#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
static int event;
static unsigned char savedState[0x700],savedMove[0x200],savedExt[0x200];
static unsigned char *cancelBase,*cancelState,*cancelMove,*cancelExt;
static int savedLadder;
static int noop(void){return 0;}
static void cancelBoundary(void){memcpy(cancelState,savedState,sizeof(savedState));memcpy(cancelMove,savedMove,sizeof(savedMove));memcpy(cancelExt,savedExt,sizeof(savedExt));*(int *)(cancelBase+0x4ba9ecc)=savedLadder;}
static void hook(void *a,void *f){unsigned char *p=a;DWORD o,t;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void oe(int e){event=e;}
static void se(int e,int p,playerState_t *s){if(e==EV_TCE_TOGGLE_AIMING)event=134;}
static int body(void *a,void *b,int c,int d,int e){return 0;}
int main(int argc,char **argv){
 HMODULE h;unsigned char *b;static unsigned char raw[0x700],rpm[0x200],ext[0x200],ch[0x900];
 playerState_t ps;pmove_t move;pmoveExt_t px;bg_character_t character;int skills[8]={0};
 int flags,press,seed,w,scope,gate,expected,cases=0,i;int weapons[]={2,3,8,10,45,46,47,1,21,19,12,15};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 hook(b+0x8680,oe);hook(BG_AddPredictableEventToPlayerstate,se);hook(b+0x2520,body);hook(BG_AnimScriptEvent,body);
 gearDef.parsed=1;
 for(w=0;w<12;++w)for(flags=0;flags<8;++flags)for(press=0;press<2;++press)for(scope=0;scope<2;++scope)for(gate=0;gate<12;++gate){
  int weapon=weapons[w];seed=(cases*17331)&65535;
  memset(raw,0,sizeof(raw));memset(rpm,0,sizeof(rpm));memset(ext,0,sizeof(ext));memset(ch,0,sizeof(ch));
  memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&px,0,sizeof(px));memset(&character,0,sizeof(character));memset(&pml,0,sizeof(pml));
  memset(b+0x4ba9e40,0,0x94);*(void **)(b+0x4ba9edc)=rpm;*(void **)rpm=raw;*(void **)(rpm+4)=ext;*(void **)(rpm+8)=ch;*(void **)(rpm+0x118)=skills;
  pm=&move;move.ps=&ps;move.pmext=&px;move.character=&character;move.skill=skills;
  ps.stats[STAT_HEALTH]=*(int *)(raw+0xd0)=100;ps.persistant[PERS_TEAM]=*(int *)(raw+0x11c)=2;
  ps.weapon=*(int *)(raw+0xa4)=weapon;move.cmd.weapon=rpm[0x12]=weapon;
  move.cmd.serverTime=*(int *)(rpm+0xc)=1000;move.cmd.wbuttons=rpm[0x11]=press?WBUTTON_ZOOM:0;
  ps.stats[8]=*(int *)(raw+0xf0)=(flags&3)*4|(flags&4?0x8000:0);
  ps.stats[7]=*(int *)(raw+0xec)=seed;ps.holdable[5]=*(int *)(raw+0x3a4)=2000;ps.holdable[6]=*(int *)(raw+0x3a8)=2000;
  memset(weaponDef,0,sizeof(weaponDef));weaponDef[weapon].parsed=1;weaponDef[weapon].noTacMode=scope;weaponDef[weapon].maxclip=30;weaponDef[weapon].uses=1;
  memcpy(b+0x4c1df20,weaponDef,sizeof(weaponDef));
  ps.ammoclip[weapon]=*(int *)(raw+0x290+weapon*4)=10;
  if(gate==1)ps.weaponTime=*(int *)(raw+0x2c)=100;
  if(gate==2)ps.stats[8]=*(int *)(raw+0xf0)=ps.stats[8]|0x1000;
  if(gate==3)move.waterlevel=*(int *)(rpm+0x110)=3;
  if(gate==4){move.waterlevel=*(int *)(rpm+0x110)=1;ps.groundEntityNum=*(int *)(raw+0x50)=ENTITYNUM_NONE;}
  if(gate==5)pml.ladder=*(int *)(b+0x4ba9ecc)=1;
  if(gate==6)ps.velocity[2]=*(float *)(raw+0x28)=-361;
  if(gate==7)px.proneTime=*(int *)(ext+0x34)=801;
  if(gate==8)px.proneTime=*(int *)(ext+0x34)=-801;
  if(gate==9)ps.velocity[2]=*(float *)(raw+0x28)=-360;
  if(gate==10){move.cmd.buttons=rpm[0x10]=BUTTON_SPRINT;move.cmd.forwardmove=rpm[0x20]=64;}
  if(gate==11)ps.weaponstate=*(int *)(raw+0xa8)=WEAPON_RELOADING;
  event=0;((void(__cdecl *)(void))(b+0xfc80))();expected=event==134?134:0;
  event=0;if(gate==0 && flags==0 && press && w<7) PM_Weapon(); else PM_TCEUpdateAimInput(qfalse);
  if(event!=expected || (ps.stats[8]&0x800c)!=(*(int *)(raw+0xf0)&0x800c) || (expected && (ps.stats[7]!=*(int *)(raw+0xec)||ps.stats[11]!=*(int *)(raw+0xfc)||ps.stats[13]!=*(int *)(raw+0x104)||ps.holdable[5]!=*(int *)(raw+0x3a4)||ps.holdable[6]!=*(int *)(raw+0x3a8)))){
   printf("aim mismatch w%d flags%x press%d scope%d gate%d event%d/%d flags%x/%x phase%d/%d hold%d,%d/%d,%d\n",weapon,flags,press,scope,gate,event,expected,ps.stats[8],*(int *)(raw+0xf0),ps.stats[13],*(int *)(raw+0x104),ps.holdable[5],ps.holdable[6],*(int *)(raw+0x3a4),*(int *)(raw+0x3a8));return 1;
  }++cases;
 }
 {
  const int hooks[]={0x2440,0x28b0,0x8e90,0x99e0,0xa3b0,0xb120,0xb630,0xbeb0,0xc9a0,0xd0e0,0xd140,0xd4b0,0xd590,0xd8a0,0xe210,0xe2c0,0xe490,0xef60,0xf100,0xf3c0,0xfbd0,0x12fa0,0x65830};
  for(i=0;i<sizeof(hooks)/sizeof(hooks[0]);++i)hook(b+hooks[i],noop);
  hook(b+0xfc80,cancelBoundary);cancelBase=b;cancelState=raw;cancelMove=rpm;cancelExt=ext;
  for(flags=0;flags<8;++flags)for(gate=0;gate<16;++gate)for(w=0;w<13;++w){
   memset(raw,0,sizeof(raw));memset(rpm,0,sizeof(rpm));memset(ext,0,sizeof(ext));memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&px,0,sizeof(px));memset(&pml,0,sizeof(pml));
   pm=&move;move.ps=&ps;move.pmext=&px;move.character=&character;ps.weapon=8;ps.weaponstate=w;ps.stats[STAT_HEALTH]=100;ps.persistant[PERS_TEAM]=2;
   ps.stats[8]=(flags&3)*4;ps.persistant[14]=flags&4?8:0;ps.stats[7]=(cases*17331)&65535;ps.aimSpreadScaleFloat=17.25f;move.cmd.serverTime=1000;
   if(gate==1)move.waterlevel=3;
   if(gate==2){move.waterlevel=1;ps.groundEntityNum=ENTITYNUM_NONE;}
   if(gate==3)ps.velocity[2]=-361;
   if(gate==4)pml.ladder=1;
   if(gate==5)ps.stats[STAT_HEALTH]=0;
   if(gate==6)ps.stats[8]|=0x1000;
   if(gate==7)px.proneTime=801;
   if(gate==8)px.proneTime=-801;
   if(gate==9)ps.eFlags=EF_PRONE_MOVING;
   if(gate>=10&&gate<=12){move.cmd.buttons=BUTTON_SPRINT;move.cmd.forwardmove=64;if(gate==11)ps.pm_flags=PMF_DUCKED;if(gate==12)ps.eFlags=EF_PRONE;}
   if(gate==13)px.proneTime=800;
   if(gate==14)px.proneTime=-800;
   if(gate==15)ps.velocity[2]=-360;
   memcpy(raw,&ps,0x420);*(float *)(raw+0x488)=ps.aimSpreadScaleFloat;
   *(void **)rpm=raw;*(void **)(rpm+4)=ext;*(void **)(rpm+8)=ch;memcpy(rpm+0xc,&move.cmd,sizeof(move.cmd));*(int *)(rpm+0x110)=move.waterlevel;
   *(int *)(ext+0x34)=px.proneTime;savedLadder=pml.ladder;
   memcpy(savedState,raw,sizeof(raw));memcpy(savedMove,rpm,sizeof(rpm));memcpy(savedExt,ext,sizeof(ext));
   /* Earlier movement dependencies are outside this branch. Restore their
      output at the original PM_Weapon call boundary, then run native tail. */
   *(int *)(raw+4)=0;*(int *)(raw+0xd0)=100;event=0;((void(__cdecl *)(void *))(b+0xc180))(rpm);expected=event;
   event=0;PM_TCECancelAim();
   if(event!=expected||ps.stats[8]!=*(int *)(raw+0xf0)||ps.stats[7]!=*(int *)(raw+0xec)||ps.stats[11]!=*(int *)(raw+0xfc)||ps.stats[13]!=*(int *)(raw+0x104)||ps.aimSpreadScaleFloat!=*(float *)(raw+0x488)){
    printf("cancel mismatch flags%d gate%d state%d event%d/%d flags%x/%x phase%d/%d\n",flags,gate,w,event,expected,ps.stats[8],*(int *)(raw+0xf0),ps.stats[13],*(int *)(raw+0x104));return 4;
   }++cases;
  }
 }
 printf("%d actual production aim-input branches compared to complete original PM_Weapon\n",cases);return 0;
}
