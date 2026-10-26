#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../cgame/cg_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
extern qboolean CG_IsHeavyWeapon(weapon_t);
int main(int argc,char **argv) {
    HMODULE dll;int (__cdecl *original)(int);int i;unsigned int x=1;
    if(argc!=2 || !(dll=LoadLibraryA(argv[1])))return 2;
    original=(void *)((unsigned char *)dll+0x43c80);gearDef.parsed=1;
    for(i=-128;i<=256;++i)if(original(i)!=(int)CG_IsHeavyWeapon(i))return 1;
    for(i=0;i<100000;++i) {
        x=x*1664525u+1013904223u;
        if(original((int)x)!=(int)CG_IsHeavyWeapon((int)x))return 1;
    }
    printf("100385 complete cgame heavy-weapon comparisons matched\n");
    FreeLibrary(dll);return 0;
}
