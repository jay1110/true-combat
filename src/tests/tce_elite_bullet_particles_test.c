#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_particles.c"
#include <windows.h>
#include <stdio.h>
#include <stddef.h>
extern void CG_EliteAddBulletParticles(vec3_t,vec3_t,int,int,int,float);
typedef struct {vec3_t p,v;int duration;} emission_t;
static emission_t events[32],expected[32];static unsigned rng;static int calls,n;
int rand(void){rng=rng*1664525u+1013904223u;++calls;return(rng>>16)&32767;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void capture(vec3_t p,vec3_t v,int duration){emission_t*e=&events[n++];if(n>32)exit(9);VectorCopy(p,e->p);VectorCopy(v,e->v);e->duration=duration;}
int main(int argc,char**argv){HMODULE dll;byte*b;int k,j,i,en,ec;unsigned er;cparticle_t raw,source;void(__cdecl*core)(float*,float*,int);void(__cdecl*emitter)(float*,float*,int,int,int,float);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;core=(void*)(b+0x51cb0);emitter=(void*)(b+0x77330);if(offsetof(cparticle_t,tceFadeInEnd)!=0x88||sizeof(cparticle_t)!=0x8c){puts("particle layout mismatch");return 3;}
 for(k=0;k<40000;++k){vec3_t p={k%73-36.f,k%107-53.f,k%149-74.f},v={k%97-48.f,k%31-15.f,k%13-6.f};int duration=k%8191-71;
  memset(&raw,0x5a,sizeof(raw));source=raw;raw.next=source.next=NULL;
  cg.time=*(int*)(b+0x407dea4)=10000+k*531;cgs.media.tceGlowSparkShader=*(int*)(b+0x2588270)=193;
  free_particles=k%7?&source:NULL;active_particles=NULL;*(void**)(b+0x25636bc)=k%7?&raw:NULL;*(void**)(b+0x25636cc)=NULL;
  core(p,v,duration);CG_ParticleBulletDebris(p,v,duration);
  /* The reference debris writer touches the original 0x88-byte prefix.
     The added smoke fade field must be reset on reuse, or untouched on exhaustion. */
  if(memcmp(&raw,&source,offsetof(cparticle_t,tceFadeInEnd))||(k%7?source.tceFadeInEnd!=0:memcmp(&source.tceFadeInEnd,&raw.tceFadeInEnd,sizeof(float)))||!!active_particles!=!!*(void**)(b+0x25636cc)||!!free_particles!=!!*(void**)(b+0x25636bc)){printf("bullet particle pool case%d\n",k);return 1;}
 }
 hook(b+0x51cb0,capture);hook(CG_ParticleBulletDebris,capture);hook(b+0x885b6,rand);
 for(k=0;k<30000;++k){vec3_t p={k%73-36.f,k%107-53.f,k%149-74.f},d={(k%9-4)*.25f,(k%11-5)*.25f,(k%13-6)*.125f};int speed=k%1401-100,duration=k%8191-71,count=k%32-2;float scale=(k%11-5)*.25f;
  n=calls=0;rng=k;emitter(p,d,speed,duration,count,scale);en=n;ec=calls;er=rng;memcpy(expected,events,sizeof(events));
  n=calls=0;rng=k;CG_EliteAddBulletParticles(p,d,speed,duration,count,scale);
  if(n!=en||calls!=ec||rng!=er){printf("elite emitter case%d count%d/%d RNG%d/%d\n",k,n,en,calls,ec);return 2;}
  for(i=0;i<n;++i){if(events[i].duration!=expected[i].duration||memcmp(events[i].p,expected[i].p,12)){printf("elite emitter case%d arg\n",k);return 4;}
   for(j=0;j<3;++j)if(fabs(events[i].v[j]-expected[i].v[j])>.0002+fabs(expected[i].v[j])*.000004){printf("elite emitter%d event%d vel%d %.9g/%.9g\n",k,i,j,events[i].v[j],expected[i].v[j]);return 5;}}
 }
 puts("40000 complete bullet-particle pool states and 30000 elite emitter original comparisons passed");return 0;
}
