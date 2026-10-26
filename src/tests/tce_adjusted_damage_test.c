#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include <windows.h>
#include <stdio.h>
extern void G_AdjustedDamageVec(gentity_t*,vec3_t,vec3_t);
int main(int argc,char**argv){HMODULE dll;byte*b;static byte raw[0x57c];static gentity_t ent;void(__cdecl*original)(void*,float*,float*);int k,brush,alias,position,j,n=0;
 if(argc!=2||!(dll=LoadLibraryA(argv[1])))return 2;b=(byte*)dll;original=(void*)(b+0x5b6c0);
 for(k=0;k<2000;++k)for(brush=0;brush<2;++brush)for(alias=0;alias<2;++alias)for(position=0;position<5;++position){vec3_t a,c,outA,outC;ent.r.bmodel=*(int*)(raw+0x130)=brush;for(j=0;j<3;++j){float lo=(k%53-26)*(j+1)*.125f,hi=lo+(k%11)*.25f;ent.r.absmin[j]=lo;ent.r.absmax[j]=hi;ent.r.currentOrigin[j]=(k%101-50)*.375f;a[j]=position==0?lo-.125f:position==1?lo:position==2?(lo+hi)*.5f:position==3?hi:hi+.125f;}memcpy(raw+0x150,ent.r.absmin,12);memcpy(raw+0x15c,ent.r.absmax,12);memcpy(raw+0x168,ent.r.currentOrigin,12);VectorCopy(a,c);memset(outA,0xa5,12);memset(outC,0xa5,12);original(raw,a,alias?a:outA);G_AdjustedDamageVec(&ent,c,alias?c:outC);if(memcmp(a,c,12)||memcmp(outA,outC,12)){printf("damage vector mismatch%d brush%d alias%d position%d\n",k,brush,alias,position);return 1;}++n;}
 printf("%d whole adjusted damage vector comparisons passed\n",n);FreeLibrary(dll);return 0;
}
