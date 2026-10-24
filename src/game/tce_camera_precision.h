#ifndef TCE_CAMERA_PRECISION_H
#define TCE_CAMERA_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_ratio_precision.h"

/* Camera fields: FILD/FMUL[/FIDIV], binary64 floor argument, floor,
 * optional retained add-one, signed64 truncation and low32 packing.
 * This models values, not the CRT floor handler's exception side effects. */
static __inline int TCE_FiniteCameraQuantize(int input, unsigned factorBits,
                                             int divisor, int addOne, int divide,
                                             int *result)
{
	tceTriggerSum_t value = { { 0 }, 0 }, one = { { 0 }, 0 };
	unsigned exponent, mantissa, magnitude, control, fraction = 0;
	unsigned denominator, negative;
	unsigned long long numerator;
	int shift, word, bit;

	/* Only the charge field divides; its scale is exactly fifteen. */
	if (divide && (!divisor || factorBits != 0x41700000u))
	{
		return 0;
	}

	exponent = (factorBits >> 23) & 255u;
	if (exponent == 255u)
	{
		return 0;
	}
	mantissa = (factorBits & 0x7fffffu) | (exponent ? 0x800000u : 0u);
	magnitude = input < 0 ? 0u - (unsigned)input : (unsigned)input;
	value.negative = (input < 0) ^ (factorBits >> 31);
	shift = (int)(exponent ? exponent - 1 : 0) + 149;
	TCE_TriggerPlace(&value, (unsigned long long)magnitude * mantissa, shift);
	__control87_2(0, 0, &control, NULL);
	TCE_TriggerRound(&value, control);
	if (divide)
	{
		/* The rounded int32*15 numerator is integral and smaller than 2^35. */
		numerator = 0;
		for (bit = 0; bit < 35; ++bit)
		{
			numerator |= (unsigned long long)TCE_TriggerBit(&value, 298 + bit) << bit;
		}
		negative = value.negative ^ (divisor < 0);
		denominator = divisor < 0 ? 0u - (unsigned)divisor : (unsigned)divisor;
		value = TCE_RatioValue(numerator, denominator, 0, negative, control);
	}
	/* All finite Float*int32 products fit normal binary64's exponent range. */
	TCE_TriggerRound(&value, (control & ~_MCW_PC) | _PC_53);
	for (word = 0; word < 9; ++word)
	{
		fraction |= value.word[word];
		value.word[word] = 0;
	}
	fraction |= value.word[9] & 0x3ffu;
	value.word[9] &= ~0x3ffu;
	TCE_TriggerPlace(&one, 1u, 298);
	if (value.negative && fraction)
	{
		one.negative = 1;
		TCE_TriggerAddExact(&value, &one);
	}
	if (addOne)
	{
		one.negative = 0;
		TCE_RotateAdd(&value, &one, control);
	}
	*result = TCE_FixedSigned64Low32(&value);
	return 1;
}
#endif
#endif
