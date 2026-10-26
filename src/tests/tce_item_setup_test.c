#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte *base,*raw;static gentity_t ent;static gitem_t item;static byte rawItem[60];
static int mode,bad,events[16],count,kase,hasNoise;static char noise[]="sound/item.wav",model[]="models/item.md3";
static void record(int e){events[count++]=e;}
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static int spawnFloat(const char*key,const char*d,float*out,const char*file,int line){int off;float value;if(!strcmp(key,"random")){off=0x310;value=(kase%9-4)*.25f;record(1);}else if(!strcmp(key,"wait")){off=0x30c;value=(kase%7-3)*.5f;record(2);}else{bad=1;return 0;}if(strcmp(d,"0")||out!=(mode?(float*)(raw+off):off==0x310?&ent.random:&ent.wait)||!file||line<=0)bad=1;*out=value;return 1;}
static int spawnString(const char*key,const char*d,char**out,const char*file,int line){record(3);if(strcmp(key,"noise")||d||!file||line<=0)bad=1;*out=hasNoise?noise:NULL;return hasNoise;}
static int soundIndex(const char*n){record(4);if(n!=noise)bad=1;return 101;}
static int modelIndex(const char*n){record(5);if(n!=model)bad=1;return 202;}
static int spawnInt(const char*key,const char*d,int*out,const char*file,int line){int off,value;if(!strcmp(key,"count")){record(6);off=0xf4;value=kase%7-3;if(strcmp(d,"1"))bad=1;}else{record(7);off=0x2d4;value=(kase%5-2)*50;if(strcmp(key,"speedscale")||strcmp(d,"100"))bad=1;}if(out!=(mode?(int*)(raw+off):off==0xf4?&ent.s.density:&ent.splashDamage)||!file||line<=0)bad=1;*out=value;return 1;}
static int hit,owner,solid,noimpact;static trace_t configured;static vec3_t target;
static void *active(void){return mode?(void*)raw:(void*)&g_entities[10];}
static void traceSink(trace_t*t,const vec3_t start,const vec3_t mi,const vec3_t ma,const vec3_t end,int pass,int mask){gentity_t*e=&g_entities[10];vec3_t expected;record(8);VectorCopy(target,expected);expected[2]+=1;if(memcmp(start,e->r.currentOrigin,12)||memcmp(mi,e->r.mins,12)||memcmp(ma,e->r.maxs,12)||memcmp(end,expected,12)||pass!=owner||mask!=0x6000081)bad=1;*t=configured;}
static void dieSink(gentity_t*a,gentity_t*b,gentity_t*c,int damage,int mod){record(9);if(a!=active()||b!=active()||c||damage!=10||mod||(mode?*(int*)(raw+0x2cc):g_entities[10].takedamage))bad=1;}
static void breakSink(gentity_t*a){record(10);if(a!=active())bad=1;}
static void skySink(gentity_t*a){record(11);if(a!=active()||(mode?*(int*)(raw+0x2cc):g_entities[10].takedamage))bad=1;}
extern void G_RunItemProp(gentity_t*,vec3_t);
int main(int argc,char**argv){HMODULE dll;void(__cdecl*spawn)(void*,void*);void(__cdecl*prop)(void*,vec3_t);int t,n=0,expected[16],expectedCount;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;raw=base+0x226e2a0+10*0x57c;spawn=(void*)(base+0x5e6b0);prop=(void*)(base+0x5e960);
 if(!hook(base+0x859f0,spawnFloat)||!hook(G_SpawnFloatExt,spawnFloat)||!hook(base+0x85940,spawnString)||!hook(G_SpawnStringExt,spawnString)||!hook(base+0x85a30,spawnInt)||!hook(G_SpawnIntExt,spawnInt)||!hook(base+0x947f0,soundIndex)||!hook(G_SoundIndex,soundIndex)||!hook(base+0x947d0,modelIndex)||!hook(G_ModelIndex,modelIndex)||!hook(base+0x8ae80,traceSink)||!hook(trap_Trace,traceSink)||!hook(base+0x782f0,breakSink)||!hook(Prop_Break_Sound,breakSink)||!hook(base+0x783d0,skySink)||!hook(Props_Chair_Skyboxtouch,skySink))return 2;
 for(t=0;t<9;++t)for(kase=0;kase<140;++kase){memset(raw,0,0x57c);memset(&ent,0,sizeof(ent));hasNoise=kase%2;ent.model=*(char**)(raw+0x1b4)=kase%4<2?model:NULL;ent.noise_index=*(int*)(raw+0x308)=-9;ent.s.modelindex2=*(int*)(raw+0xa8)=-8;ent.s.density=*(int*)(raw+0xf4)=-7;ent.splashDamage=*(int*)(raw+0x2d4)=-6;item.giType=*(int*)(rawItem+0x24)=t;level.time=*(int*)(base+0x7febe0)=1000+kase;
 count=bad=0;mode=1;spawn(raw,rawItem);if(bad)return 3;expectedCount=count;memcpy(expected,events,sizeof(events));count=bad=0;mode=0;G_SpawnItem(&ent,&item);
 if(bad||count!=expectedCount||memcmp(expected,events,count*sizeof(int))||ent.random!=*(float*)(raw+0x310)||ent.wait!=*(float*)(raw+0x30c)||ent.nextthink!=*(int*)(raw+0x29c)||ent.item!=&item||*(void**)(raw+0x33c)!=rawItem||ent.think!=FinishSpawningItem||*(void**)(raw+0x2a4)!=base+0x5e420||ent.physicsBounce!=*(float*)(raw+0x1d0)||ent.noise_index!=*(int*)(raw+0x308)||ent.s.modelindex2!=*(int*)(raw+0xa8)||ent.s.density!=*(int*)(raw+0xf4)||ent.splashDamage!=*(int*)(raw+0x2d4)){fprintf(stderr,"spawn type%d case%d mismatch\n",t,kase);return 1;}++n;}
 printf("%d whole G_SpawnItem comparisons passed\n",n);n=0;
 for(kase=0;kase<512;++kase){gentity_t*e=&g_entities[10];byte*rh;void*expectedEnemy;int expectedDamage;memset(base+0x226e2a0,0,16*0x57c);memset(g_entities,0,16*sizeof(gentity_t));owner=kase%2?11:10;hit=(kase/2)%4+10;solid=(kase/8)%2;noimpact=(kase/16)%2;rh=base+0x226e2a0+hit*0x57c;
 e->r.ownerNum=*(int*)(raw+0x180)=owner;e->takedamage=*(int*)(raw+0x2cc)=1;g_entities[hit].takedamage=*(int*)(rh+0x2cc)=(kase/32)%2;g_entities[owner].client=*(void**)(base+0x226e2a0+owner*0x57c+0x190)=(kase/64)%2?(void*)1:NULL;e->die=(void*)dieSink;*(void**)(raw+0x2bc)=dieSink;
 VectorSet(e->r.currentOrigin,kase+.25f,-12.5f,31.75f);VectorSet(e->r.mins,-10,-11,-12);VectorSet(e->r.maxs,10,11,12);memcpy(raw+0x168,e->r.currentOrigin,12);memcpy(raw+0x134,e->r.mins,12);memcpy(raw+0x140,e->r.maxs,12);VectorSet(target,25.5f,kase-.25f,70.25f);memset(&configured,0,sizeof(configured));configured.entityNum=hit;configured.startsolid=solid;configured.surfaceFlags=noimpact?SURF_NOIMPACT:0;
 count=bad=0;mode=1;prop(raw,target);if(bad)return 4;expectedCount=count;memcpy(expected,events,sizeof(events));expectedEnemy=*(void**)(raw+0x2ec);expectedDamage=*(int*)(raw+0x2cc);count=bad=0;mode=0;G_RunItemProp(e,target);
 if(bad||count!=expectedCount||memcmp(expected,events,count*sizeof(int))||e->takedamage!=expectedDamage||(expectedEnemy?e->enemy!=&g_entities[hit]:e->enemy!=NULL)){fprintf(stderr,"prop case%d mismatch\n",kase);return 1;}++n;}
 printf("%d whole G_RunItemProp comparisons passed\n",n);return 0;
}
