#include "cg_local.h"

/* TC:E Windows 30053ec0. Preserve x87 precision until the byte/int casts;
 * only the fade-out endpoint and clock are explicitly stored as floats. */
void CG_AddRefEntityWithPowerups(refEntity_t *ent, int powerups, int team,
                               entityState_t *es, const vec3_t fireRiseDir) {
    centity_t *cent = &cg_entities[es->number];
    refEntity_t backup;
    int start, end;
    double alpha;
    (void)powerups;
    (void)team;
    ent->entityNum = es->number;
    backup = *ent;
    if (CG_EntOnFire(&cg_entities[es->number]))
        ent->reFlags |= REFLAG_FORCE_LOD;
    trap_R_AddRefEntityToScene(ent);
    if (CG_EntOnFire(&cg_entities[es->number])) {
        if (ent->entityNum == cg.snap->ps.clientNum) {
            start = cg.snap->ps.onFireStart;
            end = start + 1500;
        } else {
            start = es->onFireStart;
            end = es->onFireEnd;
        }
        alpha = ((double)cg.time - start) * (1.0 / 1500.0);
        if (alpha > 1.0) {
            alpha = ((double)(float)end - (float)cg.time) * (1.0 / 1500.0);
            if (alpha > 1.0) alpha = 1.0;
        }
        if (alpha < 0.0) alpha = 0.0;
        alpha *= 255.0;
        ent->shaderRGBA[3] = (byte)alpha;
        VectorCopy(fireRiseDir, ent->fireRiseDir);
        if (VectorCompare(ent->fireRiseDir, vec3_origin))
            VectorSet(ent->fireRiseDir, 0, 0, 1);
        ent->customShader = cgs.media.onFireShader;
        trap_R_AddRefEntityToScene(ent);
        ent->customShader = cgs.media.onFireShader2;
        trap_R_AddRefEntityToScene(ent);
        if (ent->hModel == cent->pe.bodyRefEnt.hModel)
            trap_S_AddLoopingSound(ent->origin, vec3_origin,
                                  cgs.media.flameCrackSound, (int)alpha, 0);
    }
    *ent = backup;
}
