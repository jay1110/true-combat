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
int main(int argc,char**argv){HMODULE h;unsigned char*b;int i,j,k,n,pass,cases=0;rectDef_t r;vec4_t c={.1f,.2f,.3f,.4f};char expected[4096];int icons[3][3];const char*names[]={"Alpha","Beta","Gamma"};const char*images[]={"gfx/alpha","gfx/beta","gfx/gamma"};const char*queries[]={"","Alpha","bETA","Gamma","missing"};const char*types[]={"All","Objective","Bodycount","Reinforced Objective","^1custom"};int initial[]={-1,0,77};
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
patch(b+0x20570,cvarstring);patch(trap_Cvar_VariableStringBuffer,cvarstring);patch(b+0x20810,colorfn);patch(trap_R_SetColor,colorfn);patch(b+0x20730,reg);patch(trap_R_RegisterShaderNoMip,reg);patch(b+0x26f0,pic);patch(UI_DrawHandlePic,pic);patch(b+0x48f0,paint);patch(Text_Paint,paint);
for(i=0;i<5;++i){*(const char**)(b+0x2b3284+i*16)=types[i];uiInfo.gameTypes[i].gameType=types[i];}
for(i=0;i<5;++i)for(j=0;j<12;++j){float scale=.125f*(j+1);r.x=j*3.25f;r.y=j*-2.5f;r.w=125;r.h=31;*(int*)(b+0x2d926c)=ui_gameType.integer=i;trace[0]=0;((void(__cdecl*)(rectDef_t*,float,vec4_t,int))(b+0xec80))(&r,scale,c,j%6);strcpy(expected,trace);trace[0]=0;UI_DrawGameType(&r,scale,c,j%6);assert(!strcmp(trace,expected));++cases;}
for(n=0;n<=3;++n)for(k=0;k<3;++k)for(j=0;j<5;++j){team=queries[j];r.x=j*17.25f;r.y=-7;r.w=64;r.h=48;*(int*)(b+0x2b277c)=uiInfo.teamCount=n;
for(i=0;i<3;++i){unsigned char*row=b+0x2b2780+i*44;*(const char**)row=names[i];*(const char**)(row+4)=images[i];*(int*)(row+28)=initial[k];*(int*)(row+32)=810+i;*(int*)(row+36)=910+i;uiInfo.teamList[i].teamName=names[i];uiInfo.teamList[i].imageName=images[i];uiInfo.teamList[i].teamIcon=initial[k];uiInfo.teamList[i].teamIcon_Metal=810+i;uiInfo.teamList[i].teamIcon_Name=910+i;}
/* Second draw preserves first-call cache state and must not register again. */
for(pass=0;pass<2;++pass){shader=100;trace[0]=0;((void(__cdecl*)(rectDef_t*,float,vec4_t))(b+0xed00))(&r,.3f,c);strcpy(expected,trace);for(i=0;i<3;++i)memcpy(icons[i],b+0x2b2780+i*44+28,12);shader=100;trace[0]=0;UI_DrawClanLogo(&r,.3f,c);assert(!strcmp(trace,expected));for(i=0;i<3;++i){assert(icons[i][0]==uiInfo.teamList[i].teamIcon);assert(icons[i][1]==uiInfo.teamList[i].teamIcon_Metal);assert(icons[i][2]==uiInfo.teamList[i].teamIcon_Name);}++cases;}}
printf("%d original UI gametype/clan-logo comparisons passed\n",cases);FreeLibrary(h);return 0;}
