#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern char *ConcatArgs(int);
extern void SanitizeString(char*,char*,qboolean);
static int count,calls,seq[16],bad;static char args[8][1024];
static int argcSink(void){seq[calls++]=-1;return count;}
static void argvSink(int i,char*out,int size){seq[calls++]=i;if(i<0||i>=count||size!=1024){bad=1;out[0]=0;return;}Q_strncpyz(out,args[i],size);}
static int hook(void*p,void*f){DWORD old,tmp;byte*b=p;if(!VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old))return 0;b[0]=0xe9;*(int*)(b+1)=(byte*)f-b-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);return 1;}
int main(int argc,char**argv){HMODULE dll;byte*b;void(__cdecl*sanitize)(char*,char*,int);char*(__cdecl*concat)(int);int a,c,lower,alias,n=0,k,start,j,expectedCount,expected[16];char inA[8],inB[8],outA[32],outB[32],line[1024];int sizes[]={0,1,2,510,1021,1022,1023};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;sanitize=(void*)(b+0x4f4e0);concat=(void*)(b+0x4f440);
 for(a=0;a<256;++a)for(c=0;c<256;++c)for(lower=0;lower<2;++lower)for(alias=0;alias<2;++alias){memset(inA,0,8);inA[0]=(char)a;inA[1]=(char)c;memcpy(inB,inA,8);memset(outA,0xa5,32);memset(outB,0xa5,32);sanitize(inA,alias?inA:outA,lower);SanitizeString(inB,alias?inB:outB,lower);if(memcmp(inA,inB,8)||memcmp(outA,outB,32)){printf("sanitize mismatch%d/%d lower%d alias%d\n",a,c,lower,alias);return 1;}++n;}
 printf("%d whole sanitize comparisons passed\n",n);n=0;
 if(!hook(b+0x8ab60,argcSink)||!hook(trap_Argc,argcSink)||!hook(b+0x8ab70,argvSink)||!hook(trap_Argv,argvSink))return 2;
 for(count=0;count<=6;++count)for(start=0;start<9;++start)for(k=0;k<7;++k){for(j=0;j<8;++j){int len=sizes[(j+k)%7];memset(args[j],'A'+j,len);args[j][len]=0;}calls=bad=0;strcpy(line,concat(start));if(bad)return 3;expectedCount=calls;memcpy(expected,seq,sizeof(seq));calls=bad=0;if(strcmp(line,ConcatArgs(start))||bad||calls!=expectedCount||memcmp(expected,seq,calls*sizeof(int))){printf("concat mismatch count%d start%d pattern%d\n",count,start,k);return 1;}++n;}
 printf("%d whole argument concatenation comparisons passed\n",n);FreeLibrary(dll);return 0;
}
