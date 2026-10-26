#ifndef TCE_EXPLOSIVE_PRECISION_H
#define TCE_EXPLOSIVE_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_interpolation_precision.h"

/* X stays held; Y/Z store both the difference and its half as Float.
 * A spilled infinity remains infinity through halving and finite addition. */
static __inline int TCE_ExplosiveCenterFinite(const float mins[3],
                                             const float maxs[3], float result[3])
{
	tceTriggerSum_t center[3], minimum[3], negativeMinimum;
	float stored[3];
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&center[axis], maxs[axis], 1.0f, control) ||
		    !TCE_TriggerProduct(&minimum[axis], mins[axis], 1.0f, control))
		{
			return 0;
		}
	}
	for (axis = 0; axis < 3; ++axis)
	{
		negativeMinimum = minimum[axis];
		negativeMinimum.negative ^= 1u;
		TCE_RotateAdd(&center[axis], &negativeMinimum, control);
		if (axis)
		{
			stored[axis] = TCE_RotateStore(center[axis], control);
			if (!TCE_TriggerProduct(&center[axis], stored[axis], 1.0f, control))
			{
				continue;
			}
		}
		TCE_InterpolationMultiply(&center[axis], 0x3f000000u, control); /* 0.5 */
		if (axis)
		{
			stored[axis] = TCE_RotateStore(center[axis], control);
			TCE_TriggerProduct(&center[axis], stored[axis], 1.0f, control);
		}
		TCE_RotateAdd(&center[axis], &minimum[axis], control);
		stored[axis] = TCE_RotateStore(center[axis], control);
	}
	result[1] = stored[1];
	result[0] = stored[0];
	result[2] = stored[2];
	return 1;
}

/* Native damage/radius arguments use FILD32 followed directly by FSTP32. */
static __inline float TCE_ExplosiveIntegerArgument(int integer)
{
	tceTriggerSum_t value = { { 0 }, 0 };
	unsigned control, magnitude;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return (float)integer;
	}
	magnitude = integer < 0 ? 0u - (unsigned)integer : (unsigned)integer;
	value.negative = integer < 0;
	TCE_TriggerPlace(&value, magnitude, 298);
	return TCE_RotateStore(value, control);
}
#endif
#endif
