#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../cgame/tce_impact_particles.h"
#include "../cgame/tce_lightgrid.h"
#include <windows.h>
#include <stdio.h>
static unsigned rng;static int rc,om,n;static byte raw[64][448];static localEntity_t particles[64],wanted[64];
static float records[64][17],expected[64][17];
int rand(void){rng=rng*1664525u+1013904223u;++rc;return (rng>>16)&32767;}
static void lightOriginal(vec3_t p,vec3_t c){VectorSet(c,.3f,.7f,.9f);}
static void lightSource(const tce_lightGrid_t*g,const vec3_t p,vec3_t c){VectorSet(c,.3f,.7f,.9f);}
static localEntity_t *puff(const vec3_t p,const vec3_t v,float radius,float r,float g,float b,float a,float duration,int start,int fade,int flags,int shader){
 int i=n++;float*x=records[i];memcpy(x,p,12);memcpy(x+3,v,12);x[6]=radius;x[7]=r;x[8]=g;x[9]=b;x[10]=a;x[11]=duration;x[12]=start;x[13]=fade;x[14]=flags;x[15]=shader;
 /* Force both outcomes of the original's equality check following the callback. */
 if(om){*(int*)(raw[i]+12)=i%2?2:flags;return(void*)raw[i];}
 particles[i].leFlags=i%2?2:flags;return &particles[i];
}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void fromRaw(localEntity_t*out,byte*in){memset(out,0,sizeof(*out));memcpy(out,in,0x6c);memcpy((byte*)out+0x6c,in+0x70,offsetof(localEntity_t,tceFragment)-0x6c);out->tceGravity=*(float*)(in+0x6c);}
static int same(float a,float b){return _finite(a)&&_finite(b)&&fabs(a-b)<.001+fabs(b)*.000003;}
int main(int argc,char**argv){HMODULE dll;byte*b;int k,i,j,nwanted,rwanted;unsigned rngwanted;void(__cdecl*original)(float*,float*,int,int,float);tce_impactParticleMedia_t media={71,73,79};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x2a870);
 hook(b+0x885b6,rand);hook(b+0x68160,lightOriginal);hook(TCE_CG_LightForParticleSimple,lightSource);hook(b+0x2b570,puff);hook(CG_SmokePuff,puff);
 *(int*)(b+0x2588198)=media.blood;*(int*)(b+0x25885a4)=media.smoke3;*(int*)(b+0x25885a8)=media.smoke4;
 for(k=0;k<24000;++k){vec3_t p={3,-7,12},d={.3f,-.7f,.9f},w;int count=(k/48)%8-1,material=k%48;float alpha=(k%21)*.05f;cg.time=*(int*)(b+0x407dea4)=10000+k;
  VectorCopy(d,w);memset(raw,0,sizeof(raw));memset(particles,0,sizeof(particles));memset(records,0,sizeof(records));om=1;n=rc=0;rng=k;
  original(p,w,count,material,alpha);nwanted=n;rwanted=rc;rngwanted=rng;memcpy(expected,records,sizeof(expected));for(i=0;i<n;++i)fromRaw(&wanted[i],raw[i]);
  om=0;n=rc=0;rng=k;TCE_CG_ParticleTest(p,d,count,material,alpha,&media);
  if(n!=nwanted||rc!=rwanted||rng!=rngwanted){printf("case%d count%d/%d RNG%d/%d\n",k,n,nwanted,rc,rwanted);return 1;}
  for(j=0;j<3;++j)if(!same(d[j],w[j]))return 3;
  for(i=0;i<n;++i){for(j=0;j<17;++j)if(!same(records[i][j],expected[i][j])){printf("case%d particle%d arg%d %.9g/%.9g\n",k,i,j,records[i][j],expected[i][j]);return 1;}
   if(!same(particles[i].angles.trBase[0],wanted[i].angles.trBase[0])||!same(particles[i].angles.trDelta[0],wanted[i].angles.trDelta[0])||!same(particles[i].tceGravity,wanted[i].tceGravity)){printf("case%d particle%d angles %.9g/%.9g delta %.9g/%.9g gravity %.9g/%.9g\n",k,i,particles[i].angles.trBase[0],wanted[i].angles.trBase[0],particles[i].angles.trDelta[0],wanted[i].angles.trDelta[0],particles[i].tceGravity,wanted[i].tceGravity);return 1;}
   particles[i].angles.trBase[0]=wanted[i].angles.trBase[0];particles[i].angles.trDelta[0]=wanted[i].angles.trDelta[0];particles[i].tceGravity=wanted[i].tceGravity;
   if(memcmp(&particles[i],&wanted[i],sizeof(localEntity_t))){for(j=0;j<sizeof(localEntity_t);++j)if(((byte*)&particles[i])[j]!=((byte*)&wanted[i])[j]){printf("case%d particle%d state%x %x/%x\n",k,i,j,((byte*)&particles[i])[j],((byte*)&wanted[i])[j]);break;}return 1;}
  }
 }
 printf("24000 complete material-impact emitter original comparisons passed\n");return 0;
}
