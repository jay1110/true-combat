#ifndef TCE_RATIO_PRECISION_H
#define TCE_RATIO_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_rotate_precision.h"

/* Round (n/d)*2^binaryScale to x87 PC/RC without a Float spill.
 * Callers supply n <= 2^31, 0 < d < 2^40 and a scaled exponent in
 * [-144,180]. Exception flags and unmasked traps are not reproduced. */
static __inline tceTriggerSum_t TCE_RatioValue(unsigned long long n,
                                              unsigned long long d, int binaryScale,
                                              unsigned negative, unsigned control)
{
	tceTriggerSum_t value = { { 0 }, 0 };
	unsigned precision, rounding, increment = 0;
	unsigned long long probe, numerator, denominator, remainder, significand = 1;
	int numeratorBit = 0, denominatorBit = 0, exponent, bit;

	value.negative = negative;
	if (!n)
	{
		return value;
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
	TCE_TriggerPlace(&value, significand, 298 + exponent + binaryScale - ((int)precision - 1));
	return value;
}

static __inline float TCE_RatioFloat32(unsigned n, unsigned d, int binaryScale,
                                        unsigned negative, unsigned control)
{
	tceTriggerSum_t value = TCE_RatioValue(n, d, binaryScale, negative, control);
	return TCE_RotateStore(value, control);
}
#endif
#endif
