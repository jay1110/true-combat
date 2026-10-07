/* Reconstructed shared TC:E functions; see reconstruction/STATUS.md. */
#include "q_shared.h"
#include "tce_bg.h"

tce_weaponDef_t weaponDef[TCE_MAX_WEAPONS];
tce_gearDef_t gearDef;
tce_gametypeDef_t tce_gametypeDef[8];

/* qagame2002eca0: leave unrelated entries/fields untouched. */
void BG_InitializeGametypeDef(void) {
    tce_gametypeDef[5].maxClients = 20;
    tce_gametypeDef[5].field04 = 16;
    tce_gametypeDef[5].field0c = 1;
    tce_gametypeDef[7].maxClients = 36;
    tce_gametypeDef[7].field04 = 32;
    tce_gametypeDef[7].field0c = 1;
    tce_gametypeDef[2].maxClients = 36;
    tce_gametypeDef[2].field04 = 32;
    tce_gametypeDef[2].field0c = 1;
}

/* UI400016b0. This is the original official-map classification, not a
 * filesystem existence check: custom maps remain available in the list. */
int BG_MapIsOfficial(const char *mapname) {
    return !Q_stricmp(mapname,"obj_stadtrand") || !Q_stricmp(mapname,"obj_delta") ||
        !Q_stricmp(mapname,"obj_railhouse") || !Q_stricmp(mapname,"obj_northport") ||
        !Q_stricmp(mapname,"obj_village") || !Q_stricmp(mapname,"obj_snow") ||
        !Q_stricmp(mapname,"obj_hideout");
}

/* TC heavy-weapon table: qagame200b8590 and cgame3009a9b0. Legacy65/66
 * are compared as integers only, never used to index64-slot weapon arrays. */
int TCE_BG_IsHeavyWeapon(int weapon) {
    switch (weapon) {
    case 66: case 31: case 62: case 65: case 35: case 60: return 1;
    default: return 0;
    }
}

int BG_WolfClassToTCE(int playerClass) {
    switch (playerClass) {
    case 2: case 3: return 1;
    case 4: case 5: return 2;
    default: return 0;
    }
}

