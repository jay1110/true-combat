#ifndef TCE_BULLET_H
#define TCE_BULLET_H
#include "q_shared.h"
#include "tce_bg.h"

/* Inputs of qagame Bullet_Endpos2009d1a0. shotAngles is the original
 * client's +0x129c field (pmext+0x64), captured by PM_Weapon before recoil.
 * Hip-shot producer is integrated; ADS offsets and impact/penetration remain open. */
typedef struct {
    unsigned int seed;
    int aiming, prone, ducked;
    int movementSpread, shotSpread, phase;
    vec3_t shotAngles, muzzle, right, up;
} tce_bulletAim_t;

void TCE_BulletEndpos(const tce_weaponDef_t *def,
                     const tce_bulletAim_t *aim, vec3_t end);
/* Recovered client-hit damage argument in Bullet_Fire_Extended2009d500.
 * Does not implement hit location, armor, penetration or G_Damage itself. */
int TCE_BulletClientDamage(const tce_weaponDef_t *def, int damage,
                          int distance, int newBoundingBox);
/* Wall-energy subpath only; tracing, exit events and recursive dispatch remain
 * in-progress. Inputs are the snapped entry/exit material flags and distance. */
int TCE_BulletMaterialLoss(const tce_weaponDef_t *def, unsigned entryFlags,
                          unsigned exitFlags, double thickness, int energy,
                          int damage, int *remainingEnergy, int *remainingDamage);
typedef void (*tce_bulletTrace_t)(trace_t *, const float *, const float *,
                                const float *, const float *, int, int);
void TCE_BulletDeflect(const vec3_t start, vec3_t end, float retainedFraction,
                      unsigned seed, int wallsRemaining);
/* Original wall-exit search after the material/caliber gate. Entry is already
 * snapped. Returns a geometrically forward exit; does not damage breakables. */
int TCE_BulletWallExit(const trace_t *entry, const vec3_t start, const vec3_t end,
                      int passEntity, tce_bulletTrace_t trace, trace_t *exit);
#endif
