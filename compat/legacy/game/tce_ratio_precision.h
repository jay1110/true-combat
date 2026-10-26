#ifndef TCE_RATIO_PRECISION_H
#define TCE_RATIO_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_rotate_precision.h"

/* Count within a 32-bit word. MSVC x86 /O2 can terminate a shifting uint64
 * loop when its upper word becomes zero, before counting the lower word.
 * Splitting the words keeps the exponent independent of optimization. */
static __inline int TCE_RatioHighestBit(unsigned long long value)
{
	unsigned high = (unsigned)(value >> 32);
	unsigned word = high ? high : (unsigned)value;
	int bit = high ? 32 : 0;
	while (word > 1u)
	{
		word >>= 1;
		++bit;
	}
	return bit;
}

/* Round (n/d)*2^binaryScale to x87 PC/RC without a Float spill.
 * Callers supply unsigned64 n, nonzero unsigned64 d and a scaled result
 * whose significant bits fit the fixed grid. Exception flags and unmasked
 * traps are not reproduced. */
static __inline tceTriggerSum_t TCE_RatioValue(unsigned long long n,
                                              unsigned long long d, int binaryScale,
                                              unsigned negative, unsigned control)
{
	tceTriggerSum_t value = { { 0 }, 0 };
	unsigned precision, rounding, increment = 0, carry;
	unsigned long long numerator, denominator, remainder, significand = 1;
	int numeratorBit, denominatorBit, exponent, bit;

	value.negative = negative;
	if (!n)
	{
		return value;
	}
	numeratorBit = TCE_RatioHighestBit(n);
	denominatorBit = TCE_RatioHighestBit(d);
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
		/* 2*n can need 65 bits, but 0 <= 2*n-d < d fits unsigned64. */
		remainder = numerator - denominator + numerator;
		--exponent;
	}
	else
	{
		remainder = numerator - denominator;
	}
	precision = (control & _MCW_PC) == _PC_24 ? 24u :
	            (control & _MCW_PC) == _PC_53 ? 53u : 64u;
	for (bit = 1; bit < (int)precision; ++bit)
	{
		carry = (unsigned)(remainder >> 63);
		remainder <<= 1;
		significand <<= 1;
		if (carry || remainder >= denominator)
		{
			remainder -= denominator;
			significand |= 1;
		}
	}
	rounding = control & _MCW_RC;
	if (rounding == _RC_NEAR)
	{
		increment = (remainder >> 63) || (remainder << 1) > denominator ||
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

/* Divide a PC-rounded fixed value by a finite nonzero Float without an
 * intermediate Double store. Reject results outside the fixed grid. */
static __inline int TCE_FixedDivideFloat(const tceTriggerSum_t *value,
                                          float divisor, unsigned control,
                                          tceTriggerSum_t *result)
{
	unsigned bits, exponent, mantissa, negative;
	unsigned long long significand = 0;
	int highest, drop, bit, numeratorBit, denominatorBit, scale, power;

	memcpy(&bits, &divisor, sizeof(bits));
	exponent = (bits >> 23) & 255u;
	if (exponent == 255u || !(bits & 0x7fffffffu))
	{
		return 0;
	}
	mantissa = (bits & 0x7fffffu) | (exponent ? 0x800000u : 0u);
	negative = value->negative ^ (bits >> 31);
	for (highest = 575; highest >= 0 && !TCE_TriggerBit(value, highest); --highest)
	{
	}
	drop = highest > 63 ? highest - 63 : 0;
	for (bit = 0; bit < 64; ++bit)
	{
		significand |= (unsigned long long)TCE_TriggerBit(value, drop + bit) << bit;
	}
	scale = drop - 298 + (exponent ? 150 - (int)exponent : 149);
	if (significand)
	{
		numeratorBit = TCE_RatioHighestBit(significand);
		denominatorBit = TCE_RatioHighestBit(mantissa);
		power = numeratorBit - denominatorBit;
		if ((power >= 0 && significand < ((unsigned long long)mantissa << power)) ||
		    (power < 0 && (significand << -power) < mantissa))
		{
			--power;
		}
		power += scale;
		/* PC64 needs at most 63 fractional significand bits. Keep one
		 * high bit available for a rounding carry. */
		if (power < -235 || power > 276)
		{
			return 0;
		}
	}
	*result = TCE_RatioValue(significand, mantissa, scale, negative, control);
	return 1;
}

static __inline float TCE_RatioFloat32(unsigned n, unsigned d, int binaryScale,
                                        unsigned negative, unsigned control)
{
	tceTriggerSum_t value = TCE_RatioValue(n, d, binaryScale, negative, control);
	return TCE_RotateStore(value, control);
}
#endif
#endif
