#include "../game/tce_weapon_ammo.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
static unsigned seed=51847;
static unsigned next(void) { seed=seed*1664525u+1013904223u; return seed>>8; }
int main(int argc, char **argv) {
    HMODULE module;
    unsigned char *base, ps[0x500], pm[0x140];
    int ammo[64], clip[64], i, j, weapon;
    tce_weaponDef_t defs[64];
    int (__cdecl *lookup[4])(int);
    int (*port[4])(int)={TCE_BG_FindAmmoForWeapon,TCE_BG_FindClipForWeapon,TCE_BG_IsAkimboWeapon,TCE_BG_AkimboSidearm};
    void (__cdecl *reload)(int);
    void (__cdecl *useAmmo)(int,int);
    int (__cdecl *available)(int), (__cdecl *empty)(int);
    unsigned char (__cdecl *sequence)(int,int,int);
    if(argc!=2 || !(module=LoadLibraryA(argv[1]))) return 2;
    base=(unsigned char *)module;
    lookup[0]=(void *)(base+0x3800); lookup[1]=(void *)(base+0x37c0);
    lookup[2]=(void *)(base+0x3880); lookup[3]=(void *)(base+0x38b0);
    reload=(void *)(base+0x9250);
    useAmmo=(void *)(base+0x9920); available=(void *)(base+0x99e0);
    empty=(void *)(base+0x9a90); sequence=(void *)(base+0x3840);
    for(i=-100; i<200; ++i) for(j=0;j<4;++j)
        if(lookup[j](i)!=port[j](i)) { fprintf(stderr,"lookup %d id %d\n",j,i); return 1; }
    for(j=0;j<4;++j) if(lookup[j](INT_MIN)!=port[j](INT_MIN) || lookup[j](INT_MAX)!=port[j](INT_MAX)) return 1;
    memset(pm,0,sizeof(pm)); *(void **)pm=ps; *(void **)(base+0x4ba9edc)=pm;
    for(i=0;i<64000;++i) {
        memset(ps,0,sizeof(ps)); memset(defs,0,sizeof(defs));
        for(j=0;j<64;++j) {
            ammo[j]=(int)(next()%160)-2; clip[j]=(int)(next()%61)-2;
            defs[j].maxclip=(int)(next()%81)-2; defs[j].singleReload=next()%3;
        }
        weapon=i%64;
        memcpy(ps+0x190,ammo,sizeof(ammo)); memcpy(ps+0x290,clip,sizeof(clip));
        memcpy(base+0x4c1df20,defs,sizeof(defs));
        reload(weapon); TCE_PM_ReloadClip(weapon,ammo,clip,defs);
        if(memcmp(ps+0x190,ammo,sizeof(ammo)) || memcmp(ps+0x290,clip,sizeof(clip))) {
            fprintf(stderr,"reload mismatch case %d weapon %d\n",i,weapon); return 1;
        }
    }
    for(i=0;i<64000;++i) {
        int noClips=(i/64)%3, amount=(int)(next()%7)-1;
        int a=(int)(next()%65)-2, b=(int)(next()%65)-2;
        weapon=i%64;
        for(j=0;j<64;++j) { ammo[j]=(int)(next()%100)-2; clip[j]=(int)(next()%35)-2; }
        memcpy(ps+0x190,ammo,sizeof(ammo)); memcpy(ps+0x290,clip,sizeof(clip));
        memcpy(pm+0x50,&noClips,4);
        if(sequence(weapon,a,b)!=TCE_BG_AkimboFireSequence(weapon,a,b) ||
           available(weapon)!=TCE_PM_WeaponAmmoAvailable(weapon,noClips,ammo,clip) ||
           empty(weapon)!=TCE_PM_WeaponClipEmpty(weapon,noClips,ammo,clip)) {
            fprintf(stderr,"firing selection mismatch case %d\n",i); return 1;
        }
        useAmmo(weapon,amount); TCE_PM_WeaponUseAmmo(weapon,amount,noClips,ammo,clip);
        if(memcmp(ps+0x190,ammo,sizeof(ammo)) || memcmp(ps+0x290,clip,sizeof(clip))) {
            fprintf(stderr,"ammo consumption mismatch case %d\n",i); return 1;
        }
    }
    FreeLibrary(module);
    puts("1208 lookup, 64000 reload and 256000 firing-ammo comparisons passed.");
    return 0;
}
