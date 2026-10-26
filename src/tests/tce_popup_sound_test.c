#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
static char name[128];static int count,entity,buffer,nullOrigin;
static int sound(const char*n,float*o,int e,int b){++count;Q_strncpyz(name,n,sizeof(name));entity=e;buffer=b;nullOrigin=o==NULL;return 1;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;static byte raw[512];static centity_t cent;void(__cdecl*original)(void*);int kind,team,effect,density,local,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x58180);hook(b+0x60960,sound);hook(CG_SoundPlaySoundScript,sound);
 for(kind=-1;kind<10;++kind)for(team=-1;team<4;++team)for(effect=-1;effect<4;++effect)for(density=-1;density<4;++density)for(local=0;local<4;++local){int ec,ee,eb,en;char expected[128];cg.clientNum=*(int*)(b+0x40352a4)=7;cgs.clientinfo[7].team=*(int*)(b+0x2580a98+7*0x1c0)=local;cent.currentState.effect1Time=*(int*)(raw+0x10c)=kind;cent.currentState.teamNum=*(int*)(raw+0x108)=team;cent.currentState.effect2Time=*(int*)(raw+0x110)=effect;cent.currentState.density=*(int*)(raw+0xf4)=density;
  count=0;entity=buffer=nullOrigin=-99;name[0]=0;original(raw);ec=count;ee=entity;eb=buffer;en=nullOrigin;strcpy(expected,name);count=0;entity=buffer=nullOrigin=-99;name[0]=0;CG_PlayPMItemSound(&cent);if(ec!=count||ee!=entity||eb!=buffer||en!=nullOrigin||strcmp(expected,name)){printf("popup sound%d/%d/%d/%d/%d %s/%s\n",kind,team,effect,density,local,expected,name);return 1;}++n;
 }
 printf("%d whole popup-sound comparisons passed\n",n);return 0;
}

