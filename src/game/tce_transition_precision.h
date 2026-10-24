#ifndef TCE_TRANSITION_PRECISION_H
#define TCE_TRANSITION_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_rotate_precision.h"

typedef struct
{
	tceTriggerSum_t value;
	long double fallback;
	int finite;
} tceTransitionDifference_t;

/* Hold the rounded subtraction across the original rectangle copies.
 * Only nonfinite inputs evaluate the previous floating-point fallback.
 * Exception flags and unmasked traps are not reproduced. */
static __inline tceTransitionDifference_t TCE_TransitionDifferenceValue(float to,
                                                                       float from)
{
	tceTransitionDifference_t result = { { { 0 }, 0 }, 0.0L, 0 };
	tceTriggerSum_t start;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_TriggerProduct(&result.value, to, 1.0f, control) ||
	    !TCE_TriggerProduct(&start, from, 1.0f, control))
	{
		result.fallback = (long double)to - (long double)from;
		return result;
	}
	start.negative ^= 1u;
	TCE_RotateAdd(&result.value, &start, control);
	result.finite = 1;
	return result;
}

/* The original __ftol conversion occurs after the rectangle copies. */
static __inline int TCE_TransitionValueLow32(const tceTriggerSum_t *value)
{
	unsigned low = 0, high = 0;
	int bit, result;

	for (bit = 362; bit < 576; ++bit)
	{
		if (TCE_TriggerBit(value, bit))
		{
			return 0;
		}
	}
	for (bit = 0; bit < 32; ++bit)
	{
		low |= TCE_TriggerBit(value, 298 + bit) << bit;
		high |= TCE_TriggerBit(value, 330 + bit) << bit;
	}
	if (high > 0x80000000u ||
	    (high == 0x80000000u && (!value->negative || low)))
	{
		return 0;
	}
	if (value->negative)
	{
		low = 0u - low;
	}
	memcpy(&result, &low, sizeof(result));
	return result;
}
#endif
#endif
