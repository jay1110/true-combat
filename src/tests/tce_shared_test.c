/* Differential tests against the supplied Windows TC:E DLL.
 * No engine is started. Calls are restricted to verified leaf functions.
 * CMake pins the DLL SHA-256 before enabling these address-specific calls.
 */
#include "../game/q_shared.h"
#include "../game/tce_bg.h"
#include <windows.h>

static int checks;
int testWeaponParser(unsigned char *base, const char *directory);
void testWeaponInitializer(unsigned char *base);
static void check(int actual, int expected, const char *name) {
    ++checks;
    if (actual != expected) {
        fprintf(stderr, "%s: got %d, original %d\n", name, actual, expected);
        exit(1);
    }
}

/* Diagnostics required by SDK q_shared.c, not replacements for game logic. */
void QDECL Com_Printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}
void QDECL Com_Error(int level, const char *fmt, ...) {
    va_list ap; (void)level;
    va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap); exit(2);
}

int Q_vsnprintf(char *dest, int size, const char *fmt, va_list args) {
    int result;
    #undef _vsnprintf
    result = _vsnprintf(dest, size - 1, fmt, args);
    dest[size - 1] = 0;
    return result < 0 || result >= size ? -1 : result;
}

int main(int argc, char **argv) {
    HMODULE dll;
    unsigned char *base;
    int (__cdecl *originalClass)(int);
    int (__cdecl *originalId)(const char *);
    int (__cdecl *originalCheck)(int);
    int (__cdecl *originalWolfMP)(int);
    int (__cdecl *originalAvailable)(int,int,int,int,int);
    int (__cdecl *originalFiremode)(int);
    int (__cdecl *originalSidearm)(int,int);
    unsigned char (__cdecl *originalGrenade)(int);
    unsigned char (__cdecl *originalFootstep)(unsigned int);
    int (__cdecl *originalBBox)(const float *,const float *,const float *,const float *);
    int (__cdecl *originalBack)(int);
    tce_weaponDef_t *originalWeapons;
    char (*originalWeaponFiles)[64];
    int i, j, a, b, c, d, e;
    const char *prefixes[] = {"K", "G", "DC", "SH", "SHA", "TH", "th", "invalid"};
    char id[40];
    float minsA[3] = {0,0,0}, maxsA[3] = {1,1,1};
    float minsB[3], maxsB[3];
    if (argc != 3) return 2;
    dll = LoadLibraryA(argv[1]);
    if (!dll) { fprintf(stderr, "LoadLibrary failed: %lu\n", GetLastError()); return 2; }
    base = (unsigned char *)dll;
    originalClass = (void *)(base + 0x8250);
    originalId = (void *)(base + 0x8290);
    originalCheck = (void *)(base + 0x66a0);
    originalWolfMP = (void *)(base + 0x3900);
    originalAvailable = (void *)(base + 0x6650);
    originalWeaponFiles = (void *)(base + 0x4ba9f00);
    originalFiremode = (void *)(base + 0x6700);
    originalSidearm = (void *)(base + 0x81e0);
    originalGrenade = (void *)(base + 0x8220);
    originalFootstep = (void *)(base + 0x6950);
    originalBBox = (void *)(base + 0x6900);
    originalBack = (void *)(base + 0x6770);
    originalWeapons = (void *)(base + 0x4c1df20);

    for (i = -5; i < 80; ++i) {
        check(BG_WolfClassToTCE(i), originalClass(i), "class");
        check(BG_CheckUTWeapon(i), originalCheck(i), "UT weapon");
        check(TCE_BG_WeaponInWolfMP(i), originalWolfMP(i), "TC:E MP weapon");
        check(BG_WeapToWeaponOnBack(i), originalBack(i), "weapon on back");
        check(BG_WeaponOnBackToWeap(i), originalBack(i), "weapon from back");
    }
    for (i = 0; i < sizeof(prefixes)/sizeof(prefixes[0]); ++i)
        for (j = 0; j < 70; ++j) {
            sprintf(id, "%s%d", prefixes[i], j);
            check(BG_WeapIDToWeaponNum(id), originalId(id), id);
        }
    check(BG_WeapIDToWeaponNum(""), originalId(""), "empty weapon ID");
    /* Full cross product: invalid/valid IDs, missing gear, required/actual
     * skill, weapon-team selector and player team, including zero/negative
     * values. These catch the original's non-obvious zero-skill disable. */
    for (i = -5; i < 80; ++i) for (j = 0; j < 2; ++j) {
        if (i >= 0 && i < TCE_MAX_WEAPONS)
            gearDef.weaponFile[i][0] = originalWeaponFiles[i][0] = j ? 'x' : 0;
        for (a = -2; a <= 4; ++a) for (b = -2; b <= 4; ++b)
        for (c = -2; c <= 4; ++c) for (d = -2; d <= 4; ++d)
            check(BG_WeaponIsAvailable(i,a,b,c,d), originalAvailable(i,a,b,c,d), "weapon availability");
    }
    for (i = 0; i < 256; ++i)
        for (j = 0; j < 4; ++j) {
            unsigned int flags = ((unsigned int)i << 24) | ((j&1) ? 0x40 : 0) | ((j&2) ? 0x2000 : 0);
            check(TCE_BG_FootstepForSurface(flags), originalFootstep(flags), "footstep");
        }
    /* Negative, zero, true and burst counts: all combinations, every slot. */
    for (i = 0; i < TCE_MAX_WEAPONS; ++i)
        for (a = -1; a <= 2; ++a) for (b = -1; b <= 2; ++b)
        for (c = -1; c <= 3; ++c) for (d = -1; d <= 2; ++d)
        for (e = -1; e <= 2; ++e) {
            weaponDef[i].semiauto = a; weaponDef[i].fullauto = b;
            weaponDef[i].burst = c; weaponDef[i].pump = d; weaponDef[i].bolt = e;
            originalWeapons[i] = weaponDef[i];
            check(BG_FiremodeWeapon(i), originalFiremode(i), "firemode");
        }
    for (i = -2; i <= 8; ++i) for (j = -2; j <= 8; ++j) {
        weaponDef[0].loadoutWeight = originalWeapons[0].loadoutWeight = i;
        weaponDef[63].loadoutWeight = originalWeapons[63].loadoutWeight = j;
        check(BG_SidearmAvailableForPrimary(0,63), originalSidearm(0,63), "sidearm");
        check(BG_GrenadeSelectionForPrimary(63), originalGrenade(63), "grenade");
    }
    for (a = -3; a <= 3; ++a) for (b = -3; b <= 3; ++b) for (c = -3; c <= 3; ++c) {
        minsB[0] = a*0.5f; minsB[1] = b*0.5f; minsB[2] = c*0.5f;
        for (i = 0; i < 3; ++i) maxsB[i] = minsB[i] + 0.5f;
        check(TCE_BG_BBoxCollision(minsA,maxsA,minsB,maxsB), originalBBox(minsA,maxsA,minsB,maxsB), "bounds");
    }
    testWeaponInitializer(base);
    testWeaponParser(base, argv[2]);
    FreeLibrary(dll);
    printf("%d comparisons with the original TC:E Windows DLL passed.\n", checks);
    return 0;
}
