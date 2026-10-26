#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <setjmp.h>
extern void CheckExitRules(void),CheckIntermissionExit(void),BeginIntermission(void),LogExit(const char *);
extern qboolean G_ScriptAction_EndRound(gentity_t *,char *);
extern qboolean G_ScriptAction_SetRoundTimelimit(gentity_t *,char *);
static jmp_buf timelimitError;
static unsigned char *b;static char events[8192];static int winner,original;static unsigned rng=76123;
static unsigned next(void){rng=rng*1664525u+1013904223u;return rng>>8;}
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void event(const char *fmt,...){va_list a;va_start(a,fmt);vsprintf(events+strlen(events),fmt,a);va_end(a);}
static void get(int id,char *s,int n){event("get:%d/%d;",id,n);sprintf(s,"\\winner\\%d",winner);}
static void set(int id,const char *s){event("set:%d:%s;",id,s);winner=atoi(Info_ValueForKey(s,"winner"));}
static void send(int id,const char *s){event("send:%d:%s;",id,s);}
static void logexit(const char *s){event("exit:%s;",s);}
static void inter(void){event("inter;");}
static void begin(void){event("begin;");}
static void script(gentity_t *e,char *name,char *arg){event("script:%s:%s;",name,arg);}
static void cvarset(const char *key,const char *value){event("cvar:%s:%s;",key,value);}
static void error(const char *fmt,...){event("error:%s;",fmt);longjmp(timelimitError,1);}
#define BOTH(field,offset,value) field=*(int *)(b+offset)=(value)
int main(int argc,char **argv){HMODULE h;int i,j,expectedWinner,a,c;char expected[8192];int times[]={0,0,0,6999,7000,7001,9999,10000,10001};int modes[]={2,5,7,3,4,6};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x8ade0,get);patch(trap_GetConfigstring,get);patch(b+0x8adc0,set);patch(trap_SetConfigstring,set);patch(b+0x8ad60,send);patch(trap_SendServerCommand,send);
 patch(b+0x626f0,logexit);patch(LogExit,logexit);patch(b+0x623b0,begin);patch(BeginIntermission,begin);patch(b+0x7d4b0,script);patch(G_Script_ScriptEvent,script);
 /* Resolve the original intermission callee from its first relative call. */
 {unsigned char *p=b+0x62f10;for(j=0;j<64;++j)if(p[j]==0xe8){patch(p+j+5+*(int *)(p+j+1),inter);break;}if(j==64)return 9;}patch(CheckIntermissionExit,inter);
 for(i=0;i<150000;++i){
  BOTH(g_gametype.integer,0x6d8d4c,modes[next()%6]);BOTH(g_gamestate.integer,0x784f6c,(next()%6)==0?3:(next()%3)==0?1:0);
  BOTH(level.time,0x7febe0,10000);BOTH(level.startTime,0x7febec,1000);BOTH(level.timeCurrent,0x221bc08,1000+(next()%5)*30000);
  g_timelimit.value=*(float *)(b+0x7fd628)=(float)(next()%3)*.5f;
  BOTH(level.warmupTime,0x7feb90,next()%5==0?1:0);BOTH(level.intermissionQueued,0x7ffc5c,next()%13==0?9000:0);
  BOTH(level.tceExitRulesNotBefore,0x2223928,next()%11==0?10001:0);BOTH(level.tceEndRoundTime,0x2223924,next()%5==0?times[next()%9]:0);
  BOTH(level.tceTimelimitHitTime,0x2223920,next()%5==0?9000+(int)(next()%3)-1:0);BOTH(level.tceBombPlanted,0x2223914,next()%2);
  BOTH(g_fraglimit.integer,0x6d89ec,next()%3==0?10:0);BOTH(level.teamScores[1],0x7febf4,next()%15);BOTH(level.teamScores[2],0x7febf8,next()%15);
  BOTH(level.numPlayingClients,0x7fec14,next()%5);BOTH(g_maxlives.integer,0x6d7fcc,next()%3-1);BOTH(g_axismaxlives.integer,0x23ceecc,next()%3-1);BOTH(g_alliedmaxlives.integer,0x6d91cc,next()%3-1);
  BOTH(level.numTeamClients[0],0x7ff240,next()%4);BOTH(level.numTeamClients[1],0x7ff244,next()%4);BOTH(level.numFinalDead[0],0x221a418,next()%4);BOTH(level.numFinalDead[1],0x221a41c,next()%4);
  BOTH(level.firstbloodTeam,0x221a500,next()%4);BOTH(level.teamEliminateTime,0x221a504,times[next()%9]);BOTH(level.lmsWinningTeam,0x221a508,next()%4);
  level.gameManager=next()%2?g_entities+17:NULL;*(void **)(b+0x221b71c)=level.gameManager?b+0x226e2a0+17*0x57c:NULL;
  winner=(int)(next()%4)-1;expectedWinner=winner;original=1;events[0]=0;
  if(i%3==0){a=((int(__cdecl *)(void *,void *))(b+0x82e90))(NULL,NULL);c=G_ScriptAction_EndRound(NULL,NULL);if(a!=c||level.tceEndRoundTime!=*(int *)(b+0x2223924))return 3;}
  ((void(__cdecl *)(void))(b+0x62f10))();strcpy(expected,events);a=winner;winner=expectedWinner;original=0;events[0]=0;CheckExitRules();
  if(strcmp(expected,events)||winner!=a){printf("case%d gt%d gs%d: original=%s source=%s\n",i,g_gametype.integer,g_gamestate.integer,expected,events);return 1;}
  if(level.tceEndRoundTime!=*(int *)(b+0x2223924)||level.tceTimelimitHitTime!=*(int *)(b+0x2223920)||level.teamEliminateTime!=*(int *)(b+0x221a504)||level.lmsWinningTeam!=*(int *)(b+0x221a508)||level.intermissionQueued!=*(int *)(b+0x7ffc5c))return 4;
 }
 patch(b+0x8ac90,cvarset);patch(trap_Cvar_Set,cvarset);patch(b+0x5ee10,error);patch(G_Error,error);
 {int gt,u,t,p,failed;int users[]={-1,0,1,2,3,8};float limits[]={-1,0,.5f,3};char *params[]={"","2","4.5","-1","garbage"};
 for(gt=0;gt<8;++gt)for(u=0;u<6;++u)for(t=0;t<4;++t)for(p=0;p<5;++p){
  BOTH(g_gametype.integer,0x6d8d4c,gt);BOTH(g_userTimeLimit.integer,0x6d8c2c,users[u]);g_nextTimeLimit.value=*(float *)(b+0x7e1788)=limits[t];
  events[0]=0;a=-1;failed=setjmp(timelimitError);if(!failed)a=((int(__cdecl *)(void *,char *))(b+0x82eb0))(NULL,params[p]);strcpy(expected,events);
  events[0]=0;c=-1;if(!setjmp(timelimitError))c=G_ScriptAction_SetRoundTimelimit(NULL,params[p]);
  if(a!=c||strcmp(expected,events)){printf("timelimit mismatch %s / %s\n",expected,events);return 6;}
 }}
 printf("150000 Windows CheckExitRules, 50000 EndRound, 960 SetRoundTimelimit comparisons passed\n");return 0;
}
