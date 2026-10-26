#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static char trace[2048];static int result;
static void set(const char*n,const char*v){sprintf(trace+strlen(trace),"S%s=%s;",n,v);}
static void preview(rectDef_t*r,float scale,vec4_t color,qboolean net){sprintf(trace+strlen(trace),"preview%d;",net);}
static int play(const char*n,int x,int y,int w,int h,int flags){sprintf(trace+strlen(trace),"play:%s,%d,%d,%d,%d,%d;",n,x,y,w,h,flags);return result;}
static e_status run(int h){sprintf(trace+strlen(trace),"run%d;",h);return FMV_PLAY;}
static void extents(int h,int x,int y,int w,int ht){sprintf(trace+strlen(trace),"extent:%d,%d,%d,%d,%d;",h,x,y,w,ht);}
static void draw(int h){sprintf(trace+strlen(trace),"draw%d;",h);}
static struct{unsigned char*p,bytes[5];}hooks[12];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<12);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}


int main(int argc,char**argv){HMODULE h;unsigned char*b;int net,g,map,handle,res,r,j,cases=0;int modes[]={2,4,5,7};char expected[2048];int origHandles[4];rectDef_t rect;vec4_t color={1,1,1,1};const char*names[]={"obj_delta","obj_northport","obj_snow","obj_village"};
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x5590,preview);patch(UI_DrawMapPreview,preview);patch(b+0x20e60,play);patch(trap_CIN_PlayCinematic,play);patch(b+0x20eb0,run);patch(trap_CIN_RunCinematic,run);patch(b+0x20ef0,extents);patch(trap_CIN_SetExtents,extents);patch(b+0x20ed0,draw);patch(trap_CIN_DrawCinematic,draw);
for(net=0;net<2;++net)for(g=0;g<4;++g)for(map=-1;map<=3;++map)for(handle=-3;handle<=1;++handle)for(res=-1;res<=1;++res)for(r=0;r<2;++r){rect.x=r?-.71f:3.13f;rect.y=11.37f;rect.w=107.91f;rect.h=62.73f;result=res;ui_gameType.integer=*(int*)(b+0x2d926c)=0;uiInfo.gameTypes[0].gtEnum=*(int*)(b+0x2b328c)=modes[g];ui_netGameType.integer=*(int*)(b+0x2dc50c)=modes[g];ui_currentMap.integer=*(int*)(b+0x2913cc)=map;ui_currentNetMap.integer=*(int*)(b+0x291f0c)=map;uiInfo.mapCount=*(int*)(b+0x2b52a8)=2;uiInfo.campaignCount=*(int*)(b+0x2b96ac)=1;for(j=0;j<4;++j){uiInfo.mapList[j].mapLoadName=*(const char**)(b+0x2b52b0+j*0x88)=names[j];uiInfo.mapList[j].cinematic=*(int*)(b+0x2b52bc+j*0x88)=handle;}
trace[0]=0;((void(__cdecl*)(rectDef_t*,float,float*,int))(b+0xf070))(&rect,.23f,color,net);strcpy(expected,trace);for(j=0;j<4;++j)origHandles[j]=*(int*)(b+0x2b52bc+j*0x88);trace[0]=0;UI_DrawMapCinematic(&rect,.23f,color,net);assert(!strcmp(expected,trace));assert(ui_currentMap.integer==*(int*)(b+0x2913cc)&&ui_currentNetMap.integer==*(int*)(b+0x291f0c));for(j=0;j<4;++j)assert(origHandles[j]==uiInfo.mapList[j].cinematic);++cases;}
restore();assert(FreeLibrary(h));printf("%d original map-cinematic comparisons passed\n",cases);return 0;}
