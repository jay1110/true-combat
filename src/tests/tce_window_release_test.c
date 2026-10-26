#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
typedef char window_layout[(sizeof(cg_window_t)==0x288)?1:-1];
static byte*pool;static int events[64],count,now;
static int clockStub(void){events[count++]=-1;return now;}
static void removeStub(cg_window_t*w){events[count++]=(int)(((byte*)w-pool)/0x288);events[count++]=w->id;events[count++]=w->inuse;events[count++]=w->state;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*release)(void*);void(__cdecl*cleanup)(void);int c,i,op,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;release=(void*)(b+0x7cc00);cleanup=(void*)(b+0x7ccc0);hook(b+0x64bc0,clockStub);hook(trap_Milliseconds,clockStub);hook(b+0x7d630,removeStub);hook(CG_removeStrings,removeStub);
 for(c=0;c<12000;++c)for(op=0;op<2;++op){int expected[64],ec,target=c%11;now=c*13-700;cg.winHandler.numActiveWindows=*(int*)(b+0x40863f4)=c%11;memset(cg.winHandler.window,0x55,sizeof(cg.winHandler.window));memcpy(b+0x40863f8,cg.winHandler.window,10*0x288);
  for(i=0;i<10;++i){cg_window_t*w=&cg.winHandler.window[i];w->inuse=(c>>i)&1;w->state=(c+i)%5;w->effects=(c+i)%33-1;cg.winHandler.activeWindows[i]=*(int*)(b+0x40863cc+4*i)=(i+c)%10;memcpy(b+0x40863f8+i*0x288,w,0x288);}
  pool=b+0x40863f8;count=0;if(op)cleanup();else release(target==10?NULL:pool+target*0x288);ec=count;memcpy(expected,events,count*sizeof(int));pool=(byte*)cg.winHandler.window;count=0;if(op)CG_windowCleanup();else CG_windowFree(target==10?NULL:&cg.winHandler.window[target]);
  if(ec!=count||memcmp(expected,events,count*sizeof(int))||cg.winHandler.numActiveWindows!=*(int*)(b+0x40863f4)||memcmp(cg.winHandler.activeWindows,b+0x40863cc,40)||memcmp(cg.winHandler.window,b+0x40863f8,10*0x288)){printf("window release%d op%d\n",c,op);return 1;}++n;
 }
 printf("%d whole window release/cleanup comparisons passed\n",n);return 0;
}
