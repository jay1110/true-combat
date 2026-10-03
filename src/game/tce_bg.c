/* Reconstructed shared TC:E functions; see reconstruction/STATUS.md. */
#include "q_shared.h"
#include "tce_bg.h"

tce_weaponDef_t weaponDef[TCE_MAX_WEAPONS];
tce_gearDef_t gearDef;

int BG_WolfClassToTCE(int playerClass) {
    switch (playerClass) {
    case 2: case 3: return 1;
    case 4: case 5: return 2;
    default: return 0;
    }
}

int BG_WeapIDToWeaponNum(const char *id) {
    static const struct { const char *name; int weapon; } ids[] = {
        {"K1", 1},
        {"G1", 9},
        {"G2", 30},
        {"G3", 4},
        {"DC1", 15},
        {"SH1", 2},
        {"SH2", 39},
        {"SHA1", 38},
        {"SHA2", 37},
        {"SH5", 40},
        {"SH7", 10},
        {"SH8", 3},
        {"TH1", 8},
        {"TH2", 45},
        {"TH3", 41},
        {"TH4", 33},
        {"TH5", 42},
        {"TH11", 48},
        {"TH12", 49},
        {"TH7", 44},
        {"TH8", 43},
        {"TH9", 50},
        {"TH13", 47},
        {"TH14", 51},
        {"TH15", 46},
        {"SH3", 52},
        {"SH4", 14},
        {"SHA3", 54},
        {"SHA4", 53},
        {"SH6", 7},
        {"TH6", 5},
        {"TH10", 24},
        {"TH16", 23},
        {"TH17", 32},
        {"TH18", 25},
        {"TH19", 6},
        {"TH20", 13},
    };
    unsigned int i;
    for (i = 0; i < sizeof(ids)/sizeof(ids[0]); ++i)
        if (!Q_stricmp(id, ids[i].name)) return ids[i].weapon;
    return 0;
}

int BG_CheckUTWeapon(int weapon) {
    switch (weapon) {
    case 1: case 2: case 3: case 4: case 5: case 6: case 7: case 8:
    case 9: case 10: case 13: case 14: case 23: case 24: case 25:
    case 30: case 32: case 33: case 39: case 40: case 41: case 42:
    case 43: case 44: case 45: case 46: case 47: case 48: case 49:
    case 50: case 51: case 52: return 1;
    default: return 0;
    }
}

int TCE_BG_WeaponInWolfMP(int weapon) {
    /* Windows 0x30003900. TC:E additionally accepts these non-UT weapons. */
    switch (weapon) {
    case 15: case 37: case 38: case 53: case 54: return 1;
    default: return BG_CheckUTWeapon(weapon);
    }
}

int BG_WeaponIsAvailable(int weapon, int requiredSkill, int weaponTeam,
                        int playerSkill, int playerTeam) {
    /* Windows 0x30006650. Keep short circuit order: invalid IDs must never
     * index gearDef. Zero requiredSkill disables a weapon in the original;
     * negative values are deliberately not treated as zero. */
    return TCE_BG_WeaponInWolfMP(weapon) &&
        (weaponTeam > 1 || weaponTeam == playerTeam - 1) &&
        gearDef.weaponFile[weapon][0] != '\0' &&
        requiredSkill != 0 && requiredSkill <= playerSkill;
}

int BG_FiremodeWeapon(int weapon) {
    const tce_weaponDef_t *w = &weaponDef[weapon];
    return !w->bolt && ((w->semiauto && w->fullauto) ||
        (w->semiauto && w->burst > 1) || (w->semiauto && w->pump) ||
        (w->fullauto && w->burst > 1));
}

int BG_SidearmAvailableForPrimary(int sidearm, int primary) {
    return weaponDef[sidearm].loadoutWeight < 1 || weaponDef[primary].loadoutWeight < 4;
}

int BG_GrenadeSelectionForPrimary(int primary) {
    int weight = weaponDef[primary].loadoutWeight;
    return weight < 4 ? 21 : weight < 5 ? 20 : 19;
}

int BG_WeapToWeaponOnBack(int weapon) { return weapon; }
int BG_WeaponOnBackToWeap(int weapon) { return weapon; }

int TCE_BG_BBoxCollision(const float *minsA, const float *maxsA,
                     const float *minsB, const float *maxsB) {
    int i;
    for (i = 0; i < 3; ++i)
        if (maxsB[i] < minsA[i] || maxsA[i] < minsB[i]) return 0;
    return 1;
}

int TCE_BG_FootstepForSurface(unsigned int flags) {
    if (flags & 0x2000) return 23;
    if (flags & 0x40) return 5;
    switch (flags & 0xff000000U) {
    case 0x03000000: case 0x04000000: case 0x1f000000: return 1;
    case 0x05000000: return 2;
    case 0x0a000000: return 3;
    case 0x07000000: case 0x1d000000: return 4;
    case 0x13000000: return 6;
    case 0x14000000: return 11;
    case 0x0d000000: return 7;
    case 0x10000000: case 0x20000000: case 0x21000000: return 8;
    case 0x15000000: return 12;
    case 0x16000000: return 13;
    case 0x08000000: case 0x1e000000: return 10;
    default: return 0;
    }
}
