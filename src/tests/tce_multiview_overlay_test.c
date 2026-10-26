#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
static int compare(byte*b){int i;for(i=0;i<32;++i){byte*r=b+0x4085e18+28*i;cg_mvinfo_t*s=&cg.mvOverlay[i];void*w=*(void**)(r+24);int ri=w?(int)(((byte*)w-(b+0x40863f8))/0x288):-1;int si=s->w?(int)(s->w-cg.winHandler.window):-1;if(*(int*)r!=s->pID||*(int*)(r+4)!=s->classID||*(int*)(r+8)!=s->width||memcmp(r+12,s->info,8)||*(int*)(r+20)!=s->fActive||ri!=si)return 0;}return 1;}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*single)(int,int);void(__cdecl*all)(void);int c,i,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;single=(void*)(b+0x4dfe0);all=(void*)(b+0x4e0d0);
 for(c=0;c<12000;++c){unsigned mask=0x9e3779b9u*(unsigned)c;cg.mvClientList=*(unsigned*)(b+0x4085e0c)=mask;cg.mvTotalClients=*(int*)(b+0x4086398)=c%33;cg.winHandler.numActiveWindows=*(int*)(b+0x40863f4)=c%11;
  for(i=0;i<10;++i){cg.winHandler.activeWindows[i]=*(int*)(b+0x40863cc+4*i)=(i+c)%10;cg.winHandler.window[i].effects=*(int*)(b+0x4086430+0x288*i)=(c+i)%3?8:0;cg.winHandler.window[i].mvInfo=*(int*)(b+0x4086664+0x288*i)=(c+i)%32;}
  cg.mvCurrentMainview=&cg.winHandler.window[c%10];*(void**)(b+0x4085e14)=b+0x40863f8+(c%10)*0x288;
  for(i=0;i<32;++i){cg_entities[i].currentState.teamNum=*(int*)(b+0x488d908+0xa94*i)=(c+i)%5;memset(b+0x4085e18+28*i,0,28);memset(&cg.mvOverlay[i],0,sizeof(cg.mvOverlay[i]));}
  single(c%32,(c/32)%32);CG_mvOverlayClientUpdate(c%32,(c/32)%32);if(!compare(b)){printf("overlay single%d\n",c);return 1;}++n;
  all();CG_mvOverlayUpdate();if(!compare(b)){printf("overlay all%d\n",c);return 1;}++n;
 }
 printf("%d whole multiview overlay comparisons passed\n",n);return 0;
}
