#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
extern int CG_LimboPanel_TeamCount(weapon_t weap);
extern qboolean CG_IsHeavyWeapon(weapon_t weap);
static int count,events[16][2],team,selectedClass,total,users,heavy;static bg_playerclass_t info;static byte rawInfo[256];static int originalRun;
static void rec(int op,int value){events[count][0]=op;events[count++][1]=value;}
static int getTeam(void){rec(1,0);return team;}static int getClass(void){rec(2,0);return selectedClass;}
static int role(int cls){rec(3,cls);return cls%3;}
static bg_playerclass_t*classInfo(void){rec(4,0);return originalRun?(void*)rawInfo:&info;}
static int isHeavy(int weapon){rec(5,weapon);return heavy;}
static int teamCount(int weapon){rec(6,weapon);return weapon==-1?total:users;}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
#define H(o,n,f) hook(b+o,f);hook(n,f)
int main(int argc,char**argv){HMODULE m;byte*b;int(__cdecl*orig[2])(int);int k,op,arg,result,expected,n,copy[16][2],i;static float restrictions[]={0,.1f,.33333334f,.5f,.9f,1.0f,-.1f};if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;orig[0]=(void*)(b+0x43ca0);orig[1]=(void*)(b+0x43d20);
 H(0x434f0,CG_LimboPanel_GetTeam,getTeam);H(0x43530,CG_LimboPanel_GetClass,getClass);H(0x8250,BG_WolfClassToTCE,role);H(0x43560,CG_LimboPanel_GetPlayerClass,classInfo);H(0x43c80,CG_IsHeavyWeapon,isHeavy);H(0x43c20,CG_LimboPanel_TeamCount,teamCount);
 for(k=0;k<12288;k++)for(op=0;op<2;op++){
  team=k%4;selectedClass=(k/4)%7;total=(k/28)%65;users=(k/1820)%9;heavy=(k/7)%3;arg=op?k%64:k%24;
  for(i=0;i<24;i++)info.classWeapons[i]=*(int*)(rawInfo+16+i*4)=(k+i)%64;
  cg.clientNum=*(int*)(b+0x40352a4)=0;cgs.clientinfo[0].tceScorePlayerClass=*(int*)(b+0x2580bcc)=(k/13)%7;cgs.clientinfo[0].tceClassRating=*(int*)(b+0x2580bc4)=k%31-2;cgs.clientinfo[0].tcePreviousClassRating=*(int*)(b+0x2580bc8)=(k/3)%31-2;
  for(i=0;i<3;i++)gearDef.requiredSkill[k%64][i]=*(int*)(b+0x4bab200+((k%64)*3+i)*4)=(k/11+i)%31;
  cgs.weaponRestrictions=*(float*)(b+0x257e210)=restrictions[(k/5)%7];
  originalRun=1;count=0;memset(events,0,sizeof(events));expected=orig[op](arg);n=count;memcpy(copy,events,sizeof(copy));originalRun=0;count=0;memset(events,0,sizeof(events));result=op?CG_LimboPanel_RealWeaponIsDisabled(arg):CG_LimboPanel_WeaponIsDisabled(arg);
  if(expected!=result||n!=count||memcmp(copy,events,sizeof(copy))){printf("restrictions case%d op%d result%d/%d calls%d/%d\n",k,op,expected,result,n,count);for(i=0;i<n||i<count;i++)printf(" call%d original %d,%d source %d,%d\n",i,copy[i][0],copy[i][1],events[i][0],events[i][1]);return 1;}
 }puts("24576 whole limbo weapon restriction comparisons passed (12288 each)");return 0;}
