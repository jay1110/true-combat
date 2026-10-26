#ifndef TCE_GUST_PRECISION_H
#define TCE_GUST_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_ratio_precision.h"
#include "tce_numeric.h"

/* Gust operands have no product bits below the fixed 2^-298 unit.
 * The retained fraction has at most 64 significant bits and magnitude <2^32;
 * its lowest possible bit is 2^-94. Float differences are multiples of 2^-149.
 * Their product fits the existing 576-bit representation without clipping. */
static __inline void TCE_GustMultiply(tceTriggerSum_t *value,
                                     const tceTriggerSum_t *factor,
                                     unsigned control)
{
	unsigned product[36] = { 0 };
	unsigned long long carry, wide;
	int i, j;

	for (i = 0; i < 18; ++i)
	{
		carry = 0;
		for (j = 0; j < 18; ++j)
		{
			wide = (unsigned long long)value->word[i] * factor->word[j] +
			       product[i + j] + carry;
			product[i + j] = (unsigned)wide;
			carry = wide >> 32;
		}
		product[i + 18] = (unsigned)carry;
	}
	for (i = 0; i < 18; ++i)
	{
		value->word[i] = (product[i + 9] >> 10) | (product[i + 10] << 22);
	}
	value->negative ^= factor->negative;
	TCE_TriggerRound(value, control);
}

static __inline int TCE_FiniteGust(int elapsed, int duration, int fading,
                                   const float *base, const float *gust,
                                   float baseWeight, float gustWeight,
                                   int baseDrops, int gustDrops,
                                   float *current, float *weight, int *drops)
{
	tceTriggerSum_t fraction, one = { { 0 }, 0 }, start, value, differences[3];
	tceTriggerSum_t weightDifference, weightStart;
	unsigned control, n, d, magnitude;
	float delta;
	int i, difference;

	__control87_2(0, 0, &control, NULL);
	if (!duration || !TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	/* Reject unsupported values before any caller-owned output is written. */
	for (i = 0; i < 3; ++i)
	{
		if (!TCE_TriggerProduct(&differences[i], gust[i], 1.0f, control) ||
		    !TCE_TriggerProduct(&start, base[i], 1.0f, control))
		{
			return 0;
		}
		start.negative ^= 1u;
		TCE_RotateAdd(&differences[i], &start, control);
		delta = TCE_RotateStore(differences[i], control);
		if (!TCE_TriggerProduct(&differences[i], delta, 1.0f, control))
		{
			return 0;
		}
	}
	if (!TCE_TriggerProduct(&weightDifference, gustWeight, 1.0f, control) ||
	    !TCE_TriggerProduct(&weightStart, baseWeight, 1.0f, control))
	{
		return 0;
	}
	weightStart.negative ^= 1u;
	TCE_RotateAdd(&weightDifference, &weightStart, control);
	weightStart.negative ^= 1u;
	n = elapsed < 0 ? 0u - (unsigned)elapsed : (unsigned)elapsed;
	d = duration < 0 ? 0u - (unsigned)duration : (unsigned)duration;
	fraction = TCE_RatioValue(n, d, 0, (elapsed < 0) ^ (duration < 0), control);
	if (fading)
	{
		TCE_TriggerPlace(&one, 1u, 298);
		fraction.negative ^= 1u;
		TCE_RotateAdd(&one, &fraction, control);
		fraction = one;
	}
	for (i = 0; i < 3; ++i)
	{
		value = differences[i];
		TCE_GustMultiply(&value, &fraction, control);
		TCE_TriggerProduct(&start, base[i], 1.0f, control);
		TCE_RotateAdd(&value, &start, control);
		current[i] = TCE_RotateStore(value, control);
	}
	TCE_GustMultiply(&weightDifference, &fraction, control);
	TCE_RotateAdd(&weightDifference, &weightStart, control);
	*weight = TCE_RotateStore(weightDifference, control);
	difference = TCE_SubInt32(gustDrops, baseDrops);
	memset(&value, 0, sizeof(value));
	value.negative = difference < 0;
	magnitude = difference < 0 ? 0u - (unsigned)difference : (unsigned)difference;
	TCE_TriggerPlace(&value, magnitude, 298);
	TCE_GustMultiply(&value, &fraction, control);
	memset(&start, 0, sizeof(start));
	start.negative = baseDrops < 0;
	magnitude = baseDrops < 0 ? 0u - (unsigned)baseDrops : (unsigned)baseDrops;
	TCE_TriggerPlace(&start, magnitude, 298);
	TCE_RotateAdd(&value, &start, control);
	*drops = TCE_FixedSigned64Low32(&value);
	return 1;
}
#endif
#endif
