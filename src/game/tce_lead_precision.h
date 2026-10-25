#ifndef TCE_LEAD_PRECISION_H
#define TCE_LEAD_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_interpolation_precision.h"

static __inline int TCE_LeadFiniteRange(float value, unsigned maximum)
{
	unsigned bits;

	memcpy(&bits, &value, sizeof(bits));
	bits &= 0x7fffffffu;
	return !bits || (bits >= 0x2f800000u && bits <= maximum);
}

/* X stays held until its final store; Y/Z are stored after both the base
 * and horizontal sums. The vertical spread remains held across all axes. */
static __inline int TCE_FiniteLeadEndpoint(float horizontalRandom,
                                          float verticalRandom, float spread,
                                          const float muzzle[3],
                                          const float forward[3],
                                          const float right[3],
                                          const float up[3], float result[3])
{
	tceTriggerSum_t horizontal, vertical, base, start, product;
	unsigned upBits[3], control;
	float stored[3], horizontalSpread, intermediate;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_LeadFiniteRange(horizontalRandom, 0x3f800000u) ||
	    !TCE_LeadFiniteRange(verticalRandom, 0x3f800000u) ||
	    !TCE_LeadFiniteRange(spread, 0x4f800000u))
	{
		return 0;
	}
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_LeadFiniteRange(muzzle[axis], 0x4f800000u) ||
		    !TCE_LeadFiniteRange(forward[axis], 0x4f800000u) ||
		    !TCE_LeadFiniteRange(right[axis], 0x4f800000u) ||
		    !TCE_LeadFiniteRange(up[axis], 0x4f800000u))
		{
			return 0;
		}
		memcpy(&upBits[axis], &up[axis], sizeof(unsigned));
	}
	TCE_TriggerProduct(&horizontal, horizontalRandom, spread, control);
	horizontalSpread = TCE_RotateStore(horizontal, control);
	TCE_TriggerProduct(&vertical, verticalRandom, spread, control);
	for (axis = 0; axis < 3; ++axis)
	{
		TCE_TriggerProduct(&base, forward[axis], 8192.0f, control);
		TCE_TriggerProduct(&start, muzzle[axis], 1.0f, control);
		TCE_RotateAdd(&base, &start, control);
		if (axis)
		{
			intermediate = TCE_RotateStore(base, control);
			TCE_TriggerProduct(&base, intermediate, 1.0f, control);
		}
		TCE_TriggerProduct(&product, horizontalSpread, right[axis], control);
		TCE_RotateAdd(&base, &product, control);
		if (axis)
		{
			intermediate = TCE_RotateStore(base, control);
			TCE_TriggerProduct(&base, intermediate, 1.0f, control);
		}
		product = vertical;
		TCE_InterpolationMultiply(&product, upBits[axis], control);
		TCE_RotateAdd(&product, &base, control);
		stored[axis] = TCE_RotateStore(product, control);
	}
	memcpy(result, stored, sizeof(stored));
	return 1;
}

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
