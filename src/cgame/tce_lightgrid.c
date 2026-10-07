#include "cg_local.h"
#include "tce_lightgrid.h"

/* Windows 300682e0. The lower boundary deliberately accepts cell -1,
 * clamping its index but retaining its fractional coordinate. */
void TCE_CG_LightForPoint(const tce_lightGrid_t *grid, const vec3_t point,
                        vec3_t ambient, vec3_t directed, vec3_t direction) {
    int cell[3], stride[3], i, corner, offset;
    float fraction[3], relative, position;
    double weight, total = 0, normal[3] = {0,0,0};
    vec3_t normalFloat;
    qboolean outside = qfalse;
    const byte *sample;
    for (i=0; i<3; ++i) {
        relative = point[i] - grid->origin[i];
        position = (float)((double)relative / grid->spacing[i]);
        cell[i] = (int)floor((double)relative / grid->spacing[i]);
        fraction[i] = position - cell[i];
        if (cell[i] < 0) {
            if (cell[i] < -1) outside = qtrue;
            cell[i] = 0;
        } else if (cell[i] >= grid->bounds[i] - 1.0f) {
            cell[i] = (int)(grid->bounds[i] - 1.0f);
            outside = qtrue;
        }
    }
    if (outside) {
        VectorSet(ambient,96,96,96);
        VectorSet(directed,48,48,48);
        return; /* Original leaves direction unchanged on this path. */
    }
    VectorClear(ambient);
    VectorClear(directed);
    stride[0] = 8;
    stride[1] = (int)(grid->bounds[0] * 8.0);
    stride[2] = (int)((double)grid->bounds[0] * grid->bounds[1] * 8.0);
    for (corner=0; corner<8; ++corner) {
        offset = cell[0]*8 + cell[1]*stride[1] + cell[2]*stride[2];
        weight = 1.0;
        for (i=0; i<3; ++i) {
            if (corner & (1<<i)) {
                weight *= fraction[i];
                offset += stride[i];
            } else weight *= 1.0 - fraction[i];
        }
        sample = grid->samples + offset;
        if (!(sample[0] + sample[1] + sample[2])) continue;
        total += weight;
        for (i=0; i<3; ++i) {
            ambient[i] = (float)(ambient[i] + weight * sample[i]);
            directed[i] = (float)(directed[i] + weight * sample[i+3]);
        }
        normal[0] += (double)grid->sine[(sample[7]*4+256)&1023] * grid->sine[sample[6]*4] * weight;
        normal[1] += (float)((double)grid->sine[sample[7]*4] * grid->sine[sample[6]*4]) * weight;
        normal[2] += grid->sine[(sample[6]*4+256)&1023] * weight;
    }
    if (total > 0 && total < 0.99) {
        for (i=0; i<3; ++i) {
            ambient[i] = (float)(ambient[i] / total);
            directed[i] = (float)(directed[i] / total);
        }
    }
    for (i=0; i<3; ++i) normalFloat[i] = (float)normal[i];
    VectorNormalize2(normalFloat,direction);
}

/* Windows 30068160; deliberately only an upper clamp. */
void TCE_CG_LightForParticleSimple(const tce_lightGrid_t *grid,
                                 const vec3_t point, vec3_t color) {
    vec3_t directed, direction;
    int i;
    TCE_CG_LightForPoint(grid,point,color,directed,direction);
    for (i=0; i<3; ++i) {
        color[i] = (float)(((double)color[i] + directed[i]) * (1.0f/255.0f));
        if (color[i] >= 1.0f) color[i] = 1.0f;
    }
}

/* Windows 30068200 / Linux CG_LightForParticleDirected. */
void TCE_CG_LightForParticleDirected(const tce_lightGrid_t *grid,
                                   const vec3_t point,const vec3_t normal,vec3_t color) {
    vec3_t directed,direction;
#if !defined(_MSC_VER) || !defined(_M_IX86)
    float incidence;
    int i;
#endif
    TCE_CG_LightForPoint(grid,point,color,directed,direction);
#if defined(_MSC_VER) && defined(_M_IX86)
    {
        const float lightZero=0.0f, lightOne=1.0f, lightUnit=1.0f/255.0f;
        float lightRed, lightBlue;
        /* 30068224..300682d9: z+y+x, retained sqrt and green clamp.
         * The first/third clamp consume stored floats; green consumes ST0. */
        __asm {
            mov edx, normal
            mov esi, color
            lea ecx, direction
            fld dword ptr [ecx+8]
            fmul dword ptr [edx+8]
            fld dword ptr [ecx+4]
            fmul dword ptr [edx+4]
            faddp st(1), st(0)
            fld dword ptr [ecx]
            fmul dword ptr [edx]
            faddp st(1), st(0)
            fcom lightZero
            fnstsw ax
            test ah, 41h
            jnz particle_light_skip
            fsqrt
            lea ecx, directed
            fld dword ptr [ecx]
            fmul st(0), st(1)
            fadd dword ptr [esi]
            fstp dword ptr [esi]
            fld dword ptr [ecx+4]
            fmul st(0), st(1)
            fadd dword ptr [esi+4]
            fstp dword ptr [esi+4]
            fld dword ptr [ecx+8]
            fmul st(0), st(1)
            fadd dword ptr [esi+8]
            fstp dword ptr [esi+8]
particle_light_skip:
            fstp st(0)
            fld dword ptr [esi]
            fmul lightUnit
            fst lightRed
            fstp dword ptr [esi]
            fld dword ptr [esi+4]
            fmul lightUnit
            fst dword ptr [esi+4]
            fld dword ptr [esi+8]
            fmul lightUnit
            fst lightBlue
            fstp dword ptr [esi+8]
            fld lightRed
            fcomp lightOne
            fnstsw ax
            test ah, 1
            jnz particle_light_green
            mov dword ptr [esi], 3f800000h
particle_light_green:
            fcomp lightOne
            fnstsw ax
            test ah, 1
            jnz particle_light_blue
            mov dword ptr [esi+4], 3f800000h
particle_light_blue:
            fld lightBlue
            fcomp lightOne
            fnstsw ax
            test ah, 1
            jnz particle_light_done
            mov dword ptr [esi+8], 3f800000h
particle_light_done:
        }
    }
#else
    incidence=(float)((double)direction[0]*normal[0]+(double)direction[1]*normal[1]+
                      (double)direction[2]*normal[2]);
    if(incidence>0) {
        incidence=(float)sqrt(incidence);
        for(i=0;i<3;++i) color[i]=(float)((double)directed[i]*incidence+color[i]);
    }
    for(i=0;i<3;++i) {
        color[i]*=(1.0f/255.0f);
        if(color[i]>=1.0f) color[i]=1.0f;
    }
#endif
}
