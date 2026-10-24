#ifndef TCE_INTERPOLATION_PRECISION_H
#define TCE_INTERPOLATION_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_ratio_precision.h"

/* The caller establishes a nonzero wrapped duration before dividing. */
static __inline float TCE_InterpolationFactor32(int elapsed, int duration)
{
	unsigned n, d, negative, control;

	negative = (elapsed < 0) ^ (duration < 0);
	n = elapsed < 0 ? 0u - (unsigned)elapsed : (unsigned)elapsed;
	d = duration < 0 ? 0u - (unsigned)duration : (unsigned)duration;
	__control87_2(0, 0, &control, NULL);
	return TCE_RatioFloat32(n, d, 0, negative, control);
}

/* A rounded difference of two binary32 inputs is a multiple of 2^-149.
 * Multiplying its integer magnitude by a Float factor therefore loses no
 * bits when shifting right at most 149 places in the 2^-298 representation. */
static __inline void TCE_InterpolationMultiply(tceTriggerSum_t *value,
                                              unsigned factorBits,
                                              unsigned control)
{
	unsigned exponent = (factorBits >> 23) & 255u;
	unsigned mantissa = (factorBits & 0x7fffffu) | (exponent ? 0x800000u : 0u);
	unsigned long long wide, carry = 0;
	unsigned part, adjacent;
	int shift = exponent ? (int)exponent - 150 : -149;
	int word, words, bits;

	for (word = 0; word < 18; ++word)
	{
		wide = (unsigned long long)value->word[word] * mantissa + carry;
		value->word[word] = (unsigned)wide;
		carry = wide >> 32;
	}
	if (shift >= 0)
	{
		words = shift / 32;
		bits = shift % 32;
		for (word = 17; word >= 0; --word)
		{
			part = word >= words ? value->word[word - words] : 0;
			adjacent = bits && word > words ?
			           value->word[word - words - 1] >> (32 - bits) : 0;
			value->word[word] = (part << bits) | adjacent;
		}
	}
	else
	{
		words = -shift / 32;
		bits = -shift % 32;
		for (word = 0; word < 18; ++word)
		{
			part = word + words < 18 ? value->word[word + words] : 0;
			adjacent = bits && word + words + 1 < 18 ?
			           value->word[word + words + 1] << (32 - bits) : 0;
			value->word[word] = (part >> bits) | adjacent;
		}
	}
	value->negative ^= factorBits >> 31;
	TCE_TriggerRound(value, control);
}

static __inline int TCE_InterpolateFiniteFloat(float from, float to,
                                              float factor, float *result)
{
	tceTriggerSum_t value, start;
	unsigned control, factorBits;

	memcpy(&factorBits, &factor, sizeof(factorBits));
	if (((factorBits >> 23) & 255u) == 255u)
	{
		return 0;
	}
	__control87_2(0, 0, &control, NULL);
	if (!TCE_TriggerProduct(&value, to, 1.0f, control) ||
	    !TCE_TriggerProduct(&start, from, 1.0f, control))
	{
		return 0;
	}
	start.negative ^= 1u;
	TCE_RotateAdd(&value, &start, control);
	start.negative ^= 1u;
	TCE_InterpolationMultiply(&value, factorBits, control);
	TCE_RotateAdd(&value, &start, control);
	*result = TCE_RotateStore(value, control);
	return 1;
}

/* Windows LerpAngle retains the adjusted endpoint between its comparisons.
 * Its caller stores the returned value directly as binary32. */
static __inline int TCE_InterpolateFiniteAngle(float from, float to,
                                              float factor, float *result)
{
	tceTriggerSum_t endpoint, start, negativeStart, value;
	tceTriggerSum_t limit = { { 0 }, 0 }, turn = { { 0 }, 0 };
	unsigned control, factorBits;

	memcpy(&factorBits, &factor, sizeof(factorBits));
	if (((factorBits >> 23) & 255u) == 255u)
	{
		return 0;
	}
	__control87_2(0, 0, &control, NULL);
	if (!TCE_TriggerProduct(&endpoint, to, 1.0f, control) ||
	    !TCE_TriggerProduct(&start, from, 1.0f, control))
	{
		return 0;
	}
	negativeStart = start;
	negativeStart.negative ^= 1u;
	TCE_TriggerPlace(&limit, 180u, 298);
	TCE_TriggerPlace(&turn, 360u, 298);
	value = endpoint;
	TCE_RotateAdd(&value, &negativeStart, control);
	if (TCE_TriggerCompare(&value, &limit) > 0)
	{
		turn.negative = 1;
		TCE_RotateAdd(&endpoint, &turn, control);
	}
	value = endpoint;
	TCE_RotateAdd(&value, &negativeStart, control);
	limit.negative = 1;
	if (TCE_TriggerCompare(&value, &limit) < 0)
	{
		turn.negative = 0;
		TCE_RotateAdd(&endpoint, &turn, control);
	}
	value = endpoint;
	TCE_RotateAdd(&value, &negativeStart, control);
	TCE_InterpolationMultiply(&value, factorBits, control);
	TCE_RotateAdd(&value, &start, control);
	*result = TCE_RotateStore(value, control);
	return 1;
}

/* FILD difference; FMUL stored Float factor; FIADD previous; __ftol.
 * Keep arithmetic rounding separate from the final truncation to signed64.
 * Exception flags and unmasked traps are not reproduced. */
static __inline int TCE_InterpolateCycleLow32(int difference, float factor,
                                            int previous)
{
	tceTriggerSum_t value = { { 0 }, 0 }, start = { { 0 }, 0 };
	unsigned bits, exponent, mantissa, magnitude, control, low = 0, high = 0;
	int bit, result;

	memcpy(&bits, &factor, sizeof(bits));
	exponent = (bits >> 23) & 255u;
	if (exponent == 255u)
	{
		return 0; /* Masked signed64 integer-indefinite has low32 zero. */
	}
	mantissa = (bits & 0x7fffffu) | (exponent ? 0x800000u : 0u);
	value.negative = (difference < 0) ^ (bits >> 31);
	magnitude = difference < 0 ? 0u - (unsigned)difference : (unsigned)difference;
	TCE_TriggerPlace(&value, (unsigned long long)magnitude * mantissa,
	                 (exponent ? (int)exponent - 1 : 0) + 149);
	__control87_2(0, 0, &control, NULL);
	TCE_TriggerRound(&value, control);
	start.negative = previous < 0;
	magnitude = previous < 0 ? 0u - (unsigned)previous : (unsigned)previous;
	TCE_TriggerPlace(&start, magnitude, 298);
	TCE_TriggerAdd(&value, &start, control);

	for (bit = 362; bit < 576; ++bit)
	{
		if (TCE_TriggerBit(&value, bit))
		{
			return 0;
		}
	}
	for (bit = 0; bit < 32; ++bit)
	{
		low |= TCE_TriggerBit(&value, 298 + bit) << bit;
		high |= TCE_TriggerBit(&value, 330 + bit) << bit;
	}
	if (high > 0x80000000u ||
	    (high == 0x80000000u && (!value.negative || low)))
	{
		return 0;
	}
	if (value.negative)
	{
		low = 0u - low;
	}
	memcpy(&result, &low, sizeof(result));
	return result;
}
#endif
#endif
