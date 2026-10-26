#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../cgame/tce_smoke_grenade.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
static unsigned char *base;
static char events[16384],serverInfo[1024],multiInfo[128],targets[32][512];
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void event(const char *fmt,...){va_list ap;va_start(ap,fmt);vsprintf(events+strlen(events),fmt,ap);va_end(ap);}
static const char *config(int id){event("config:%d;",id);if(id==0)return serverInfo;if(id==13)return multiInfo;if(id>=753&&id<785)return targets[id-753];return "INVALID";}
static void set(const char *key,const char *value){event("set:%s=%s;",key,value);}
static void update(vmCvar_t *v){if(v==&cg_antilag||(void *)v==base+0x4889900)event("update:antilag;");else if(v==&cg_gameType||(void *)v==base+0x488c180)event("update:gametype;");else{puts("Wrong update pointer");exit(1);}}
static const char *translate(const char *s){event("translate:%s;",s);return s;}
static void transform(float *x,float *y){event("transform:%g,%g;",*x,*y);*x=*x*.25f+7;*y=*y*.5f-9;}
static int serverSame(void){return cgs.gametype==*(int *)(base+0x257e100)&&cgs.antilag==*(int *)(base+0x257e104)&&cgs.timelimit==*(float *)(base+0x257e108)&&cgs.maxclients==*(int *)(base+0x257e10c)&&!strcmp(cgs.mapname,(char *)base+0x257e110)&&!strcmp(cgs.rawmapname,(char *)base+0x257e150)&&cgs.weaponRestrictions==*(float *)(base+0x257e210)&&cgs.minclients==*(int *)(base+0x3fb780c)&&tceSmokeNewBBox==*(int *)(base+0x3fdf380)&&cgs.tceLeanMode==*(int *)(base+0x3fdf384)&&cgs.tceKillMessage==*(int *)(base+0x3fdf388)&&!strcmp(cgs.tceVersion,(char *)base+0x3fdf394)&&cg_gameType.integer==*(int *)(base+0x488c18c)&&cg_antilag.integer==*(int *)(base+0x488990c)&&cg_redlimbotime.integer==*(int *)(base+0x488a32c)&&cg_bluelimbotime.integer==*(int *)(base+0x488b2ec);}
static void seedSpawns(void){memset(cg.spawnPoints,0x31,sizeof(cg.spawnPoints));memset(base+0x4081fb0,0x31,sizeof(cg.spawnPoints));memset(cg.spawnCoordsUntransformed,0x31,sizeof(cg.spawnCoordsUntransformed));memset(base+0x4082fb0,0x31,sizeof(cg.spawnCoordsUntransformed));memset(cg.spawnCoords,0x31,sizeof(cg.spawnCoords));memset(base+0x4083130,0x31,sizeof(cg.spawnCoords));memset(cg.spawnTeams,0,sizeof(cg.spawnTeams));memset(base+0x40832b0,0,sizeof(cg.spawnTeams));memset(cg.spawnTeams_old,0,sizeof(cg.spawnTeams_old));memset(base+0x4083330,0,sizeof(cg.spawnTeams_old));memset(cg.spawnTeams_changeTime,0,sizeof(cg.spawnTeams_changeTime));memset(base+0x40833b0,0,sizeof(cg.spawnTeams_changeTime));memset(cg.spawnPlayerCounts,0,sizeof(cg.spawnPlayerCounts));memset(base+0x4083430,0,sizeof(cg.spawnPlayerCounts));cg.spawnCount=*(int *)(base+0x40834b0)=17;}
static int spawnSame(void){return cg.spawnCount==*(int *)(base+0x40834b0)&&!memcmp(cg.spawnPoints,base+0x4081fb0,sizeof(cg.spawnPoints))&&!memcmp(cg.spawnCoordsUntransformed,base+0x4082fb0,sizeof(cg.spawnCoordsUntransformed))&&!memcmp(cg.spawnCoords,base+0x4083130,sizeof(cg.spawnCoords))&&!memcmp(cg.spawnTeams,base+0x40832b0,sizeof(cg.spawnTeams))&&!memcmp(cg.spawnTeams_old,base+0x4083330,sizeof(cg.spawnTeams_old))&&!memcmp(cg.spawnTeams_changeTime,base+0x40833b0,sizeof(cg.spawnTeams_changeTime))&&!memcmp(cg.spawnPlayerCounts,base+0x4083430,sizeof(cg.spawnPlayerCounts));}
int main(int argc,char **argv){HMODULE h;int v,l,a,n,layer,missing,i,cases=0;char expected[16384];
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;base=(unsigned char *)h;
 patch(base+0x481a0,config);patch(CG_ConfigString,config);patch(base+0x64c10,set);patch(trap_Cvar_Set,set);patch(base+0x64bf0,update);patch(trap_Cvar_Update,update);patch(base+0x2a830,translate);patch(CG_TranslateString,translate);patch(base+0x192e0,transform);patch(CG_TransformToCommandMapCoord,transform);
 for(v=0;v<16;++v)for(l=0;l<2;++l)for(a=0;a<3;++a){
  sprintf(serverInfo,"\\g_gametype\\%d\\g_antilag\\%d\\timelimit\\%g\\sv_maxclients\\%d\\mapname\\obj_delta\\g_redlimbotime\\%d\\g_bluelimbotime\\%d\\g_heavyWeaponRestriction\\%d\\g_minGameClients\\%d\\g_newbbox\\%d\\g_leanmode\\%d\\g_killmessage\\%d\\tce_version\\TC test %d\\voteFlags\\37",v%8,v%3,v*1.25,2+v,v*1000,v*2000,v*13,v-2,v%2,v%4,v%3,v);
  if(v==15)serverInfo[0]=0;
  cgs.localServer=*(int *)(base+0x257e0fc)=l;authLevel.integer=*(int *)(base+0x4849aec)=a;
  events[0]=0;((void(__cdecl *)(void))(base+0x5b290))();strcpy(expected,events);events[0]=0;CG_ParseServerinfo();
  if(strcmp(expected,events)||!serverSame()){printf("server info mismatch %d/%d/%d\n%s\n%s\n",v,l,a,expected,events);return 1;}++cases;
 }
 for(n=0;n<=31;++n)for(layer=0;layer<2;++layer)for(missing=0;missing<6;++missing){
  sprintf(multiInfo,"\\numspawntargets\\%d",n);if(missing==5)multiInfo[0]=0;
  for(i=0;i<32;++i){sprintf(targets[i],"\\spawn_targ\\Zone %d\\x\\%g\\y\\%g\\z\\%g\\t\\%d\\c\\%d",i,i*23.5,i*-17.25,i*100.5,i%4,i+2);if(i==n/2&&missing>0&&missing<5){const char *key=missing==1?"spawn_targ":missing==2?"x":missing==3?"y":"z";Info_RemoveKey(targets[i],key);}}
  seedSpawns();cgs.ccLayers=*(int *)(base+0x3fc4274)=layer;cg.time=*(int *)(base+0x407dea4)=3141;
  events[0]=0;((void(__cdecl *)(void))(base+0x5b870))();strcpy(expected,events);events[0]=0;CG_ParseSpawns();
  if(strcmp(expected,events)||!spawnSame()){printf("spawn mismatch %d/%d/%d\n%s\n%s\n",n,layer,missing,expected,events);return 1;}++cases;
 }
 printf("%d whole original serverinfo/spawn-target comparisons passed\n",cases);return 0;
}
