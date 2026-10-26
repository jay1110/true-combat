#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../cgame/tce_centerprint.h"
static int count;static const char *text="^3All players ready!^7\nMatch starting...";
static void check(int b){if(!b)exit(1);}
static void popup(int type,const char *s,int shader){check(type==4&&shader==713&&!strcmp(s,text));++count;}
static void hook(unsigned char *p,void *f){DWORD old,tmp;check(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv){HMODULE dll;unsigned char *b;void (__cdecl *original)(const char *,int,int,int);int time,old,priority,requested,kind,n,pt,pr,tests=0;check(argc==2);dll=LoadLibraryA(argv[1]);check(dll!=0);b=(unsigned char *)dll;hook(b+0x57970,popup);*(int *)(b+0x259d19c)=713;
 for(kind=0;kind<2;++kind)for(time=-1;time<4000;time+=173)for(old=-1;old<2;++old)for(priority=-3;priority<5;++priority)for(requested=-3;requested<5;++requested){
  original=(void *)(b+(kind?0x211c0:0x21180));*(int *)(b+0x407dea4)=time;*(int *)(b+0x4080948)=old;*(int *)(b+0x4080d58)=priority;count=0;original(text,384,8,requested);n=count;count=0;pt=old;pr=priority;if(kind)TCE_PriorityCenterPrint(text,time,requested,&pt,&pr,713,popup);else TCE_CenterPrint(text,time,&pt,&pr,713,popup);check(n==count&&pt==*(int *)(b+0x4080948)&&pr==*(int *)(b+0x4080d58));++tests;
 }printf("%d original centerprint comparisons passed\n",tests);FreeLibrary(dll);return 0;}
