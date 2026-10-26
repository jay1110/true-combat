#include "../game/g_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
extern void Bullet_Fire(gentity_t *,float,int,qboolean);
extern void Bullet_Endpos(gentity_t *,float,vec3_t *);
extern void ShotgunPattern(vec3_t,vec3_t,int,gentity_t *);
extern qboolean Bullet_Fire_Extended(gentity_t *,gentity_t *,vec3_t,vec3_t,float,int,int,int,int,int,int,int,int,int);
static unsigned char raw[0x2000],entity[0x57c],targetRaw[0x2000];
static gclient_t client,targetClient; static gentity_t actor,temp;
static int calls[32],count,damageResult,modResult;static float spreadResult;
static float endpoints[16][3];static int bulletCount;
static void hook(void *address,void *fn) {
 unsigned char *p=address;DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);
 p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
 FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
static void begin(void *e,...){calls[count++]=1;}
static void end(void *e){calls[count++]=4;}
static void endpoint(void *e,float spread,float *v){calls[count++]=2;v[0]=123;v[1]=456;v[2]=789;spreadResult=spread;}
static int origExtended(void *s,void *a,float *start,float *dest,float spread,int damage,int penetration,
 int d,int e,int f,int g,int h,int seed,int pellet) {calls[count++]=3;damageResult=damage;
 if(bulletCount<16)memcpy(endpoints[bulletCount++],dest,sizeof(vec3_t));return 0;}
