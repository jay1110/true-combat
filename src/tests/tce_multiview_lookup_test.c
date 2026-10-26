#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
int main(int argc,char**argv){HMODULE dll;byte*b;void*(__cdecl*locate)(int);int(__cdecl*merged)(int);int c,i,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;locate=(void*)(b+0x4cea0);merged=(void*)(b+0x4e110);
 for(c=0;c<24000;++c){int id=c%260-2,count=c%11,mcount=c%33,ri,si;void*r;cg_window_t*s;
  cg.winHandler.numActiveWindows=*(int*)(b+0x40863f4)=count;cg.mvTotalClients=*(int*)(b+0x4086398)=mcount;
  for(i=0;i<10;++i){int slot=(i+c)%10;cg.winHandler.activeWindows[i]=*(int*)(b+0x40863cc+4*i)=slot;cg.winHandler.window[i].effects=*(int*)(b+0x4086430+i*0x288)=((c+i)%3)?8:0;cg.winHandler.window[i].mvInfo=*(int*)(b+0x4086664+i*0x288)=0x123400|((i*31+c/10)%256);}
  for(i=0;i<32;++i)cg.mvOverlay[i].pID=*(int*)(b+0x4085e18+28*i)=(i*3+c/33)%256;
  r=locate(id);s=CG_mvClientLocate(id);ri=r?(int)(((byte*)r-(b+0x40863f8))/0x288):-1;si=s?(int)(s-cg.winHandler.window):-1;
  if(ri!=si||merged(id)!=CG_mvMergedClientLocate(id)){printf("multiview lookup case%d id%d\n",c,id);return 1;}n+=2;
 }
 printf("%d whole multiview lookup comparisons passed\n",n);return 0;
}

