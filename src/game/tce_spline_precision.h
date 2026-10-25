#ifndef TCE_SPLINE_PRECISION_H
#define TCE_SPLINE_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_interpolation_precision.h"

/* The Windows spline reduction retains its X difference but stores/reloads
 * Y/Z differences as Float before multiplication. No exception flags are
 * synthesized; unsupported operands leave the original FP path in control. */
static __inline int TCE_SplineFinitePoint(volatile float *current,
                                          const float *next, float tension)
{
	tceTriggerSum_t differences[3], starts[3], negativeStart;
	float results[3], rounded;
	unsigned control, factorBits, bits;
	int axis, infinity[3] = { 0, 0, 0 };

	memcpy(&factorBits, &tension, sizeof(factorBits));
	__control87_2(0, 0, &control, NULL);
	if ((factorBits & 0x7f800000u) == 0x7f800000u ||
	    !TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&differences[axis], next[axis], 1.0f, control) ||
		    !TCE_TriggerProduct(&starts[axis], current[axis], 1.0f, control))
		{
			return 0;
		}
		negativeStart = starts[axis];
		negativeStart.negative ^= 1u;
		TCE_RotateAdd(&differences[axis], &negativeStart, control);
		if (axis)
		{
			rounded = TCE_RotateStore(differences[axis], control);
			if (!TCE_TriggerProduct(&differences[axis], rounded, 1.0f, control))
			{
				/* A finite difference can overflow its Float store. A zero
				 * factor then needs the original invalid-operation/NaN path. */
				if (!(factorBits & 0x7fffffffu))
				{
					return 0;
				}
				memcpy(&bits, &rounded, sizeof(bits));
				bits ^= factorBits & 0x80000000u;
				memcpy(&results[axis], &bits, sizeof(bits));
				infinity[axis] = 1;
			}
		}
	}
	/* Differences have quantum >= 2^-149. Their exact Float products fit
	 * the existing 2^-298 grid and have magnitude < 2^257. */
	for (axis = 0; axis < 3; ++axis)
	{
		if (!infinity[axis])
		{
			TCE_InterpolationMultiply(&differences[axis], factorBits, control);
			TCE_RotateAdd(&differences[axis], &starts[axis], control);
			results[axis] = TCE_RotateStore(differences[axis], control);
		}
	}
	/* No destination changes until every axis is supported. The point rows
	 * are distinct entries in the caller's local control-polygon array. */
	for (axis = 0; axis < 3; ++axis)
	{
		current[axis] = results[axis];
	}
	return 1;
}
#endif
#endif
