/* Probe the real Windows G_Damage return contract. No SDK adapter is substituted. */
#define WIN32_LEAN_AND_MEAN
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static unsigned char *base,*entity,*part;static unsigned char client[0x1700];
static int traceMode,traceCalls;
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void *spawn(void){memset(part,0,0x57c);*(int *)part=70;return part;}
static void nop(void *p,...){(void)p;}
static int sameTeam(void *a,void *b){return a==b;}
static void trace(trace_t *tr,const float *start,const float *mins,const float *maxs,const float *end,int pass,int mask) {
 ++traceCalls;memset(tr,0,sizeof(*tr));tr->fraction=traceMode?.5f:1.0f;tr->entityNum=traceMode?70:1022;memcpy(tr->endpos,end,sizeof(vec3_t));
}
static void setup(int hasClient,int canDamage) {
 memset(entity,0,0x57c);memset(client,0,sizeof(client));
 *(int *)(entity+0x194)=1;*(int *)(entity+0x2c8)=100;*(int *)(entity+0x2cc)=canDamage;
 *(void **)(entity+0x190)=hasClient?client:NULL;*(void **)(entity+0x2b8)=nop;
 *(int *)(client+0xb30)=1;*(int *)(client+0xa4)=39;*(int *)(client+0xbc)=40;
 *(int *)(client+0xd0)=100;*(int *)(client+0x1334)=-10000;
 *(float *)(entity+0x140)=14;*(float *)(entity+0x144)=14;*(float *)(entity+0x148)=40;
 *(int *)(base+0x7febe0)=10000;*(int *)(base+0x7ffc5c)=0;*(int *)(base+0x784f6c)=0;
 *(int *)(base+0x7852cc)=0;*(int *)(base+0x7849cc)=0;*(int *)(base+0x6d8d4c)=0;
 traceCalls=0;
}
int main(int argc,char **argv) {
 HMODULE dll;float (__cdecl *damage)(void *,void *,void *,float *,float *,int,unsigned,int);
 int i,y,z,take,cases=0,fractional=0;float result,expected,dir[3]={1,0,0},point[3];
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)dll;entity=base+0x226e2a0;part=entity+70*0x57c;damage=(void *)(base+0x589c0);
 patch(base+0x94e30,spawn);patch(base+0x94f30,nop);patch(base+0x8afa0,nop);patch(base+0x8ae80,trace);
 patch(base+0x8daa0,sameTeam);patch(base+0x7d4b0,nop);patch(base+0x21780,nop);patch(base+0x65470,nop);
 for(i=0;i<2;++i){setup(i,0);result=damage(entity,entity,entity,NULL,NULL,20,0x20,0);if(result!=0)return 3;++cases;}
 for(i=1;i<40;++i){setup(1,1);result=damage(entity,entity,entity,NULL,NULL,i,0x20,0);if(fabs(result-.1f)>1e-6f){printf("nonbullet return %g damage%d\n",result,i);return 4;}++cases;}
 setup(0,1);result=damage(entity,entity,entity,NULL,NULL,20,0x20,0);if(result!=0)return 5;++cases;
 traceMode=0;setup(1,1);point[0]=-100;point[1]=0;point[2]=32;
 result=damage(entity,entity,entity,dir,point,20,0x20,7);if(result!=1||traceCalls!=3){printf("body miss return%g trace%d\n",result,traceCalls);return 6;}++cases;
 traceMode=1;
 for(y=-24;y<=24;y+=2)for(z=-20;z<=60;z+=2) {
  setup(1,1);point[0]=-100;point[1]=(float)y;point[2]=(float)z;
  result=damage(entity,entity,entity,dir,point,15,0x20,7);take=*(int *)(client+0x131c);
  if(result==1 && take==0){++cases;continue;}
  expected=1.0f-(float)take/15.0f;if(expected<.1f)expected=.1f;
  if(fabs(result-expected)>1e-6f){printf("fraction mismatch y%d z%d return%g expected%g take%d\n",y,z,result,expected,take);return 7;}
  if(result>.1f && result<1)++fractional;
  ++cases;
 }
 if(!fractional){printf("No fractional body outcome exercised\n");return 8;}
 printf("PASS %d original G_Damage probes; %d fractional returns plus0/1/minimum0.1 paths\n",cases,fractional);return 0;
}
