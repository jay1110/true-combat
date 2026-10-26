#define WIN32_LEAN_AND_MEAN
#include "../cgame/tce_shotgun_prediction.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
void QDECL Com_Printf(const char *fmt,...) {(void)fmt;}
void QDECL Com_Error(int level,const char *fmt,...) {(void)level;fputs(fmt,stderr);exit(2);}
int Q_vsnprintf(char *out,int size,const char *fmt,va_list args) {
 int result;
 #undef _vsnprintf
 result=_vsnprintf(out,size-1,fmt,args);out[size-1]=0;return result;
}
static tce_weaponDef_t defs[64];
static unsigned char rawSnap[0x1000];
static int mode,entryMat,exitMat,mutation,traces,count;
typedef struct { int type,a,b,c,d,e,f,g; float value[15]; } call_t;
static call_t logCalls[100],expected[100];
static void hook(void *at,void *fn) { DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t); }
static void restore(void *at,void *data) {DWORD o,t;VirtualProtect(at,5,PAGE_EXECUTE_READWRITE,&o);memcpy(at,data,5);FlushInstructionCache(GetCurrentProcess(),at,5);VirtualProtect(at,5,o,&t);}
static call_t *record(int type) {call_t *r;if(count>=100)abort();r=&logCalls[count++];memset(r,0,sizeof(*r));r->type=type;return r;}
static void traceSink(trace_t *tr,const float *s,const float *mi,const float *ma,const float *e,int pass,int mask) {
 call_t *r=record(1);int n=traces++;memcpy(r->value,s,12);memcpy(r->value+3,e,12);r->a=pass;r->b=mask;if(mi||ma)abort();
 memset(tr,0,sizeof(*tr));tr->fraction=.5f;tr->entityNum=1022;tr->plane.normal[0]=-1;tr->surfaceFlags=BG_SurfaceType2Flag(entryMat);tr->endpos[0]=100.25f;tr->endpos[1]=3.75f;tr->endpos[2]=4.25f;
 if(n==0){if(mode==1)tr->fraction=1.1f;if(mode==2)tr->entityNum=1;if(mode==3)tr->entityNum=1023;if(mode==4)tr->surfaceFlags|=0x80000;}
 else if(n==1){tr->endpos[0]=mode==5||mode==6?101.25f:1000.25f;if(mode==7)tr->fraction=1;}
 else if(n==2&&(mode==5||mode==6)){tr->endpos[0]=1000.25f;if(mode==6)tr->fraction=1;}
 else if(n==2||(n==3&&mode==5)){tr->endpos[0]=mode==8?80.5f:123.25f;tr->plane.normal[0]=1;tr->surfaceFlags=BG_SurfaceType2Flag(exitMat);}
 else tr->entityNum=1023;
}
static void bulletSink(float *end,int source,float *normal,int flesh,int a,int b,float water,int seed,int mat,float *start,int predicted) {
 call_t *r=record(2);r->a=source;r->b=flesh;r->c=a;r->d=b;r->e=seed;r->f=mat;r->g=predicted;memcpy(r->value,end,12);memcpy(r->value+3,normal,12);memcpy(r->value+6,start,12);r->value[9]=water;if(mutation)end[0]+=2;
}
static void wallSink(int weapon,int type,float *end,float *normal,float *direction,int material,int world,int exit) {
 call_t *r=record(3);r->a=weapon;r->b=type;r->c=material;r->d=world;r->e=exit;memcpy(r->value,end,12);memcpy(r->value+3,normal,12);memcpy(r->value+6,direction,12);
}
static void pelletSink(int source,int weapon,float *start,float *end,int energy,int walls,int seed) {
 call_t *r=record(4);r->a=source;r->b=weapon;r->c=energy;r->d=walls;r->e=seed;memcpy(r->value,start,12);memcpy(r->value+3,end,12);
}
static void sourcePellet(const tce_shotgunPrediction_t *ctx,int source,int weapon,float *start,float *end,int energy,int walls,int seed) {pelletSink(source,weapon,start,end,energy,walls,seed);}
static int compare(int expectedCount,int scenario) {
 int i,j;if(count!=expectedCount){printf("count scenario%d %d/%d mode%d materials%d/%d\n",scenario,count,expectedCount,mode,entryMat,exitMat);return 0;}
 for(i=0;i<count;++i){if(memcmp(&logCalls[i],&expected[i],8*sizeof(int))){printf("args scenario%d call%d\n",scenario,i);return 0;}for(j=0;j<15;++j)if(!_finite(logCalls[i].value[j]) || !_finite(expected[i].value[j]) || fabs(logCalls[i].value[j]-expected[i].value[j])>.003f){printf("value scenario%d call%d axis%d %.9g/%.9g\n",scenario,i,j,logCalls[i].value[j],expected[i].value[j]);return 0;}}return 1;
}
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *b,oldOrig[5],oldSource[5];int k,local,force,predict,n=0,energy,caliber,walls,expectedCount;
 tce_shotgunPrediction_t ctx={0,20,1,defs,traceSink,bulletSink,wallSink};
 void (__cdecl *original)(int,int,float*,float*,int,int,int);
 void (__cdecl *pattern)(float*,float*,int,int,int);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char*)dll;original=(void*)(b+0x75ba0);pattern=(void*)(b+0x7b1c0);
 *(void**)(b+0x40352c4)=rawSnap;*(int*)(rawSnap+0xcc)=0;*(int*)(b+0x257e10c)=20;
 hook(b+0x58420,traceSink);hook(b+0x7bd40,bulletSink);hook(b+0x78260,wallSink);
 memcpy(oldOrig,b+0x75ba0,5);memcpy(oldSource,TCE_CG_BulletFireExtended,5);hook(b+0x75ba0,pelletSink);hook(TCE_CG_BulletFireExtended,sourcePellet);
 for(k=0;k<256;++k)for(local=0;local<2;++local)for(force=0;force<2;++force)for(predict=-1;predict<=1;++predict) {
  vec3_t start={-101.25f,19.75f,41.3f},dir={k-127.f,32.f,k*.123f};
  ctx.predictBullets=*(int*)(b+0x25761cc)=predict;defs[48].unknown_134=*(int*)(b+0x4c23694)=k*100;
  count=0;pattern(start,dir,k,local,force);expectedCount=count;memcpy(expected,logCalls,sizeof(expected));count=0;
  TCE_CG_ShotgunPattern(&ctx,start,dir,k,local,force);if(!compare(expectedCount,n))return 1;++n;
 }
 for(k=0;k<256;++k) {
  entityState_t event;unsigned char rawEvent[0x120];memset(&event,0,sizeof(event));memset(rawEvent,0,sizeof(rawEvent));
  event.pos.trBase[0]=-33;event.pos.trBase[1]=45;event.pos.trBase[2]=k*.25f;
  event.origin2[0]=8192;event.origin2[1]=k;event.origin2[2]=-13;
  event.eventParm=k;event.otherEntityNum=k%3;ctx.predictBullets=*(int*)(b+0x25761cc)=k%2;
  memcpy(rawEvent+0x18,event.pos.trBase,12);memcpy(rawEvent+0x68,event.origin2,12);*(int*)(rawEvent+0xbc)=k;*(int*)(rawEvent+0x8c)=event.otherEntityNum;
  count=0;((void(__cdecl*)(void*))(b+0x7b320))(rawEvent);expectedCount=count;memcpy(expected,logCalls,sizeof(expected));
  count=0;TCE_CG_ShotgunFire(&ctx,&event);if(!compare(expectedCount,n))return 1;++n;
 }
 restore(b+0x75ba0,oldOrig);restore(TCE_CG_BulletFireExtended,oldSource);
 for(mode=0;mode<9;++mode)for(entryMat=0;entryMat<37;++entryMat)for(exitMat=0;exitMat<37;exitMat+=6)
 for(caliber=0;caliber<3;++caliber)for(walls=0;walls<3;++walls)for(energy=0;energy<4;++energy)for(mutation=0;mutation<2;++mutation) {
  vec3_t start={0,-7,3},end={8192,37,-84},actualEnd;int e=energy==0?0:energy==1?4:energy==2?90:10000;
  strcpy(defs[48].caliberClass,caliber==0?"12GA":caliber==1?"338LAPUA":"50BMG");memcpy(b+0x4c1df20+48*0x1cc,&defs[48],sizeof(defs[48]));
  VectorCopy(end,actualEnd);count=traces=0;original(2,48,start,end,e,walls,n);expectedCount=count;memcpy(expected,logCalls,sizeof(expected));
  count=traces=0;TCE_CG_BulletFireExtended(&ctx,2,48,start,actualEnd,e,walls,n);if(!compare(expectedCount,n))return 1;
  for(k=0;k<3;++k)if(!_finite(actualEnd[k]) || !_finite(end[k]) || fabs(actualEnd[k]-end[k])>.003f){printf("end scenario%d\n",n);return 1;}++n;
 }
 printf("%d original client shotgun pattern and full trace sequences matched\n",n);return 0;
}
