#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static char trace[2048];static int servers;static const char*stamp;
static int count(int source){sprintf(trace+strlen(trace),"C%d;",source);return servers;}
static void string(const char*n,char*b,int size){sprintf(trace+strlen(trace),"V%s;",n);Q_strncpyz(b,stamp,size);}
static const char* translate(const char*s){sprintf(trace+strlen(trace),"L%s;",s);return s;}
static void draw(float x,float y,float scale,vec4_t color,const char*s,float adjust,int limit,int style){sprintf(trace+strlen(trace),"P:%.9g,%.9g,%.9g:%08x,%08x,%08x,%08x:%s:%d;",x,y,scale,*(unsigned*)(color+0),*(unsigned*)(color+1),*(unsigned*)(color+2),*(unsigned*)(color+3),s,style);}
static struct{unsigned char*p,bytes[5];}hooks[8];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<8);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}


int main(int argc,char**argv){HMODULE h;byte*b;int active,source,n,t,c,st,cases=0;char expected[2048];rectDef_t r={3.13f,11.71f,123.19f,44.77f};vec4_t color;int times[]={-151,-75,-1,0,1,74,75,149,150,999,12345};const char*stamps[]={"","Oct-5, 2026 at 09:07","abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789LONGTAIL"};assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(byte*)h;
patch(b+0x20b30,count);patch(trap_LAN_GetServerCount,count);patch(b+0x20570,string);patch(trap_Cvar_VariableStringBuffer,string);patch(b+0x20f40,translate);patch(trap_TranslateString,translate);patch(b+0x48f0,draw);patch(Text_Paint,draw);
for(active=0;active<2;++active)for(source=0;source<3;++source)for(n=-1;n<3;++n)for(t=0;t<11;++t)for(c=0;c<3;++c)for(st=0;st<3;++st){servers=n;stamp=stamps[st];uiInfo.serverStatus.refreshActive=*(int*)(b+0x2cf2fc)=active;ui_netSource.integer=*(int*)(b+0x28fd4c)=source;uiInfo.uiDC.realTime=*(int*)(b+0x292140)=times[t];color[0]=c==0?1:c==1?.13f:-.17f;color[1]=.37f;color[2]=c==2?1.37f:.71f;color[3]=.91f;
trace[0]=0;((void(__cdecl*)(rectDef_t*,float,float*,int))(b+0x10760))(&r,.23f,color,3);strcpy(expected,trace);trace[0]=0;UI_DrawServerRefreshDate(&r,.23f,color,3);if(strcmp(expected,trace)){fprintf(stderr,"refreshdate mismatch active%d source%d n%d time%d color%d stamp%d\nO%s\nS%s\n",active,source,n,times[t],c,st,expected,trace);return 1;}++cases;}
restore();assert(FreeLibrary(h));printf("%d original refresh-date comparisons passed\n",cases);return 0;}
