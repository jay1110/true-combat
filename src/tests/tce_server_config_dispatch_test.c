#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_servercmds.c"
#include <windows.h>
#include <stdio.h>
#include <setjmp.h>
static jmp_buf abortCall;static char errorText[256];static int run,n,mask,events[256][3];static byte*module;
static void error(const char*fmt,...){va_list ap;va_start(ap,fmt);Q_vsnprintf(errorText,sizeof(errorText),fmt,ap);va_end(ap);longjmp(abortCall,1);}
static int sequence(void){return run?*(int*)(module+0x257e0f4):cgs.serverCommandSequence;}
static qboolean get(int num){events[n][0]=1;events[n][1]=num;events[n++][2]=sequence();return ((unsigned)num&(unsigned)mask)!=0;}
static void dispatch(void){events[n][0]=2;events[n++][1]=sequence();}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE m;const char*(__cdecl*config)(int);void(__cdecl*execute)(int);int k,i,expectedAbort,actualAbort,count,copy[256][3],expectedSeq;ptrdiff_t expectedOffset,actualOffset;char expectedError[256];
 if(argc!=2||!(m=LoadLibraryA(argv[1])))return 2;module=(byte*)m;config=(void*)(module+0x481a0);execute=(void*)(module+0x5e3c0);
#define H(off,fn,to) hook(module+off,to);hook(fn,to)
 H(0x47bf0,CG_Error,error);H(0x655d0,trap_GetServerCommand,get);H(0x5e400,CG_ServerCommand,dispatch);
 for(i=0;i<MAX_CONFIGSTRINGS;i++){cgs.gameState.stringOffsets[i]=*(int*)(module+0x2577600+i*4)=(i*37)%15000;}
 for(k=0;k<4096;k++){int index=k<3072?k%1024:(k&1)?-1-k:1024+k;
  errorText[0]=0;expectedOffset=-1;expectedAbort=setjmp(abortCall);if(!expectedAbort)expectedOffset=config(index)-(char*)module-0x2578600;strcpy(expectedError,errorText);
  errorText[0]=0;actualOffset=-1;actualAbort=setjmp(abortCall);if(!actualAbort)actualOffset=CG_ConfigString(index)-cgs.gameState.stringData;
  if(expectedAbort!=actualAbort||expectedOffset!=actualOffset||strcmp(expectedError,errorText)){printf("config case%d index%d error[%s]/[%s]\n",k,index,expectedError,errorText);return 1;}
 }
 for(k=0;k<8192;k++){int initial=k%257-128,latest=initial+k%65-16;mask=k%16;cgs.serverCommandSequence=*(int*)(module+0x257e0f4)=initial;
  run=1;n=0;memset(events,0,sizeof(events));execute(latest);count=n;memcpy(copy,events,sizeof(copy));expectedSeq=sequence();
  run=0;n=0;memset(events,0,sizeof(events));CG_ExecuteNewServerCommands(latest);
  if(n!=count||memcmp(copy,events,sizeof(copy))||sequence()!=expectedSeq){printf("dispatch case%d calls%d/%d sequence%d/%d\n",k,count,n,expectedSeq,sequence());return 1;}
 }
 puts("12288 whole config/dispatch comparisons passed (4096 config,8192 dispatch)");return 0;
}
