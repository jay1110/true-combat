#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static char trace[2048];static int clipped;
static int width(const char*s,float scale,int limit){sprintf(trace+strlen(trace),"W:%s,%.9g,%d;",s,scale,limit);return *s?7:0;}
static void paint(float*max,float x,float y,float scale,vec4_t color,const char*s,float adjust,int limit){sprintf(trace+strlen(trace),"P:%.9g,%.9g,%.9g,%.9g,%s,%.9g,%d;",*max,x,y,scale,s,adjust,limit);*max=clipped?0:17.37f;}
static struct{unsigned char*p,bytes[5];}hooks[4];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<4);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}


int main(int argc,char**argv){HMODULE h;unsigned char*b;int len,w,x,x2,off,t,c,r,cases=0,i;char expected[2048];int state[6],actual[6];int px[]={-1,3,5,90},px2[]={-1,0,37};vec4_t color={.13f,.37f,.71f,.9f};rectDef_t rect;
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;patch(b+0x4270,width);patch(Text_Width,width);patch(b+0x10af0,paint);patch(Text_Paint_Limit,paint);
for(len=0;len<2;++len)for(w=0;w<2;++w)for(x=0;x<4;++x)for(x2=0;x2<3;++x2)for(off=0;off<4;++off)for(t=0;t<3;++t)for(c=0;c<2;++c)for(r=0;r<3;++r){rect.x=r==0?3.13f:r==1?-2.71f:11.37f;rect.y=9.73f;rect.w=r==0?81.19f:r==1?27.33f:101.71f;rect.h=22;clipped=c;
strcpy(uiInfo.serverStatus.motd,"TC:E");strcpy((char*)(b+0x2d1330),"TC:E");uiInfo.serverStatus.motdLen=*(int*)(b+0x2d1318)=len?4:0;uiInfo.serverStatus.motdWidth=*(int*)(b+0x2d131c)=w?-1:0;uiInfo.serverStatus.motdPaintX=*(int*)(b+0x2d1320)=px[x];uiInfo.serverStatus.motdPaintX2=*(int*)(b+0x2d1324)=px2[x2];uiInfo.serverStatus.motdOffset=*(int*)(b+0x2d1328)=off==3?5:off==2?4:off;uiInfo.serverStatus.motdTime=*(int*)(b+0x2d132c)=100;uiInfo.uiDC.realTime=*(int*)(b+0x292140)=99+t;
trace[0]=0;((void(__cdecl*)(rectDef_t*,float,float*))(b+0x108e0))(&rect,.23f,color);strcpy(expected,trace);memcpy(state,b+0x2d1318,sizeof(state));trace[0]=0;UI_DrawServerMOTD(&rect,.23f,color);actual[0]=uiInfo.serverStatus.motdLen;actual[1]=uiInfo.serverStatus.motdWidth;actual[2]=uiInfo.serverStatus.motdPaintX;actual[3]=uiInfo.serverStatus.motdPaintX2;actual[4]=uiInfo.serverStatus.motdOffset;actual[5]=uiInfo.serverStatus.motdTime;
if(strcmp(trace,expected)||memcmp(state,actual,sizeof(state))){fprintf(stderr,"motd mismatch len%d w%d x%d x2%d off%d time%d clip%d rect%d\nO%s\nS%s\n",len,w,x,x2,off,t,c,r,expected,trace);for(i=0;i<6;++i)fprintf(stderr,"state%d %d/%d\n",i,state[i],actual[i]);return 1;}++cases;}
restore();assert(FreeLibrary(h));printf("%d original MOTD-controller comparisons passed\n",cases);return 0;}
