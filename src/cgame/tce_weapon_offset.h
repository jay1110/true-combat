#ifndef TCE_WEAPON_OFFSET_H
#define TCE_WEAPON_OFFSET_H
/* Explicit projection of the original TC client globals and weapon media.
 * weapon is a TC ID; weaponDef[weapon] must be initialized by the caller. */
typedef struct {
    int time, aimTime, aiming, secondaryAiming, secondaryAimTime;
    int developer, gunPosition;
    float developerOffset[3], gunViewOffset[3], gunAimOffset[3];
} tce_weaponOffset_t;

/* A non-null legacyStack supplies the three indeterminate original local
 * values for comparison/replay. NULL deliberately uses zero-initialized
 * locals: a deterministic repair, not a claim about the original stack. */
void TCE_CG_EliteFPWeaponOffset(int weapon, const tce_weaponOffset_t *state,
    const float axis[3][3], float offset[3], float secondaryOffset[3],
    float *tacticalScale, const float legacyStack[3]);
#endif
