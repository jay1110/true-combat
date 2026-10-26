#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
static int events[16],count;
static void create(int id){events[count++]=1000+id;}
static void release(int id){events[count++]=2000+id;}
static void update(void){events[count++]=3000;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;static byte windows[2][0x288];static cg_window_t sw[2];void(__cdecl*fn[4])(void);int c,i,op,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;fn[0]=(void*)(b+0x4cc70);fn[1]=(void*)(b+0x4cca0);fn[2]=(void*)(b+0x4ccf0);fn[3]=(void*)(b+0x4cef0);
 hook(b+0x4cd40,create);hook(CG_mvCreate,create);hook(b+0x4ce60,release);hook(CG_mvFree,release);hook(b+0x4e0d0,update);hook(CG_mvOverlayUpdate,update);
 for(c=0;c<8000;++c)for(op=0;op<4;++op){int expected[16],ec,e0,e1;cg.mvTotalClients=*(int*)(b+0x4086398)=c%33;
  for(i=0;i<32;++i){cg.mvOverlay[i].pID=*(int*)(b+0x4085e18+28*i)=i+21;cg.mvOverlay[i].fActive=*(int*)(b+0x4085e2c+28*i)=(i==c%34||i==(c/34)%34);cg.mvOverlay[i].w=(c+i)%2?&sw[1]:NULL;*(void**)(b+0x4085e30+28*i)=(c+i)%2?windows[1]:NULL;}
  cg.mvCurrentMainview=&sw[0];*(void**)(b+0x4085e14)=windows[0];cg.mvCurrentActive=c%3==0?NULL:&sw[c%2];*(void**)(b+0x4085e10)=c%3==0?NULL:windows[c%2];
  sw[0].mvInfo=*(int*)(windows[0]+0x26c)=0x123400|(c%256);sw[1].mvInfo=*(int*)(windows[1]+0x26c)=0x567800|((c+81)%256);
  count=0;if(op==3)((void(__cdecl*)(void*))fn[3])(windows[c%2]);else fn[op]();ec=count;memcpy(expected,events,count*sizeof(int));e0=*(int*)(windows[0]+0x26c);e1=*(int*)(windows[1]+0x26c);
  count=0;if(op==0)CG_mvHideView_f();else if(op==1)CG_mvShowView_f();else if(op==2)CG_mvToggleView_f();else CG_mvMainviewSwap(&sw[c%2]);
  if(ec!=count||memcmp(expected,events,count*sizeof(int))||e0!=sw[0].mvInfo||e1!=sw[1].mvInfo){printf("mv controls case%d operation%d\n",c,op);return 1;}++n;
 }
 printf("%d whole multiview control comparisons passed\n",n);return 0;
}
