#ifndef TCE_WALL_RENDERER_H
#define TCE_WALL_RENDERER_H
#include "cg_local.h"
typedef struct
{
	const int *explosionFlash; /* 32588280 */
	const int *waterRipple; /* 3258843c */
	const int *bulletDefault; /* 32588468 */
	const int *bulletMetal; /* 3258846c */
	const int *bulletWood; /* 32588470 */
	const int *bulletGlass; /* 32588474 */
	const int *burn; /* 32588478 */
	const int *bulletStone; /* 3258847c */
	const int *knife; /* 32588480 */
	const int *exitStone; /* 32588488 */
	const int *exitMetal; /* 3258848c */
	const int *exitWood; /* 32588494 */
	const int *dirt; /* 32588578 */
	const int *water; /* 3258857c */
	const int *snow; /* 32588580 */
	const int *grass; /* 32588590 */
	const int *mud; /* 32588594 */
	int rocket[3]; /* 325886d4 */
	int rocketFar; /* 325886e0 */
	int mortar[3]; /* 325886e4 */
	int mortarFar; /* 325886f0 */
	const int *explosion; /* 32588870 */
	const int *explosionFar; /* 32588874 */
	const int *explosionWater; /* 32588878 */
	int dynamite; /* 3258887c */
	int dynamiteFar; /* 32588880 */
	int satchel; /* 32588884 */
	int satchelFar; /* 32588888 */
	const int *grenade; /* 325888a0 */
	const int *grenadeFar; /* 325888a4 */
	const int *fabricHit[5]; /* 32588ac8 */
	const int *metal2Hit[5]; /* 32588adc */
	const int *strawHit[5]; /* 32588b40 */
	const int *explosive4; /* 32588b54 */
	const int *explosive30; /* 32588b58 */
	const int *metalHit[5]; /* 32588b98 */
	const int *woodHit[5]; /* 32588bac */
	const int *glassHit[5]; /* 32588bc0 */
	const int *stoneHit[5]; /* 32588bd4 */
	const int *artillery; /* 32588bfc */
	const int *artilleryFar; /* 32588c00 */
	const int *knifeFlesh[4]; /* 32588c08 */
	const int *knifeWall; /* 32588c18 */
} tce_wallMedia_t;
typedef struct
{
	/* Original globals are read at the consuming branch, after callbacks. */
	refdef_t *const *view;
	const int *time, *markTime, *portal;
	const qboolean *forceMarks;
	const int *weaponMaterialMode[66];
	tce_wallMedia_t media;
	int (*surfaceType)(int);
	int (*soundVolume)(vec3_t, float, float, int);
	void (*print)(const char *);
	void (*bulletParticles)(vec3_t, vec3_t, int, int, int, float);
	void (*dirtParticles)(vec3_t, vec3_t, int, int, int, float, float, float, float, int);
	void (*particle)(vec3_t, vec3_t, int, int, float);
	void (*flatSparks)(vec3_t, vec3_t, int);
	void (*sparks)(vec3_t, vec3_t, int);
	void (*explode)(vec3_t, vec3_t, int, int, int, int, int);
	void (*debrisParticles)(vec3_t, vec3_t, int, int, int, float);
	void (*concussion)(vec3_t, int);
	void *(*smokePuff)(vec3_t, vec3_t, float, float, float, float, float, float, int, int, int, int);
	int (*pointContents)(const vec3_t, int);
	int (*cmPointContents)(const vec3_t, int);
	void (*boxTrace)(trace_t *, const vec3_t, const vec3_t, const vec3_t, const vec3_t, int, int);
	void (*particleExplosion)(const char *, vec3_t, vec3_t, int, int, int, int);
	void (*debris)(vec3_t, vec3_t, int, int, int);
	void (*ripple)(int, vec3_t, vec3_t, int, int);
	void (*eliteMark)(int, vec3_t, vec3_t, float, float, float, float, float, int, float, int, int);
	void (*impactMark)(int, vec3_t, vec3_t, float, float, float, float, float, float, int);
	int (*registerShader)(const char *);
	void (*sound)(const vec3_t, int, int, int, int);
	void (*soundEx)(const vec3_t, int, int, int, int, int);
	void (*projectDecal)(int, int, const vec3_t, const vec4_t, const vec4_t, int, int);
} tce_wallRendererContext_t;
void TCE_CG_MissileHitWall(int, int, vec3_t, vec3_t, vec3_t, unsigned, int, int, const tce_wallRendererContext_t *);
#endif
