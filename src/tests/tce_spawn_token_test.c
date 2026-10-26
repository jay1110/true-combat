#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern char*CG_AddSpawnVarToken(const char*);
static jmp_buf jump;static char error[1024];
static void fatal(const char*s){Q_strncpyz(error,s,sizeof(error));longjmp(jump,1);}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;char*(__cdecl*original)(const char*);int c,j;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x64430);hook(b+0x64ba0,fatal);hook(trap_Error,fatal);
 for(c=0;c<18000;++c){char text[257],expectedError[1024];int initial=c%2049,length=(c/31)%257;static int ef,sf;static char*er,*sr;for(j=0;j<length;++j)text[j]='a'+j%26;text[length]=0;cg.numSpawnVarChars=*(int*)(b+0x4081780)=initial;memset(cg.spawnVarChars,0x55,2048);memset(b+0x4081784,0x55,2048);ef=sf=0;er=sr=NULL;error[0]=0;
  if(!setjmp(jump))er=original(text);else ef=1;strcpy(expectedError,error);error[0]=0;if(!setjmp(jump))sr=CG_AddSpawnVarToken(text);else sf=1;
  if(ef!=sf||strcmp(expectedError,error)||cg.numSpawnVarChars!=*(int*)(b+0x4081780)||memcmp(cg.spawnVarChars,b+0x4081784,2048)||(!ef&&(er-(char*)(b+0x4081784)!=sr-cg.spawnVarChars))){printf("spawn token case%d initial%d length%d\n",c,initial,length);return 1;}
 }
 printf("18000 whole spawn-token comparisons passed\n");return 0;
}
