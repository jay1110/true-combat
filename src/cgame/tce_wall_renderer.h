#ifndef TCE_WALL_RENDERER_H
#define TCE_WALL_RENDERER_H
#include "cg_local.h"
typedef struct {
    int explosionFlash; /* 32588280 */
    int waterRipple; /* 3258843c */
    int bulletDefault; /* 32588468 */
    int bulletMetal; /* 3258846c */
    int bulletWood; /* 32588470 */
    int bulletGlass; /* 32588474 */
    int burn; /* 32588478 */
    int bulletStone; /* 3258847c */
    int knife; /* 32588480 */
    int exitStone; /* 32588488 */
    int exitMetal; /* 3258848c */
    int exitWood; /* 32588494 */
    int dirt; /* 32588578 */
    int water; /* 3258857c */
    int snow; /* 32588580 */
    int grass; /* 32588590 */
    int mud; /* 32588594 */
    int rocket[3]; /* 325886d4 */
    int rocketFar; /* 325886e0 */
    int mortar[3]; /* 325886e4 */
    int mortarFar; /* 325886f0 */
    int explosion; /* 32588870 */
    int explosionFar; /* 32588874 */
    int explosionWater; /* 32588878 */
    int dynamite; /* 3258887c */
    int dynamiteFar; /* 32588880 */
    int satchel; /* 32588884 */
    int satchelFar; /* 32588888 */
    int grenade; /* 325888a0 */
    int grenadeFar; /* 325888a4 */
    int fabricHit[5]; /* 32588ac8 */
    int metal2Hit[5]; /* 32588adc */
    int strawHit[5]; /* 32588b40 */
    int explosive4; /* 32588b54 */
    int explosive30; /* 32588b58 */
    int metalHit[5]; /* 32588b98 */
    int woodHit[5]; /* 32588bac */
    int glassHit[5]; /* 32588bc0 */
    int stoneHit[5]; /* 32588bd4 */
    int artillery; /* 32588bfc */
    int artilleryFar; /* 32588c00 */
    int knifeFlesh[4]; /* 32588c08 */
    int knifeWall; /* 32588c18 */
} tce_wallMedia_t;
typedef struct {
    const refdef_t *view;
    int time,markTime,forceMarks,portal;
    int weaponMaterialMode[66];
    tce_wallMedia_t media;
    int (*surfaceType)(int);
    int (*soundVolume)(vec3_t,float,float,int);
    void (*print)(const char *);
    void (*bulletParticles)(vec3_t,vec3_t,int,int,int,float);
    void (*dirtParticles)(vec3_t,vec3_t,int,int,int,float,float,float,float,int);
    void (*particle)(vec3_t,vec3_t,int,int,float);
    void (*flatSparks)(vec3_t,vec3_t,int);
    void (*sparks)(vec3_t,vec3_t,int);
    void (*explode)(vec3_t,vec3_t,int,int,int,int,int);
    void (*debrisParticles)(vec3_t,vec3_t,int,int,int,float);
    void (*concussion)(vec3_t,int);
    void *(*smokePuff)(vec3_t,vec3_t,float,float,float,float,float,float,int,int,int,int);
    int (*pointContents)(const vec3_t,int);
    int (*cmPointContents)(const vec3_t,int);
    void (*boxTrace)(trace_t *,const vec3_t,const vec3_t,const vec3_t,const vec3_t,int,int);
    void (*particleExplosion)(const char *,vec3_t,vec3_t,int,int,int,int);
    void (*debris)(vec3_t,vec3_t,int,int,int);
    void (*ripple)(int,vec3_t,vec3_t,int,int);
    void (*eliteMark)(int,vec3_t,vec3_t,float,float,float,float,float,int,float,int,int);
    void (*impactMark)(int,vec3_t,vec3_t,float,float,float,float,float,float,int);
    int (*registerShader)(const char *);
    void (*sound)(const vec3_t,int,int,int,int);
    void (*soundEx)(const vec3_t,int,int,int,int,int);
    void (*projectDecal)(int,int,const vec3_t,const vec4_t,const vec4_t,int,int);
} tce_wallRendererContext_t;
void TCE_CG_MissileHitWall(int,int,vec3_t,vec3_t,vec3_t,unsigned,int,int,const tce_wallRendererContext_t *);
#endif