static void trace(trace_t *tr,const float *start,const float *mins,const float *maxs,const float *dest,int pass,int mask) {
 memset(tr,0,sizeof(*tr));tr->fraction=.5f;tr->entityNum=1;tr->endpos[0]=500;tr->plane.normal[0]=-1;
}
static void gameTrace(void *e,trace_t *tr,const float *s,const float *mi,const float *ma,const float *d,int pass,int mask){trace(tr,s,mi,ma,d,pass,mask);}
static float damageSink(void *t,void *i,void *a,float *dir,float *point,int damage,int flags,int mod) {
 damageResult=damage;modResult=mod;return 0;
}
static void sourceDamageSink(void *t,void *i,void *a,float *dir,float *point,int damage,int flags,int mod) {
 damageResult=damage;modResult=mod;
}
static gentity_t *tempSink(vec3_t origin,int event){memset(&temp,0,sizeof(temp));return &temp;}
static qboolean accuracy(void *t,void *a){return qfalse;}
static void savePatch(unsigned char *p,void *fn,unsigned char old[5]){memcpy(old,p,5);hook(p,fn);}
static void restore(unsigned char *p,unsigned char old[5]){DWORD prev,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&prev);memcpy(p,old,5);FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,prev,&tmp);}
int main(int argc,char **argv) {
 const int ids[]={2,3,5,6,7,8,10,13,14,23,24,25,32,33,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54};
 HMODULE dll;unsigned char *base,*ent,*target,oldOrig[5],oldSource[5];tce_weaponDef_t *defs;
 void (__cdecl *original)(void *,float);int (__cdecl *extended)(void *,void *,float *,float *,float,int,int,int,int,int,int,int,int,int);
 void (__cdecl *pattern)(float *,float *,int,void *);
 int w,k,boost,aim,lag,bot,skill,n=0,expected,expectedCalls[32],expectedCount,expectedMod;float spread;
 float start[3]={0},finish[3]={8192,0,0};
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;defs=(void *)(base+0x24435c0);
 original=(void *)(base+0x9d410);extended=(void *)(base+0x9d500);
 pattern=(void *)(base+0x9ee90);
 hook(base+0x4ebf0,begin);hook(base+0x4ee70,end);hook(G_TimeShiftAllClients,begin);hook(G_UnTimeShiftAllClients,end);
 hook(base+0x9d1a0,endpoint);hook(Bullet_Endpos,endpoint);
 savePatch(base+0x9d500,origExtended,oldOrig);savePatch((void *)Bullet_Fire_Extended,origExtended,oldSource);
 *(void **)(entity+0x190)=raw;actor.client=&client;gearDef.parsed=1;
 for(k=0;k<128;++k)for(lag=0;lag<2;++lag)for(bot=0;bot<2;++bot) {
  vec3_t origin={-101.25f,19.75f,41.3f},direction={k-60.f,32.f,k*.123f};float expectedEndpoints[16][3];int j;
  *(int *)(entity+0xe8)=actor.s.weapon=48;*(int *)(raw+0xa4)=client.ps.weapon=48;
  *(int *)(entity+0x128)=actor.r.svFlags=bot?SVF_BOT:0;
  *(int *)(base+0x23cda8c)=g_antilag.integer=lag;
  defs[48].unknown_12c=weaponDef[48].unknown_12c=k*19;
  bulletCount=count=0;pattern(origin,direction,k,entity);expected=damageResult;expectedCount=count;
  if(bulletCount!=9)return 1;memcpy(expectedEndpoints,endpoints,sizeof(endpoints));memcpy(expectedCalls,calls,sizeof(calls));
  bulletCount=count=0;ShotgunPattern(origin,direction,k,&actor);
  if(bulletCount!=9 || damageResult!=expected || count!=expectedCount || memcmp(calls,expectedCalls,count*sizeof(int))) {
   fprintf(stderr,"shotgun seed%d damage%d/%d calls%d/%d\n",k,damageResult,expected,count,expectedCount);return 1;
  }
  for(j=0;j<27;++j)if(fabs(((float *)expectedEndpoints)[j]-((float *)endpoints)[j])>.003f) {
   fprintf(stderr,"shotgun endpoint seed%d component%d %.9g/%.9g\n",k,j,((float *)endpoints)[j],((float *)expectedEndpoints)[j]);return 1;
  }++n;
 }
 for(w=0;w<32;++w)for(k=0;k<7;++k)for(boost=0;boost<2;++boost)for(aim=0;aim<2;++aim)
 for(lag=0;lag<2;++lag)for(bot=0;bot<2;++bot)for(skill=0;skill<5;++skill) {
  int id=ids[w];memset(&defs[id],0,sizeof(*defs));memset(&weaponDef[id],0,sizeof(*defs));
  *(int *)(entity+0xe8)=actor.s.weapon=id;*(int *)(raw+0xa4)=client.ps.weapon=id;
  *(int *)(entity+0x128)=actor.r.svFlags=bot?SVF_BOT:0;
  *(int *)(base+0x23cda8c)=g_antilag.integer=lag;
  *(int *)(raw+0xf0)=client.ps.stats[STAT_TCE_WEAPON_FLAGS]=aim?4:0;
  client.sess.skill[SK_LIGHT_WEAPONS]=skill;
  defs[id].unknown_12c=weaponDef[id].unknown_12c=(k-1)*37;
  defs[id].unknown_1c0=weaponDef[id].unknown_1c0=boost;weaponDef[id].parsed=1;
  count=bulletCount=0;damageResult=0;original(entity,600);expected=damageResult;expectedCount=count;memcpy(expectedCalls,calls,sizeof(calls));spread=spreadResult;
  count=bulletCount=0;damageResult=0;Bullet_Fire(&actor,600,-123,qtrue);
  if(damageResult!=expected || count!=expectedCount || memcmp(expectedCalls,calls,count*sizeof(int)) || spread!=spreadResult) {
   fprintf(stderr,"bullet wrapper id%d k%d boost%d aim%d lag%d bot%d skill%d damage%d/%d count%d/%d\n",id,k,boost,aim,lag,bot,skill,damageResult,expected,count,expectedCount);return 1;
  }++n;
 }
 restore(base+0x9d500,oldOrig);restore((void *)Bullet_Fire_Extended,oldSource);
 hook(base+0x8ae80,trace);hook(trap_Trace,trace);hook(base+0x589c0,damageSink);hook(G_Damage,damageSink);
 hook(G_TempEntity,tempSink);hook(AccuracyHit,accuracy);
 ent=base+0x226e2a0;target=ent+0x57c;memset(ent,0,0x57c*2);memset(g_entities,0,2*sizeof(*g_entities));
 *(void **)(ent+0x190)=raw;*(void **)(target+0x190)=targetRaw;*(int *)target=1;
 *(int *)(target+0x2cc)=1;*(char **)(target+0x1a4)="player";
 g_entities[0].client=&client;g_entities[1].client=&targetClient;g_entities[1].s.number=1;g_entities[1].takedamage=1;g_entities[1].classname="player";
 *(int *)(base+0x7849cc)=g_debugBullets.integer=0;*(int *)(base+0x7e208c)=g_newbbox.integer=0;
 for(w=0;w<32;++w)for(k=0;k<8;++k) {
  int id=ids[w];memset(&defs[id],0,sizeof(*defs));memset(&weaponDef[id],0,sizeof(*defs));
  *(int *)(ent+0xe8)=g_entities[0].s.weapon=id;*(int *)(raw+0xa4)=client.ps.weapon=id;
  defs[id].mod=weaponDef[id].mod=k+1;defs[id].unknown_12c=weaponDef[id].unknown_12c=73;
  defs[id].unknown_130=weaponDef[id].unknown_130=50;weaponDef[id].parsed=1;
  damageResult=modResult=-1;extended(ent,ent,start,finish,0,73,100,0,0,4,4,8,123,0);expected=damageResult;expectedMod=modResult;
  damageResult=modResult=-1;Bullet_Fire_Extended(&g_entities[0],&g_entities[0],start,finish,0,73,100,0,0,4,4,8,123,0);
  if(damageResult!=expected || modResult!=expectedMod){fprintf(stderr,"impact id%d damage%d/%d mod%d/%d\n",id,damageResult,expected,modResult,expectedMod);return 1;}++n;
 }
 printf("%d original gear bullet wrapper/impact MOD comparisons passed\n",n);return 0;
}
