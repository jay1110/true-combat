#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <float.h>
#undef vsnprintf
#undef _vsnprintf
extern void BotRecordDeath(int,int),BotRecordPain(int,int,int);
extern void G_Voice(gentity_t *,gentity_t *,int,const char *,qboolean);
static unsigned char *base,*raw,*part,rc[2][0x1700];
static gclient_t clients[2];static int traceCalls,callbacks[10],hitMask;
static char debugText[4096];
static gentity_t debugEvents[8];static int debugEventCount;
static gentity_t *debugEvent(vec3_t origin,int event){gentity_t *e;if(debugEventCount>=8||event!=EV_RAILTRAIL)exit(10);e=&debugEvents[debugEventCount++];memset(e,0,sizeof(*e));VectorCopy(origin,e->s.origin);return e;}
static void debugPrint(const char *fmt,...){va_list ap;size_t len=strlen(debugText);va_start(ap,fmt);vsnprintf(debugText+len,sizeof(debugText)-len,fmt,ap);va_end(ap);}
static void patch(void *at,void *fn){DWORD o;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);}
static void *spawnOriginal(void){memset(part,0,0x57c);*(int *)part=70;return part;}
static gentity_t *spawnSource(void){memset(&g_entities[70],0,sizeof(g_entities[70]));g_entities[70].s.number=70;return &g_entities[70];}
static void nop(void *p,...){(void)p;}
static void freePart(void *p){++callbacks[0];}
static void linkPart(void *p){++callbacks[1];}
static void script(void *p,...){++callbacks[2];}
static void pain(void *p,void *a,int amount,void *point){++callbacks[3];callbacks[4]+=amount;}
static void died(void *p,void *i,void *a,int amount,int mod){++callbacks[5];callbacks[4]+=amount;}
static void stats(void *p,void *a,int amount,int mod){++callbacks[6];callbacks[4]+=amount;}
static void limboStub(void *p,int q){++callbacks[7];}
static void voice(void *p,void *target,int channel,const char *id,int only){++callbacks[8];callbacks[9]+=(*(int *)p+1)*(!strcmp(id,"Contact")?1:7);}
static int teamOriginal(unsigned char *a,unsigned char *b){unsigned char *x=*(void **)(a+0x190),*y=*(void **)(b+0x190);return x&&y&&*(int *)(x+0xb30)==*(int *)(y+0xb30);}
static qboolean teamSource(gentity_t *a,gentity_t *b){return a->client&&b->client&&a->client->sess.sessionTeam==b->client->sess.sessionTeam;}
static void trace(trace_t *tr,const float *start,const float *mins,const float *maxs,const float *end,int pass,int mask){
 if(mask!=0x6000081||mins||maxs||pass!=0){printf("Unexpected body trace contract %x/%d\n",mask,pass);exit(8);}
 int n=traceCalls++;memset(tr,0,sizeof(*tr));tr->fraction=hitMask&(1<<n)?0.5f:1;tr->entityNum=tr->fraction<1?70:1022;memcpy(tr->endpos,end,12);
}
static void setup(int variant,int mod){
 int i;memset(raw,0,2*0x57c);memset(rc,0,sizeof(rc));memset(g_entities,0,2*sizeof(g_entities[0]));memset(clients,0,sizeof(clients));
 for(i=0;i<2;++i){unsigned char *e=raw+i*0x57c;gentity_t *s=&g_entities[i];
  *(int *)e=s->s.number=i;*(int *)(e+0x194)=s->inuse=1;*(int *)(e+0x2c8)=s->health=100;*(int *)(e+0x2cc)=s->takedamage=1;
  *(void **)(e+0x190)=rc[i];s->client=&clients[i];*(void **)(e+0x2b8)=pain;s->pain=(void *)pain;*(void **)(e+0x2bc)=died;s->die=(void *)died;
  *(int *)(rc[i]+0xb30)=clients[i].sess.sessionTeam=i+1;*(int *)(rc[i]+0xa4)=clients[i].ps.weapon=39;*(int *)(rc[i]+0xd0)=clients[i].ps.stats[STAT_HEALTH]=100;
  *(int *)(rc[i]+0x1334)=clients[i].tceDamageSpawnTime=-10000;
 }
 *(int *)(base+0x7febe0)=level.time=10000;*(int *)(base+0x7ffc5c)=level.intermissionQueued=0;
 *(int *)(base+0x784f6c)=g_gamestate.integer=0;*(int *)(base+0x7852cc)=g_debugDamage.integer=0;
 *(int *)(base+0x7849cc)=g_debugBullets.integer=0;*(int *)(base+0x6d8d4c)=g_gametype.integer=0;
 *(int *)(base+0x7febd8)=level.maxclients=2;
 switch(variant){
 case 1:*(int *)(raw+0x2cc)=g_entities[0].takedamage=0;break;
 case 2:*(int *)(base+0x784f6c)=g_gamestate.integer=1;break;
 case 3:*(int *)(raw+0x47c)=g_entities[0].entstate=STATE_INVISIBLE;break;
 case 4:*(int *)(raw+0x47c)=g_entities[0].entstate=STATE_UNDERCONSTRUCTION;break;
 case 5:*(int *)(rc[0]+0xe00)=clients[0].noclip=1;break;
 case 6:*(int *)(raw+0x1b0)=g_entities[0].flags=FL_GODMODE;break;
 case 7:*(int *)(rc[0]+0x1334)=clients[0].tceDamageSpawnTime=9000;break;
 case 8:*(int *)(raw+0x2c8)=g_entities[0].health=-1;*(int *)(rc[0]+0x398)=clients[0].ps.holdable[2]=110;break;
 case 9:*(void **)(raw+0x190)=NULL;g_entities[0].client=NULL;break;
 case 10:*(void **)(raw+0x2b8)=NULL;g_entities[0].pain=NULL;break;
 case 11:*(int *)(rc[0]+0x39c)=clients[0].ps.holdable[3]=95;break;
 case 12:*(int *)(rc[0]+0xb30)=clients[0].sess.sessionTeam=2;break;
 case 13:*(int *)(raw+0x304)=g_entities[0].waterlevel=3;break;
 case 14:*(int *)(base+0x7ffc5c)=level.intermissionQueued=1;break;
 case 15:*(int *)(base+0x6d8d4c)=g_gametype.integer=5;break;
 case 16:*(int *)(base+0x6d8d4c)=g_gametype.integer=7;*(int *)(rc[0]+0x1334)=clients[0].tceDamageSpawnTime=9000;break;
 case 17:*(int *)(base+0x6d8d4c)=g_gametype.integer=5;*(int *)(rc[0]+0x1334)=clients[0].tceDamageSpawnTime=9000;*(int *)(rc[0]+0xb30)=clients[0].sess.sessionTeam=2;break;
 case 18:*(int *)(base+0x6d8d4c)=g_gametype.integer=5;*(int *)(rc[0]+0xb30)=clients[0].sess.sessionTeam=2;break;
 case 26:*(int *)(base+0x7852cc)=g_debugDamage.integer=1;break;
 }
 if(variant>=19&&variant<26){
  *(void **)(raw+0x190)=NULL;g_entities[0].client=NULL;
  *(int *)(raw+4)=variant<22?4:variant<24?14:33;g_entities[0].s.eType=variant<22?ET_MOVER:variant<24?ET_EXPLOSIVE:ET_CONSTRUCTIBLE;
  *(char **)(raw+0x1a4)=g_entities[0].classname="script_mover";
  *(int *)(raw+0x2e4)=g_entities[0].count=100;*(int *)(raw+0x108)=g_entities[0].s.teamNum=1;
  if(variant==20){*(int *)(raw+0x388)=g_entities[0].isProp=1;}
  if(variant==21){*(char **)(raw+0x3b4)=g_entities[0].scriptName="fixture";*(int *)(raw+0x1a8)=g_entities[0].spawnflags=4;}
  if(variant==23){*(void **)(raw+0x218)=raw+0x57c;g_entities[0].parent=&g_entities[1];}
  if(variant==25){*(int *)(raw+0x57c+0x578)=g_entities[1].etpro_misc_1=1;}
 }
 if(variant>=26){*(int *)(base+0x7852cc)=g_debugDamage.integer=1;
  if(variant==27||variant==28){*(int *)(rc[0]+0x68)=clients[0].ps.eFlags=EF_PRONE;}
  if(variant==29||variant==30){*(int *)(rc[0]+0xc)=clients[0].ps.pm_flags=PMF_DUCKED;}
  if(variant==28||variant==30||variant==31){*(int *)(rc[0]+0xf0)=clients[0].ps.stats[STAT_TCE_WEAPON_FLAGS]=0x20;}
  if(variant>=32){*(int *)(rc[0]+0xa4)=clients[0].ps.weapon=variant==32?1:variant==33?4:6;}
 }
 traceCalls=0;memset(callbacks,0,sizeof(callbacks));debugText[0]=0;
}
static int check(void){int i,j;
 if(*(int *)(raw+0x2c8)!=g_entities[0].health){printf("health %d/%d ",*(int *)(raw+0x2c8),g_entities[0].health);return 0;}
 for(i=0;i<2;++i){int *v=clients[i].ps.holdable;
  if(*(int *)(raw+i*0x57c+0x2c8)!=g_entities[i].health||*(int *)(rc[i]+0x1318)!=clients[i].tceDamageVoiceTime){printf("health/voice client%d ",i);return 0;}
  for(j=2;j<=4;++j)if(*(int *)(rc[i]+0x390+4*j)!=v[j]){printf("region%d %d/%d ",j,*(int *)(rc[i]+0x390+4*j),v[j]);return 0;}
  if(*(int *)(rc[i]+0x131c)!=clients[i].tceLastAppliedDamage||*(int *)(rc[i]+0xe2c)!=clients[i].damage_blood||
     *(int *)(rc[i]+0xd0)!=clients[i].ps.stats[STAT_HEALTH]||*(int *)(rc[i]+0x114)!=clients[i].ps.persistant[PERS_HITS]||
     *(int *)(rc[i]+0x1030)!=clients[i].combatState||*(int *)(rc[i]+0xe48)!=clients[i].lasthurt_client||
     *(int *)(rc[i]+0xe4c)!=clients[i].lasthurt_mod){printf("client fields%d ",i);return 0;}
 }return 1;
}
int main(int argc,char **argv){
 HMODULE dll;float (__cdecl *original)(void *,void *,void *,float *,float *,int,unsigned,int);
 int variant,m,d,prot,n=0,captured[10],count,damageValues[]={-10,0,1,7,15,100,200};char expectedDebug[4096];
 int mods[]={0,4,7,8,18,19,26,62,65};vec3_t dir,point={-100,0,32};float expected,actual;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;raw=base+0x226e2a0;part=raw+70*0x57c;original=(void *)(base+0x589c0);
 patch(base+0x94e30,spawnOriginal);patch(G_Spawn,spawnSource);patch(base+0x94f30,freePart);patch(G_FreeEntity,freePart);
 patch(base+0x8afa0,linkPart);patch(trap_LinkEntity,linkPart);patch(base+0x8ae80,trace);patch(trap_Trace,trace);
 /* Keep actual source OnSameTeam: an identical stub can be linker-folded
  * to the same address and an inline hook would then jump to itself. */
 patch(base+0x8daa0,teamOriginal);patch(base+0x7d4b0,script);patch(G_Script_ScriptEvent,script);
 patch(base+0x65470,stats);patch(G_addStats,stats);patch(base+0x4b410,limboStub);patch(limbo,limboStub);
 patch(base+0x870a0,nop);patch(G_AddKillSkillPoints,nop);
 patch(base+0x51820,voice);patch(G_Voice,voice);
 patch(base+0x5ed80,debugPrint);patch(G_Printf,debugPrint);
 for(variant=0;variant<35;++variant)for(m=0;m<9;++m)for(d=0;d<7;++d)for(prot=0;prot<2;++prot)for(hitMask=0;hitMask<8;++hitMask){
  setup(variant,mods[m]);VectorSet(dir,1,0,0);expected=original(raw,raw+0x57c,raw+0x57c,dir,point,damageValues[d],prot?0x20:0,mods[m]);
  memcpy(captured,callbacks,sizeof(captured));count=traceCalls;memset(callbacks,0,sizeof(callbacks));traceCalls=0;VectorSet(dir,1,0,0);strcpy(expectedDebug,debugText);debugText[0]=0;
  actual=G_Damage(&g_entities[0],&g_entities[1],&g_entities[1],dir,point,damageValues[d],prot?0x20:0,mods[m]);
  if(strcmp(expectedDebug,debugText)){printf("Debug mismatch mod%d original[%s] source[%s]\n",mods[m],expectedDebug,debugText);return 7;}
  if(fabs(expected-actual)>0.000001f||memcmp(captured,callbacks,sizeof(captured))||count!=traceCalls||!check()){
   printf("Controller mismatch variant%d mod%d damage%d prot%d mask%d result%g/%g traces%d/%d callbacks%d/%d,%d/%d,%d/%d,%d/%d\n",variant,mods[m],damageValues[d],prot,hitMask,expected,actual,count,traceCalls,captured[0],callbacks[0],captured[2],callbacks[2],captured[4],callbacks[4],captured[7],callbacks[7]);return 1;}
  ++n;
 }
 setup(0,7);g_debugBullets.integer=3;hitMask=0;debugEventCount=0;patch(G_TempEntity,debugEvent);VectorSet(dir,1,0,0);
 actual=G_Damage(&g_entities[0],&g_entities[1],&g_entities[1],dir,point,15,0x20,7);
 if(actual!=1||debugEventCount!=8)return 11;
 for(m=0;m<8;++m){if(debugEvents[m].s.dmgFlags!=(m%2==0))return 12;
  for(d=0;d<3;++d)if(!_finite(debugEvents[m].s.origin[d])||!_finite(debugEvents[m].s.origin2[d]))return 13;}
 printf("PASS %d original G_Damage controller state/return/callback comparisons plus deterministic 8-event debug repair\n",n);return 0;
}
