#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static float value;static char trace[256];
static float get(const char*n){sprintf(trace+strlen(trace),"G%s;",n);return value;}
static void set(const char*n,const char*v){sprintf(trace+strlen(trace),"S%s=%s;",n,v);}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int c,v,k,blue,num,er,ar,cases=0;int keys[]={K_MOUSE1,K_MOUSE2,K_ENTER,K_KP_ENTER,K_ESCAPE,0};char expected[256];
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
 patch(b+0x20550,get);patch(trap_Cvar_VariableValue,get);patch(b+0x20530,set);patch(trap_Cvar_Set,set);
 for(c=0;c<=8;++c)for(v=-5;v<=25;++v)for(k=0;k<6;++k)for(blue=0;blue<2;++blue)for(num=1;num<=5;++num){
  value=v*.5f;uiInfo.characterCount=*(int*)(b+0x2b2070)=c;
  trace[0]=0;er=((int(__cdecl*)(int,float*,int,int,int))(b+0x111a0))(0,NULL,keys[k],blue,num);strcpy(expected,trace);
  trace[0]=0;ar=UI_TeamMember_HandleKey(0,NULL,keys[k],blue,num);assert(er==ar&&!strcmp(expected,trace));++cases;
 }
 printf("%d original team-member slot input comparisons passed\n",cases);FreeLibrary(h);return 0;}
