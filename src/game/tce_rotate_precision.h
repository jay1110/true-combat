#ifndef TCE_ROTATE_PRECISION_H
#define TCE_ROTATE_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_trigger_precision.h"

/* These finite products fit in the existing 2^-298 integer representation.
 * Preserve the original arithmetic precision before rounding the final store
 * to binary32. Floating-point exception flags are not reproduced. */
static __inline void TCE_RotateAdd(tceTriggerSum_t *sum,
                                  const tceTriggerSum_t *product,
                                  unsigned control)
{
	tceTriggerSum_t zero = { { 0 }, 0 };
	unsigned opposite = sum->negative != product->negative;

	TCE_TriggerAddExact(sum, product);
	if (opposite && !TCE_TriggerMagnitude(sum, &zero))
	{
		sum->negative = (control & _MCW_RC) == _RC_DOWN;
	}
	TCE_TriggerRound(sum, control);
}

static __inline float TCE_RotateStore(tceTriggerSum_t value, unsigned control)
{
	unsigned rounding = control & _MCW_RC;
	unsigned bits, discarded = 0, belowHalf = 0, increment = 0, carry;
	unsigned long long wide;
	int highest, drop, bit, word;
	float result;

	for (highest = 575; highest >= 0; --highest)
	{
		if (TCE_TriggerBit(&value, highest))
		{
			break;
		}
	}
	/* Subnormals have a fixed 2^-149 quantum, regardless of magnitude. */
	drop = highest >= 172 ? highest - 23 : 149;
	for (bit = 0; bit < drop; ++bit)
	{
		if (TCE_TriggerBit(&value, bit))
		{
			discarded = 1;
			if (bit < drop - 1)
			{
				belowHalf = 1;
			}
		}
	}
	if (rounding == _RC_NEAR)
	{
		increment = TCE_TriggerBit(&value, drop - 1) &&
		            (belowHalf || TCE_TriggerBit(&value, drop));
	}
	else if (rounding == _RC_UP)
	{
		increment = discarded && !value.negative;
	}
	else if (rounding == _RC_DOWN)
	{
		increment = discarded && value.negative;
	}
	for (bit = 0; bit < drop; ++bit)
	{
		value.word[bit / 32] &= ~(1u << (bit % 32));
	}
	if (increment)
	{
		word = drop / 32;
		wide = (unsigned long long)value.word[word] + (1u << (drop % 32));
		value.word[word] = (unsigned)wide;
		carry = (unsigned)(wide >> 32);
		while (carry && ++word < 18)
		{
			++value.word[word];
			carry = value.word[word] == 0;
		}
	}
	for (highest = 575; highest >= 0; --highest)
	{
		if (TCE_TriggerBit(&value, highest))
		{
			break;
		}
	}
	bits = value.negative << 31;
	if (highest >= 426)
	{
		if (rounding == _RC_NEAR ||
		    (rounding == _RC_UP && !value.negative) ||
		    (rounding == _RC_DOWN && value.negative))
		{
			bits |= 0x7f800000u;
		}
		else
		{
			bits |= 0x7f7fffffu;
		}
	}
	else
	{
		drop = highest >= 172 ? highest - 23 : 149;
		if (highest >= 172)
		{
			bits |= (unsigned)(highest - 171) << 23;
		}
		for (bit = 0; bit < 23; ++bit)
		{
			bits |= TCE_TriggerBit(&value, drop + bit) << bit;
		}
	}
	memcpy(&result, &bits, sizeof(result));
	return result;
}

static __inline int TCE_FiniteDifferenceStore(float left, float right,
                                             float *result)
{
	tceTriggerSum_t sum, subtrahend;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_TriggerProduct(&sum, left, 1.0f, control) ||
	    !TCE_TriggerProduct(&subtrahend, right, 1.0f, control))
	{
		return 0;
	}
	subtrahend.negative ^= 1u;
	TCE_RotateAdd(&sum, &subtrahend, control);
	*result = TCE_RotateStore(sum, control);
	return 1;
}

static __inline int TCE_RotateFiniteRow(float x, float y, float z,
                                       const float *row, float *result)
{
	tceTriggerSum_t sum, product;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_TriggerProduct(&sum, z, row[2], control) ||
	    !TCE_TriggerProduct(&product, y, row[1], control))
	{
		return 0;
	}
	TCE_RotateAdd(&sum, &product, control);
	if (!TCE_TriggerProduct(&product, x, row[0], control))
	{
		return 0;
	}
	TCE_RotateAdd(&sum, &product, control);
	*result = TCE_RotateStore(sum, control);
	return 1;
}

static __inline int TCE_MuzzleFiniteComponent(float direction, float factor,
                                             float origin, int subtract,
                                             float *result)
{
	tceTriggerSum_t product, position;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_TriggerProduct(&product, direction, factor, control) ||
	    !TCE_TriggerProduct(&position, origin, 1.0f, control))
	{
		return 0;
	}
	product.negative ^= subtract != 0;
	TCE_RotateAdd(&product, &position, control);
	*result = TCE_RotateStore(product, control);
	return 1;
}
#endif
#endif
