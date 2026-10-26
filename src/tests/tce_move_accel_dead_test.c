#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/bg_pmove.c"
#include <windows.h>
#include <stdio.h>
static int same(float *a,float *b){int i;for(i=0;i<3;++i)if(!_finite(a[i])||!_finite(b[i])||fabs(a[i]-b[i])>0.0001+fabs(b[i])*0.000003)return 0;return 1;}
int main(int argc,char **argv){HMODULE dll;byte*b;static byte rawPs[4096];static void*rawPm[2];static pmove_t movement;static playerState_t ps;int k,s,a,w,n=0;float speed[]={0,1,20,100,320,1000},acc[]={0,1,10};void(__cdecl*accelerate)(float*,float,float);void(__cdecl*dead)(void);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;accelerate=(void*)(b+0xbb80);dead=(void*)(b+0xe210);rawPm[0]=rawPs;*(void**)(b+0x4ba9edc)=rawPm;movement.ps=&ps;pm=&movement;
 for(k=0;k<6000;++k){vec3_t initial,dir;VectorSet(initial,(k%101-50)*.5f,(k%37-18)*.25f,(k%73-36)*.125f);if(k<4)VectorSet(initial,19.999f+k*.001f,0,0);VectorSet(dir,k%3-1,k%5-2,k%7-3);VectorNormalize(dir);
  for(s=0;s<6;++s)for(a=0;a<3;++a){VectorCopy(initial,ps.velocity);memcpy(rawPs+32,initial,12);pml.frametime=*(float*)(b+0x4ba9e64)=(k%201)*.001f;accelerate(dir,speed[s],acc[a]);PM_Accelerate(dir,speed[s],acc[a]);if(!same(ps.velocity,(float*)(rawPs+32))){printf("accel mismatch%d speed%d acc%d\n",k,s,a);return 1;}++n;}
  for(w=0;w<2;++w){VectorCopy(initial,ps.velocity);memcpy(rawPs+32,initial,12);pml.walking=*(int*)(b+0x4ba9e6c)=w;dead();PM_DeadMove();if(!same(ps.velocity,(float*)(rawPs+32))){printf("dead mismatch%d walking%d\n",k,w);return 1;}++n;}
 }
 printf("%d whole acceleration/dead-movement comparisons passed\n",n);FreeLibrary(dll);return 0;
}
