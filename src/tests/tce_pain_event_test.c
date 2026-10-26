#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
static char record[256];
static int custom(int client,const char*name){sprintf(record,"custom:%d:%s;",client,name);return 713;}
static void sound(const vec3_t origin,int client,int channel,int handle){char s[96];sprintf(s,"sound:%d:%d:%d:%d",origin!=NULL,client,channel,handle);strcat(record,s);}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;static byte originalCent[2048];static centity_t cent;void(__cdecl*original)(void*,int);int health,delta,direction,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x35c80);
 hook(b+0x52d60,custom);hook(CG_CustomSound,custom);hook(b+0x64fb0,sound);hook(trap_S_StartSound,sound);
 for(health=-10;health<=110;++health)for(delta=490;delta<=510;++delta)for(direction=0;direction<4;++direction){char expected[256];int et,ed;
  cg.time=*(int*)(b+0x407dea4)=1000;cent.currentState.number=*(int*)originalCent=health+10;
  cent.pe.painTime=*(int*)(originalCent+0x40c)=1000-delta;cent.pe.painDirection=*(int*)(originalCent+0x414)=direction;
  record[0]=0;original(originalCent,health);strcpy(expected,record);et=*(int*)(originalCent+0x40c);ed=*(int*)(originalCent+0x414);
  record[0]=0;CG_PainEvent(&cent,health,direction&1);
  if(strcmp(record,expected)||cent.pe.painTime!=et||cent.pe.painDirection!=ed){printf("pain h%d delta%d direction%d %s/%s\n",health,delta,direction,record,expected);return 1;}++n;
 }
 printf("%d whole CG_PainEvent comparisons passed\n",n);return 0;
}
