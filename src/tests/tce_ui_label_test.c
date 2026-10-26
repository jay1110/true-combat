#define WIN32_LEAN_AND_MEAN
#define NOGDI
/* Calls patched below must remain calls, including the included production TU. */
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
static float handicap;static const char*clan;static char trace[2048];
static float get(const char*n){sprintf(trace+strlen(trace),"G%s;",n);return handicap;}
static void cvarstring(const char*n,char*out,int size){sprintf(trace+strlen(trace),"S%s;",n);Q_strncpyz(out,clan,size);}
static void paint(float x,float y,float scale,vec4_t color,const char*text,float adjust,int limit,int style){sprintf(trace+strlen(trace),"P%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%s,%.9g,%d,%d;",x,y,scale,color[0],color[1],color[2],color[3],text,adjust,limit,style);}
static void patch(void*at,void*fn){unsigned char*p=at;DWORD o,t;assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int i,mode,cases=0;rectDef_t rect;vec4_t color={.1f,.2f,.3f,.4f};char expected[2048];const char*names[]={"","Clan","^1Colored name","Mixed % text"};
 assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;patch(b+0x20550,get);patch(trap_Cvar_VariableValue,get);patch(b+0x20570,cvarstring);patch(trap_Cvar_VariableStringBuffer,cvarstring);patch(b+0x48f0,paint);patch(Text_Paint,paint);
 for(i=-20;i<=440;++i)for(mode=0;mode<2;++mode){float scale=.1f+(i+20)%7*.1f;int style=(i+20)%6;handicap=i*.25f;clan=names[(i+20)%4];rect.x=i*.5f;rect.y=i*.25f;rect.w=200;rect.h=16;
  trace[0]=0;((void(__cdecl*)(rectDef_t*,float,vec4_t,int))(b+(mode?0xec40:0xebd0)))(&rect,scale,color,style);strcpy(expected,trace);
  trace[0]=0;if(mode)UI_DrawClanName(&rect,scale,color,style);else UI_DrawHandicap(&rect,scale,color,style);assert(!strcmp(expected,trace));++cases;
 }printf("%d original UI handicap/clan label callback comparisons passed\n",cases);FreeLibrary(h);return 0;}

