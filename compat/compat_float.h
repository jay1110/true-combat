#ifndef TCE_COMPAT_FLOAT_H
#define TCE_COMPAT_FLOAT_H

/* Class B: original numeric conversions and Float classification.
 * Existing per-platform profile selection is preserved, not generalized to
 * new architectures. These helpers do not reproduce FPU flags or traps.
 * Class C capability guards remain in the implementation until validated. */
#include "legacy/game/tce_numeric.h"
#include "legacy/game/tce_float_integer.h"

#endif
