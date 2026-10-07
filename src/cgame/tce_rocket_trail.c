/* TC:E Windows CG_RocketTrail @ 3006ca10. */
#include "cg_local.h"

void CG_RocketTrail(centity_t *ent, const weaponInfo_t *wi) {
    entityState_t *es = &ent->currentState;
    vec3_t origin, lastPos;
    int step, time, contents, lastContents;
    (void)wi;
    step = es->eType == ET_FLAMEBARREL ? 30 : es->eType == ET_FP_PARTS ? 50 : 10;
    time = ((int)((unsigned int)ent->trailTime + step) / step) * step;
    BG_EvaluateTrajectory(&es->pos, cg.time, origin, qfalse, es->effect2Time);
    contents = CG_PointContents(origin, -1);
    if (es->eType != ET_RAMJET && es->pos.trType == TR_STATIONARY) {
        ent->trailTime = cg.time;
        return;
    }
    BG_EvaluateTrajectory(&es->pos, ent->trailTime, lastPos, qfalse, es->effect2Time);
    lastContents = CG_PointContents(lastPos, -1);
    ent->trailTime = cg.time;
    if (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA)) {
        if (contents & lastContents & CONTENTS_WATER) CG_BubbleTrail(lastPos, origin, 3, 8);
        return;
    }

    for (; time <= ent->trailTime; time = (int)((unsigned int)time + step)) {
        float randomSize;
        BG_EvaluateTrajectory(&es->pos, time, lastPos, qfalse, es->effect2Time);
        randomSize = (rand() & 0x7fff) * (1.0f / 32767.0f);
        /* Original stores randomSize as float, but multiplies in x87 before
         * truncating to integers. Double avoids premature float rounding. */
        if (es->eType == ET_FLAMEBARREL || es->eType == ET_FP_PARTS) {
            if (rand() % 100 > 50)
                CG_ParticleExplosion("twiltb2", lastPos, vec3_origin,
                    100 + (int)((double)randomSize * 400), 5,
                    7 + (int)((double)randomSize * 10), qfalse);
        } else if (es->eType == ET_RAMJET) {
            VectorCopy(ent->lerpOrigin, lastPos);
            CG_ParticleExplosion("twiltb2", lastPos, vec3_origin,
                100 + (int)((double)randomSize * 100), 5,
                5 + (int)((double)randomSize * 10), qfalse);
            CG_ParticleExplosion("blacksmokeanim", lastPos, vec3_origin,
                400 + (int)((double)randomSize * 750), 12,
                24 + (int)((double)randomSize * 30), qfalse);
            continue;
        } else if (es->eType == ET_FIRE_COLUMN || es->eType == ET_FIRE_COLUMN_SMOKE) {
            int duration, sizeStart, sizeEnd;
            if (es->density) {
                vec3_t angles, right;
                VectorCopy(es->apos.trBase, angles);
                angles[ROLL] += cg.time % 360;
                AngleVectors(angles, NULL, right, NULL);
                VectorMA(lastPos, es->density, right, lastPos);
            }
            duration = (int)es->angles[0];
            sizeStart = (int)es->angles[1];
            sizeEnd = (int)es->angles[2];
            if (!duration) duration = 100;
            if (!sizeStart) sizeStart = 5;
            if (!sizeEnd) sizeEnd = 7;
            CG_ParticleExplosion("twiltb2", lastPos, vec3_origin,
                duration + (int)((double)randomSize * 400), sizeStart,
                sizeEnd + (int)((double)randomSize * 10), qfalse);
            if (es->eType != ET_FIRE_COLUMN_SMOKE || rand() % 100 <= 50) continue;
        }
        CG_ParticleExplosion("blacksmokeanim", lastPos, vec3_origin,
            800 + (int)((double)randomSize * 1500), 5,
            12 + (int)((double)randomSize * 30), qfalse);
    }
}
