#include "q_shared.h"
#include "bg_public.h"
#include "tce_trajectory.h"

/* TC IDs and x87 intermediate precision; this is not the derivative of every
 * corresponding position formula. Preserve the original's special cases. */
void TCE_BG_EvaluateTrajectoryDelta(const trajectory_t *tr, int atTime,
    vec3_t result, qboolean isAngle, int splinePath, float gravityScale) {
    int i, end = (int)((unsigned)tr->trTime + (unsigned)tr->trDuration);
    int milliseconds = (int)((unsigned)atTime - (unsigned)tr->trTime);
    double time = milliseconds * .001, scale;
    (void)isAngle;
    (void)splinePath;

    switch ((int)tr->trType) {
    case 0: case 1: case 12: case 16:
        VectorClear(result);
        return;
    case 3:
        if (atTime > end) { VectorClear(result); return; }
        /* Before the start, the original still returns the full velocity. */
    case 2:
        VectorCopy(tr->trDelta, result);
        return;
    case 5:
        scale = cos(((double)milliseconds / tr->trDuration) *
            (double)6.2831854820251465f) * .5;
        break;
    case 6: case 7: case 8: case 13:
        scale = tr->trType == 6 ? 800 : tr->trType == 7 ? 400 :
            tr->trType == 8 ? 200 : 500;
        VectorCopy(tr->trDelta, result);
        result[2] = (float)(tr->trDelta[2] - time * scale);
        return;
    case 10: case 11:
        if (atTime > end) { VectorClear(result); return; }
        scale = tr->trType == 10 ? time * time : time;
        break;
    case 14:
        if (atTime > end) { VectorClear(result); }
        else { VectorCopy(tr->trDelta, result); }
        result[2] = (float)(result[2] - (double)tr->trDuration * .005f * 8);
        return;
    case 15:
        if (atTime > end) { VectorClear(result); }
        else {
            for (i = 0; i < 3; ++i) result[i] = (float)(time * tr->trDelta[i]);
        }
        /* Z velocity is stored as float before subtracting the gravity term. */
        result[2] = (float)(result[2] - (double)gravityScale * 16);
        return;
    default:
        /* Including paused gravity (9); original diagnostic prints trTime. */
        Com_Error(ERR_DROP, "BG_EvaluateTrajectoryDelta: unknown trType: %i", tr->trTime);
        return;
    }
    for (i = 0; i < 3; ++i) result[i] = (float)(scale * tr->trDelta[i]);
}
