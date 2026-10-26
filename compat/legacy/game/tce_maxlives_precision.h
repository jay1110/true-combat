#ifndef TCE_MAXLIVES_PRECISION_H
#define TCE_MAXLIVES_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_ratio_precision.h"

/* Keep denominator, quotient, scaled lives and fraction wide. The original
 * compares against the signed32 low word returned by signed64 truncation.
 * Zero/nonfinite limits retain the caller's existing exceptional path. */
static __inline int TCE_MaxLivesFinite(int elapsed, int remaining, float limit,
                                      int *result)
{
	tceTriggerSum_t denominator, value;
	tceTriggerSum_t one = { { 0 }, 0 }, integer = { { 0 }, 0 };
	tceTriggerSum_t half = { { 0 }, 0 };
	unsigned bits, control, magnitude, negative;
	unsigned long long divisor = 0, wide, carry = 0;
	int low, bit, word, converted;

	memcpy(&bits, &limit, sizeof(bits));
	if (((bits >> 23) & 255u) == 255u || !(bits & 0x7fffffffu))
	{
		return 0;
	}
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	TCE_TriggerProduct(&denominator, limit, 60000.0f, control);
	/* A nonzero Float times 60000 cannot underflow the extended format.
	 * Its rounded significand needs at most 35 bits (40 raw product bits). */
	for (low = 0; !TCE_TriggerBit(&denominator, low); ++low)
	{
	}
	for (bit = 0; bit < 40; ++bit)
	{
		divisor |= (unsigned long long)TCE_TriggerBit(&denominator, low + bit) << bit;
	}
	negative = (elapsed < 0) ^ denominator.negative;
	magnitude = elapsed < 0 ? 0u - (unsigned)elapsed : (unsigned)elapsed;
	value = TCE_RatioValue(magnitude, divisor, 298 - low, negative, control);
	TCE_TriggerPlace(&one, 1u, 298);
	value.negative ^= 1u;
	TCE_RotateAdd(&value, &one, control);

	magnitude = remaining < 0 ? 0u - (unsigned)remaining : (unsigned)remaining;
	value.negative ^= remaining < 0;
	for (word = 0; word < 18; ++word)
	{
		wide = (unsigned long long)value.word[word] * magnitude + carry;
		value.word[word] = (unsigned)wide;
		carry = wide >> 32;
	}
	TCE_TriggerRound(&value, control);
	converted = TCE_FixedSigned64Low32(&value);

	integer.negative = converted >= 0;
	magnitude = converted < 0 ? 0u - (unsigned)converted : (unsigned)converted;
	TCE_TriggerPlace(&integer, magnitude, 298);
	TCE_RotateAdd(&value, &integer, control);
	TCE_TriggerPlace(&half, 1u, 297);
	if (TCE_TriggerCompare(&value, &half) >= 0)
	{
		bits = (unsigned)converted + 1u;
		memcpy(&converted, &bits, sizeof(converted));
	}
	*result = converted;
	return 1;
}
#endif
#endif
