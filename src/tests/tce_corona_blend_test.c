#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_draw.c"
#include <windows.h>
#include <stdio.h>
static float result[8];static int calls;
static void fill(float x,float y,float w,float h,const float*c){calls++;result[0]=x;result[1]=y;result[2]=w;result[3]=h;memcpy(result+4,c,16);}
static void hook(void*at,void*fn){DWORD o,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*original)(void);int i,j,n;float expected[8];snapshot_t snap;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x27540);hook(b+0x29850,fill);hook(CG_FillRect,fill);
 for(i=0;i<12000;i++){cg.snap=i%3?&snap:NULL;*(void**)(b+0x40352c4)=cg.snap;cg.renderingThirdPerson=*(int*)(b+0x407deb8)=i%4==0;cg.tceCoronaBlendAlpha=*(float*)(b+0x40a49e4)=(i%301-100)/100.f;for(j=0;j<3;j++)cg.tceCoronaBlendColor[j]=((float*)(b+0x40a49e8))[j]=(i%17+j)/7.f;calls=0;memset(result,0x55,sizeof(result));original();n=calls;memcpy(expected,result,sizeof(result));calls=0;memset(result,0x55,sizeof(result));CG_DrawCoronaBlend();if(calls!=n||memcmp(expected,result,sizeof(result))){printf("corona blend%d\n",i);return 1;}}
 printf("12000 whole corona blend comparisons passed\n");return 0;
}
