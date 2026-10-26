#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include <windows.h>
#include <stdio.h>
static const char*value;static int found,calls;static const char*seenKey,*seenDefault;
static int lookup(const char*k,const char*d,char**out){++calls;seenKey=k;seenDefault=d;*out=(char*)(found?value:d);return found;}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;int(__cdecl*original[3])(const char*,const char*,void*);int c,op,n=0;const char*values[]={"","bad","-5.75","0.1 0.2 0.3","3 4","1.23456789 -9.87654321 0.000003","  +4.5e2  -7e-3  11","0x10","42 trailing","-0x10","+0X20","1.5 0x10 7","1 2 -0x1p2","1e","1e+","-1e-"};
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original[0]=(void*)(b+0x63c60);original[1]=(void*)(b+0x63c90);original[2]=(void*)(b+0x63cc0);hook(b+0x63bc0,lookup);hook(CG_SpawnString,lookup);
 for(c=0;c<12000;++c)for(op=0;op<3;++op){union {float f[3];int i[3];} expected,actual;int er,sr,ec;const char*k="fixtureKey",*d=values[(c/16)%16],*ek,*ed;value=values[c%16];found=c&1;expected.f[0]=actual.f[0]=123;expected.f[1]=actual.f[1]=456;expected.f[2]=actual.f[2]=789;calls=0;seenKey=seenDefault=NULL;er=original[op](k,d,&expected);ec=calls;ek=seenKey;ed=seenDefault;calls=0;seenKey=seenDefault=NULL;
  if(op==0)sr=CG_SpawnFloat(k,d,actual.f);else if(op==1)sr=CG_SpawnInt(k,d,actual.i);else sr=CG_SpawnVector(k,d,actual.f);
  if(er!=sr||ec!=calls||seenKey!=ek||seenDefault!=ed||seenKey!=k||seenDefault!=d||memcmp(&expected,&actual,sizeof(actual))){printf("spawn numeric%d op%d input=%s expected=%08x,%08x,%08x actual=%08x,%08x,%08x\n",c,op,found?value:d,expected.i[0],expected.i[1],expected.i[2],actual.i[0],actual.i[1],actual.i[2]);return 1;}++n;
 }
 printf("%d whole spawn numeric comparisons passed\n",n);return 0;
}


