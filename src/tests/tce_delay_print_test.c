#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void G_delayPrint(gentity_t*);
static byte raw[0x57c], clients[4][0x2000], *base;static gentity_t ent;static gclient_t nc[4];static int side,scenario,bad,count;
typedef struct {int kind,a,b;char text[1024];} record;static record events[32];
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
static void logEvent(int k,int a,int b,const char*s){record*r;if(count>=32){bad=1;return;}r=&events[count++];memset(r,0,sizeof(*r));r->kind=k;r->a=a;r->b=b;if(s)Q_strncpyz(r->text,s,sizeof(r->text));}
static void send(int n,const char*s){logEvent(1,n,0,s);}static void config(int n,const char*s){logEvent(2,n,0,s);}static void sound(char*s){logEvent(3,0,0,s);}static void print(char*s,void*p){if(p)bad=1;logEvent(4,0,0,s);}
static void spawnPrint(int kind,int time,void*p){if(p)bad=1;logEvent(5,kind,time,NULL);}
static void release(void*p){if(p!=(side?(void*)&ent:(void*)raw))bad=1;logEvent(6,side?ent.nextthink:*(int*)(raw+0x29c),side?!!ent.think:!!*(void**)(raw+0x2a4),NULL);}
static void regen(void*p,int refs){int ix=side?(gentity_t*)p-g_entities:((byte*)p-(base+0x226e2a0))/0x57c;logEvent(7,ix,refs,NULL);if(scenario&16){if(side)level.numConnectedClients=1;else *(int*)(base+0x7fec0c)=1;}}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*original)(void*);int type,pause,sc,j,n=0;int deltas[]={-1001,-1,0,999,1000,1001,9999,10000,10001,10999,11000,15001};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x651f0);
 if(!hook(base+0x8ad60,send)||!hook(trap_SendServerCommand,send)||!hook(base+0x8adc0,config)||!hook(trap_SetConfigstring,config)||!hook(base+0x651b0,sound)||!hook(G_globalSound,sound)||!hook(base+0x65120,print)||!hook(G_printFull,print)||!hook(base+0x65400,spawnPrint)||!hook(G_spawnPrintf,spawnPrint)||!hook(base+0x94f30,release)||!hook(G_FreeEntity,release)||!hook(base+0x75de0,regen)||!hook(G_smvRegenerateClients,regen))return 2;
 for(type=-1;type<10;type++)for(pause=0;pause<5;pause++)for(sc=0;sc<384;sc++){
  record expected[32];int expectedCount;scenario=sc;memset(raw,0,sizeof(raw));memset(&ent,0,sizeof(ent));
  ent.spawnflags=*(int*)(raw+0x1a8)=type;ent.timestamp=*(int*)(raw+0x24c)=100000-deltas[sc%12];ent.think=G_delayPrint;*(void**)(raw+0x2a4)=original;
  level.time=*(int*)(base+0x7febe0)=100000;level.startTime=*(int*)(base+0x7febec)=95000;level.timeDelta=*(int*)(base+0x221bc0c)=sc-192;
  level.match_pause=*(int*)(base+0x221bafc)=pause;level.server_settings=*(int*)(base+0x221bb04)=sc;
  match_timeoutlength.integer=*(int*)(base+0x7e21ac)=(sc%4)-1;
  level.numConnectedClients=*(int*)(base+0x7fec0c)=(sc&32)?0:4;
  for(j=0;j<4;j++){g_entities[j].client=&nc[j];*(void**)(base+0x226e2a0+j*0x57c+0x190)=clients[j];level.sortedClients[j]=*(int*)(base+0x7fec18+j*4)=(j+sc)%4;nc[j].pers.mvReferenceList=*(int*)(clients[j]+0xaec)=(sc+j)%3;nc[j].sess.sessionTeam=*(int*)(clients[j]+0xb30)=(sc+j)%4;}
  side=0;bad=count=0;original(raw);if(bad)return 3;expectedCount=count;memcpy(expected,events,sizeof(expected));side=1;bad=count=0;G_delayPrint(&ent);
  if(bad||count!=expectedCount||memcmp(expected,events,count*sizeof(record))||ent.nextthink!=*(int*)(raw+0x29c)||!!ent.think!=!!*(void**)(raw+0x2a4)||level.match_pause!=*(int*)(base+0x221bafc)||level.server_settings!=*(int*)(base+0x221bb04)||level.numConnectedClients!=*(int*)(base+0x7fec0c)){fprintf(stderr,"delay type%d pause%d scenario%d events%d/%d failed\n",type,pause,sc,count,expectedCount);return 1;}n++;
 }
 printf("%d whole delay-print controller comparisons passed\n",n);return 0;
}
