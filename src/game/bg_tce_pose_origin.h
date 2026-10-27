/* Affected-pose translation. Private inputs; no SDK/entity ABI extension. */
#ifndef BG_TCE_POSE_ORIGIN_H
#define BG_TCE_POSE_ORIGIN_H
#include "q_shared.h"
#include "bg_public.h"
#include <stdint.h>

typedef struct {
    trajectory_t position, angles;
    int number, type, flags, parent, spline;
    float distance;
} tcePoseMoverState_t;
typedef struct {
    trajectory_t position;
    int number, ground, spline;
} tcePoseOriginState_t;
typedef int (*tcePoseMoverRead_t)(void *user,int number,tcePoseMoverState_t *out);

void BG_TCEPoseMoverState(const entityState_t *state,tcePoseMoverState_t *out);
void BG_TCEPoseOriginState(const entityState_t *state,tcePoseOriginState_t *out);
/* Read resolves immutable state owned by the publication/snapshot. All linked
 * ancestors must exist. Entity-chain bounds use the real normal-entity limit;
 * cycles/invalid parents fail, without altering any caller state or caches. */
int BG_TCEPoseMoverEvaluate(int number,uint32_t time,tcePoseMoverRead_t read,
                            void *user,float origin[3],float angles[3]);
/* Preserve the SDK interpolation early return. Extrapolation alone receives
 * target-relative mover pushing, using the NEW mover pivot and the actual
 * G_TryPushingEntity delta-angle order. No observer or predicted state is read.
 * Inputs/output may alias; failure clears output. Caller owns receipt/life/
 * teleport/gap selection and passes only an eligible optional next sample. */
int BG_TCEPosePresentationOrigin(const tcePoseOriginState_t *older,
    const tcePoseOriginState_t *newer,uint32_t from,uint32_t nextTime,uint32_t time,
    tcePoseMoverRead_t read,void *user,float out[3]);
#endif
