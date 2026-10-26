#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static char trace[8192];static qtime_t clockTime;static const char*debug;static float protocol;
static void add(const char*f,...){va_list a;va_start(a,f);vsprintf(trace+strlen(trace),f,a);va_end(a);}
static int realtime(qtime_t*q){*q=clockTime;add("time;");return 123;}
static void set(const char*n,const char*v){add("set:%s=%s;",n,v);}
static void mark(int s,int i,qboolean v){add("mark:%d,%d,%d;",s,i,v);}
static void reset(int s){add("reset:%d;",s);}
static void command(int m,const char*s){add("cmd:%d:%s;",m,s);}
static void string(const char*n,char*b,int size){add("string:%s;",n);Q_strncpyz(b,debug,size);}
static float value(const char*n){add("value:%s;",n);return protocol;}
static struct{unsigned char*p,bytes[5];}hooks[14];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<14);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}


int main(int argc,char**argv){HMODULE h;unsigned char*b;int mode,source,month,tm,d,seed,cases=0;char expected[8192];int offsets[]={0x2cf2fc,0x2cf2f0,0x2d1304,0x2d1308,0x2d130c},expectedState[5],actual[5];int i;assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
patch(b+0x20e40,realtime);patch(trap_RealTime,realtime);patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x20ca0,mark);patch(trap_LAN_MarkServerVisible,mark);patch(b+0x20d00,reset);patch(trap_LAN_ResetPings,reset);patch(b+0x205d0,command);patch(trap_Cmd_ExecuteText,command);patch(b+0x20570,string);patch(trap_Cvar_VariableStringBuffer,string);patch(b+0x20550,value);patch(trap_Cvar_VariableValue,value);
for(mode=0;mode<3;++mode)for(source=0;source<3;++source)for(month=0;month<12;++month)for(tm=0;tm<3;++tm)for(d=0;d<2;++d)for(seed=0;seed<2;++seed){
memset(&clockTime,0,sizeof(clockTime));clockTime.tm_mon=month;clockTime.tm_mday=seed?31:1;clockTime.tm_year=126;clockTime.tm_hour=tm==0?0:tm==1?9:23;clockTime.tm_min=tm==0?0:tm==1?9:59;debug=d?"84":"";protocol=seed?84.75f:82.0f;
ui_netSource.integer=*(int*)(b+0x28fd4c)=source;uiInfo.uiDC.realTime=*(int*)(b+0x292140)=seed?19873:-100;uiInfo.serverStatus.refreshActive=*(int*)(b+offsets[0])=seed;uiInfo.serverStatus.refreshtime=*(int*)(b+offsets[1])=77;uiInfo.serverStatus.numDisplayServers=*(int*)(b+offsets[2])=13;uiInfo.serverStatus.numPlayersOnServers=*(int*)(b+offsets[3])=19;uiInfo.serverStatus.nextDisplayRefresh=*(int*)(b+offsets[4])=41;
trace[0]=0;if(mode==2)((void(__cdecl*)(void))(b+0x12880))();else ((void(__cdecl*)(int))(b+0x126d0))(mode);strcpy(expected,trace);for(i=0;i<5;++i)expectedState[i]=*(int*)(b+offsets[i]);trace[0]=0;if(mode==2)UI_UpdatePendingPings();else UI_StartServerRefresh(mode);actual[0]=uiInfo.serverStatus.refreshActive;actual[1]=uiInfo.serverStatus.refreshtime;actual[2]=uiInfo.serverStatus.numDisplayServers;actual[3]=uiInfo.serverStatus.numPlayersOnServers;actual[4]=uiInfo.serverStatus.nextDisplayRefresh;
if(strcmp(trace,expected)||memcmp(actual,expectedState,sizeof(actual))){fprintf(stderr,"start mismatch mode%d source%d month%d time%d debug%d seed%d\nO%s\nS%s\n",mode,source,month,tm,d,seed,expected,trace);return 1;}++cases;}
restore();assert(FreeLibrary(h));printf("%d original refresh-start comparisons passed\n",cases);return 0;}
