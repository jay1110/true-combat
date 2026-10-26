#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_snapshot.c"
#include <windows.h>
#include <stdio.h>
typedef struct {int type,a,b;} event_t;
static event_t events[4096];static int count,originalRun,successAt,newFlags;static byte *base;
static int slot(void*p){if(!p)return -1;if(originalRun)return p==base+0x40352cc?0:p==base+0x40598b0?1:2;return p==&cg.activeSnapshots[0]?0:p==&cg.activeSnapshots[1]?1:2;}
static void record(int type,int a,int b){events[count].type=type;events[count].a=a;events[count++].b=b;}
static int snapshot(int number,void*dest){record(1,number,slot(dest));if(number!=successAt)return 0;*(int*)dest=newFlags;return 1;}
static void lag(void*p){record(2,slot(p),0);}
static void print(const char*fmt,...){record(3,!strcmp(fmt,"[skipnotify]WARNING: CG_ReadNextSnapshot: way out of range, %i > %i\n"),0);}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE module;byte*b;void*(__cdecl*original)(void);static event_t expected[4096];int k,processed,latest,n,result,expectProcessed,oldFlags,which;void*ret;
 if(argc!=2||!(module=LoadLibraryA(argv[1])))return 2;b=base=(byte*)module;original=(void*)(b+0x605e0);hook(b+0x655b0,snapshot);hook(trap_GetSnapshot,snapshot);hook(b+0x210b0,lag);hook(CG_AddLagometerSnapshotInfo,lag);hook(b+0x47b10,print);hook(CG_Printf,print);
 for(k=0;k<4800;k++){
  processed=k%17-8;latest=processed+(k%6==0?1001:k%6-2);which=k%4;oldFlags=(k/4)%8;newFlags=(k/32)%8;successAt=processed+1+(k/256)%5;
  if(k%7==0)successAt=latest+1;
  *(int*)(b+0x257e0f8)=cgs.processedSnapshotNum=processed;*(int*)(b+0x40352bc)=cg.latestSnapshotNum=latest;
  cg.snap=which==0?NULL:which==1?&cg.activeSnapshots[0]:which==2?&cg.activeSnapshots[1]:&cg.activeSnapshots[0];
  *(void**)(b+0x40352c4)=which==0?NULL:which==2?b+0x40598b0:b+0x40352cc;
  if(cg.snap)cg.snap->snapFlags=oldFlags;if(*(void**)(b+0x40352c4))**(int**)(b+0x40352c4)=oldFlags;
  cg.damageTime=*(int*)(b+0x4080ef0)=71;cg.duckTime=*(int*)(b+0x407ef64)=72;cg.landTime=*(int*)(b+0x407ef6c)=73;cg.stepTime=*(int*)(b+0x407ef5c)=74;
  originalRun=1;count=0;ret=original();result=slot(ret);n=count;memcpy(expected,events,n*sizeof(event_t));expectProcessed=*(int*)(b+0x257e0f8);
  originalRun=0;count=0;ret=CG_ReadNextSnapshot();
  if(slot(ret)!=result||count!=n||memcmp(expected,events,n*sizeof(event_t))||cgs.processedSnapshotNum!=expectProcessed||cg.damageTime!=*(int*)(b+0x4080ef0)||cg.duckTime!=*(int*)(b+0x407ef64)||cg.landTime!=*(int*)(b+0x407ef6c)||cg.stepTime!=*(int*)(b+0x407ef5c)){printf("read snapshot case%d result%d/%d calls%d/%d\n",k,result,slot(ret),n,count);return 1;}
 }puts("4800 whole CG_ReadNextSnapshot comparisons passed");return 0;}
