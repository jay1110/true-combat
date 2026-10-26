#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_DrawTracer(vec3_t,vec3_t);
extern void CG_Tracer(vec3_t,vec3_t,int);
static polyVert_t captured[4],expected[4];
static int count,shader,randomValue,randomCalls;
static int rng(void){++randomCalls;return randomValue;}
static void capture(int s,int n,const polyVert_t*v){if(n!=4)abort();++count;shader=s;memcpy(captured,v,sizeof(captured));}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;refdef_t ref;int k,mode,j,a,total=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;
 hook(b+0x652b0,capture);hook(trap_R_AddPolyToScene,capture);hook(b+0x885b6,rng);hook(rand,rng);
 memset(&ref,0,sizeof(ref));cg.refdef_current=&ref;*(void**)(b+0x40863ac)=&ref;
 for(mode=0;mode<2;++mode)for(k=0;k<12000;++k){vec3_t start={k%31-15.f,k%47-23.f,k%23-11.f},end,angles={k%90-45.f,k%360,0};int wantCount,wantShader,wantRandom;
  AnglesToAxis(angles,ref.viewaxis);
  end[0]=start[0]+(k%201-100);end[1]=start[1]+(k%401-200);end[2]=start[2]+(k%301-150);
  if(k%12==0)VectorCopy(start,end);
  cg_tracerWidth.value=*(float*)(b+0x4847548)=(k%17-3)*.25f;
  cg_tracerLength.value=*(float*)(b+0x4011488)=(k%401-10)*.5f;
  cgs.media.tracerShader=*(int*)(b+0x2588114)=k%123;
  randomValue=(k*17331)&32767;count=randomCalls=0;
  if(mode)((void(__cdecl*)(float*,float*,int))(b+0x7b5e0))(start,end,k&1);
  else ((void(__cdecl*)(float*,float*))(b+0x7b3c0))(start,end);
  wantCount=count;wantRandom=randomCalls;wantShader=shader;memcpy(expected,captured,sizeof(expected));count=randomCalls=0;
  if(mode)CG_Tracer(start,end,k&1);else CG_DrawTracer(start,end);
  if(count!=wantCount||randomCalls!=wantRandom||(count&&shader!=wantShader)){printf("tracer calls mode%d case%d\n",mode,k);return 1;}
  if(count)for(j=0;j<4;++j){if(memcmp(captured[j].st,expected[j].st,sizeof(expected[j].st))||memcmp(captured[j].modulate,expected[j].modulate,4))return 3;
   for(a=0;a<3;++a)if(fabs(captured[j].xyz[a]-expected[j].xyz[a])>0.0002f){printf("tracer mode%d case%d vert%d axis%d actual%.9g expected%.9g\n",mode,k,j,a,captured[j].xyz[a],expected[j].xyz[a]);return 4;}}
  ++total;
 }
 printf("%d whole tracer/controller original comparisons passed\n",total);return 0;
}
