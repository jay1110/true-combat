#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdio.h>
/* Whole feeder callbacks and real map/campaign selectors. Only engine traps and
 * the independently verified server-status controller are replaced. */
static char trace[8192];static int scenario;
static void patch(void *a,void *f){unsigned char*p=a;DWORD o,t;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static int shader(const char*s){sprintf(trace+strlen(trace),"shader:%s;",s);return scenario&1?0:73;}
static void set(const char*n,const char*v){sprintf(trace+strlen(trace),"set:%s:%s;",n,v);}
static void info(int s,int i,char*out,int size){sprintf(trace+strlen(trace),"info:%d:%d;",s,i);Q_strncpyz(out,scenario&2?"\\mapname\\obj_delta":"\\mapname\\",size);}
static int stop(int h){sprintf(trace+strlen(trace),"stop:%d;",h);return 0;}
static int play(const char*s,int x,int y,int w,int h,int flags){sprintf(trace+strlen(trace),"play:%s:%d:%d:%d:%d:%d;",s,x,y,w,h,flags);return 91;}
static void menu(menuDef_t*m,int f,int i,const char*n){sprintf(trace+strlen(trace),"menu:%d:%d;",f,i);}
static void status(qboolean force){sprintf(trace+strlen(trace),"status:%d;",force);}
static struct{int*source;int off;} state[]={
 {&updateModel,0x33204},{&ui_mapIndex.integer,0x29184c},{&ui_currentMap.integer,0x2913cc},
 {&ui_currentNetMap.integer,0x291f0c},{&ui_campaignIndex.integer,0x2d9dac},
 {&ui_currentCampaign.integer,0x2d902c},{&ui_currentNetCampaign.integer,0x28fc2c},
 {&ui_currentCampaignCompleted.integer,0x2dbf6c},{&uiInfo.serverStatus.currentServer,0x2cf300},
 {&uiInfo.serverStatus.currentServerPreview,0x2d1310},{&uiInfo.currentFoundPlayerServer,0x2d3d3c},
 {&uiInfo.playerIndex,0x2b349c},{&uiInfo.teamIndex,0x2b3494},{&uiInfo.modIndex,0x2ce2d0},
 {&uiInfo.movieIndex,0x2ceae0},{&uiInfo.previewMovie,0x2ceae4},{&uiInfo.savegameIndex,0x2cf2ec},
 {&uiInfo.demoIndex,0x2ce6d8},{&uiInfo.profileIndex,0x2ce0c4}};
static const char* names[]={"first","second","third","fourth"};
static void setup(unsigned char*b,int mask,int cache,int count,int mode){int i;
 memset(&uiInfo,0,sizeof(uiInfo));
 uiInfo.characterCount=*(int*)(b+0x2b2070)=count;uiInfo.q3HeadCount=*(int*)(b+0x2d3d54)=count;
 uiInfo.mapCount=*(int*)(b+0x2b52a8)=count;uiInfo.campaignCount=*(int*)(b+0x2b96ac)=count;
 uiInfo.savegameCount=*(int*)(b+0x2cf2e8)=count;uiInfo.numFoundPlayerServers=*(int*)(b+0x2d3d40)=3;
 ui_gameType.integer=*(int*)(b+0x2d926c)=0;uiInfo.gameTypes[0].gtEnum=*(int*)(b+0x2b328c)=mode;
 ui_netGameType.integer=*(int*)(b+0x2dc50c)=mode;ui_netSource.integer=*(int*)(b+0x28fd4c)=scenario%3;
 for(i=0;i<sizeof(state)/sizeof(state[0]);i++)*state[i].source=*(int*)(b+state[i].off)=0;
 uiInfo.previewMovie=*(int*)(b+0x2ceae4)=cache;
 strcpy(uiInfo.serverStatusAddress,"unchanged");strcpy((char*)b+0x2d1730,"unchanged");
 for(i=0;i<4;i++){
  unsigned char*rm=b+0x2b52ac+i*0x88,*rc=b+0x2b96b0+i*0x74,*rh=b+0x2b2078+i*0x10;
  uiInfo.characterList[i].name=*(const char**)rh=names[i];uiInfo.characterList[i].imageName=*(const char**)(rh+4)=names[i];
  uiInfo.characterList[i].headImage=*(int*)(rh+8)=cache;uiInfo.characterList[i].female=*(int*)(rh+12)=i%2;
  strcpy(uiInfo.q3HeadNames[i],names[i]);strcpy((char*)b+0x2d3d58+i*64,names[i]);
  uiInfo.q3HeadIcons[i]=*(int*)(b+0x2d7d58+i*4)=20+i;
  uiInfo.mapList[i].mapName=*(const char**)rm=names[i];uiInfo.mapList[i].imageName=*(const char**)(rm+8)=names[i];
  uiInfo.mapList[i].levelShot=*(int*)(rm+0x5c)=cache;uiInfo.mapList[i].active=*(int*)(rm+0x60)=(mask>>i)&1;
  uiInfo.campaignList[i].campaignShortName=*(const char**)rc=names[i];uiInfo.campaignList[i].campaignName=*(const char**)(rc+4)=names[i];
  uiInfo.campaignList[i].campaignShot=*(int*)(rc+0x5c)=cache;uiInfo.campaignList[i].campaignCinematic=*(int*)(rc+0x58)=cache;
  uiInfo.campaignList[i].unlocked=*(int*)(rc+0x60)=(mask>>i)&1;
  uiInfo.campaignList[i].progress=*(int*)(rc+0x64)=scenario%3;uiInfo.campaignList[i].mapCount=*(int*)(rc+0x14)=i;
  uiInfo.campaignList[i].order=*(int*)(rc+0x6c)=3-i;uiInfo.campaignList[i].typeBits=*(int*)(rc+0x70)=((mask>>i)&1)?5:0;
  uiInfo.savegameList[i].name=*(const char**)(b+0x2ceae8+i*8)=names[i];uiInfo.savegameList[i].sshotImage=*(int*)(b+0x2ceaec+i*8)=cache;
  uiInfo.profileList[i].name=*(const char**)(b+0x2cdec0+i*8)=names[i];
  uiInfo.serverStatus.displayServers[i]=*(int*)(b+0x2cf304+i*4)=i+7;
  strcpy(uiInfo.foundPlayerServerAddresses[i],names[i]);strcpy((char*)b+0x2d353c+i*64,names[i]);
 }
}
int main(int argc,char**argv){HMODULE h;unsigned char*b;int f,idx,mask,cache,c,mode,pass,i,cases=0,expected,got,values[32];char saved[8192],address[64];
 const float feeders[]={FEEDER_HEADS,FEEDER_Q3HEADS,FEEDER_MAPS,FEEDER_ALLMAPS,FEEDER_CAMPAIGNS,FEEDER_ALLCAMPAIGNS,FEEDER_SAVEGAMES,FEEDER_SERVERS,FEEDER_GLINFO,FEEDER_SERVERSTATUS,FEEDER_FINDPLAYER,FEEDER_PLAYER_LIST,FEEDER_TEAM_LIST,FEEDER_MODS,FEEDER_CINEMATICS,FEEDER_DEMOS,FEEDER_PROFILES,99};
 const int caches[]={-1,0,44},modes[]={2,4,5,7};
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char*)h;
#define PATCH(off,fn,target) patch(b+off,fn);patch(target,fn)
 PATCH(0x20730,shader,trap_R_RegisterShaderNoMip);PATCH(0x20530,set,trap_Cvar_Set);PATCH(0x20ba0,info,trap_LAN_GetServerInfo);
 PATCH(0x20e90,stop,trap_CIN_StopCinematic);PATCH(0x20e60,play,trap_CIN_PlayCinematic);
 PATCH(0x1d8f0,menu,Menu_SetFeederSelection);PATCH(0xc4d0,status,UI_BuildServerStatus);
 for(f=0;f<18;f++)for(idx=-1;idx<5;idx++)for(mask=0;mask<16;mask++)for(cache=0;cache<3;cache++)for(c=0;c<5;c++)for(mode=0;mode<4;mode++){
  scenario=mask;setup(b,mask,caches[cache],c,modes[mode]);trace[0]=0;
  expected=((int(__cdecl*)(float,int))(b+0x11820))(feeders[f],idx);strcpy(saved,trace);trace[0]=0;
  got=UI_FeederItemImage(feeders[f],idx);
  if(expected!=got||strcmp(saved,trace)){printf("image mismatch f%.1f idx%d mask%d cache%d count%d mode%d: %d/%d %s/%s\n",feeders[f],idx,mask,caches[cache],c,modes[mode],expected,got,saved,trace);return 3;}
  for(i=0;i<4;i++)if(uiInfo.characterList[i].headImage!=*(int*)(b+0x2b2080+i*16)||uiInfo.mapList[i].levelShot!=*(int*)(b+0x2b5308+i*0x88)||uiInfo.campaignList[i].campaignShot!=*(int*)(b+0x2b970c+i*0x74)||uiInfo.savegameList[i].sshotImage!=*(int*)(b+0x2ceaec+i*8))return 4;
  ++cases;
 }
 for(f=0;f<18;f++)for(idx=-1;idx<5;idx++)for(mask=1;mask<16;mask++)for(cache=0;cache<3;cache++)for(mode=0;mode<4;mode++){
  /* These selectors have no original bounds guard for network/profile rows. */
  if((f==7||f==10||f==16)&&(idx<0||idx>3))continue;
  for(pass=0;pass<2;pass++){
   scenario=mask;setup(b,mask,caches[cache],4,modes[mode]);trace[0]=0;
   if(!pass){((void(__cdecl*)(float,int))(b+0xd120))(feeders[f],idx);strcpy(saved,trace);strcpy(address,(char*)b+0x2d1730);for(i=0;i<sizeof(state)/sizeof(state[0]);i++)values[i]=*(int*)(b+state[i].off);for(i=0;i<4;i++)values[20+i]=*(int*)(b+0x2b9708+i*0x74);}
   else{UI_FeederSelection(feeders[f],idx);if(strcmp(saved,trace)||strcmp(address,uiInfo.serverStatusAddress)){printf("selection mismatch f%.1f idx%d mask%d cache%d mode%d\n%s\n%s\n",feeders[f],idx,mask,caches[cache],modes[mode],saved,trace);return 5;}for(i=0;i<sizeof(state)/sizeof(state[0]);i++)if(values[i]!=*state[i].source){printf("selection state %d mismatch f%.1f idx%d\n",i,feeders[f],idx);return 6;}for(i=0;i<4;i++)if(values[20+i]!=uiInfo.campaignList[i].campaignCinematic)return 7;}
  }++cases;
 }
 printf("%d whole feeder image/selection state and callback comparisons passed\n",cases);return 0;
}

