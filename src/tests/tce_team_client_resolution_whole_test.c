#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void G_TeamDataForString(const char*,int,team_t*,spectatorState_t*,int*);extern int ClientNumberFromString(gentity_t*,char*);
static byte*base,rawClients[64][0x16b0];static gclient_t clients[64];static int side,v,count;static char query[1024];
typedef struct{int k,a,b;char text[1200];}event;static event calls[4];
typedef struct{int ret,team,state,spec,count;event calls[4];}result;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static event*note(int k){event*e;if(count==4)exit(3);e=&calls[count++];e->k=k;return e;}
static int pick(int n){note(1)->a=n;return(v/64)%2+1;}
static int join(int t,void*p){event*e=note(2);e->a=t;e->b=side?(gentity_t*)p-g_entities:((byte*)p-base-0x226e2a0)/0x57c;return(v/128)%2;}
static void send(int n,const char*s){event*e=note(3);e->a=n;Q_strncpyz(e->text,s,sizeof(e->text));}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*team)(const char*,int,int*,int*,int*);int(__cdecl*number)(void*,char*);int phase,i,total=0,n,ret,spec;team_t t;spectatorState_t state;static result a,b;const char*aliases[]={"follow1","FOLLOW1","follow2","FoLlOw2","spectator","S","red","r","AXIS","blue","B","allies","","auto","terrorists","specops"," red","blue ","follow3"};const char*names[]={"^1Jay","jAy","^212","Alice"," ALICE","Bob","","^7","^1A^2B","A\tB"};const char*queries[]={"Jay","^3JAY","12","012","0","63","64","999","-1","Alice"," ALICE","Bob","missing","","^7","AB","A\tB"," 12","12x"};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;team=(void*)(base+0x4fee0);number=(void*)(base+0x4f540);
#define H(r,f,t) if(!hook(base+r,t)||!hook(f,t))return 2
 H(0x4b900,PickTeam,pick);H(0x8fb40,G_teamJoinCheck,join);H(0x8ad60,trap_SendServerCommand,send);
 for(phase=0;phase<2;phase++)for(v=0;v<16384;v++)for(side=0;side<2;side++){
  result*r=side?&b:&a;n=v%64;count=0;memset(calls,0,sizeof(calls));memset(clients,0,sizeof(clients));memset(rawClients,0,sizeof(rawClients));level.clients=clients;*(void**)(base+0x7feb80)=rawClients;level.maxclients=*(int*)(base+0x7febd8)=(v/19)%65;
  for(i=0;i<64;i++){clients[i].pers.connected=*(int*)(rawClients[i]+0x5ac)=(i+v/1235)%3;Q_strncpyz(clients[i].pers.netname,names[(i+v/65)%10],sizeof(clients[i].pers.netname));strcpy((char*)rawClients[i]+0x5f8,clients[i].pers.netname);}
  t=(team_t)77;state=(spectatorState_t)88;spec=99;ret=0;if(!phase){if(side)G_TeamDataForString(aliases[v%19],n,&t,&state,v&1?NULL:&spec);else team(aliases[v%19],n,(int*)&t,(int*)&state,v&1?NULL:&spec);}else{Q_strncpyz(query,queries[v%19],sizeof(query));ret=side?ClientNumberFromString(&g_entities[n],query):number(base+0x226e2a0+n*0x57c,query);}
  memset(r,0,sizeof(*r));r->ret=ret;r->team=t;r->state=state;r->spec=spec;r->count=count;memcpy(r->calls,calls,sizeof(calls));if(side){if(memcmp(&a,&b,sizeof(a))){printf("Team/client resolution mismatch phase%d case%d ret%d/%d team%d/%d state%d/%d spec%d/%d calls%d/%d\n",phase,v,a.ret,b.ret,a.team,b.team,a.state,b.state,a.spec,b.spec,a.count,b.count);for(i=0;i<a.count||i<b.count;i++)if(memcmp(&a.calls[i],&b.calls[i],sizeof(event)))printf("call%d %d,%d,%d[%s] / %d,%d,%d[%s]\n",i,a.calls[i].k,a.calls[i].a,a.calls[i].b,a.calls[i].text,b.calls[i].k,b.calls[i].a,b.calls[i].b,b.calls[i].text);return 1;}total++;}
 }
 printf("PASS whole team alias and client resolution %d cases (16384 each), real sanitize/atoi/string helpers\n",total);return 0;
}
