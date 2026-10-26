#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "../game/g_local.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
#include <limits.h>

/* Real ValidStatWeapon -> WolfMP -> Duplicate -> Item dependencies remain intact. */
static int compareStatWeapon(unsigned char *base, unsigned entry, int *count) {
    void *(__cdecl *original)(int)=(void *)(base+entry);
    int n, weapon; const int edges[]={INT_MIN,INT_MAX,-1,0,63,64,65,255};
    for(n=0;n<521;++n) {
        void *expected; gitem_t *actual;
        weapon=n<513?n-256:edges[n-513];
        expected=original(weapon);actual=BG_ValidStatWeapon(weapon);++*count;
        if(!!expected!=!!actual) {fprintf(stderr,"stat weapon%d null mismatch\n",weapon);return 1;}
        if(actual && (strcmp(*(char **)expected,actual->classname) ||
            *(int *)((char *)expected+0x24)!=actual->giType ||
            *(int *)((char *)expected+0x28)!=actual->giTag ||
            *(int *)((char *)expected+0x2c)!=actual->giAmmoIndex ||
            *(int *)((char *)expected+0x30)!=actual->giClipIndex ||
            actual!=BG_FindItemForWeapon(weapon))) {
            fprintf(stderr,"stat weapon%d item identity mismatch\n",weapon);return 1;
        }
    }
    return 0;
}

static int compareWeaponHelpers(unsigned char *base, int *stateCount, int *fastCount, int *ammoCount) {
    int (__cdecl *state)(int)=(void *)(base+0x2e740);
    int (__cdecl *fast)(int)=(void *)(base+0x2e8a0);
    int (__cdecl *ammo)(int,int *)=(void *)(base+0x2e1c0);
    tce_weaponDef_t *defs=(void *)(base+0x24435c0);
    int n,w,k,skill[7],values[]={INT_MIN,-100,-1,0,1,30,999,INT_MAX};
    for(n=0;n<522;++n) {
        int edge[]={INT_MIN,INT_MAX,-1,0,63,64,65,255,256};
        w=n<513?n-256:edge[n-513];
        ++*stateCount;if(state(w)!=BG_simpleWeaponState(w)){fprintf(stderr,"state%d mismatch\n",w);return 1;}
        ++*fastCount;if(fast(w)!=BG_isLightWeaponSupportingFastReload(w)){fprintf(stderr,"fast%d mismatch\n",w);return 1;}
    }
    for(w=0;w<64;++w)for(n=0;n<8;++n)for(k=0;k<4;++k) {
        int j;for(j=0;j<7;++j)skill[j]=k==1?-1:k==2?INT_MAX:j;
        defs[w].maxammo=weaponDef[w].maxammo=values[n];++*ammoCount;
        if(ammo(w,k?skill:NULL)!=BG_MaxAmmoForWeapon(w,k?skill:NULL)) {
            fprintf(stderr,"maxammo weapon%d value%d skill%d mismatch\n",w,values[n],k);return 1;
        }
    }
    return 0;
}

static int compareUtilityHelpers(unsigned char *base) {
    int (__cdecl *hints)(int,int)=(void *)(base+0x2e790);
    int (__cdecl *clean)(const char *,char *,unsigned,int)=(void *)(base+0x2e830);
    int (__cdecl *grenades)(int,int *)=(void *)(base+0x2b5c0);
    int (__cdecl *team)(int)=(void *)(base+0x2b620);
    int values[]={INT_MIN,-256,-1,0,1,2,255,INT_MAX},sizes[]={1,2,3,8,32};
    int i,j,k,t,n=0,skill[7]={0};char input[32],a[64],b[64];
    for(i=-256;i<=256;++i)for(j=0;j<8;++j) {
        if(hints(i,values[j])!=BG_simpleHintsCollapse(i,values[j])) {fprintf(stderr,"hint%d value%d mismatch\n",i,values[j]);return 1;}++n;
        skill[2]=values[j];skill[3]=values[7-j];
        if(grenades(i,skill)!=BG_GrenadesForClass(i,skill)) {fprintf(stderr,"grenade class%d mismatch\n",i);return 1;}++n;
    }
    for(i=0;i<8;++i){if(team(values[i])!=BG_GrenadeTypeForTeam(values[i]))return 1;++n;}
    for(i=-256;i<=256;++i){if(team(i)!=BG_GrenadeTypeForTeam(i))return 1;++n;}
    for(i=0;i<256;++i)for(j=0;j<5;++j)for(k=0;k<2;++k)for(t=0;t<3;++t) {
        int x,y; memset(input,0,sizeof(input));
        if(t==0){input[0]=(char)i;strcpy(input+1,"^1a^^b^\nZ");}
        else if(t==1){strcpy(input,"^7");input[2]=(char)i;strcpy(input+3,"^9X");}
        else{strcpy(input,"A\n\r");input[3]=(char)i;strcpy(input+4,"^");}
        memset(a,0x5a,sizeof(a));memset(b,0x5a,sizeof(b));
        x=clean(input,a,sizes[j],k);y=BG_cleanName(input,b,sizes[j],k);
        if(x!=y||memcmp(a,b,sizeof(a))){fprintf(stderr,"clean byte%d size%d newline%d pattern%d mismatch\n",i,sizes[j],k,t);return 1;}++n;
    }
    printf("%d whole hint/name/grenade utility comparisons passed\n",n);return 0;
}

