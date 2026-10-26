#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_LimboPanel_SendSetupMsg(qboolean);
static int team,disabled,weaponCount;static void *focus;static char events[1024];
static void patch(void *a,void *f){DWORD old,tmp;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static int getTeam(void){return team;}static int isDisabled(int w){return disabled;}
static void stats(void){strcat(events,"stats;");}static void send(int x){strcat(events,"UNEXPECTED-SEND;");}
static void sound(int x,int ch){strcat(events,va("sound:%d:%d;",x,ch));}
static void *getClass(void){return NULL;}static int count(void){return weaponCount;}
static void *getFocus(void){return focus;}static void setFocus(void *p){focus=p;strcat(events,"focus-clear;");}
static int inside(void *rect){return 1;}static int getWeapon(int n,int slot,int ignore){return n+1;}
int main(int argc,char **argv){HMODULE h;unsigned char *b;panel_button_t button;int mode,slot,key,number,old,caseCount=0,r1,r2,c1,w1,w2,w3,s1;char expected[1024];
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;memset(&button,0,sizeof(button));button.rect.w=180;button.rect.h=30;
 patch(b+0x434f0,getTeam);patch(CG_LimboPanel_GetTeam,getTeam);patch(b+0x43ca0,isDisabled);patch(CG_LimboPanel_WeaponIsDisabled,isDisabled);patch(b+0x43d20,isDisabled);patch(CG_LimboPanel_RealWeaponIsDisabled,isDisabled);
 patch(b+0x43b20,stats);patch(CG_LimboPanel_RequestWeaponStats,stats);patch(b+0x407f0,send);patch(CG_LimboPanel_SendSetupMsg,send);patch(b+0x65070,sound);patch(trap_S_StartLocalSound,sound);cgs.media.sndLimboSelect=*(int *)(b+0x2588dd8)=123;
 patch(b+0x43560,getClass);patch(CG_LimboPanel_GetPlayerClass,getClass);patch(b+0x43580,count);patch(CG_LimboPanel_WeaponCount,count);
 patch(b+0x88380,getFocus);patch(BG_PanelButtons_GetFocusButton,getFocus);patch(b+0x88390,setFocus);patch(BG_PanelButtons_SetFocusButton,setFocus);patch(b+0x87af0,inside);patch(BG_CursorInRect,inside);patch(b+0x43790,getWeapon);patch(CG_LimboPanel_GetWeaponForNumber,getWeapon);
 for(mode=0;mode<4;++mode)for(team=1;team<=3;++team)for(slot=0;slot<3;++slot)for(key=177;key<=178;++key)for(number=0;number<7;++number)for(old=0;old<3;++old)for(disabled=0;disabled<2;++disabled){
 cgs.ccSelectedWeaponNumber=*(int *)(b+0x3fde380)=slot;cgs.ccSelectedClass=*(int *)(b+0x3fde384)=old;cgs.ccSelectedWeapon=*(int *)(b+0x3fde388)=old;cgs.ccSelectedWeapon2=*(int *)(b+0x3fde38c)=old;cgs.ccSelectedWeapon3=*(int *)(b+0x3fdf38c)=old;
 button.data[0]=slot;button.data[1]=number;weaponCount=number;focus=&button;events[0]=0;r1=0;
 if(mode==0)((void(__cdecl *)(int))(b+0x43bc0))(number);
 if(mode==1)r1=((int(__cdecl *)(void *,int))(b+0x41a20))(&button,key);
 if(mode==2)r1=((int(__cdecl *)(void *,int))(b+0x40b30))(&button,key);
 if(mode==3)r1=((int(__cdecl *)(void *,int))(b+0x41b50))(&button,key);
 strcpy(expected,events);c1=*(int *)(b+0x3fde384);w1=*(int *)(b+0x3fde388);w2=*(int *)(b+0x3fde38c);w3=*(int *)(b+0x3fdf38c);s1=*(int *)(b+0x3fde380);
 focus=&button;events[0]=0;r2=0;
 if(mode==0)CG_LimboPanel_SetSelectedWeaponNum(number);
 if(mode==1)r2=CG_LimboPanel_WeaponLights_KeyDown(&button,key);
 if(mode==2)r2=CG_LimboPanel_ClassButton_KeyDown(&button,key);
 if(mode==3)r2=CG_LimboPanel_WeaponPanel_KeyUp(&button,key);
 if(r1!=r2||strcmp(expected,events)||c1!=cgs.ccSelectedClass||w1!=cgs.ccSelectedWeapon||w2!=cgs.ccSelectedWeapon2||w3!=cgs.ccSelectedWeapon3||s1!=cgs.ccSelectedWeaponNumber){printf("selection mismatch mode%d team%d slot%d key%d num%d old%d disabled%d\n%s\n%s\n",mode,team,slot,key,number,old,disabled,expected,events);return 1;}++caseCount;
 }
 printf("%d whole limbo selection callback comparisons passed\n",caseCount);return 0;
}
