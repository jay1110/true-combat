#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
static unsigned rng;static int calls,mode,allocated,testCase;static byte raw[256][448];static localEntity_t source[256],expected[256];
int rand(void){rng=rng*1664525u+1013904223u;++calls;return(rng>>16)&32767;}
static localEntity_t *allocate(void){int i=allocated++;if(i>=256)exit(8);return mode?(void*)raw[i]:&source[i];}
static int shader(void){++calls;return 193;}
static void hook(void *at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void translate(localEntity_t*out,byte*in){memset(out,0,sizeof(*out));memcpy(out,in,0x6c);memcpy((byte*)out+0x6c,in+0x70,offsetof(localEntity_t,tceFragment)-0x6c);out->tceGravity=*(float*)(in+0x6c);}
static int same(float a,float b){return _finite(a)&&_finite(b)&&fabs(a-b)<=.00003+fabs(b)*.000002;}
static void compare(localEntity_t*a,localEntity_t*b){int i;float *x[]={&a->refEntity.rotation,&a->refEntity.shaderTime,&a->lifeRate,a->pos.trDelta,a->pos.trDelta+1,a->pos.trDelta+2,a->pos.trBase,a->pos.trBase+1,a->pos.trBase+2};float *y[]={&b->refEntity.rotation,&b->refEntity.shaderTime,&b->lifeRate,b->pos.trDelta,b->pos.trDelta+1,b->pos.trDelta+2,b->pos.trBase,b->pos.trBase+1,b->pos.trBase+2};
 for(i=0;i<9;++i){if(!same(*x[i],*y[i])){printf("case%d float%d %.9g/%.9g\n",testCase,i,*x[i],*y[i]);exit(1);}*x[i]=*y[i];}
 if(memcmp(a,b,sizeof(*a))){for(i=0;i<sizeof(*a);++i)if(((byte*)a)[i]!=((byte*)b)[i]){printf("case%d byte%x %x/%x\n",testCase,i,((byte*)a)[i],((byte*)b)[i]);break;}exit(1);}
}
static void reset(int original,int seed){mode=original;allocated=calls=0;rng=seed;memset(raw,0,sizeof(raw));memset(source,0,sizeof(source));}
int main(int argc,char**argv){HMODULE dll;byte*b;int k,i,n,rc;unsigned r;void(__cdecl*bubble)(float*,float*,float,float);void*(__cdecl*puff)(const float*,const float*,float,float,float,float,float,float,int,int,int,int);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;bubble=(void*)(b+0x2b2d0);puff=(void*)(b+0x2b570);
 hook(b+0x885b6,rand);hook(b+0x44900,allocate);hook(CG_AllocLocalEntity,allocate);hook(b+0x2bc40,shader);hook(getTestShader,shader);
 cgs.media.waterBubbleShader=*(int*)(b+0x2588194)=131;cgs.media.smokePuffRageProShader=*(int*)(b+0x258818c)=137;
 for(k=0;k<20000;++k){vec3_t start={2.5f,-7.25f,39},end={k%41-20.f,k%113-56.f,k%79-39.f};float spacing=1+k%23+(k%4)*.25f,size=(k%41)*.03f;testCase=k;cg.time=*(int*)(b+0x407dea4)=10000+k;
 reset(1,k);bubble(start,end,size,spacing);n=allocated;rc=calls;r=rng;for(i=0;i<n;++i)translate(&expected[i],raw[i]);
 reset(0,k);CG_BubbleTrail(start,end,size,spacing);if(n!=allocated||rc!=calls||r!=rng){printf("case%d counts%d/%d random%d/%d\n",k,allocated,n,calls,rc);return 1;}for(i=0;i<n;++i)compare(&source[i],&expected[i]);
 }
 for(k=0;k<60000;++k){vec3_t p={3,-7,11},v={.3f,.7f,-.2f};float radius=(k%41)*.13f,red=(k%51-10)*.025f,green=(k%37)*.037f,blue=(k%23)*.053f,alpha=(k%29)*.037f,duration=500+(k%137)*.37f;int start=10000+k,fade=start+(k%3==0?100:-100),flags=k%512;testCase=20000+k;
 cgs.glconfig.hardwareType=*(int*)(b+0x257e09c)=k%7;cg_fxflags=*(char*)(b+0x18531a0)=k%4;
 reset(1,k);puff(p,v,radius,red,green,blue,alpha,duration,start,fade,flags,149);n=allocated;rc=calls;translate(&expected[0],raw[0]);
 reset(0,k);CG_SmokePuff(p,v,radius,red,green,blue,alpha,duration,start,fade,flags,149);if(n!=1||allocated!=1||rc!=calls)return 3;compare(&source[0],&expected[0]);
 }
 printf("80000 complete bubble-trail and smoke-puff original comparisons passed\n");return 0;
}
