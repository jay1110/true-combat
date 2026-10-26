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
static void stretch(float x,float y,float w,float h,float s0,float t0,float s1,float t1,int sh){sprintf(trace+strlen(trace),"D%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%d;",x,y,w,h,s0,t0,s1,t1,sh);}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int mode,res,i,j,k,cases=0;int dims[][2]={{640,480},{800,600},{1280,720},{1920,1080},{1280,1024}};float values[]={-96,-1,-0.0f,0,1,77.25f,96};vec4_t c={.1f,.2f,.3f,.4f};char expected[4096];assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;patch(b+0x20810,colorfn);patch(trap_R_SetColor,colorfn);patch(b+0x20850,stretch);patch(trap_R_DrawStretchPic,stretch);*(int*)(b+0x2b2048)=uiInfo.uiDC.whiteShader=231;
for(res=0;res<5;++res){*(int*)(b+0x2b2028)=uiInfo.uiDC.glconfig.vidWidth=dims[res][0];*(int*)(b+0x2b202c)=uiInfo.uiDC.glconfig.vidHeight=dims[res][1];*(float*)(b+0x292138)=uiInfo.uiDC.xscale=dims[res][0]/640.f;*(float*)(b+0x292134)=uiInfo.uiDC.yscale=dims[res][1]/480.f;
for(mode=0;mode<2;++mode)for(i=0;i<7;++i)for(j=0;j<7;++j)for(k=0;k<7;++k){float x=i*3.125f,y=j*-2.25f;trace[0]=0;((void(__cdecl*)(float,float,float,float,float))(b+(mode?0x4100:0x4070)))(x,y,values[i],values[j],values[k]);strcpy(expected,trace);trace[0]=0;if(mode)_UI_DrawTopBottom(x,y,values[i],values[j],values[k]);else _UI_DrawSides(x,y,values[i],values[j],values[k]);if(strcmp(expected,trace)){fprintf(stderr,"primitive mismatch res%d mode%d i%d j%d\n%s\n%s\n",res,mode,i,j,expected,trace);return 1;}++cases;}}
printf("%d original UI border primitive comparisons passed\n",cases);FreeLibrary(h);return 0;}
