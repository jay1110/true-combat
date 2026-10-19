#ifndef TCE_NAV_CAPSULE_TRACE_H
#define TCE_NAV_CAPSULE_TRACE_H

#include <float.h>

/* Included after g_local.h. Kept separate so the engine-query contract can be
 * exercised with a trace stub, without starting a second game instance. */
static void TCE_NavCapsuleTrace(trace_t *result, const vec3_t start,
                                const vec3_t mins, const vec3_t maxs, const vec3_t end,
                                int passEntityNum, int contentMask)
{
	int      axis;
	qboolean finite = qtrue;
	vec3_t   up, down;
	trace_t  reverse;

	for (axis = 0; axis < 3; ++axis)
	{
		if (!(start[axis] >= -FLT_MAX && start[axis] <= FLT_MAX) ||
		    !(end[axis] >= -FLT_MAX && end[axis] <= FLT_MAX) ||
		    !(mins[axis] >= -FLT_MAX && mins[axis] <= FLT_MAX) ||
		    !(maxs[axis] >= -FLT_MAX && maxs[axis] <= FLT_MAX) ||
		    mins[axis] > maxs[axis])
		{
			finite = qfalse;
		}
	}
	if (!finite)
	{
		memset(result, 0, sizeof(*result));
		result->startsolid = result->allsolid = qtrue;
		result->entityNum  = ENTITYNUM_WORLD;
		return;
	}
	if (!VectorCompare(start, end))
	{
		trap_TraceCapsule(result, start, mins, maxs, end,
		                  passEntityNum, contentMask);
		return;
	}

	/* Classic ET's stationary capsule query can report false occupancy on
	 * valid stairs. As in NG_Trace, require BOTH full-body sweeps to be clear.
	 * Both include the original point; no hull shrink or one-sided fallback. */
	VectorCopy(start, up);
	VectorCopy(start, down);
	up[2]   += .01f;
	down[2] -= .01f;
	trap_TraceCapsule(result, start, mins, maxs, up,
	                  passEntityNum, contentMask);
	trap_TraceCapsule(&reverse, start, mins, maxs, down,
	                  passEntityNum, contentMask);
	if (!result->startsolid && !result->allsolid && result->fraction == 1.f)
	{
		if (!reverse.startsolid && !reverse.allsolid && reverse.fraction == 1.f)
		{
			VectorCopy(start, result->endpos);
			return;
		}
		*result = reverse;
	}
	/* Omni's trace ABI has no allsolid field. Preserve the blocking trace's
	 * entity/contents, but expose every obstruction as occupied to callers. */
	result->startsolid = result->allsolid = qtrue;
	result->fraction   = 0.f;
	VectorCopy(start, result->endpos);
}

#endif
