#ifndef TCE_IMPACT_SPARKS_H
#define TCE_IMPACT_SPARKS_H
/* Include cg_local.h before this header. */
#define TCE_LE_FLAT_SPARK 14
#define TCE_LE_GLOW_SPARK 15
void TCE_CG_FlatSparks(vec3_t,vec3_t,int,float,qhandle_t);
void TCE_CG_GlowSparks(vec3_t,vec3_t,int,float,qhandle_t);
void TCE_CG_AddFlatSpark(localEntity_t *);
void TCE_CG_AddGlowSpark(localEntity_t *);
#endif
