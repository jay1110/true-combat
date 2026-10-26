#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_servercmds.c"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
static char events[8192], wolf[1024], warm[64];
static int randomValue;
static snapshot_t snap;
static unsigned char rawSnap[0x1000];
static void patch(void *at,void *fn){DWORD old,tmp;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
static void logEvent(const char *fmt,...){va_list ap;va_start(ap,fmt);vsprintf(events+strlen(events),fmt,ap);va_end(ap);}
static const char *config(int n){logEvent("config:%d;",n);return n==5?warm:n==21?wolf:"INVALID";}
static void set(const char *n,const char *v){logEvent("set:%s=%s;",n,v);}
static void sound(int h,int chan){logEvent("sound:%d,%d;",h,chan);}
int rand(void){logEvent("rand:%d;",randomValue);return randomValue;}
static void record(void){logEvent("record;");}
static const char *localize(const char *s){logEvent("localize:%s;",s);return s;}
static void print(const char *fmt,...){char text[2048];va_list ap;va_start(ap,fmt);vsprintf(text,fmt,ap);va_end(ap);logEvent("print:%s;",text);}
static void center(const char *s,int y,int w){logEvent("center:%s,%d,%d;",s,y,w);}
int main(int argc,char **argv){HMODULE dll;unsigned char *b;int old,state,local,team,r,w,d,aa,c,count=0;char expected[8192];int ew,ec;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)dll;
 patch(b+0x481a0,config);patch(CG_ConfigString,config);patch(b+0x64c10,set);patch(trap_Cvar_Set,set);patch(b+0x65070,sound);patch(trap_S_StartLocalSound,sound);patch(b+0x885b6,rand);
 patch(b+0x1ca60,record);patch(CG_autoRecord_f,record);patch(b+0x5cf20,localize);patch(CG_LocalizeServerCommand,localize);patch(b+0x47b10,print);patch(CG_Printf,print);patch(b+0x21180,center);patch(CG_CenterPrint,center);
 cg.snap=&snap;*(void **)(b+0x40352c4)=rawSnap;
 for(team=0;team<2;++team)for(r=0;r<2;++r)cgs.media.tceRoundStart[team][r]=*(int *)(b+(team?0x2588b64:0x2588b6c)+r*4)=101+team*20+r;
 for(old=0;old<4;++old)for(state=0;state<4;++state)for(local=0;local<2;++local)for(team=0;team<4;++team)for(r=0;r<2;++r){
  sprintf(wolf,"\\g_currentRound\\%d\\g_nextTimeLimit\\3.75\\gamestate\\%d\\g_currentCampaign\\test_campaign\\g_currentCampaignMap\\%d",old+2,state,team);strcpy(warm,"12345");
  cgs.gamestate=*(int *)(b+0x3fb7810)=old;cgs.localServer=*(int *)(b+0x257e0fc)=local;snap.ps.persistant[PERS_TEAM]=*(int *)(rawSnap+0x148)=team;randomValue=r;
  cg.warmup=*(int *)(b+0x4080e30)=0;cg.warmupCount=*(int *)(b+0x4080e34)=1;cg.demoPlayback=*(int *)(b+0x40352b0)=0;cg_autoAction.integer=*(int *)(b+0x4846e8c)=1;
  events[0]=0;((void(__cdecl *)(void))(b+0x5b6a0))();strcpy(expected,events);events[0]=0;CG_ParseWolfinfo();
  if(strcmp(expected,events)||cgs.currentRound!=*(int *)(b+0x3fb7804)||cgs.nextTimeLimit!=*(float *)(b+0x3fb7808)||cgs.gamestate!=*(int *)(b+0x3fb7810)||strcmp(cgs.currentCampaign,*(char **)(b+0x3fb7814))||cgs.currentCampaignMap!=*(int *)(b+0x3fb7818)||cg.warmup!=*(int *)(b+0x4080e30)||cg.warmupCount!=*(int *)(b+0x4080e34)){printf("wolf mismatch %d %d %d %d %d\n%s\n%s\n",old,state,local,team,r,expected,events);return 1;}++count;
 }
 for(state=0;state<4;++state)for(old=-1;old<2;++old)for(w=-1;w<2;++w)for(d=0;d<2;++d)for(aa=0;aa<4;++aa)for(c=-1;c<2;++c){
  sprintf(warm,"%d",w*1000);cgs.gamestate=*(int *)(b+0x3fb7810)=state;cg.demoPlayback=*(int *)(b+0x40352b0)=d;cg_autoAction.integer=*(int *)(b+0x4846e8c)=aa;
  cg.warmup=*(int *)(b+0x4080e30)=old*1000;cg.warmupCount=*(int *)(b+0x4080e34)=c;
  events[0]=0;((void(__cdecl *)(void))(b+0x5b7c0))();strcpy(expected,events);ew=*(int *)(b+0x4080e30);ec=*(int *)(b+0x4080e34);events[0]=0;CG_ParseWarmup();
  if(strcmp(expected,events)||cg.warmup!=ew||cg.warmupCount!=ec){printf("warmup mismatch\n%s\n%s\n",expected,events);return 1;}++count;
 }
 printf("%d whole original round/warmup parser comparisons passed\n",count);return 0;
}
