#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_windowReset(cg_window_t*,int,int);
typedef char window_layout[(sizeof(cg_window_t)==0x288)?1:-1];
static int clockValue,clockCalls;
static int clockStub(void){++clockCalls;return clockValue;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*reset)(void*,int,int);void*(__cdecl*alloc)(int,int);int c,i,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;reset=(void*)(b+0x7caa0);alloc=(void*)(b+0x7cba0);hook(b+0x64bc0,clockStub);hook(trap_Milliseconds,clockStub);
 for(c=0;c<16000;++c){cg_window_t r,s;int calls,fx=c%257-16,length=c%1001-250;void*rp;cg_window_t*sp;clockValue=c*137-1000;memset(&r,0x55,sizeof(r));s=r;clockCalls=0;reset(&r,fx,length);calls=clockCalls;clockCalls=0;CG_windowReset(&s,fx,length);if(calls!=clockCalls||memcmp(&r,&s,sizeof(r))){printf("window reset%d\n",c);return 1;}++n;
  memset(b+0x40863f8,0x55,10*0x288);memset(cg.winHandler.window,0x55,sizeof(cg.winHandler.window));cg.winHandler.numActiveWindows=*(int*)(b+0x40863f4)=c%11;
  for(i=0;i<10;++i){cg.winHandler.window[i].inuse=*(int*)(b+0x40863f8+i*0x288+0x5c)=(c>>i)&1;cg.winHandler.activeWindows[i]=*(int*)(b+0x40863cc+i*4)=-7;}
  clockCalls=0;rp=alloc(fx,length);calls=clockCalls;clockCalls=0;sp=CG_windowAlloc(fx,length);if(calls!=clockCalls||cg.winHandler.numActiveWindows!=*(int*)(b+0x40863f4)||memcmp(cg.winHandler.window,b+0x40863f8,10*0x288)||memcmp(cg.winHandler.activeWindows,b+0x40863cc,40)||(rp?((byte*)rp-(b+0x40863f8))/0x288:-1)!=(sp?sp-cg.winHandler.window:-1)){printf("window alloc%d\n",c);return 1;}++n;
 }
 printf("%d whole window allocation comparisons passed\n",n);return 0;
}
