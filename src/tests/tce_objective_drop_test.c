#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
static byte rawEntity[0x57c],rawClient[0x2000],rawDrop[0x57c];
static gentity_t entity,drop;static gclient_t client;
static int objectiveMode,stage;
static int originalMode,randomValue,randomCount,traceCount,launchCount,passSeen,maskSeen,ownerSeen;
static vec3_t startSeen,endSeen,minsSeen,maxsSeen,originSeen,velocitySeen;
static int randomFixed(void){if(stage!=0)exit(5);stage=1;++randomCount;return randomValue;}
static int hook(void *p,void *f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
static void traceSink(trace_t*t,const vec3_t s,const vec3_t mi,const vec3_t ma,const vec3_t e,int pass,int mask){if(stage!=1)exit(5);stage=2;++traceCount;VectorCopy(s,startSeen);VectorCopy(e,endSeen);VectorCopy(mi,minsSeen);VectorCopy(ma,maxsSeen);passSeen=pass;maskSeen=mask;memset(t,0,sizeof(*t));VectorSet(t->endpos,17,31,53);}
static gentity_t *launchSink(gitem_t *item,vec3_t o,vec3_t v,int owner){if(stage!=2)exit(5);stage=3;++launchCount;ownerSeen=owner;if(!item||strcmp(item->classname,objectiveMode?"ammo_VIP":"ammo_dynamite")){puts("wrong objective item");exit(1);}VectorCopy(o,originSeen);VectorCopy(v,velocitySeen);return originalMode?(gentity_t*)rawDrop:&drop;}
static int closeVector(const vec3_t a,const vec3_t b){int j;for(j=0;j<3;++j)if(!_finite(a[j])||!_finite(b[j])||fabs(a[j]-b[j])>.00003+fabs(b[j])*.000002)return 0;return 1;}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*original)(void*);int k,j,n=0;
const float pitches[]={-180,-31,-30,-29,0,29,30,31,180};
if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x5cfe0);
if(!hook(b+0xa20e4,randomFixed)||!hook(rand,randomFixed)||!hook(b+0x8ae80,traceSink)||!hook(trap_Trace,traceSink)||!hook(b+0x5df70,launchSink)||!hook(LaunchItem,launchSink))return 2;
entity.client=&client;*(void**)(rawEntity+0x190)=rawClient;
for(objectiveMode=0;objectiveMode<2;objectiveMode++)for(k=0;k<9000;++k){vec3_t expectedEnd,expectedVelocity;int team=k%4;
 original=(void*)(b+(objectiveMode?0x5d220:0x5cfe0));
 level.time=*(int*)(b+0x7febe0)=100000+k;level.tceBombDropCount=*(int*)(b+0x2223934)=k%4;
 entity.s.number=*(int*)rawEntity=k%64;client.ps.clientNum=*(int*)(rawClient+0xa0)=(k+3)%64;
 client.ps.viewheight=*(int*)(rawClient+0xbc)=k%101-30;
 client.ps.persistant[PERS_TEAM]=*(int*)(rawClient+0x11c)=team;
 VectorSet(client.ps.origin,k%29-14.f,k%53-26.f,k%71-35.f);memcpy(rawClient+0x14,client.ps.origin,12);
 VectorSet(client.ps.viewangles,pitches[k%9],k%721-360.f,k%181-90.f);memcpy(rawClient+0xb0,client.ps.viewangles,12);
 randomValue=k%3==0?0:k%3==1?32767:12345;
 memset(rawDrop,0,sizeof(rawDrop));memset(&drop,0,sizeof(drop));stage=randomCount=traceCount=launchCount=0;originalMode=1;original(rawEntity);
 if(randomCount!=1||traceCount!=1||launchCount!=1||passSeen!=entity.s.number||maskSeen!=1||ownerSeen!=client.ps.clientNum)return 3;
 VectorCopy(endSeen,expectedEnd);VectorCopy(velocitySeen,expectedVelocity);
 stage=randomCount=traceCount=launchCount=0;originalMode=0;if(objectiveMode)G_TCEDropVip(&entity);else G_TCEDropBomb(&entity);
 if(randomCount!=1||traceCount!=1||launchCount!=1||passSeen!=entity.s.number||maskSeen!=1||ownerSeen!=client.ps.clientNum||drop.s.otherEntityNum2!=*(int*)(rawDrop+0x90)||drop.s.effect2Time!=*(int*)(rawDrop+0x110)||drop.s.effect3Time!=*(int*)(rawDrop+0x114)||drop.s.otherEntityNum!=*(int*)(rawDrop+0x8c)||drop.r.svFlags!=*(int*)(rawDrop+0x128)||level.tceBombDropCount!=*(int*)(b+0x2223934)||!closeVector(endSeen,expectedEnd)||!closeVector(velocitySeen,expectedVelocity)){fprintf(stderr,"objective drop case%d mismatch\n",k);return 1;}
 for(j=0;j<3;++j)if(startSeen[j]!=client.ps.origin[j]||minsSeen[j]!=(j==2?0:-10)||maxsSeen[j]!=(j==2?20:10)||originSeen[j]!=(j==0?17:j==1?31:53))return 4;
 ++n;
}
printf("%d complete bomb/VIP-drop controller comparisons passed\n",n);return 0;}
