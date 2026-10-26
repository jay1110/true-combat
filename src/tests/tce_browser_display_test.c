#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdio.h>
static char trace[65536];
static int serverCount,scenario,visibility[16],pingValue,extension;
static void patch(void *address,void *fn){unsigned char *p=address;DWORD old,tmp;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void motd(const char *name,char *out,int size){Q_strncpyz(out,scenario&1?"TC test motd":"",size);strcat(trace,"motd;");}
static void update(vmCvar_t *v){sprintf(trace+strlen(trace),"update:%d;",v->integer);}
static void feeder(menuDef_t *m,float f,int i,const char *s){strcat(trace,"feeder;");}
static void mark(int source,int i,qboolean v){int k;sprintf(trace+strlen(trace),"mark:%d:%d:%d;",source,i,v);if(i==-1){for(k=0;k<16;k++)visibility[k]=v;}else visibility[i]=v;}
static int count(int source){sprintf(trace+strlen(trace),"count:%d;",source);return serverCount;}
static int visible(int source,int i){sprintf(trace+strlen(trace),"visible:%d;",i);return visibility[i];}
static int ping(int source,int i){sprintf(trace+strlen(trace),"ping:%d;",i);return pingValue+(i%2);}
static void info(int source,int i,char *out,int size){char b[1024];int x=(scenario+i)%4;sprintf(b,"\\clients\\%d\\sv_maxclients\\4\\game\\%s\\mapname\\%s\\needpass\\%d\\friendlyFire\\%d\\maxlives\\%d\\punkbuster\\%d\\g_antilag\\%d\\weaprestrict\\%d\\balancedteams\\%d\\gametype\\%d",x==3?4:x,scenario&16?(i%3==0?"etmain":extension?"tce2":"tcetest"):extension?"tce2":"tcetest",scenario&32?(i%3==0?"oasis":"obj_delta"):"obj_delta",x%2,x/2,x%2,x/2,x%2,x%2?100:80,x/2,x%2?5:7);Q_strncpyz(out,b,size);sprintf(trace+strlen(trace),"info:%d;",i);}
static int compare(int source,int key,int dir,int a,int b){sprintf(trace+strlen(trace),"compare:%d:%d:%d:%d:%d;",source,key,dir,a,b);return a==b?0:((a>b)?1:-1)*(dir?-1:1);}
static int shader(const char *name){sprintf(trace+strlen(trace),"shader:%s;",name);return 77;}
static const struct {vmCvar_t *source;int address;} cvars[]={
 {&ui_browserShowEmptyOrFull,0x291cc0},{&ui_browserShowPasswordProtected,0x2dc080},
 {&ui_browserShowFriendlyFire,0x290400},{&ui_browserShowMaxlives,0x2dce00},
 {&ui_browserShowPunkBuster,0x2912a0},{&ui_browserShowAntilag,0x290760},
 {&ui_browserShowWeaponsRestricted,0x2d8cc0},{&ui_browserShowTeamBalanced,0x2dc980},
 {&ui_joinGameType,0x291060}};
int main(int argc,char **argv){
 HMODULE dll;unsigned char *b;char expected[65536];int expectedVis[16],expectedList[32],expectedValues[8];
 int force,source,f,value,i,pass,cases=0,operation,n,arg,pos,compat;int values[]={0,1,2,-1,5,7};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)dll;
#define PATCH(off,fn,target) patch(b+off,fn);patch(target,fn)
 PATCH(0x20570,motd,trap_Cvar_VariableStringBuffer);PATCH(0x20510,update,trap_Cvar_Update);
 PATCH(0x1d8f0,feeder,Menu_SetFeederSelection);PATCH(0x20ca0,mark,trap_LAN_MarkServerVisible);
 PATCH(0x20b30,count,trap_LAN_GetServerCount);PATCH(0x20c20,visible,trap_LAN_ServerIsVisible);
 PATCH(0x20c00,ping,trap_LAN_GetServerPing);PATCH(0x20ba0,info,trap_LAN_GetServerInfo);
 PATCH(0x20b50,compare,trap_LAN_CompareServers);PATCH(0x20730,shader,trap_R_RegisterShaderNoMip);
 for(compat=0;compat<2;++compat)for(scenario=0;scenario<64;++scenario)for(source=0;source<3;++source)for(force=0;force<3;++force)for(f=0;f<9;++f)for(value=0;value<6;++value){
  for(pass=0;pass<2;++pass){
   memset(&uiInfo.serverStatus,0,sizeof(uiInfo.serverStatus));memset(b+0x2cf2f0,0,0x450);memset(b+0x2d1304,0,0x440);
   uiInfo.uiDC.realTime=*(int *)(b+0x292140)=scenario&2?50:1000;
   uiInfo.serverStatus.nextDisplayRefresh=*(int *)(b+0x2d130c)=100;
   uiInfo.serverStatus.sortDir=*(int *)(b+0x2cf2f8)=scenario&1;
   ui_netSource.integer=*(int *)(b+0x28fd4c)=source;
   for(i=0;i<9;++i){int val=i==f?values[value]:(i==8?-1:0);cvars[i].source->integer=val;*(int *)(b+cvars[i].address+12)=val;}
   for(i=0;i<16;++i)visibility[i]=(i%3!=0);
   extension=pass?compat:0;serverCount=scenario&4?12:scenario&8?-1:0;pingValue=scenario&8?0:50;trace[0]=0;
   if(!pass){
    ((void(__cdecl *)(int))(b+0xb420))(force);strcpy(expected,trace);memcpy(expectedVis,visibility,sizeof(visibility));memcpy(expectedList,b+0x2cf304,sizeof(expectedList));
    expectedValues[0]=*(int *)(b+0x2d1304);expectedValues[1]=*(int *)(b+0x2d1308);expectedValues[2]=*(int *)(b+0x2d130c);expectedValues[3]=*(int *)(b+0x2d1310);expectedValues[4]=*(int *)(b+0x2d1318);expectedValues[5]=*(int *)(b+0x2d131c);expectedValues[6]=*(int *)(b+0x2cf2f0);
   }else{
    UI_BuildServerDisplayList(force);
    if(strcmp(expected,trace)||memcmp(expectedVis,visibility,sizeof(visibility))||memcmp(expectedList,uiInfo.serverStatus.displayServers,sizeof(expectedList))||expectedValues[0]!=uiInfo.serverStatus.numDisplayServers||expectedValues[1]!=uiInfo.serverStatus.numPlayersOnServers||expectedValues[2]!=uiInfo.serverStatus.nextDisplayRefresh||expectedValues[3]!=uiInfo.serverStatus.currentServerPreview||expectedValues[4]!=uiInfo.serverStatus.motdLen||expectedValues[5]!=uiInfo.serverStatus.motdWidth||expectedValues[6]!=uiInfo.serverStatus.refreshtime){printf("browser mismatch case%d scenario%d source%d force%d filter%d value%d\noriginal:%s\nsource:%s\n",cases,scenario,source,force,f,values[value],expected,trace);return 3;}
   }
  }++cases;
 }
 for(operation=0;operation<3;++operation)for(n=0;n<17;++n)for(arg=-1;arg<19;++arg)for(pos=-1;pos<n+2;++pos){
  for(pass=0;pass<2;++pass){
   for(i=0;i<32;++i){uiInfo.serverStatus.displayServers[i]=i/2;((int *)(b+0x2cf304))[i]=i/2;}
   uiInfo.serverStatus.numDisplayServers=*(int *)(b+0x2d1304)=n;trace[0]=0;
   if(!pass){
    if(operation==0)((void(__cdecl *)(int))(b+0xbab0))(arg);
    else if(operation==1)((void(__cdecl *)(int))(b+0xbb00))(arg);
    else ((void(__cdecl *)(int,int))(b+0xbb80))(arg,pos);
    memcpy(expectedList,b+0x2cf304,sizeof(expectedList));expectedValues[0]=*(int *)(b+0x2d1304);strcpy(expected,trace);
   }else{
    if(operation==0)UI_RemoveServerFromDisplayList(arg);else if(operation==1)UI_BinaryServerInsertion(arg);else UI_InsertServerIntoDisplayList(arg,pos);
    if(memcmp(expectedList,uiInfo.serverStatus.displayServers,sizeof(expectedList))||expectedValues[0]!=uiInfo.serverStatus.numDisplayServers||strcmp(expected,trace)){printf("browser list operation%d n%d arg%d pos%d mismatch\n",operation,n,arg,pos);return 4;}
   }
  }++cases;
 }
 printf("%d complete browser/list state/callback comparisons passed\n",cases);return 0;
}
