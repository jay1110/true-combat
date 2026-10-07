#ifndef TCE_WEAPON_CYCLE_H
#define TCE_WEAPON_CYCLE_H
/* Windows command/HUD state, projected independently of the original cg_t.
 * Production adapter lives in cg_weapons.c; cg owns reconnect-reset state. */
typedef struct {
    int hasSnapshot, snapshotFlags, snapshotPmFlags;
    int time, weaponSelectTime, snapshotWeaponState, predictedWeaponState;
    int snapshotWeapon, predictedWeapon;
    int cycleTime, cycleOffset, weaponSelect;
    int cycleSelected, buttons, cycleDelay;
} tce_weaponCycle_t;
void TCE_CG_CycleWeaponCommand(tce_weaponCycle_t *state,int direction);
/* Renderer services mirror the original HUD dependencies. Width is measured at
 * scale .2; text is painted at .2/.2 with style 3, border size is 1. */
typedef struct {
    int (*selectable)(int weapon);
    int (*width)(int weapon);
    void (*border)(float x,float y,float w,float h,const float *color);
    void (*text)(float x,float y,const float *color,int weapon);
    void (*finish)(int oldWeapon,int newWeapon);
} tce_weaponCycleServices_t;
void TCE_CG_DrawWeaponCycle(tce_weaponCycle_t *s,const int banks[10][22],
    const float *rect,const float *color,float hudAlpha,
    const tce_weaponCycleServices_t *api);
void TCE_CG_DrawSelectedWeapon(const tce_weaponCycle_t *s,const float *rect,
    const float *color,const tce_weaponCycleServices_t *api);
#endif
