#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
extern void CheckWolfMP(void);
static unsigned char *base,raw[64][0x16b0];
static gclient_t clients[64];
static char events[4096];
static int stateValue;
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void event(const char *fmt,...){va_list ap;va_start(ap,fmt);vsprintf(events+strlen(events),fmt,ap);va_end(ap);}
static void set(const char *k,const char *v){event("set:%s=%s;",k,v);if(!strcmp(k,"gamestate"))stateValue=atoi(v);}
static void update(vmCvar_t *v){if(v!=&g_gamestate && (void *)v!=base+0x784f60){puts("Wrong updated Cvar pointer");exit(1);}event("update:gamestate;");v->integer=stateValue;}
static void config(int id,const char *v){event("config:%d=%s;",id,v);}
static void server(int id,const char *v){event("server:%d=%s;",id,v);}
static void console(int id,const char *v){event("console:%d=%s;",id,v);}
static void seed(int n,int variant){int i;level.clients=clients;*(void **)(base+0x7feb80)=raw;
 level.numConnectedClients=*(int *)(base+0x7fec0c)=n;level.numNonSpectatorClients=*(int *)(base+0x7fec10)=n;level.numPlayingClients=*(int *)(base+0x7fec14)=n;level.voteInfo.numVotingClients=*(int *)(base+0x7ff130)=variant&1?n:0;
 for(i=0;i<n;++i){int index=n-1-i;level.sortedClients[i]=*(int *)(base+0x7fec18+i*4)=index;clients[index].pers.connected=*(int *)(raw[index]+0x5ac)=variant==0?2:(index+variant)%3;clients[index].sess.sessionTeam=*(int *)(raw[index]+0xb30)=variant<2?1:index%4;clients[index].pers.ready=*(int *)(raw[index]+0xafc)=(index+variant)%2;g_entities[index].r.svFlags=*(int *)(base+0x226e3c8+index*0x57c)=index%3==0?SVF_BOT:0;}
 level.ref_allready=*(int *)(base+0x221bb00)=variant==3;teamInfo[1].team_lock=*(int *)(base+0x66fe0c)=0;teamInfo[2].team_lock=*(int *)(base+0x66fe34)=0;
 level.lastRestartTime=*(int *)(base+0x221a414)=2000;level.restarted=*(int *)(base+0x7fec08)=0;level.intermissiontime=*(int *)(base+0x7ffc60)=variant&1;
}
static void settings(int gt,int state,int warm,int round,int now,int doWarm){g_gametype.integer=*(int *)(base+0x6d8d4c)=gt;g_gamestate.integer=*(int *)(base+0x784f6c)=state;g_warmup.integer=*(int *)(base+0x23cea4c)=warm;g_currentRound.integer=*(int *)(base+0x2223cec)=round;level.time=*(int *)(base+0x7febe0)=now;level.warmupTime=*(int *)(base+0x7feb90)=12000;g_doWarmup.integer=*(int *)(base+0x7851ac)=doWarm;
 match_minplayers.integer=*(int *)(base+0x7fdcec)=2;match_readypercent.integer=*(int *)(base+0x2223bcc)=50;g_minGameClients.integer=*(int *)(base+0x7e154c)=1;g_lms_lockTeams.integer=*(int *)(base+0x785bcc)=round&1;stateValue=state;
}
static int same(void){return level.lastRestartTime==*(int *)(base+0x221a414)&&level.restarted==*(int *)(base+0x7fec08)&&level.warmupTime==*(int *)(base+0x7feb90)&&level.ref_allready==*(int *)(base+0x221bb00)&&teamInfo[1].team_lock==*(int *)(base+0x66fe0c)&&teamInfo[2].team_lock==*(int *)(base+0x66fe34)&&g_gamestate.integer==*(int *)(base+0x784f6c);}
int main(int argc,char **argv){HMODULE h;int n,v,gt,gs,w,r,t,d,f,a,c,cases=0;const int sizes[]={0,1,2,7,64},warms[]={-2,0,5,9,10,11,30},times[]={1999,2000,3000,3001,12000,12001,22001};char expected[4096];
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)h;
 patch(base+0x8ac90,set);patch(trap_Cvar_Set,set);patch(base+0x8ac70,update);patch(trap_Cvar_Update,update);patch(base+0x8adc0,config);patch(trap_SetConfigstring,config);patch(base+0x8ad60,server);patch(trap_SendServerCommand,server);patch(base+0x8ac30,console);patch(trap_SendConsoleCommand,console);
 for(f=0;f<3;++f)for(n=0;n<5;++n)for(v=0;v<4;++v)for(gt=0;gt<8;++gt)for(gs=0;gs<4;++gs)for(w=0;w<7;++w)for(r=0;r<2;++r)for(t=0;t<7;++t)for(d=0;d<2;++d){
  seed(sizes[n],v);settings(gt,gs,warms[w],r,times[t],d);
  {const int percents[]={0,1,49,50,51,99,100};const int minimums[]={0,1,2,64};match_readypercent.integer=*(int *)(base+0x2223bcc)=percents[w];match_minplayers.integer=*(int *)(base+0x7fdcec)=minimums[v];}
  events[0]=0;
  if(f==0)a=((int(__cdecl *)(void))(base+0x8f890))();else if(f==1)a=((int(__cdecl *)(void))(base+0x8f9a0))();else{((void(__cdecl *)(void))(base+0x635a0))();a=0;}
  strcpy(expected,events);events[0]=0;stateValue=gs;
  if(f==0)c=G_checkReady();else if(f==1)c=G_readyMatchState();else{CheckWolfMP();c=0;}
  if(a!=c||strcmp(expected,events)||!same()){printf("mismatch function%d n%d v%d gt%d gs%d w%d r%d t%d d%d\n%s\n%s\n",f,sizes[n],v,gt,gs,warms[w],r,times[t],d,expected,events);return 1;}++cases;
 }
 printf("%d whole original ready/countdown chain comparisons passed\n",cases);return 0;
}
