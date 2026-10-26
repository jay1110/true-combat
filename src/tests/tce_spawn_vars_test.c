#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
extern qboolean CG_ParseSpawnVars(void);
static char tokens[140][1024],error[1024];static int tokenCount,cursor,calls,badSize;static jmp_buf jump;
static int token(char*out,int size){++calls;if(size!=1024)badSize=1;if(cursor>=tokenCount)return 0;Q_strncpyz(out,tokens[cursor++],size);return 1;}
static void fatal(const char*s){Q_strncpyz(error,s,sizeof(error));longjmp(jump,1);}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*original)(void);int c,i,j;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x644a0);hook(b+0x65910,token);hook(trap_GetEntityToken,token);hook(b+0x64ba0,fatal);hook(trap_Error,fatal);
 for(c=0;c<5000;++c){static int er,sr,ef,sf;int ec,ep;char expectedError[1024];int pairs=c%66;tokenCount=0;strcpy(tokens[tokenCount++],"{");for(i=0;i<pairs;++i){sprintf(tokens[tokenCount++],"key%d",i);sprintf(tokens[tokenCount++],"value%d",i);}strcpy(tokens[tokenCount++],"}");if(c%7==0)--tokenCount;if(c%11==0)tokenCount=0;if(c%13==0)strcpy(tokens[0],"bad");if(c%17==0&&pairs)strcpy(tokens[2],"}");if(c%19==0&&pairs){memset(tokens[2],'x',1000);tokens[2][1000]=0;}
  memset(b+0x4081784,0x55,2048);memset(cg.spawnVarChars,0x55,2048);memset(b+0x4081580,0,512);memset(cg.spawnVars,0,sizeof(cg.spawnVars));cg.numSpawnVars=*(int*)(b+0x408157c)=37;cg.numSpawnVarChars=*(int*)(b+0x4081780)=71;cursor=calls=badSize=0;error[0]=0;er=sr=-99;ef=sf=0;
  if(!setjmp(jump))er=original();else ef=1;ec=calls;ep=cursor;strcpy(expectedError,error);cursor=calls=0;error[0]=0;if(!setjmp(jump))sr=CG_ParseSpawnVars();else sf=1;
  if(er!=sr||ef!=sf||ec!=calls||ep!=cursor||badSize||strcmp(expectedError,error)||cg.numSpawnVars!=*(int*)(b+0x408157c)||cg.numSpawnVarChars!=*(int*)(b+0x4081780)||memcmp(cg.spawnVarChars,b+0x4081784,2048)){printf("spawn vars%d\n",c);return 1;}
  for(i=0;i<64;++i)for(j=0;j<2;++j){char*r=*(char**)(b+0x4081580+8*i+4*j),*s=cg.spawnVars[i][j];if((r?r-(char*)(b+0x4081784):-1)!=(s?s-cg.spawnVarChars:-1)){printf("spawn pointers%d/%d/%d\n",c,i,j);return 1;}}
 }
 printf("5000 whole spawn-vars comparisons passed\n");return 0;
}
