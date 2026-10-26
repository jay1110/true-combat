#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_sound.c"
#include <windows.h>
#include <stdio.h>
static int calls,which,entity,result;static void*script;static float*origin;static char printed[1024];
static int pick(void*s,float*o,int e){++calls;which=1;script=s;origin=o;entity=e;return result;}
static void buffer(void*s){++calls;which=2;script=s;}
static void print(const char*s){strcpy(printed,s);}
static void hook(void*at,void*fn){DWORD old,tmp;byte*p=at;VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old);p[0]=0xe9;*(int*)(p+1)=(byte*)fn-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,old,&tmp);}
int main(int argc,char**argv){HMODULE dll;byte*b;static byte raw[2][100];static soundScript_t ss[2];int(__cdecl*original)(const char*,float*,int,int);int c,i,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x60960);hook(b+0x60790,pick);hook(CG_SoundPickOldestRandomSound,pick);hook(b+0x608b0,buffer);hook(CG_AddBufferedSoundScript,buffer);hook(b+0x64b80,print);hook(trap_Print,print);
 for(c=0;c<12000;++c){const char*name=c%5==0?NULL:c%5==1?"":c%5==2?"missing":"MATCH";vec3_t pos={1,2,3};float*org=c&1?pos:NULL;int ec,ew,ee,er;void*es;float*eo;char ep[1024];long hash=generateHashValue("MATCH");memset(hashTable,0,sizeof(hashTable));memset(b+0x140ecc8,0,4096);
  for(i=0;i<2;++i){memset(raw[i],0,100);memset(&ss[i],0,sizeof(ss[i]));strcpy((char*)raw[i]+4,i?"match":"other");strcpy(ss[i].name,i?"match":"other");}*(void**)(raw[0]+96)=raw[1];ss[0].nextHash=&ss[1];*(void**)(b+0x140ecc8+hash*4)=raw[c%2];hashTable[hash]=&ss[c%2];result=c%900-1;
  calls=which=0;entity=-999;script=NULL;origin=NULL;printed[0]=0;er=original(name,org,c%64,c%3);ec=calls;ew=which;ee=entity;es=script;eo=origin;strcpy(ep,printed);
  calls=which=0;entity=-999;script=NULL;origin=NULL;printed[0]=0;if(CG_SoundPlaySoundScript(name,org,c%64,c%3)!=er||calls!=ec||which!=ew||entity!=ee||origin!=eo||strcmp(ep,printed)||(es==raw[1]?script!=&ss[1]:script!=NULL)){printf("script dispatch%d\n",c);return 1;}++n;
 }
 printf("%d whole sound-script dispatch comparisons passed\n",n);return 0;
}
