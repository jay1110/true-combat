#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte raw[0x57c];static gentity_t ent;static byte*base;static int mode,bad,path,traces,events[16],count;static char model[]="item.md3",classname[]="test_item",target[]="target";
extern void Use_Item(gentity_t*,gentity_t*,gentity_t*);
static void event(int id){events[count++]=id;}
static void* active(void){return mode?(void*)raw:(void*)&ent;}
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static int modelIndex(const char*n){event(1);if(n!=model)bad=1;return 177;}
static void traceSink(trace_t*t,const vec3_t s,const vec3_t mi,const vec3_t ma,const vec3_t end,int pass,int mask){vec3_t start,dest,minExpected={-10,-10,0},maxExpected={10,10,5};event(2);VectorCopy(ent.s.origin,start);VectorCopy(start,dest);dest[2]-=4096;if(traces)start[2]-=10;if(memcmp(s,start,12)||memcmp(end,dest,12)||memcmp(mi,minExpected,12)||memcmp(ma,maxExpected,12)||pass!=ent.s.number||mask!=1)bad=1;memset(t,0,sizeof(*t));t->startsolid=path==2||(path==1&&traces==0);t->entityNum=91;VectorSet(t->endpos,13.25f,-9.5f,3.125f);++traces;}
static void freeSink(gentity_t*e){event(4);if(e!=active())bad=1;}
static void linkSink(gentity_t*e){event(5);if(e!=active())bad=1;}
static void printSink(const char*fmt,...){va_list ap;const char*c,*v;event(3);va_start(ap,fmt);c=va_arg(ap,const char*);v=va_arg(ap,const char*);if(strcmp(fmt,"FinishSpawningItem: %s startsolid at %s\n")||c!=classname||!v||!*v)bad=1;va_end(ap);}
static int stateEqual(void){return ent.s.eType==*(int*)(raw+4)&&ent.s.eFlags==*(int*)(raw+8)&&!memcmp(&ent.s.pos,raw+0xc,sizeof(trajectory_t))&&!memcmp(&ent.s.apos,raw+0x30,sizeof(trajectory_t))&&ent.s.otherEntityNum2==*(int*)(raw+0x90)&&ent.s.groundEntityNum==*(int*)(raw+0x94)&&ent.s.modelindex==*(int*)(raw+0xa4)&&ent.s.modelindex2==*(int*)(raw+0xa8)&&ent.s.density==*(int*)(raw+0xf4)&&ent.r.contents==*(int*)(raw+0x14c)&&!memcmp(ent.r.mins,raw+0x134,12)&&!memcmp(ent.r.maxs,raw+0x140,12)&&!memcmp(ent.r.currentOrigin,raw+0x168,12)&&!memcmp(ent.r.currentAngles,raw+0x174,12)&&ent.flags==*(int*)(raw+0x1b0);}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*original)(void*);byte*itemRaw;int flags,hide,type,stages,hasModel,n=0,expected[16],expectedCount,expectedTraces;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x5e420);itemRaw=base+0xb6b38+60;
 if(!hook(base+0x947d0,modelIndex)||!hook(G_ModelIndex,modelIndex)||!hook(base+0x8ae80,traceSink)||!hook(trap_Trace,traceSink)||!hook(base+0x94f30,freeSink)||!hook(G_FreeEntity,freeSink)||!hook(base+0x8afa0,linkSink)||!hook(trap_LinkEntity,linkSink)||!hook(base+0x5ed80,printSink)||!hook(G_Printf,printSink))return 2;
 for(flags=0;flags<4;++flags)for(hide=0;hide<4;++hide)for(type=0;type<9;++type)for(stages=0;stages<=4;++stages)for(path=0;path<3;++path)for(hasModel=0;hasModel<2;++hasModel){int j;memset(raw,0,sizeof(raw));memset(&ent,0,sizeof(ent));memset(itemRaw,0,60);memset(&bg_itemlist[1],0,sizeof(gitem_t));
  ent.s.number=*(int*)raw=17;ent.spawnflags=*(int*)(raw+0x1a8)=flags;ent.flags=*(int*)(raw+0x1b0)=hide&1?FL_TEAMSLAVE:0;ent.targetname=*(char**)(raw+0x258)=hide&2?target:NULL;ent.model=*(char**)(raw+0x1b4)=hasModel?model:NULL;ent.classname=*(char**)(raw+0x1a4)=classname;ent.item=&bg_itemlist[1];*(void**)(raw+0x33c)=itemRaw;ent.item->giType=*(int*)(itemRaw+0x24)=type;
  for(j=0;j<4;++j){ent.item->world_model[j]=j<stages?model:NULL;*(char**)(itemRaw+8+4*j)=ent.item->world_model[j];}
  VectorSet(ent.s.origin,4.5f,-12.25f,125.75f);memcpy(raw+0x5c,ent.s.origin,12);VectorSet(ent.s.angles,13.5f,182.25f,-9.75f);memcpy(raw+0x74,ent.s.angles,12);ent.s.modelindex2=*(int*)(raw+0xa8)=-17;ent.s.density=*(int*)(raw+0xf4)=-12;ent.s.otherEntityNum2=*(int*)(raw+0x90)=44;ent.s.groundEntityNum=*(int*)(raw+0x94)=55;
  memset(&ent.s.pos,0x25,sizeof(trajectory_t));memcpy(raw+0xc,&ent.s.pos,sizeof(trajectory_t));memset(&ent.s.apos,0x25,sizeof(trajectory_t));memcpy(raw+0x30,&ent.s.apos,sizeof(trajectory_t));
  bad=count=traces=0;mode=1;original(raw);if(bad){fprintf(stderr,"original callback mismatch case%d\n",n);return 3;}expectedCount=count;expectedTraces=traces;memcpy(expected,events,sizeof(events));bad=count=traces=0;mode=0;FinishSpawningItem(&ent);
  if(bad||count!=expectedCount||traces!=expectedTraces||memcmp(events,expected,count*sizeof(int))||!stateEqual()||ent.touch!=(type==7?Touch_Item:Touch_Item_Auto)||*(void**)(raw+0x2b0)!=base+(type==7?0x5dd20:0x5dc90)||ent.use!=Use_Item||*(void**)(raw+0x2b4)!=base+0x5e410){fprintf(stderr,"finish case%d flags%d hide%d type%d stages%d path%d model%d mismatch callbacks%d/%d bad%d\n",n,flags,hide,type,stages,path,hasModel,count,expectedCount,bad);return 1;}++n;
 }
 printf("%d whole FinishSpawningItem comparisons passed\n",n);return 0;
}
