#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void explosive_indicator_think(gentity_t*);
extern void constructible_indicator_think(gentity_t*);
extern void G_SetConfigStringValue(int,const char*,const char*);
static byte*base;static int side,variant,count,bad;static mapEntityData_t nodes[2];
typedef struct{int kind,a,b;char text[1024];}event;
static event events[10],expectedEvents[10];
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static int entityIndex(void*p){return side?(gentity_t*)p-g_entities:((byte*)p-base-0x226e2a0)/0x57c;}
static int listIndex(void*p){return side?(mapEntityData_Team_t*)p-mapEntityData:((byte*)p-base-0x76c280)/0xc034;}
static void note(int k,int a,int b,const char*s){event*e;if(count==10)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->kind=k;e->a=a;e->b=b;if(s)Q_strncpyz(e->text,s,sizeof(e->text));}
static void*find(void*p,int n){int t=listIndex(p);if(t<0||t>1)exit(3);note(1,t,n,NULL);return variant&(1<<t)?&nodes[t]:NULL;}
static void removeMap(void*p,void*n){int t=listIndex(p);if(n!=&nodes[t])bad=1;note(2,t,0,NULL);}
static void release(void*p){note(3,entityIndex(p),0,NULL);if(variant&4)memset(p,0,side?sizeof(gentity_t):0x57c);}
static const char*initial[] = {"", "\\owner\\red\\state\\active", "\\state\\old\\owner\\blue", "\\STATE\\capital", "\\state\\one\\state\\two"};
static const char*keys[]={"state","owner","a","STATE"};static const char*values[]={"", "new", "^1terrorists", "12345"};
static void getConfig(int n,char*s,int len){note(4,n,len,NULL);Q_strncpyz(s,initial[variant%5],len);}
static void setConfig(int n,const char*s){note(5,n,0,s);}
typedef struct{entityState_t state[5];int next[5],count2[5],inuse[5];}result;
static void capture(result*r){int i;memset(r,0,sizeof(*r));for(i=0;i<5;i++){byte*p=base+0x226e2a0+i*0x57c;memcpy(&r->state[i],side?(void*)&g_entities[i].s:(void*)p,sizeof(entityState_t));r->next[i]=side?g_entities[i].nextthink:*(int*)(p+0x29c);r->count2[i]=side?g_entities[i].count2:*(int*)(p+0x3a4);r->inuse[i]=side?g_entities[i].inuse:*(int*)(p+0x194);}}
/* Opt-in safety contract, separately reported from strict original parity.
 * A target-state4 marker is unlinked from both map lists, freed once, and
 * receives no writes after release. The native body instead continues. */
