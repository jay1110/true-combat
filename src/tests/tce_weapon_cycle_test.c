#include "../cgame/tce_weapon_cycle.h"
#include <windows.h>
#include <stdio.h>
int testCycleHud(HMODULE dll);
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base;static unsigned char snap[0x800];
 int direction,scenario,d,t,w,cases=0;
 int delays[]={-1,0,1599,1600,1601,5000},timeouts[]={1499,1500,1501,3000};
 tce_weaponCycle_t s;
 void (__cdecl *next)(void),(__cdecl *prev)(void);
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
 base=(unsigned char *)dll;next=(void *)(base+0x75120);prev=(void *)(base+0x751c0);
 for(direction=-1;direction<=1;direction+=2)for(scenario=0;scenario<8;++scenario)
 for(d=0;d<6;++d)for(t=0;t<4;++t)for(w=0;w<64;++w) {
  s.hasSnapshot=scenario!=0;*(void **)(base+0x40352c4)=s.hasSnapshot?snap:NULL;
  s.snapshotFlags=*(int *)(snap+0x94)=scenario==1?0x1000000:0;
  s.snapshotPmFlags=*(int *)(snap+0x38)=scenario==2?0x1000:0;
  s.time=*(int *)(base+0x407dea4)=10000;
  s.weaponSelectTime=*(int *)(base+0x4080e44)=10000-delays[d];
  s.snapshotWeaponState=*(int *)(snap+0xd4)=scenario==3?1:0;
  s.predictedWeaponState=*(int *)(base+0x407df68)=scenario==4?1:0;
  s.snapshotWeapon=*(int *)(snap+0xd0)=w;
  s.predictedWeapon=*(int *)(base+0x407df64)=scenario==5?(w+1)%64:w;
  s.cycleTime=*(int *)(base+0x40a4ba0)=10000-timeouts[t];
  s.cycleOffset=*(int *)(base+0x40a4ba4)=scenario==6?-7:5;
  s.weaponSelect=*(int *)(base+0x407ef70)=w;
  if(direction>0)next();else prev();
  TCE_CG_CycleWeaponCommand(&s,direction);
  if(s.cycleTime!=*(int *)(base+0x40a4ba0) || s.cycleOffset!=*(int *)(base+0x40a4ba4) ||
     s.weaponSelect!=*(int *)(base+0x407ef70) || s.weaponSelectTime!=*(int *)(base+0x4080e44))return 1;
  ++cases;
 }
 printf("%d original Windows next/previous command states matched\n",cases);return testCycleHud(dll);
}
