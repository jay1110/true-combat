#ifndef TCE_IMPACT_PARTICLES_H
#define TCE_IMPACT_PARTICLES_H
typedef struct { qhandle_t blood,smoke3,smoke4; } tce_impactParticleMedia_t;
void TCE_CG_ParticleTest(vec3_t origin,vec3_t direction,int count,int material,float alpha,const tce_impactParticleMedia_t *media);
#endif
