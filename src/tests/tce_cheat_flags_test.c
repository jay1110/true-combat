#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern qboolean CheatsOk(gentity_t*);
extern void Cmd_Nofatigue_f(gentity_t*);
extern void Cmd_Notarget_f(gentity_t*);
static const char *args[]={"","on","ON","off","OFF","0","00","1","-1","23tail"," +2","foo","all","false","0x10"," 0"};
static const char*argument;static int count,bad;
typedef struct {int kind,arg;char text[1024];} event_t;static event_t events[8];
static int hook(void*p,void*f){DWORD o,t;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);return 1;}
static void record(int kind,int arg,const char*s){event_t*e;if(count>=8){bad=1;return;}e=&events[count++];memset(e,0,sizeof(*e));e->kind=kind;e->arg=arg;if(s)Q_strncpyz(e->text,s,sizeof(e->text));}
static int argcFn(void){record(1,0,NULL);return *argument?2:1;}
static void argvFn(int index,char *out,int size){record(2,index,NULL);if(index!=1||size!=1024)bad=1;Q_strncpyz(out,argument,size);}
static void send(int client,const char*s){record(3,client,s);}
int main(int argc,char**argv){HMODULE dll;byte*b,*raw;int(__cdecl*check)(void*);void(__cdecl*commands[2])(void*);int mode,ai,sc,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;check=(void*)(b+0x4f3b0);commands[0]=(void*)(b+0x4fbf0);commands[1]=(void*)(b+0x4fcc0);
 if(!hook(b+0x8ab60,argcFn)||!hook(trap_Argc,argcFn)||!hook(b+0x8ab70,argvFn)||!hook(trap_Argv,argvFn)||!hook(b+0x8ad60,send)||!hook(trap_SendServerCommand,send))return 2;
 for(mode=0;mode<3;mode++)for(ai=0;ai<16;ai++)for(sc=0;sc<1024;sc++){
  event_t expected[8];byte before[0x57c];gentity_t*nativeEnt=&g_entities[sc%64],nativeBefore;int expectedCount,x=0,y=0;
  raw=b+0x226e2a0+(sc%64)*0x57c;argument=args[ai];memset(raw,0,sizeof(before));memset(nativeEnt,0,sizeof(*nativeEnt));
  nativeEnt->flags=*(int*)(raw+0x1b0)=(int)(0x9e3779b9u*(sc+1));nativeEnt->health=*(int*)(raw+0x2c8)=(sc%7)-2;
  g_cheats.integer=*(int*)(b+0x6d9f4c)=(sc%3)-1;memcpy(before,raw,sizeof(before));nativeBefore=*nativeEnt;
  bad=count=0;if(!mode)x=check(raw);else commands[mode-1](raw);if(bad)return 3;expectedCount=count;memcpy(expected,events,sizeof(expected));
  bad=count=0;if(!mode)y=CheatsOk(nativeEnt);else if(mode==1)Cmd_Nofatigue_f(nativeEnt);else Cmd_Notarget_f(nativeEnt);
  if(bad||x!=y||count!=expectedCount||memcmp(expected,events,count*sizeof(event_t))||nativeEnt->flags!=*(int*)(raw+0x1b0)){fprintf(stderr,"cheat mode%d arg%d case%d result%d/%d calls%d/%d failed\n",mode,ai,sc,y,x,count,expectedCount);return 1;}
  memcpy(before+0x1b0,raw+0x1b0,4);nativeBefore.flags=nativeEnt->flags;
  if(memcmp(before,raw,sizeof(before))||memcmp(&nativeBefore,nativeEnt,sizeof(nativeBefore))){fprintf(stderr,"cheat mode%d case%d unexpected write\n",mode,sc);return 1;}n++;
 }
 printf("%d whole cheat eligibility/nofatigue/notarget comparisons passed (16384 each)\n",n);return 0;
}
