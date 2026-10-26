#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
extern void CG_NextWeap(qboolean),CG_PrevWeap(qboolean),CG_FinishWeaponChange(int,int);
static int calls,from,to;
static void finish(int a,int b){++calls;from=a;to=b;}
static void patch(void *address,void *fn){unsigned char *p=address;DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv){
 HMODULE h;unsigned char *b;static snapshot_t snap;unsigned char raw[0x800]={0};
 int direction,w,mask,flags,all,banks,remember,i,count=0,ec,ef,et;unsigned owned[2];
 if(argc!=2 || !(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 /* All selection/bank dependencies run unmodified. Capture the already separately
  * verified finish boundary, including whether it was invoked at all. */
 patch(b+0x74580,finish);patch(CG_FinishWeaponChange,finish);
 cg.snap=&snap;*(void **)(b+0x40352c4)=raw;
 if(memcmp(weapBanksMultiPlayer,b+0x964b8,sizeof(weapBanksMultiPlayer)))return 3;
 memset(weapAlts,0,256);memset(b+0x106e78,0,256);
 for(direction=0;direction<2;++direction)for(w=0;w<64;++w)
 for(mask=0;mask<12;++mask)for(flags=0;flags<3;++flags)
 for(all=0;all<2;++all)for(banks=0;banks<2;++banks)for(remember=0;remember<2;++remember){
  owned[0]=owned[1]=0;
  for(i=1;i<64;++i)if(mask==0 || (mask==1?i==w:(i+mask)%11==0))owned[i/32]|=1u<<(i%32);
  memcpy(cg.predictedPlayerState.weapons,owned,8);memcpy(b+0x407e294,owned,8);
  cg.predictedPlayerState.eFlags=*(int *)(b+0x407df28)=flags==2?EF_MG42_ACTIVE:0;
  snap.ps.eFlags=*(int *)(raw+0x94)=flags==1?0x1000000:0;
  cg.weaponSelect=*(int *)(b+0x407ef70)=w;
  cg_cycleAllWeaps.integer=*(int *)(b+0x4013f4c)=all;
  /* Original exhausted scans read beyond the ten-element bank memory. Zero
   * those unrelated bytes for comparison; production instead bounds the read. */
  memset(b+0x4080f28,0,80);memset(cg.lastWeapSelInBank,0,sizeof(cg.lastWeapSelInBank));
  for(i=0;i<10;++i)cg.lastWeapSelInBank[i]=((int *)(b+0x4080f28))[i]=remember?weapBanksMultiPlayer[i][0]:0;
  calls=0;from=to=-1;((void (__cdecl *)(int))(b+(direction?0x74c10:0x74870)))(banks);
  ec=calls;ef=from;et=to;calls=0;from=to=-1;
  if(direction)CG_PrevWeap(banks);else CG_NextWeap(banks);
  if(calls!=ec || from!=ef || to!=et){printf("bank mismatch dir%d w%d mask%d flags%d all%d banks%d mem%d: %d/%d %d/%d %d/%d\n",direction,w,mask,flags,all,banks,remember,calls,ec,from,ef,to,et);return 1;}
  ++count;
 }
 {
  int cmd,scenario,delay,back,etm,eb;const int delays[]={-1,0,99,100,101,5000};
  for(cmd=0;cmd<3;++cmd)for(w=0;w<64;++w)for(scenario=0;scenario<6;++scenario)
  for(delay=0;delay<6;++delay)for(back=0;back<3;++back){
   memset(cg.predictedPlayerState.weapons,0xff,8);memset(b+0x407e294,0xff,8);
   cg.predictedPlayerState.eFlags=*(int *)(b+0x407df28)=scenario==4?EF_MG42_ACTIVE:0;
   snap.ps.eFlags=*(int *)(raw+0x94)=scenario==3?0x1000000:0;
   snap.ps.pm_type=*(int *)(raw+0x30)=scenario==1?PM_FREEZE:PM_NORMAL;
   snap.ps.weaponstate=*(int *)(raw+0xd4)=scenario==2?9:0;
   cg.zoomval=*(float *)(b+0x407f1d0)=scenario==5?60:0;cg_useWeapsForZoom.integer=1;
   cg.time=*(int *)(b+0x407dea4)=20000;
   cg.weaponSelectTime=*(int *)(b+0x4080e44)=20000-delays[delay];
   cg_weaponCycleDelay.integer=*(int *)(b+0x400f62c)=100;
   cg.weaponSelect=*(int *)(b+0x407ef70)=w;
   cg.switchbackWeapon=*(int *)(b+0x4080f08)=back==0?0:back==1?8:39;
   calls=0;from=to=-1;((void (__cdecl *)(void))(b+(cmd==0?0x75000:cmd==1?0x750a0:0x750e0)))();
   ec=calls;ef=from;et=to;etm=*(int *)(b+0x4080e44);eb=*(int *)(b+0x4080f08);
   calls=0;from=to=-1;if(cmd==0)CG_LastWeaponUsed_f();else if(cmd==1)CG_NextWeaponInBank_f();else CG_PrevWeaponInBank_f();
   if(calls!=ec || from!=ef || to!=et || cg.weaponSelectTime!=etm || cg.switchbackWeapon!=eb){
    printf("command mismatch cmd%d weapon%d scenario%d delay%d back%d\n",cmd,w,scenario,delay,back);return 4;}
   ++count;
  }
 }
 printf("%d complete next/previous bank-controller comparisons passed\n",count);return 0;
}
