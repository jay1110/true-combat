#include "cg_local.h"
#include "tce_lightgrid.h"
#include "../game/tce_numeric.h"

/* Windows 300682e0 / Linux 000b4d36. The lower boundary accepts cell -1,
 * clamping its index but retaining its fraction. Valid grids must provide all
 * eight addressed samples; outside returns leave direction unchanged. */
void TCE_CG_LightForPoint(const tce_lightGrid_t *grid, const vec3_t point,
                        vec3_t ambient, vec3_t directed, vec3_t direction) {
    int cell[3], stride[3], i, corner, offset;
    unsigned offsetBits, ambientBytes[3], directedBytes[3], latitude, longitude;
    float fraction[3], relative[3];
    long double position, limit, weight, total = 0.0L, inverse;
    long double xDirection, yDirection, zDirection;
    vec3_t normalFloat = {0,0,0};
#if defined(_WIN32)
    float storedPosition;
    long double normal[3] = {0.0L,0.0L,0.0L};
#else
    long double rowStride;
#endif
    qboolean outside = qfalse;
    const byte *sample;

    for (i=0; i<3; ++i) relative[i] = point[i] - grid->origin[i];
    for (i=0; i<3; ++i) {
#if defined(_WIN32)
        position = (long double)relative[i] / grid->spacing[i];
        /* The CRT receives a double copy; fraction uses the separate float. */
        storedPosition = (float)position;
        cell[i] = TCE_TruncateNativeInteger(floor((double)position));
        fraction[i] = (float)((long double)storedPosition - cell[i]);
#else
        position = (1.0L / grid->spacing[i]) * relative[i];
        cell[i] = TCE_TruncateNativeInteger((double)floorl(position));
        fraction[i] = (float)(position - cell[i]);
#endif
        if (cell[i] < 0) {
            if (cell[i] < -1) outside = qtrue;
            cell[i] = 0;
        } else {
#if defined(_WIN32)
            limit = (float)((long double)grid->bounds[i] - 1.0L);
            if ((long double)cell[i] >= limit) {
#else
            limit = (long double)grid->bounds[i] - 1.0L;
            /* Linux also takes this branch for an unordered upper limit. */
            if (!(limit > (long double)cell[i])) {
#endif
                cell[i] = TCE_TruncateNativeInteger(limit);
                outside = qtrue;
            }
        }
    }
    if (outside) {
        VectorSet(ambient,96,96,96);
        VectorSet(directed,48,48,48);
        return; /* Original leaves direction unchanged on this path. */
    }
    ambient[2] = 0.0f;
    ambient[1] = 0.0f;
    ambient[0] = 0.0f;
    directed[2] = 0.0f;
    directed[1] = 0.0f;
    directed[0] = 0.0f;
    stride[0] = 8;
#if defined(_WIN32)
    stride[1] = TCE_TruncateNativeInteger((long double)grid->bounds[0] * 8.0L);
    stride[2] = TCE_TruncateNativeInteger(
        ((long double)grid->bounds[1] * grid->bounds[0]) * 8.0L);
#else
    rowStride = (long double)grid->bounds[0] * 8.0L;
    stride[1] = TCE_TruncateNativeInteger(rowStride);
    stride[2] = TCE_TruncateNativeInteger(rowStride * grid->bounds[1]);
#endif
    for (corner=0; corner<8; ++corner) {
        /* Preserve native low32 address arithmetic without signed C overflow. */
        offsetBits = (unsigned)cell[0]*8u +
                     (unsigned)cell[1]*(unsigned)stride[1] +
                     (unsigned)cell[2]*(unsigned)stride[2];
        weight = 1.0L;
        for (i=0; i<3; ++i) {
            if (corner & (1<<i)) {
                weight *= fraction[i];
                offsetBits += (unsigned)stride[i];
            } else weight *= 1.0L - fraction[i];
        }
        memcpy(&offset, &offsetBits, sizeof(offset));
        sample = grid->samples + offset;
        for (i=0; i<3; ++i) ambientBytes[i] = sample[i];
        if (!(ambientBytes[0] + ambientBytes[1] + ambientBytes[2])) continue;
        total += weight;
        for (i=0; i<3; ++i) directedBytes[i] = sample[i+3];
        for (i=0; i<3; ++i)
            ambient[i] = (float)((long double)ambient[i] + weight * ambientBytes[i]);
        for (i=0; i<3; ++i)
            directed[i] = (float)((long double)directed[i] + weight * directedBytes[i]);

        longitude = (unsigned)sample[7] * 4u;
        latitude = (unsigned)sample[6] * 4u;
        xDirection = (long double)grid->sine[(longitude+256u)&1023u] * grid->sine[latitude];
        yDirection = (long double)grid->sine[longitude] * grid->sine[latitude];
        zDirection = grid->sine[(latitude+256u)&1023u];
#if defined(_WIN32)
        /* Only this sine product has an intermediate float store on Windows. */
        yDirection = (float)yDirection;
        normal[0] += xDirection * weight;
        normal[1] += yDirection * weight;
        normal[2] += zDirection * weight;
#else
        /* Linux publishes each direction sum to float after every sample. */
        normalFloat[0] = (float)(xDirection * weight + normalFloat[0]);
        normalFloat[1] = (float)(yDirection * weight + normalFloat[1]);
        normalFloat[2] = (float)(weight * zDirection + normalFloat[2]);
#endif
    }
#if defined(_WIN32)
    normalFloat[2] = (float)normal[2];
    normalFloat[1] = (float)normal[1];
    normalFloat[0] = (float)normal[0];
#endif
    /* The native cutoff is binary64 0.99, not an extended decimal literal. */
    if (total > 0.0L && total < (long double)0.99) {
        inverse = 1.0L / total;
        for (i=0; i<3; ++i) ambient[i] = (float)(inverse * ambient[i]);
        for (i=0; i<3; ++i) directed[i] = (float)(inverse * directed[i]);
    }
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

/* Own directed-light consumer: Windows 30068200 / Linux 000b4c36.
 * Preserve each build's visible float stores and retained clamp inputs. */
void TCE_CG_LightForParticleDirected(const tce_lightGrid_t *grid,
                                   const vec3_t point,const vec3_t normal,vec3_t color) {
    vec3_t directed, direction;
    const float lightUnit = 1.0f / 255.0f;
    long double incidence, red, green, blue;

    TCE_CG_LightForPoint(grid, point, color, directed, direction);
#if defined(_WIN32)
    incidence = ((long double)direction[2] * normal[2] +
                 (long double)direction[1] * normal[1]) +
                 (long double)direction[0] * normal[0];
#else
    incidence = ((long double)direction[0] * normal[0] +
                 (long double)direction[1] * normal[1]) +
                 (long double)direction[2] * normal[2];
#endif
    /* Both originals skip the directed contribution for zero, negative or NaN. */
    if (incidence > 0.0L) {
#if defined(_WIN32)
        long double illumination = sqrtl(incidence);
        int channel;
        for (channel = 0; channel < 3; ++channel)
            color[channel] = (float)((long double)directed[channel] * illumination + color[channel]);
#else
        /* Linux evaluates the root separately for each RGB contribution. */
        color[0] = (float)((long double)color[0] + sqrtl(incidence) * directed[0]);
        color[1] = (float)((long double)directed[1] * sqrtl(incidence) + color[1]);
        color[2] = (float)((long double)color[2] + sqrtl(incidence) * directed[2]);
#endif
    }

#if defined(_WIN32)
    red = (long double)color[0] * lightUnit;
    color[0] = (float)red;
    green = (long double)color[1] * lightUnit;
    color[1] = (float)green;
    blue = (long double)color[2] * lightUnit;
    color[2] = (float)blue;
    /* Red/blue compare their stored float values; green stays extended. */
    red = (float)red;
    blue = (float)blue;
#else
    green = (long double)color[1] * lightUnit;
    red = (long double)color[0] * lightUnit;
    blue = (long double)color[2] * lightUnit;
    color[1] = (float)green;
    color[0] = (float)red;
    color[2] = (float)blue;
    /* Linux retains red for its clamp, then reloads stored green/blue. */
    green = (float)green;
    blue = (float)blue;
#endif
    if (red >= 1.0L) color[0] = 1.0f;
    if (green >= 1.0L) color[1] = 1.0f;
    if (blue >= 1.0L) color[2] = 1.0f;
}
