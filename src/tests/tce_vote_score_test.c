#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
static unsigned char *base,raw[64][0x16b0];
static gclient_t clients[64];
static char events[8192];
static int mask,nonzero,mv;
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void event(const char *fmt,...){va_list ap;va_start(ap,fmt);vsprintf(events+strlen(events),fmt,ap);va_end(ap);}
static void set(const char *key,const char *value){event("set:%s=%s;",key,value);}
static int get(const char *key){int i;event("get:%s;",key);for(i=0;i<numVotesAvailable;++i)if(!strcmp(key,voteToggles[i].pszCvar))return mask&(1<<i)?0:nonzero;puts("Unknown vote Cvar");exit(1);}
static void server(int id,const char *s){event("server:%d:%s\n",id,s);}
static int locate(void *ent,int id,int remove){int index=((unsigned char *)ent>=base+0x226e2a0&&(unsigned char *)ent<base+0x226e2a0+64*0x57c)?((unsigned char *)ent-base-0x226e2a0)/0x57c:(gentity_t *)ent-g_entities;event("mv:%d:%d:%d;",index,id,remove);return mv&&(id%3==0);}
static void seed(int n,int v,int gt,int receiver){int i,j;level.clients=clients;*(void **)(base+0x7feb80)=raw;level.numConnectedClients=*(int *)(base+0x7fec0c)=n;g_gametype.integer=*(int *)(base+0x6d8d4c)=gt;level.time=*(int *)(base+0x7febe0)=987654;level.intermissiontime=*(int *)(base+0x7ffc60)=v&1;
 level.teamScores[1]=*(int *)(base+0x7febf4)=v*13;level.teamScores[2]=*(int *)(base+0x7febf8)=-v*17;
 for(i=0;i<64;++i){gclient_t *c=clients+i;unsigned char *r=raw[i];memset(c,0,sizeof(*c));memset(r,0,0x16b0);g_entities[i].client=c;*(void **)(base+0x226e430+i*0x57c)=r;
  level.sortedClients[i]=*(int *)(base+0x7fec18+i*4)=63-i;
  c->pers.connected=*(int *)(r+0x5ac)=(i+v)%3;c->ps.persistant[PERS_TEAM]=*(int *)(r+0x11c)=(i+v)%4;
  c->ps.persistant[PERS_SCORE]=*(int *)(r+0x110)=i*7-v*13;c->ps.persistant[PERS_RESPAWNS_LEFT]=*(int *)(r+0x12c)=(i+v)%5-2;
  c->ps.stats[STAT_PLAYER_CLASS]=*(int *)(r+0xe4)=(i+v)%7;c->ps.pm_flags=*(int *)(r+0xc)=i%3?0:PMF_LIMBO;
  c->ps.ping=*(int *)(r+0x468)=v==3?1001:i*23-1;c->pers.enterTime=*(int *)(r+0x624)=i*62345;
  g_entities[i].r.svFlags=*(int *)(base+0x226e3c8+i*0x57c)=v==2&&i%3==0?SVF_POW:0;
  g_entities[i].health=*(int *)(base+0x226e568+i*0x57c)=i%3?100:0;g_entities[i].s.powerups=*(int *)(base+0x226e384+i*0x57c)=i%4;
  c->sess.damage_given=*(int *)(r+0xbe4)=i*77;c->sess.damage_received=*(int *)(r+0xbe8)=i*21;c->sess.team_damage=*(int *)(r+0xc0c)=i*3;
  c->sess.kills=*(int *)(r+0xbf4)=i+2;c->sess.deaths=*(int *)(r+0xbec)=i+3;c->sess.suicides=*(int *)(r+0xc08)=i+4;c->sess.team_kills=*(int *)(r+0xc10)=i+5;
  for(j=0;j<7;++j)c->sess.skillpoints[j]=*(float *)(r+0xb68+j*4)=(i+j)%7-1.75f;
 }
}
int main(int argc,char **argv){HMODULE h;int v,n,g,r,k,cases=0;char expected[8192];const int sizes[]={0,1,2,16,31,32,33,63,64,65};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)h;
 patch(base+0x8acb0,get);patch(trap_Cvar_VariableIntegerValue,get);patch(base+0x8ac90,set);patch(trap_Cvar_Set,set);
 /* Exhaust all 17 disabled-vote combinations and the no-change path. */
 for(mask=0;mask<(1<<17);++mask)for(k=0;k<2;++k){nonzero=mask&1?-1:2;voteFlags.integer=*(int *)(base+0x784d2c)=k?mask:~mask;events[0]=0;((void(__cdecl *)(void))(base+0x96630))();strcpy(expected,events);events[0]=0;G_voteFlags();if(strcmp(expected,events)){printf("vote mismatch %d %d\n%s\n%s\n",mask,k,expected,events);return 1;}++cases;}
 patch(base+0x8ad60,server);patch(trap_SendServerCommand,server);patch(base+0x75cf0,locate);patch(G_smvLocateEntityInMVList,locate);
 for(n=0;n<10;++n)for(v=0;v<4;++v)for(g=0;g<8;++g)for(r=0;r<3;++r)for(mv=0;mv<2;++mv){int receiver=r*17;seed(sizes[n],v,g,receiver);events[0]=0;((void(__cdecl *)(void *))(base+0x4ef80))(base+0x226e2a0+receiver*0x57c);strcpy(expected,events);events[0]=0;G_SendScore(g_entities+receiver);if(strcmp(expected,events)){printf("score mismatch n%d v%d gt%d receiver%d mv%d\n%s\n%s\n",sizes[n],v,g,receiver,mv,expected,events);return 1;}++cases;}
 printf("%d whole original vote/score comparisons passed\n",cases);return 0;
}
