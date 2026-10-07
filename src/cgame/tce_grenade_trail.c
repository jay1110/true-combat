/* TC:E cgame 30022a30; Linux CG_GrenadeTrail 000cf0fe.
 * Both originals return immediately. Do not restore the ET SDK smoke/bubble
 * trail here: even trailTime and lastTrailTime must remain untouched.
 */
#include "cg_local.h"

void TCE_CG_GrenadeTrail(centity_t *ent, const weaponInfo_t *wi) {
    (void)ent;
    (void)wi;
}
