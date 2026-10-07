#ifndef TCE_WEAPON_FRAMES_H
#define TCE_WEAPON_FRAMES_H
void CG_StartWeaponAnim(int anim);
void CG_ContinueWeaponAnim(int anim);
qboolean CG_GetPartFramesFromWeap(centity_t *cent, refEntity_t *part,
    refEntity_t *parent, int partId, weaponInfo_t *wi);
/* Explicit animation-array interface supports both SDK and TC:E media. */
void TCE_CG_SetWeapLerpFrameAnimation(animation_t *animations, lerpFrame_t *lf, int number);
void TCE_CG_ClearWeapLerpFrame(animation_t *animations, lerpFrame_t *lf, int number);
qboolean TCE_CG_GetPartFramesFromWeap(const lerpFrame_t *lf, refEntity_t *part,
    const refEntity_t *parent, int partId, const animation_t *animations);
int TCE_PM_RaiseAnimForWeapon(int weapon);
void TCE_CG_RunWeapLerpFrame(animation_t *animations, lerpFrame_t *lf, int number, float speedScale, int raiseAnimation);
void TCE_CG_WeaponAnimation(int number, animation_t *animations, int *oldFrame,
    int *frame, float *backlerp, int raiseAnimation);
#endif
