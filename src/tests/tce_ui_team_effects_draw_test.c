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
int main(int argc,char**argv){HMODULE h;unsigned char*b;int n,j,i,blue,style,cases=0;rectDef_t r;vec4_t c={.1f,.2f,.3f,.4f};char expected[4096];const char*names[]={"Alpha","Beta","Gamma"};const char*queries[]={"","Alpha","bETA","Gamma","unknown"};
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
patch(b+0x20570,cvarstring);patch(trap_Cvar_VariableStringBuffer,cvarstring);patch(b+0x26f0,pic);patch(UI_DrawHandlePic,pic);patch(b+0x48f0,paint);patch(Text_Paint,paint);
for(i=0;i<3;++i){*(const char**)(b+0x2b2780+i*44)=names[i];uiInfo.teamList[i].teamName=names[i];}
for(n=0;n<=3;++n)for(j=0;j<5;++j)for(blue=0;blue<2;++blue)for(style=0;style<6;++style){team=queries[j];*(int*)(b+0x2b277c)=uiInfo.teamCount=n;r.x=style*7.25f;r.y=j*-3.125f;r.w=50;r.h=22;trace[0]=0;((void(__cdecl*)(rectDef_t*,float,vec4_t,int,int))(b+0xef80))(&r,.3f,c,blue,style);strcpy(expected,trace);trace[0]=0;UI_DrawTeamName(&r,.3f,c,blue,style);assert(!strcmp(trace,expected));++cases;}
*(int*)(b+0x2b0374)=uiInfo.uiDC.Assets.fxBasePic=217;
for(i=0;i<7;++i){*(int*)(b+0x2b0378+i*4)=uiInfo.uiDC.Assets.fxPic[i]=80+i;}
for(i=0;i<7;++i)for(j=-20;j<=20;++j){*(int*)(b+0x2d815c)=uiInfo.effectsColor=i;r.x=j*2.125f;r.y=j*.25f;r.w=j*19;r.h=j*7;trace[0]=0;((void(__cdecl*)(rectDef_t*,float,vec4_t))(b+0xf010))(&r,.3f,c);strcpy(expected,trace);trace[0]=0;UI_DrawEffects(&r,.3f,c);assert(!strcmp(trace,expected));++cases;}
printf("%d original UI team/effects draw comparisons passed\n",cases);FreeLibrary(h);return 0;}
