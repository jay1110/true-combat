#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
#include <limits.h>

int main(int argc, char **argv) {
    HMODULE dll; unsigned char *b;
    int module, parsed, weapon, a, c, n, count=0;
    const int clips[]={INT_MIN,-99,-1,0,1,2,7,30,31,INT_MAX};
    const int offsets[2][4]={{0x3840,0x3880,0x38b0,0x3900},{0x2b180,0x2b1c0,0x2b1f0,0x2b2c0}};
    if(argc!=3)return 2;
    for(module=0;module<2;++module) {
        if(!(dll=LoadLibraryA(argv[module+1])))return 3;b=(unsigned char *)dll;
        for(parsed=0;parsed<2;++parsed) {
            gearDef.parsed=parsed;
            for(n=0;n<515;++n) {
                weapon=n<513?n-256:n==513?INT_MIN:INT_MAX;
                if(BG_IsAkimboWeapon(weapon)!=((int (__cdecl *)(int))(b+offsets[module][1]))(weapon) ||
                   BG_AkimboSidearm(weapon)!=((int (__cdecl *)(int))(b+offsets[module][2]))(weapon) ||
                   BG_WeaponInWolfMP(weapon)!=((int (__cdecl *)(int))(b+offsets[module][3]))(weapon)) {
                    printf("identity mismatch module%d gear%d weapon%d\n",module,parsed,weapon);return 1;
                }
                ++count;
                for(a=0;a<10;++a)for(c=0;c<10;++c) {
                    int original=((unsigned char (__cdecl *)(int,int,int))(b+offsets[module][0]))(weapon,clips[a],clips[c]);
                    if(!!BG_AkimboFireSequence(weapon,clips[a],clips[c])!=!!original)return 4;
                    ++count;
                }
            }
        }
        FreeLibrary(dll);
    }
    printf("%d complete production identity/akimbo comparisons passed in both modules, before/after Gear\n",count);
    return 0;
}
