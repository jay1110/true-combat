#ifndef TCE_COMPAT_POSE_ORIGIN_H
#define TCE_COMPAT_POSE_ORIGIN_H
struct splinePath_s;
/* Class B domain adapter for the mod's historical spline/root store ordering.
 * Real OS/ABI policy stays outside gameplay. Bounded traversal affects only
 * the canonical presentation plan; original gameplay spline helper is intact. */
int TCE_PosePathOffset(float radius,struct splinePath_s **path,float *fraction,float point[3]);
/* Affected snapshot presentation uses one explicit binary64 interpolation
 * contract on all targets. Stored stages avoid compiler/x87 excess precision;
 * the ordinary SDK CGame interpolator retains its original platform behavior.
 * This is an intentional shared-policy boundary, not a parity claim for every
 * legacy extended-precision halfway input. */
static inline float TCE_PoseOriginInterpolate(float older,float newer,float fraction) {
    volatile double difference=(double)newer-(double)older;
    volatile double product=(double)fraction*difference;
    volatile double sum=(double)older+product;
    return (float)sum;
}
#endif
