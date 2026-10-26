#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../cgame/cg_local.h"
#include "../ui/ui_shared.h"
static int used, expected, calls, shader;
static float values[8];
static void require(int ok) { if(!ok) { fprintf(stderr,"Loading callback comparison failed\n"); exit(1); } }
void trap_GetHunkData(int *u,int *e) { *u=used; *e=expected; }
void CG_DrawPicST(float x,float y,float w,float h,float s,float t,float s2,float t2,qhandle_t sh) {
    float v[]={x,y,w,h,s,t,s2,t2}; memcpy(values,v,sizeof(v)); shader=sh; ++calls;
}
static void hook(unsigned char *p,void *fn) {
    DWORD old,tmp; require(VirtualProtect(p,5,PAGE_EXECUTE_READWRITE,&old));
    p[0]=0xe9; *(int *)(p+1)=(unsigned char *)fn-p-5;
    FlushInstructionCache(GetCurrentProcess(),p,5); VirtualProtect(p,5,old,&tmp);
}
int main(int argc,char **argv) {
    HMODULE dll; unsigned char *base; int i,j,k,n=0,counts,sh;
    const int inputs[]={-2147483647,-1,0,1,2,17,99,100,101,2147483647};
    float original[8]; panel_button_t button={0};
    const char *(__cdecl *type)(int); void (__cdecl *bar)(panel_button_t *);
    require(argc==2); dll=LoadLibraryA(argv[1]); require(dll!=NULL); base=(unsigned char *)dll;
    type=(void *)(base+0x43e20); bar=(void *)(base+0x44160);
    for(i=-10;i<32;++i) { require(!strcmp(type(i),CG_LoadPanel_GameTypeName(i))); ++n; }
    hook(base+0x65b00,trap_GetHunkData); hook(base+0x29e50,CG_DrawPicST);
    for(i=0;i<10;++i) for(j=0;j<10;++j) for(k=0;k<7;++k) {
        used=inputs[i]; expected=inputs[j]; button.rect.x=327.f+k*.125f;
        button.rect.y=461.f-k*.25f; button.rect.w=198.f+k*13.75f; button.rect.h=4.f+k;
        button.hShaderNormal=321; calls=0; bar(&button); counts=calls; sh=shader;
        memcpy(original,values,sizeof(values)); calls=0; CG_LoadPanel_RenderLoadingBar(&button);
        require(calls==counts); if(calls) { int c; require(shader==sh); for(c=0;c<8;++c) require(fabs(original[c]-values[c])<.0001); } ++n;
    }
    FreeLibrary(dll); printf("%d original loading callback comparisons passed.\n",n); return 0;
}
