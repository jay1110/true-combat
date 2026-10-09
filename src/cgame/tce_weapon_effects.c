#include "cg_local.h"
#include "tce_weapon_effects.h"

/* Windows cgame30072320: entity-state weapons already carry TC protocol IDs.
 * In particular SDK WP_FLAMETHROWER (6) is an ordinary TC gear slot, not66. */
void TCE_CG_FlamethrowerFlame(centity_t *cent, int tceWeapon, vec3_t origin) {
    if (tceWeapon != 66) return;
    CG_FireFlameChunks(cent, origin, cent->lerpAngles, 1.0f, qtrue);
}

void CG_FlamethrowerFlame(centity_t *cent, vec3_t origin) {
    TCE_CG_FlamethrowerFlame(cent, cent->currentState.weapon, origin);
}

/* Windows cgame 30072350: all three contextual arguments are unused. The
 * original submits the existing entity once, without adding powerup passes. */
void CG_AddWeaponWithPowerups(refEntity_t *gun, int powerups,
    playerState_t *ps, centity_t *cent) {
    (void)powerups;
    (void)ps;
    (void)cent;
    trap_R_AddRefEntityToScene(gun);
}
