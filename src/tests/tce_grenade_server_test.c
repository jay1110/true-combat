#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
static unsigned char *base,rawEnt[0x57c],rawClient[0x1700],rawBolt[0x57c];
static gclient_t client;static gentity_t bolt,shake;
static int eventType,eventParm,links,radiusCalls,shakeCalls,cases,weaponDamage;
extern vec3_t forward,muzzleEffect;
extern gentity_t *weapon_grenadelauncher_fire(gentity_t *,int);
static int throwing,traceMode,traceCount;static vec3_t thrownStart,thrownVelocity,traceStarts[2];
static void patch(void *at,void *fn){DWORD o;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);}
static void require(int yes,const char *what){if(!yes){fprintf(stderr,"grenade case%d %s\n",cases,what);exit(1);}}
static void *spawnRaw(void){memset(rawBolt,0,sizeof(rawBolt));return rawBolt;}
static gentity_t *spawn(void){memset(&bolt,0,sizeof(bolt));return &bolt;}
static int damage(int weapon){return weaponDamage;}
static void link(void *ent){++links;}
static void eventRaw(void *ent,int event,int parm){eventType=event;eventParm=parm;}
static void eventSource(void *ent,int event,int parm){eventType=event==EV_MISSILE_MISS?60:event==EV_MISSILE_MISS_SMALL?89:event==EV_MISSILE_MISS_LARGE?90:event;eventParm=parm;}
static void setRaw(unsigned char *ent,const float *p){memcpy(ent+0x168,p,12);memcpy(ent+0x18,p,12);*(int *)(ent+0xc)=0;}
static void setSource(gentity_t *ent,const float *p){VectorCopy(p,ent->r.currentOrigin);VectorCopy(p,ent->s.pos.trBase);ent->s.pos.trType=TR_STATIONARY;}
static void trace(trace_t *t,const float *a,const float *b,const float *c,const float *d,int pass,int mask){memset(t,0,sizeof(*t));t->fraction=1;require(mask==MASK_MISSILESHOT,"radius trace mask");if(throwing){require(traceCount<2,"trace count");VectorCopy(a,traceStarts[traceCount]);++traceCount;t->startsolid=traceMode==2;t->fraction=traceMode?.5f:1;VectorSet(t->endpos,31.25f,-22.75f,48.25f);}}
static qboolean radius(const float *o,void *i,void *a,float dmg,float rad,void *ignore,int mod){++radiusCalls;return qfalse;}
static void *tempRaw(const float *o,int event){++shakeCalls;memset(rawEnt,0,sizeof(rawEnt));return rawEnt;}
static gentity_t *tempSource(const float *o,int event){++shakeCalls;memset(&shake,0,sizeof(shake));return &shake;}
static void *fireRaw(void *self,const float *start,const float *dir,int weapon){VectorCopy(start,thrownStart);VectorCopy(dir,thrownVelocity);memset(rawBolt,0,sizeof(rawBolt));*(void **)(rawBolt+0x2a4)=base+0x6aab0;return rawBolt;}
static gentity_t *fireSource(gentity_t *self,vec3_t start,vec3_t dir,int weapon){VectorCopy(start,thrownStart);VectorCopy(dir,thrownVelocity);memset(&bolt,0,sizeof(bolt));bolt.think=G_ExplodeMissile;return &bolt;}
#define RI(a) (*(int *)(base+(a)-0x20000000))
#define B(o) (*(int *)(rawBolt+(o)))
int main(int argc,char **argv){HMODULE h;int w,large,flags,fuse,accuracy,amount;int weapons[]={4,9,30};vec3_t start={12.25f,-7.75f,64.5f},dir={111.3f,-99.7f,130.8f};
 void *(__cdecl *fire)(void *,const float *,const float *,int);void(__cdecl *explode)(void *);
 require(argc==2,"args");h=LoadLibraryA(argv[1]);require(h!=NULL,"load");base=(unsigned char *)h;fire=(void *)(base+0x6cc30);explode=(void *)(base+0x6aab0);
 patch(base+0x94e30,spawnRaw);patch(G_Spawn,spawn);patch(base+0x9cf40,damage);patch(G_GetWeaponDamage,damage);
 patch(base+0x8afa0,link);patch(trap_LinkEntity,link);patch(base+0x950e0,eventRaw);patch(G_AddEvent,eventSource);
 patch(base+0x951d0,setRaw);patch(G_SetOrigin,setSource);patch(base+0x8ae80,trace);patch(trap_Trace,trace);
 patch(base+0x5b750,radius);patch(G_RadiusDamage,radius);patch(base+0x94fa0,tempRaw);patch(G_TempEntity,tempSource);
 for(w=0;w<3;++w)for(large=0;large<2;++large)for(flags=0;flags<2;++flags)for(fuse=0;fuse<3;++fuse)for(accuracy=0;accuracy<4;++accuracy)for(amount=0;amount<2;++amount){
  int expEvent,expParm,expLinks,expRadius,expShake,k;gentity_t *self=&g_entities[0];
  memset(rawEnt,0,sizeof(rawEnt));memset(rawClient,0,sizeof(rawClient));memset(self,0,sizeof(*self));memset(&client,0,sizeof(client));
  *(void **)(rawEnt+0x190)=rawClient;self->client=&client;self->s.number=*(int *)rawEnt=3;
  self->s.groundEntityNum=*(int *)(rawEnt+0x94)=ENTITYNUM_WORLD;
  client.sess.sessionTeam=*(int *)(rawClient+0xb30)=2;
  client.ps.grenadeTimeLeft=*(int *)(rawClient+0x34)=fuse?fuse*333:0;
  client.ps.stats[STAT_TCE_WEAPON_FLAGS]=*(int *)(rawClient+0xf0)=flags?4:0;
  level.time=RI(0x207febe0)=50000;g_newbbox.integer=RI(0x207e208c)=large;weaponDamage=amount?250:0;
  fire(rawEnt,start,dir,weapons[w]);fire_grenade(self,start,dir,weapons[w]);
  require(B(0x29c)==bolt.nextthink&&B(0xe8)==bolt.s.weapon&&B(0xc)==bolt.s.pos.trType,"fuse weapon trajectory");
  require(B(8)==bolt.s.eFlags&&B(0x2d0)==bolt.damage&&B(0x2d4)==bolt.splashDamage&&B(0x2d8)==bolt.splashRadius,"flags damage radius");
  require(B(0x2dc)==bolt.methodOfDeath&&B(0x2e0)==bolt.splashMethodOfDeath&&!strcmp(*(char **)(rawBolt+0x1a4),bolt.classname),"MOD classname");
  for(k=0;k<3;++k)require(*(float *)(rawBolt+0x24+k*4)==bolt.s.pos.trDelta[k],"velocity");
  *(float *)(rawBolt+0x420)=bolt.accuracy=accuracy;
  level.time=RI(0x207febe0)=51000;
  eventType=eventParm=links=radiusCalls=shakeCalls=0;explode(rawBolt);
  expEvent=eventType;expParm=eventParm;expLinks=links;expRadius=radiusCalls;expShake=shakeCalls;
  eventType=eventParm=links=radiusCalls=shakeCalls=0;G_ExplodeMissile(&bolt);
  require(expEvent==eventType&&expParm==eventParm&&expLinks==links&&expRadius==radiusCalls&&expShake==shakeCalls,"explode callbacks");
  require(B(4)==bolt.s.eType&&B(0x1c4)==bolt.freeAfterEvent&&B(0x29c)==bolt.nextthink&&B(0x8c)==bolt.s.otherEntityNum&&B(0x54)==bolt.s.time,"smoke lifecycle");
  for(k=0;k<3;++k)require(fabs(*(float *)(rawBolt+0x168+k*4)-bolt.r.currentOrigin[k])<.001,"explosion origin");
  ++cases;
 }
 throwing=1;patch(base+0x6cc30,fireRaw);patch(fire_grenade,fireSource);
 for(w=0;w<7;++w)for(large=0;large<2;++large)for(flags=0;flags<2;++flags)for(fuse=0;fuse<4;++fuse)for(traceMode=0;traceMode<3;++traceMode){
  int kinds[]={4,9,30,15,26,27,22},j,k,count;vec3_t expectedStart,expectedVelocity,expectedTrace[2];gentity_t *self=&g_entities[0];
  memset(rawEnt,0,sizeof(rawEnt));memset(rawClient,0,sizeof(rawClient));memset(self,0,sizeof(*self));memset(&client,0,sizeof(client));
  self->client=&client;*(void **)(rawEnt+0x190)=rawClient;self->s.number=*(int *)rawEnt=3;
  for(k=0;k<3;++k){self->s.pos.trBase[k]=*(float *)(rawEnt+0x18+k*4)=start[k];self->r.currentOrigin[k]=*(float *)(rawEnt+0x168+k*4)=start[k]+10;}
  client.ps.viewheight=*(int *)(rawClient+0xbc)=32;client.sess.sessionTeam=*(int *)(rawClient+0xb30)=2;
  client.ps.stats[STAT_TCE_WEAPON_FLAGS]=*(int *)(rawClient+0xf0)=flags?4:0;
  level.time=RI(0x207febe0)=50000;g_newbbox.integer=RI(0x207e208c)=large;
  VectorSet(dir,.6f,.8f,(fuse-2)*.4f);memcpy(base+0x66bba0,dir,12);memcpy(base+0x66bbc0,start,12);
  traceCount=0;((void(__cdecl *)(void *,int))(base+0x9ea00))(rawEnt,kinds[w]);
  count=traceCount;VectorCopy(thrownStart,expectedStart);VectorCopy(thrownVelocity,expectedVelocity);memcpy(expectedTrace,traceStarts,sizeof(expectedTrace));
  VectorCopy(dir,forward);VectorCopy(start,muzzleEffect);traceCount=0;weapon_grenadelauncher_fire(self,kinds[w]);
  require(count==traceCount,"throw trace count");

  for(k=0;k<3;++k){require(fabs(expectedStart[k]-thrownStart[k])<.0001,"throw start");require(fabs(expectedVelocity[k]-thrownVelocity[k])<.0002,"throw speed");for(j=0;j<count;++j){if(fabs(expectedTrace[j][k]-traceStarts[j][k])>=.0001)fprintf(stderr,"trace %d axis %d original %.9g source %.9g\n",j,k,expectedTrace[j][k],traceStarts[j][k]);require(fabs(expectedTrace[j][k]-traceStarts[j][k])<.0001,"throw trace start");}}
  require((*(void **)(rawBolt+0x2a4)==base+0x6aab0)==(bolt.think==G_ExplodeMissile),"throw think override");
  require(B(0x29c)==bolt.nextthink&&B(0x2e4)==bolt.count&&B(0x108)==bolt.s.teamNum&&B(0x90)==bolt.s.otherEntityNum2,"throw post state");++cases;
 }
 printf("TC grenade producer/lifecycle/throw PASS %d original comparisons\n",cases);return 0;
}
