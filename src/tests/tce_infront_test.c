#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern qboolean infront(gentity_t*,gentity_t*);
int main(int argc,char**argv){HMODULE dll;byte*b;static byte ra[0x57c],rb[0x57c];static gentity_t a,c;int k,w,n=0;int(__cdecl*original)(void*,void*);
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x952a0);
 for(k=0;k<20000;++k)for(w=0;w<4;++w){vec3_t delta;int expected;VectorSet(a.s.angles,(k%179)-89,k%360,k%91-45);if(k<360)VectorSet(a.s.angles,0,k,0);VectorSet(a.r.currentOrigin,k%17,k%23,-k%31);VectorSet(delta,k%13-6,k%19-9,k%7-3);if(w==1)VectorSet(delta,1,0,0);else if(w==2)VectorSet(delta,0,1,0);else if(w==3)VectorClear(delta);VectorAdd(a.r.currentOrigin,delta,c.r.currentOrigin);memcpy(ra+0x74,a.s.angles,12);memcpy(ra+0x168,a.r.currentOrigin,12);memcpy(rb+0x168,c.r.currentOrigin,12);expected=original(ra,rb);if(infront(&a,&c)!=expected){printf("infront mismatch case%d vector%d\n",k,w);return 1;}if(memcmp(ra+0x74,a.s.angles,12)||memcmp(ra+0x168,a.r.currentOrigin,12)||memcmp(rb+0x168,c.r.currentOrigin,12))return 3;++n;}
 printf("%d whole infront comparisons passed\n",n);FreeLibrary(dll);return 0;
}
