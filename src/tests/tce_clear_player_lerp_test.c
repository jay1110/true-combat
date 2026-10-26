#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_players.c"
#include <windows.h>
#include <stdio.h>
static animation_t anim;
static byte rawAnim[112];
static void *gotCent,*gotCi;
static int mode,calls,gotAnimation,observedOld,observedTime;
static void originalSet(void*cent,void*ci,byte*lf,int number){++calls;gotCent=cent;gotCi=ci;gotAnimation=number;observedOld=*(int*)(lf+4);observedTime=*(int*)(lf+16);*(void**)(lf+52)=mode?rawAnim:NULL;}
static void sourceSet(centity_t*cent,clientInfo_t*ci,lerpFrame_t*lf,int number){++calls;gotCent=cent;gotCi=ci;gotAnimation=number;observedOld=lf->oldFrameTime;observedTime=lf->frameTime;lf->animation=mode?&anim:NULL;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;static byte lf[256];lerpFrame_t sl;int c,op,n=0;void(__cdecl*original[2])(void*,void*,void*,int);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original[0]=(void*)(b+0x56380);original[1]=(void*)(b+0x53e70);
 hook(b+0x536c0,originalSet);hook(b+0x53760,originalSet);hook(CG_SetLerpFrameAnimation,sourceSet);hook(CG_SetLerpFrameAnimationRate,sourceSet);
 for(c=0;c<12000;++c)for(op=0;op<2;++op){void *centArg=(void*)(0x10000+c*16),*ciArg=(void*)(0x80000+c*32),*expectedCent,*expectedCi;int expectedCalls,expectedNumber,expectedOld,expectedTime;mode=c%2;memset(lf,0,sizeof(lf));memset(&sl,0,sizeof(sl));cg.time=*(int*)(b+0x407dea4)=c-6000;
  *(int*)rawAnim=anim.mdxFile=c%431;*(int*)(rawAnim+68)=anim.firstFrame=c-999;
  *(int*)lf=sl.oldFrame=51;*(int*)(lf+12)=sl.frame=52;*(int*)(lf+8)=sl.oldFrameModel=53;*(int*)(lf+20)=sl.frameModel=54;
  calls=0;original[op](centArg,ciArg,lf,c);expectedCent=gotCent;expectedCi=gotCi;expectedCalls=calls;expectedNumber=gotAnimation;expectedOld=observedOld;expectedTime=observedTime;
  calls=0;if(op)CG_ClearLerpFrameRate(centArg,ciArg,&sl,c);else CG_ClearLerpFrame(centArg,ciArg,&sl,c);
  if(gotCent!=expectedCent||gotCi!=expectedCi||gotCent!=centArg||gotCi!=ciArg||calls!=expectedCalls||gotAnimation!=expectedNumber||observedOld!=expectedOld||observedTime!=expectedTime||*(int*)lf!=sl.oldFrame||*(int*)(lf+4)!=sl.oldFrameTime||*(int*)(lf+8)!=sl.oldFrameModel||*(int*)(lf+12)!=sl.frame||*(int*)(lf+16)!=sl.frameTime||*(int*)(lf+20)!=sl.frameModel||((*(void**)(lf+52)!=NULL)!=(sl.animation!=NULL))){printf("clear lerp case%d op%d\n",c,op);return 1;}++n;
 }
 printf("%d whole player clear-lerp comparisons passed\n",n);return 0;
}

