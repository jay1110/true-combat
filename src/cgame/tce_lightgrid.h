#ifndef TCE_LIGHTGRID_H
#define TCE_LIGHTGRID_H

/* Explicit view of the TC:E grid, independent of the incompatible cg_t.
 * Each sample has ambient RGB, directed RGB, latitude and longitude bytes.
 * The caller supplies a valid grid and the original 1024-entry sine table.
 * Map loading and lifetime management are reconstructed separately. */
typedef struct {
    vec3_t origin, spacing, bounds;
    const byte *samples;
    const float *sine;
} tce_lightGrid_t;

extern vec3_t tce_lightGridSpacing;
void TCE_CG_InitLightSine(void);
qboolean TCE_CG_LoadLightGrid(const char *mapname);
const tce_lightGrid_t *TCE_CG_MapLightGrid(void);

void TCE_CG_LightForPoint(const tce_lightGrid_t *grid, const vec3_t point,
                        vec3_t ambient, vec3_t directed, vec3_t direction);
void TCE_CG_LightForParticleSimple(const tce_lightGrid_t *grid,
                                 const vec3_t point, vec3_t color);

void TCE_CG_LightForParticleDirected(const tce_lightGrid_t *grid,
                                   const vec3_t point,const vec3_t normal,vec3_t color);

#endif
