#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_WaterRipple(qhandle_t,vec3_t,vec3_t,int,int);
static byte raw[448];static localEntity_t source,expected;static int mode,count;
static localEntity_t *allocate(void){++count;return mode?(void*)raw:&source;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;int k,j;void(__cdecl*original)(int,float*,float*,int,int);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x781a0);hook(b+0x44900,allocate);hook(CG_AllocLocalEntity,allocate);
 for(k=0;k<40000;++k){vec3_t p={k%73-36.f,k%107-53.f,k%149-74.f},d={.3f,.7f,-.2f};int size=k%512-17,lifetime=k%8191+1,shader=k%377;
  memset(raw,0,sizeof(raw));memset(&source,0,sizeof(source));memset(&expected,0,sizeof(expected));cg.time=*(int*)(b+0x407dea4)=10000+k*531;
  count=0;mode=1;original(shader,p,d,size,lifetime);if(count!=1)return 3;
  memcpy(&expected,raw,0x6c);memcpy((byte*)&expected+0x6c,raw+0x70,offsetof(localEntity_t,tceFragment)-0x6c);expected.tceGravity=*(float*)(raw+0x6c);
  count=0;mode=0;CG_WaterRipple(shader,p,d,size,lifetime);if(count!=1)return 4;
  if(memcmp(&source,&expected,sizeof(source))){for(j=0;j<sizeof(source);++j)if(((byte*)&source)[j]!=((byte*)&expected)[j]){printf("ripple case%d byte%x %x/%x\n",k,j,((byte*)&source)[j],((byte*)&expected)[j]);break;}return 1;}
 }
 puts("40000 complete water-ripple original comparisons passed");return 0;
}
