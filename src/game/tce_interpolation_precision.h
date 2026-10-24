#ifndef TCE_INTERPOLATION_PRECISION_H
#define TCE_INTERPOLATION_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_rotate_precision.h"

/* The caller establishes a nonzero wrapped duration before dividing.
 * Generate the normalized rational significand, round to x87 PC/RC, then
 * round the original FSTP binary32 store separately. */
static __inline float TCE_InterpolationFactor32(int elapsed, int duration)
{
	tceTriggerSum_t value = { { 0 }, 0 };
	unsigned n, d, probe, control, precision, rounding, increment = 0;
	unsigned long long numerator, denominator, remainder, significand = 1;
	int numeratorBit = 0, denominatorBit = 0, exponent, bit;

	value.negative = (elapsed < 0) ^ (duration < 0);
	n = elapsed < 0 ? 0u - (unsigned)elapsed : (unsigned)elapsed;
	d = duration < 0 ? 0u - (unsigned)duration : (unsigned)duration;
	__control87_2(0, 0, &control, NULL);
	if (!n)
	{
		return TCE_RotateStore(value, control);
	}
	for (probe = n; probe >> 1; probe >>= 1)
	{
		++numeratorBit;
	}
	for (probe = d; probe >> 1; probe >>= 1)
	{
		++denominatorBit;
	}
	exponent = numeratorBit - denominatorBit;
	numerator = n;
	denominator = d;
	if (exponent >= 0)
	{
		denominator <<= exponent;
	}
	else
	{
		numerator <<= -exponent;
	}
	if (numerator < denominator)
	{
		numerator <<= 1;
		--exponent;
	}
	remainder = numerator - denominator;
	precision = (control & _MCW_PC) == _PC_24 ? 24u :
	            (control & _MCW_PC) == _PC_53 ? 53u : 64u;
	for (bit = 1; bit < (int)precision; ++bit)
	{
		remainder <<= 1;
		significand <<= 1;
		if (remainder >= denominator)
		{
			remainder -= denominator;
			significand |= 1;
		}
	}
	rounding = control & _MCW_RC;
	if (rounding == _RC_NEAR)
	{
		increment = (remainder << 1) > denominator ||
		            ((remainder << 1) == denominator && (significand & 1));
	}
	else if (rounding == _RC_UP)
	{
		increment = remainder && !value.negative;
	}
	else if (rounding == _RC_DOWN)
	{
		increment = remainder && value.negative;
	}
	if (increment)
	{
		if (significand == ~0ULL)
		{
			significand = 1ULL << 63;
			++exponent;
		}
		else
		{
			++significand;
			if (precision < 64 && (significand >> precision))
			{
				significand >>= 1;
				++exponent;
			}
		}
	}
	TCE_TriggerPlace(&value, significand, 298 + exponent - ((int)precision - 1));
	return TCE_RotateStore(value, control);
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
