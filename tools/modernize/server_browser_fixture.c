/* Actual UI_BuildServerDisplayList with controlled engine replies and filters. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
typedef int qboolean;
enum { qfalse, qtrue, AS_LOCAL=0, AS_GLOBAL=1, AS_FAVORITES=2, MAX_STRING_CHARS=1024, FEEDER_SERVERS=1 };
#define Q3_VERSION "fixture"
typedef struct { int integer; } fixtureCvar_t;
static fixtureCvar_t ui_netSource,ui_browserShowEmptyOrFull,ui_browserShowPasswordProtected,
 ui_browserShowFriendlyFire,ui_browserShowMaxlives,ui_browserShowPunkBuster,ui_browserShowAntilag,
 ui_browserShowWeaponsRestricted,ui_browserShowTeamBalanced,ui_joinGameType;
static struct { struct { int realTime; } uiDC; struct { int nextDisplayRefresh,motdLen,motdWidth,numDisplayServers,numPlayersOnServers,currentServerPreview,refreshtime;char motd[1024]; } serverStatus; } uiInfo;
static int visible=1,serverPing=-1,hidden,inserted,clients;
static char game[32],map[64];
static char *va(const char *format,...) { static char data[16][1024];static unsigned index;char *p=data[index++&15];va_list ap;va_start(ap,format);vsnprintf(p,1024,format,ap);va_end(ap);return p; }
static int Q_stricmpn(const char *a,const char *b,int n) { while(n-- && (*a || *b)){int d=tolower((unsigned char)*a++)-tolower((unsigned char)*b++);if(d)return d;}return 0; }
static int Q_stricmp(const char *a,const char *b) { return Q_stricmpn(a,b,1024); }
static int Q_strlenInt(const char *s) { return (int)strlen(s); }
static void trap_Cvar_VariableStringBuffer(const char *key,char *buf,int size) { (void)key;snprintf(buf,size,"fixture motd"); }
static void trap_Cvar_Update(fixtureCvar_t *cvar) { (void)cvar; }
static void Menu_SetFeederSelection(void *menu,int feeder,int index,void *item) { (void)menu;(void)feeder;(void)index;(void)item; }
static void trap_LAN_MarkServerVisible(int source,int index,qboolean value) { (void)source;visible=value;if(index>=0 && !value)++hidden; }
static int trap_LAN_GetServerCount(int source) { (void)source;return 1; }
static int trap_LAN_ServerIsVisible(int source,int index) { (void)source;(void)index;return visible; }
static int trap_LAN_GetServerPing(int source,int index) { (void)source;(void)index;return serverPing; }
static void trap_LAN_GetServerInfo(int source,int index,char *info,int size) { (void)source;(void)index;snprintf(info,size,"controlled engine snapshot"); }
static char *Info_ValueForKey(const char *info,const char *key) {
 (void)info;if(!strcmp(key,"game"))return game;if(!strcmp(key,"mapname"))return map;
 if(!strcmp(key,"clients"))return va("%d",clients);if(!strcmp(key,"sv_maxclients"))return "16";
 if(!strcmp(key,"weaprestrict"))return "100";if(!strcmp(key,"gametype"))return "2";return "0";
}
static int trap_R_RegisterShaderNoMip(const char *name) { (void)name;return 1; }
static void UI_RemoveServerFromDisplayList(int index) { (void)index;if(uiInfo.serverStatus.numDisplayServers)--uiInfo.serverStatus.numDisplayServers; }
static void UI_BinaryServerInsertion(int index) { (void)index;++inserted;++uiInfo.serverStatus.numDisplayServers; }
/* ACTUAL_BROWSER */
#define CHECK(test) do { ++checks;if(!(test)){fprintf(stderr,"failed line %d: %s\n",__LINE__,#test);return 1;} } while(0)
int main(void) {
 int checks=0;
 ui_netSource.integer=AS_FAVORITES;ui_joinGameType.integer=-1;uiInfo.uiDC.realTime=100;
 UI_BuildServerDisplayList(qtrue);
 CHECK(visible && !hidden && !inserted);CHECK(!uiInfo.serverStatus.numPlayersOnServers);
 strcpy(game,"tce2");uiInfo.uiDC.realTime=1000;UI_BuildServerDisplayList(qfalse);
 CHECK(visible && !hidden && !inserted); /* partial reply still awaits map */
 strcpy(map,"obj_railhouse");serverPing=23;clients=1;uiInfo.uiDC.realTime=2000;UI_BuildServerDisplayList(qfalse);
 CHECK(inserted==1 && uiInfo.serverStatus.numDisplayServers==1);CHECK(!visible && hidden==1);CHECK(uiInfo.serverStatus.numPlayersOnServers==1);
 serverPing=-1;hidden=inserted=0;UI_BuildServerDisplayList(qtrue);
 CHECK(inserted==1 && visible && !hidden); /* complete cached favorites retain original behavior */
 serverPing=24;strcpy(game,"etmain");inserted=hidden=0;UI_BuildServerDisplayList(qtrue);
 CHECK(!inserted && hidden==1 && !visible); /* genuine different mod remains rejected */
 strcpy(game,"tce2");strcpy(map,"goldrush");inserted=hidden=0;UI_BuildServerDisplayList(qtrue);
 CHECK(!inserted && hidden==1 && !visible); /* genuine different map family remains rejected */
 strcpy(map,"obj_railhouse");ui_browserShowPasswordProtected.integer=1;inserted=hidden=0;UI_BuildServerDisplayList(qtrue);
 CHECK(!inserted && hidden==1 && !visible); /* user's ordinary filters remain effective */
 printf("%d stateful browser checks passed\n",checks);return 0;
}
