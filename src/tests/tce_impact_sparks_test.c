#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../cgame/tce_impact_sparks.h"
#include <windows.h>
#include <stdio.h>
extern void CG_FreeLocalEntity(localEntity_t *);
static unsigned rng;static int calls,mode,allocated,caseNo,collision,polys,freed,reflected;static byte raw[32][448];static localEntity_t entities[32],expected[32];static polyVert_t captured[4];static int capturedShader;static char events[512];
int rand(void){rng=rng*1664525u+1013904223u;calls++;return(rng>>16)&32767;}
static localEntity_t*allocate(void){int i=allocated++;if(i>=32)exit(9);return mode?(void*)raw[i]:&entities[i];}
static void hook(void*a,void*f){byte*p=a;DWORD o,t;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int*)(p+1)=(byte*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void translate(localEntity_t*out,byte*in){memset(out,0,sizeof(*out));memcpy(out,in,0x6c);memcpy((byte*)out+0x6c,in+0x70,offsetof(localEntity_t,tceFragment)-0x6c);out->tceGravity=*(float*)(in+0x6c);}
static void rawify(byte*out,localEntity_t*in){memset(out,0,448);memcpy(out,in,0x6c);memcpy(out+0x70,(byte*)in+0x6c,offsetof(localEntity_t,tceFragment)-0x6c);*(float*)(out+0x6c)=in->tceGravity;}
static int same(float a,float b){return _finite(a)&&_finite(b)&&fabs(a-b)<=.00004+fabs(b)*.000004;}
static void compare(localEntity_t*a,localEntity_t*b){int i;float*x[]={&a->bounceFactor,&a->lifeRate,a->color,a->color+1,a->color+2,a->color+3,a->pos.trDelta,a->pos.trDelta+1,a->pos.trDelta+2,a->refEntity.oldorigin,a->refEntity.oldorigin+1,a->refEntity.oldorigin+2,a->refEntity.origin,a->refEntity.origin+1,a->refEntity.origin+2};float*y[]={&b->bounceFactor,&b->lifeRate,b->color,b->color+1,b->color+2,b->color+3,b->pos.trDelta,b->pos.trDelta+1,b->pos.trDelta+2,b->refEntity.oldorigin,b->refEntity.oldorigin+1,b->refEntity.oldorigin+2,b->refEntity.origin,b->refEntity.origin+1,b->refEntity.origin+2};for(i=0;i<15;i++){if(!same(*x[i],*y[i])){printf("case%d float%d %.9g/%.9g\n",caseNo,i,*x[i],*y[i]);exit(3);}*x[i]=*y[i];}if(memcmp(a,b,sizeof(*a))){for(i=0;i<sizeof(*a);i++)if(((byte*)a)[i]!=((byte*)b)[i]){printf("case%d byte%x %x/%x\n",caseNo,i,((byte*)a)[i],((byte*)b)[i]);break;}exit(4);}}
static void reset(int original,int seed){mode=original;rng=seed;allocated=calls=0;memset(raw,0,sizeof(raw));memset(entities,0,sizeof(entities));events[0]=0;polys=freed=reflected=0;}
static void poly(qhandle_t s,int n,const polyVert_t*p){strcat(events,"poly;");capturedShader=s;polys++;if(n!=4)exit(7);memcpy(captured,p,sizeof(captured));}
static void evaluate(const trajectory_t*t,int time,vec3_t out,qboolean a,int b){strcat(events,"eval;");VectorSet(out,17,-23,45);}
static void tracefn(trace_t*t,const vec3_t start,const vec3_t mins,const vec3_t maxs,const vec3_t end,int skip,int mask){if(skip!=-1||mask!=CONTENTS_SOLID||mins||maxs)exit(10);sprintf(events+strlen(events),"trace:%.4f,%.4f,%.4f:%.4f,%.4f,%.4f;",start[0],start[1],start[2],end[0],end[1],end[2]);memset(t,0,sizeof(*t));t->fraction=collision?0.4f:1;VectorCopy(end,t->endpos);VectorSet(t->plane.normal,0,0,1);}
static int contents(const vec3_t p,int m){if(m!=0)exit(11);strcat(events,"contents;");return collision==2?CONTENTS_NODROP:0;}
static void release(localEntity_t*l){strcat(events,"free;");freed++;}
static void reflect(localEntity_t*l,trace_t*t){strcat(events,"reflect;");reflected++;l->pos.trType=TR_STATIONARY;}
int main(int argc,char**argv){HMODULE h;byte*b;int k,i,j,n,rc,kind,pass,ec,ef,er,es;unsigned r;polyVert_t ep[4];char ev[512];float intensity;
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(byte*)h;hook(b+0x885b6,rand);hook(b+0x44900,allocate);hook(CG_AllocLocalEntity,allocate);
 for(kind=0;kind<2;kind++)for(k=0;k<20000;k++){vec3_t origin={3,-7,19},dir={k%3-1.f,k%5-2.f,k%7-3.f};caseNo=kind*20000+k;cg.time=*(int*)(b+0x407dea4)=10000+k;intensity=*(float*)(b+0x40a4c04)=.1f+(k%20)*.1f;*(int*)(b+0x258826c)=131;*(int*)(b+0x2588270)=137;
 reset(1,k);((void(__cdecl*)(vec3_t,vec3_t,int))(b+(kind?0x2b9a0:0x2b730)))(origin,dir,k%10-1);n=allocated;rc=calls;r=rng;for(i=0;i<n;i++)translate(&expected[i],raw[i]);
 reset(0,k);if(kind)TCE_CG_GlowSparks(origin,dir,k%10-1,intensity,137);else TCE_CG_FlatSparks(origin,dir,k%10-1,intensity,131);if(n!=allocated||rc!=calls||r!=rng){printf("case%d alloc%d/%d rand%d/%d\n",caseNo,n,allocated,rc,calls);return 5;}for(i=0;i<n;i++)compare(&entities[i],&expected[i]);}
#define PATCH(off,fn,target) hook(b+off,fn);hook(target,fn)
 PATCH(0x652b0,poly,trap_R_AddPolyToScene);PATCH(0x58420,tracefn,CG_Trace);PATCH(0x64e20,contents,trap_CM_PointContents);PATCH(0x448b0,release,CG_FreeLocalEntity);PATCH(0x44f20,reflect,CG_ReflectVelocity);
 /* Both trajectory evaluators remain real. Trace/contents/reflect are callback boundaries. */
 for(kind=0;kind<2;kind++)for(k=0;k<10000;k++){
  caseNo=40000+kind*10000+k;collision=k%3;cg.time=*(int*)(b+0x407dea4)=10000+(k%120);cg.frametime=16;
  VectorSet(cg.refdef.vieworg,-13,7,61);memcpy(b+0x407f01c,cg.refdef.vieworg,12);AxisClear(cg.refdef.viewaxis);memcpy(b+0x407f028,cg.refdef.viewaxis,36);
  for(pass=0;pass<2;pass++){localEntity_t*l=&entities[0];reset(!pass,k);memset(l,0,sizeof(*l));l->leFlags=(k%2)*128;l->startTime=10000;l->endTime=10100;l->fadeInTime=10050;l->lifeRate=.02f;l->leType=(leType_t)(kind?15:14);l->refEntity.radius=.1f+(k%7)*.2f;l->refEntity.customShader=137;VectorSet(l->refEntity.origin,5,7,11);VectorSet(l->refEntity.oldorigin,13,17,23);for(j=0;j<4;j++)l->color[j]=.05f*(j+1)*(k%6);l->pos.trType=kind ? (k%4==0?TR_STATIONARY:k%4==1?TR_LINEAR:TR_GRAVITY_LOW) : TR_STATIONARY;l->pos.trTime=9990;VectorSet(l->pos.trBase,3,7,19);VectorSet(l->pos.trDelta,12,-23,37);
   if(!pass){rawify(raw[0],l);((void(__cdecl*)(void*))(b+(kind?0x46120:0x45da0)))(raw[0]);translate(&expected[0],raw[0]);memcpy(ep,captured,sizeof(ep));ec=polys;ef=freed;er=reflected;es=capturedShader;strcpy(ev,events);}
   else{if(kind)TCE_CG_AddGlowSpark(l);else TCE_CG_AddFlatSpark(l);compare(l,&expected[0]);if(ec!=polys||ef!=freed||er!=reflected||strcmp(ev,events)){printf("render callbacks case%d %s/%s\n",caseNo,ev,events);return 6;}if(polys){if(es!=capturedShader)return 7;for(i=0;i<4;i++){for(j=0;j<3;j++)if(!same(ep[i].xyz[j],captured[i].xyz[j])){printf("vertex case%d %d,%d %.9g/%.9g\n",caseNo,i,j,ep[i].xyz[j],captured[i].xyz[j]);return 8;}if(memcmp(ep[i].st,captured[i].st,8)||memcmp(ep[i].modulate,captured[i].modulate,4)){printf("color case%d %d %d,%d,%d,%d/%d,%d,%d,%d\n",caseNo,i,ep[i].modulate[0],ep[i].modulate[1],ep[i].modulate[2],ep[i].modulate[3],captured[i].modulate[0],captured[i].modulate[1],captured[i].modulate[2],captured[i].modulate[3]);return 9;}}}}
  }
 }
 printf("60000 whole spark producer/renderer comparisons passed, including movement, bounce and nodrop\n");return 0;
}
