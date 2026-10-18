#ifndef TCE_NAVGEN_H
#define TCE_NAVGEN_H

/* Read-only exact button/relay binding, shared with the live bot gate. */
qboolean TCE_NavDoorUseMatches(int useEntity,int doorEntity,int doorModel);
/* Exact real capsule clearance from the door's full bounded motion sweep.
 * Inputs are physical feet/hull dimensions; no actor or game time is moved. */
qboolean TCE_NavDoorSweepSafe(int doorEntity,const vec3_t feet,float width,float height,float tolerance);
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
/* Standing spawn clearance at the authored player origin; does not move it. */
const char *TCE_NavgenValidateSpawn(const vec3_t origin, vec3_t feet, int *traces);
/* 0 unsupported, 1 fixed ground, 2 presently solid destructible ground.
 * Shared by generation and each runtime navigation support trace. */
int TCE_NavGroundKind(int entityNum);
#endif
