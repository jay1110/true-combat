#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern void Think_SetupObjectiveInfo(gentity_t*),Touch_ObjectiveInfo(gentity_t*,gentity_t*,trace_t*);
extern void explosive_indicator_think(gentity_t*),constructible_indicator_think(gentity_t*);
static byte*base;static int side,variant,kind,count,bad,spawnCount;static jmp_buf jump;
typedef struct{int k,a;float pos[3];char text[256];}event;static event events[12];
typedef struct{entityState_t state;int target,parent,chain,tag,score,next,owner,sv,count2,think,touch;float current[3];char classname[40],tagName[64];}entityResult;
typedef struct{int n,spawns,fatal;event events[12];entityResult entities[5];}result;
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static void*ent(int i){return side?(void*)&g_entities[i]:(void*)(base+0x226e2a0+i*0x57c);}
static int idx(void*p){if(!p)return -1;return side?(gentity_t*)p-g_entities:((byte*)p-base-0x226e2a0)/0x57c;}
static void note(int k,int a,const char*s){event*e;if(count==12)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->k=k;e->a=a;if(s)Q_strncpyz(e->text,s,sizeof(e->text));}
static void*find(void*p,const char*name){note(1,idx(p),name);if(!p)return kind==9?NULL:ent(1);if(p!=ent(1))bad=1;return variant&32?ent(2):NULL;}
static void*spawn(void){note(2,4,NULL);spawnCount++;if(spawnCount!=1)bad=1;return ent(4);}
static void linker(void*p){int n=idx(p);note(3,n,NULL);if(n<0||n>4){bad=1;return;}memcpy(events[count-1].pos,side?g_entities[n].s.pos.trBase:(float*)((byte*)p+0x18),12);}
static void QDECL fatal(const char*f,...){va_list a;char b[256];va_start(a,f);Q_vsnprintf(b,sizeof(b),f,a);va_end(a);note(4,0,b);longjmp(jump,1);}
static int thinkId(void*p){if(!p)return 0;if(p==(side?(void*)explosive_indicator_think:(void*)(base+0x93570)))return 1;if(p==(side?(void*)constructible_indicator_think:(void*)(base+0x93720)))return 2;return 3;}
static void capture(result*r,int fatalSeen){int i;memset(r,0,sizeof(*r));r->n=count;r->spawns=spawnCount;r->fatal=fatalSeen;memcpy(r->events,events,sizeof(events));for(i=0;i<5;i++){entityResult*t=&r->entities[i];gentity_t*e=&g_entities[i];byte*p=base+0x226e2a0+i*0x57c;memcpy(&t->state,side?(void*)&e->s:(void*)p,sizeof(entityState_t));
#define PTR(out,field,off) t->out=idx(side?(void*)e->field:*(void**)(p+off))
#define VAL(out,field,off) t->out=side?e->field:*(int*)(p+off)
 PTR(target,target_ent,0x264);PTR(parent,parent,0x218);PTR(chain,chain,0x2e8);PTR(tag,tagParent,0x464);VAL(score,tceObjectiveScore,0x2e4);VAL(next,nextthink,0x29c);VAL(owner,r.ownerNum,0x180);VAL(sv,r.svFlags,0x128);VAL(count2,count2,0x3a4);t->think=thinkId(side?(void*)e->think:*(void**)(p+0x2a4));t->touch=side?(e->touch==Touch_ObjectiveInfo):(*(void**)(p+0x2b0)==base+0x938f0);memcpy(t->current,side?(void*)e->r.currentOrigin:(void*)(p+0x168),12);Q_strncpyz(t->classname,side?(e->classname?e->classname:""):(*(char**)(p+0x1a4)?*(char**)(p+0x1a4):""),sizeof(t->classname));memcpy(t->tagName,side?(void*)e->tagName:(void*)(p+0x424),64);
}}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*original)(void*);int v,i,j,total=0,fatalSeen;int types[]={14,63,64,65,66,48,33,59,0,0};result a,b;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;original=(void*)(base+0x93910);if(sizeof(entityState_t)!=0x120)return 2;
#define H(r,fn,repl) if(!hook(base+r,repl)||!hook(fn,repl))return 2
 H(0x94960,G_FindByTargetname,find);H(0x94e30,G_Spawn,spawn);H(0x8afa0,trap_LinkEntity,linker);H(0x5ee10,G_Error,fatal);
 for(kind=0;kind<10;kind++)for(v=0;v<1024;v++)for(side=0;side<2;side++){
  variant=v;count=bad=spawnCount=fatalSeen=0;memset(events,0,sizeof(events));level.time=*(int*)(base+0x7febe0)=100000+v;
  for(i=0;i<5;i++){gentity_t*e=&g_entities[i];byte*p=base+0x226e2a0+i*0x57c;memset(e,0,sizeof(*e));memset(p,0,0x57c);e->s.number=*(int*)p=i;e->nextthink=*(int*)(p+0x29c)=-7;e->s.teamNum=*(int*)(p+0x108)=i+7;e->constructibleStats.weaponclass=*(int*)(p+0x570)=i+4;}
  {gentity_t*e=&g_entities[0];byte*p=base+0x226e2a0;
#define E(field,off,val) e->field=*(int*)(p+off)=(val)
   e->target="test_target";*(char**)(p+0x254)=e->target;E(spawnflags,0x1a8,v%16);E(count2,0x3a4,(v/16)%8-1);e->tagParent=v&16?&g_entities[3]:NULL;*(void**)(p+0x464)=v&16?p+3*0x57c:NULL;Q_strncpyz(e->tagName,"tag_objective_attachment",64);memcpy(p+0x424,e->tagName,64);
   for(j=0;j<3;j++){e->r.currentOrigin[j]=*(float*)(p+0x168+j*4)=71.25f+j;e->r.absmin[j]=*(float*)(p+0x150+j*4)=-24.75f-j;e->r.absmax[j]=*(float*)(p+0x15c+j*4)=36.125f+j;e->s.pos.trBase[j]=*(float*)(p+0x18+j*4)=v&64?(j-1)*-7.625f:0.f;}
   g_entities[1].s.eType=*(int*)(p+0x57c+4)=types[kind];g_entities[2].s.eType=*(int*)(p+2*0x57c+4)=(v&128)?ET_GENERAL:ET_CONSTRUCTIBLE;
   g_entities[1].spawnflags=*(int*)(p+0x57c+0x1a8)=v&256?4:0;g_entities[2].spawnflags=*(int*)(p+2*0x57c+0x1a8)=v&512?4:0;
   g_entities[1].s.angles2[1]=*(float*)(p+0x57c+0x84)=v&4?1.f:0.f;g_entities[1].count2=*(int*)(p+0x57c+0x3a4)=v&1;g_entities[2].count2=*(int*)(p+2*0x57c+0x3a4)=v&2;g_entities[1].grenadeFired=*(int*)(p+0x57c+0x3ac)=(v/8)%3;g_entities[2].grenadeFired=*(int*)(p+2*0x57c+0x3ac)=(v/24)%3;
  }
  if(!setjmp(jump)){if(side)Think_SetupObjectiveInfo(&g_entities[0]);else original(ent(0));}else fatalSeen=1;
  capture(side?&b:&a,fatalSeen);if(side){if(bad||memcmp(&a,&b,sizeof(a))){printf("Objective setup mismatch kind%d case%d bad%d calls%d/%d fatal%d/%d\n",types[kind],v,bad,a.n,b.n,a.fatal,b.fatal);for(i=0;i<sizeof(a);i++)if(((byte*)&a)[i]!=((byte*)&b)[i]){printf("resultbyte%x %02x/%02x\n",i,((byte*)&a)[i],((byte*)&b)[i]);break;}for(i=0;i<a.n||i<b.n;i++)if(memcmp(&a.events[i],&b.events[i],sizeof(event)))printf("call%d %d,%d[%s] / %d,%d[%s]\n",i,a.events[i].k,a.events[i].a,a.events[i].text,b.events[i].k,b.events[i].a,b.events[i].text);return 1;}total++;}
 }
 printf("PASS whole Think_SetupObjectiveInfo %d cases including three fatal paths\n",total);return 0;
}
