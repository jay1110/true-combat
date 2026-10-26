#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../cgame/tce_popup_text.h"
static void check(int b){if(!b)exit(1);}
static const char *config(int n){static char s[64];sprintf(s,"config%d",n);return s;}
static const char *name(int n){static char s[64];sprintf(s,"Player%d",n);return s;}
static const char *location(const float *v){return "Warehouse";}
static void hook(unsigned char *p,void *f){DWORD old,tmp;check(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char **argv){HMODULE dll;unsigned char *b;unsigned char snap[1024]={0},entity[1024]={0};const char *(__cdecl *original)(void *);tce_popup_text_t s={0};tce_popup_text_api_t api={config,name,location};char expected[1024],out[1024];const char *r,*actual;int i,n,tests=0;
 check(argc==2);dll=LoadLibraryA(argv[1]);check(dll!=NULL);b=(unsigned char *)dll;original=(void *)(b+0x57d80);hook(b+0x481a0,config);hook(b+0x6890,location);*(void **)(b+0x40352c4)=snap;*(int *)(b+0x40352a4)=0;
 for(i=0;i<64;++i)strcpy((char *)b+0x2580a18+i*0x1c0,name(i));
 for(s.kind=-1;s.kind<10;++s.kind)for(s.action=-2;s.action<5;++s.action)for(s.density=-1;s.density<7;++s.density)for(s.localTeam=0;s.localTeam<4;++s.localTeam)for(s.snapshotTeam=0;s.snapshotTeam<4;++s.snapshotTeam){
  s.client=tests%64;*(int *)(entity+0x10c)=s.kind;*(int *)(entity+0x110)=s.action;*(int *)(entity+0x114)=s.client;*(int *)(entity+0xf4)=s.density;*(int *)(snap+0x148)=s.snapshotTeam;*(int *)(b+0x2580a98)=s.localTeam;
  r=original(entity);n=r!=NULL;if(r)strcpy(expected,r);actual=TCE_GetPMItemText(&s,&api,out,sizeof(out));if(n!=(actual!=NULL)||(n&&strcmp(expected,actual))){fprintf(stderr,"popup mismatch kind%d action%d density%d teams%d/%d: %s / %s\n",s.kind,s.action,s.density,s.localTeam,s.snapshotTeam,n?expected:"NULL",actual?actual:"NULL");return 1;}++tests;
 }
 printf("%d original popup text comparisons passed\n",tests);FreeLibrary(dll);return 0;
}
