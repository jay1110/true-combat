#include "../game/tce_bullet.h"
#include <windows.h>
#include <stdio.h>
void QDECL Com_Printf(const char *fmt,...) {(void)fmt;}
void QDECL Com_Error(int level,const char *fmt,...) {(void)level;fputs(fmt,stderr);exit(2);}
int Q_vsnprintf(char *out,int size,const char *fmt,va_list args) {
    int r;
    #undef _vsnprintf
    r=_vsnprintf(out,size-1,fmt,args);out[size-1]=0;return r;
}

static unsigned char client[0x1700],targetClient[0x1700];
static int distanceValue,recorded,calls;
static void hook(unsigned char *p,void *fn) {
 DWORD old,tmp;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))abort();
 p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;
 FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);
}
static void traceHit(trace_t *tr,const float *start,const float *mins,const float *maxs,
                     const float *end,int pass,int mask) {
 memset(tr,0,sizeof(*tr));tr->fraction=0.5f;tr->entityNum=1;
 tr->endpos[0]=(float)distanceValue;tr->plane.normal[0]=-1;
}
static float damageHit(void *target,void *inflictor,void *attacker,float *dir,
                       float *point,int damage,int flags,int mod) {
 recorded=damage;++calls;return 0;
}
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base,*ent,*target;tce_weaponDef_t *defs;
 int (__cdecl *original)(void *,void *,float *,float *,float,int,int,int,int,int,int,int,int,int);
 int range,damage,dist,special,bbox,expected,n=0;
 const int ranges[]={-100,0,1,10,50,100,1000};
 const int damages[]={-1,0,1,10,50,100,125,126,149,150,151,300,1000};
 const int distances[]={0,1,100,239,240,241,479,480,481,1000,8192,16000};
 float start[3]={0},end[3]={8192,0,0};
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
 base=(unsigned char *)dll;original=(void *)(base+0x9d500);
 hook(base+0x8ae80,traceHit);hook(base+0x589c0,damageHit);
 ent=base+0x226e2a0;target=ent+0x57c;
 memset(ent,0,0x57c*2);*(void **)(ent+0x190)=client;
 *(void **)(target+0x190)=targetClient;*(int *)target=1;
 *(int *)(target+0x2cc)=1;*(char **)(target+0x1a4)="player";
 *(int *)(ent+0xe8)=8;*(int *)(client+0xa4)=8;
 *(int *)(base+0x7849cc)=0;
 defs=(void *)(base+0x24435c0);memset(defs,0,64*sizeof(*defs));
 defs[8].unknown_12c=100;
 for(range=0;range<7;++range)for(damage=0;damage<13;++damage)
 for(dist=0;dist<12;++dist)for(special=0;special<2;++special)for(bbox=0;bbox<2;++bbox) {
  defs[8].unknown_130=ranges[range];defs[8].unknown_1c0=special;
  *(int *)(base+0x7e208c)=bbox;distanceValue=distances[dist];calls=0;
  original(ent,ent,start,end,0,damages[damage],100,0,0,4,4,8,123,0);
  expected=TCE_BulletClientDamage(&defs[8],damages[damage],distanceValue,bbox);
  if(calls!=1 || recorded!=expected) {
   fprintf(stderr,"range%d damage%d dist%d special%d bbox%d: calls%d original%d recovered%d\n",
    ranges[range],damages[damage],distanceValue,special,bbox,calls,recorded,expected);return 1;
  }
  ++n;
 }
 printf("%d original Bullet_Fire_Extended client-damage argument comparisons matched\n",n);
 return 0;
}
