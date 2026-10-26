#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern qboolean G_IsOnFireteam(int entityNum, fireteamData_t **teamNum);
extern void G_Say(gentity_t *ent, gentity_t *target, int mode, const char *chatText);
extern void G_SayTo(gentity_t*,gentity_t*,int,int,const char*,const char*,qboolean),Cmd_Say_f(gentity_t*,int,qboolean);extern char*ConcatArgs(int);
static byte*base,rawClients[2][0x16b0];static gclient_t clients[2];static int side,v,count,bad;static int fireteams[2];
typedef struct{int k,a,b,c;char text[512];}event;static event calls[8];
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static byte*raw(int n){return base+0x226e2a0+n*0x57c;}static void*ent(int n){return side?(void*)&g_entities[n]:(void*)raw(n);}static int idx(void*p){return !p?-1:side?(gentity_t*)p-g_entities:((byte*)p-raw(0))/0x57c;}
static event*note(int k){event*e;if(count==8)exit(3);e=&calls[count++];e->k=k;return e;}
static int same(void*a,void*b){event*e=note(1);e->a=idx(a);e->b=idx(b);return(v>>13)&1;}
static int fireteam(int n,void**out){event*e=note(2);int x=(v>>14)&3;e->a=n;*out=&fireteams[(v>>16)&1&&n==9?1:0];return x==0?0:x==1?n==5:x==2?n==9:1;}
static void send(int n,const char*s){event*e=note(3);e->a=n;Q_strncpyz(e->text,s,sizeof(e->text));}
static int argcMock(void){note(4);return v%5;}
static char*concat(int n){event*e=note(5);e->a=n;return "^2Message text";}
static void say(void*a,void*b,int mode,const char*s){event*e=note(6);e->a=idx(a);e->b=idx(b);e->c=mode;Q_strncpyz(e->text,s,sizeof(e->text));}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*sayto)(void*,void*,int,int,const char*,const char*,int);void(__cdecl*cmd)(void*,int,int);int phase,i,slot,n,total=0,originalCount=0;static event expected[8];
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;sayto=(void*)(base+0x50fa0);cmd=(void*)(base+0x514f0);
#define H(r,f,t) if(!hook(base+r,t)||!hook(f,t))return 2
 H(0x8daa0,OnSameTeam,same);H(0x5c110,G_IsOnFireteam,fireteam);H(0x8ad60,trap_SendServerCommand,send);H(0x8ab60,trap_Argc,argcMock);H(0x4f440,ConcatArgs,concat);H(0x51150,G_Say,say);
 for(phase=0;phase<2;phase++)for(v=0;v<(phase?1280:131072);v++)for(side=0;side<2;side++){
  count=bad=0;memset(calls,0,sizeof(calls));memset(clients,0,sizeof(clients));memset(rawClients,0,sizeof(rawClients));for(i=0;i<2;i++){gentity_t*e;byte*p;slot=i?9:5;e=&g_entities[slot];p=raw(slot);memset(e,0,sizeof(*e));memset(p,0,0x57c);e->client=&clients[i];*(void**)(p+0x190)=rawClients[i];e->inuse=*(int*)(p+0x194)=1;e->s.clientNum=*(int*)(p+0xac)=i&&((v>>12)&1)?5:slot;clients[i].sess.sessionTeam=*(int*)(rawClients[i]+0xb30)=(v>>(4+i*2))&3;clients[i].ps.stats[STAT_HEALTH]=*(int*)(rawClients[i]+0xd0)=((v>>(8+i))&1)?100:0;clients[i].sess.referee=*(int*)(rawClients[i]+0xbf8)=(v>>10)&1;clients[i].ps.clientNum=*(int*)(rawClients[i]+0xa0)=63-slot;}
  n=(v>>2)&3;if(n==1)g_entities[9].inuse=*(int*)(raw(9)+0x194)=0;if(n==2){g_entities[9].client=NULL;*(void**)(raw(9)+0x190)=NULL;}
  if(!phase){if(side)G_SayTo(ent(5),n==0?NULL:ent(9),v&3,'0'+(v%8),"^1Name: ","Message \"quoted\" text",(v>>11)&1);else sayto(ent(5),n==0?NULL:ent(9),v&3,'0'+(v%8),"^1Name: ","Message \"quoted\" text",(v>>11)&1);}else{if(side)Cmd_Say_f(ent(5),(v/5)%4,(v/20)%3);else cmd(ent(5),(v/5)%4,(v/20)%3);}
  if(!side){originalCount=count;memcpy(expected,calls,sizeof(calls));}else{if(bad||originalCount!=count||memcmp(expected,calls,sizeof(calls))){printf("Chat delivery mismatch phase%d case%d calls%d/%d\n",phase,v,originalCount,count);for(i=0;i<originalCount||i<count;i++)if(memcmp(&expected[i],&calls[i],sizeof(event)))printf("call%d %d,%d,%d,%d[%s] / %d,%d,%d,%d[%s]\n",i,expected[i].k,expected[i].a,expected[i].b,expected[i].c,expected[i].text,calls[i].k,calls[i].a,calls[i].b,calls[i].c,calls[i].text);return 1;}total++;}
 }
 printf("PASS whole G_SayTo/Cmd_Say_f %d cases (131072 delivery,1280 command)\n",total);return 0;
}
