/* TC:E cgame CG_PanzerFaustEjectBrass @ 3006e480. */
#include "cg_local.h"

void CG_PanzerFaustEjectBrass(centity_t *cent) {
    localEntity_t *le = CG_AllocLocalEntity();
    vec3_t axis[3];
    float waterScale = 1.0f;
    int i, lifetimeBase;
    le->leType = LE_FRAGMENT;
    le->startTime = cg.time;
    lifetimeBase = (int)((unsigned int)le->startTime + (unsigned int)cg_brassTime.integer * 8u);
    /* Original keeps the random product/sum in x87 until __ftol. */
    le->endTime = (int)((double)(rand() & 0x7fff) * (1.0f / 32767.0f) *
                        cg_brassTime.integer + lifetimeBase);
    le->pos.trType = TR_GRAVITY;
    le->pos.trTime = (int)((unsigned int)cg.time - (rand() & 15));
    AnglesToAxis(cent->lerpAngles, axis);
    for (i = 0; i < 3; ++i) {
        le->refEntity.origin[i] = (float)((double)axis[2][i] * 24.0 -
            (double)axis[1][i] * 4.0 - (double)axis[0][i] * 24.0 + cent->lerpOrigin[i]);
    }
    VectorCopy(le->refEntity.origin, le->pos.trBase);
    if (CG_PointContents(le->refEntity.origin, -1) & (CONTENTS_WATER | CONTENTS_SLIME))
        waterScale = 0.1f;
    for (i = 0; i < 3; ++i)
        le->pos.trDelta[i] = (float)(((double)axis[0][i] * 16.0 -
                                    (double)axis[1][i] * 200.0) * waterScale);
    AxisCopy(axisDefault, le->refEntity.axis);
    le->sizeScale = 3.0f;
    le->refEntity.hModel = cgs.media.panzerfaustBrassModel;
    le->bounceFactor = (float)(0.4 * waterScale);
    le->angles.trType = TR_LINEAR;
    le->angles.trTime = cg.time;
    VectorSet(le->angles.trBase, 0, cent->currentState.apos.trBase[1], 0);
    VectorClear(le->angles.trDelta);
    le->leFlags = LEF_TUMBLE | LEF_SMOKING;
    le->leBounceSoundType = LEBS_NONE;
    le->leMarkType = LEMT_NONE;
}
