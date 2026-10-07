#include "cg_local.h"
#include "tce_smoke_grenade.h"

int tceSmokeNewBBox;
static qhandle_t smokeShaders[4];

void TCE_CG_RegisterSmokeMedia(void) {
    smokeShaders[0]=trap_R_RegisterShader("m83Smoke");
    smokeShaders[1]=trap_R_RegisterShader("m83Smoke2");
    smokeShaders[2]=trap_R_RegisterShader("m83Smoke3");
    /* Original RegisterGraphics also precaches this currently unused variant. */
    smokeShaders[3]=trap_R_RegisterShader("m83Smoke4");
}

void TCE_CG_DrawSmokeGrenade(centity_t *cent) {
    const tce_lightGrid_t *grid=TCE_CG_MapLightGrid();
    /* Invalid map data or a future network start must not enter sqrt/grid
     * indexing with NaNs. Valid TC:E states follow the original path. */
    if(!grid || cg.time<cent->currentState.time)return;
    TCE_CG_SmokeGrenadeExtended(grid,cg.time,cent->currentState.time,
        tceSmokeNewBBox!=0,cent->lerpOrigin,cg.refdef.viewaxis[0],
        cent->tceSmokeColor,smokeShaders);
}
