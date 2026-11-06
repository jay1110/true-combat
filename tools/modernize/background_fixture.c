/* Exact background policy/init with deterministic stock-trap services. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <ctype.h>
typedef int fileHandle_t;
typedef struct {int tm_hour,tm_min,tm_mon;} qtime_t;
#define MAX_QPATH 64
#define FS_READ 0
#define CVAR_ARCHIVE 1
static int modernBackgroundReady,now=1794000001,ticks=122,clockHour=12,clockMinute=0,clockMonth=6;
static int marker=1,remaps,registrations,archiveLast,checks;
static char selected[64],status[32];
static struct {char key[64],value[96];} cvars[32];
static int cvarCount;
static int find(const char *key){int i;for(i=0;i<cvarCount;++i)if(!strcmp(cvars[i].key,key))return i;return -1;}
static void trap_Cvar_VariableStringBuffer(const char *key,char *out,int size){int i=find(key);snprintf(out,size,"%s",i<0?"":cvars[i].value);}
static void trap_Cvar_Set(const char *key,const char *value){int i=find(key);if(i<0){i=cvarCount++;strcpy(cvars[i].key,key);}strcpy(cvars[i].value,value);if(!strcmp(key,"ui_backgroundStatus"))strcpy(status,value);}
static void trap_Cvar_Register(void *unused,const char *key,const char *value,int flags){(void)unused;if(find(key)<0)trap_Cvar_Set(key,value);if(!strcmp(key,"ui_backgroundLast"))archiveLast=flags==CVAR_ARCHIVE;}
static int trap_FS_FOpenFile(const char *path,fileHandle_t *file,int mode){(void)path;(void)mode;*file=marker?1:0;return marker?(int)strlen("TCE2_BACKGROUNDS 1\n"):-1;}
static void trap_FS_Read(void *data,int length,fileHandle_t file){(void)file;memcpy(data,marker==2?"TCE2_BACKGROUNDS X\n":"TCE2_BACKGROUNDS 1\n",length);}
static void trap_FS_FCloseFile(fileHandle_t file){(void)file;}
static int trap_RealTime(qtime_t *clock){clock->tm_hour=clockHour;clock->tm_min=clockMinute;clock->tm_mon=clockMonth;return now;}
static int Q_stricmp(const char *a,const char *b){while(*a&&tolower((unsigned char)*a)==tolower((unsigned char)*b)){++a;++b;}return tolower((unsigned char)*a)-tolower((unsigned char)*b);}
static int trap_Milliseconds(void){return ticks;}
static int trap_R_RegisterShaderNoMip(const char *name){++registrations;strcpy(selected,name);return 7;}
static void trap_R_RemapShader(const char *old,const char *next,const char *offset){if(strcmp(old,"tce2_ui/background")||strcmp(next,selected)||strcmp(offset,"0"))abort();++remaps;}
static void Com_sprintf(char *out,int size,const char *format,...){va_list args;va_start(args,format);vsnprintf(out,size,format,args);va_end(args);}
/* ACTUAL_BACKGROUND_FUNCTIONS */
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void reset(void){memset(cvars,0,sizeof(cvars));cvarCount=modernBackgroundReady=remaps=registrations=archiveLast=0;status[0]=selected[0]=0;marker=1;clockHour=12;clockMinute=0;clockMonth=6;}
static int chosen(void){return atoi(selected+strlen("tce2_ui/backgrounds/b"));}
static void weather(int stamp,int snow){char s[32];trap_Cvar_Set("ui_backgroundMode","1");sprintf(s,"%d",stamp);trap_Cvar_Set("ui_backgroundWeatherStamp",s);trap_Cvar_Set("ui_backgroundUtcOffset","3600");trap_Cvar_Set("ui_backgroundSunrise","420");trap_Cvar_Set("ui_backgroundSunset","1140");trap_Cvar_Set("ui_backgroundLocation","Berlin");sprintf(s,"%d",snow);trap_Cvar_Set("ui_backgroundSnow",s);}
int main(void){int phase,i,previous,first[]={4,11,14,6,1},count[]={2,3,3,5,3};
 CHECK(TCE_ModernBackgroundPhase(420,420,1140,0)==0);
 CHECK(TCE_ModernBackgroundPhase(720,420,1140,0)==1);
 CHECK(TCE_ModernBackgroundPhase(1120,420,1140,0)==2);
 CHECK(TCE_ModernBackgroundPhase(120,420,1140,0)==3);
 CHECK(TCE_ModernBackgroundPhase(720,420,1140,1)==4);
 CHECK(TCE_ModernBackgroundPhase(720,0,0,0)==1);
 CHECK(TCE_ModernBackgroundPhase(-1,0,0,0)==1);
#ifndef BACKGROUND_SEASON_ONLY
 for(phase=0;phase<5;++phase)for(previous=0;previous<=17;++previous)for(i=0;i<256;++i){int id=TCE_ModernBackgroundPick(phase,(unsigned)i,previous);CHECK(id>=first[phase]&&id<first[phase]+count[phase]);CHECK(id!=previous);}
#endif
 reset();marker=0;TCE_ModernBackgroundInit();CHECK(remaps==0&&cvarCount==0);
 reset();marker=2;TCE_ModernBackgroundInit();CHECK(remaps==0&&cvarCount==0);
 reset();TCE_ModernBackgroundInit();CHECK(chosen()>=11&&chosen()<=13);CHECK(archiveLast&&remaps==1&&!strcmp(status,"day/season"));previous=chosen();
 clockHour=2;TCE_ModernBackgroundInit();CHECK(chosen()==previous&&remaps==1);
 modernBackgroundReady=0;TCE_ModernBackgroundInit();CHECK(chosen()==previous&&remaps==2); /* UI reload, same engine process */
 trap_Cvar_Set("ui_backgroundSession","0");modernBackgroundReady=0;clockHour=12;TCE_ModernBackgroundInit();CHECK(chosen()!=previous&&chosen()>=11&&chosen()<=13);
 reset();weather(now-100,1);TCE_ModernBackgroundInit();CHECK(chosen()>=1&&chosen()<=3&&!strcmp(status,"winter/weather"));
 reset();weather(now-10801,1);TCE_ModernBackgroundInit();CHECK(chosen()>=11&&chosen()<=13&&!strcmp(status,"day/local-clock"));
 reset();weather(now+1,1);TCE_ModernBackgroundInit();CHECK(chosen()>=11&&chosen()<=13);
 reset();weather(now-10,1);trap_Cvar_Set("ui_backgroundLocation","");TCE_ModernBackgroundInit();CHECK(chosen()>=11&&chosen()<=13);
 reset();weather(now-10,1);trap_Cvar_Set("ui_backgroundUtcOffset","999999");TCE_ModernBackgroundInit();CHECK(chosen()>=11&&chosen()<=13);
 for(i=1;i<=12;++i){CHECK(TCE_ModernBackgroundWinter(i,"DE","auto")== (i==12||i<=2));CHECK(TCE_ModernBackgroundWinter(i,"NZ","auto")== (i>=6&&i<=8));CHECK(!TCE_ModernBackgroundWinter(i,"SG","auto"));CHECK(!TCE_ModernBackgroundWinter(i,"BR","auto"));CHECK(!TCE_ModernBackgroundWinter(i,"DE","none"));}
 CHECK(TCE_ModernBackgroundWinter(1,"de","auto"));CHECK(TCE_ModernBackgroundWinter(7,"AU","auto"));
 CHECK(TCE_ModernBackgroundWinter(3,"SG"," 3, 4,5 "));CHECK(!TCE_ModernBackgroundWinter(3,"DE","1,13"));CHECK(!TCE_ModernBackgroundWinter(1,"DE","1,"));CHECK(!TCE_ModernBackgroundWinter(1,"DE","1, x"));
 reset();clockMonth=11;TCE_ModernBackgroundInit();CHECK(chosen()>=1&&chosen()<=3&&!strcmp(status,"winter/season"));
 reset();clockMonth=0;trap_Cvar_Set("ui_backgroundCountry","SG");TCE_ModernBackgroundInit();CHECK(chosen()>=11&&chosen()<=13);
 reset();clockMonth=6;trap_Cvar_Set("ui_backgroundCountry","NZ");TCE_ModernBackgroundInit();CHECK(chosen()>=1&&chosen()<=3);
 reset();clockMonth=0;trap_Cvar_Set("ui_backgroundWinterMonths","none");TCE_ModernBackgroundInit();CHECK(chosen()>=11&&chosen()<=13);
 reset();clockMonth=0;trap_Cvar_Set("ui_backgroundMode","2");TCE_ModernBackgroundInit();CHECK(chosen()>=11&&chosen()<=13);
 printf("%d actual background policy/init checks passed\n",checks);return 0;
}
