#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static char trace[128];
static void set(const char*n,const char*v){sprintf(trace+strlen(trace),"%s=%s;",n,v);}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int count,value,k,mode,expected,actual,cases=0;int keys[]={K_MOUSE1,K_MOUSE2,K_ENTER,K_KP_ENTER,K_ESCAPE,0,255};char saved[128];
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;patch(b+0x20530,set);patch(trap_Cvar_Set,set);
 for(count=0;count<=10;++count)for(value=-2;value<=14;++value)for(k=0;k<7;++k)for(mode=0;mode<3;++mode){
  uiInfo.characterCount=*(int*)(b+0x2b2070)=count;uiInfo.botIndex=*(int*)(b+0x2b2074)=value;uiInfo.redBlue=*(int*)(b+0x2b3488)=value;uiInfo.currentCrosshair=*(int*)(b+0x2d3d48)=value;
  trace[0]=0;expected=((int(__cdecl*)(int,float*,int))(b+(mode==0?0x113d0:mode==1?0x11440:0x11480)))(0,NULL,keys[k]);strcpy(saved,trace);
  trace[0]=0;actual=mode==0?UI_BotName_HandleKey(0,NULL,keys[k]):mode==1?UI_RedBlue_HandleKey(0,NULL,keys[k]):UI_Crosshair_HandleKey(0,NULL,keys[k]);
  assert(expected==actual&&!strcmp(saved,trace));assert(uiInfo.botIndex==*(int*)(b+0x2b2074));assert(uiInfo.redBlue==*(int*)(b+0x2b3488));assert(uiInfo.currentCrosshair==*(int*)(b+0x2d3d48));++cases;
 }
 printf("%d original bot/redblue/crosshair input comparisons passed\n",cases);FreeLibrary(h);return 0;}
