#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
extern void LogExit(const char *);
extern void bani_storemapxp(void);
extern void G_BuildEndgameStats(void);
static unsigned char *b,raw[16][0x16b0];static gclient_t clients[16];
static char events[32768];static int original,winner,defender,values[5];
static const char *keys[]={"g_axiswins","g_alliedwins","g_currentRound","g_lms_currentMatch","g_nextTimeLimit"};
static const int offsets[]={0x23ce260,0x76bce0,0x2223ce0,0x23cdcc0};
static vmCvar_t *vars[]={&g_axiswins,&g_alliedwins,&g_currentRound,&g_lms_currentMatch};
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void event(const char *fmt,...){va_list ap;va_start(ap,fmt);vsprintf(events+strlen(events),fmt,ap);va_end(ap);}
static void logmsg(const char *fmt,...){va_list ap;strcat(events,"log:");va_start(ap,fmt);vsprintf(events+strlen(events),fmt,ap);va_end(ap);}
static void set(const char *k,const char *v){int i;event("set:%s=%s;",k,v);for(i=0;i<5;++i)if(!strcmp(k,keys[i])){values[i]=atoi(v);return;}exit(3);}
static void update(vmCvar_t *v){int i;for(i=0;i<4;++i)if(v==vars[i]||(void *)v==b+offsets[i]){event("update:%s;",keys[i]);v->integer=values[i];return;}exit(4);}
static void config(int id,const char *s){event("config:%d=%s;",id,s);}
static void get(int id,char *s,int n){event("get:%d/%d;",id,n);sprintf(s,"\\winner\\%d\\defender\\%d",winner,defender);}
static void console(int mode,const char *s){event("console:%d:%s;",mode,s);}
static void xp(void){event("xp;");}
static void stats(void){event("endstats;");}
static void userinfo(int index){event("userinfo:%d;",index);}
static void script(gentity_t *ent,char *name,char *arg){event("script:%s:%s;",name,arg);if(original){if(!strcmp(arg,"terrorists_win"))*(int *)(b+0x7febf4)+=20;if(!strcmp(arg,"specops_win"))*(int *)(b+0x7febf8)+=20;}else{if(!strcmp(arg,"terrorists_win"))level.teamScores[1]+=20;if(!strcmp(arg,"specops_win"))level.teamScores[2]+=20;}}
static void seed(int gt,int gs,int round,int limit,int variant,int manager){int i,j,n=variant==0?0:variant==1?1:variant==2?4:16;
 level.clients=clients;*(void **)(b+0x7feb80)=raw;level.numConnectedClients=*(int *)(b+0x7fec0c)=n;
 g_gametype.integer=*(int *)(b+0x6d8d4c)=gt;g_gamestate.integer=*(int *)(b+0x784f6c)=gs;g_currentRound.integer=*(int *)(b+0x2223cec)=round;
 g_lms_roundlimit.integer=*(int *)(b+0x7e19cc)=limit;g_lms_matchlimit.integer=*(int *)(b+0x76be0c)=variant;g_lms_currentMatch.integer=*(int *)(b+0x23cdccc)=variant%2;
 g_axiswins.integer=*(int *)(b+0x23ce26c)=variant*7;g_alliedwins.integer=*(int *)(b+0x76bcec)=variant*3;
 level.time=*(int *)(b+0x7febe0)=123456;level.startTime=*(int *)(b+0x7febec)=900;level.timeCurrent=*(int *)(b+0x221bc08)=variant*91919;g_timelimit.value=*(float *)(b+0x7fd628)=variant*.37f;
 level.intermissionQueued=*(int *)(b+0x7ffc5c)=0;level.lmsDoNextMap=*(int *)(b+0x7ffc84)=0;
 level.teamScores[1]=*(int *)(b+0x7febf4)=variant*5-3;level.teamScores[2]=*(int *)(b+0x7febf8)=variant*3;
 level.firstbloodTeam=*(int *)(b+0x221a500)=variant;level.gameManager=manager?g_entities+17:NULL;*(void **)(b+0x221b71c)=manager?b+0x226e2a0+17*0x57c:NULL;
 level.currentCampaign=*(int *)(b+0x221a510)=0;g_campaigns[0].current=*(int *)(b+0x6da2e4)=variant;
 for(i=0;i<n;++i){int idx=n-1-i;gclient_t *cl=clients+idx;unsigned char *r=raw[idx];memset(cl,0,sizeof(*cl));memset(r,0,0x16b0);level.sortedClients[i]=*(int *)(b+0x7fec18+4*i)=idx;
  g_entities[idx].client=cl;*(void **)(b+0x226e430+idx*0x57c)=r;
  cl->sess.sessionTeam=*(int *)(r+0xb30)=(i+variant)%4;cl->pers.connected=*(int *)(r+0x5ac)=i%3;cl->ps.ping=*(int *)(r+0x468)=i*150-1;
  cl->ps.persistant[PERS_SCORE]=*(int *)(r+0x110)=i*9;cl->ps.eFlags=*(int *)(r+0x68)=i*123+8;g_entities[idx].s.eFlags=*(int *)(b+0x226e2a8+idx*0x57c)=i*37+8;cl->pers.ready=*(int *)(r+0xafc)=i%2;
  sprintf(cl->pers.netname,"player%d",idx);strcpy((char *)r+0x5f8,cl->pers.netname);
  for(j=0;j<7;++j){cl->sess.skillpoints[j]=*(float *)(r+0xb68+4*j)=(i+j)%5+(variant==3?.75f:0);cl->sess.medals[j]=*(int *)(r+0xbc4+4*j)=i;}
 }
}
int main(int argc,char **argv){HMODULE h;int gt,gs,w,r,l,v,m,i,j,n,cases=0;char expected[32768];const int limits[]={0,3,5},rounds[]={0,1,2,4,5,9};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x62600,logmsg);patch(G_LogPrintf,logmsg);patch(b+0x8ac90,set);patch(trap_Cvar_Set,set);patch(b+0x8ac70,update);patch(trap_Cvar_Update,update);patch(b+0x8adc0,config);patch(trap_SetConfigstring,config);patch(b+0x8ade0,get);patch(trap_GetConfigstring,get);patch(b+0x8ac30,console);patch(trap_SendConsoleCommand,console);patch(b+0x60a50,xp);patch(bani_storemapxp,xp);patch(b+0x879a0,stats);patch(G_BuildEndgameStats,stats);patch(b+0x7d4b0,script);patch(G_Script_ScriptEvent,script);
 /* ClientUserinfoChanged only updates external presentation; medals are compared. */
 patch(b+0x4bf70,userinfo);patch(ClientUserinfoChanged,userinfo);
 for(gt=0;gt<8;++gt)for(gs=0;gs<4;++gs)for(w=-1;w<3;++w)for(r=0;r<6;++r)for(l=0;l<3;++l)for(v=0;v<4;++v)for(m=0;m<2;++m){
  winner=w;defender=v%2;seed(gt,gs,rounds[r],limits[l],v,m);original=1;events[0]=0;((void(__cdecl *)(const char *))(b+0x626f0))("fixture");strcpy(expected,events);original=0;events[0]=0;LogExit("fixture");
  if(strcmp(events,expected)){printf("LogExit mismatch gt%d gs%d winner%d round%d limit%d v%d manager%d\n%s\n%s\n",gt,gs,w,rounds[r],limits[l],v,m,expected,events);return 1;}
  if(level.intermissionQueued!=*(int *)(b+0x7ffc5c)||level.lmsDoNextMap!=*(int *)(b+0x7ffc84))return 2;
  for(i=0;i<4;++i)if(vars[i]->integer!=*(int *)(b+offsets[i]+12))return 3;
  n=level.numConnectedClients;for(i=0;i<n;++i){gclient_t *cl=clients+i;unsigned char *rawcl=raw[i];if(cl->pers.ready!=*(int *)(rawcl+0xafc)||cl->ps.eFlags!=*(int *)(rawcl+0x68)||g_entities[i].s.eFlags!=*(int *)(b+0x226e2a8+i*0x57c))return 4;for(j=0;j<7;++j)if(cl->sess.medals[j]!=*(int *)(rawcl+0xbc4+j*4))return 5;}++cases;
 }
 printf("%d whole Windows LogExit/G_MakeUnready comparisons passed\n",cases);return 0;
}
