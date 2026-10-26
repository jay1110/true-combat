#ifndef TCE_COMPAT_MATH_H
#define TCE_COMPAT_MATH_H

/* Class B finite legacy arithmetic; Class C controls implementation support.
 * Domain adapters remain separately included so that game types, random state
 * and callbacks do not become dependencies of this common arithmetic layer.
 * The MSVC/x86 fixed-grid implementation keeps its original guards. Other
 * targets retain the existing caller fallbacks; no new parity is implied. */
#include "compat_float.h"
#include "legacy/game/tce_trigger_precision.h"
#include "legacy/game/tce_rotate_precision.h"
#include "legacy/game/tce_ratio_precision.h"
#include "legacy/game/tce_length_precision.h"
#include "legacy/game/tce_gust_precision.h"
#include "legacy/game/tce_normalize_precision.h"

#endif
