#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
typedef struct{int *p;unsigned off;} field_t;
#define F(n,o) {&cg.n,o}
static field_t fields[]={F(time,0x407dea4),F(scoresRequestTime,0x407f5d4),F(demoPlayback,0x40352b0),F(mvTotalClients,0x4086398),F(showGameView,0x4081fa8),F(tceShowLimboPanel,0x40a49dc),F(showScores,0x40804f8),F(numScores,0x407f5d8),F(scoreFadeTime,0x4080500)};
static int events[8][3],count;
static void send(const char*s){events[count][0]=1;events[count][1]=!strcmp(s,"score");events[count++][2]=0;}
static void event(int t,qboolean f){events[count][0]=2;events[count][1]=t;events[count++][2]=f;}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE m;byte*b;void(__cdecl*original[3])(void);void(*source[3])(void)={CG_LimboMenu_f,CG_ScoresDown_f,CG_ScoresUp_f};int k,op,i,n,expected[9],copy[8][3];static int gaps[]={-100,0,1999,2000,2001,9000};if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;original[0]=(void*)(b+0x1beb0);original[1]=(void*)(b+0x1bf20);original[2]=(void*)(b+0x1bfa0);hook(b+0x64da0,send);hook(trap_SendClientCommand,send);hook(b+0x4f1b0,event);hook(CG_EventHandling,event);
 for(k=0;k<1536;k++)for(op=0;op<3;op++){
  for(i=0;i<9;i++)*fields[i].p=*(int*)(b+fields[i].off)=17+i;
  cg.scoresRequestTime=*(int*)(b+0x407f5d4)=k-200;
  cg.time=*(int*)(b+0x407dea4)=cg.scoresRequestTime+gaps[k%6];
  cg.demoPlayback=*(int*)(b+0x40352b0)=(k/6)%2;
  cg.mvTotalClients=*(int*)(b+0x4086398)=(k/12)%4-1;
  cg.showGameView=*(int*)(b+0x4081fa8)=(k/48)%2;
  cg.tceShowLimboPanel=*(int*)(b+0x40a49dc)=(k/96)%2;
  cg.showScores=*(int*)(b+0x40804f8)=(k/192)%3;
  count=0;original[op]();n=count;memcpy(copy,events,sizeof(copy));for(i=0;i<9;i++)expected[i]=*(int*)(b+fields[i].off);
  count=0;source[op]();if(n!=count||memcmp(copy,events,n*sizeof(events[0]))){printf("commands case%d op%d calls\n",k,op);return 1;}
  for(i=0;i<9;i++)if(expected[i]!=*fields[i].p){printf("commands case%d op%d field%x\n",k,op,fields[i].off);return 1;}
 }puts("4608 whole LimboMenu/ScoresDown/ScoresUp comparisons passed (1536 each)");return 0;}
