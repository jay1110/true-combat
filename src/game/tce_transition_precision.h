#ifndef TCE_TRANSITION_PRECISION_H
#define TCE_TRANSITION_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_ratio_precision.h"

typedef struct
{
	tceTriggerSum_t value;
	long double fallback;
	int finite;
} tceTransitionDifference_t;

/* Divide the signed result of native ABS by a finite, nonzero Float.
 * Preserve division precision before rounding the destination Float. */
static __inline int TCE_TransitionDivide32(int numerator, float denominator,
                                          float *result)
{
	unsigned bits, exponent, mantissa, magnitude, negative, control;
	int scale;

	memcpy(&bits, &denominator, sizeof(bits));
	exponent = (bits >> 23) & 255u;
	if (exponent == 255u || !(bits & 0x7fffffffu))
	{
		return 0;
	}
	mantissa = (bits & 0x7fffffu) | (exponent ? 0x800000u : 0u);
	scale = exponent ? 150 - (int)exponent : 149;
	negative = (numerator < 0) ^ (bits >> 31);
	magnitude = numerator < 0 ? 0u - (unsigned)numerator : (unsigned)numerator;
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	*result = TCE_RatioFloat32(magnitude, mantissa, scale, negative, control);
	return 1;
}

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
	return TCE_FixedSigned64Low32(value);
}
#endif
#endif
