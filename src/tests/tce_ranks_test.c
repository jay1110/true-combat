#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
extern int QDECL SortRanks(const void *,const void *);
extern void etpro_PlayerInfo(void);
extern void CheckExitRules(void);
static gclient_t clients[64];
static unsigned char raw[64][0x16b0];
static char events[4096];
static void patch(void *a,void *f){DWORD o,t;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void config(int i,const char *s){strcat(events,va("config:%d:%s;",i,s));}
static void info(void){strcat(events,"playerinfo;");}
static void exitRules(void){strcat(events,"exit;");}
static void scores(void){strcat(events,"scores;");}
static const struct {size_t own;int rva,bytes;} levelFields[]={
 {offsetof(level_locals_t,numConnectedClients),0x7fec0c,4},
 {offsetof(level_locals_t,numNonSpectatorClients),0x7fec10,4},
 {offsetof(level_locals_t,numPlayingClients),0x7fec14,4},
 {offsetof(level_locals_t,sortedClients),0x7fec18,256},
 {offsetof(level_locals_t,follow1),0x7fed18,4},{offsetof(level_locals_t,follow2),0x7fed1c,4},
 {offsetof(level_locals_t,voteInfo.numVotingClients),0x7ff130,4},
 {offsetof(level_locals_t,voteInfo.numVotingTeamClients),0x7ff134,8},
 {offsetof(level_locals_t,numTeamClients),0x7ff240,8},
 {offsetof(level_locals_t,numFinalDead),0x221a418,8}
};
static void seed(unsigned char *b,int n,int variant){int i,j;level.clients=clients;*(void **)(b+0x7feb80)=raw;level.maxclients=*(int *)(b+0x7febd8)=n;
 for(i=0;i<64;++i){gclient_t *c=clients+i;unsigned char *r=raw[i];memset(c,0,sizeof(*c));memset(r,0,0x16b0);
 c->pers.connected=*(int *)(r+0x5ac)=variant==0?2:(i+variant)%3;
 c->sess.sessionTeam=*(int *)(r+0xb30)=variant<2?1+i%2:(i+variant)%4;
 c->sess.spectatorClient=*(int *)(r+0xb3c)=variant==4?-1:0;
 c->sess.spectatorTime=*(int *)(r+0xb34)=i*31;
 c->ps.persistant[PERS_SCORE]=*(int *)(r+0x110)=(i%4)*17;
 c->ps.persistant[PERS_RANK]=*(int *)(r+0x118)=99;
 c->ps.persistant[PERS_RESPAWNS_LEFT]=*(int *)(r+0x12c)=i%2;
 c->ps.pm_flags=*(int *)(r+0xc)=i%3==0?PMF_LIMBO:0;
 c->ps.stats[STAT_PLAYER_CLASS]=*(int *)(r+0xe4)=i%7;
 c->sess.damage_given=*(int *)(r+0xbe4)=i*13;c->sess.damage_received=*(int *)(r+0xbe8)=i*2;c->sess.team_damage=*(int *)(r+0xc0c)=i;
 for(j=0;j<7;++j)c->sess.skillpoints[j]=*(float *)(r+0xb68+j*4)=((i+j)%5)-1.5f;
 g_entities[i].health=*(int *)(b+0x226e568+i*0x57c)=i%3==0?0:100;
 g_entities[i].r.svFlags=*(int *)(b+0x226e3c8+i*0x57c)=i%4==0?SVF_BOT:0;
 }
 for(i=0;i<sizeof(levelFields)/sizeof(levelFields[0]);++i){memset((char *)&level+levelFields[i].own,0x35,levelFields[i].bytes);memset(b+levelFields[i].rva,0x35,levelFields[i].bytes);}
}
static int same(unsigned char *b){int i,j;for(i=0;i<sizeof(levelFields)/sizeof(levelFields[0]);++i)if(memcmp((char *)&level+levelFields[i].own,b+levelFields[i].rva,levelFields[i].bytes)){printf("level field %x\n",levelFields[i].rva);return 0;}
 for(i=0;i<64;++i){if(clients[i].ps.persistant[PERS_RANK]!=*(int *)(raw[i]+0x118))return 0;for(j=0;j<7;++j)if(clients[i].sess.skillpoints[j]!=*(float *)(raw[i]+0xb68+j*4)){printf("rating client%d role%d %.2f %.2f\n",i,j,clients[i].sess.skillpoints[j],*(float *)(raw[i]+0xb68+j*4));return 0;}}return 1;}
int main(int argc,char **argv){HMODULE h;unsigned char *b;int n,v,gt,state,round,t,aa,i,j,a,c,cases=0;char expected[4096];const int times[]={-1,0,999,1000,1001,300000,600001};const int sizes[]={0,1,2,3,6,9,13,24,64};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 for(gt=0;gt<8;++gt)for(v=0;v<5;++v){seed(b,64,v);g_gametype.integer=*(int *)(b+0x6d8d4c)=gt;
 for(i=0;i<64;++i)for(j=0;j<64;++j){a=((int(__cdecl *)(void *,void *))(b+0x618e0))(&i,&j);c=SortRanks(&i,&j);if(a!=c){printf("sort mismatch gt%d v%d i%d j%d %d %d\n",gt,v,i,j,a,c);return 1;}++cases;}}
 patch(b+0x8adc0,config);patch(trap_SetConfigstring,config);patch(b+0x61ad0,info);patch(etpro_PlayerInfo,info);patch(b+0x62f10,exitRules);patch(CheckExitRules,exitRules);patch(b+0x620e0,scores);patch(SendScoreboardMessageToAllClients,scores);
 for(n=0;n<9;++n)for(v=0;v<4;++v)for(gt=0;gt<8;++gt)for(state=0;state<4;++state)for(round=0;round<3;++round)for(t=0;t<7;++t)for(aa=0;aa<2;++aa){
 seed(b,sizes[n],v);g_gametype.integer=*(int *)(b+0x6d8d4c)=gt;g_gamestate.integer=*(int *)(b+0x784f6c)=state;g_currentRound.integer=*(int *)(b+0x2223cec)=round*3;
 g_aabasetime.integer=*(int *)(b+0x23ce14c)=aa?5:0;level.startTime=*(int *)(b+0x7febec)=500;level.time=*(int *)(b+0x7febe0)=500+times[t];level.timeCurrent=*(int *)(b+0x221bc08)=300+times[t];
 level.teamScores[1]=*(int *)(b+0x7febf4)=v;level.teamScores[2]=*(int *)(b+0x7febf8)=2;level.firstbloodTeam=*(int *)(b+0x221a500)=1;g_axiswins.integer=*(int *)(b+0x23ce26c)=3;g_alliedwins.integer=*(int *)(b+0x76bcec)=4;
 events[0]=0;((void(__cdecl *)(void))(b+0x61b60))();strcpy(expected,events);events[0]=0;CalculateRanks();
 if(!same(b)||strcmp(events,expected)){printf("CalculateRanks mismatch n%d v%d gt%d state%d round%d time%d aa%d\n%s\n%s\n",sizes[n],v,gt,state,round*3,t,aa,expected,events);return 1;}++cases;
 }
 printf("%d complete original SortRanks/CalculateRanks comparisons passed\n",cases);return 0;
}
