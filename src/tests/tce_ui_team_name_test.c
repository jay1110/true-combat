#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static const char *value;static char trace[512];
static void get(const char*n,char*out,int size){sprintf(trace+strlen(trace),"G%s;",n);Q_strncpyz(out,value,size);}
static void set(const char*n,const char*v){sprintf(trace+strlen(trace),"S%s=%s;",n,v);}
static e_status stop(int handle){sprintf(trace+strlen(trace),"X%d;",handle);return FMV_EOF;}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b,*p;int n,v,k,c,i,mode,er,ar,cases=0;const char*names[]={"first","SECOND","third","fourth"};int keys[]={K_MOUSE1,K_MOUSE2,K_ENTER,K_KP_ENTER,K_ESCAPE,0};char expected[512];
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
 patch(b+0x20570,get);patch(trap_Cvar_VariableStringBuffer,get);patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x20e90,stop);patch(trap_CIN_StopCinematic,stop);
 for(n=1;n<=4;++n)for(v=0;v<5;++v)for(k=0;k<6;++k)for(c=-1;c<2;++c)for(mode=0;mode<3;++mode){
  uiInfo.teamCount=*(int*)(b+0x2b277c)=n;updateModel=*(int*)(b+0x33204)=0;
  for(i=0;i<n;++i){p=b+0x2b2780+i*0x2c;memset(p,0,0x2c);memset(&uiInfo.teamList[i],0,sizeof(uiInfo.teamList[i]));uiInfo.teamList[i].teamName=*(const char**)p=names[i];uiInfo.teamList[i].cinematic=*(int*)(p+0x28)=c;}
  value=v<4?names[v]:"missing";
  assert(UI_TeamIndexFromName(value)==((int(__cdecl*)(const char*))(b+0xe450))(value));
  trace[0]=0;er=mode==0?((int(__cdecl*)(int,float*,int))(b+0x11050))(0,NULL,keys[k]):((int(__cdecl*)(int,float*,int,int))(b+0x11100))(0,NULL,keys[k],mode==1);strcpy(expected,trace);
  trace[0]=0;ar=mode==0?UI_ClanName_HandleKey(0,NULL,keys[k]):UI_TeamName_HandleKey(0,NULL,keys[k],mode==1);
  assert(er==ar&&!strcmp(expected,trace));assert(updateModel==*(int*)(b+0x33204));
  for(i=0;i<n;++i)assert(uiInfo.teamList[i].cinematic==*(int*)(b+0x2b27a8+i*0x2c));++cases;
 }
 printf("%d original clan/team key and %d team-name lookup comparisons passed\n",cases,cases);FreeLibrary(h);return 0;}
