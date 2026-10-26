#include "../game/g_local.h"
#include "../game/tce_bullet.h"
#include <windows.h>
#include <stdio.h>

static unsigned char client[0x1700],temporary[0x57c],saved[5],*entry;
static int traces,calls,recordedEnergy,recordedDamage;
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
 ++calls;recordedEnergy=energy;recordedDamage=damage;
 if(walls!=3)abort();return 42;
}
typedef struct {vec3_t start,end;int pass,mask;trace_t result;} traceCall_t;
static traceCall_t seen[5];
static int mode,replay,offset;
static void traceWall(trace_t *tr,const float *start,const float *mins,const float *maxs,
 const float *end,int pass,int mask) {
 int index=traces++,j;
 if(index>=5)abort();
 if(replay) {
  for(j=0;j<3;++j)if(fabs(start[j]-seen[index].start[j])>0.0001 ||
      fabs(end[j]-seen[index].end[j])>0.0001) {
   fprintf(stderr,"trace%d axis%d mode%d offset%d start%g/%g end%g/%g\n",
    index,j,mode,offset,start[j],seen[index].start[j],end[j],seen[index].end[j]);exit(1);
  }
  if(pass!=seen[index].pass || mask!=seen[index].mask || mins || maxs)abort();
  *tr=seen[index].result;return;
 }
 memset(tr,0,sizeof(*tr));tr->fraction=0.5f;tr->entityNum=1;
 tr->plane.normal[0]=-1;tr->surfaceFlags=3u<<24;
 tr->endpos[0]=offset+100.25f;
 if(index==0)hook(entry,recursive);
 else if(index==1) {
  tr->endpos[0]=offset+((mode>=2)?104.25f:1000.25f);
  if(mode==1)tr->fraction=1;
 } else if(index==2 && mode>=2) {
  tr->endpos[0]=offset+1000.25f;if(mode==3)tr->fraction=1;
 } else {
  tr->endpos[0]=offset+((mode==4)?99.25f:132.25f);tr->plane.normal[0]=1;
 }
 VectorCopy(start,seen[index].start);VectorCopy(end,seen[index].end);
 seen[index].mask=mask;seen[index].pass=pass;seen[index].result=*tr;
}
static void *tempEntity(float *origin,int event) {memset(temporary,0,sizeof(temporary));return temporary;}
int main(int argc,char **argv) {
 HMODULE dll;unsigned char *base,*ent,*target;tce_weaponDef_t *defs;
 int (__cdecl *original)(void *,void *,float *,float *,float,int,int,int,int,int,int,int,int,int);
 int expected,n=0,directionCase;
 float start[3]={0},end[3];
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
 base=(unsigned char *)dll;entry=base+0x9d500;original=(void *)entry;memcpy(saved,entry,5);
 hook(base+0x8ae80,traceWall);hook(base+0x94fa0,tempEntity);
 ent=base+0x226e2a0;target=ent+0x57c;memset(ent,0,0x57c*2);
 *(void **)(ent+0x190)=client;*(int *)(client+0xa4)=8;*(int *)(ent+0xe8)=8;
 *(int *)target=1;*(char **)(target+0x1a4)="worldspawn";
 *(int *)(base+0x7849cc)=0;*(int *)(base+0x7e208c)=0;*(float *)(base+0x66bba0)=1;
 defs=(void *)(base+0x24435c0);memset(defs,0,64*sizeof(*defs));defs[8].unknown_134=10000;
 for(offset=-10000;offset<=10000;offset+=100)for(mode=0;mode<5;++mode)for(directionCase=0;directionCase<3;++directionCase) {
  trace_t first,exitTrace;int count,result;
  start[0]=(float)offset;end[0]=(float)offset+8192;end[1]=(float)(directionCase*120);end[2]=(float)(directionCase*-40);
  strcpy(defs[8].caliberClass,"9MM");calls=traces=replay=0;patch(entry,saved);
  original(ent,ent,start,end,0,137,10000,0,0,4,4,8,123,0);
  count=traces;expected=calls;
  first=seen[0].result;
  { vec3_t attackerOrigin={0}; SnapVectorTowards(first.endpos,attackerOrigin); }
  SnapVectorTowards(first.endpos,start);
  end[0]=(float)offset+8192;end[1]=(float)(directionCase*120);end[2]=(float)(directionCase*-40);
  traces=1;replay=1;
  result=TCE_BulletWallExit(&first,start,end,0,traceWall,&exitTrace);
  if(traces!=count || result!=expected) {
   fprintf(stderr,"mode%d offset%d traces%d/%d result%d/%d\n",mode,offset,traces,count,result,expected);return 1;
  }
  ++n;
 }
 patch(entry,saved);
 printf("%d original wall-exit trace sequences matched\n",n);return 0;
}
