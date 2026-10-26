#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
static byte*base;static int originalRun,disabled,count;static int events[8][5];
static int lookup(int number,int slot,qboolean ignore){events[count][0]=1;events[count][1]=number;events[count][2]=slot;events[count++][3]=ignore;return 700+number*7+slot;}
static qboolean isDisabled(int number){events[count][0]=2;events[count++][1]=number;return disabled;}
static void request(void){events[count][0]=3;events[count][1]=originalRun?*(int*)(base+0x3fde388):cgs.ccSelectedWeapon;events[count][2]=originalRun?*(int*)(base+0x3fde38c):cgs.ccSelectedWeapon2;events[count][3]=originalRun?*(int*)(base+0x3fdf38c):cgs.ccSelectedWeapon3;events[count++][4]=originalRun?*(int*)(base+0x3fde380):cgs.ccSelectedWeaponNumber;}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE m;int(__cdecl*get)(int);void(__cdecl*set)(int);int k,op,arg,n,expected,result,copy[8][5];if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;base=(byte*)m;get=(void*)(base+0x43a60);set=(void*)(base+0x43bc0);hook(base+0x43790,lookup);hook(CG_LimboPanel_GetWeaponForNumber,lookup);hook(base+0x43ca0,isDisabled);hook(CG_LimboPanel_WeaponIsDisabled,isDisabled);hook(base+0x43b20,request);hook(CG_LimboPanel_RequestWeaponStats,request);
 for(k=0;k<4096;k++)for(op=0;op<2;op++){
  cgs.ccSelectedWeapon=*(int*)(base+0x3fde388)=k%67-2;cgs.ccSelectedWeapon2=*(int*)(base+0x3fde38c)=(k/7)%67-2;cgs.ccSelectedWeapon3=*(int*)(base+0x3fdf38c)=(k/13)%67-2;cgs.ccSelectedWeaponNumber=*(int*)(base+0x3fde380)=(k/19)%6-2;disabled=(k/114)%3;arg=op?k%69-3:k%7-2;
  originalRun=1;count=0;memset(events,0,sizeof(events));expected=0;if(op)set(arg);else expected=get(arg);n=count;memcpy(copy,events,sizeof(copy));
  originalRun=0;count=0;memset(events,0,sizeof(events));result=0;if(op)CG_LimboPanel_SetSelectedWeaponNum(arg);else result=CG_LimboPanel_GetSelectedWeaponForSlot(arg);
  if(expected!=result||n!=count||memcmp(copy,events,sizeof(copy))||cgs.ccSelectedWeapon!=*(int*)(base+0x3fde388)||cgs.ccSelectedWeapon2!=*(int*)(base+0x3fde38c)||cgs.ccSelectedWeapon3!=*(int*)(base+0x3fdf38c)||cgs.ccSelectedWeaponNumber!=*(int*)(base+0x3fde380)){printf("limbo weapon case%d op%d\n",k,op);return 1;}
 }puts("8192 whole limbo weapon selection comparisons passed (4096 each)");return 0;}
