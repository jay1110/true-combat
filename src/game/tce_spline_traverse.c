#include "q_shared.h"
#include "bg_public.h"

#if defined(_WIN32) || defined(__linux__)
#include "tce_numeric.h"

qboolean BG_TraverseSpline(float *deltaTime, splinePath_t **pSpline) {
    volatile float *storedFraction = deltaTime;
    long double fraction = *storedFraction;
    qboolean traverseBackwards;

    while (fraction > 1.0L) {
        splinePath_t *current, *next;
        long double remaining, distance;
#if defined(_WIN32)
        remaining = (long double)*storedFraction - 1.0L;
#else
        remaining = fraction - 1.0L;
#endif
        *storedFraction = (float)remaining;
        current = *pSpline;
        next = current->next;
        distance = remaining * current->length;
        if (!next || TCE_FloatIsZeroOrNaN(next->length)) return qfalse;
#if defined(__linux__)
        next = current->next;
#endif
        *pSpline = next;
#if defined(_WIN32)
        fraction = distance / next->length;
#else
        {
            long double reciprocal = 1.0L / next->length;
            fraction = distance * reciprocal;
        }
#endif
        *storedFraction = (float)fraction;
    }
#if defined(_WIN32)
    /* Windows reloads the rounded fraction and follows unordered backwards. */
    fraction = *storedFraction;
    traverseBackwards = !(fraction >= 0.0L);
#else
    /* The first Linux backwards gate is ordered; later gates include unordered. */
    traverseBackwards = fraction < 0.0L;
#endif
    if (traverseBackwards) {
        do {
            splinePath_t *current = *pSpline;
            splinePath_t *previous;
            long double distance;
#if defined(_WIN32)
            distance = (long double)current->length * *storedFraction;
            previous = current->prev;
#else
            previous = current->prev;
            distance = fraction * current->length;
#endif
            distance = -distance;
            if (!previous || TCE_FloatIsZeroOrNaN(previous->length)) return qfalse;
#if defined(__linux__)
            previous = current->prev;
#endif
            *pSpline = previous;
#if defined(_WIN32)
            fraction = distance / previous->length;
            fraction = 1.0L - fraction;
#else
            {
                long double reciprocal = 1.0L / previous->length;
                long double ratio = distance * reciprocal;
                fraction = 1.0L - ratio;
            }
#endif
            *storedFraction = (float)fraction;
        } while (!(fraction >= 0.0L));
    }
    return qtrue;
}
#else
#ifdef CGAMEDLL
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
#else
qboolean BG_TraverseSpline( float* deltaTime, splinePath_t** pSpline) {
	float dist;

	while( (*deltaTime) > 1 ) {
		(*deltaTime) -= 1;
		dist = (*pSpline)->length * (*deltaTime);

		if(!(*pSpline)->next || !(*pSpline)->next->length) {
			return qfalse;
//			Com_Error( ERR_DROP, "Spline path end passed (%s)", (*pSpline)->point.name );
		}

		(*pSpline) = (*pSpline)->next;
		*deltaTime = dist / (*pSpline)->length;
	}

	while( (*deltaTime) < 0 ) {
		dist = -((*pSpline)->length * (*deltaTime));

		if(!(*pSpline)->prev || !(*pSpline)->prev->length) {
			return qfalse;
//			Com_Error( ERR_DROP, "Spline path end passed (%s)", (*pSpline)->point.name );
		}

		(*pSpline) = (*pSpline)->prev;
		(*deltaTime) = 1 - (dist / (*pSpline)->length);
	}

	return qtrue;
}
#endif
#endif
