#define WIN32_LEAN_AND_MEAN
#include "../game/q_shared.h"
#include "../game/bg_public.h"
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
extern void PC_SourceError(int,char *,...);
static pc_token_t tokens[8];static int available,cursor;static char events[4096];
static int readToken(int h,pc_token_t *out){sprintf(events+strlen(events),"read:%d;",h);if(cursor>=available)return 0;*out=tokens[cursor++];return 1;}
static void error(int h,char *fmt,...){va_list ap;va_start(ap,fmt);vsprintf(events+strlen(events),fmt,ap);va_end(ap);}
static void patch(void *a,void *f){DWORD o,t;unsigned char *p=a;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o);p[0]=0xe9;*(int *)(p+1)=(unsigned char *)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
int main(int argc,char **argv){HMODULE h;unsigned char *b;int mode,type,sign,v,i,a,c,used,cases=0;unsigned int out[4],expectedOut[4];char expected[4096],expectedString[MAX_TOKEN_CHARS];
 if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;b=(unsigned char *)h;
#ifdef GAMEDLL
 patch(b+0x8b180,readToken);patch(b+0x2e4a0,error);
#define VECTOR_RVA 0x2e5c0
#else
 patch(b+0x657c0,readToken);patch(b+0x5f40,error);
#define VECTOR_RVA 0x60a0
#endif
 patch(trap_PC_ReadToken,readToken);patch(PC_SourceError,error);
 for(mode=0;mode<2;++mode)for(type=0;type<6;++type)for(sign=0;sign<4;++sign)for(available=0;available<=8;++available)for(v=-5;v<=5;++v){
  memset(tokens,0,sizeof(tokens));for(i=0;i<8;++i){tokens[i].type=i==sign?type:TT_NUMBER;tokens[i].floatvalue=v*.125f+i;strcpy(tokens[i].string,(sign==1&&!(i%2))?"-":sign==2?"words with spaces":"12");}
  cursor=0;events[0]=0;memset(out,0x35,sizeof(out));
#ifdef GAMEDLL
  if(mode){const char *s=((const char *(__cdecl *)(int))(b+0x2e6a0))(19);a=s!=NULL;if(s)strcpy(expectedString,s);strcpy(expected,events);used=cursor;cursor=0;events[0]=0;s=PC_String_Parse(19);if((s!=NULL)!=a||(s&&strcmp(s,expectedString))||used!=cursor||strcmp(events,expected)){puts("string mismatch");return 1;}++cases;continue;}
#else
  if(mode)a=((int(__cdecl *)(int,void *))(b+0x6060))(19,out);else
#endif
  a=((int(__cdecl *)(int,void *))(b+VECTOR_RVA))(19,out);
  memcpy(expectedOut,out,sizeof(out));strcpy(expected,events);used=cursor;cursor=0;events[0]=0;memset(out,0x35,sizeof(out));
#ifdef GAMEDLL
  c=PC_Vec_Parse(19,(vec3_t *)out);
#else
  c=mode?PC_Color_Parse(19,(vec4_t *)out):PC_Vec_Parse(19,(vec3_t *)out);
#endif
  if(a!=c||used!=cursor||memcmp(out,expectedOut,sizeof(out))||strcmp(expected,events)){puts("vector/color mismatch");return 1;}++cases;
 }
 printf("%d complete Windows bulk-parser comparisons passed\n",cases);return 0;
}
