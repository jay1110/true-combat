#ifndef TCE_POSE_PRECISION_H
#define TCE_POSE_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_interpolation_precision.h"

/* Windows pose interpolation retains 1-frac across all three components.
 * Keep both rounded products instead of rewriting as older+frac*(newer-older).
 * The pose-history callers supply distinct source and destination fields.
 * Floating exception flags and nonfinite operands remain separate. */
static __inline int TCE_FinitePose(float fraction, const float *newer,
                                   const float *older, float *output)
{
	tceTriggerSum_t inverse = { { 0 }, 0 }, value, sum;
	unsigned bits, control;
	int axis;

	memcpy(&bits, &fraction, sizeof(bits));
	if (((bits >> 23) & 255u) == 255u)
	{
		return 0;
	}
	/* Reject before writing any destination component. */
	for (axis = 0; axis < 3; ++axis)
	{
		memcpy(&bits, &newer[axis], sizeof(bits));
		if (((bits >> 23) & 255u) == 255u)
		{
			return 0;
		}
		memcpy(&bits, &older[axis], sizeof(bits));
		if (((bits >> 23) & 255u) == 255u)
		{
			return 0;
		}
	}
	__control87_2(0, 0, &control, NULL);
	TCE_TriggerPlace(&inverse, 1u, 298);
	TCE_TriggerProduct(&value, fraction, 1.0f, control);
	value.negative ^= 1u;
	TCE_RotateAdd(&inverse, &value, control);
	for (axis = 0; axis < 3; ++axis)
	{
		memcpy(&bits, &older[axis], sizeof(bits));
		if (axis == 0)
		{
			TCE_TriggerProduct(&sum, fraction, newer[axis], control);
			value = inverse;
			TCE_InterpolationMultiply(&value, bits, control);
		}
		else
		{
			sum = inverse;
			TCE_InterpolationMultiply(&sum, bits, control);
			TCE_TriggerProduct(&value, fraction, newer[axis], control);
		}
		TCE_RotateAdd(&sum, &value, control);
		output[axis] = TCE_RotateStore(sum, control);
	}
	return 1;
}
#endif
#endif
