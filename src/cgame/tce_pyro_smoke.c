/* TC:E cgame: CG_GetWindVector 3006c610, CG_PyroSmokeTrail 3006c670. */
#include "cg_local.h"

/* The original multiplies by the float reciprocal, rather than dividing. */
static float smokeRandom(void) {
    return (rand() & 0x7fff) * (1.0f / 32767.0f);
}

void CG_GetWindVector(vec3_t direction) {
    direction[0] = smokeRandom() * 0.25f;
    direction[1] = cgs.smokeWindDir;
    direction[2] = smokeRandom();
    VectorNormalize(direction);
}

void CG_PyroSmokeTrail(centity_t *ent, const weaponInfo_t *wi) {
    entityState_t *es = &ent->currentState;
    vec3_t origin, lastPos, direction;
    int team, time;
    float randomSize, tint, speed;
    (void)wi;

    if (es->weapon == WP_LANDMINE) {
        if (es->teamNum < 8) {
            ent->miscTime = 0;
            return;
        }
        if (es->teamNum < 12 && !ent->miscTime) {
            ent->trailTime = ent->miscTime = cg.time;
            trap_S_StartSound(NULL, es->number, CHAN_WEAPON, cgs.media.minePrimedSound);
        }
        if ((int)((unsigned int)cg.time - (unsigned int)ent->miscTime) > 1000) return;
        team = es->otherEntityNum2 ? TEAM_AXIS : TEAM_ALLIES;
    } else {
        team = es->teamNum;
    }

    time = ((int)((unsigned int)ent->trailTime + 30u) / 30) * 30;
    BG_EvaluateTrajectory(&es->pos, cg.time, origin, qfalse, es->effect2Time);
    (void)CG_PointContents(origin, -1);
    BG_EvaluateTrajectory(&es->pos, ent->trailTime, lastPos, qfalse, es->effect2Time);
    (void)CG_PointContents(lastPos, -1);
    ent->trailTime = cg.time;

    /* Contents queries are retained but do not suppress dye/smoke in water. */
    for (; time <= ent->trailTime; time = (int)((unsigned int)time + 30u)) {
        BG_EvaluateTrajectory(&es->pos, time, lastPos, qfalse, es->effect2Time);
        (void)rand(); /* Original consumes an otherwise unused random value. */
        if (es->density) {
            vec3_t angles, right;
            VectorCopy(es->apos.trBase, angles);
            angles[ROLL] += cg.time % 360;
            AngleVectors(angles, NULL, right, NULL);
            VectorMA(lastPos, es->density, right, lastPos);
        }
        direction[0] = (float)(2.0 * (smokeRandom() - 0.5) * 5.0);
        direction[1] = (float)(2.0 * (smokeRandom() - 0.5) * 5.0);
        direction[2] = 0;
        VectorAdd(lastPos, direction, origin);
        randomSize = smokeRandom();
        CG_GetWindVector(direction);
        speed = es->weapon == WP_LANDMINE ? 45.0f : 65.0f;
        VectorScale(direction, speed, direction);
        tint = (randomSize + 1.0f) * 0.5f;
        CG_SmokePuff(origin, direction, 25.0f + randomSize * 110.0f,
                     team == TEAM_ALLIES ? tint : 1.0f, tint,
                     team == TEAM_ALLIES ? 1.0f : tint, 0.5f,
                     (float)(4800 + rand() % 2800), time, 0, 0,
                     cgs.media.smokePuffShader);
    }
}
