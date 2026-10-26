#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_character.c"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
static jmp_buf jump;
static char errorText[1024];
static void fatal(const char*s){Q_strncpyz(errorText,s,sizeof(errorText));longjmp(jump,1);}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*original)(const char*,const char*,void**);int c,i,n=0;static void*result;static animModelInfo_t*sourceResult;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x19020);hook(b+0x64ba0,fatal);hook(trap_Error,fatal);
 for(c=0;c<2200;++c){int ret,sourceRet,idx,sourceIdx;static int originalFatal,sourceFatal;char expected[1024];
  for(i=0;i<32;++i){byte*r=b+0x259d1f8+i*0xd0be0;animModelInfo_t*s=&cgs.animScriptData.modelInfo[i];memset(r,0x5a,128);memset(s,0x5a,128);strcpy((char*)r,"other");strcpy(s->animationGroup,"other");strcpy((char*)r+64,"script");strcpy(s->animationScript,"script");if(i==c%34){r[(c&1)?64:0]=0;if(c&1)s->animationScript[0]=0;else s->animationGroup[0]=0;}if(i==(c/34)%34){strcpy((char*)r,"MATCH");strcpy(s->animationGroup,"MATCH");}}
  result=NULL;sourceResult=NULL;originalFatal=sourceFatal=0;errorText[0]=0;ret=sourceRet=-9;
  if(!setjmp(jump))ret=original("match","SCRIPT",&result);else originalFatal=1;strcpy(expected,errorText);errorText[0]=0;
  if(!setjmp(jump))sourceRet=CG_CheckForExistingAnimModelInfo("match","SCRIPT",&sourceResult);else sourceFatal=1;
  idx=result?(int)(((byte*)result-(b+0x259d1f8))/0xd0be0):-1;sourceIdx=sourceResult?(int)(sourceResult-cgs.animScriptData.modelInfo):-1;
  if(originalFatal!=sourceFatal||strcmp(expected,errorText)||idx!=sourceIdx||(!originalFatal&&ret!=sourceRet)){printf("lookup case%d\n",c);return 1;}
  for(i=0;i<32;++i)if(memcmp(b+0x259d1f8+i*0xd0be0,&cgs.animScriptData.modelInfo[i],128)){printf("lookup mutation%d slot%d\n",c,i);return 1;}++n;
 }
 printf("%d whole animation-model lookup comparisons passed\n",n);return 0;
}
