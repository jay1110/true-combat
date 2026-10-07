#ifndef TCE_EJECT_BRASS_H
#define TCE_EJECT_BRASS_H
typedef struct {
    int weapon;                 /* TC ID, not the SDK weapon enum */
    vec3_t entityVelocity;      /* original centity offset0xa84 */
    int sizeVariant;            /* original global33fdf380; semantics unresolved */
} tce_brassContext_t;
localEntity_t *TCE_CG_AddEjectBrass(const refEntity_t *parent, const centity_t *cent,
    const tce_brassContext_t *context, qboolean localPlayer, qboolean cachedOrigin);
#endif
