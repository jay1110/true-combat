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
int main(int argc,char**argv){HMODULE h;unsigned char*b;int mode,n,k,j,v,pass,i,cases=0;rectDef_t r={-3.75f,8.99f,120.75f,60.125f};vec4_t c={.1f,.2f,.3f,.4f};char expected[4096];int states[]={-9,-3,-2,-1,0,17};int results[]={-4,-2,-1,0,23};int saved[3];const char*names[]={"Alpha","Beta","Gamma"};const char*queries[]={"","Alpha","bETA","Gamma","unknown"};
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;
patch(b+0x20570,cvarstring);patch(trap_Cvar_VariableStringBuffer,cvarstring);patch(b+0x20810,colorfn);patch(trap_R_SetColor,colorfn);patch(b+0x26f0,pic);patch(UI_DrawHandlePic,pic);
patch(b+0x20e60,play);patch(trap_CIN_PlayCinematic,play);patch(b+0x20eb0,run);patch(trap_CIN_RunCinematic,run);patch(b+0x20ef0,ext);patch(trap_CIN_SetExtents,ext);patch(b+0x20ed0,draw);patch(trap_CIN_DrawCinematic,draw);
for(mode=0;mode<2;++mode)for(n=0;n<=3;++n)for(k=0;k<6;++k)for(j=0;j<5;++j)for(v=0;v<5;++v){team=queries[j];result=results[v];*(int*)(b+0x2b277c)=uiInfo.teamCount=n;*(int*)(b+0x2ceae4)=uiInfo.previewMovie=states[k];*(int*)(b+0x2ceae0)=uiInfo.movieIndex=j%3;
for(i=0;i<3;++i){unsigned char*row=b+0x2b2780+i*44;*(const char**)row=names[i];*(const char**)(row+4)=names[i];*(int*)(row+28)=80+i;*(int*)(row+40)=states[k];uiInfo.teamList[i].teamName=names[i];uiInfo.teamList[i].imageName=names[i];uiInfo.teamList[i].teamIcon=80+i;uiInfo.teamList[i].cinematic=states[k];*(const char**)(b+0x2ce6dc+i*4)=names[i];uiInfo.movieList[i]=names[i];}
for(pass=0;pass<2;++pass){int movie;trace[0]=0;((void(__cdecl*)(rectDef_t*,float,vec4_t))(b+(mode?0xeee0:0xedd0)))(&r,.3f,c);strcpy(expected,trace);movie=*(int*)(b+0x2ceae4);for(i=0;i<3;++i)saved[i]=*(int*)(b+0x2b2780+i*44+40);trace[0]=0;if(mode)UI_DrawPreviewCinematic(&r,.3f,c);else UI_DrawClanCinematic(&r,.3f,c);assert(!strcmp(trace,expected));assert(movie==uiInfo.previewMovie);for(i=0;i<3;++i)assert(saved[i]==uiInfo.teamList[i].cinematic);++cases;}}
printf("%d original UI cinematic comparisons passed\n",cases);FreeLibrary(h);return 0;}
