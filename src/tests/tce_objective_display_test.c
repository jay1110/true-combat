#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_draw.c"
#include <windows.h>
#include <stdio.h>
typedef struct{float x,y,sx,sy,color[4],adjust;int limit,style;char text[1024];} paint_t;
static paint_t calls[8];static int count,bindingMode,locationMode,queries;static char bindingText[40];
static const char*config(int n){queries=n;return locationMode?"Bombsite A":NULL;}
static const char*binding(const char*s){if(strcmp(s,"toggleobjectivedesc"))exit(3);return bindingMode?"L":"(toggleobjectivedesc)";}
static int width(const char*s,float scale,int limit,fontInfo_t*f){return (int)strlen(s)*3;}
static void paint(float x,float y,float sx,float sy,vec4_t color,const char*s,float adjust,int limit,int style,fontInfo_t*f){paint_t*p=&calls[count++];memset(p,0,sizeof(*p));p->x=x;p->y=y;p->sx=sx;p->sy=sy;memcpy(p->color,color,16);p->adjust=adjust;p->limit=limit;p->style=style;strcpy(p->text,s);}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b,raw[0x600];snapshot_t snap;void(__cdecl*original)(void);int c,n=0;paint_t expected[8];int gt[]={2,5,7};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x217d0);hook(b+0x481a0,config);hook(CG_ConfigString,config);hook(b+0x84090,binding);hook(BindingFromName,binding);hook(b+0x20910,width);hook(CG_Text_Width_Ext,width);hook(b+0x20ba0,paint);hook(CG_Text_Paint_Ext,paint);
 for(c=0;c<3600;c++){int ec,eq;memset(raw,0,sizeof(raw));memset(&snap,0,sizeof(snap));cg.snap=&snap;*(void**)(b+0x40352c4)=raw;cg.snap->ps.persistant[PERS_TEAM]=*(int*)(raw+0x148)=1+c%3;cg.snap->ps.pm_flags=*(int*)(raw+0x38)=(c/3)%2?PMF_LIMBO:0;
  cg.mvTotalClients=*(int*)(b+0x4086398)=(c/6)%2;cg_aspectMode.integer=*(int*)(b+0x401496c)=(c/12)%3;cgs.gametype=*(int*)(b+0x257e100)=gt[(c/36)%3];cg.tceShowObjectiveDesc=*(int*)(b+0x40a49e0)=(c/108)%2;cg.snap->ps.holdable[13]=*(int*)(raw+0x3f0)=(c/216)%2?4:0;
  bindingMode=(c/432)%2;locationMode=(c/864)%2;strcpy(cg.objDescription_Axis[0],c%5?"Attack A":"");strcpy(cg.objDescription_Allied[0],c%7?"Defend B":"");strcpy((char*)b+0x40a2d30,cg.objDescription_Axis[0]);strcpy((char*)b+0x40a3530,cg.objDescription_Allied[0]);count=0;queries=-1;original();ec=count;eq=queries;memcpy(expected,calls,sizeof(calls));count=0;queries=-1;CG_DrawObjectiveDesc();
  if(count!=ec||queries!=eq||memcmp(calls,expected,count*sizeof(paint_t))){printf("objective display case%d calls%d/%d query%d/%d\n",c,ec,count,eq,queries);return 1;}n++;
 }printf("%d whole GT objective display comparisons passed\n",n);return 0;
}
