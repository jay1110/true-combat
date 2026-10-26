#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern qboolean G_ScriptAction_SetCamo(gentity_t *,char *);
extern qboolean G_ScriptAction_SetDemolitionTeam(gentity_t *,char *);
extern qboolean G_ScriptAction_SetRoundFraglimit(gentity_t *,char *);
extern qboolean G_ScriptAction_SetRoundAABaseTime(gentity_t *,char *);
extern qboolean G_ScriptAction_AddScore(gentity_t *,char *);
extern qboolean G_ScriptAction_Score(gentity_t *,char *);
static char trace[8192],info[1024],allocation[1024];static int absent,custom,opens;
static void patch(void *addr,void *fn){DWORD old,tmp;unsigned char *p=addr;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void set(const char *n,const char *v){strcat(trace,n);strcat(trace,"=");strcat(trace,v);strcat(trace,";");}
static void get(int i,char *p,int size){strcat(trace,va("getconfig%d:%d;",i,size));Q_strncpyz(p,info,size);}
static void config(int i,const char *v){set(va("config%d",i),v);Q_strncpyz(info,v,sizeof(info));}
static void reg(vmCvar_t *v,const char *n,const char *d,int flags){memset(v,0,sizeof(*v));Q_strncpyz(v->string,!strcmp(n,"mapname")?"obj_delta":!strcmp(n,"g_scriptName")?"override":"0",sizeof(v->string));}
static void var(const char *n,char *p,int size){Q_strncpyz(p,custom?"override":"",size);}
static int openFile(const char *p,fileHandle_t *f,fsMode_t mode){strcat(trace,p);strcat(trace,";");*f=1;++opens;return absent>=opens?-1:4;}
static void readFile(void *p,int size,fileHandle_t f){memcpy(p,"test",size);}
static void closeFile(fileHandle_t f){strcat(trace,"close;");}
static void *alloc(int size){memset(allocation,0xcc,sizeof(allocation));return allocation;}
static void ranks(void){strcat(trace,"ranks;");}
static void noop(void){}
static jmp_buf errorJump;
static void QDECL fatal(const char *fmt,...){char text[2048];va_list ap;va_start(ap,fmt);vsprintf(text,fmt,ap);va_end(ap);set("fatal",text);longjmp(errorJump,1);}
int main(int argc,char **argv){
 unsigned char *b;HMODULE dll;int i,j,k,n=0;char expected[8192],buf[64];
 int addresses[]={0x848b0,0x84950,0x84a80,0x84ac0,0x84b00};
 qboolean (*ports[])(gentity_t *,char *)={G_ScriptAction_SetCamo,G_ScriptAction_SetDemolitionTeam,G_ScriptAction_SetRoundFraglimit,G_ScriptAction_SetRoundAABaseTime,G_ScriptAction_AddScore};
 if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)dll;
 patch(b+0x8ac90,set);patch(trap_Cvar_Set,set);patch(b+0x8ade0,get);patch(trap_GetConfigstring,get);patch(b+0x8adc0,config);patch(trap_SetConfigstring,config);
 patch(b+0x61b60,ranks);patch(CalculateRanks,ranks);
 for(i=0;i<5;i++)for(j=0;j<12;j++){
  int team,score1,score2;
  sprintf(buf,"%d %d",i==1 || i==4 ? j%2:j,j*7-20);
  strcpy(info,"\\camo\\1");trace[0]=0;
  *(int *)(b+0x7febf4)=11;*(int *)(b+0x7febf8)=22;
  ((int (__cdecl *)(void *,char *))(b+addresses[i]))(NULL,buf);
  strcpy(expected,trace);team=*(int *)(b+0x222393c);score1=*(int *)(b+0x7febf4);score2=*(int *)(b+0x7febf8);
  strcpy(info,"\\camo\\1");trace[0]=0;level.teamScores[1]=11;level.teamScores[2]=22;
  ports[i](NULL,buf);
  if(strcmp(expected,trace) || (i==1 && team!=level.tceDemolitionTeam) || (i==4 && (score1!=level.teamScores[1] || score2!=level.teamScores[2]))){printf("action mismatch %d %d %s / %s\n",i,j,expected,trace);return 1;}++n;
 }
 patch(b+0x5ee10,fatal);patch(G_Error,fatal);
 {const char *params[]={"", " ", "\"\"", "3", "3 trailing", "X", "\"3\"", "//comment"};
 for(i=0;i<8;++i){int originalError,portError;
  strcpy(info,"\\camo\\3");trace[0]=0;originalError=setjmp(errorJump);if(!originalError)((int(__cdecl *)(void *,char *))(b+0x848b0))(NULL,(char *)params[i]);strcpy(expected,trace);
  strcpy(info,"\\camo\\3");trace[0]=0;portError=setjmp(errorJump);if(!portError)G_ScriptAction_SetCamo(NULL,(char *)params[i]);
  if(originalError!=portError||strcmp(expected,trace)){printf("camo error/noop mismatch %d\n%s\n%s\n",i,expected,trace);return 1;}++n;
 }}
 patch(b+0x8ac50,reg);patch(trap_Cvar_Register,reg);patch(b+0x8acd0,var);patch(trap_Cvar_VariableStringBuffer,var);
 for(i=-12;i<24;i++){
  unsigned char originalEntity[4096]={0};gentity_t portEntity;
  memset(&portEntity,0,sizeof(portEntity));sprintf(buf,"%d trailing",i*11);
  ((int (__cdecl *)(void *,char *))(b+0x84ba0))(originalEntity,buf);
  G_ScriptAction_Score(&portEntity,buf);
  if(portEntity.tceObjectiveScore!=*(int *)(originalEntity+0x2e4))return 1;
  ++n;
 }
 patch(b+0x8ab90,openFile);patch(trap_FS_FOpenFile,openFile);patch(b+0x8abb0,readFile);patch(trap_FS_Read,readFile);
 patch(b+0x8abf0,closeFile);patch(trap_FS_FCloseFile,closeFile);patch(b+0x663b0,alloc);patch(G_Alloc,alloc);patch(b+0x7d350,noop);
 for(i=0;i<8;i++)for(j=0;j<3;j++)for(k=0;k<2;k++){
  absent=j;custom=k;g_gametype.integer=*(int *)(b+0x6d8d4c)=i;
  opens=0;trace[0]=0;((void (__cdecl *)(void))(b+0x7c8a0))();strcpy(expected,trace);
  opens=0;trace[0]=0;G_Script_ScriptLoad();
  if(strcmp(trace,expected) || (j==2 ? level.scriptEntity!=NULL : strcmp(level.scriptEntity,"test"))){printf("loader mismatch %d %d %s / %s\n",i,j,expected,trace);return 1;}++n;
 }
 printf("%d original map-script loader/action comparisons passed\n",n);return 0;
}
