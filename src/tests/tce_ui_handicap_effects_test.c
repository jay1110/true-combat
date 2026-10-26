#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static float value;static char trace[256];
static float get(const char*n){sprintf(trace+strlen(trace),"G%s;",n);return value;}
static void set(const char*n,const char*v){sprintf(trace+strlen(trace),"S%s=%s;",n,v);}
static void setvalue(const char*n,float v){sprintf(trace+strlen(trace),"V%s=%.9g;",n,v);}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int v,k,mode,er,ar,cases=0;int keys[]={K_MOUSE1,K_MOUSE2,K_ENTER,K_KP_ENTER,K_ESCAPE,0};char expected[256];
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
 patch(b+0x20550,get);patch(trap_Cvar_VariableValue,get);patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x20590,setvalue);patch(trap_Cvar_SetValue,setvalue);
 for(v=-20;v<=440;++v)for(k=0;k<6;++k)for(mode=0;mode<2;++mode){value=v*.25f;uiInfo.effectsColor=*(int*)(b+0x2d815c)=v%13;
  trace[0]=0;er=((int(__cdecl*)(int,float*,int))(b+(mode==0?0x10f50:0x10fe0)))(0,NULL,keys[k]);strcpy(expected,trace);
  trace[0]=0;ar=mode==0?UI_Handicap_HandleKey(0,NULL,keys[k]):UI_Effects_HandleKey(0,NULL,keys[k]);
  assert(er==ar&&!strcmp(expected,trace));assert(uiInfo.effectsColor==*(int*)(b+0x2d815c));++cases;
 }
 printf("%d original handicap/effects input comparisons passed\n",cases);FreeLibrary(h);return 0;}
