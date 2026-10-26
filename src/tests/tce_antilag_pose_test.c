/* Actual production rewind/restore and trace-boundary controller. */
#define WIN32_LEAN_AND_MEAN
#include "../game/g_antilag.c"
#include <windows.h>
#include <stdio.h>
static unsigned char rawClient[64][0x16b0];
static gclient_t clients[64];
static unsigned char *base,*rawEntities;
static int links[64],linkCount;
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void originalLink(unsigned char *e){links[linkCount++]=(int)(e-rawEntities)/0x57c;}
static void sourceLink(gentity_t *e){links[linkCount++]=(int)(e-g_entities);}
static void init(int slot,int top,int pattern) {
 int j,k;unsigned char *e=rawEntities+slot*0x57c,*c=rawClient[slot];gentity_t *s=&g_entities[slot];gclient_t *p=&clients[slot];
 memset(e,0,0x57c);memset(c,0,0x16b0);memset(s,0,sizeof(*s));memset(p,0,sizeof(*p));
 *(void **)(e+0x190)=c;s->client=p;*(int *)(e+0x194)=s->inuse=1;
 *(int *)(e+0x2c8)=s->health=100;*(int *)(c+0xb30)=p->sess.sessionTeam=2;
 *(int *)(c+0x5b0)=p->pers.cmd.serverTime=900;*(int *)(c+0x133c)=p->topMarker=top;
 *(int *)(c+0xc)=p->ps.pm_flags=pattern*7;*(int *)(c+0xf0)=p->ps.stats[STAT_TCE_WEAPON_FLAGS]=pattern*13;
 *(int *)(c+0x68)=p->ps.eFlags=pattern&1?EF_PRONE:0;
 for(j=0;j<3;++j) {
  *(float *)(e+0x134+j*4)=s->r.mins[j]=-17-j*.5f;
  *(float *)(e+0x140+j*4)=s->r.maxs[j]=20+j*.25f;
  *(float *)(e+0x168+j*4)=s->r.currentOrigin[j]=99+j*.75f;
  *(float *)(e+0x174+j*4)=s->r.currentAngles[j]=350+j*4.5f;
  *(float *)(c+0xb0+j*4)=p->ps.viewangles[j]=-35+j*15.25f;
 }
 for(j=0;j<10;++j){clientMarker_t *m=&p->clientMarkers[j];int age=(top-j+10)%10;
  for(k=0;k<15;++k)((float *)m)[k]=(float)(j*17+k*.25f-pattern);
  m->time=1000-age*50;m->serverTime=7000+j;m->pm_flags=j*31;m->weaponFlags=j*53;m->eFlags=pattern&1?EF_PRONE:0;
 }
 memcpy(c+0x1340,p->clientMarkers,800);memset(&p->backupMarker,0x35,80);memcpy(c+0x1660,&p->backupMarker,80);
}
static int close(float a,float b){return fabs(a-b)<.0001f;}
static int same(int slot) {
 int j;unsigned char *e=rawEntities+slot*0x57c,*c=rawClient[slot];gentity_t *s=&g_entities[slot];gclient_t *p=&clients[slot];
 for(j=0;j<3;++j)if(!close(*(float *)(e+0x134+j*4),s->r.mins[j])||!close(*(float *)(e+0x140+j*4),s->r.maxs[j])||!close(*(float *)(e+0x168+j*4),s->r.currentOrigin[j])||!close(*(float *)(e+0x174+j*4),s->r.currentAngles[j])||!close(*(float *)(c+0xb0+j*4),p->ps.viewangles[j]))return 0;
 return *(int *)(c+0xc)==p->ps.pm_flags && *(int *)(c+0xf0)==p->ps.stats[STAT_TCE_WEAPON_FLAGS] && *(int *)(c+0x68)==p->ps.eFlags && *(int *)(e+0x128)==s->r.svFlags && !memcmp(c+0x1660,&p->backupMarker,80);
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
  if(emitCount!=expectedCount||emitEvent!=expectedEvent||memcmp(&originalTrace,&sourceTrace,sizeof(sourceTrace)))return -1;
  if(emitCount) {
   if(memcmp(emitOrigin,expectedOrigin,sizeof(vec3_t))||memcmp(emitterRaw+0x5c,emitter.s.origin,sizeof(vec3_t))||memcmp(emitterRaw+0x68,emitter.s.origin2,sizeof(vec3_t))||*(int *)(emitterRaw+0x54)!=emitter.s.time||*(int *)(emitterRaw+0xf4)!=emitter.s.density)return -1;
  }
  ++cases;
 }
 return cases;
}
int main(int argc,char **argv) {
 HMODULE dll;int top,t,pattern,repeat,i,scale,n,expected[64],count=0;const int times[]={-100,500,549,550,551,600,949,950,951,999,1000,1001,5000};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;rawEntities=base+0x226e2a0;
 level.time=*(int *)(base+0x7febe0)=1000;patch(base+0x8afa0,originalLink);patch(trap_LinkEntity,sourceLink);
 for(top=0;top<10;++top)for(t=0;t<13;++t)for(pattern=0;pattern<4;++pattern) {
  init(0,top,pattern);
  for(repeat=0;repeat<2;++repeat) {
   ((void(__cdecl *)(void *,int))(base+0x4e790))(rawEntities,times[t]+repeat*7);G_TimeShiftClient(&g_entities[0],times[t]+repeat*7);
   if(!same(0)){printf("rewind mismatch top%d time%d pattern%d repeat%d\n",top,times[t],pattern,repeat);return 1;}++count;
  }
  ((void(__cdecl *)(void *))(base+0x4ed40))(rawEntities);G_UnTimeShiftClient(&g_entities[0]);
  if(!same(0)){printf("restore mismatch\n");return 1;}++count;
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
  if(n!=linkCount||memcmp(expected,links,n*sizeof(int))){printf("begin link mismatch\n");return 1;}
  for(i=0;i<64;++i)if(!same(i)){printf("begin mismatch slot%d scale%d pattern%d time%d\n",i,scale,pattern,times[t]);return 1;}
  linkCount=0;((void(__cdecl *)(void *))(base+0x4ee70))(rawEntities);n=linkCount;memcpy(expected,links,n*sizeof(int));
  linkCount=0;G_UnTimeShiftAllClients(&g_entities[0]);
  if(n!=linkCount||memcmp(expected,links,n*sizeof(int))){printf("end link mismatch\n");return 1;}
  for(i=0;i<64;++i)if(!same(i)){printf("end mismatch slot%d scale%d pattern%d time%d\n",i,scale,pattern,times[t]);return 1;}
  ++count;
 }
 i=emitterCases();if(i<0){printf("emitter mismatch\n");return 1;}count+=i;
 printf("PASS %d original pose/boundary scenarios including64-slot selection and reversible scaling\n",count);return 0;
}
