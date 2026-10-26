#include "../game/g_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
extern void G_RegisterCvars(void);
extern void G_RemapTeamShaders(void);
typedef struct {vmCvar_t *vm;char name[64],value[256];} variable_t;
static variable_t vars[256];static int nv,mode,locked,developer,clients,msec,ready,toggles;
static char events[65536];
static void patch(void *at,void *fn){DWORD old,tmp;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void logValue(const char *n,const char *v){strcat(events,n);strcat(events,"=");strcat(events,v);strcat(events,";");}
static void assign(vmCvar_t *v,const char *s){if(v){v->integer=atoi(s);v->value=(float)atof(s);Q_strncpyz(v->string,s,sizeof(v->string));v->modificationCount=3;}}
static void reg(vmCvar_t *v,const char *n,const char *d,int flags){
 char data[512],value[256];int x=0,override=1;
 if(!strcmp(n,"g_gametype"))x=mode;else if(!strcmp(n,"sv_gametype"))x=locked;
 else if(!strcmp(n,"developer"))x=developer;else if(!strcmp(n,"sv_maxclients"))x=clients;
 else if(!strcmp(n,"pmove_msec"))x=msec;else if(!strcmp(n,"match_readypercent"))x=ready;else override=0;
 if(override)sprintf(value,"%d",x);else Q_strncpyz(value,d,sizeof(value));
 vars[nv].vm=v;strcpy(vars[nv].name,n);strcpy(vars[nv++].value,value);assign(v,value);
 if(!strcmp(n,"gamename")||!strcmp(n,"gamedate")||!strcmp(n,"tce_version"))d="<identity>";
 sprintf(data,"%s,%d",d,flags);logValue(n,data);
}
static void set(const char *n,const char *v){int i;logValue(n,v);for(i=0;i<nv;i++)if(!strcmp(n,vars[i].name))strcpy(vars[i].value,v);}
static void update(vmCvar_t *v){int i;for(i=0;i<nv;i++)if(vars[i].vm==v){logValue("update",vars[i].name);assign(v,vars[i].value);return;}exit(7);}
static void config(int n,const char *v){char key[32];sprintf(key,"config%d",n);logValue(key,v);}
static void print(const char *fmt,...){char s[1024];va_list ap;va_start(ap,fmt);vsprintf(s,fmt,ap);va_end(ap);logValue("print",s);}
static int toggle(vmCvar_t *v){int i;++toggles;for(i=0;i<nv;i++)if(vars[i].vm==v){logValue("toggle",vars[i].name);break;}return 0;}
static void remap(void){logValue("remap","");}
static void reset(void){nv=0;toggles=0;events[0]=0;}
int main(int argc,char **argv){HMODULE dll;unsigned char *b;char expected[65536];int n,et,i,cases=0;void (*original)(void);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)dll;original=(void *)(b+0x60000);
 for(n=0;n<32;n++){
  memset(b+0x23cf520,n,128);memset(tce_gametypeDef,n,sizeof(tce_gametypeDef));
  ((void (__cdecl *)(void))(b+0x2eca0))();BG_InitializeGametypeDef();
  if(memcmp(b+0x23cf520,tce_gametypeDef,128))return 3;++cases;
 }
 patch(b+0x8ac50,reg);patch(trap_Cvar_Register,reg);patch(b+0x8ac90,set);patch(trap_Cvar_Set,set);
 patch(b+0x8ac70,update);patch(trap_Cvar_Update,update);patch(b+0x8adc0,config);patch(trap_SetConfigstring,config);
 patch(b+0x5ed80,print);patch(G_Printf,print);patch(b+0x660f0,toggle);patch(G_checkServerToggle,toggle);
 patch(b+0x1b750,remap);patch(G_RemapTeamShaders,remap);
 memset(b+0x23cf520,0,128);memset(tce_gametypeDef,0,sizeof(tce_gametypeDef));((void (__cdecl *)(void))(b+0x2eca0))();BG_InitializeGametypeDef();
 for(mode=-1;mode<=8;mode++)for(i=0;i<4;i++)for(developer=0;developer<2;developer++)for(n=0;n<9;n++){
  static int locks[]={0,2,5,7},sizes[]={1,16,20,21,32,36,37,48,64},steps[]={-1,0,7,8,16,33,34,64,100};
  locked=locks[i];clients=sizes[n];msec=steps[n];ready=n-3;
  *(int *)(b+0x6d8d4c)=g_gametype.integer=mode;
  reset();original();strcpy(expected,events);et=toggles;
  reset();G_RegisterCvars();
  if(strcmp(expected,events)||et!=toggles){fprintf(stderr,"register mismatch mode%d lock%d dev%d clients%d\n%s\n%s\n",mode,locked,developer,clients,expected,events);return 1;}++cases;
 }
 printf("%d gametype-definition/full-registration original comparisons passed\n",cases);return 0;
}
