#ifndef TCE_ADD_FRAGMENT_H
#define TCE_ADD_FRAGMENT_H
typedef struct {
    int sizeVariant; /* original 33fdf380 */
    float gravityScale; /* original localEntity +0x6c; absent in SDK */
    /* Required TC dependency bindings; no SDK fallback for nondefault gravity. */
    void (*evaluateTrajectory)(const trajectory_t *, int, vec3_t, qboolean, int, float);
    void (*bounceSound)(localEntity_t *, trace_t *);
} tce_fragmentContext_t;
void TCE_CG_AddFragment(localEntity_t *le, const tce_fragmentContext_t *context);
#endif
