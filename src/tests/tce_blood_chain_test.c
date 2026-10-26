#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../game/tce_trajectory.h"
#include <windows.h>
#include <stdio.h>
extern void CG_AddBloodTrails(vec3_t,vec3_t,int,int,int,float,vec3_t);
extern void CG_AddBloodElements(localEntity_t *);
static unsigned char raw[32][448],rawSnap[4096];static localEntity_t particles[32],wanted[32];
static int currentCase;
static int originalMode,allocated,rngCalls,mode,count;static unsigned rng;
typedef struct {int kind,args[8];float values[20];} event_t;
static event_t events[100],expected[100];static snapshot_t snapshot;
static void require(int ok,const char *why){if(!ok){printf("blood mode%d case%d: %s\n",mode,currentCase,why);exit(1);}}
static event_t *event(int kind){event_t*e=&events[count++];require(count<100,"event overflow");memset(e,0,sizeof(*e));e->kind=kind;return e;}
static void hook(void *at,void *fn){DWORD o,t;unsigned char*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int*)(p+1)=(unsigned char*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void fromRaw(localEntity_t*out,void*in){memset(out,0,sizeof(*out));memcpy(out,in,0x6c);memcpy((char*)out+0x6c,(char*)in+0x70,offsetof(localEntity_t,tceFragment)-0x6c);}
static void toRaw(void*out,const localEntity_t*in){memset(out,0,448);memcpy(out,in,0x6c);memcpy((char*)out+0x70,(char*)in+0x6c,offsetof(localEntity_t,tceFragment)-0x6c);}
int rand(void){rng=rng*1664525u+1013904223u;++rngCalls;return (rng>>16)&32767;}
static localEntity_t *allocate(void){int i=allocated++;require(i<32,"particle overflow");return originalMode?(localEntity_t*)raw[i]:&particles[i];}
static void bleedOrigin(vec3_t head,vec3_t body,int entity){event_t*e=event(1);e->args[0]=entity;head[0]=3;head[1]=-7;head[2]=50;body[0]=-2;body[1]=3;body[2]=5;}
static void traceSink(trace_t*t,const vec3_t s,const vec3_t mi,const vec3_t ma,const vec3_t e,int skip,int mask){event_t*r=event(2);require(!mi&&!ma,"trace bounds");memcpy(r->values,s,12);memcpy(r->values+3,e,12);r->args[0]=skip;r->args[1]=mask;memset(t,0,sizeof(*t));t->fraction=mode==0?1:mode==1?.25f:mode==2?0:.6f;t->startsolid=mode==3;VectorCopy(e,t->endpos);t->endpos[0]-=2;t->plane.normal[2]=1;}
static void evalOriginal(const trajectory_t*t,int time,vec3_t out,int angle,int spline,float scale){event_t*r=event(3);r->args[0]=time;r->args[1]=angle;r->args[2]=spline;r->args[3]=t->trTime;r->args[4]=t->trType;r->values[0]=scale;VectorCopy(t->trBase,out);out[0]+=13;out[1]-=17;out[2]+=5;}
static void evalSource(const trajectory_t*t,int time,vec3_t out,int angle,int spline){evalOriginal(t,time,out,angle,spline,1);}
static int trail(int index,void *owner,int shader,int time,int stype,vec3_t pos,int life,float a,float b,float w,float w2,int flags,vec3_t c,vec3_t c2,float ratio,float speed){event_t*r=event(4);r->args[0]=index;r->args[1]=shader;r->args[2]=time;r->args[3]=stype;r->args[4]=life;r->args[5]=flags;memcpy(r->values,pos,12);memcpy(r->values+3,c,12);memcpy(r->values+6,c2,12);r->values[9]=a;r->values[10]=b;r->values[11]=w;r->values[12]=w2;r->values[13]=ratio;r->values[14]=speed;require(owner!=NULL,"trail owner");return index+1;}
static void reflect(localEntity_t*p,trace_t*t){event_t*r=event(5);memcpy(r->values,t->endpos,12);p->pos.trBase[1]+=11;}
static int closeFloat(float a,float b){return _finite(a)&&_finite(b)&&fabs(a-b)<=.00003f+fabs(b)*.000002f;}
static void compareParticle(localEntity_t*a,localEntity_t*b){int i;for(i=0;i<3;++i){if(!closeFloat(a->pos.trDelta[i],b->pos.trDelta[i]))printf("velocity axis%d %.9g %.9g\n",i,a->pos.trDelta[i],b->pos.trDelta[i]);require(closeFloat(a->pos.trDelta[i],b->pos.trDelta[i]),"velocity");require(closeFloat(a->pos.trBase[i],b->pos.trBase[i]),"position");require(closeFloat(a->refEntity.origin[i],b->refEntity.origin[i]),"render origin");a->refEntity.origin[i]=b->refEntity.origin[i];a->pos.trDelta[i]=b->pos.trDelta[i];a->pos.trBase[i]=b->pos.trBase[i];}if(memcmp(a,b,sizeof(*a))){for(i=0;i<sizeof(*a);++i)if(((byte*)a)[i]!=((byte*)b)[i]){printf("particle byte %x actual%x expected%x\n",i,((byte*)a)[i],((byte*)b)[i]);break;}}require(!memcmp(a,b,sizeof(*a)),"complete particle");}
static void compareEvents(int expectedCount){int i,j;require(count==expectedCount,"dependency count");for(i=0;i<count;++i){if(memcmp(events[i].args,expected[i].args,sizeof(events[i].args))) { int z; printf("event %d kind %d/%d\n",i,events[i].kind,expected[i].kind); for(z=0;z<8;++z) printf("arg%d %d/%d\n",z,events[i].args[z],expected[i].args[z]); }require(events[i].kind==expected[i].kind&&!memcmp(events[i].args,expected[i].args,sizeof(events[i].args)),"dependency arguments");for(j=0;j<20;++j)require(closeFloat(events[i].values[j],expected[i].values[j]),"dependency floats");}}
int main(int argc,char**argv){HMODULE dll;unsigned char*b;int n=0,k,i,enabled,self,expectedCount,expectedRng,expectedAllocated,j;unsigned savedRng;
 void(__cdecl*add)(float*,float*,int,int,int,float,float*);void(__cdecl*bleed)(float*,int,float*);
 require(argc==2,"DLL path");dll=LoadLibraryA(argv[1]);require(dll!=NULL,"DLL load");b=(unsigned char*)dll;add=(void*)(b+0x2bc60);bleed=(void*)(b+0x2be80);
 cg.snap=&snapshot;*(void**)(b+0x40352c4)=rawSnap;*(int*)(rawSnap+0xcc)=snapshot.ps.clientNum=0;
 hook(b+0x885b6,rand);hook(b+0x44900,allocate);hook(CG_AllocLocalEntity,allocate);hook(b+0x563d0,bleedOrigin);hook(CG_GetBleedOrigin,bleedOrigin);
 for(k=0;k<3000;++k) {
  vec3_t origin={k*.013f,-7.3f,40},dir={.13f,-.21f,.7f},color={.1f,.25f,.9f};int amount=k%8-1,speed=k%201-100,duration=k%1000;
  cg.time=*(int*)(b+0x407dea4)=10000+k;
  memset(raw,0,sizeof(raw));memset(particles,0,sizeof(particles));count=allocated=rngCalls=0;originalMode=1;rng=k;
  add(origin,dir,speed,duration,amount,k*.0001f,color);expectedAllocated=allocated;expectedRng=rngCalls;savedRng=rng;for(i=0;i<allocated;++i)fromRaw(&wanted[i],raw[i]);
  count=allocated=rngCalls=0;originalMode=0;rng=k;CG_AddBloodTrails(origin,dir,speed,duration,amount,k*.0001f,color);
  require(allocated==expectedAllocated&&rngCalls==expectedRng&&rng==savedRng,"add count/RNG");for(i=0;i<allocated;++i)compareParticle(&particles[i],&wanted[i]);++n;
 }
 for(k=0;k<2000;++k)for(enabled=0;enabled<2;++enabled)for(self=0;self<2;++self){
  currentCase=k;vec3_t origin={k%19-9.f,k%31-15.f,k%111-30.f},color={.3f,.5f,.7f};cg_blood.integer=*(int*)(b+0x40132ec)=enabled;
  memset(raw,0,sizeof(raw));memset(particles,0,sizeof(particles));count=allocated=rngCalls=0;originalMode=1;rng=k;
  bleed(origin,self,color);expectedAllocated=allocated;expectedRng=rngCalls;savedRng=rng;expectedCount=count;memcpy(expected,events,sizeof(expected));for(i=0;i<allocated;++i)fromRaw(&wanted[i],raw[i]);
  count=allocated=rngCalls=0;originalMode=0;rng=k;CG_Bleed(origin,self,color);
  require(allocated==expectedAllocated&&rngCalls==expectedRng&&rng==savedRng,"bleed count/RNG");compareEvents(expectedCount);for(i=0;i<allocated;++i)compareParticle(&particles[i],&wanted[i]);++n;
 }
 hook(b+0x58420,traceSink);hook(CG_Trace,traceSink);hook(b+0x43f0,evalOriginal);hook(TCE_BG_EvaluateTrajectory,evalOriginal);hook(b+0x65c60,trail);hook(CG_AddTrailJunc,trail);hook(b+0x44f20,reflect);hook(CG_ReflectVelocity,reflect);
 cgs.media.bloodTrailShader=*(int*)(b+0x2588198)=731;
 for(k=0;k<1000;++k)for(mode=0;mode<4;++mode){localEntity_t le,want;currentCase=k;unsigned char one[448];memset(&le,0,sizeof(le));
  cg.time=*(int*)(b+0x407dea4)=1000+k;cg.frametime=*(int*)(b+0x407dea0)=k%50+1;le.startTime=cg.time-17;le.endTime=cg.time+337;
  le.pos.trType=TR_GRAVITY_LOW;le.pos.trTime=cg.time-3;le.headJuncIndex=k%7;
  for(j=0;j<3;++j){le.pos.trBase[j]=j*11.f;le.refEntity.origin[j]=j*13.f;le.color[j]=j*.2f;}
  toRaw(one,&le);count=0;originalMode=1;((void(__cdecl*)(void*))(b+0x464d0))(one);expectedCount=count;memcpy(expected,events,sizeof(expected));fromRaw(&want,one);
  count=0;originalMode=0;CG_AddBloodElements(&le);compareEvents(expectedCount);compareParticle(&le,&want);++n;
 }
 printf("%d complete blood emitter, bleed and frame-renderer original comparisons passed\n",n);return 0;
}
