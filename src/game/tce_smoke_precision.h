#ifndef TCE_SMOKE_PRECISION_H
#define TCE_SMOKE_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_interpolation_precision.h"

/* A rounded difference of two Floats has at most64 significant bits and is
 * a multiple of 2^-149. Square its significand exactly before PC rounding. */
static __inline void TCE_SmokeSquareDifference(const tceTriggerSum_t *difference,
                                               tceTriggerSum_t *squared,
                                               unsigned control)
{
	unsigned input[2], product[4] = { 0 };
	unsigned long long significand = 0, wide, carry;
	int lowest, bit, i, j, shift;

	memset(squared, 0, sizeof(*squared));
	for (lowest = 0; lowest < 576; ++lowest)
	{
		if (TCE_TriggerBit(difference, lowest))
		{
			break;
		}
	}
	if (lowest == 576)
	{
		return;
	}
	for (bit = 0; bit < 64 && lowest + bit < 576; ++bit)
	{
		significand |= (unsigned long long)TCE_TriggerBit(difference, lowest + bit) << bit;
	}
	input[0] = (unsigned)significand;
	input[1] = (unsigned)(significand >> 32);
	for (i = 0; i < 2; ++i)
	{
		carry = 0;
		for (j = 0; j < 2; ++j)
		{
			wide = (unsigned long long)input[i] * input[j] + product[i + j] + carry;
			product[i + j] = (unsigned)wide;
			carry = wide >> 32;
		}
		product[i + 2] = (unsigned)carry;
	}
	shift = 2 * lowest - 298;
	TCE_TriggerPlace(squared, product[0] | ((unsigned long long)product[1] << 32), shift);
	TCE_TriggerPlace(squared, product[2] | ((unsigned long long)product[3] << 32), shift + 64);
	TCE_TriggerRound(squared, control);
}

static __inline int TCE_FiniteSmokeDistance(const float *start, const float *end,
                                            int *farEnough)
{
	tceTriggerSum_t difference[3], value, sum, boundary = { { 0 }, 0 };
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&difference[axis], end[axis], 1.0f, control) ||
		    !TCE_TriggerProduct(&value, start[axis], 1.0f, control))
		{
			return 0;
		}
		value.negative ^= 1u;
		TCE_RotateAdd(&difference[axis], &value, control);
	}
	TCE_SmokeSquareDifference(&difference[2], &sum, control);
	for (axis = 1; axis >= 0; --axis)
	{
		TCE_SmokeSquareDifference(&difference[axis], &value, control);
		TCE_RotateAdd(&sum, &value, control);
	}
	TCE_TriggerPlace(&boundary, 10000u, 298);
	*farEnough = TCE_TriggerCompare(&sum, &boundary) >= 0;
	return 1;
}

/* A Float square needs at most48 significant bits. After PC rounding it
 * therefore fits binary64 exactly, with no extra rounding at this boundary. */
static __inline double TCE_SmokeSquaredRadius(float radius)
{
	tceTriggerSum_t squared;
	unsigned control;
	unsigned long long bits = 0;
	int highest, bit, position;
	double result;

	__control87_2(0, 0, &control, NULL);
	TCE_TriggerProduct(&squared, radius, radius, control);
	for (highest = 575; highest >= 0; --highest)
	{
		if (TCE_TriggerBit(&squared, highest))
		{
			break;
		}
	}
	if (highest >= 0)
	{
		bits = (unsigned long long)(highest + 725) << 52;
		for (bit = 0; bit < 52; ++bit)
		{
			position = highest - 52 + bit;
			if (position >= 0)
			{
				bits |= (unsigned long long)TCE_TriggerBit(&squared, position) << bit;
			}
		}
	}
	memcpy(&result, &bits, sizeof(result));
	return result;
}

/* FILD signed elapsed; multiply by double .0001, then double320; store
 * Float before comparing the retained radius to320. Negative ages are not
 * clamped. Exception flags and unmasked traps remain separate. */
static __inline float TCE_SmokeRadius32(int elapsed)
{
	tceTriggerSum_t value = { { 0 }, 0 }, maximum;
	unsigned magnitude, control;
	unsigned long long low, high;
	float result;

	value.negative = elapsed < 0;
	magnitude = value.negative ? 0u - (unsigned)elapsed : (unsigned)elapsed;
	/* .0001 is exactly 0x1a36e2eb1c432d * 2^-66. */
	low = (unsigned long long)magnitude * 0xeb1c432du;
	high = (unsigned long long)magnitude * 0x1a36e2u + (low >> 32);
	TCE_TriggerPlace(&value, (unsigned)low, 232);
	TCE_TriggerPlace(&value, high, 264);
	__control87_2(0, 0, &control, NULL);
	TCE_TriggerRound(&value, control);
	TCE_InterpolationMultiply(&value, 0x43a00000u, control);
	result = TCE_RotateStore(value, control);
	TCE_TriggerProduct(&maximum, 320.0f, 1.0f, control);
	if (TCE_TriggerCompare(&value, &maximum) > 0)
	{
		result = 320.0f;
	}
	return result;
}
#endif
#endif
