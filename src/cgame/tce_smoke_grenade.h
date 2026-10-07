#ifndef TCE_SMOKE_GRENADE_H
#define TCE_SMOKE_GRENADE_H
#include "tce_lightgrid.h"
extern int tceSmokeNewBBox;
void TCE_CG_SmokeGrenadeExtended(const tce_lightGrid_t *grid, int now, int start,
    qboolean large, const vec3_t origin, const vec3_t forward, vec3_t cachedColor,
    const qhandle_t shaders[3]);
void TCE_CG_RegisterSmokeMedia(void);
void TCE_CG_DrawSmokeGrenade(centity_t *cent);
#endif
