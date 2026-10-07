#ifndef TCE_TRAJECTORY_H
#define TCE_TRAJECTORY_H
/* Original TC trajectory IDs: 13/14/15 are ballistic modes, 16 is linear path.
 * Script producers and linked-entity consumers use TC16. The obsolete SDK enum
 * spelling TR_LINEAR_PATH still denotes13; do not use it for new TC paths. */
void TCE_BG_EvaluateTrajectory(const trajectory_t *tr, int atTime, vec3_t result,
    qboolean isAngle, int splinePath, float gravityScale);
void TCE_BG_EvaluateTrajectoryDelta(const trajectory_t *tr, int atTime, vec3_t result,
    qboolean isAngle, int splinePath, float gravityScale);
#endif
