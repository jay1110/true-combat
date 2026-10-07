#ifndef TCE_RELOAD_H
#define TCE_RELOAD_H
#include "tce_weapon_ammo.h"
int TCE_PM_IdleAnimForWeapon(int weapon);
int TCE_PM_StartWeaponAnim(int animation, int current, int moveType, int timer, int commandWeapon);
int TCE_PM_ContinueWeaponAnim(int animation, int current, int moveType, int timer, int commandWeapon);
typedef struct {
    int state, time, prone, noClips;
    int emptyAnimation, nonemptyAnimation;
    int reloadRequested, attack, attackHeld, idleAnimation;
    const int *ammo, *clip;
} tce_reloadState_t;
typedef struct {
    int bodyAnimation, weaponAnimation, event, missingItem;
} tce_reloadEffects_t;
enum { TCE_RELOAD_NONE, TCE_RELOAD_BEGIN, TCE_RELOAD_ROUND, TCE_RELOAD_CHANGE };
typedef struct { int action, alternate; } tce_reloadAction_t;
/* -1 means no effect. Event numbers retain original TC identities. */
void TCE_PM_BeginWeaponReload(int weapon, tce_reloadState_t *state,
    const tce_weaponDef_t *defs, tce_reloadEffects_t *effects);
void TCE_PM_ReloadSingleRound(int weapon, tce_reloadState_t *state,
    const tce_weaponDef_t *defs, tce_reloadEffects_t *effects);
/* These functions operate on valid gear/player-array indices0..63. */
void TCE_PM_CheckForReload(int weapon, tce_reloadState_t *state,
    const tce_weaponDef_t *defs, const int *legacyClipLimit, const int *alternates,
    tce_reloadEffects_t *effects, tce_reloadAction_t *action);
void TCE_PM_FinishWeaponReload(int weapon, tce_reloadState_t *state,
    const tce_weaponDef_t *defs, int *ammo, int *clip, tce_reloadEffects_t *effects);
#endif
