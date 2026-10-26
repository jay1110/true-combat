#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../cgame/tce_weapon_media.h"
#include <windows.h>
#include <stdio.h>
static int team,cls,slots[3];static char events[2048];
static int getTeam(void){return team;}
static int getClass(void){return cls;}
static int getSlot(int slot){return slots[slot];}
static void command(const char *s){strcat(events,s);}
static void closeEvent(int event,int force){strcat(events,va("close:%d:%d;",event,force));}
static void popup(int type,const char *s,int shader){strcat(events,va("popup:%d:%d:%s;",type,shader,s));}
static void patch(void *a,void *f){DWORD old,tmp;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static int disabled;
static int isDisabled(int n){return disabled;}
static int getter(int number,int slot,int ignore){return 1000+number*10+slot;}
int main(int argc,char **argv){HMODULE h;unsigned char *b;int force,current,w,n=0,e1,e2,index;char expected[2048];
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x434f0,getTeam);patch(CG_LimboPanel_GetTeam,getTeam);
 patch(b+0x43530,getClass);patch(CG_LimboPanel_GetClass,getClass);
 patch(b+0x64da0,command);patch(trap_SendClientCommand,command);
 patch(b+0x4f1b0,closeEvent);patch(CG_EventHandling,closeEvent);
 patch(b+0x57970,popup);patch(CG_AddPMItemBig,popup);
 patch(b+0x43790,getter);patch(CG_LimboPanel_GetWeaponForNumber,getter);
 cgs.ccSelectedWeapon=*(int *)(b+0x3fde388)=3;cgs.ccSelectedWeapon2=*(int *)(b+0x3fde38c)=5;cgs.ccSelectedWeapon3=*(int *)(b+0x3fdf38c)=7;
 for(index=-2;index<5;++index)if(CG_LimboPanel_GetSelectedWeaponForSlot(index)!=((int(__cdecl *)(int))(b+0x43a60))(index))return 3;
 patch(b+0x43ca0,isDisabled);patch(CG_LimboPanel_WeaponIsDisabled,isDisabled);
 for(index=-1;index<4;++index)for(disabled=0;disabled<2;++disabled){
  cgs.ccSelectedWeaponNumber=*(int *)(b+0x3fde380)=index;
  cgs.ccSelectedWeapon=*(int *)(b+0x3fde388)=3;
  cgs.ccSelectedWeapon2=*(int *)(b+0x3fde38c)=5;
  cgs.ccSelectedWeapon3=*(int *)(b+0x3fdf38c)=7;
  if(CG_LimboPanel_GetSelectedWeaponNum()!=((int(__cdecl *)(void))(b+0x43ae0))() || cgs.ccSelectedWeapon!=*(int *)(b+0x3fde388))return 4;
  if(CG_LimboPanel_GetSelectedWeapon()!=((int(__cdecl *)(void))(b+0x43ac0))())return 5;
 }
 patch(b+0x43a60,getSlot);patch(CG_LimboPanel_GetSelectedWeaponForSlot,getSlot);
 cg.clientNum=*(int *)(b+0x40352a4)=0;cgs.media.pmImages[4]=*(int *)(b+0x259d19c)=123;
 for(force=0;force<2;++force)for(current=0;current<4;++current)for(team=0;team<4;++team)for(cls=0;cls<7;++cls)for(w=0;w<64;++w){
  cgs.clientinfo[0].team=*(int *)(b+0x2580a98)=current;
  slots[1]=w;slots[0]=2;slots[2]=36;
  strcpy(tce_cg_weapons[w].deployMenuShortName,"Test weapon");strcpy((char *)b+0x484af6c+w*0xfd0,"Test weapon");
  *(int *)(b+0x3fdecb4)=*(int *)(b+0x3fdecb8)=cgs.limboLoadoutSelected=cgs.limboLoadoutModified=0;
  events[0]=0;((void(__cdecl *)(int))(b+0x407f0))(force);strcpy(expected,events);e1=*(int *)(b+0x3fdecb4);e2=*(int *)(b+0x3fdecb8);
  events[0]=0;CG_LimboPanel_SendSetupMsg(force);
  if(strcmp(expected,events)||e1!=cgs.limboLoadoutSelected||e2!=cgs.limboLoadoutModified){printf("deployment mismatch force%d current%d team%d class%d weapon%d\nEXPECTED %s\nACTUAL %s\n",force,current,team,cls,w,expected,events);return 1;}++n;
 }
 printf("%d full deployment sender comparisons; three-slot getter matched\n",n);return 0;
}
