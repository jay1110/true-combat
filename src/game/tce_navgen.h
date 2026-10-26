#ifndef TCE_NAVGEN_H
#define TCE_NAVGEN_H

#include "tce_nav_runtime.h"

#ifdef FEATURE_NAVSTUDIO
/* Server console only. Writes staged navigation; never installs or replaces routes.
 * Authored Format5/6 may append TCE_NODE_METADATA 1 after all edges: source ID,
 * imported Omni UID (0=new, max2147483647), unit facing XYZ, UTF8 name as hex or -.
 * This preserves identity and labels without changing physical action recipes. */
/* Optional following TCE_NODE_PROPERTIES 1 tail preserves opaque PropertyMap
 * literals: source ID, UTF8 key/value hex (- for empty value). Reserved tce_ keys
 * are derived from native recipes. Limits32/node,4096pairs,1MiB decoded strings. */
qboolean TCE_NavgenCommand(void);
/* Temporary local devmap authoring hold, with an automatic ten-minute expiry. */
qboolean TCE_NavgenAuthoringActive(void);
#endif /* FEATURE_NAVSTUDIO */
#endif
