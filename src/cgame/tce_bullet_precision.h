#ifndef TCE_BULLET_PRECISION_H
#define TCE_BULLET_PRECISION_H

#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "../game/tce_length_precision.h"
#include "../game/tce_ambient_precision.h"

/* The masked integer is exact Float. Preserve each native random factor,
 * optional addition and scaling stage before the final Float argument. */
static __inline int TCE_BulletRandomEffect(unsigned sample, float addition,
                                           float scale, float *result)
{
	tceTriggerSum_t value, addend, factor;
	unsigned control, additionBits, scaleBits;

	memcpy(&additionBits, &addition, sizeof(additionBits));
	memcpy(&scaleBits, &scale, sizeof(scaleBits));
	/* Only these three production chains are covered by the range proof.
	 * All operands/results are normal or zero and at most 32767; only
	 * inexact can occur. Other exception masks need not force a fallback. */
	if (!((additionBits == 0x3f800000u &&
	       (scaleBits == 0x3f400000u || scaleBits == 0x41000000u)) ||
	      (additionBits == 0u && scaleBits == 0x43b40000u)))
	{
		return 0;
	}
	__control87_2(0, 0, &control, NULL);
	if (sample > 32767u || !(control & _EM_INEXACT) ||
	    !TCE_TriggerProduct(&value, (float)sample, 1.0f / 32767.0f, control) ||
	    !TCE_TriggerProduct(&factor, scale, 1.0f, control))
	{
		return 0;
	}
	if (addition != 0.0f)
	{
		if (!TCE_TriggerProduct(&addend, addition, 1.0f, control))
		{
			return 0;
		}
		TCE_RotateAdd(&value, &addend, control);
	}
	if (!TCE_AmbientProductFits(&value, &factor))
	{
		return 0;
	}
	TCE_GustMultiply(&value, &factor, control);
	*result = TCE_RotateStore(value, control);
	return 1;
}

/* Q_crandom(seed)*100 has at most 20 significant bits. Both spread inputs
 * are therefore exact Float values even though the second native result
 * stays on the x87 stack. Round each subsequent operation at current PC/RC. */
static __inline int TCE_BulletAntilagEnd(const float *forward, const float *right,
                                        const float *up, const float *muzzle,
                                        float horizontal, float vertical, float *end)
{
	tceTriggerSum_t spread, rise, distance, origin;
	float result[3];
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&spread, right[axis], horizontal, control) ||
		    !TCE_TriggerProduct(&rise, vertical, up[axis], control) ||
		    !TCE_TriggerProduct(&distance, forward[axis], 8192.0f, control) ||
		    !TCE_TriggerProduct(&origin, muzzle[axis], 1.0f, control))
		{
			return 0;
		}
		TCE_RotateAdd(&spread, &rise, control);
		TCE_RotateAdd(&spread, &distance, control);
		TCE_RotateAdd(&spread, &origin, control);
		result[axis] = TCE_RotateStore(spread, control);
	}
	memcpy(end, result, sizeof(result));
	return 1;
}

/* A retained length has at most 64 significand bits, a Float factor 24.
 * If their low product bits fall below the fixed grid, scale the root by
 * 2^64 before multiplication. This is exact and preserves PC/RC rounding.
 * After scaling back, retain a sticky bit below the Float subnormal quantum. */
static __inline int TCE_BulletLengthProduct(const tceTriggerSum_t *root,
                                           const tceTriggerSum_t *factor,
                                           unsigned control, float *result)
{
	tceTriggerSum_t product = *root;
	int word;

	if (TCE_AmbientProductFits(root, factor))
	{
		TCE_GustMultiply(&product, factor, control);
	}
	else
	{
		tceTriggerSum_t scaled = { { 0 }, 0 };

		scaled.negative = root->negative;

		if (root->word[16] || root->word[17])
		{
			return 0;
		}
		for (word = 0; word < 16; ++word)
		{
			scaled.word[word + 2] = root->word[word];
		}
		if (!TCE_AmbientProductFits(&scaled, factor))
		{
			return 0;
		}
		TCE_GustMultiply(&scaled, factor, control);
		memset(&product, 0, sizeof(product));
		product.negative = scaled.negative;
		for (word = 0; word < 16; ++word)
		{
			product.word[word] = scaled.word[word + 2];
		}
		if (scaled.word[0] || scaled.word[1])
		{
			product.word[0] |= 1u;
		}
	}
	*result = TCE_RotateStore(product, control);
	return 1;
}

/* Windows stores each root*direction product before adding the impact point.
 * Keep the root in the existing integer representation instead of narrowing
 * it through MSVC's binary64 long double return. Commit only complete output. */
static __inline int TCE_BulletProjectedOffset(const float *delta,
                                             const float *direction, float *offset)
{
	tceTriggerSum_t squared, term, root, factor;
	float result[3];
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&squared, delta[0], delta[0], control))
	{
		return 0;
	}
	for (axis = 1; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&term, delta[axis], delta[axis], control))
		{
			return 0;
		}
		TCE_RotateAdd(&squared, &term, control);
	}
	root = TCE_LengthRoot(&squared, control);
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&factor, direction[axis], 1.0f, control) ||
		    !TCE_BulletLengthProduct(&root, &factor, control, &result[axis]))
		{
			return 0;
		}
	}
	memcpy(offset, result, sizeof(result));
	return 1;
}
#endif
#endif
