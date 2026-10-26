#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_CalcVrect(void);
extern vmCvar_t cg_aspectMode;
extern void CG_Letterbox(float,float,qboolean);
static float got[3];static int count;
static void box(float x,float y,int c){got[0]=x;got[1]=y;got[2]=(float)c;++count;}
static void adjust(float*x,float*y,float*w,float*h){*x=3.75f;*y=-4.5f;*w=799.25f;*h=601.75f;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;int w,h,mode,show,n=0;void(__cdecl*original)(void);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x6a110);
 hook(b+0x67370,box);hook(CG_Letterbox,box);hook(b+0x29760,adjust);hook(CG_AdjustFrom640,adjust);
 for(w=320;w<=3840;w+=137)for(h=240;h<=2160;h+=113)for(mode=-1;mode<=3;++mode)for(show=0;show<2;++show){float expected[3];int expectedRect[4],expectedCount;
  cgs.glconfig.vidWidth=*(int*)(b+0x257e0c8)=w;cgs.glconfig.vidHeight=*(int*)(b+0x257e0cc)=h;
  cg_aspectMode.integer=*(int*)(b+0x401496c)=mode;cg.showGameView=*(int*)(b+0x4081fa8)=show;
  cg.refdef.x=cg.refdef.y=cg.refdef.width=cg.refdef.height=0;memset(b+0x407f004,0,16);
  count=0;original();memcpy(expected,got,sizeof(got));memcpy(expectedRect,b+0x407f004,16);expectedCount=count;
  count=0;CG_CalcVrect();if(count!=expectedCount||got[0]!=expected[0]||got[2]!=expected[2]||fabs(got[1]-expected[1])>0.00002||cg.refdef.x!=expectedRect[0]||cg.refdef.y!=expectedRect[1]||cg.refdef.width!=expectedRect[2]||cg.refdef.height!=expectedRect[3]){printf("vrect %d %d mode%d show%d height%.9g/%.9g\n",w,h,mode,show,got[1],expected[1]);return 1;}++n;
 }
 printf("%d whole CalcVrect original controller comparisons passed\n",n);return 0;
}
