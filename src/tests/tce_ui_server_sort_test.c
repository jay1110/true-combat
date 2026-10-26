#define WIN32_LEAN_AND_MEAN
#define NOGDI
#pragma optimize("", off)
#include "../ui/ui_main.c"
#include <windows.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
static int expectedSource,expectedKey,expectedDir,calls,lastA,lastB;
static int compare(int source,int key,int dir,int a,int b){int ka,kb;assert(source==expectedSource&&key==expectedKey&&dir==expectedDir);++calls;lastA=a;lastB=b;ka=(a*(key+3))%97;kb=(b*(key+3))%97;return (dir?-1:1)*(ka<kb?-7:ka>kb?11:a<b?-3:a>b?5:0);}
static struct{unsigned char*p,bytes[5];}hooks[2];static int hc;
static void patch(void*a,void*f){unsigned char*p=a;DWORD o,t;assert(hc<2);hooks[hc].p=p;memcpy(hooks[hc++].bytes,p,5);assert(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&o));p[0]=0xe9;*(int*)(p+1)=(unsigned char*)f-p-5;FlushInstructionCache(GetCurrentProcess(),p,5);VirtualProtect(p,5,o,&t);}
static void restore(void){DWORD o,t;while(hc){--hc;assert(VirtualProtect(hooks[hc].p,5,PAGE_EXECUTE_READWRITE,&o));memcpy(hooks[hc].p,hooks[hc].bytes,5);FlushInstructionCache(GetCurrentProcess(),hooks[hc].p,5);VirtualProtect(hooks[hc].p,5,o,&t);}}


int main(int argc,char**argv){HMODULE h;unsigned char*b;int source,key,dir,n,force,same,pattern,i,cases=0,a,c;int expected[33],originalCalls,ret;
assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;patch(b+0x20b50,compare);patch(trap_LAN_CompareServers,compare);
for(source=0;source<3;++source)for(key=0;key<6;++key)for(dir=0;dir<2;++dir){expectedSource=ui_netSource.integer=*(int*)(b+0x28fd4c)=source;expectedDir=uiInfo.serverStatus.sortDir=*(int*)(b+0x2cf2f8)=dir;expectedKey=key;
for(a=-2;a<9;++a)for(c=-2;c<9;++c){uiInfo.serverStatus.sortKey=*(int*)(b+0x2cf2f4)=key;calls=0;ret=((int(__cdecl*)(const void*,const void*))(b+0x65f0))(&a,&c);assert(calls==1&&lastA==a&&lastB==c);calls=0;assert(UI_ServersQsortCompare(&a,&c)==ret);assert(calls==1&&lastA==a&&lastB==c);++cases;}
for(n=0;n<33;++n)for(force=0;force<2;++force)for(same=0;same<2;++same)for(pattern=0;pattern<3;++pattern){uiInfo.serverStatus.sortKey=*(int*)(b+0x2cf2f4)=same?key:key+1;uiInfo.serverStatus.numDisplayServers=*(int*)(b+0x2d1304)=n;for(i=0;i<33;++i)uiInfo.serverStatus.displayServers[i]=((int*)(b+0x2cf304))[i]=pattern==0?i:pattern==1?32-i:(i*13)%33;
calls=0;((void(__cdecl*)(int,int))(b+0x65b0))(key,force);originalCalls=calls;memcpy(expected,b+0x2cf304,sizeof(expected));calls=0;UI_ServersSort(key,force);assert(uiInfo.serverStatus.sortKey==*(int*)(b+0x2cf2f4));assert(!memcmp(expected,uiInfo.serverStatus.displayServers,sizeof(expected)));assert((calls==0)==(originalCalls==0));assert(uiInfo.serverStatus.numDisplayServers==n);++cases;}}
restore();assert(FreeLibrary(h));printf("%d original server-sort comparisons passed\n",cases);return 0;}
