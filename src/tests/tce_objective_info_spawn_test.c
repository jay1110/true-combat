#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern void SP_trigger_objective_info(gentity_t*),InitTrigger(gentity_t*),Think_SetupObjectiveInfo(gentity_t*);
extern void G_SetConfigStringValue(int,const char*,const char*);
static byte*base;static int side,variant,bad,count;static jmp_buf jump;
typedef struct{int k,a;char key[64],value[128];}event;static event events[48];
static void*entity(void){return side?(void*)&g_entities[13]:(void*)(base+0x226e2a0+13*0x57c);}
static void note(int k,int a,const char*s,const char*t){event*e;if(count>=48)exit(3);e=&events[count++];memset(e,0,sizeof(*e));e->k=k;e->a=a;if(s)Q_strncpyz(e->key,s,sizeof(e->key));if(t)Q_strncpyz(e->value,t,sizeof(e->value));}
static int hook(void*p,void*f){DWORD a,b;byte*q=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&a))return 0;q[0]=0xe9;*(int*)(q+1)=(byte*)f-q-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,a,&b);return 1;}
static int spawnInt(const char*k,const char*d,int*out,const char*f,int line){note(1,0,k,d);*out=!strcmp(k,"objnumber")?variant%4:variant*3;return 1;}
static int spawnString(const char*k,const char*d,char**out,const char*f,int line){int bit=-1;note(2,0,k,d);if(!strcmp(k,"score")){*out=variant&1?"-1.25":"23.75";return 1;}if(!strcmp(k,"customimage"))bit=0;if(!strcmp(k,"customaxisimage"))bit=1;if(!strcmp(k,"customalliesimage"))bit=2;if(!strcmp(k,"customalliedimage"))bit=3;if(bit<0){bad=1;*out="";return 0;}*out=(char*)(variant&(1<<bit)?k:d);return !!(variant&(1<<bit));}
static int shader(const char*s){note(3,0,s,NULL);return !strcmp(s,"customimage")?101:!strcmp(s,"customaxisimage")?102:!strcmp(s,"customalliesimage")?103:104;}
static void value(int index,const char*k,const char*v){note(4,index,k,v);}
static void set(int index,const char*v){note(5,index,NULL,v);}
static void fatal(const char*f,...){note(6,0,NULL,f);longjmp(jump,1);}
static void init(void*p){int k;note(7,0,NULL,NULL);if(p!=entity())bad=1;for(k=0;k<3;k++){float org=(variant&32)?(float)(k*13)-1.75f:0;float lo=(float)(variant-k*3)+.25f,hi=(float)(variant+k*7)+.5f;if(side){g_entities[13].s.origin[k]=org;g_entities[13].r.absmin[k]=lo;g_entities[13].r.absmax[k]=hi;g_entities[13].s.pos.trBase[k]+=.5f;}else{*(float*)((byte*)p+0x5c+k*4)=org;*(float*)((byte*)p+0x150+k*4)=lo;*(float*)((byte*)p+0x15c+k*4)=hi;*(float*)((byte*)p+0x18+k*4)+=.5f;}}}
static void link(void*p){note(8,0,NULL,NULL);if(p!=entity())bad=1;}
typedef struct{int n,flags,objnum,team,sv,type,next,think,counter;float accuracy,origin[3],current[3],pos[3];event log[48];}result;
int main(int argc,char**argv){HMODULE dll;void(__cdecl*original)(void*);int gm,map,fl,v,n=0,k;gentity_t*e=&g_entities[13];byte*r;result a,b;const int modes[]={2,5,7,0};const int flags[]={0,2,4,0x42,0x82,0x40,0x80,0xc2};const char*maps[]={"dem_northport","DEM_RAILHOUSE","dem_northport2","obj_northport"};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;base=(byte*)dll;r=base+0x226e2a0+13*0x57c;original=(void*)(base+0x93f30);
#define HOOK(rva,fn,repl) if(!hook(base+rva,repl)||!hook(fn,repl))return 2
 HOOK(0x85a30,G_SpawnIntExt,spawnInt);HOOK(0x85940,G_SpawnStringExt,spawnString);HOOK(0x94830,G_ShaderIndex,shader);HOOK(0x938a0,G_SetConfigStringValue,value);HOOK(0x8adc0,trap_SetConfigstring,set);HOOK(0x5ee10,G_Error,fatal);HOOK(0x91e90,InitTrigger,init);HOOK(0x8afa0,trap_LinkEntity,link);
 for(gm=0;gm<4;gm++)for(map=0;map<4;map++)for(fl=0;fl<8;fl++)for(v=0;v<256;v++)for(side=0;side<2;side++){
 variant=v;count=bad=0;memset(events,0,sizeof(events));memset(e,0,sizeof(*e));memset(r,0,0x57c);memset(&b,0,sizeof(b));
 g_gametype.integer=*(int*)(base+0x6d8d4c)=modes[gm];strcpy(level.rawmapname,maps[map]);strcpy((char*)base+0x7feb98,maps[map]);level.time=*(int*)(base+0x7febe0)=123456;level.numOidTriggers=*(int*)(base+0x221a420)=v%20;
 e->spawnflags=*(int*)(r+0x1a8)=flags[fl];e->track=v&64?NULL:"objective track";*(char**)(r+0x3b0)=e->track;e->spawnitem=v&128?NULL:"override";*(char**)(r+0x394)=e->spawnitem;e->message=v&16?NULL:"objective shortname";*(char**)(r+0x248)=e->message;e->target=v&8?"target":NULL;*(char**)(r+0x254)=e->target;e->r.svFlags=*(int*)(r+0x128)=0x123;e->nextthink=*(int*)(r+0x29c)=77;
 for(k=0;k<3;k++)e->s.pos.trBase[k]=*(float*)(r+0x18+k*4)=(float)(v-k*4)+.25f;
 if(!setjmp(jump)){if(side)SP_trigger_objective_info(e);else original(r);}b.n=count;memcpy(b.log,events,sizeof(events));
#define FIELD(name,member,off) b.name=side?e->member:*(int*)(r+off)
 FIELD(flags,spawnflags,0x1a8);FIELD(objnum,count2,0x3a4);FIELD(team,s.teamNum,0x108);FIELD(sv,r.svFlags,0x128);FIELD(type,s.eType,4);FIELD(next,nextthink,0x29c);b.think=side?(e->think==Think_SetupObjectiveInfo):(*(void**)(r+0x2a4)==base+0x93910);b.counter=side?level.numOidTriggers:*(int*)(base+0x221a420);b.accuracy=side?e->accuracy:*(float*)(r+0x420);memcpy(b.origin,side?e->s.origin:(float*)(r+0x5c),12);memcpy(b.current,side?e->r.currentOrigin:(float*)(r+0x168),12);memcpy(b.pos,side?e->s.pos.trBase:(float*)(r+0x18),12);
 if(!side)a=b;else{if(bad||memcmp(&a,&b,sizeof(a))){int q;printf("objective info mismatch gm%d map%d flags%d v%d events%d/%d bad%d\n",modes[gm],map,flags[fl],v,a.n,b.n,bad);for(q=0;q<a.n||q<b.n;q++)printf("%d: %d %d %s %s / %d %d %s %s\n",q,a.log[q].k,a.log[q].a,a.log[q].key,a.log[q].value,b.log[q].k,b.log[q].a,b.log[q].key,b.log[q].value);return 1;}n++;}}
 printf("PASS objective info spawn %d whole cases\n",n);return 0;
}
