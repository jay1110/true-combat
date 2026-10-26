#ifndef TCE_TEAM_SPAWN_PRECISION_H
#define TCE_TEAM_SPAWN_PRECISION_H

#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_length_precision.h"
#include "tce_gust_precision.h"

/* Retain the Windows FSQRT and group penalty until the score comparison.
 * The first candidate instead stores Float after its root and 16000 penalty.
 * Nonfinite operands and unmasked exceptions leave the scalar caller intact.
 * This models values, not the processor's floating exception status flags. */
static __inline int TCE_TeamSpawnFiniteScore(const float *delta, int groupDelta,
                                             int hasGroup, int ungroupedPenalty,
                                             int initial, float threshold,
                                             int *closer, float *stored)
{
	tceTriggerSum_t score, term, root, group = { { 0 }, 0 }, boundary;
	unsigned control, magnitude;
	float snapshot;
	int axis, selected;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&score, delta[0], delta[0], control))
	{
		return 0;
	}
	for (axis = 1; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&term, delta[axis], delta[axis], control))
		{
			return 0;
		}
		TCE_RotateAdd(&score, &term, control);
	}
	root = TCE_LengthRoot(&score, control);
	score = root;
	if (initial)
	{
		snapshot = TCE_RotateStore(score, control);
		if (!TCE_TriggerProduct(&score, snapshot, 1.0f, control))
		{
			return 0;
		}
	}
	if (ungroupedPenalty)
	{
		TCE_TriggerProduct(&term, 16000.0f, 1.0f, control);
		TCE_RotateAdd(&score, &term, control);
		if (initial)
		{
			snapshot = TCE_RotateStore(score, control);
			if (!TCE_TriggerProduct(&score, snapshot, 1.0f, control))
			{
				return 0;
			}
		}
	}
	if (hasGroup && groupDelta)
	{
		/* FILD is exact; only the optional 0.5-delta operation rounds at PC. */
		magnitude = groupDelta < 0 ? 0u - (unsigned)groupDelta : (unsigned)groupDelta;
		TCE_TriggerPlace(&group, magnitude, 298);
		if (groupDelta < 0)
		{
			TCE_TriggerProduct(&term, 0.5f, 1.0f, control);
			TCE_RotateAdd(&group, &term, control);
		}
		/* Integer/half-integer times 2^11 fits the fixed grid exactly before
		 * PC rounding: no bits below 2^-298 or above its 576-bit capacity. */
		TCE_TriggerProduct(&term, 2048.0f, 1.0f, control);
		TCE_GustMultiply(&group, &term, control);
		TCE_RotateAdd(&score, &group, control);
	}
	selected = initial;
	if (!initial)
	{
		if (!TCE_TriggerProduct(&boundary, threshold, 1.0f, control))
		{
			return 0;
		}
		selected = TCE_TriggerCompare(&score, &boundary) < 0;
	}
	if (selected)
	{
		*stored = TCE_RotateStore(score, control);
	}
	*closer = selected;
	return 1;
}
#endif
#endif
