#ifndef TCE_X87_SUM_H
#define TCE_X87_SUM_H

#if defined(_MSC_VER) && defined(_M_IX86)
#include <float.h>
#include <string.h>

/* Exact int32 + binary32 addition in units of 2^-149. Nine 32-bit words
 * cover every finite float. Round once to the current x87 precision/mode,
 * then return the low word of signed64 truncation, as __ftol does.
 * Floating-point exception flags and unmasked traps are not reproduced. */
static __inline unsigned TCE_X87SumBit(const unsigned *words, int bit)
{
	return (words[bit / 32] >> (bit % 32)) & 1u;
}

static __inline void TCE_X87SumPlace(unsigned *words, unsigned value, int shift)
{
	int bit;
	for (bit = 0; bit < 32; ++bit)
	{
		if ((value >> bit) & 1u)
		{
			words[(shift + bit) / 32] |= 1u << ((shift + bit) % 32);
		}
	}
}

static __inline int TCE_IntFloatSumLow32(int integer, float value)
{
	unsigned a[9] = { 0 }, b[9] = { 0 }, sum[9] = { 0 };
	unsigned bits, exponent, mantissa, negative, integerNegative, magnitude;
	unsigned control, rounding, precision, low = 0, high = 0;
	unsigned carry = 0, borrow = 0, discarded = 0, belowHalf = 0, increment = 0;
	unsigned long long wide;
	const unsigned *large, *small;
	int i, highest, drop, order = 0, result;

	memcpy(&bits, &value, sizeof(bits));
	exponent = (bits >> 23) & 255u;
	if (exponent == 255u)
	{
		/* NaN/Inf produce the signed64 integer-indefinite value: low32=0. */
		return 0;
	}
	mantissa = bits & 0x7fffffu;
	if (exponent)
	{
		mantissa |= 0x800000u;
	}
	negative = bits >> 31;
	integerNegative = integer < 0;
	magnitude = integerNegative ? 0u - (unsigned)integer : (unsigned)integer;
	TCE_X87SumPlace(a, magnitude, 149);
	TCE_X87SumPlace(b, mantissa, exponent ? (int)exponent - 1 : 0);

	if (negative == integerNegative)
	{
		for (i = 0; i < 9; ++i)
		{
			wide = (unsigned long long)a[i] + b[i] + carry;
			sum[i] = (unsigned)wide;
			carry = (unsigned)(wide >> 32);
		}
	}
	else
	{
		for (i = 8; i >= 0; --i)
		{
			if (a[i] != b[i])
			{
				order = a[i] > b[i] ? 1 : -1;
				break;
			}
		}
		large = order >= 0 ? a : b;
		small = order >= 0 ? b : a;
		negative = order >= 0 ? integerNegative : negative;
		for (i = 0; i < 9; ++i)
		{
			wide = (unsigned long long)small[i] + borrow;
			sum[i] = large[i] - (unsigned)wide;
			borrow = (unsigned long long)large[i] < wide;
		}
	}

	for (highest = 287; highest >= 0; --highest)
	{
		if (TCE_X87SumBit(sum, highest))
		{
			break;
		}
	}
	if (highest < 0)
	{
		return 0;
	}
	/* A zero mask reads the control word without changing it. */
	__control87_2(0, 0, &control, NULL);
	precision = (control & _MCW_PC) == _PC_24 ? 24u :
	            (control & _MCW_PC) == _PC_53 ? 53u : 64u;
	rounding = control & _MCW_RC;
	drop = highest + 1 - (int)precision;
	if (drop > 0)
	{
		for (i = 0; i < drop; ++i)
		{
			if (TCE_X87SumBit(sum, i))
			{
				discarded = 1;
				if (i < drop - 1)
				{
					belowHalf = 1;
				}
			}
		}
		if (rounding == _RC_NEAR)
		{
			increment = TCE_X87SumBit(sum, drop - 1) &&
			            (belowHalf || TCE_X87SumBit(sum, drop));
		}
		else if (rounding == _RC_UP)
		{
			increment = discarded && !negative;
		}
		else if (rounding == _RC_DOWN)
		{
			increment = discarded && negative;
		}
		for (i = 0; i < drop; ++i)
		{
			sum[i / 32] &= ~(1u << (i % 32));
		}
		if (increment)
		{
			i = drop / 32;
			wide = (unsigned long long)sum[i] + (1u << (drop % 32));
			sum[i] = (unsigned)wide;
			carry = (unsigned)(wide >> 32);
			while (carry && ++i < 9)
			{
				++sum[i];
				carry = sum[i] == 0;
			}
		}
	}

	/* Discard fractional bits only after rounding the addition. */
	for (i = 213; i < 288; ++i)
	{
		if (TCE_X87SumBit(sum, i))
		{
			return 0;
		}
	}
	for (i = 0; i < 64; ++i)
	{
		if (TCE_X87SumBit(sum, 149 + i))
		{
			if (i < 32)
			{
				low |= 1u << i;
			}
			else
			{
				high |= 1u << (i - 32);
			}
		}
	}
	if (high > 0x80000000u ||
	    (high == 0x80000000u && (!negative || low)))
	{
		return 0;
	}
	if (negative)
	{
		low = 0u - low;
	}
	memcpy(&result, &low, sizeof(result));
	return result;
}
#endif
#endif
