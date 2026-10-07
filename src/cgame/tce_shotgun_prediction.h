#ifndef TCE_SHOTGUN_PREDICTION_H
#define TCE_SHOTGUN_PREDICTION_H
#include "../game/q_shared.h"
#include "../game/tce_bg.h"

/* Complete original call boundary, deliberately not the SDK CG_Bullet ABI.
 * The renderers may mutate end; the trace controller observes that mutation. */
typedef struct {
    int localClient, maxClients, predictBullets;
    const tce_weaponDef_t *weapons;
    void (*trace)(trace_t *, const float *, const float *, const float *, const float *, int, int);
    void (*bullet)(float *, int, float *, int, int, int, float, int, int, float *, int);
    void (*wall)(int, int, float *, float *, float *, int, int, int);
} tce_shotgunPrediction_t;

void TCE_CG_BulletFireExtended(const tce_shotgunPrediction_t *, int, int,
                             vec3_t, vec3_t, int, int, int);
void TCE_CG_ShotgunPattern(const tce_shotgunPrediction_t *, vec3_t, vec3_t, int, int, int);
void TCE_CG_ShotgunFire(const tce_shotgunPrediction_t *, entityState_t *);
#endif
