#ifndef TCE_AIM_H
#define TCE_AIM_H

/* Projected ABI of Windows PM_AdjustAimSpreadScale, 30009c30. */
typedef struct {
    int zooming, ducked, weaponState;
    int commandTime, oldCommandTime;
    int angles[2], oldAngles[2];
    float velocity[2];
    int movementRecovery, shotRecovery;
    float scoped;
    unsigned int seed, weaponFlags, movementFlags;
    int movementInstability, shotInstability, phase;
    float aimSpreadFloat;
    int aimSpread;
} tce_aimState_t;

void TCE_PM_AdjustAimSpreadScale(tce_aimState_t *s);
#endif
