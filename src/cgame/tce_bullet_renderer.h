#ifndef TCE_BULLET_RENDERER_H
#define TCE_BULLET_RENDERER_H
#include "cg_local.h"
/* The SDK wall and mark ABIs cannot satisfy these callbacks. */
typedef struct {
    void (*wall)(int,int,vec3_t,vec3_t,vec3_t,unsigned,int,int);
    void (*particle)(vec3_t,vec3_t,int,int,float);
    void (*mark)(qhandle_t,vec3_t,vec3_t,float,float,float,float,float,int,float,int,int);
    int (*soundVolume)(vec3_t,float,float,int);
    float attenuation;
    int portal;
    int *lastBloodSpat;
} tce_bulletRendererContext_t;
void TCE_CG_Bullet(vec3_t end,int source,vec3_t normal,int flesh,int victim,int other,
    float water,int seed,int damage,vec3_t start,int predicted,const tce_bulletRendererContext_t *context);
#endif