int main(int argc, char **argv) {
    HMODULE dll;
    unsigned char *base;
    int (__cdecl *lookup)(int), (__cdecl *duplicate)(int);
    tce_weaponDef_t *original;
    tce_weaponDef_t saved[64], savedSource[64];
    int parsed, scenario, slot, query, i, n = 0, duplicates = 0, failed = 0, stats = 0, states = 0, fasts = 0, ammos = 0;
    int edges[] = { INT_MIN, -1, 0, 1, 63, 64, 65, 255, INT_MAX };
    if(argc != 3 || !(dll = LoadLibraryA(argv[1]))) return 2;
    base = (unsigned char *)dll;
    lookup = (void *)(base + 0x2dd00);
    duplicate = (void *)(base + 0x2dc20);
    original = (void *)(base + 0x24435c0);
    memcpy(saved, original, sizeof(saved));
    memcpy(savedSource, weaponDef, sizeof(savedSource));
    for(parsed = 0; parsed < 2; ++parsed) {
        gearDef.parsed = parsed;
        for(scenario = 0; scenario < 4; ++scenario) for(slot = 0; slot < 64; ++slot) {
            /* Distinct, all duplicate, adjacent duplicate and extreme MOD tables.
             * Mutate input data only: original and production bodies are untouched. */
            for(i = 0; i < 64; ++i) {
                int value = scenario == 0 ? 1000+i : scenario == 1 ? -7 :
                    scenario == 2 ? i/2 : (i&1 ? INT_MAX : INT_MIN);
                original[i].mod = weaponDef[i].mod = value;
            }
            original[slot].mod = weaponDef[slot].mod = 1234567;
            for(query = 0; query < 74; ++query) {
                int mod = query < 64 ? weaponDef[query].mod :
                    query == 64 ? 1234568 : edges[query-65];
                int expected = lookup(mod), actual = BG_WeaponForMOD(mod);
                ++n;
                if(expected != actual) {
                    fprintf(stderr,"MOD scenario%d slot%d query%d expected%d actual%d\n",scenario,slot,mod,expected,actual);
                    failed = 1; goto done;
                }
            }
        }
        for(i = -256; i <= 256; ++i) {
            if(duplicate(i) != BG_DuplicateWeapon(i)) { failed = 1; goto done; }
            ++duplicates;
        }
        for(i = 0; i < 9; ++i) {
            if(duplicate(edges[i]) != BG_DuplicateWeapon(edges[i])) { failed = 1; goto done; }
            ++duplicates;
        }
    }
done:
    if(!failed) failed=compareUtilityHelpers(base);
    if(!failed) failed=compareWeaponHelpers(base,&states,&fasts,&ammos);
    if(!failed) failed=compareStatWeapon(base,0x2dca0,&stats);
    memcpy(original, saved, sizeof(saved));
    memcpy(weaponDef, savedSource, sizeof(savedSource));
    FreeLibrary(dll);
    if(!failed) {
        if(!(dll = LoadLibraryA(argv[2]))) return 2;
        duplicate = (void *)((unsigned char *)dll + 0x5710);
        for(i = -256; i <= 256; ++i) {
            if(duplicate(i) != BG_DuplicateWeapon(i)) { failed = 1; break; }
            ++duplicates;
        }
        for(i = 0; i < 9 && !failed; ++i) {
            if(duplicate(edges[i]) != BG_DuplicateWeapon(edges[i])) { failed = 1; break; }
            ++duplicates;
        }
        if(!failed) failed=compareStatWeapon((unsigned char *)dll,0x5790,&stats);
        FreeLibrary(dll);
    }
    if(!failed) printf("%d MOD table, %d duplicate-ID and %d complete stat-weapon comparisons passed\n",n,duplicates,stats);
    if(!failed) printf("%d whole weapon-state, %d fast-reload and %d maxammo comparisons passed\n",states,fasts,ammos);
    return failed;
}
