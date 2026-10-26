#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern void RespawnItem(gentity_t *);
static byte raw[8][0x57c];static gentity_t ents[8];static int mode,rng,draws,links,events,indexSeen,eventSeen,paramSeen;
static jmp_buf fatalJump;static int fatalCalls;static char fatalText[1024];
static void fatalSink(const char *text){++fatalCalls;Q_strncpyz(fatalText,text,sizeof(fatalText));longjmp(fatalJump,1);}
static int randomFixed(void){++draws;return rng;}
static int indexOf(void*p){return mode?((byte*)p-(byte*)raw)/0x57c:(gentity_t*)p-ents;}
static void linkSink(void*p){++links;indexSeen=indexOf(p);}
static void eventSink(void*p,int e,int v){++events;if(indexSeen!=indexOf(p)){puts("event entity/order");exit(1);}eventSeen=e;paramSeen=v;}
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*original)(void*);int count,k,j,group,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x5dbf0);
 if(!hook(b+0xa20e4,randomFixed)||!hook(rand,randomFixed)||!hook(b+0x8afa0,linkSink)||!hook(trap_LinkEntity,linkSink)||!hook(b+0x950e0,eventSink)||!hook(G_AddEvent,eventSink))return 2;
 for(group=0;group<2;++group)for(count=1;count<=8;++count)for(k=0;k<256;++k){int expectedIndex,expectedEvent,expectedDraws;
  memset(raw,0,sizeof(raw));memset(ents,0,sizeof(ents));
  for(j=0;j<count;++j){ents[j].team=group?"t":NULL;*(void**)(raw[j]+0x260)=ents[j].team;
   ents[j].teammaster=ents;*(void**)(raw[j]+0x2f8)=raw;
   ents[j].teamchain=j+1<count?ents+j+1:NULL;*(void**)(raw[j]+0x2f4)=j+1<count?raw[j+1]:NULL;
   ents[j].r.contents=*(int*)(raw[j]+0x14c)=k*13;ents[j].flags=*(int*)(raw[j]+0x1b0)=0x5a5a5a5a;
   ents[j].r.svFlags=*(int*)(raw[j]+0x128)=k;ents[j].nextthink=*(int*)(raw[j]+0x29c)=12345;
  }
  rng=k*127;draws=links=events=0;mode=1;original(raw[count-1]);expectedIndex=indexSeen;expectedEvent=eventSeen;expectedDraws=draws;
  if(links!=1||events!=1||paramSeen)return 3;
  draws=links=events=0;mode=0;RespawnItem(ents+count-1);
  if(links!=1||events!=1||paramSeen||eventSeen!=EV_ITEM_RESPAWN||expectedEvent!=46||draws!=expectedDraws||indexSeen!=expectedIndex){fprintf(stderr,"respawn group%d size%d case%d calls mismatch\n",group,count,k);return 1;}
  for(j=0;j<count;++j)if(ents[j].r.contents!=*(int*)(raw[j]+0x14c)||ents[j].flags!=*(int*)(raw[j]+0x1b0)||ents[j].r.svFlags!=*(int*)(raw[j]+0x128)||ents[j].nextthink!=*(int*)(raw[j]+0x29c)){puts("respawn state mismatch");return 1;}++n;
 }
 if(!hook(b+0x8ab30,fatalSink)||!hook(trap_Error,fatalSink))return 2;
 for(mode=0;mode<2;++mode){
  memset(raw,0,sizeof(raw));memset(ents,0,sizeof(ents));ents[0].team="t";*(void**)(raw[0]+0x260)="t";
  draws=links=events=fatalCalls=0;fatalText[0]=0;
  if(setjmp(fatalJump)==0){if(mode)original(raw[0]);else RespawnItem(ents);puts("fatal branch unexpectedly returned");return 1;}
  if(fatalCalls!=1||strcmp(fatalText,"RespawnItem: bad teammaster")||draws||links||events){fprintf(stderr,"fatal contract mismatch original%d calls%d RNG%d links%d events%d text[%s]\n",mode,fatalCalls,draws,links,events,fatalText);return 1;}
 }
 puts("2 original/production fatal NULL-master captures matched exactly before RNG/link/event");
 printf("%d whole valid-chain RespawnItem comparisons passed\n",n);return 0;
}
