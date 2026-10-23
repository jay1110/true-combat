#include "cg_local.h"
#include "../game/tce_trajectory.h"
#if defined(__linux__) && !defined(Q3_VM)
#include "../game/tce_numeric.h"
#endif

/* Windows CG_ReflectVelocity 30044f20. TC trajectory IDs and gravity must
 * remain intact; the original sixth argument is 1.0f. */
void CG_ReflectVelocity(localEntity_t *le, trace_t *trace)
{
#if defined(__linux__) && !defined(Q3_VM)
	vec3_t velocity;
	long double dot, reflected[3], vertical;
	float bounce;
	int i;
	int frameStart = TCE_SubInt32(cg.time, cg.frametime);
	int hitTime = TCE_TruncateNativeInteger((long double)cg.frametime * trace->fraction + frameStart);

	TCE_BG_EvaluateTrajectoryDelta(&le->pos, hitTime, velocity, qfalse, -1, 1.0f);
	dot = (long double)velocity[1] * trace->plane.normal[1]
	      + (long double)velocity[0] * trace->plane.normal[0];
	dot += (long double)velocity[2] * trace->plane.normal[2];
	for (i = 0; i < 3; ++i)
	{
		reflected[i] = ((long double)trace->plane.normal[i] * dot) * -2.0f + velocity[i];
		if (i < 2)
		{
			/* Native publishes X/Y but retains their unrounded values. */
			le->pos.trDelta[i] = (float)reflected[i];
		}
	}
	bounce = le->bounceFactor;
	for (i = 0; i < 3; ++i)
	{
		reflected[i] *= bounce;
		le->pos.trDelta[i] = (float)reflected[i];
	}
	vertical = reflected[2];
	VectorCopy(trace->endpos, le->pos.trBase);
	le->pos.trTime = cg.time;
	if (le->leMarkType == LEMT_BLOOD && trace->startsolid)
	{
		return;
	}
	if (trace->allsolid || (trace->plane.normal[2] > 0.0f &&
	                        (!(vertical >= 40.0L) ||
	                         (long double)TCE_SubInt32(0, cg.frametime) * vertical > vertical)))
	{
		le->pos.trType = le->leType == LE_FRAGMENT && trace->entityNum < ENTITYNUM_WORLD
		    ? TR_GRAVITY_PAUSED : TR_STATIONARY;
	}
#else
	vec3_t velocity;
	double reflection;
	int    hitTime, i;
	int    frameStart = (int)((unsigned)cg.time - (unsigned)cg.frametime);
	hitTime = (int)(frameStart + (double)cg.frametime * trace->fraction);
	TCE_BG_EvaluateTrajectoryDelta(&le->pos, hitTime, velocity, qfalse, -1, 1.0f);
	/* Original sums X, Z, Y without rounding the dot product to float. */
	reflection = -2.0 * ((double)velocity[0] * trace->plane.normal[0]
	                     + (double)velocity[2] * trace->plane.normal[2]
	                     + (double)velocity[1] * trace->plane.normal[1]);
	for (i = 0; i < 3; ++i)
	{
		le->pos.trDelta[i]  = (float)(reflection * trace->plane.normal[i] + velocity[i]);
		le->pos.trDelta[i] *= le->bounceFactor;
	}
	VectorCopy(trace->endpos, le->pos.trBase);
	le->pos.trTime = cg.time;
	if (le->leMarkType == LEMT_BLOOD && trace->startsolid)
	{
		return;
	}
	if (trace->allsolid || (trace->plane.normal[2] > 0 &&
	                        (le->pos.trDelta[2] < 40 || le->pos.trDelta[2] <
	                         (double)(int)(0u - (unsigned)cg.frametime) * le->pos.trDelta[2])))
	{
		le->pos.trType = le->leType == LE_FRAGMENT && trace->entityNum < ENTITYNUM_WORLD
		    ? TR_GRAVITY_PAUSED : TR_STATIONARY;
	}
#endif
}
