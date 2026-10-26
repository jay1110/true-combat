#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_StartShakeCamera(float);
static int value,calls;
static int rng(void){++calls;return value;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;int k,total=0;void(__cdecl*fade)(int,int,int,int,int,int);void(__cdecl*shake)(float);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;fade=(void*)(b+0x21560);shake=(void*)(b+0x21c70);
 hook(b+0x885b6,rng);hook(rand,rng);
 for(k=0;k<40000;++k){int time=k%16001-8000,duration=k%2001-1000,alpha=k%1025-512;float p=(k%801-400)*.005f;
  cg.time=*(int*)(b+0x407dea4)=k%10001-5000;cgs.fadeAlphaCurrent=*(float*)(b+0x2587ff8)=.375f;
  fade(k,-k,k*2,alpha,time,duration);CG_Fade(k,-k,k*2,alpha,time,duration);
  if(cgs.fadeStartTime!=*(int*)(b+0x2587ffc)||cgs.fadeDuration!=*(int*)(b+0x2588000)||cgs.fadeAlpha!=*(float*)(b+0x2587ff4)||cgs.fadeAlphaCurrent!=*(float*)(b+0x2587ff8)){printf("fade case%d\n",k);return 1;}
  ++total;value=(k*17331)&32767;calls=0;shake(p);if(calls!=1)return 2;calls=0;CG_StartShakeCamera(p);
  if(calls!=1||cg.cameraShakeTime!=*(int*)(b+0x4081560)||cg.cameraShakeLength!=*(float*)(b+0x408156c)||cg.cameraShakeScale!=*(float*)(b+0x4081568)||cg.cameraShakePhase!=*(float*)(b+0x4081564)){printf("shake case%d time%d/%d length%.9g/%.9g phase%.9g/%.9g\n",k,cg.cameraShakeTime,*(int*)(b+0x4081560),cg.cameraShakeLength,*(float*)(b+0x408156c),cg.cameraShakePhase,*(float*)(b+0x4081564));return 1;}
  ++total;
 }
 printf("%d complete fade/shake-state original comparisons passed\n",total);return 0;
}
