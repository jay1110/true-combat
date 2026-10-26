#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
typedef struct{int*value;unsigned offset;} mapping_t;
#define C(f,a) {&cg.f,a}
#define S(f,a) {&cgs.f,a}
static mapping_t maps[]={C(demoPlayback,0x40352b0),C(time,0x407dea4),C(showGameView,0x4081fa8),C(tceShowLimboPanel,0x40a49dc),C(limboEndCinematicTime,0x408a3a0),C(showFireteamMenu,0x4081fac),C(demohelpWindow,0x4085df8),S(eventHandling,0x2587fe0),S(fResize,0x3fc8f28),S(fSelect,0x3fc8f2c),S(cursorUpdate,0x3fc42e4),S(timescaleUpdate,0x3fc9330),S(limboLoadoutModified,0x3fdecb8),S(limboLoadoutSelected,0x3fdecb4),S(ftMenuPos,0x3fdeca8),S(ftMenuMode,0x3fdecac),{&ccInitial,0x9f5a4}};
typedef struct{int id,a,b,c;} event_t;static event_t ev[32];static int count,catcher;static void rec(int id,int a,int b,int c){ev[count].id=id;ev[count].a=a;ev[count].b=b;ev[count++].c=c;}
static void cvar(const char*k,const char*v){rec(1,!strcmp(k,"cl_bypassMouseInput"),v?atoi(v):-7,0);}
static void scores(void){rec(2,0,0,0);}static void fade(float f,int a,int b){rec(3,*(int*)&f,a,b);}static void stop(int n){rec(4,n,0,0);}static void command(const char*s){rec(5,!strcmp(s,"rs"),0,0);}static void popup(int n){rec(6,n,0,0);}static int get(void){rec(7,0,0,0);return catcher;}static void set(int n){rec(8,n,0,0);}static void help(int*p){rec(9,*p,0,0);*p=0;}static void setup(void){rec(10,0,0,0);}
static void hook(void*at,void*fn){DWORD old,t;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&t);}
#define H(o,n,f) hook(b+o,f);hook(n,f)
int main(int argc,char**argv){HMODULE m;byte*b;void(__cdecl*original)(int,int);int k,i,n,type,forced,expected[32];event_t events[32];static byte snap[0x600];if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;b=(byte*)m;original=(void*)(b+0x4f1b0);
 H(0x64c10,trap_Cvar_Set,cvar);H(0x1bfa0,CG_ScoresUp_f,scores);H(0x651d0,trap_S_FadeBackgroundTrack,fade);H(0x65120,trap_S_StopStreamingSound,stop);H(0x64da0,trap_SendClientCommand,command);H(0x65930,trap_UI_Popup,popup);H(0x65700,trap_Key_GetCatcher,get);H(0x65760,trap_Key_SetCatcher,set);H(0x3ece0,CG_ShowHelp_Off,help);H(0x430a0,CG_LimboPanel_Setup,setup);
 for(k=0;k<4096;k++){type=k%8-2;forced=(k/8)%2;catcher=k%16;for(i=0;i<sizeof(maps)/sizeof(maps[0]);i++)*maps[i].value=*(int*)(b+maps[i].offset)=(k>>(i%10))&1;
 cg.time=*(int*)(b+0x407dea4)=12345; cgs.eventHandling=*(int*)(b+0x2587fe0)=(k/16)%7;
 cg.snap=(k&256)?&cg.activeSnapshots[0]:NULL;*(void**)(b+0x40352c4)=cg.snap?snap:NULL;cg.activeSnapshots[0].ps.pm_type=(k&512)?PM_INTERMISSION:PM_NORMAL;*(int*)(snap+0x30)=(k&512)?5:0;
 count=0;original(type,forced);n=count;memcpy(events,ev,sizeof(ev));for(i=0;i<sizeof(maps)/sizeof(maps[0]);i++)expected[i]=*(int*)(b+maps[i].offset);
 count=0;CG_EventHandling(type,forced);if(count!=n||memcmp(events,ev,n*sizeof(event_t))){printf("eventhandling calls case%d\n",k);return 1;}for(i=0;i<sizeof(maps)/sizeof(maps[0]);i++)if(expected[i]!=*maps[i].value){printf("eventhandling state case%d offset%x\n",k,maps[i].offset);return 1;}
 }puts("4096 whole CG_EventHandling comparisons passed");return 0;}
