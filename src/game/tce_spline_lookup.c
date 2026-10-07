#include "q_shared.h"
#include "bg_public.h"

/* TC cgame 30005c30: signed, one-based path number; sign selects direction.
 * Unsigned arithmetic preserves x86 NEG/DEC even for INT_MIN. */
splinePath_t *BG_GetSplineData(int number, qboolean *backwards) {
    unsigned magnitude = (unsigned)number;
    int index;
    *backwards = number < 0 ? qtrue : qfalse;
    if (number < 0) magnitude = 0u - magnitude;
    index = (int)(magnitude - 1u);
    if (index < 0 || index >= numSplinePaths) return NULL;
    return &splinePaths[index];
}
