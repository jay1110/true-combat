#include "../game/g_local.h"
#include "../game/tce_bullet.h"
#include <windows.h>
#include <stdio.h>

static unsigned char client[0x1700],temporary[0x57c],saved[5],*entry;
static unsigned entryFlags,exitFlags;
static vec3_t recordedEnd;
static int thickness,traces,calls,recordedEnergy,recordedDamage;
static void patch(void *p,const void *bytes) {
 DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);
 memcpy(p,bytes,5);FlushInstructionCache(GetCurrentProcess(),p,5);
 VirtualProtect(p,5,old,&tmp);
}
static void hook(unsigned char *p,void *fn) {
 unsigned char bytes[5];bytes[0]=0xe9;*(int *)(bytes+1)=(unsigned char *)fn-p-5;
 patch(p,bytes);
}
static int recursive(void *source,void *attacker,float *start,float *end,float spread,
 int damage,int energy,int p8,int p9,int walls,int p11,int p12,int seed,int p14) {
 VectorCopy(end,recordedEnd);++calls;recordedEnergy=energy;recordedDamage=damage;
 if(walls!=3)abort();return 42;
}
static void traceWall(trace_t *tr,const float *start,const float *mins,const float *maxs,
 const float *end,int pass,int mask) {
 memset(tr,0,sizeof(*tr));tr->fraction=0.5f;tr->entityNum=1;
 ++traces;
 if(traces==1) {hook(entry,recursive);tr->endpos[0]=100;tr->plane.normal[0]=-1;tr->surfaceFlags=entryFlags;}
 else if(traces==2) {tr->endpos[0]=1000;tr->plane.normal[0]=-1;}
 else if(traces==3) {tr->endpos[0]=(float)(100+thickness);tr->plane.normal[0]=1;tr->surfaceFlags=exitFlags;}
 else abort();
}
static void *tempEntity(float *origin,int event) {memset(temporary,0,sizeof(temporary));return temporary;}
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base,*ent,*target;tce_weaponDef_t *defs;
 int (__cdecl *original)(void *,void *,float *,float *,float,int,int,int,int,int,int,int,int,int);
 const int energies[]={1,5,50,100,200,205,250,1000,2000,10000,30000};
 const int widths[]={1,4,8,16,32,100,240};
 const char *calibers[]={"9MM","338LAPUA","50BMG","338lapua"};
 int a,b,e,w,c,blocked,expected,outEnergy,outDamage,n=0,passed=0;
 unsigned random=17;int k,j;
 void (__cdecl *originalSnap)(float *,float *);
 vec3_t v,recovered,to;
 float start[3]={0},end[3];
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
 base=(unsigned char *)dll;entry=base+0x9d500;original=(void *)entry;memcpy(saved,entry,5);
 originalSnap=(void *)(base+0x9cef0);
 for(k=0;k<100000;++k) {
  for(j=0;j<3;++j) {
   random=random*1664525u+1013904223u;
   v[j]=((int)(random&0xffffff)-0x800000)/256.0f;
   to[j]=v[j]+(k%3-1)*10.0f;
  }
  VectorCopy(v,recovered);originalSnap(v,to);SnapVectorTowards(recovered,to);
  if(memcmp(v,recovered,sizeof(v))) {fputs("SnapVectorTowards mismatch\n",stderr);return 1;}
 }
 puts("100000 original SnapVectorTowards comparisons matched");
 hook(base+0x8ae80,traceWall);hook(base+0x94fa0,tempEntity);
 ent=base+0x226e2a0;target=ent+0x57c;memset(ent,0,0x57c*2);
 *(void **)(ent+0x190)=client;*(int *)(client+0xa4)=8;*(int *)(ent+0xe8)=8;
 *(int *)target=1;*(char **)(target+0x1a4)="worldspawn";
 *(int *)(base+0x7849cc)=0;*(int *)(base+0x7e208c)=0;*(float *)(base+0x66bba0)=1;
 defs=(void *)(base+0x24435c0);memset(defs,0,64*sizeof(*defs));defs[8].unknown_134=10000;
 for(a=0;a<36;++a)for(b=0;b<36;++b)for(c=0;c<4;++c)
 for(e=0;e<11;++e)for(w=0;w<7;++w)for(blocked=0;blocked<2;++blocked) {
  entryFlags=((unsigned)a<<24)|(blocked?0x80000:0);exitFlags=(unsigned)b<<24;
  thickness=widths[w];strcpy(defs[8].caliberClass,calibers[c]);calls=traces=0;
  defs[8].unknown_134=(b&1)?3333:10000;
  end[0]=8192;end[1]=(float)(b*12);end[2]=(float)(c*15);patch(entry,saved);
  original(ent,ent,start,end,0,137,energies[e],0,0,4,4,8,(unsigned)n*1664525u+17u,0);
  expected=TCE_BulletMaterialLoss(&defs[8],entryFlags,exitFlags,thickness,energies[e],137,&outEnergy,&outDamage);
  if(calls!=expected || (calls && (recordedEnergy!=outEnergy || recordedDamage!=outDamage))) {
   fprintf(stderr,"a%d b%d caliber%s e%d w%d block%d traces%d: calls%d/%d energy%d/%d damage%d/%d\n",
    a,b,calibers[c],energies[e],thickness,blocked,traces,calls,expected,recordedEnergy,outEnergy,recordedDamage,outDamage);return 1;
  }
  if(calls) {
   const tce_pierceMaterial_t *m=&tcePierceTable[BG_SurfaceFlag2Type(entryFlags)];
   const tce_pierceMaterial_t *x=&tcePierceTable[BG_SurfaceFlag2Type(exitFlags)];
   float fraction;vec3_t recoveredEnd={8192,0,0};
   recoveredEnd[1]=(float)(b*12);recoveredEnd[2]=(float)(c*15);
   if(x->resistance>=m->resistance)m=x;
   fraction=(float)((energies[e]-(thickness/(double)m->thicknessScale+1)*m->resistance)/energies[e]);
   TCE_BulletDeflect(start,recoveredEnd,fraction,(unsigned)n*1664525u+17u,4);
   for(j=0;j<3;++j)if(fabs(recoveredEnd[j]-recordedEnd[j])>0.002) {
    fprintf(stderr,"deflection axis%d original%g recovered%g\n",j,recordedEnd[j],recoveredEnd[j]);return 1;
   }
  }
  ++n;passed+=calls;
 }
 patch(entry,saved);
 printf("%d original wall-energy comparisons matched; %d recursive penetrations\n",n,passed);return 0;
}
