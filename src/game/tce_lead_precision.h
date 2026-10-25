#ifndef TCE_LEAD_PRECISION_H
#define TCE_LEAD_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_interpolation_precision.h"

/* Keep the Windows X+Z+Y reflection dot through the three component stores.
 * Restrict inputs so every intermediate fits the fixed-point representation.
 * Exception flags are not reproduced; unmasked exceptions use the fallback. */
static __inline int TCE_FiniteLeadReflection(const float normal[3],
                                            const float forward[3],
                                            float result[3])
{
	tceTriggerSum_t dot, product, component, start;
	unsigned normalBits[3], forwardBits[3], magnitude, control;
	float stored[3];
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	for (axis = 0; axis < 3; ++axis)
	{
		memcpy(&normalBits[axis], &normal[axis], sizeof(unsigned));
		memcpy(&forwardBits[axis], &forward[axis], sizeof(unsigned));
		magnitude = normalBits[axis] & 0x7fffffffu;
		if (magnitude && (magnitude < 0x2f800000u || magnitude > 0x4f800000u))
		{
			return 0;
		}
		magnitude = forwardBits[axis] & 0x7fffffffu;
		if (magnitude && (magnitude < 0x2f800000u || magnitude > 0x4f800000u))
		{
			return 0;
		}
	}
	TCE_TriggerProduct(&dot, normal[0], forward[0], control);
	TCE_TriggerProduct(&product, normal[2], forward[2], control);
	TCE_RotateAdd(&dot, &product, control);
	TCE_TriggerProduct(&product, normal[1], forward[1], control);
	TCE_RotateAdd(&dot, &product, control);
	TCE_InterpolationMultiply(&dot, 0xc0000000u, control); /* -2.0f */
	for (axis = 0; axis < 3; ++axis)
	{
		component = dot;
		TCE_InterpolationMultiply(&component, normalBits[axis], control);
		TCE_TriggerProduct(&start, forward[axis], 1.0f, control);
		TCE_RotateAdd(&component, &start, control);
		stored[axis] = TCE_RotateStore(component, control);
	}
	memcpy(result, stored, sizeof(stored));
	return 1;
}
#endif
#endif
