#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include "../game/tce_trajectory.h"
#include <windows.h>
#include <stdio.h>
static byte raw[0x57c];
static gentity_t entity, result;
static gitem_t definition;
static int mode,randomValue,randomCalls,launchCalls,deltaCalls,bad,timeSeen;
static vec3_t velocitySeen,originSeen,deltaInput;
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static int fixedRandom(void){++randomCalls;return randomValue;}
static gentity_t *launch(gitem_t*d,vec3_t o,vec3_t v,int owner){++launchCalls;if(d!=&definition||owner!=entity.s.number)bad=1;VectorCopy(o,originSeen);VectorCopy(v,velocitySeen);return &result;}
static void delta(const trajectory_t*t,int when,vec3_t out,qboolean angle,int spline,float gravity){++deltaCalls;timeSeen=when;if((const void*)t!=(mode?(void*)(raw+12):(void*)&entity.s.pos)||angle||spline!=entity.s.effect2Time||gravity!=1)bad=1;VectorCopy(deltaInput,out);}
static int same(const vec3_t a,const vec3_t b){int i;for(i=0;i<3;++i)if(!_finite(a[i])||!_finite(b[i])||fabs(a[i]-b[i])>0.00003+fabs(b[i])*0.000002)return 0;return 1;}
extern void G_BounceItem(gentity_t*,trace_t*);
int main(int argc,char**argv){HMODULE dll;byte*b;gentity_t*(__cdecl*drop)(void*,gitem_t*,float,int);void(__cdecl*bounce)(void*,trace_t*);int k,novel,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;drop=(void*)(b+0x5e330);bounce=(void*)(b+0x5e7d0);
 if(!hook(b+0xa20e4,fixedRandom)||!hook(rand,fixedRandom)||!hook(b+0x5df70,launch)||!hook(LaunchItem,launch)||!hook(b+0x2ce80,delta)||!hook(TCE_BG_EvaluateTrajectoryDelta,delta))return 2;
 for(novel=0;novel<3;++novel)for(k=0;k<4096;++k){vec3_t expected;float angle=(float)(k%181-90);int expectedRandom;
  memset(raw,0,sizeof(raw));memset(&entity,0,sizeof(entity));entity.s.number=*(int*)raw=k%64;
  VectorSet(entity.s.apos.trBase,k%181-90.f,k%721-360.f,k%89-44.f);memcpy(raw+0x3c,entity.s.apos.trBase,12);
  VectorSet(entity.s.pos.trBase,k%29-14.f,k%53-26.f,k%71-35.f);memcpy(raw+0x18,entity.s.pos.trBase,12);
  randomValue=k%4==0?0:k%4==1?32767:k%4==2?12345:65535;
  randomCalls=launchCalls=bad=0;mode=1;if(drop(raw,&definition,angle,novel)!=&result||bad||launchCalls!=1||!same(originSeen,entity.s.pos.trBase))return 3;expectedRandom=randomCalls;VectorCopy(velocitySeen,expected);
  randomCalls=launchCalls=bad=0;mode=0;if(Drop_Item(&entity,&definition,angle,novel)!=&result||bad||launchCalls!=1||randomCalls!=expectedRandom||!same(originSeen,entity.s.pos.trBase)||!same(velocitySeen,expected)){fprintf(stderr,"drop case%d noVelocity%d mismatch\n",k,novel);return 1;}++n;
 }
 printf("%d whole Drop_Item comparisons passed\n",n);n=0;
 for(k=0;k<12000;++k){trace_t originalTrace,sourceTrace;int expectedTime;const float z[]={-1,0,0.001f,0.5f,1};const float speed[]={-200,-40,0,39.999f,40,40.001f,200,800};
  memset(raw,0,sizeof(raw));memset(&entity,0,sizeof(entity));memset(&sourceTrace,0,sizeof(sourceTrace));
  entity.s.pos.trType=*(int*)(raw+12)=TR_GRAVITY;entity.s.pos.trTime=*(int*)(raw+16)=-123;
  entity.s.groundEntityNum=*(int*)(raw+0x94)=ENTITYNUM_NONE;
  entity.s.effect2Time=*(int*)(raw+0x110)=k%19-9;
  entity.physicsBounce=*(float*)(raw+0x1d0)=(k%5)*0.25f;
  VectorSet(entity.r.currentOrigin,k%31+0.25f,k%47-23.5f,k%17+0.75f);memcpy(raw+0x168,entity.r.currentOrigin,12);
  level.time=*(int*)(b+0x7febe0)=100000+k;level.previousTime=*(int*)(b+0x7febe4)=level.time-(k%101);
  sourceTrace.fraction=(k%11)*0.1f;VectorSet(sourceTrace.plane.normal,(k%3-1)*0.5f,(k%7-3)*0.25f,z[k%5]);VectorSet(sourceTrace.endpos,k%13-6.25f,k%17-8.75f,k%19-9.5f);sourceTrace.entityNum=k%1024;
  VectorSet(deltaInput,(k%23-11)*10.f,(k%29-14)*10.f,speed[k%8]);originalTrace=sourceTrace;
  deltaCalls=bad=0;mode=1;bounce(raw,&originalTrace);expectedTime=timeSeen;if(bad||deltaCalls!=1)return 4;
  deltaCalls=bad=0;mode=0;G_BounceItem(&entity,&sourceTrace);
  if(bad||deltaCalls!=1||timeSeen!=expectedTime||entity.s.pos.trType!=*(int*)(raw+12)||entity.s.pos.trTime!=*(int*)(raw+16)||entity.s.groundEntityNum!=*(int*)(raw+0x94)||!same(entity.s.pos.trBase,(float*)(raw+0x18))||!same(entity.s.pos.trDelta,(float*)(raw+0x24))||!same(entity.r.currentOrigin,(float*)(raw+0x168))||memcmp(&sourceTrace,&originalTrace,sizeof(trace_t))){fprintf(stderr,"bounce case%d mismatch time%d/%d\n",k,timeSeen,expectedTime);return 1;}++n;
 }
 printf("%d whole G_BounceItem controller comparisons passed\n",n);return 0;
}
