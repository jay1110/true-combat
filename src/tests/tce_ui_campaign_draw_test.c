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
static int result;
static int play(const char*n,int x,int y,int w,int h,int bits){sprintf(trace+strlen(trace),"P%s,%d,%d,%d,%d,%d;",n,x,y,w,h,bits);return result;}
static e_status run(int h){sprintf(trace+strlen(trace),"R%d;",h);return FMV_EOF;}
static void ext(int h,int x,int y,int w,int hh){sprintf(trace+strlen(trace),"E%d,%d,%d,%d,%d;",h,x,y,w,hh);}
static void draw(int h){sprintf(trace+strlen(trace),"D%d;",h);}
static void set(const char*n,const char*v){sprintf(trace+strlen(trace),"S%s=%s;",n,v);}
static int regresult;
static int shaderreg(const char*n){sprintf(trace+strlen(trace),"I%s;",n);return regresult;}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int mode,net,idx,k,s,v,i,pass,cases=0;rectDef_t r={-3.75f,8.99f,120.75f,60.125f};vec4_t c={.1f,.2f,.3f,.4f};char expected[4096];int states[]={-3,-2,-1,0,17};int shots[]={-2,-1,0,23};int returns[]={-1,0,99};int saved[4][2];const char*names[]={"camp0","camp1","camp2","sentinel"};
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x20730,shaderreg);patch(trap_R_RegisterShaderNoMip,shaderreg);patch(b+0x26f0,pic);patch(UI_DrawHandlePic,pic);patch(b+0x20e60,play);patch(trap_CIN_PlayCinematic,play);patch(b+0x20eb0,run);patch(trap_CIN_RunCinematic,run);patch(b+0x20ef0,ext);patch(trap_CIN_SetExtents,ext);patch(b+0x20ed0,draw);patch(trap_CIN_DrawCinematic,draw);
for(mode=0;mode<2;++mode)for(net=0;net<2;++net)for(idx=-1;idx<=4;++idx)for(k=0;k<5;++k)for(s=0;s<4;++s)for(v=0;v<3;++v){result=regresult=returns[v];*(int*)(b+0x2b96ac)=uiInfo.campaignCount=3;*(int*)(b+0x2d902c)=ui_currentCampaign.integer=idx;*(int*)(b+0x28fc2c)=ui_currentNetCampaign.integer=idx;
for(i=0;i<4;++i){unsigned char*row=b+0x2b96b0+i*116;*(const char**)row=names[i];*(const char**)(row+84)=names[i];*(int*)(row+88)=states[k];*(int*)(row+92)=shots[s];uiInfo.campaignList[i].campaignShortName=names[i];uiInfo.campaignList[i].campaignShotName=names[i];uiInfo.campaignList[i].campaignCinematic=states[k];uiInfo.campaignList[i].campaignShot=shots[s];}
for(pass=0;pass<2;++pass){trace[0]=0;((void(__cdecl*)(rectDef_t*,float,vec4_t,int))(b+(mode?0xf1e0:0xf310)))(&r,.3f,c,net);strcpy(expected,trace);for(i=0;i<4;++i)memcpy(saved[i],b+0x2b96b0+i*116+88,8);trace[0]=0;if(mode)UI_DrawCampaignCinematic(&r,.3f,c,net);else UI_DrawCampaignPreview(&r,.3f,c,net);assert(!strcmp(trace,expected));assert(*(int*)(b+0x2d902c)==ui_currentCampaign.integer);assert(*(int*)(b+0x28fc2c)==ui_currentNetCampaign.integer);for(i=0;i<4;++i){assert(saved[i][0]==uiInfo.campaignList[i].campaignCinematic);assert(saved[i][1]==uiInfo.campaignList[i].campaignShot);}++cases;}}
printf("%d original UI campaign drawing comparisons passed\n",cases);FreeLibrary(h);return 0;}
