#include "cg_local.h"

/* CG_MG42EFX300757c0. Snapshot order determines the first matching mount.
 * Only the snapshot IDs are read here; state comes from cg_entities. */
void CG_MG42EFX(centity_t *cent) {
    int i,component;
    vec3_t forward;
    refEntity_t flash;
    centity_t *mount;

    for (i = 0; i < cg.snap->numEntities; ++i) {
        mount = &cg_entities[cg.snap->entities[i].number];
        if (mount->currentState.eType != ET_MG42_BARREL ||
            mount->currentState.otherEntityNum != cent->currentState.number)
            continue;

        AngleVectors(cent->lerpAngles, forward, NULL, NULL);
        memset(&flash, 0, sizeof(flash));
        flash.renderfx = RF_LIGHTING_ORIGIN;
        flash.hModel = cgs.media.mg42muzzleflash;
        for (component = 0; component < 3; ++component)
            flash.origin[component] = (float)(mount->currentState.pos.trBase[component] +
                40.0 * forward[component]);
        AnglesToAxis(cent->lerpAngles, flash.axis);
        trap_R_AddRefEntityToScene(&flash);

        /* Original consumes rand(), but integer division makes its light
         * contribution zero. Keep both the RNG step and constant intensity. */
        (void)rand();
        trap_R_AddLightToScene(flash.origin, 320.0f, 1.25f,
            1.0f, 0.6f, 0.23f, 0, 0);
        return;
    }
}
