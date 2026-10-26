#define WIN32_LEAN_AND_MEAN
#include "../game/q_shared.h"
#include "../game/bg_public.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
extern void PC_SourceError(int,char *,...);
extern const char *String_Alloc(const char *);
static pc_token_t tokens[8];static int cursor,available;static char events[16384];static const char allocated[]="allocation";
static int readToken(int handle,pc_token_t *t){sprintf(events+strlen(events),"read:%d;",handle);if(cursor>=available)return 0;*t=tokens[cursor++];return 1;}
static void error(int handle,char *fmt,...){va_list ap;sprintf(events+strlen(events),"error:%d:",handle);va_start(ap,fmt);vsprintf(events+strlen(events),fmt,ap);va_end(ap);}
static const char *alloc(const char *s){sprintf(events+strlen(events),"alloc:%s;",s);return allocated;}
static void patch(void *at,void *fn){DWORD o,t;unsigned char *p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char **argv){HMODULE h;unsigned char *b;int mode,type,sign,v,i,a,c,expectedCursor,cases=0;unsigned int out[4],expectedOut[4];char expected[16384];
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
 patch(b+0x20dd0,readToken);patch(trap_PC_ReadToken,readToken);patch(b+0x1170,error);patch(PC_SourceError,error);patch(b+0x14910,alloc);patch(String_Alloc,alloc);
 for(mode=0;mode<2;++mode)for(type=0;type<6;++type)for(sign=0;sign<4;++sign)for(available=0;available<=8;++available)for(v=-5;v<=5;++v){
  memset(tokens,0,sizeof(tokens));for(i=0;i<8;++i){tokens[i].type=i==sign?type:TT_NUMBER;tokens[i].floatvalue=v*.125f+i;tokens[i].intvalue=v+i;strcpy(tokens[i].string,(sign==1&&!(i%2))?"-":sign==2?"label with spaces":"12");}
  memset(out,0x35,sizeof(out));cursor=0;events[0]=0;a=((int(__cdecl *)(int,void *))(b+(mode?0x1370:0x1290)))(23,out);memcpy(expectedOut,out,sizeof(out));strcpy(expected,events);expectedCursor=cursor;
  memset(out,0x35,sizeof(out));cursor=0;events[0]=0;c=mode?PC_String_Parse(23,(const char **)out):PC_Color_Parse(23,(vec4_t *)out);
  if(a!=c||cursor!=expectedCursor||memcmp(out,expectedOut,sizeof(out))||strcmp(events,expected)){printf("panel parser mismatch %d %d %d %d %d\n",mode,type,sign,available,v);return 1;}++cases;
 }
 printf("%d whole Windows color/string parser comparisons passed\n",cases);return 0;
}
