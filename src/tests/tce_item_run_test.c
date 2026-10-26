#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include "../game/tce_trajectory.h"
#include <windows.h>
#include <stdio.h>
static byte raw[0x57c],rawItem[60];static gentity_t ent;static gitem_t item;static int mode,bad,events[16],count,nodrop,mutate;static trace_t configured;static vec3_t destination={51.25f,-31.5f,15.75f};
extern void G_RunItemProp(gentity_t*,vec3_t);extern void G_BounceItem(gentity_t*,trace_t*);
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static void*active(void){return mode?(void*)raw:(void*)&ent;}
static void event(int id,gentity_t*e){events[count++]=id;if(e!=active())bad=1;}
static void trajectory(const trajectory_t*t,int when,vec3_t out,qboolean angle,int spline,float gravity){event(1,active());if((void*)t!=(mode?(void*)(raw+12):(void*)&ent.s.pos)||when!=level.time||angle||spline!=7||gravity!=1)bad=1;VectorCopy(destination,out);}
static void traceSink(trace_t*t,const vec3_t s,const vec3_t mi,const vec3_t ma,const vec3_t end,int pass,int mask){event(2,active());if(memcmp(s,ent.r.currentOrigin,12)||memcmp(mi,ent.r.mins,12)||memcmp(ma,ent.r.maxs,12)||memcmp(end,destination,12)||pass!=11||mask!=(ent.clipmask?ent.clipmask:1))bad=1;*t=configured;}
static void prop(gentity_t*e,vec3_t o){event(3,e);if(memcmp(o,destination,12))bad=1;}
static void linkSink(gentity_t*e){event(4,e);if(memcmp(mode?(void*)(raw+0x168):(void*)ent.r.currentOrigin,configured.endpos,12))bad=1;}
static void think(gentity_t*e){event(5,e);if(mutate){if(mode){*(float*)(raw+0x168)=999;*(void**)(raw+0x33c)=NULL;}else{ent.r.currentOrigin[0]=999;ent.item=NULL;}}}
static int contents(const vec3_t p,int pass){event(6,active());if(pass!=-1||memcmp(p,mode?(void*)(raw+0x168):(void*)ent.r.currentOrigin,12))bad=1;return nodrop?CONTENTS_NODROP:0;}
static void bounce(gentity_t*e,trace_t*t){trace_t expected=configured;event(7,e);if(expected.startsolid)expected.fraction=0;if(memcmp(t,&expected,sizeof(expected)))bad=1;}
static void freeSink(gentity_t*e){event(8,e);}static void returnFlag(gentity_t*e){event(9,e);}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*original)(void*);int type,ground,clip,content,property,itemtype,hit,n=0;const int types[]={0,2,6,9,13,15,16};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x5ea50);
 if(!hook(b+0x2c2e0,trajectory)||!hook(TCE_BG_EvaluateTrajectory,trajectory)||!hook(b+0x8ae80,traceSink)||!hook(trap_Trace,traceSink)||!hook(b+0x5e960,prop)||!hook(G_RunItemProp,prop)||!hook(b+0x8afa0,linkSink)||!hook(trap_LinkEntity,linkSink)||!hook(b+0x63b80,think)||!hook(G_RunThink,think)||!hook(b+0x8af40,contents)||!hook(trap_PointContents,contents)||!hook(b+0x5e7d0,bounce)||!hook(G_BounceItem,bounce)||!hook(b+0x94f30,freeSink)||!hook(G_FreeEntity,freeSink)||!hook(b+0x8db30,returnFlag)||!hook(Team_ReturnFlag,returnFlag))return 2;
 for(type=0;type<7;++type)for(ground=0;ground<2;++ground)for(clip=0;clip<2;++clip)for(content=0;content<2;++content)for(property=0;property<4;++property)for(itemtype=0;itemtype<3;++itemtype)for(hit=0;hit<4;++hit)for(nodrop=0;nodrop<2;++nodrop)for(mutate=0;mutate<2;++mutate){int expected[16],expectedCount;
  memset(raw,0,sizeof(raw));memset(&ent,0,sizeof(ent));ent.s.pos.trType=*(int*)(raw+12)=types[type];ent.s.pos.trTime=*(int*)(raw+16)=-10;ent.s.groundEntityNum=*(int*)(raw+0x94)=ground?-1:91;ent.s.effect2Time=*(int*)(raw+0x110)=7;ent.clipmask=*(int*)(raw+0x1d4)=clip?0x6000081:0;ent.r.contents=*(int*)(raw+0x14c)=content;ent.r.ownerNum=*(int*)(raw+0x180)=11;ent.isProp=*(int*)(raw+0x388)=property&1;ent.takedamage=*(int*)(raw+0x2cc)=(property>>1)&1;ent.item=itemtype?&item:NULL;*(void**)(raw+0x33c)=itemtype?rawItem:NULL;item.giType=*(int*)(rawItem+0x24)=itemtype==2?8:4;level.time=*(int*)(b+0x7febe0)=100001;
  VectorSet(ent.r.currentOrigin,1,2,3);VectorSet(ent.r.mins,-10,-10,0);VectorSet(ent.r.maxs,10,10,20);memcpy(raw+0x168,ent.r.currentOrigin,12);memcpy(raw+0x134,ent.r.mins,12);memcpy(raw+0x140,ent.r.maxs,12);memset(&configured,0,sizeof(configured));configured.fraction=hit&1?1:.5f;configured.startsolid=hit>>1;VectorSet(configured.endpos,31,-22,9);configured.entityNum=8;configured.plane.normal[2]=1;
  mode=1;bad=count=0;original(raw);if(bad){fprintf(stderr,"original run callback case%d\n",n);return 3;}expectedCount=count;memcpy(expected,events,sizeof(events));mode=0;bad=count=0;G_RunItem(&ent);
  if(bad||count!=expectedCount||memcmp(events,expected,count*sizeof(int))||ent.s.pos.trType!=*(int*)(raw+12)||ent.s.pos.trTime!=*(int*)(raw+16)||memcmp(ent.r.currentOrigin,raw+0x168,12)||((ent.item!=NULL)!=(*(void**)(raw+0x33c)!=NULL))){fprintf(stderr,"run case%d mismatch callbacks%d/%d bad%d\n",n,count,expectedCount,bad);return 1;}++n;
 }
 printf("%d whole G_RunItem comparisons passed\n",n);return 0;
}
