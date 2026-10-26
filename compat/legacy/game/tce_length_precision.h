#ifndef TCE_LENGTH_PRECISION_H
#define TCE_LENGTH_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_rotate_precision.h"

/* Compare a positive candidate root squared with the retained squared length.
 * The midpoint uses a 65-bit significand, so its square needs five words.
 * Negative shifts retain a sticky fraction below the fixed-point quantum. */
static __inline int TCE_LengthSquareCompare(unsigned long long significand,
                                            int shift, int midpoint,
                                            const tceTriggerSum_t *squared)
{
	unsigned input[3], product[6] = { 0 };
	tceTriggerSum_t candidate = { { 0 }, 0 };
	unsigned long long wide, carry;
	unsigned fractional = 0;
	int i, j, bit, position, order;

	input[0] = (unsigned)significand;
	input[1] = (unsigned)(significand >> 32);
	input[2] = 0;
	if (midpoint)
	{
		input[2] = input[1] >> 31;
		input[1] = (input[1] << 1) | (input[0] >> 31);
		input[0] = (input[0] << 1) | 1u;
		shift -= 2;
	}
	for (i = 0; i < 3; ++i)
	{
		carry = 0;
		for (j = 0; j < 3; ++j)
		{
			wide = (unsigned long long)input[i] * input[j] +
			       product[i + j] + carry;
			product[i + j] = (unsigned)wide;
			carry = wide >> 32;
		}
		product[i + 3] = (unsigned)carry;
	}
	for (bit = 0; bit < 192; ++bit)
	{
		if ((product[bit / 32] >> (bit % 32)) & 1u)
		{
			position = shift + bit;
			if (position < 0)
			{
				fractional = 1;
			}
			else
			{
				candidate.word[position / 32] |= 1u << (position % 32);
			}
		}
	}
	order = TCE_TriggerMagnitude(&candidate, squared);
	return order ? order : (int)fractional;
}

/* Finite binary32 inputs bound the root exponent to [-149,128]. All candidate
 * squares fit the existing 576-bit representation. Round the root once at
 * current PC/RC, without using MSVC's binary64 long double or spilling Float.
 * Floating exception flags and unmasked traps remain outside this helper. */
static __inline tceTriggerSum_t TCE_LengthRoot(const tceTriggerSum_t *squared,
                                              unsigned control)
{
	tceTriggerSum_t root = { { 0 }, 0 };
	unsigned precision, rounding, increment = 0;
	unsigned long long low, high, middle, difference;
	int highest, exponent, shift, order;

	for (highest = 575; highest >= 0; --highest)
	{
		if (TCE_TriggerBit(squared, highest))
		{
			break;
		}
	}
	if (highest < 0)
	{
		return root;
	}
	exponent = highest - 298;
	exponent = exponent >= 0 ? exponent / 2 : -((-exponent + 1) / 2);
	precision = (control & _MCW_PC) == _PC_24 ? 24u :
	            (control & _MCW_PC) == _PC_53 ? 53u : 64u;
	shift = 298 + 2 * (exponent - ((int)precision - 1));
	low = 1ULL << (precision - 1);
	high = precision == 64 ? ~0ULL : (1ULL << precision) - 1;
	while (low < high)
	{
		difference = high - low;
		middle = low + (difference >> 1) + (difference & 1u);
		if (TCE_LengthSquareCompare(middle, shift, 0, squared) <= 0)
		{
			low = middle;
		}
		else
		{
			high = middle - 1;
		}
	}
	order = TCE_LengthSquareCompare(low, shift, 0, squared);
	rounding = control & _MCW_RC;
	if (order && rounding == _RC_UP)
	{
		increment = 1;
	}
	else if (order && rounding == _RC_NEAR)
	{
		order = TCE_LengthSquareCompare(low, shift, 1, squared);
		increment = order < 0 || (!order && (low & 1u));
	}
	if (increment)
	{
		if (low == ~0ULL)
		{
			low = 1ULL << 63;
			++exponent;
		}
		else
		{
			++low;
		}
	}
	TCE_TriggerPlace(&root, low, 298 + exponent - ((int)precision - 1));
	return root;
}

static __inline int TCE_NearestFiniteLength(const float *delta, float threshold,
                                            int *closer, float *stored)
{
	tceTriggerSum_t squared, product, root, boundary;
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	if (!TCE_TriggerProduct(&boundary, threshold, 1.0f, control) ||
	    !TCE_TriggerProduct(&squared, delta[0], delta[0], control))
	{
		return 0;
	}
	for (axis = 1; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&product, delta[axis], delta[axis], control))
		{
			return 0;
		}
		TCE_RotateAdd(&squared, &product, control);
	}
	root = TCE_LengthRoot(&squared, control);
	*closer = TCE_TriggerCompare(&root, &boundary) < 0;
	if (*closer)
	{
		*stored = TCE_RotateStore(root, control);
	}
	return 1;
}
#endif
#endif
