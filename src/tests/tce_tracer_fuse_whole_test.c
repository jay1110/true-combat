#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../game/tce_bg.h"
#include "../game/tce_trajectory.h"
#include <windows.h>
#include <stdio.h>
extern void CG_AddMovingTracer(localEntity_t*);
extern void CG_AddFuseSparkElements(localEntity_t*);
static byte raw[448];static localEntity_t le;static int run,n;static int events[128][32];static float sample[3];
static void evaluate(const trajectory_t*t,int time,vec3_t result,qboolean angle,int spline,float scale){int*p=events[n++];p[0]=1;p[1]=time;p[2]=angle;p[3]=spline;memcpy(p+4,&scale,4);p[5]=t==(run?(void*)(raw+32):&le.pos);VectorCopy(sample,result);}
static vec_t normalize(const vec3_t in,vec3_t out){int*p=events[n++];p[0]=2;memcpy(p+1,in,12);VectorCopy(in,out);return 1.f;}
static void tracer(vec3_t start,vec3_t end){int*p=events[n++];p[0]=3;memcpy(p+1,start,12);memcpy(p+4,end,12);}
static int trail(int head,void*owner,qhandle_t shader,int time,int type,vec3_t pos,int life,float a,float b,float c,float d,int flags,vec3_t ca,vec3_t cb,float ratio,float speed){int*p=events[n++];p[0]=4;p[1]=head;p[2]=owner==(run?(void*)raw:&le);p[3]=shader;p[4]=time;p[5]=type;memcpy(p+6,pos,12);p[9]=life;memcpy(p+10,&a,4);memcpy(p+11,&b,4);memcpy(p+12,&c,4);memcpy(p+13,&d,4);p[14]=flags;memcpy(p+15,ca,12);memcpy(p+18,cb,12);memcpy(p+21,&ratio,4);memcpy(p+22,&speed,4);return head+7;}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE m;byte*b;void(__cdecl*original[2])(void*);int k,op,j,count,copy[128][32];
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;original[0]=(void*)(b+0x45a80);original[1]=(void*)(b+0x45c90);hook(b+0x43f0,evaluate);hook(TCE_BG_EvaluateTrajectory,evaluate);hook(b+0x7e5c0,normalize);hook(VectorNormalize2,normalize);hook(b+0x7b3c0,tracer);hook(CG_DrawTracer,tracer);hook(b+0x65c60,trail);hook(CG_AddTrailJunc,trail);
 for(k=0;k<4096;k++)for(op=0;op<2;op++){
  memset(&le,0,sizeof(le));memset(raw,0,sizeof(raw));le.startTime=*(int*)(raw+16)=k%37-100;le.endTime=*(int*)(raw+20)=le.startTime+1+(k%777);le.lastTrailTime=*(int*)(raw+0x18c)=le.startTime+k%91;le.headJuncIndex=*(int*)(raw+400)=k%11;
  for(j=0;j<3;j++){sample[j]=(k%71-35)*.13f+j*.27f;le.pos.trDelta[j]=(k%19-9)*.037f+j*.17f;}memcpy(raw+0x20,&le.pos,sizeof(le.pos));cg.time=*(int*)(b+0x407dea4)=le.lastTrailTime+(k%81)-11;cg_tracerLength.value=*(float*)(b+0x4011488)=(k%11-2)*13.7f;cgs.media.sparkParticleShader=*(int*)(b+0x25884e8)=k+70;
  run=1;n=0;memset(events,0,sizeof(events));original[op](raw);count=n;memcpy(copy,events,sizeof(copy));run=0;n=0;memset(events,0,sizeof(events));if(op)CG_AddFuseSparkElements(&le);else CG_AddMovingTracer(&le);
  if(count!=n||memcmp(copy,events,sizeof(copy))||le.lastTrailTime!=*(int*)(raw+0x18c)||le.headJuncIndex!=*(int*)(raw+400)||memcmp(le.refEntity.origin,raw+0x104,12)){printf("tracer/fuse case%d op%d calls%d/%d\n",k,op,count,n);for(j=0;j<sizeof(copy)/4;j++)if(((int*)copy)[j]!=((int*)events)[j]){printf("word%d expected%08x actual%08x\n",j,((int*)copy)[j],((int*)events)[j]);break;}return 1;}
 }puts("8192 whole moving-tracer/fuse comparisons passed (4096 each)");return 0;}
