#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern const char* CG_PickupItemText(int);
extern void CG_DrawTimedMenus(void);
static char calls[128];
static void closeMenu(const char*n){strcat(calls,n);strcat(calls,";");}
static void setvar(const char*n,const char*v){strcat(calls,n);strcat(calls,"=");strcat(calls,v);strcat(calls,";");}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b,*item;DWORD protect,tmp;byte saved[60];gitem_t savedSource;int type,model,name,k,n=0;char expected[128];
 const char*names[]={"","Ammo","ammo","apple","A","a","Orange","Health","health","Terrorist flag","^1Armor"};
 const char*(__cdecl*pickup)(int);void(__cdecl*timed)(void);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;pickup=(void*)(b+0x20ff0);timed=(void*)(b+0x21520);item=b+0x98f58+60;
 VirtualProtect(item,60,PAGE_READWRITE,&protect);memcpy(saved,item,60);savedSource=bg_itemlist[1];
 for(type=-1;type<=12;++type)for(model=0;model<2;++model)for(name=0;name<11;++name){
  *(int*)(item+36)=bg_itemlist[1].giType=type;*(const char**)(item+28)=bg_itemlist[1].pickup_name=(char*)names[name];*(char**)(item+16)=bg_itemlist[1].world_model[2]=model?"model":NULL;
  strcpy(expected,pickup(1));if(strcmp(expected,CG_PickupItemText(1))){printf("pickup type%d model%d name%d\n",type,model,name);return 1;}++n;
 }
 memcpy(item,saved,60);bg_itemlist[1]=savedSource;VirtualProtect(item,60,protect,&tmp);
 hook(b+0x80a90,closeMenu);hook(Menus_CloseByName,closeMenu);hook(b+0x64c10,setvar);hook(trap_Cvar_Set,setvar);
 for(k=0;k<12000;++k){int expectedTime;cg.time=*(int*)(b+0x407dea4)=5000;cg.voiceTime=*(int*)(b+0x4080de8)=k%6001-500;calls[0]=0;timed();strcpy(expected,calls);expectedTime=*(int*)(b+0x4080de8);calls[0]=0;CG_DrawTimedMenus();if(cg.voiceTime!=expectedTime||strcmp(calls,expected)){printf("timed%d\n",k);return 1;}++n;}
 printf("%d whole pickup-text/timed-menu original comparisons passed\n",n);return 0;
}
