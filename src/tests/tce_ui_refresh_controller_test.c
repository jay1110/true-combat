#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static char trace[8192];static int serverCount,pings;
static void add(const char*f,...){va_list a;va_start(a,f);vsprintf(trace+strlen(trace),f,a);va_end(a);}
static int count(int source){add("C%d;",source);return serverCount;}
static int ping(int source){add("P%d;",source);return pings;}
static void list(int force){add("B%d;",force);}
static void print(const char*f,...){va_list a;va_start(a,f);vsprintf(trace+strlen(trace),f,a);va_end(a);}
static struct{unsigned char*p,bytes[5];}hooks[8];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<8);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}

int main(int argc,char**argv){HMODULE h;unsigned char*b;int mode,active,source,n,p,t,shown,cases=0;int nums[]={-1,0,1,9},times[]={999,1000,1001};char expected[8192];int ea,et;
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
patch(b+0x20b30,count);patch(trap_LAN_GetServerCount,count);patch(b+0x20b10,ping);patch(trap_LAN_UpdateVisiblePings,ping);patch(b+0xb420,list);patch(UI_BuildServerDisplayList,list);patch(b+0x2380,print);patch(Com_Printf,print);
for(mode=0;mode<2;++mode)for(active=0;active<2;++active)for(source=0;source<3;++source)for(n=0;n<4;++n)for(p=0;p<2;++p)for(t=0;t<3;++t)for(shown=0;shown<3;++shown){
serverCount=nums[n];pings=p;ui_netSource.integer=*(int*)(b+0x28fd4c)=source;uiInfo.uiDC.realTime=*(int*)(b+0x292140)=times[t];uiInfo.serverStatus.refreshtime=*(int*)(b+0x2cf2f0)=1000;uiInfo.serverStatus.refreshActive=*(int*)(b+0x2cf2fc)=active;uiInfo.serverStatus.numDisplayServers=*(int*)(b+0x2d1304)=shown;uiInfo.serverStatus.numPlayersOnServers=*(int*)(b+0x2d1308)=17;
trace[0]=0;((void(__cdecl*)(void))(b+(mode?0x12620:0x125c0)))();strcpy(expected,trace);ea=*(int*)(b+0x2cf2fc);et=*(int*)(b+0x2cf2f0);trace[0]=0;if(mode)UI_DoServerRefresh();else UI_StopServerRefresh();if(strcmp(trace,expected)||ea!=uiInfo.serverStatus.refreshActive||et!=uiInfo.serverStatus.refreshtime){fprintf(stderr,"refresh mismatch mode%d active%d source%d count%d ping%d time%d shown%d\nO%s\nS%s\n",mode,active,source,serverCount,p,times[t],shown,expected,trace);return 1;}assert(uiInfo.serverStatus.numDisplayServers==shown);assert(uiInfo.serverStatus.numPlayersOnServers==17);++cases;}
restore();assert(FreeLibrary(h));printf("%d original refresh-controller comparisons passed\n",cases);return 0;}
