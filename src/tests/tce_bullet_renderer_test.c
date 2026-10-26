#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/tce_bullet_renderer.h"
#include "../cgame/tce_lightgrid.h"
#include <windows.h>
#include <stdio.h>
extern qboolean CG_CalcMuzzlePoint(int,vec3_t);
static unsigned rng;static int randCalls,n,scenario,testCase,lastBlood,traceN;static byte rawSnap[4096];static snapshot_t snapshot;
typedef struct {int type,arg[8];float v[30];} event_t;
static event_t events[128],expected[128];
static void require(int b,const char*s){if(!b){printf("case%d: %s\n",testCase,s);exit(1);}}
static event_t*event(int type){event_t*e=&events[n++];require(n<128,"event overflow");memset(e,0,sizeof(*e));e->type=type;return e;}
static void vec(float*out,const float*in){if(in)memcpy(out,in,12);else out[0]=-999;}
int rand(void){rng=rng*1664525u+1013904223u;++randCalls;return (rng>>16)&32767;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void trace(trace_t*out,const float*s,const float*mi,const float*ma,const float*e,int skip,int mask){event_t*r=event(1);vec(r->v,s);vec(r->v+3,e);r->arg[0]=skip;r->arg[1]=mask;memset(out,0,sizeof(*out));out->fraction=mask==0x81?.6f:mask==0x38?(scenario%2?.4f:.8f):mask==0x10000?.3f:(scenario%3==0?1.f:.4f);VectorCopy(e,out->endpos);out->endpos[0]-=2;out->plane.normal[2]=1;out->surfaceFlags=0x14000000;out->entityNum=scenario%2?1022:9;++traceN;}
static void box(trace_t*out,const float*s,const float*e,const float*mi,const float*ma,int model,int mask){trace(out,s,mi,ma,e,model,mask);}
static int contents(const float*p,int skip){event_t*r=event(2);vec(r->v,p);r->arg[0]=skip;return (scenario%4==0?32:scenario%4==1?0:p[0]>10?32:0);}
static int muzzle(int source,float*out){event_t*r=event(3);r->arg[0]=source;VectorSet(out,1,2,3);return scenario%7!=0;}
static void bubble(float*s,float*e,float size,float spacing){event_t*r=event(4);vec(r->v,s);vec(r->v+3,e);r->v[6]=size;r->v[7]=spacing;}
static void wall(int w,int effect,vec3_t o,vec3_t normal,vec3_t d,unsigned flags,int force,int type){event_t*r=event(5);r->arg[0]=w;r->arg[1]=effect;r->arg[2]=flags;r->arg[3]=force;r->arg[4]=type;vec(r->v,o);vec(r->v+3,normal);vec(r->v+6,d);}
static int volume(vec3_t o,float v,float range,int linear){event_t*r=event(6);vec(r->v,o);r->v[3]=v;r->v[4]=range;r->arg[0]=linear;return scenario%5?93:0;}
static void sound(const float*o,int entity,int channel,int snd,int vol){event_t*r=event(7);vec(r->v,o);r->arg[0]=entity;r->arg[1]=channel;r->arg[2]=snd;r->arg[3]=vol;}
static void simpleOriginal(const float*p,float*c){event(8);VectorSet(c,.3f,.5f,.7f);}
static void simpleSource(const tce_lightGrid_t*g,const float*p,float*c){simpleOriginal(p,c);}
static void directedOriginal(const float*p,const float*normal,float*c){event_t*r=event(9);vec(r->v,p);vec(r->v+3,normal);VectorSet(c,.7f,.5f,.3f);}
static void directedSource(const tce_lightGrid_t*g,const float*p,const float*normal,float*c){directedOriginal(p,normal,c);}
static void bleed(float*p,int victim,float*c){event_t*r=event(10);vec(r->v,p);vec(r->v+3,c);r->arg[0]=victim;}
static int tag(void*cent,void*parent,char*name,int start,float*out,void*axis){event_t*r=event(11);r->arg[0]=start;require(!strcmp(name,"tag_mouth"),"tag name");VectorSet(out,19,13,45);return 1;}
static void particle(float*p,float*d,int count,int material,float alpha){event_t*r=event(12);vec(r->v,p);vec(r->v+3,d);r->arg[0]=count;r->arg[1]=material;r->v[6]=alpha;VectorNormalize(d);}
static void mark(int shader,float*p,float*normal,float angle,float red,float green,float blue,float alpha,int fade,float radius,int temp,int duration){event_t*r=event(13);vec(r->v,p);vec(r->v+3,normal);r->v[6]=angle;r->v[7]=red;r->v[8]=green;r->v[9]=blue;r->v[10]=alpha;r->v[11]=radius;r->arg[0]=shader;r->arg[1]=fade;r->arg[2]=temp;r->arg[3]=duration;}
int main(int argc,char**argv){HMODULE dll;byte*b;int k,j,i,en,erc,wlast;unsigned erng;vec3_t end,wantedEnd,normal={0,0,1},start={3,7,11};tce_bulletRendererContext_t ctx;
 void(__cdecl*original)(float*,int,float*,int,int,int,float,int,int,float*,int);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x7bd40);
 hook(b+0x885b6,rand);hook(b+0x58420,trace);hook(CG_Trace,trace);hook(b+0x64e60,box);hook(trap_CM_BoxTrace,box);hook(b+0x587d0,contents);hook(CG_PointContents,contents);hook(b+0x7b6f0,muzzle);hook(CG_CalcMuzzlePoint,muzzle);
 hook(b+0x2b2d0,bubble);hook(CG_BubbleTrail,bubble);hook(b+0x78260,wall);hook(b+0x44b50,volume);hook(b+0x64fe0,sound);hook(trap_S_StartSoundVControl,sound);
 hook(b+0x68160,simpleOriginal);hook(TCE_CG_LightForParticleSimple,simpleSource);hook(b+0x68200,directedOriginal);hook(TCE_CG_LightForParticleDirected,directedSource);hook(b+0x2be80,bleed);hook(CG_Bleed,bleed);hook(b+0x2c5a0,tag);hook(CG_GetOriginForTag,tag);hook(b+0x2a870,particle);hook(b+0x4bbd0,mark);
 memset(&ctx,0,sizeof(ctx));ctx.wall=wall;ctx.particle=particle;ctx.mark=mark;ctx.soundVolume=volume;ctx.lastBloodSpat=&lastBlood;
 cg.snap=&snapshot;*(void**)(b+0x40352c4)=rawSnap;*(int*)(rawSnap+0xcc)=snapshot.ps.clientNum=0;
 for(i=0;i<5;++i){*(int*)(b+0x2588b84+i*4)=cgs.media.sfx_bullet_fleshhit[i]=100+i;*(int*)(b+0x2588be8+i*4)=cgs.media.sfx_bullet_waterhit[i]=200+i;*(int*)(b+0x2588454+i*4)=cgs.media.bloodDotShaders[i]=300+i;}
 for(k=0;k<10000;++k){int damages[]={0,9,10,29,30,59,60,999,1000};int flesh=k%2,source=1,victim=2,other=3,damage=damages[(k/2)%9];scenario=k/18;testCase=k;
  cg.time=*(int*)(b+0x407dea4)=10000+k;cg_blood.integer=*(int*)(b+0x40132ec)=scenario%3!=0;cg_bloodTime.integer=*(int*)(b+0x401268c)=17;
  cg_tracerChance.value=*(float*)(b+0x400fbc8)=1;cg_predictBullets.integer=*(int*)(b+0x25761cc)=0;cgs.antilag=*(int*)(b+0x257e104)=0;
  *(int*)(rawSnap+0x16c)=snapshot.ps.persistant[PERS_HWEAPON_USE]=scenario%7==0;
  VectorSet(cg_entities[victim].currentState.pos.trBase,19,21,38);memcpy(b+0x488d818+victim*0xa94,cg_entities[victim].currentState.pos.trBase,12);
  VectorSet(cg_entities[victim].currentState.origin,17,9,25);memcpy(b+0x488d85c+victim*0xa94,cg_entities[victim].currentState.origin,12);
  *(int*)(b+0x488d808+source*0xa94)=cg_entities[source].currentState.eFlags=0;
  *(float*)(b+0x40a49cc)=ctx.attenuation=.2f;
  /* The reference's failed-muzzle flesh start is undefined; cover its gate
     with nonflesh cases, and all defined flesh paths with a successful muzzle. */
  if(flesh&&scenario%7==0)++scenario;
  VectorSet(end,21,17,37);VectorCopy(end,wantedEnd);n=randCalls=traceN=0;rng=k;*(int*)(b+0x15c0e60)=-77;
  original(wantedEnd,source,normal,flesh,victim,other,0,123,damage,start,1);en=n;erc=randCalls;erng=rng;wlast=*(int*)(b+0x15c0e60);memcpy(expected,events,sizeof(expected));
  n=randCalls=traceN=0;rng=k;lastBlood=-77;TCE_CG_Bullet(end,source,normal,flesh,victim,other,0,123,damage,start,1,&ctx);
  if(n!=en||randCalls!=erc||rng!=erng||lastBlood!=wlast){printf("counts %d/%d RNG%d/%d last%d/%d\n",n,en,randCalls,erc,lastBlood,wlast);require(0,"effects count/RNG/state");}
  for(i=0;i<n;++i){if(events[i].type!=expected[i].type||memcmp(events[i].arg,expected[i].arg,sizeof(events[i].arg))){printf("event%d kind%d/%d\n",i,events[i].type,expected[i].type);for(j=0;j<8;++j)printf("arg%d %d/%d\n",j,events[i].arg[j],expected[i].arg[j]);require(0,"arguments");}
   for(j=0;j<30;++j)if(!_finite(events[i].v[j])||!_finite(expected[i].v[j])||fabs(events[i].v[j]-expected[i].v[j])>.002+fabs(expected[i].v[j])*.000004){printf("event%d type%d field%d %.9g/%.9g\n",i,events[i].type,j,events[i].v[j],expected[i].v[j]);require(0,"values");}}
  for(j=0;j<3;++j)require(fabs(end[j]-wantedEnd[j])<.001,"endpoint");
 }
 printf("10000 bullet controller original comparisons passed; undefined input vectors excluded\n");return 0;
}
