#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <limits.h>

static int rankCalls;
static void ranks(void) { ++rankCalls; }
static void patch(void *where,void *to) {
 DWORD old;unsigned char *p=where;
 VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;
 *(int *)(p+1)=(unsigned char *)to-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);
}
static unsigned char rawEnt[0x57c],rawClient[0x1600];
static gentity_t actor;static gclient_t client;
int main(int argc,char **argv) {
 HMODULE h;unsigned char *base;int mod,i,state,type,flags,score,expected,calls,team,kind,owner,count=0;
 int scores[]={-500,-1,0,1,500};int types[]={-1,0,1,2,3,4,14,33,44,100};
 int (__cdecl *explosive)(int),(__cdecl *weaponclass)(int),(__cdecl *getteam)(void *);
 void (__cdecl *addscore)(void *,int);
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)h;
 explosive=(void *)(base+0x97c80);weaponclass=(void *)(base+0x97cf0);
 getteam=(void *)(base+0x96220);addscore=(void *)(base+0x571d0);
 patch(base+0x61b60,ranks);patch(CalculateRanks,ranks);
 for(i=0;i<1027;++i){mod=i<1025?i-512:i==1025?INT_MIN:INT_MAX;
  if(explosive(mod)!=G_WeaponIsExplosive(mod)||weaponclass(mod)!=G_GetWeaponClassForMOD(mod)){
   printf("MOD classifier mismatch %d\n",mod);return 1;}
  count+=2;
 }
 for(state=-1;state<5;++state)for(type=-1;type<10;++type)for(flags=0;flags<4;++flags)
 for(score=0;score<5;++score)for(kind=0;kind<3;++kind){
  memset(rawEnt,0,sizeof(rawEnt));memset(rawClient,0,sizeof(rawClient));memset(&actor,0,sizeof(actor));memset(&client,0,sizeof(client));
  *(void **)(rawEnt+0x190)=kind==1?NULL:rawClient;actor.client=kind==1?NULL:&client;
  *(int *)(base+0x784f6c)=g_gamestate.integer=state;*(int *)(base+0x6d8d4c)=g_gametype.integer=type;
  *(int *)(rawClient+0xf4)=client.ps.stats[STAT_TCE_FLAGS]=flags==0?0:flags==1?0x400:flags==2?0x401:0x200;
  *(int *)(rawClient+0xbf0)=client.sess.game_points=25;
  rankCalls=0;addscore(kind==0?NULL:rawEnt,scores[score]);expected=*(int *)(rawClient+0xbf0);calls=rankCalls;
  rankCalls=0;AddScore(kind==0?NULL:&actor,scores[score]);
  if(expected!=client.sess.game_points||calls!=rankCalls){printf("AddScore mismatch state%d type%d flags%d score%d kind%d %d/%d calls%d/%d\n",state,type,flags,scores[score],kind,expected,client.sess.game_points,calls,rankCalls);return 1;}
  ++count;
 }
 for(type=0;type<10;++type)for(mod=-1;mod<70;++mod)for(team=-5;team<8;++team)for(kind=0;kind<3;++kind){
  memset(rawEnt,0,sizeof(rawEnt));memset(rawClient,0,sizeof(rawClient));memset(&actor,0,sizeof(actor));memset(&client,0,sizeof(client));
  *(void **)(rawEnt+0x190)=kind==0?NULL:rawClient;actor.client=kind==0?NULL:&client;
  *(int *)(rawEnt+4)=types[type];actor.s.eType=types[type];
  /* ET_MG42_BARREL is mapped to its SDK enum through the entity type boundary. */
  if(types[type]==44){actor.s.eType=ET_MG42_BARREL;owner=7;*(int *)(rawEnt+0x180)=actor.r.ownerNum=owner;
   *(int *)(base+0x226e2a0+owner*0x57c+4)=33;*(int *)(base+0x226e2a0+owner*0x57c+0x108)=team;
   memset(&g_entities[owner],0,sizeof(g_entities[owner]));g_entities[owner].s.eType=ET_CONSTRUCTIBLE;g_entities[owner].s.teamNum=team;}
  if(types[type]==33)actor.s.eType=ET_CONSTRUCTIBLE;
  *(int *)(rawEnt+0x2dc)=actor.methodOfDeath=mod;*(int *)(rawEnt+0x108)=actor.s.teamNum=team;
  *(int *)(rawClient+0xb30)=client.sess.sessionTeam=team;
  *(char **)(rawEnt+0x1a4)=actor.classname=kind==2?"SCRIPT_MOVER":"other";
  expected=getteam(rawEnt);i=G_GetTeamFromEntity(&actor);
  if(expected!=i){printf("Team mismatch type%d mod%d team%d kind%d %d/%d\n",types[type],mod,team,kind,expected,i);return 1;}
  ++count;
 }
 printf("PASS %d full damage dependency comparisons (MOD classifiers/AddScore/entity team)\n",count);return 0;
}
