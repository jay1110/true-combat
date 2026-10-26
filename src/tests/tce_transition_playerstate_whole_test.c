#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
extern void CG_CheckLocalSounds(playerState_t*,playerState_t*);
extern void CG_CheckPlayerstateEvents(playerState_t*,playerState_t*);
extern void CG_CheckAmmo(void);
extern void CG_DamageFeedback(int,int,int);
static byte*base;static int run,n,events[16][40],client;static playerState_t *cur,*old;static byte *rawCur,*rawOld;
typedef struct{int offset;size_t native;}mapping;
#define M(off,field) {off,offsetof(cg_t,field)}
static mapping maps[]={M(0x407de98,thisFrameTeleport),M(0x407deb4,mapRestart),M(0x4080f18,weaponFireTime),M(0x4080f10,lastFiredWeaponTime),M(0x408a3a4,proneMovingTime),M(0x407ef60,duckChange),M(0x407ef64,duckTime),M(0x40a4948,tceWeaponDuckTime),M(0x40a4958,tceProneTime),M(0x40a4950,tceStanceTime),M(0x40a4954,tceActionTransitionTime)};
static void call(int id,int a,int b,int c){int j,*p=events[n++];p[0]=id;p[1]=a;p[2]=b;p[3]=c;for(j=0;j<sizeof(maps)/sizeof(maps[0]);j++)p[4+j]=run?*(int*)(base+maps[j].offset):*(int*)((byte*)&cg+maps[j].native);p[20]=run?*(int*)(rawOld+0x120):old->persistant[PERS_SPAWN_COUNT];}
static void local(playerState_t*p,playerState_t*o){call(1,p==(run?(void*)rawCur:cur),o==(run?(void*)rawOld:old),0);}
static void event(playerState_t*p,playerState_t*o){call(2,p==(run?(void*)rawCur:cur),o==(run?(void*)rawOld:old),0);}
static void damage(int y,int p,int d){call(3,y,p,d);}
static void respawn(qboolean revive){call(4,revive,0,0);}
static void ammo(void){call(5,0,0,0);}
static void finish(int a,int b){call(6,a,b,0);}
static void command(const char*s){call(7,strcmp(s,"-zoom\n"),0,0);}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
int main(int argc,char**argv){HMODULE m;byte a[0x700],b[0x700],snap[0x1000];playerState_t ps,ops;snapshot_t ns;void(__cdecl*original)(void*,void*);int k,j,count,copy[16][40];int flags[]={0,EF_PRONE,EF_PRONE_MOVING|EF_ZOOMING,EF_FIRING,EF_PRONE|EF_PRONE_MOVING};
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;base=(byte*)m;original=(void*)(base+0x57060);if(sizeof(ps)!=0x5ac){printf("ps layout %u\n",(unsigned)sizeof(ps));return 3;}cur=&ps;old=&ops;rawCur=a;rawOld=b;
#define H(off,fn,repl) hook(base+off,repl);hook(fn,repl)
 H(0x56fc0,CG_CheckLocalSounds,local);H(0x56ee0,CG_CheckPlayerstateEvents,event);H(0x56930,CG_DamageFeedback,damage);H(0x56cb0,CG_Respawn,respawn);H(0x568b0,CG_CheckAmmo,ammo);H(0x74580,CG_FinishWeaponChange,finish);H(0x64d60,trap_SendConsoleCommand,command);
 for(k=0;k<16384;k++){
  memset(&ps,0,sizeof(ps));memset(&ops,0,sizeof(ops));memset(snap,0,sizeof(snap));memset(&ns,0,sizeof(ns));client=k%64;ps.clientNum=client;ops.clientNum=k%4?client:(client+1)%64;ps.eFlags=flags[(k/4)%5];ops.eFlags=flags[(k/20)%5];ps.pm_flags=(k&8)?PMF_LADDER:0;ops.pm_flags=(k&16)?PMF_LADDER:0;ps.stats[STAT_TCE_WEAPON_FLAGS]=(k&32?0x4000:0)|(k&64?0x20:0);ops.stats[STAT_TCE_WEAPON_FLAGS]=(k&128?0x4000:0)|(k&256?0x20:0);
  ps.persistant[PERS_SPAWN_COUNT]=k%3;ops.persistant[PERS_SPAWN_COUNT]=(k/3)%3;ps.persistant[PERS_REVIVE_COUNT]=k%2;ops.persistant[PERS_REVIVE_COUNT]=(k/2)%2;ps.persistant[PERS_TEAM]=(k/7)%4;ps.damageEvent=k%3;ops.damageEvent=(k/3)%3;ps.damageCount=k%5;ps.damageYaw=k%256;ps.damagePitch=(k/9)%256;ps.weapon=(k&512)?20:3;ps.nextWeapon=k%64;ps.viewheight=k%3?40:64;ops.viewheight=(k/3)%3?40:64;
  memcpy(a,&ps,sizeof(ps));memcpy(b,&ops,sizeof(ops));cg.snap=&ns;*(void**)(base+0x40352c4)=snap;ns.ps.pm_type=*(int*)(snap+0x30)=(k/31)%7;cg.mvTotalClients=*(int*)(base+0x4086398)=(k/13)%3-1;cg.clientNum=*(int*)(base+0x40352a4)=(k&1024)?client:(client+2)%64;cg.time=*(int*)(base+0x407dea4)=k-5000;cg.frametime=*(int*)(base+0x407dea0)=17;cg.weaponSelect=*(int*)(base+0x407ef70)=k%2?62:3;cg.tceAimActive=*(int*)(base+0x40a496c)=(k/17)%2;
  for(j=0;j<sizeof(maps)/sizeof(maps[0]);j++)*(int*)((byte*)&cg+maps[j].native)=*(int*)(base+maps[j].offset)=j*71+k%3;
  cg.mapRestart=*(int*)(base+0x407deb4)=k%2;cg.tceWeaponDuckTime=*(int*)(base+0x40a4948)=cg.time+(k%5-3)*100;cg.tceActionTransitionTime=*(int*)(base+0x40a4954)=cg.time+(k%7-4)*100;cg.weaponFireTime=*(int*)(base+0x4080f18)=k%4==0?500:k%4==1?501:0;cg.predictedPlayerEntity.voiceChatSpriteTime=*(int*)(base+0x407ed78)=81;cg_entities[client].voiceChatSpriteTime=*(int*)(base+0x488e10c+client*0xa94)=82;
  run=1;n=0;memset(events,0,sizeof(events));original(a,b);count=n;memcpy(copy,events,sizeof(copy));run=0;n=0;memset(events,0,sizeof(events));CG_TransitionPlayerState(&ps,&ops);
  if(count!=n||memcmp(copy,events,sizeof(copy))||memcmp(a,&ps,sizeof(ps))||memcmp(b,&ops,sizeof(ops))||cg.predictedPlayerEntity.voiceChatSpriteTime!=*(int*)(base+0x407ed78)||cg_entities[client].voiceChatSpriteTime!=*(int*)(base+0x488e10c+client*0xa94)){printf("transition playerstate case%d calls%d/%d\n",k,count,n);return 1;}
  for(j=0;j<sizeof(maps)/sizeof(maps[0]);j++)if(*(int*)((byte*)&cg+maps[j].native)!=*(int*)(base+maps[j].offset)){printf("transition playerstate case%d field%x\n",k,maps[j].offset);return 1;}
 }puts("16384 whole TransitionPlayerState comparisons passed");return 0;}
