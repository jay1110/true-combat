#ifndef TCE_NAV_RUNTIME_H
#define TCE_NAV_RUNTIME_H

/* Read-only exact button/relay binding, shared with the live bot gate. */
qboolean TCE_NavDoorUseMatches(int useEntity,int doorEntity,int doorModel);
/* Actual capsule clearance across the complete bounded door motion. */
qboolean TCE_NavDoorSweepSafe(int doorEntity,const vec3_t feet,float width,float height,float tolerance);
/* Standing player-origin clearance, without projecting onto another storey. */
const char *TCE_NavValidateSpawn(const vec3_t origin,vec3_t feet,int *traces);
/* 0 unsupported, 1 fixed ground, 2 presently solid destructible ground. */
int TCE_NavGroundKind(int entityNum);

#endif
