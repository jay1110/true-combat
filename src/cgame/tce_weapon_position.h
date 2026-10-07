#ifndef TCE_WEAPON_POSITION_H
#define TCE_WEAPON_POSITION_H
/* Explicit projection of TC:E client state, not an SDK cg_t layout.
 * Unknown input semantics retain their original address suffix. */
typedef struct {
    int time, weapon, eFlags, thirdPerson, weaponState;
    float mountedPitch;
    float viewOrigin[3], viewAxis[3][3], viewAngles[3], gunViewAngles[3];
    int postureModified, proneMovingTime, flags_3407dfb0, stanceTime, pmFlags, duckTime;
    float lean;
    int aiming, leanTime, shotTime, count_3407dff8, scopeEnabled, firemodeTime;
    float speed, bobSin;
    int bobCycle, swayTime;
    float priorForward[3], swayHorizontal, swayVertical, velocity[3];
    int proneTime, stepTime, scale_3407dfbc, scale_3407dfc0, landTime;
    float stepChange, landChange;
    int developer;
    float developerAngles[3], tacticalScale;
    int tacticalPitch, tacticalYaw;
    float kickAngles[3];
} tce_weaponPosition_t;
void TCE_CG_CalculateWeaponPosition(tce_weaponPosition_t *s, float *origin, float *angles);
#endif
