#ifndef TCE_EYE_PRECISION_H
#define TCE_EYE_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_rotate_precision.h"

/* FIADD rounds the sum, not its signed32 integer operand. Keep that eye
 * height unspilled while storing X/Y, then subtract it from target Z.
 * As with the shared arithmetic helpers, exception flags are not reproduced. */
static __inline int TCE_FiniteEyeDirection(const float *origin, int height,
                                           const float *target, float *direction)
{
	tceTriggerSum_t eye, integer = { { 0 }, 0 }, z;
	unsigned control, magnitude;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_TriggerProduct(&eye, origin[2], 1.0f, control))
	{
		return 0;
	}
	integer.negative = height < 0;
	magnitude = integer.negative ? 0u - (unsigned)height : (unsigned)height;
	TCE_TriggerPlace(&integer, magnitude, 298);
	TCE_RotateAdd(&eye, &integer, control);
	for (axis = 0; axis < 2; ++axis)
	{
		if (!TCE_FiniteDifferenceStore(target[axis], origin[axis], &direction[axis]))
		{
			return 0;
		}
	}
	if (!TCE_TriggerProduct(&z, target[2], 1.0f, control))
	{
		return 0;
	}
	eye.negative ^= 1u;
	TCE_RotateAdd(&z, &eye, control);
	direction[2] = TCE_RotateStore(z, control);
	return 1;
}
#endif
#endif
