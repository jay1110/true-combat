#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include "../game/tce_botinfo.h"
#include <windows.h>
#include <stdio.h>
static byte*base,rawClients[64][0x16b0],expected[sizeof(tceBotInfo)];static gclient_t clients[64];
static int variant,side,count,bad;static const char*query;
typedef struct{int kind,a;char text[128];}event;static event events[128],wantEvents[128];
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void note(int k,int a,const char*s){event*e;if(count>=128)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->kind=k;e->a=a;if(s)Q_strncpyz(e->text,s,sizeof(e->text));}
static int argcHook(void){note(1,variant%4,NULL);return variant%4;}
static void argvHook(int n,char*s,int len){note(2,n,NULL);if(len!=1024)bad=1;Q_strncpyz(s,query,len);}
static void send(int mode,const char*s){note(3,mode,s);}
static void QDECL print(const char*f,...){va_list a;char b[128];va_start(a,f);Q_vsnprintf(b,sizeof(b),f,a);va_end(a);note(4,0,b);}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*clear)(int),(__cdecl*init)(void),(__cdecl*update)(void),(__cdecl*kick)(void);int(__cdecl*match)(const char*,int*);int phase,v,i,j,n=0,ret,wantRet=0,wantCount=0,wantBotCount=0,matches[64],wanted[64];const char*queries[]={"", "bot", "^1BoT", "ALPHA", "eta", "not found", "123456789012345678901234567890123456", "^7"};const char*names[]={"^1Bot Alpha","aBOTb","Beta","ALPHA","","^2","123456789012345678901234567890123456","human"};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;if(sizeof(tceBotInfo_t)!=0x1258)return 3;clear=(void*)(base+0x3e930);init=(void*)(base+0x3ea40);update=(void*)(base+0x3ea60);match=(void*)(base+0x54d00);kick=(void*)(base+0x89fe0);
#define H(r,fn,repl) if(!hook(base+r,repl)||!hook(fn,repl))return 2
 H(0x8ab60,trap_Argc,argcHook);H(0x8ab70,trap_Argv,argvHook);H(0x8ac30,trap_SendConsoleCommand,send);H(0x5ed80,G_Printf,print);
 for(phase=0;phase<5;phase++)for(v=0;v<512;v++)for(side=0;side<2;side++){
  byte*state=side?(byte*)tceBotInfo:base+0x2224700;variant=v;query=queries[(v/4)%8];count=bad=ret=0;memset(events,0,sizeof(events));memset(matches,0x5a,sizeof(matches));memset(tceBotInfo,v,sizeof(tceBotInfo));memset(base+0x2224700,v,sizeof(tceBotInfo));tceNumBots=*(int*)(base+0x247d78)=v%17-3;level.maxclients=*(int*)(base+0x7febd8)=v%65;
  for(i=0;i<64;i++){byte*p=base+0x226e2a0+i*0x57c,*q=rawClients[i];gentity_t*e=&g_entities[i];memset(e,0,sizeof(*e));memset(p,0,0x57c);memset(&clients[i],0,sizeof(clients[i]));memset(q,0,0x16b0);e->client=(v+i)%5?&clients[i]:NULL;*(void**)(p+0x190)=(v+i)%5?q:NULL;clients[i].pers.connected=*(int*)(q+0x5ac)=(v+i)%3;Q_strncpyz(clients[i].pers.netname,names[(v+i)%8],sizeof(clients[i].pers.netname));Q_strncpyz((char*)q+0x5f8,names[(v+i)%8],36);e->r.svFlags=*(int*)(p+0x128)=(v+i)%2?SVF_BOT:0;tceBotInfo[i].active=*(int*)(base+0x2224700+i*0x1258)=(v+i)%4==0?0:(v+i)%2?1:-1;}
  if(phase==0){if(side)TCE_ClearBotInfo(v%64);else clear(v%64);}
  if(phase==1){if(side)TCE_InitBotInfo();else init();}
  if(phase==2){if(side)TCE_UpdateBotInfo();else update();}
  if(phase==3)ret=side?TCE_ClientNumberFromNameMatch(query,matches):match(query,matches);
  if(phase==4){if(side)TCE_SvcmdKickBot();else kick();}
  if(!side){memcpy(expected,state,sizeof(expected));memcpy(wanted,matches,sizeof(wanted));memcpy(wantEvents,events,sizeof(events));wantCount=count;wantRet=ret;wantBotCount=*(int*)(base+0x247d78);}else{if(bad||ret!=wantRet||count!=wantCount||tceNumBots!=wantBotCount||memcmp(expected,state,sizeof(expected))||memcmp(wanted,matches,sizeof(matches))||memcmp(wantEvents,events,sizeof(events))){printf("BotInfo mismatch phase%d case%d bad%d return%d/%d count%d/%d bots%d/%d\n",phase,v,bad,wantRet,ret,wantCount,count,wantBotCount,tceNumBots);for(j=0;j<sizeof(expected);j++)if(expected[j]!=state[j]){printf("state byte%x %02x/%02x\n",j,expected[j],state[j]);break;}for(j=0;j<64;j++)if(wanted[j]!=matches[j]){printf("match%d %d/%d\n",j,wanted[j],matches[j]);break;}for(j=0;j<count||j<wantCount;j++)if(memcmp(&events[j],&wantEvents[j],sizeof(event)))printf("event%d %d,%d[%s]/%d,%d[%s]\n",j,wantEvents[j].kind,wantEvents[j].a,wantEvents[j].text,events[j].kind,events[j].a,events[j].text);return 1;}n++;}
 }
 printf("PASS five BotInfo controllers %d cases (512 per body)\n",n);return 0;
}
