#include "q_shared.h"
#include "bg_public.h"

/* TC cgame 30003e00: each iteration reloads the stored fraction, but its
 * loop condition uses the unrounded result still held in x87. */
qboolean BG_TraverseSpline(float *deltaTime, splinePath_t **pSpline) {
    double fraction = *deltaTime;
    while (fraction > 1) {
        splinePath_t *next;
        double distance;
        fraction = (double)*deltaTime - 1;
        *deltaTime = (float)fraction;
        distance = fraction * (*pSpline)->length;
        next = (*pSpline)->next;
        if (!next || !next->length) return qfalse;
        *pSpline = next;
        fraction = distance / next->length;
        *deltaTime = (float)fraction;
    }
    fraction = *deltaTime;
    while (fraction < 0) {
        splinePath_t *previous = (*pSpline)->prev;
        double distance = -((double)(*pSpline)->length * *deltaTime);
        if (!previous || !previous->length) return qfalse;
        *pSpline = previous;
        fraction = 1 - distance / previous->length;
        *deltaTime = (float)fraction;
    }
    return qtrue;
}
