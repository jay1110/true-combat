#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../ui/ui_shared.c"
#include <windows.h>
#include <stdio.h>
#include <assert.h>
int main(int argc,char**argv){HMODULE h;unsigned char*b;displayContextDef_t dc[2];int i,cases=0;unsigned mask;assert(argc==2);h=LoadLibraryA(argv[1]);assert(h);b=(unsigned char*)h;for(i=0;i<65536;++i){mask=(unsigned)i|((unsigned)(i*37)<<16);assert(IsVisible((int)mask)==((int(__cdecl*)(int))(b+0x155a0))((int)mask));++cases;}memset(dc,0x35,sizeof(dc));for(i=0;i<6;++i){displayContextDef_t*p=i%3==0?NULL:&dc[i%2];Init_Display(p);((void(__cdecl*)(void*))(b+0x14e60))(p);assert(DC==p&&*(void**)(b+0x245fa4)==p);++cases;}for(i=0;i<(int)sizeof(dc);++i)assert(((unsigned char*)dc)[i]==0x35);printf("%d original UI display/visibility comparisons passed\n",cases);FreeLibrary(h);return 0;}
