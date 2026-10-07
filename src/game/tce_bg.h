#ifndef TCE_BG_H
#define TCE_BG_H

/* Recovered TC:E 0.49b shared ABI. Unknown fields remain explicitly unnamed.
 * Evidence: reconstruction/evidence and the symbol-bearing Linux modules.
 * Do not substitute the ET weapon_t enum: TC:E reuses and extends its IDs.
 */
#include <stddef.h>
#include "tce_weapon_ids.h"
#define TCE_MAX_WEAPONS TCE_WEAPON_CAPACITY

typedef struct {
    unsigned char unknown_000[0x40];
    int startClips;                       /* 0x040 */
    unsigned char unknown_044[8];
    char weapClass[64];                   /* 0x04c */
    char caliberClass[64];                /* 0x08c */
    int characteristic;                  /* 0x0cc */
    int maxammo, uses;                    /* 0x0d0, 0x0d4 */
    int maxclip;                         /* 0x0d8 */
    int startingAmmo, startingClip, reloadTime, fireDelayTime, nextShotTime;
    int maxHeat, coolRate;                /* SDK-compatible ammo fields */
    int unknown_0f8[9];
    int loadoutWeight;                   /* 0x11c; descriptive recovered name */
    int unknown_120, unknown_124, mod;
    int unknown_12c, unknown_130, unknown_134, unknown_138;
    int unknown_13c[3];
    int pelletCount;                     /* 0x148; descriptive name */
    int unknown_14c[2];
    int unknown_154, unknown_158;
    int semiauto, fullauto, burst, pump, bolt; /* 0x15c..0x16c */
    int singleReload;
    float scoped;
    int scopeReticleType, useScopeReticleShader, nightVision, longWaveIR;
    int suppressed, subsonic;
    float jamPercentage;
    int noTacMode, grenadeTimer;
    int unknown_19c, unknown_1a0, weaponCategory;
    int unknown_1a8, unknown_1ac, unknown_1b0;
    int usesWolfAnim, usesPistolAnimMod, usesRecoilAnimMod;
    int unknown_1c0;
    int parsed;
    int unknown_1c8;
} tce_weaponDef_t;

typedef struct {
    char weaponFile[TCE_MAX_WEAPONS][64];
    int startClips[TCE_MAX_WEAPONS];
    int team[TCE_MAX_WEAPONS];
    int slot[TCE_MAX_WEAPONS];
    int requiredSkill[TCE_MAX_WEAPONS][3];
    int equivalentWeapon[TCE_MAX_WEAPONS];
    char playerSkinGroup[64], playerVoiceGroup[64], playerIconGroup[64];
    char customWeaponGroup[64];
    int mapOverbrightBits, parsed;
} tce_gearDef_t;

/* C89-compatible compile-time checks for the recovered 32-bit layouts. */
typedef char tce_weapon_size_check[sizeof(tce_weaponDef_t) == 0x1cc ? 1 : -1];
typedef char tce_weight_offset_check[offsetof(tce_weaponDef_t, loadoutWeight) == 0x11c ? 1 : -1];
typedef char tce_firemode_offset_check[offsetof(tce_weaponDef_t, semiauto) == 0x15c ? 1 : -1];
typedef char tce_parsed_offset_check[offsetof(tce_weaponDef_t, parsed) == 0x1c4 ? 1 : -1];
typedef char tce_gear_size_check[sizeof(tce_gearDef_t) == 0x1808 ? 1 : -1];

extern tce_weaponDef_t weaponDef[TCE_MAX_WEAPONS];
extern tce_gearDef_t gearDef;

/* Windows gametypeDef stride16: the third field is not named speculatively. */
typedef struct { int maxClients, field04, unknown08, field0c; } tce_gametypeDef_t;
extern tce_gametypeDef_t tce_gametypeDef[8];
void BG_InitializeGametypeDef(void);
int BG_MapIsOfficial(const char *mapname);

int TCE_PWF_ParseError(int handle, const char *format, ...);
int BG_ParseGearDef(const char *primary, const char *fallback, tce_gearDef_t *definition);
int BG_ParseWeaponDef(const char *filename, tce_weaponDef_t *definition);
int BG_InitializeWeaponDef(tce_weaponDef_t *definition);

int BG_WolfClassToTCE(int playerClass);
int TCE_BG_IsHeavyWeapon(int weapon);
int BG_DefaultWeaponForClass(int team, int playerClass);
int TCE_BG_SupportsFastReload(int weapon);
int TCE_PM_NonemptyReloadAnimForWeapon(int weapon, int lightWeaponSkill);
int TCE_PM_ReloadAnimForWeapon(int weapon, int lightWeaponSkill);
int BG_WeapIDToWeaponNum(const char *id);
int BG_CheckUTWeapon(int weapon);
/* Separate from the ET SDK whitelist until all weapon IDs are integrated. */
int TCE_BG_WeaponInWolfMP(int weapon);
int BG_WeaponIsAvailable(int weapon, int requiredSkill, int weaponTeam,
                        int playerSkill, int playerTeam);
int BG_FiremodeWeapon(int weapon);
int BG_SidearmAvailableForPrimary(int sidearm, int primary);
int BG_GrenadeSelectionForPrimary(int primary);
int BG_WeapToWeaponOnBack(int weapon);
int BG_WeaponOnBackToWeap(int weapon);
int TCE_BG_BBoxCollision(const float *minsA, const float *maxsA,
                     const float *minsB, const float *maxsB);
int BG_SurfaceFlag2Type(unsigned int flags);
unsigned int BG_SurfaceType2Flag(unsigned int material);
typedef struct { int resistance, thicknessScale; } tce_pierceMaterial_t;
extern const tce_pierceMaterial_t tcePierceTable[37];
int TCE_BG_FootstepForSurface(unsigned int flags);

#endif
