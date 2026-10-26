/* Compare the cycle-producing portion of actual PM_Weapon to Windows TC.
 * Stop at reload entry, so this test does not claim whole-controller parity. */
#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
static jmp_buf stop;
static int reached, eventCount, lastEvent;
static void hook(unsigned char *p,void *fn) {
 DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);
 p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
 FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
static void reloadStop(int weapon) {reached=1;longjmp(stop,1);}
static void originalEvent(int e) {++eventCount;lastEvent=e;}
static void sourceEvent(int e,int parm,playerState_t *ps) {
 ++eventCount;lastEvent=e==EV_TCE_RELOAD_BOLT?133:e==EV_TCE_RELOAD_PUMP2?132:e;
}
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base,raw[0x700],rawpm[0x200],rawext[0x200],rawchar[0x900];
 playerState_t ps;pmove_t move;pmoveExt_t ext;bg_character_t character;
 tce_weaponDef_t *defs;int skills[8]={0},kind,semi,mode,skip,clip,delay,time,state,count=0;
 int gotReach,gotCount,gotEvent,wi,weapon;const int weapons[]={25,32,46,48,49,51};void (__cdecl *original)(void);
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;original=(void *)(base+0xfc80);
 hook(base+0x9350,reloadStop);hook((unsigned char *)PM_CheckForReload,reloadStop);
 hook(base+0x8680,originalEvent);hook((unsigned char *)BG_AddPredictableEventToPlayerstate,sourceEvent);
 for(wi=0;wi<6;++wi)for(kind=0;kind<4;++kind)for(semi=0;semi<2;++semi)for(mode=0;mode<3;++mode)
 for(skip=0;skip<2;++skip)for(clip=0;clip<2;++clip)for(delay=0;delay<2;++delay)
 for(time=0;time<3;++time)for(state=0;state<2;++state) {
 weapon=weapons[wi];
 memset(raw,0,sizeof(raw));memset(rawpm,0,sizeof(rawpm));memset(rawext,0,sizeof(rawext));memset(rawchar,0,sizeof(rawchar));
 memset(&ps,0,sizeof(ps));memset(&move,0,sizeof(move));memset(&ext,0,sizeof(ext));memset(&character,0,sizeof(character));
 *(void **)(base+0x4ba9edc)=rawpm;*(void **)rawpm=raw;*(void **)(rawpm+4)=rawext;*(void **)(rawpm+8)=rawchar;
 *(void **)(rawpm+0x118)=skills;*(int *)(rawpm+0xc)=1000;rawpm[0x12]=weapon;*(int *)(base+0x4ba9e68)=16;
 *(int *)(raw+0xd0)=ps.stats[STAT_HEALTH]=100;*(int *)(raw+0x11c)=ps.persistant[PERS_TEAM]=2;
 *(int *)(raw+0xa4)=ps.weapon=weapon;*(int *)(raw+0xa8)=ps.weaponstate=state?WEAPON_FIRING:WEAPON_READY;
 *(int *)(raw+0x2c)=ps.weaponTime=time==0?0:time==1?8:64;
 *(int *)(raw+0x30)=ps.weaponDelay=delay?64:0;
 *(int *)(raw+0x138)=ps.persistant[10]=mode;
 *(int *)(raw+0xf0)=ps.stats[STAT_TCE_WEAPON_FLAGS]=skip?0x804:4;
 *(int *)(raw+0x290+weapon*4)=ps.ammoclip[weapon]=clip;
 *(int *)(raw+0x480)=ps.weapAnim=131;
 defs=(void *)(base+0x4c1df20);memset(defs,0,64*sizeof(*defs));
 defs[weapon].parsed=1;defs[weapon].uses=1;defs[weapon].pump=kind&1;defs[weapon].bolt=(kind>>1)&1;
 defs[weapon].semiauto=semi;defs[weapon].fullauto=1;defs[weapon].burst=3;
 defs[weapon].unknown_19c=321;defs[weapon].unknown_1a0=789;
 memcpy(weaponDef,defs,sizeof(weaponDef));gearDef.parsed=1;
 pm=&move;move.ps=&ps;move.pmext=&ext;move.character=&character;move.skill=skills;
 move.cmd.serverTime=1000;move.cmd.weapon=weapon;pml.msec=16;pml.frametime=.016f;
 reached=eventCount=lastEvent=0;if(!setjmp(stop))original();gotReach=reached;gotCount=eventCount;gotEvent=lastEvent;
 reached=eventCount=lastEvent=0;if(!setjmp(stop))PM_Weapon();
 if(gotReach!=reached || gotCount!=eventCount || gotEvent!=lastEvent ||
 *(int *)(raw+0x2c)!=ps.weaponTime || *(int *)(raw+0x30)!=ps.weaponDelay ||
 *(int *)(raw+0xa8)!=ps.weaponstate || *(int *)(raw+0xf0)!=ps.stats[STAT_TCE_WEAPON_FLAGS] ||
 *(int *)(raw+0x480)!=ps.weapAnim) {
 fprintf(stderr,"Cycle mismatch kind%d semi%d mode%d skip%d clip%d delay%d time%d state%d: reached%d/%d events%d,%d/%d,%d time%d/%d state%d/%d anim%d/%d flags%x/%x\n",kind,semi,mode,skip,clip,delay,time,state,gotReach,reached,gotCount,gotEvent,eventCount,lastEvent,*(int *)(raw+0x2c),ps.weaponTime,*(int *)(raw+0xa8),ps.weaponstate,*(int *)(raw+0x480),ps.weapAnim,*(int *)(raw+0xf0),ps.stats[STAT_TCE_WEAPON_FLAGS]);return 1;
 }
 ++count;
 }
 printf("PASS: %d original PM_Weapon cycle-boundary cases; whole controller remains open\n",count);return 0;
}
