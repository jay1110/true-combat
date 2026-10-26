#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static const char*team;static char trace[4096];static int shader;
static void cvarstring(const char*n,char*out,int size){sprintf(trace+strlen(trace),"G%s;",n);Q_strncpyz(out,team,size);}
static void colorfn(const float*c){if(c)sprintf(trace+strlen(trace),"C%.9g,%.9g,%.9g,%.9g;",c[0],c[1],c[2],c[3]);else strcat(trace,"Cnull;");}
static int reg(const char*n){sprintf(trace+strlen(trace),"R%s;",n);return ++shader;}
static void pic(float x,float y,float w,float h,int icon){sprintf(trace+strlen(trace),"D%.9g,%.9g,%.9g,%.9g,%d;",x,y,w,h,icon);}
static void paint(float x,float y,float scale,vec4_t c,const char*t,float adjust,int limit,int style){sprintf(trace+strlen(trace),"P%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%s,%.9g,%d,%d;",x,y,scale,c[0],c[1],c[2],c[3],t,adjust,limit,style);}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void set(const char*n,const char*v){sprintf(trace+strlen(trace),"S%s=%s;",n,v);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int net,idx,unlocked,style,i,cases=0;rectDef_t r={-3.75f,8.99f,120.75f,60.125f};vec4_t c={.1f,.2f,.3f,.4f};char expected[4096];const char*names[]={"","^1Campaign","Name % percent","Sentinel"};
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x20570,cvarstring);patch(trap_Cvar_VariableStringBuffer,cvarstring);patch(b+0x48f0,paint);patch(Text_Paint,paint);
for(net=0;net<2;++net)for(idx=-1;idx<=4;++idx)for(unlocked=0;unlocked<3;++unlocked)for(style=0;style<6;++style){*(int*)(b+0x2b96ac)=uiInfo.campaignCount=3;*(int*)(b+0x2d902c)=ui_currentCampaign.integer=idx;*(int*)(b+0x28fc2c)=ui_currentNetCampaign.integer=idx;for(i=0;i<4;++i){*(const char**)(b+0x2b96b0+i*116+4)=names[i];*(int*)(b+0x2b96b0+i*116+96)=unlocked;uiInfo.campaignList[i].campaignName=names[i];uiInfo.campaignList[i].unlocked=unlocked;}trace[0]=0;((void(__cdecl*)(rectDef_t*,float,vec4_t,int,int))(b+0xf3f0))(&r,.3f,c,style,net);strcpy(expected,trace);trace[0]=0;UI_DrawCampaignName(&r,.3f,c,style,net);assert(!strcmp(trace,expected));assert(*(int*)(b+0x2d902c)==ui_currentCampaign.integer);assert(*(int*)(b+0x28fc2c)==ui_currentNetCampaign.integer);++cases;}
for(i=0;i<4;++i)for(style=0;style<6;++style){team=names[i];trace[0]=0;((void(__cdecl*)(rectDef_t*,float,vec4_t,int))(b+0x102d0))(&r,.3f,c,style);strcpy(expected,trace);trace[0]=0;UI_DrawOpponentName(&r,.3f,c,style);assert(!strcmp(trace,expected));++cases;}
printf("%d original UI campaign/opponent label comparisons passed\n",cases);FreeLibrary(h);return 0;}
