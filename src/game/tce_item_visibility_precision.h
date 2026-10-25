#ifndef TCE_ITEM_VISIBILITY_PRECISION_H
#define TCE_ITEM_VISIBILITY_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_gust_precision.h"

/* The native item cone compares retained Z+Y+X products against
 * -0.94f - (distance * (1.0f / 255.0f)) * 0.057f, without Float spills.
 * Nonfinite operands and unmasked exceptions keep the caller's fallback.
 * Masked floating-point status flags are not reproduced. */
static __inline int TCE_ItemVisibilityCone(const float *facing,
                                          const float *direction,
                                          float distance, int *visible)
{
	tceTriggerSum_t dot, term, threshold, slope;
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&dot, facing[2], direction[2], control))
	{
		return 0;
	}
	for (axis = 1; axis >= 0; --axis)
	{
		if (!TCE_TriggerProduct(&term, facing[axis], direction[axis], control))
		{
			return 0;
		}
		TCE_RotateAdd(&dot, &term, control);
	}
	if (!TCE_TriggerProduct(&term, distance, 1.0f / 255.0f, control))
	{
		return 0;
	}
	TCE_TriggerProduct(&slope, 0.057f, 1.0f, control);
	/* The reciprocal has quantum 2^-31 and slope has quantum 2^-28.
	 * Even a subnormal Float distance leaves the triple product on the
	 * fixed 2^-298 grid; its magnitude is below 2^117. */
	TCE_GustMultiply(&term, &slope, control);
	term.negative ^= 1u;
	TCE_TriggerProduct(&threshold, -0.94f, 1.0f, control);
	TCE_RotateAdd(&threshold, &term, control);
	*visible = TCE_TriggerCompare(&threshold, &dot) >= 0;
	return 1;
}
#endif
#endif
