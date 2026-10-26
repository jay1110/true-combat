#define WIN32_LEAN_AND_MEAN
#include "../game/q_shared.h"
#include "../game/bg_public.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
extern void PC_SourceError(int,char *,...);
static pc_token_t tokens[2];static int cursor,available;static char events[4096];
static int readToken(int handle,pc_token_t *t){sprintf(events+strlen(events),"read:%d;",handle);if(cursor>=available)return 0;*t=tokens[cursor++];return 1;}
static void error(int handle,char *fmt,...){va_list ap;sprintf(events+strlen(events),"error:%d:",handle);va_start(ap,fmt);vsprintf(events+strlen(events),fmt,ap);va_end(ap);}
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char **argv){HMODULE h;unsigned char *b;int type,negative,mode,v,a,c,expectedCursor,cases=0;unsigned int output,expectedOutput;char expected[4096];
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
#ifdef GAMEDLL
#define FLOAT_RVA 0x2e510
#define INT_RVA 0x2e600
 patch(b+0x8b180,readToken);patch(b+0x2e4a0,error);
#else
#define FLOAT_RVA 0x11e0
#define INT_RVA 0x12d0
 patch(b+0x20dd0,readToken);patch(b+0x1170,error);
#endif
 patch(trap_PC_ReadToken,readToken);patch(PC_SourceError,error);
 for(mode=0;mode<2;++mode)for(type=0;type<6;++type)for(negative=0;negative<4;++negative)for(available=0;available<3;++available)for(v=-20;v<=20;++v){
  memset(tokens,0,sizeof(tokens));tokens[0].type=tokens[1].type=type;tokens[0].intvalue=tokens[1].intvalue=v*917;tokens[0].floatvalue=tokens[1].floatvalue=v*.125f;strcpy(tokens[0].string,negative==0?"12":negative==1?"-":negative==2?"-word":"bad");strcpy(tokens[1].string,"17");
  cursor=0;events[0]=0;output=0x35353535;a=((int(__cdecl *)(int,void *))(b+(mode?INT_RVA:FLOAT_RVA)))(19,&output);expectedOutput=output;expectedCursor=cursor;strcpy(expected,events);
  cursor=0;events[0]=0;output=0x35353535;c=mode?PC_Int_Parse(19,(int *)&output):PC_Float_Parse(19,(float *)&output);
  if(a!=c||output!=expectedOutput||cursor!=expectedCursor||strcmp(expected,events)){printf("parser mismatch %d %d %d %d %d\n",mode,type,negative,available,v);return 1;}++cases;
 }
 printf("%d whole Windows integer/float parser comparisons passed\n",cases);return 0;
}
