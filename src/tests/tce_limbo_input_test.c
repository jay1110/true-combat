#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern qboolean CG_LimboPanel_ClassButton_KeyDown(panel_button_t*,int);
extern qboolean CG_LimboPanel_WeaponPanel_KeyUp(panel_button_t*,int);
extern qboolean CG_LimboPanel_RealWeaponIsDisabled(weapon_t);
static int run,team,count,weaponCount,selected,mask,blocked,focusMatch,probe;
static int events[256][6];static panel_button_t button;static byte raw[256];
static void ev(int id,int a,int b,int c){int*p=events[count++];p[0]=id;p[1]=a;p[2]=b;p[3]=c;}
static int getTeam(void){ev(1,0,0,0);return team;}
static void sound(int s,int c){ev(2,s,c,0);}
static void request(void){ev(3,0,0,0);}
static panel_button_t*focus(void){ev(4,0,0,0);return focusMatch?(run?(void*)raw:&button):NULL;}
static void setfocus(panel_button_t*p){ev(5,p!=NULL,0,0);}
static bg_playerclass_t*getclass(void){ev(6,0,0,0);return NULL;}
static int weapons(void){ev(7,0,0,0);return weaponCount;}
static qboolean cursor(rectDef_t*r){int*p=events[count++];p[0]=8;memcpy(p+1,r,16);return (mask>>(probe++%8))&1;}
static int getselected(void){ev(9,0,0,0);return selected;}
static weapon_t lookup(int n,int s,qboolean i){ev(10,n,s,i);return (weapon_t)(n+17);}
static qboolean disabled(weapon_t w){ev(11,w,0,0);return blocked&&(w&1);}
static void setselected(int n){ev(12,n,0,0);selected=n;}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE m;byte*b;int(__cdecl*orig[2])(void*,int);int k,op,key,n,r,s,expected,copy[256][6],initial;
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;orig[0]=(void*)(b+0x40b30);orig[1]=(void*)(b+0x41b50);
#define H(off,fn,repl) hook(b+off,repl);hook(fn,repl)
 H(0x434f0,CG_LimboPanel_GetTeam,getTeam);H(0x65070,trap_S_StartLocalSound,sound);H(0x43b20,CG_LimboPanel_RequestWeaponStats,request);H(0x88380,BG_PanelButtons_GetFocusButton,focus);H(0x88390,BG_PanelButtons_SetFocusButton,setfocus);H(0x43560,CG_LimboPanel_GetPlayerClass,getclass);H(0x43580,CG_LimboPanel_WeaponCount,weapons);H(0x87af0,BG_CursorInRect,cursor);H(0x43ae0,CG_LimboPanel_GetSelectedWeaponNum,getselected);H(0x43790,CG_LimboPanel_GetWeaponForNumber,lookup);H(0x43d20,CG_LimboPanel_RealWeaponIsDisabled,disabled);H(0x43bc0,CG_LimboPanel_SetSelectedWeaponNum,setselected);
 for(k=0;k<8192;k++)for(op=0;op<2;op++){
  memset(&button,0,sizeof(button));memset(raw,0,sizeof(raw));team=k%5;key=k%7?K_MOUSE1:K_MOUSE2;focusMatch=k%11!=0;weaponCount=(k/5)%9-1;initial=(k/45)%9-2;mask=k%256;blocked=(k/23)%2;
  button.rect.x=(float)(k%19)*.37f;button.rect.y=(float)(k%79)*.13f;button.rect.w=17.3f;button.rect.h=(k%4)?13.7f:-2.3f;memcpy(raw+8,&button.rect,16);button.data[1]=*(int*)(raw+28)=k%7;
  cgs.ccSelectedClass=*(int*)(b+0x3fde384)=(k/7)%7;cgs.ccSelectedWeaponNumber=*(int*)(b+0x3fde380)=(k/19)%4;cgs.media.sndLimboSelect=*(int*)(b+0x2588dd8)=k-300;
  run=1;count=probe=0;selected=initial;memset(events,0,sizeof(events));r=orig[op](raw,key);n=count;expected=selected;memcpy(copy,events,sizeof(copy));
  run=0;count=probe=0;selected=initial;memset(events,0,sizeof(events));s=op?CG_LimboPanel_WeaponPanel_KeyUp(&button,key):CG_LimboPanel_ClassButton_KeyDown(&button,key);
  if(r!=s||n!=count||expected!=selected||memcmp(copy,events,sizeof(copy))||cgs.ccSelectedClass!=*(int*)(b+0x3fde384)){printf("limbo input case%d op%d result%d/%d calls%d/%d selected%d/%d\n",k,op,r,s,n,count,expected,selected);return 1;}
 }puts("16384 whole limbo input comparisons passed (8192 each)");return 0;}
