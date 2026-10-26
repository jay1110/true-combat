#define WIN32_LEAN_AND_MEAN
#define NOGDI
/* Whole production functions, including their private status parser/sorter. */
#include "../ui/ui_main.c"
#include <windows.h>
#include <stdarg.h>
static char events[16384], reply[2048];
static int ready,menus,connection;
static serverStatusInfo_t originalInfo,ownInfo;
static void patch(void *at,void *fn){DWORD old,tmp;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void logEvent(const char *fmt,...){va_list ap;va_start(ap,fmt);vsprintf(events+strlen(events),fmt,ap);va_end(ap);}
static int status(const char *address,char *buffer,int length){logEvent("status:%s,%d,%d;",address?address:"NULL",buffer!=NULL,length);if(buffer && length>0)Q_strncpyz(buffer,reply,length);return ready;}
static menuDef_t *findMenu(const char *name){logEvent("find:%s;",name);return menus?(menuDef_t *)(!strcmp(name,"popupError")?0x2000:0x1000):NULL;}
static void show(menuDef_t *m,const char *name,qboolean value){logEvent("show:%p,%s,%d;",m,name,value);}
static void set(const char *name,const char *value){logEvent("set:%s=%s;",name,value);}
static void client(uiClientState_t *s){memset(s,0,sizeof(*s));s->connState=connection;logEvent("client;");}
static void feeder(menuDef_t *m,float f,int i,const char *name){logEvent("feeder:%p,%g,%d,%s;",m,f,i,name?name:"NULL");}
static int same(const serverStatusInfo_t *a,const serverStatusInfo_t *b){int i,j;if(a->numLines!=b->numLines||strcmp(a->address,b->address)||memcmp(a->text,b->text,sizeof(a->text))||memcmp(a->pings,b->pings,sizeof(a->pings)))return 0;for(i=0;i<a->numLines;++i)for(j=0;j<4;++j){if(!a->lines[i][j]!=!b->lines[i][j])return 0;if(a->lines[i][j]&&strcmp(a->lines[i][j],b->lines[i][j]))return 0;}return 1;}
int main(int argc,char **argv){HMODULE dll;unsigned char *b;int i,j,k,force,find,refresh,n,current,count=0,expectedRefresh,a,c;char expected[16384];
 const char *responses[]={"","\\gamename\\tcetest\\mapname\\obj_delta\\sv_hostname\\TC server\\g_gametype\\7\\url\\https://example.test/\\mod_url\\https://mod.test/\\\\10 40 Player One\\-3 0 Player Two","\\SV_HOSTNAME\\Uppercase\\protocol\\84\\version\\ET 2.60b\\timelimit\\20\\fraglimit\\50\\\\0 999 Name","\\truncated","\\url\\https://example.test/\\","\\a\\b\\\\no_spaces","\\a\\b\\\\1 no_second_space"};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)dll;
 if(sizeof(originalInfo)!=0x1104){printf("Unexpected status ABI\n");return 2;}
 patch(b+0x20c40,status);patch(trap_LAN_ServerStatus,status);patch(b+0x15d10,findMenu);patch(Menus_FindByName,findMenu);patch(b+0x15c20,show);patch(Menu_ShowItemByName,show);patch(b+0x20530,set);patch(trap_Cvar_Set,set);patch(b+0x20ab0,client);patch(trap_GetClientState,client);patch(b+0x1d8f0,feeder);patch(Menu_SetFeederSelection,feeder);
 for(i=0;i<7;++i)for(ready=0;ready<2;++ready)for(menus=0;menus<2;++menus){
  strcpy(reply,responses[i]);memset(&originalInfo,0x55,sizeof(originalInfo));memset(&ownInfo,0x55,sizeof(ownInfo));
  events[0]=0;a=((int(__cdecl *)(const char *,serverStatusInfo_t *))(b+0xbfa0))("127.0.0.1:27960",&originalInfo);strcpy(expected,events);events[0]=0;c=UI_GetServerStatusInfo("127.0.0.1:27960",&ownInfo);
  if(a!=c||strcmp(expected,events)||!same(&originalInfo,&ownInfo)){printf("parse mismatch %d %d %d\n%s\n%s\n",i,ready,menus,expected,events);return 1;}++count;
 }
 for(i=0;i<4;++i){memset(&originalInfo,0,sizeof(originalInfo));memset(&ownInfo,0,sizeof(ownInfo));originalInfo.numLines=ownInfo.numLines=12;
  for(j=0;j<12;++j){const char *key=j<9?serverStatusCvars[(j+i)%9].name:j==9?"SV_HOSTNAME":"other";originalInfo.lines[j][0]=ownInfo.lines[j][0]=(char *)key;originalInfo.lines[j][1]=ownInfo.lines[j][1]=j==10?NULL:j==11?"score":"";originalInfo.lines[j][2]=ownInfo.lines[j][2]="";originalInfo.lines[j][3]=ownInfo.lines[j][3]=j&1?"A":"B";}
  ((void(__cdecl *)(serverStatusInfo_t *))(b+0xc380))(&originalInfo);UI_SortServerStatusInfo(&ownInfo);if(!same(&originalInfo,&ownInfo)){puts("sort mismatch");return 1;}++count;
 }
 for(force=0;force<2;++force)for(find=0;find<2;++find)for(refresh=0;refresh<3;++refresh)for(connection=0;connection<=6;connection+=3)for(n=0;n<2;++n)for(current=-1;current<3;++current)for(ready=0;ready<2;++ready)for(menus=0;menus<2;++menus){
  strcpy(reply,responses[1]);uiInfo.nextFindPlayerRefresh=*(int *)(b+0x2d3d44)=find;uiInfo.nextServerStatusRefresh=*(int *)(b+0x2d2874)=refresh==0?0:refresh==1?100:101;uiInfo.uiDC.realTime=*(int *)(b+0x292140)=100;
  uiInfo.serverStatus.currentServer=*(int *)(b+0x2cf300)=current;uiInfo.serverStatus.numDisplayServers=*(int *)(b+0x2d1304)=n;strcpy(uiInfo.serverStatusAddress,"127.0.0.1:27960");strcpy((char *)b+0x2d1730,uiInfo.serverStatusAddress);
  memset(&uiInfo.serverStatusInfo,0,sizeof(uiInfo.serverStatusInfo));memset(b+0x2d1770,0,sizeof(uiInfo.serverStatusInfo));uiInfo.serverStatusInfo.numLines=*(int *)(b+0x2d2870)=0;
  events[0]=0;((void(__cdecl *)(int))(b+0xc4d0))(force);strcpy(expected,events);expectedRefresh=*(int *)(b+0x2d2874);events[0]=0;UI_BuildServerStatus(force);
  if(strcmp(expected,events)||uiInfo.nextServerStatusRefresh!=expectedRefresh||!same((serverStatusInfo_t *)(b+0x2d1770),&uiInfo.serverStatusInfo)){printf("refresh mismatch f%d find%d r%d con%d n%d cur%d ready%d menus%d\n%s\n%s\n",force,find,refresh,connection,n,current,ready,menus,expected,events);return 1;}++count;
 }
 puts("Whole original UI status parser, sorter and refresh comparisons passed");printf("%d cases\n",count);return 0;
}
