#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
static int selectedStat,maxObjectives,team,count;static int calls[8];static char command[64];
static int getStat(void){calls[count++]=1;return selectedStat;}
static int getMax(void){calls[count++]=2;return maxObjectives;}
static int getTeam(void){calls[count++]=3;return team;}
static void send(const char*s){calls[count++]=4;Q_strncpyz(command,s,sizeof(command));}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE m;byte*b;void(__cdecl*orig[2])(void);int k,op,n,copy[8];char text[64];if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;orig[0]=(void*)(b+0x43b20);orig[1]=(void*)(b+0x43b40);hook(b+0x43c10,getStat);hook(CG_LimboPanel_GetSelectedWeaponStat,getStat);hook(b+0x40dc0,getMax);hook(CG_LimboPanel_GetMaxObjectives,getMax);hook(b+0x434f0,getTeam);hook(CG_LimboPanel_GetTeam,getTeam);hook(b+0x64da0,send);hook(trap_SendClientCommand,send);
 for(k=0;k<4096;k++)for(op=0;op<2;op++){
  selectedStat=k%69-3;maxObjectives=k%9-1;team=(k/9)%5;cgs.ccSelectedObjective=*(int*)(b+0x3fde378)=(k/45)%10-2;cgs.ccRequestedObjective=*(int*)(b+0x3fde3bc)=91;cgs.ccLastObjectiveRequestTime=*(int*)(b+0x3fde3c0)=92;cg.time=*(int*)(b+0x407dea4)=k*7-8192;
  count=0;command[0]=0;orig[op]();n=count;memcpy(copy,calls,sizeof(copy));strcpy(text,command);count=0;command[0]=0;if(op)CG_LimboPanel_RequestObjective();else CG_LimboPanel_RequestWeaponStats();
  if(n!=count||memcmp(copy,calls,n*sizeof(int))||strcmp(text,command)||cgs.ccRequestedObjective!=*(int*)(b+0x3fde3bc)||cgs.ccLastObjectiveRequestTime!=*(int*)(b+0x3fde3c0)){printf("limbo requests case%d op%d\n",k,op);return 1;}
 }puts("8192 whole limbo request comparisons passed (4096 each)");return 0;}
