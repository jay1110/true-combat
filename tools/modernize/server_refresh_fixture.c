#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
typedef int qboolean;
enum { qfalse, qtrue, AS_LOCAL=0, AS_GLOBAL=1, AS_FAVORITES=2, EXEC_APPEND=2 };
typedef struct { int tm_mon,tm_mday,tm_year,tm_hour,tm_min; } qtime_t;
static struct { int integer; } ui_netSource;
static struct { struct { int realTime; } uiDC; struct { int refreshActive,nextDisplayRefresh,numDisplayServers,numPlayersOnServers,refreshtime; } serverStatus; } uiInfo;
static const char *MonthAbbrev[]={"Jan"};
static char debugProtocol[16],command[256];
static int pending,visible,resets;
#define Com_sprintf snprintf
static char *va(const char *format,...) { static char data[512];va_list ap;va_start(ap,format);vsnprintf(data,sizeof(data),format,ap);va_end(ap);return data; }
static void trap_RealTime(qtime_t *q) { memset(q,0,sizeof(*q)); }
static void trap_Cvar_Set(const char *key,const char *value) { (void)key;(void)value; }
static char *UI_Cvar_VariableString(const char *key) { (void)key;return debugProtocol; }
static float trap_Cvar_VariableValue(const char *key) { (void)key;return 84; }
static void UI_UpdatePendingPings(void) { ++pending; }
static void trap_LAN_MarkServerVisible(int source,int index,int value) { (void)source;(void)index;visible=value; }
static void trap_LAN_ResetPings(int source) { (void)source;++resets; }
static void trap_Cmd_ExecuteText(int mode,const char *text) { (void)mode;snprintf(command,sizeof(command),"%s",text); }
/* ACTUAL_REFRESH */
#define CHECK(test) do { if (!(test)) { fprintf(stderr,"failed line %d: %s\n",__LINE__,#test);return 1; } } while(0)
int main(void) {
 int source,debug,full,checks=0;
 for(source=0;source<3;++source)for(debug=0;debug<2;++debug)for(full=0;full<2;++full) {
  memset(&uiInfo,0,sizeof(uiInfo));uiInfo.uiDC.realTime=100;uiInfo.serverStatus.numDisplayServers=7;
  ui_netSource.integer=source;strcpy(debugProtocol,debug?"85":"");command[0]=0;pending=visible=resets=0;
  UI_StartServerRefresh(full);
  if(!full) { CHECK(pending==1 && !resets && !command[0]);CHECK(uiInfo.serverStatus.numDisplayServers==7);checks+=2;continue; }
  CHECK(!pending && resets==1 && visible==1);CHECK(uiInfo.serverStatus.numDisplayServers==0 && uiInfo.serverStatus.refreshActive);
  if(source==AS_GLOBAL) { CHECK(!strcmp(command,debug?"globalservers 0 85 empty full\n":"globalservers 0 84 empty full\n"));CHECK(uiInfo.serverStatus.refreshtime==5100); }
  else if(source==AS_LOCAL) { CHECK(!strcmp(command,"localservers\n"));CHECK(uiInfo.serverStatus.refreshtime==1100); }
  else { CHECK(!command[0]);CHECK(uiInfo.serverStatus.refreshtime==5100); }
  checks+=4;
 }
 printf("%d server refresh checks passed\n",checks);return 0;
}
