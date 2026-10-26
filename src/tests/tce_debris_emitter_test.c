#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_AddDebris(vec3_t,vec3_t,int,int,int);
static byte raw[32][448];static localEntity_t source[32],expected;static int mode,count,randomCalls;static unsigned rng;
int rand(void){rng=rng*1664525u+1013904223u;++randomCalls;return(rng>>16)&32767;}
static localEntity_t *allocate(void){int i=count++;if(i>=32)exit(9);return mode?(void*)raw[i]:source+i;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;int k,j,i,n,rc;unsigned r;void(__cdecl*original)(float*,float*,int,int,int);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x77eb0);hook(b+0x44900,allocate);hook(CG_AllocLocalEntity,allocate);hook(b+0x885b6,rand);
 for(k=0;k<30000;++k){vec3_t p={k%73-36.f,k%107-53.f,k%149-74.f},d={(k%9-4)*.25f,(k%11-5)*.25f,(k%13-6)*.125f};int speed=k%1401-100,duration=k%8191+1,c=k%32-2;
  memset(raw,0,sizeof(raw));memset(source,0,sizeof(source));cg.time=*(int*)(b+0x407dea4)=10000+k*31;
  count=randomCalls=0;rng=k;mode=1;original(p,d,speed,duration,c);n=count;rc=randomCalls;r=rng;
  count=randomCalls=0;rng=k;mode=0;CG_AddDebris(p,d,speed,duration,c);if(count!=n||randomCalls!=rc||rng!=r){printf("debris case%d count%d/%d RNG%d/%d\n",k,count,n,randomCalls,rc);return 2;}
  for(i=0;i<n;++i){memset(&expected,0,sizeof(expected));memcpy(&expected,raw[i],0x6c);memcpy((byte*)&expected+0x6c,raw[i]+0x70,offsetof(localEntity_t,tceFragment)-0x6c);expected.tceGravity=*(float*)(raw[i]+0x6c);
   for(j=0;j<3;++j){float actual=source[i].pos.trBase[j],want=expected.pos.trBase[j];if(fabs(actual-want)>.00005+fabs(want)*.000002){printf("debris%d elem%d position%d %.9g/%.9g\n",k,i,j,actual,want);return 3;}source[i].pos.trBase[j]=want;}
   if(memcmp(&source[i],&expected,sizeof(expected))){for(j=0;j<sizeof(expected);++j)if(((byte*)&source[i])[j]!=((byte*)&expected)[j]){printf("debris case%d elem%d byte%x %x/%x\n",k,i,j,((byte*)&source[i])[j],((byte*)&expected)[j]);break;}return 1;}
  }
 }
 puts("30000 complete debris-emitter original comparisons passed");return 0;
}
