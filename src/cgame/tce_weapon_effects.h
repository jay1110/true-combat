#ifndef TCE_WEAPON_EFFECTS_H
#define TCE_WEAPON_EFFECTS_H
void TCE_CG_FlamethrowerFlame(centity_t *cent, int tceWeapon, vec3_t origin);
void CG_FlamethrowerFlame(centity_t *cent, vec3_t origin);
void CG_AddWeaponWithPowerups(refEntity_t *gun, int powerups,
    playerState_t *ps, centity_t *cent);
#endif
