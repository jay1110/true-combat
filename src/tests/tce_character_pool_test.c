#define WIN32_LEAN_AND_MEAN
#include "../game/q_shared.h"
#include "../game/bg_public.h"
#include <windows.h>
#include <stdio.h>
#include "tce_character_pool_checks.h"
#undef _vsnprintf
int Q_vsnprintf(char *s,int n,const char *fmt,va_list ap){return _vsnprintf(s,n,fmt,ap);}
void QDECL Com_Printf(const char *fmt,...){}
void QDECL Com_Error(int code,const char *fmt,...){exit(3);}
int trap_PC_LoadSource(const char *f){return 0;}
int trap_PC_ReadToken(int h,pc_token_t *t){return 0;}
int trap_PC_FreeSource(int h){return 0;}
int trap_PC_SourceFileAndLine(int h,char *f,int *l){return 0;}
qboolean PC_String_ParseNoAlloc(int h,char *s,size_t n){return qfalse;}
int main(int argc,char **argv){HMODULE h;int n;if(argc!=2||!(h=LoadLibraryA(argv[1])))return 2;n=checkCharacterPool((unsigned char *)h,0);printf("qagame character pool/getter comparisons: %d\n",n);return n<0;}
