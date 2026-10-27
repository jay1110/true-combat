/* Production pose semantics plus retained unrelated original-DLL comparisons.
 * Chronology, historical inputs and transactional cleanup intentionally differ
 * from the shipped bug. See tools/modernize/hitbox_regression.py for collision,
 * damage/death and sanitizer contracts without an original-DLL dependency. */
#define WIN32_LEAN_AND_MEAN
#include "../game/g_antilag.c"
#include <windows.h>
#include <stdio.h>
#define LEGACY_MARKER_BYTES 80
static unsigned char rawClient[64][0x16b0];
static gclient_t clients[64];
static unsigned char *base,*rawEntities;
static int links[64],linkCount,legacyDifferences;
static struct { clientMarker_t pose; int svFlags; } saved[64];
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void originalLink(unsigned char *e){links[linkCount++]=(int)(e-rawEntities)/0x57c;}
static void sourceLink(gentity_t *e){links[linkCount++]=(int)(e-g_entities);}
static int close(float a,float b){return fabs(a-b)<.0001f;}
static float coordinate(int time,int axis,int pattern,int group) {
 return time*(group==0?.125f:group==1?.05f:.025f)+axis*.75f-pattern;
}
static void init(int slot,int top,int pattern) {
 int j,k;unsigned char *e=rawEntities+slot*0x57c,*c=rawClient[slot];gentity_t *s=&g_entities[slot];gclient_t *p=&clients[slot];
 memset(e,0,0x57c);memset(c,0,0x16b0);memset(s,0,sizeof(*s));memset(p,0,sizeof(*p));
 *(void **)(e+0x190)=c;s->client=p;*(int *)(e+0x194)=s->inuse=1;
 *(int *)(e+0x2c8)=s->health=100;*(int *)(c+0xb30)=p->sess.sessionTeam=2;
 *(int *)(c+0x5b0)=p->pers.cmd.serverTime=900;*(int *)(c+0x133c)=p->topMarker=top;
 p->ps.pm_type=PM_NORMAL;
 *(int *)(c+0xc)=p->ps.pm_flags=PMF_TIME_LAND|(pattern&1?PMF_DUCKED:0);
 *(int *)(c+0xf0)=p->ps.stats[STAT_TCE_WEAPON_FLAGS]=0x80|(pattern&1?0x20:0);
 *(int *)(c+0x68)=p->ps.eFlags=EF_HEADSHOT|(pattern&1?EF_PRONE:0);
 *(float *)(c+0x3c)=p->ps.leanf=25;*(int *)(c+0xa4)=p->ps.weapon=30;
 for(j=0;j<3;++j) {
  *(float *)(e+0x134+j*4)=s->r.mins[j]=-17-j*.5f;
  *(float *)(e+0x140+j*4)=s->r.maxs[j]=20+j*.25f;
  *(float *)(e+0x168+j*4)=s->r.currentOrigin[j]=coordinate(1000,j,pattern,0);
  *(float *)(e+0x174+j*4)=s->r.currentAngles[j]=coordinate(1000,j,pattern,1);
  *(float *)(c+0xb0+j*4)=p->ps.viewangles[j]=coordinate(1000,j,pattern,2);
 }
 for(j=0;j<MAX_CLIENT_MARKERS;++j) {
  clientMarker_t *m=&p->clientMarkers[j];int age=(top-j+MAX_CLIENT_MARKERS)%MAX_CLIENT_MARKERS;
  m->time=1000-age*50;m->serverTime=7000+j;
  for(k=0;k<3;++k) {
   m->mins[k]=s->r.mins[k]-age*.125f;m->maxs[k]=s->r.maxs[k]+age*.25f;
   m->origin[k]=coordinate(m->time,k,pattern,0);m->angles[k]=coordinate(m->time,k,pattern,1);m->viewangles[k]=coordinate(m->time,k,pattern,2);
  }
  m->pm_flags=PMF_TIME_KNOCKBACK|((age+pattern)&1?PMF_DUCKED:0);
  m->weaponFlags=0x100|((age+pattern)&1?0x20:0);
  m->eFlags=(age+pattern)&1?EF_PRONE:0;m->lean=25-age*5.0f;m->weapon=age&1?1:30;
  memcpy(c+0x1340+j*LEGACY_MARKER_BYTES,m,LEGACY_MARKER_BYTES);
 }
 /* Capture the pre-shot state independently of the production capture helper. */
 VectorCopy(s->r.mins,saved[slot].pose.mins);VectorCopy(s->r.maxs,saved[slot].pose.maxs);
 VectorCopy(s->r.currentOrigin,saved[slot].pose.origin);VectorCopy(s->r.currentAngles,saved[slot].pose.angles);
 VectorCopy(p->ps.viewangles,saved[slot].pose.viewangles);saved[slot].pose.pm_flags=p->ps.pm_flags;
 saved[slot].pose.weaponFlags=p->ps.stats[STAT_TCE_WEAPON_FLAGS];saved[slot].pose.eFlags=p->ps.eFlags;
 saved[slot].pose.lean=p->ps.leanf;saved[slot].pose.weapon=p->ps.weapon;saved[slot].svFlags=s->r.svFlags;
}
static int legacySamePose(int slot) {
 int j;unsigned char *e=rawEntities+slot*0x57c,*c=rawClient[slot];gentity_t *s=&g_entities[slot];gclient_t *p=&clients[slot];
 for(j=0;j<3;++j)if(!close(*(float *)(e+0x134+j*4),s->r.mins[j])||!close(*(float *)(e+0x140+j*4),s->r.maxs[j])||!close(*(float *)(e+0x168+j*4),s->r.currentOrigin[j])||!close(*(float *)(e+0x174+j*4),s->r.currentAngles[j])||!close(*(float *)(c+0xb0+j*4),p->ps.viewangles[j]))return 0;
 return *(int *)(c+0xc)==p->ps.pm_flags&&*(int *)(c+0xf0)==p->ps.stats[STAT_TCE_WEAPON_FLAGS]&&*(int *)(c+0x68)==p->ps.eFlags&&*(int *)(e+0x128)==s->r.svFlags;
}
static int restored(int slot) {
 int j;gentity_t *s=&g_entities[slot];gclient_t *p=&clients[slot];clientMarker_t *m=&saved[slot].pose;
 for(j=0;j<3;++j)if(s->r.mins[j]!=m->mins[j]||s->r.maxs[j]!=m->maxs[j]||s->r.currentOrigin[j]!=m->origin[j]||s->r.currentAngles[j]!=m->angles[j]||p->ps.viewangles[j]!=m->viewangles[j])return 0;
 return p->ps.pm_flags==m->pm_flags&&p->ps.eFlags==m->eFlags&&p->ps.stats[STAT_TCE_WEAPON_FLAGS]==m->weaponFlags&&p->ps.leanf==m->lean&&p->ps.weapon==m->weapon&&s->r.svFlags==saved[slot].svFlags&&!p->tceRewind.active;
}
static int projected(int slot,int time,int pattern,int scale) {
 int j,age,row,pm,eFlags,flags,svFlags,proneFlags=EF_PRONE|EF_PRONE_MOVING;
 float xy=1,z=1;gentity_t *s=&g_entities[slot];gclient_t *p=&clients[slot];clientMarker_t *m,*live=&saved[slot].pose;
 if(time>=1000) {
  m=live;time=1000;age=0;
 } else {
  if(time<550)time=550;
  age=(1000-time+49)/50;row=(p->topMarker-age+MAX_CLIENT_MARKERS)%MAX_CLIENT_MARKERS;m=&p->clientMarkers[row];
 }
 pm=(live->pm_flags&~PMF_DUCKED)|(m->pm_flags&PMF_DUCKED);
 eFlags=(live->eFlags&~proneFlags)|(m->eFlags&proneFlags);
 flags=(live->weaponFlags&~0x20)|(m->weaponFlags&0x20);svFlags=saved[slot].svFlags;
 if(scale){if(eFlags&EF_PRONE){xy=2.5f;z=.5f;svFlags|=0x40000;}else{xy=1.5f;z=1.25f;svFlags|=0x20000;}}
 for(j=0;j<3;++j) {
  if(!close(s->r.currentOrigin[j],coordinate(time,j,pattern,0))||!close(s->r.currentAngles[j],coordinate(time,j,pattern,1))||!close(p->ps.viewangles[j],coordinate(time,j,pattern,2)))return 0;
  if(!close(s->r.mins[j],m->mins[j]*(j<2?xy:1))||!close(s->r.maxs[j],m->maxs[j]*(j<2?xy:z)))return 0;
 }
 return p->ps.pm_flags==pm&&p->ps.eFlags==eFlags&&p->ps.stats[STAT_TCE_WEAPON_FLAGS]==flags&&s->r.svFlags==svFlags&&p->tceRewind.active&&close(p->tceRewind.lean,25-(1000-time)*.1f)&&p->tceRewind.weapon==m->weapon&&p->ps.leanf==live->lean&&p->ps.weapon==live->weapon;
}
extern void EmitterCheck(gentity_t *,gentity_t *,trace_t *);
static unsigned char emitterRaw[0x57c];static gentity_t emitter;
static int emitCount,emitEvent;static vec3_t emitOrigin;
static void *originalTemp(vec3_t origin,int event) {++emitCount;emitEvent=event;VectorCopy(origin,emitOrigin);memset(emitterRaw,0,sizeof(emitterRaw));return emitterRaw;}
static gentity_t *sourceTemp(vec3_t origin,int event) {++emitCount;emitEvent=event==EV_EMITTER?97:event;VectorCopy(origin,emitOrigin);memset(&emitter,0,sizeof(emitter));return &emitter;}
static int emitterCases(void) {
 const char *names[]={"func_leaky","FUNC_LEAKY","func_explosive","worldspawn","func_leaky_suffix"};
 int n,i,k,expectedCount,expectedEvent,cases=0;float expectedOrigin[3];trace_t originalTrace,sourceTrace;
 patch(base+0x94fa0,originalTemp);patch(G_TempEntity,sourceTemp);
 for(n=0;n<5;++n)for(i=0;i<128;++i) {
  init(0,0,0);init(1,0,0);g_entities[1].classname=(char *)names[n];*(const char **)(rawEntities+0x57c+0x1a4)=names[n];
  memset(&originalTrace,0,sizeof(originalTrace));
  for(k=0;k<3;++k) {
   originalTrace.endpos[k]=(i-64)*.31f+k*.75f;originalTrace.plane.normal[k]=(i%7)*.1f-k;
   *(float *)(rawEntities+0x5c+k*4)=g_entities[0].s.origin[k]=i&1?200:-200;
  }
  sourceTrace=originalTrace;emitCount=emitEvent=0;
  ((void(__cdecl *)(void *,void *,void *))(base+0x9d0f0))(rawEntities+0x57c,rawEntities,&originalTrace);
  expectedCount=emitCount;expectedEvent=emitEvent;VectorCopy(emitOrigin,expectedOrigin);
  emitCount=emitEvent=0;EmitterCheck(&g_entities[1],&g_entities[0],&sourceTrace);
  if(emitCount!=expectedCount||emitEvent!=expectedEvent||memcmp(&originalTrace,&sourceTrace,sizeof(sourceTrace))){printf("emitter control mismatch name%d case%d count%d/%d event%d/%d trace%d\n",n,i,emitCount,expectedCount,emitEvent,expectedEvent,memcmp(&originalTrace,&sourceTrace,sizeof(sourceTrace)));return -1;}
  if(emitCount) {
   if(memcmp(emitOrigin,expectedOrigin,sizeof(vec3_t))||memcmp(emitterRaw+0x5c,emitter.s.origin,sizeof(vec3_t))||memcmp(emitterRaw+0x68,emitter.s.origin2,sizeof(vec3_t))||*(int *)(emitterRaw+0x54)!=emitter.s.time||*(int *)(emitterRaw+0xf4)!=emitter.s.density){printf("emitter payload mismatch name%d case%d origin %.9g,%.9g,%.9g / %.9g,%.9g,%.9g eventOrigin%d normal%d time%d/%d density%d/%d\n",n,i,emitOrigin[0],emitOrigin[1],emitOrigin[2],expectedOrigin[0],expectedOrigin[1],expectedOrigin[2],memcmp(emitterRaw+0x5c,emitter.s.origin,sizeof(vec3_t)),memcmp(emitterRaw+0x68,emitter.s.origin2,sizeof(vec3_t)),*(int *)(emitterRaw+0x54),emitter.s.time,*(int *)(emitterRaw+0xf4),emitter.s.density);return -1;}
  }
  ++cases;
 }
 return cases;
}
int main(int argc,char **argv) {
 HMODULE dll;int top,t,pattern,repeat,i,scale,n,expected[64],count=0;
 const int times[]={-100,500,549,550,551,600,949,950,951,999,1000,1001,5000};
 if(offsetof(clientMarker_t,lean)!=LEGACY_MARKER_BYTES)return 2;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;rawEntities=base+0x226e2a0;
 level.time=*(int *)(base+0x7febe0)=1000;patch(base+0x8afa0,originalLink);patch(trap_LinkEntity,sourceLink);
 for(top=0;top<MAX_CLIENT_MARKERS;++top)for(t=0;t<13;++t)for(pattern=0;pattern<4;++pattern) {
  init(0,top,pattern);
  for(repeat=0;repeat<2;++repeat) {
   int requested=times[t]+repeat*7;
   ((void(__cdecl *)(void *,int))(base+0x4e790))(rawEntities,requested);G_TimeShiftClient(&g_entities[0],requested);
   if(!projected(0,requested,pattern,0)){printf("semantic rewind mismatch top%d time%d pattern%d repeat%d\n",top,requested,pattern,repeat);return 1;}
   if(!legacySamePose(0))++legacyDifferences;++count;
   ((void(__cdecl *)(void *))(base+0x4ed40))(rawEntities);G_UnTimeShiftClient(&g_entities[0]);
   if(!restored(0)){printf("semantic restore mismatch\n");return 1;}++count;
  }
 }
 for(scale=0;scale<2;++scale)for(pattern=0;pattern<16;++pattern)for(t=0;t<13;++t) {
  for(i=0;i<64;++i){unsigned char *e=rawEntities+i*0x57c,*c=rawClient[i];init(i,i%10,i+pattern);
   *(int *)(c+0xb30)=clients[i].sess.sessionTeam=(i%6)-1;
   *(int *)(e+0x194)=g_entities[i].inuse=!!(i%3);
   *(int *)(e+0x2c8)=g_entities[i].health=i%5?100:0;
   if(i%11==0){*(void **)(e+0x190)=NULL;g_entities[i].client=NULL;}
  }
  init(0,0,pattern);*(int *)(rawClient[0]+0x5b0)=clients[0].pers.cmd.serverTime=times[t];
  linkCount=0;((void(__cdecl *)(void *,int))(base+0x4ebf0))(rawEntities,scale);n=linkCount;memcpy(expected,links,n*sizeof(int));
  linkCount=0;G_TimeShiftAllClients(&g_entities[0],scale);
  if(n!=linkCount||memcmp(expected,links,n*sizeof(int))){printf("original begin selection/link mismatch\n");return 1;}
  for(i=0;i<64;++i) {
   int eligible=g_entities[i].client&&g_entities[i].inuse&&(unsigned)clients[i].sess.sessionTeam<TEAM_SPECTATOR&&i&&g_entities[i].health>0;
   if(eligible?!projected(i,times[t],i+pattern,scale):!restored(i)){printf("semantic begin mismatch slot%d scale%d pattern%d time%d\n",i,scale,pattern,times[t]);return 1;}
   if(!legacySamePose(i))++legacyDifferences;
  }
  linkCount=0;((void(__cdecl *)(void *))(base+0x4ee70))(rawEntities);n=linkCount;memcpy(expected,links,n*sizeof(int));
  linkCount=0;G_UnTimeShiftAllClients(&g_entities[0]);
  if(n!=linkCount||memcmp(expected,links,n*sizeof(int))){printf("original end selection/link mismatch\n");return 1;}
  for(i=0;i<64;++i)if(!restored(i)){printf("semantic end mismatch slot%d scale%d pattern%d time%d\n",i,scale,pattern,times[t]);return 1;}
  ++count;
 }
 i=emitterCases();if(i<0){printf("original emitter mismatch\n");return 1;}count+=i;
 if(!legacyDifferences){printf("expected original rewind differences were not observed\n");return 1;}
 printf("PASS %d scenarios: semantic pose/restore, original64-slot link selection,640 original emitter checks; %d intended original pose differences recorded\n",count,legacyDifferences);
 printf("Original reversed chronology/full-state restore is deliberately not an equality gate. Run tools/modernize/hitbox_regression.py for overlap, world-cover, death cleanup and stats contracts.\n");return 0;
}