static int safeRemovalMatches(const result*before,const result*after){
 result wanted=*before;event trace[10];int n=0,t;memset(trace,0,sizeof(trace));
 if(variant&4){memset(&wanted.state[0],0,sizeof(wanted.state[0]));wanted.next[0]=wanted.count2[0]=wanted.inuse[0]=0;}
 for(t=0;t<2;t++){trace[n].kind=1;trace[n++].a=t;if(variant&(1<<t)){trace[n].kind=2;trace[n++].a=t;}}
 trace[n++].kind=3;
 return !bad&&count==n&&!memcmp(trace,events,sizeof(trace))&&!memcmp(&wanted,after,sizeof(wanted));
}
int main(int argc,char**argv){HMODULE dll;void(__cdecl*original[2])(void*);void(__cdecl*setValue)(int,const char*,const char*);int phase,v,i,j,wantCount,total=0;result a,b,before;int acceptedSafety=0,safetyCases=0,safeRemoval=0;int types[]={13,38,39,32,0};
 if((argc!=2&&argc!=3)||!(dll=LoadLibraryA(argv[1])))return 2;acceptedSafety=argc==3&&!strcmp(argv[2],"--accept-safe-marker-removal");if(argc==3&&!acceptedSafety)return 2;base=(byte*)dll;original[0]=(void*)(base+0x93570);original[1]=(void*)(base+0x93720);setValue=(void*)(base+0x938a0);if(sizeof(entityState_t)!=0x120)return 2;
#define H(r,fn,repl) if(!hook(base+r,repl)||!hook(fn,repl))return 2
 H(0x90250,G_FindMapEntityData,find);H(0x90190,G_FreeMapEntityData,removeMap);H(0x94f30,G_FreeEntity,release);H(0x8ade0,trap_GetConfigstring,getConfig);H(0x8adc0,trap_SetConfigstring,setConfig);
 for(phase=0;phase<3;phase++)for(v=0;v<4096;v++)for(side=0;side<2;side++){
  variant=v;count=bad=0;memset(events,0,sizeof(events));level.time=*(int*)(base+0x7febe0)=100000+v;
  for(i=0;i<5;i++){gentity_t*e=&g_entities[i];byte*p=base+0x226e2a0+i*0x57c;memset(e,0,sizeof(*e));memset(p,0,0x57c);e->s.number=*(int*)p=i;e->s.teamNum=*(int*)(p+0x108)=1+i%2;e->constructibleStats.weaponclass=*(int*)(p+0x570)=10+i;e->inuse=*(int*)(p+0x194)=1;e->r.linked=*(int*)(p+0x120)=1;e->nextthink=*(int*)(p+0x29c)=-1;e->count2=*(int*)(p+0x3a4)=17;for(j=0;j<3;j++)e->r.currentOrigin[j]=*(float*)(p+0x168+j*4)=i*100+j*.25f;}
  {gentity_t*e=&g_entities[0],*parent=&g_entities[1];byte*p=base+0x226e2a0,*q=p+0x57c;
#define E(field,off,val) e->field=*(int*)(p+off)=(val)
#define P(field,off,val) parent->field=*(int*)(q+off)=(val)
   E(s.eType,4,types[(v/8)%5]);E(r.ownerNum,0x180,1);E(s.onFireStart,0xfc,(v/40)%3?0:4);E(tceObjectiveScore,0x2e4,(v/120)%3-1);E(s.teamNum,0x108,(v/360)%4);e->parent=&g_entities[4];*(void**)(p+0x218)=p+4*0x57c;
   P(inuse,0x194,(v/1440)%2);P(r.linked,0x120,(v/720)%2);P(spawnflags,0x1a8,(v/8)%16);P(s.eType,4,(v/48)%3==0?ET_CONSTRUCTIBLE:(v/48)%3==1?ET_OID_TRIGGER:ET_GENERAL);
   parent->target_ent=&g_entities[2];*(void**)(q+0x264)=p+2*0x57c;parent->chain=(v/96)%2?&g_entities[3]:NULL;*(void**)(q+0x2e8)=(v/96)%2?p+3*0x57c:NULL;
   g_entities[2].entstate=*(int*)(p+2*0x57c+0x47c)=(v/16)%6;
   g_entities[2].s.angles2[1]=*(float*)(p+2*0x57c+0x84)=(v/24)%2?1.f:0.f;g_entities[3].s.angles2[1]=*(float*)(p+3*0x57c+0x84)=(v/72)%2?-1.f:0.f;
   if(phase==1&&!parent->chain&&(v/288)%3==2){parent->target_ent=NULL;*(void**)(q+0x264)=NULL;}
  }
  safeRemoval=phase==0&&g_entities[0].s.eType==ET_EXPLOSIVE_INDICATOR&&g_entities[1].inuse&&
   !(g_entities[1].s.eType==ET_CONSTRUCTIBLE&&!g_entities[1].r.linked)&&g_entities[1].target_ent==&g_entities[2]&&g_entities[2].entstate==4;
  if(side&&safeRemoval)capture(&before);
  if(phase<2){if(side){if(phase)constructible_indicator_think(&g_entities[0]);else explosive_indicator_think(&g_entities[0]);}else original[phase](base+0x226e2a0);}else{if(side)G_SetConfigStringValue(v%64,keys[(v/5)%4],values[(v/20)%4]);else setValue(v%64,keys[(v/5)%4],values[(v/20)%4]);}
  capture(side?&b:&a);if(!side){wantCount=count;memcpy(expectedEvents,events,sizeof(events));}else{if(acceptedSafety&&safeRemoval){if(!safeRemovalMatches(&before,&b)){printf("Safe marker removal contract failed phase%d case%d\n",phase,v);return 1;}safetyCases++;total++;continue;}if(bad||count!=wantCount||memcmp(events,expectedEvents,sizeof(events))||memcmp(&a,&b,sizeof(a))){printf("Objective marker mismatch phase%d case%d bad%d calls%d/%d\n",phase,v,bad,wantCount,count);for(i=0;i<sizeof(a);i++)if(((byte*)&a)[i]!=((byte*)&b)[i]){printf("statebyte%x %02x/%02x\n",i,((byte*)&a)[i],((byte*)&b)[i]);break;}for(i=0;i<count||i<wantCount;i++)if(memcmp(&events[i],&expectedEvents[i],sizeof(event)))printf("call%d %d,%d,%d[%s] / %d,%d,%d[%s]\n",i,expectedEvents[i].kind,expectedEvents[i].a,expectedEvents[i].b,expectedEvents[i].text,events[i].kind,events[i].a,events[i].b,events[i].text);return 1;}total++;}
 }
 printf("PASS objective marker lifecycle and config update %d cases (4096 per body), separately accepted safe removals%d\n",total,safetyCases);return 0;
}