int BG_WeapIDToWeaponNum(const char *id) {
    static const struct { const char *name; int weapon; } ids[] = {
        {"K1", TCE_WP_K1},
        {"G1", TCE_WP_G1},
        {"G2", TCE_WP_G2},
        {"G3", TCE_WP_G3},
        {"DC1", TCE_WP_DC1},
        {"SH1", TCE_WP_SH1},
        {"SH2", TCE_WP_SH2},
        {"SHA1", TCE_WP_SHA1},
        {"SHA2", TCE_WP_SHA2},
        {"SH5", TCE_WP_SH5},
        {"SH7", TCE_WP_SH7},
        {"SH8", TCE_WP_SH8},
        {"TH1", TCE_WP_TH1},
        {"TH2", TCE_WP_TH2},
        {"TH3", TCE_WP_TH3},
        {"TH4", TCE_WP_TH4},
        {"TH5", TCE_WP_TH5},
        {"TH11", TCE_WP_TH11},
        {"TH12", TCE_WP_TH12},
        {"TH7", TCE_WP_TH7},
        {"TH8", TCE_WP_TH8},
        {"TH9", TCE_WP_TH9},
        {"TH13", TCE_WP_TH13},
        {"TH14", TCE_WP_TH14},
        {"TH15", TCE_WP_TH15},
        {"SH3", TCE_WP_SH3},
        {"SH4", TCE_WP_SH4},
        {"SHA3", TCE_WP_SHA3},
        {"SHA4", TCE_WP_SHA4},
        {"SH6", TCE_WP_SH6},
        {"TH6", TCE_WP_TH6},
        {"TH10", TCE_WP_TH10},
        {"TH16", TCE_WP_TH16},
        {"TH17", TCE_WP_TH17},
        {"TH18", TCE_WP_TH18},
        {"TH19", TCE_WP_TH19},
        {"TH20", TCE_WP_TH20},
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

/* qagame2002b330; Linux00081f2c. Unknown top-byte IDs return0. */
unsigned int BG_SurfaceType2Flag(unsigned int material) {
    static const unsigned char highByte[37] = {
        0,0,2,3,4,5,7,8,9,10,11,12,13,15,16,0,23,18,19,
        20,21,22,1,6,14,17,24,25,26,27,28,29,30,31,32,33,34
    };
    return material<37 ? (unsigned int)highByte[material]<<24 : 0;
}

int BG_SurfaceFlag2Type(unsigned int flags) {
    static const unsigned char type[35]={
        1,22,2,3,4,5,23,6,7,8,9,10,11,12,24,13,14,25,
        17,18,19,20,21,16,26,27,28,29,30,31,32,33,34,35,36
    };
    unsigned int material=flags>>24;
    if(!material && (flags&0x14)) return 0;
    return material<35 ? type[material] : 0;
}

/* Original pierceTable200b66b0; descriptive field names from2009d500.
 * Data recovered, recursive wall penetration not yet integrated. */
const tce_pierceMaterial_t tcePierceTable[37]={
    {9999,16},{2000,16},{1000,24},{200,32},{200,32},{200,32},
    {9999,32},{9999,32},{200,32},{200,32},{50,64},{200,32},
    {200,32},{200,32},{200,32},{200,32},{200,32},{200,32},
    {200,32},{200,32},{200,32},{200,32},{1000,24},{200,32},
    {200,32},{200,32},{1000,24},{1000,24},{1000,24},{1000,24},
    {1000,24},{9999,32},{9999,32},{2000,4},{50,64},{50,64},{2000,16}
};

int TCE_BG_FootstepForSurface(unsigned int flags) {
    if (flags & 0x2000) return 23;
    if (flags & 0x40) return 5;
    switch (BG_SurfaceFlag2Type(flags)) {
    case 3: case 4: case 33: return 1;
    case 5: return 2;
    case 9: return 3;
    case 6: case 31: return 4;
    case 18: return 6;
    case 19: return 11;
    case 12: return 7;
    case 14: case 34: case 35: return 8;
    case 20: return 12;
    case 21: return 13;
    case 7: case 32: return 10;
    default: return 0;
    }
}

/* qagame 20030500; Linux 000881ce. Team 2 is Specops. */
int BG_DefaultWeaponForClass(int team, int playerClass) {
    int tcClass = BG_WolfClassToTCE(playerClass);
    if (team == 2)
        return tcClass == 1 ? TCE_WP_TH1 : tcClass == 2 ? TCE_WP_TH13 : TCE_WP_TH2;
    return tcClass == 1 ? TCE_WP_SH7 : tcClass == 2 ? TCE_WP_TH14 : TCE_WP_TH4;
}

/* Original whitelist, including reserved SH3 (52), not SDK MP5SD (41). */
int TCE_BG_SupportsFastReload(int weapon) {
    switch (weapon) {
    case 2: case 7: case 3: case 8: case 10: case 14: case 33: case 52: return 1;
    default: return 0;
    }
}

/* cgame 30008820/30008870. Skill is projected from pm->skill[4]. */
int TCE_PM_NonemptyReloadAnimForWeapon(int weapon, int lightWeaponSkill) {
    if (weapon == TCE_WP_SH1 || weapon == TCE_WP_SH2 || weapon == TCE_WP_SH5) return 9;
    return lightWeaponSkill >= 2 && TCE_BG_SupportsFastReload(weapon) ? 8 : 7;
}

int TCE_PM_ReloadAnimForWeapon(int weapon, int lightWeaponSkill) {
    if (weapon == 55 || weapon == 56) return 8;
    if (weapon == 62) return 9;
    return lightWeaponSkill >= 2 && TCE_BG_SupportsFastReload(weapon) ? 8 : 7;
}
