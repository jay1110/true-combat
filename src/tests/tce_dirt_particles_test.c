#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_particles.c"
#include "../cgame/tce_lightgrid.h"
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
void CG_AddDirtBulletParticles(vec3_t,vec3_t,int,int,int,float,float,float,float,qhandle_t);
void CG_AddBulletParticles(vec3_t,vec3_t,int,int,int,float);
void CG_TCEAddLocalBulletSparks(vec3_t,vec3_t,int,int,int,float);
static unsigned char originalSparks[32][512];
static localEntity_t sparkEntities[32];
static int sparkCount;
static void *originalAlloc(void) {void *p=originalSparks[sparkCount++];memset(p,0,512);return p;}
static localEntity_t *sparkAlloc(void) {localEntity_t *p=&sparkEntities[sparkCount++];memset(p,0,sizeof(*p));return p;}
static void hook(unsigned char *p,void *fn) {
 DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);
 p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
 FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
static int shader(void) {return 1234;}
static int randomInt(void) {return rand();}
static void light(const float *p,float *c) {c[0]=.2f;c[1]=.4f;c[2]=.7f;}
static void lightGrid(const tce_lightGrid_t *g,const float *p,float *c) {light(p,c);}
typedef struct {float org[3],vel[3],color[3],width,height,alpha;int duration,shader;} emission;
static emission records[32];static int count;
static void capture(float *p,float *v,int d,float w,float h,float a,const float *c,int s) {
 emission *e=&records[count++];memset(e,0,sizeof(*e));
 VectorCopy(p,e->org);VectorCopy(v,e->vel);VectorCopy(c,e->color);
 e->duration=d;e->width=w;e->height=h;e->alpha=a;e->shader=s;
}
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base;int i,j,k,n,expectedCount,cases=0;
 cparticle_t originalParticle,ours;emission expected[32];
 vec3_t org={13,-25,67},vel={7,-3,1},color={.2f,.4f,.7f};
 void (__cdecl *core)(float *,float *,int,float,float,float,const float *,int);
 void (__cdecl *emitter)(float *,float *,int,int,int,float,float,float,float,int);
 setbuf(stdout,NULL);
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
 base=(unsigned char *)dll;core=(void *)(base+0x51e60);emitter=(void *)(base+0x77d50);
 if(offsetof(cparticle_t,tceFadeInEnd)!=0x88||sizeof(cparticle_t)!=0x8c){puts("particle layout mismatch");return 3;}
 hook(base+0x2bc40,shader);hook((void *)getTestShader,shader);
 for(i=0;i<1000;++i) {
  memset(&originalParticle,0x5a,sizeof(originalParticle));ours=originalParticle;
  originalParticle.next=ours.next=NULL;
  *(void **)(base+0x25636bc)=i%7?&originalParticle:NULL;
  *(void **)(base+0x25636cc)=NULL;free_particles=i%7?&ours:NULL;active_particles=NULL;
  cg.time=*(int *)(base+0x407dea4)=i*123;
  cg_fxflags=*(int *)(base+0x18531a0)=i&1;
  core(org,vel,i-13,40,8,.33f,color,57);
  TCE_CG_ParticleDirtBulletDebris(org,vel,i-13,40,8,.33f,color,57);
  if(memcmp(&originalParticle,&ours,offsetof(cparticle_t,tceFadeInEnd)) ||
    (i%7?ours.tceFadeInEnd!=0:memcmp(&ours.tceFadeInEnd,&originalParticle.tceFadeInEnd,sizeof(float))) ||
    (!!active_particles != !!*(void **)(base+0x25636cc)))return 4;
 }
 puts("1000 pool states matched");
 hook(base+0x68160,light);hook((void *)TCE_CG_LightForParticleSimple,lightGrid);
 hook(base+0x51e60,capture);hook((void *)TCE_CG_ParticleDirtBulletDebris,capture);
 hook(base+0x885b6,randomInt);
 for(i=0;i<256;++i)for(n=-1;n<9;++n) {
  count=0;srand(i);emitter(org,vel,190,901,n,.25f,40,8,.33f,57);
  expectedCount=count;memcpy(expected,records,sizeof(expected));
  count=0;srand(i);CG_AddDirtBulletParticles(org,vel,190,901,n,.25f,40,8,.33f,57);
  if(count!=expectedCount)return 5;
  for(j=0;j<count;++j) {
   for(k=0;k<3;++k)if(fabs(records[j].vel[k]-expected[j].vel[k])>.0002f) {
    printf("velocity mismatch seed %d component %d: %g / %g\n",i,k,records[j].vel[k],expected[j].vel[k]);return 6;
   }
   VectorCopy(expected[j].vel,records[j].vel);
   if(memcmp(&records[j],&expected[j],sizeof(emission)))return 7;
  }
  ++cases;
 }
 printf("1000 original particle states and %d emitter sequences matched\n",cases);
 {
  void (__cdecl *sparks)(float *,float *,int,int,int,float)=(void *)(base+0x77100);
  hook(base+0x44900,originalAlloc);hook((void *)CG_AllocLocalEntity,sparkAlloc);
  cg.time=*(int *)(base+0x407dea4)=12345;
  cgs.media.bulletParticleTrailShader=*(int *)(base+0x25884e8)=975;
  for(i=0;i<256;++i)for(n=-1;n<10;++n) {
   srand(i);sparkCount=0;sparks(org,vel,350,201,n,.2f);expectedCount=sparkCount;
   /*30077100 emits local sparks, not the distinct30077bb0 event emitter. */
   srand(i);sparkCount=0;CG_TCEAddLocalBulletSparks(org,vel,350,201,n,.2f);
   if(sparkCount!=expectedCount)return 8;
   for(j=0;j<sparkCount;++j) {
    unsigned char *p=originalSparks[j];localEntity_t *e=&sparkEntities[j];
    if(e->leType!=LE_SPARK || *(int *)(p+8)!=8 || e->startTime!=*(int *)(p+16) ||
       e->endTime!=*(int *)(p+20) || e->lastTrailTime!=*(int *)(p+0x18c) ||
       e->pos.trType!=*(int *)(p+0x20) || e->pos.trTime!=*(int *)(p+0x24) || e->refEntity.customShader!=*(int *)(p+0x14c) ||
       e->bounceFactor!=*(float *)(p+0x68))return 9;
    for(k=0;k<3;++k)if(fabs(e->pos.trDelta[k]-*(float *)(p+0x38+4*k))>.0003 ||
       fabs(e->pos.trBase[k]-*(float *)(p+0x2c+4*k))>.00003 ||
       e->refEntity.origin[k]!=*(float *)(p+0x104+4*k))return 10;
    if(memcmp(e->refEntity.axis,p+0xb8,sizeof(vec3_t)*3))return 11;
   }
  }
  puts("2816 original spark emitter sequences matched");
 }
 return 0;
}
