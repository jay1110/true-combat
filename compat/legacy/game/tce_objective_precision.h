#ifndef TCE_OBJECTIVE_PRECISION_H
#define TCE_OBJECTIVE_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_rotate_precision.h"

/* Compare the retained squared distance, storing a Float only for a winner.
 * VectorDistanceSquared stores its three differences before squaring them. */
static __inline int TCE_ObjectiveDistance(const float *from, const float *to,
                                         int first, float best, int *accepted,
                                         float *storedDistance)
{
	tceTriggerSum_t value, term, sum, bound;
	float delta[3];
	unsigned control, bits;
	int axis, choose;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&value, to[axis], 1.0f, control) ||
		    !TCE_TriggerProduct(&term, from[axis], -1.0f, control))
		{
			return 0;
		}
		TCE_RotateAdd(&value, &term, control);
		delta[axis] = TCE_RotateStore(value, control);
	}
	if (!TCE_TriggerProduct(&sum, delta[0], delta[0], control) ||
	    !TCE_TriggerProduct(&term, delta[1], delta[1], control))
	{
		return 0;
	}
	TCE_RotateAdd(&sum, &term, control);
	if (!TCE_TriggerProduct(&term, delta[2], delta[2], control))
	{
		return 0;
	}
	TCE_RotateAdd(&sum, &term, control);
	choose = first;
	if (!first)
	{
		memcpy(&bits, &best, sizeof(bits));
		if ((bits & 0x7f800000u) == 0x7f800000u)
		{
			/* C0 selects unordered and a finite sum is below positive infinity. */
			choose = (bits & 0x7fffffu) != 0 || !(bits & 0x80000000u);
		}
		else
		{
			TCE_TriggerProduct(&bound, best, 1.0f, control);
			choose = TCE_TriggerCompare(&sum, &bound) < 0;
		}
	}
	if (choose)
	{
		*storedDistance = TCE_RotateStore(sum, control);
	}
	*accepted = choose;
	return 1;
}
#endif
#endif
