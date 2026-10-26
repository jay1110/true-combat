#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
extern void G_ClientSwap(gclient_t *);
static char events[8192],session[2048];static int mv;
static void patch(void *a,void *f){DWORD old,tmp;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void set(const char *k,const char *v){strcat(events,va("set:%s=%s;",k,v));if(!strcmp(k,"session0"))strcpy(session,v);}
static void get(const char *k,char *v,int n){strcat(events,va("get:%s;",k));Q_strncpyz(v,!strcmp(k,"session0")?session:!strcmp(k,"wstats0")?(mv&1?"stats":""):"1.5 2.5 3.5 4.5 5.5 6.5 7.5 1 2 3 4 5 6 7",n);}
static int list(void *e){return mv;}
static void del(int n){strcat(events,va("delete:%d;",n));}
static char *stats(void *e){strcat(events,"stats;");return "statsdata";}
static void parse(char *s){strcat(events,va("parse:%s;",s));}
static void rank(void *c){strcat(events,"rank;");}
static void swap(void *c){strcat(events,"swap;");}
static void print(const char *fmt,...){char s[1024];va_list ap;va_start(ap,fmt);vsprintf(s,fmt,ap);va_end(ap);strcat(events,s);}
static const struct {size_t source;int original;} fields[]={
 {offsetof(gclient_t,sess.sessionTeam),0xb30},
 {offsetof(gclient_t,sess.spectatorTime),0xb34},
 {offsetof(gclient_t,sess.spectatorState),0xb38},
 {offsetof(gclient_t,sess.spectatorClient),0xb3c},
 {offsetof(gclient_t,sess.playerType),0xb40},
 {offsetof(gclient_t,sess.playerWeapon),0xb44},
 {offsetof(gclient_t,sess.playerWeapon2),0xb48},
 {offsetof(gclient_t,sess.playerWeapon3),0xde4},
 {offsetof(gclient_t,sess.latchPlayerType),0xb50},
 {offsetof(gclient_t,sess.latchPlayerWeapon),0xb54},
 {offsetof(gclient_t,sess.latchPlayerWeapon2),0xb58},
 {offsetof(gclient_t,sess.latchPlayerWeapon3),0xde8},
 {offsetof(gclient_t,sess.tceSetupFlags),0xde0},
 {offsetof(gclient_t,sess.tceSessionValues[0]),0xdec},
 {offsetof(gclient_t,sess.tceSessionValues[1]),0xdf0},
 {offsetof(gclient_t,sess.tceSessionValues[2]),0xdf4},
 {offsetof(gclient_t,sess.tceSessionValues[3]),0xdf8},
 {offsetof(gclient_t,sess.coach_team),0xbe0},
 {offsetof(gclient_t,sess.deaths),0xbec},
 {offsetof(gclient_t,sess.game_points),0xbf0},
 {offsetof(gclient_t,sess.kills),0xbf4},
 {offsetof(gclient_t,sess.referee),0xbf8},
 {offsetof(gclient_t,sess.spec_invite),0xc00},
 {offsetof(gclient_t,sess.spec_team),0xc04},
 {offsetof(gclient_t,sess.suicides),0xc08},
 {offsetof(gclient_t,sess.team_kills),0xc10},
 {offsetof(gclient_t,sess.muted),0xb64},
 {offsetof(gclient_t,sess.ignoreClients[0]),0xb5c},
 {offsetof(gclient_t,sess.ignoreClients[1]),0xb60},
 {offsetof(gclient_t,sess.spawnObjectiveIndex),0xb4c},
 {offsetof(gclient_t,sess.rounds),0xbfc},
 {offsetof(gclient_t,sess.rank),0xbc0},
 {offsetof(gclient_t,pers.enterTime),0x624},
 {offsetof(gclient_t,pers.mvReferenceList),0xaec},
 {offsetof(gclient_t,ps.clientNum),0xa0},
 {offsetof(gclient_t,pers.localClient),0x5e8},
 {offsetof(gclient_t,sess.startxptotal),0xba0},
 {offsetof(gclient_t,sess.skillpoints[0]),0xb68},
 {offsetof(gclient_t,sess.skillpoints[1]),0xb6c},
 {offsetof(gclient_t,sess.skillpoints[2]),0xb70},
 {offsetof(gclient_t,sess.skillpoints[3]),0xb74},
 {offsetof(gclient_t,sess.skillpoints[4]),0xb78},
 {offsetof(gclient_t,sess.skillpoints[5]),0xb7c},
 {offsetof(gclient_t,sess.skillpoints[6]),0xb80},
 {offsetof(gclient_t,sess.startskillpoints[0]),0xb84},
 {offsetof(gclient_t,sess.startskillpoints[1]),0xb88},
 {offsetof(gclient_t,sess.startskillpoints[2]),0xb8c},
 {offsetof(gclient_t,sess.startskillpoints[3]),0xb90},
 {offsetof(gclient_t,sess.startskillpoints[4]),0xb94},
 {offsetof(gclient_t,sess.startskillpoints[5]),0xb98},
 {offsetof(gclient_t,sess.startskillpoints[6]),0xb9c},
 {offsetof(gclient_t,sess.skill[0]),0xba4},
 {offsetof(gclient_t,sess.skill[1]),0xba8},
 {offsetof(gclient_t,sess.skill[2]),0xbac},
 {offsetof(gclient_t,sess.skill[3]),0xbb0},
 {offsetof(gclient_t,sess.skill[4]),0xbb4},
 {offsetof(gclient_t,sess.skill[5]),0xbb8},
 {offsetof(gclient_t,sess.skill[6]),0xbbc},
 {offsetof(gclient_t,sess.medals[0]),0xbc4},
 {offsetof(gclient_t,sess.medals[1]),0xbc8},
 {offsetof(gclient_t,sess.medals[2]),0xbcc},
 {offsetof(gclient_t,sess.medals[3]),0xbd0},
 {offsetof(gclient_t,sess.medals[4]),0xbd4},
 {offsetof(gclient_t,sess.medals[5]),0xbd8},
 {offsetof(gclient_t,sess.medals[6]),0xbdc},
};
static gclient_t c;static unsigned char rc[0x16b0];
static void seed(int n){int i;memset(&c,0,sizeof(c));memset(rc,0,sizeof(rc));for(i=0;i<sizeof(fields)/sizeof(fields[0]);++i){int v=i*3+n;memcpy((char *)&c+fields[i].source,&v,4);memcpy(rc+fields[i].original,&v,4);}for(i=0;i<7;++i)c.sess.skillpoints[i]=*(float *)(rc+0xb68+i*4)=i+.125f;}
static int same(void){int i;for(i=0;i<sizeof(fields)/sizeof(fields[0]);++i)if(memcmp((char *)&c+fields[i].source,rc+fields[i].original,4)){printf("field mismatch original %x\n",fields[i].original);return 0;}return 1;}
int main(int argc,char **argv){HMODULE h;unsigned char *b;int r,w,reset,n,t,state,round,alt,sw,cmp,newc,count=0;char expected[8192];
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;level.clients=&c;*(void **)(b+0x7feb80)=rc;
 patch(b+0x75da0,list);patch(G_smvGenerateClientList,list);patch(b+0x658d0,del);patch(G_deleteStats,del);patch(b+0x656f0,stats);patch(G_createStats,stats);
 patch(b+0x8ac90,set);patch(trap_Cvar_Set,set);patch(b+0x8acd0,get);patch(trap_Cvar_VariableStringBuffer,get);patch(b+0x5ed80,print);patch(G_Printf,print);
 patch(b+0x65960,parse);patch(G_parseStats,parse);patch(b+0x84f90,rank);patch(G_CalcRank,rank);patch(b+0x84f20,swap);patch(G_ClientSwap,swap);
 for(n=0;n<4;++n)for(r=0;r<2;++r)for(w=0;w<2;++w)for(reset=0;reset<2;++reset){
 seed(n);mv=0x81230000+n;level.warmupTime=*(int *)(b+0x7feb90)=w;level.fResetStats=*(int *)(b+0x221baf8)=reset;
 events[0]=0;((void(__cdecl *)(void *,int))(b+0x84c00))(rc,r);strcpy(expected,events);events[0]=0;G_WriteClientSessionData(&c,r);
 if(strcmp(expected,events)||!same()){printf("writer mismatch\n%s\n%s\n",expected,events);return 1;}++count;
 }
 for(t=0;t<8;++t)for(state=0;state<3;++state)for(round=0;round<3;++round)for(alt=0;alt<2;++alt)for(sw=0;sw<2;++sw)for(cmp=0;cmp<2;++cmp)for(newc=0;newc<2;++newc)for(n=0;n<2;++n){
 seed(5);mv=n;g_gametype.integer=*(int *)(b+0x6d8d4c)=t;g_gamestate.integer=*(int *)(b+0x784f6c)=state;g_currentRound.integer=*(int *)(b+0x2223cec)=round;g_altStopwatchMode.integer=*(int *)(b+0x6d80ec)=alt;g_swapteams.integer=*(int *)(b+0x2223e0c)=sw;level.currentCampaign=*(int *)(b+0x221a510)=0;g_campaigns[0].current=*(int *)(b+0x6da2e4)=cmp;level.newCampaign=*(int *)(b+0x221a514)=newc;
 events[0]=0;((void(__cdecl *)(void *))(b+0x85000))(rc);strcpy(expected,events);events[0]=0;G_ReadSessionData(&c);
 if(strcmp(expected,events)||!same()){printf("reader mismatch gt%d state%d round%d alt%d swap%d campaign%d new%d stats%d\n%s\n%s\n",t,state,round,alt,sw,cmp,newc,n,expected,events);return 1;}++count;
 }
 for(n=0;n<4;++n){seed(n);c.pers.localClient=*(int *)(rc+0x5e8)=n&1;level.time=*(int *)(b+0x7febe0)=12345;events[0]=0;((void(__cdecl *)(void *,char *))(b+0x85380))(rc,"");strcpy(expected,events);events[0]=0;G_InitSessionData(&c,"");if(strcmp(expected,events)||!same()){printf("init mismatch\n%s\n%s\n",expected,events);return 1;}++count;}
 printf("%d whole session writer/reader/init original comparisons passed\n",count);return 0;
}
