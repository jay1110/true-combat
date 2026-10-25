#ifndef TCE_IMPACT_PRECISION_H
#define TCE_IMPACT_PRECISION_H

#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_gust_precision.h"
#include "tce_normalize_precision.h"

/* The caller has already consumed its one masked CRT random sample. These
 * bounded chains stay above the fixed grid's 2^-298 quantum throughout. */
static __inline int TCE_ImpactRandomBegin(int sample, unsigned *control,
                                         tceTriggerSum_t *value)
{
	__control87_2(0, 0, control, NULL);
	if (!TCE_FiniteExceptionsMasked(*control) || sample < 0 || sample > 32767)
	{
		return 0;
	}
	return TCE_TriggerProduct(value, (float)sample, 1.0f / 32767.0f, *control);
}

static __inline void TCE_ImpactScale(tceTriggerSum_t *value, float factor,
                                    unsigned control)
{
	tceTriggerSum_t term;
	TCE_TriggerProduct(&term, factor, 1.0f, control);
	TCE_GustMultiply(value, &term, control);
}

static __inline void TCE_ImpactCenter(tceTriggerSum_t *value, unsigned control)
{
	tceTriggerSum_t half;
	TCE_TriggerProduct(&half, -0.5f, 1.0f, control);
	TCE_RotateAdd(value, &half, control);
	TCE_ImpactScale(value, 2.0f, control);
}

static __inline int TCE_ImpactFiniteRandom(int sample, int mode, float *stored)
{
	tceTriggerSum_t value, half;
	unsigned control;
	if (!TCE_ImpactRandomBegin(sample, &control, &value))
	{
		return 0;
	}
	if (mode == 1)
	{
		TCE_TriggerProduct(&half, 0.5f, 1.0f, control);
		TCE_RotateAdd(&value, &half, control);
	}
	else if (mode == 2)
	{
		TCE_ImpactScale(&value, 360.0f, control);
	}
	else if (mode != 0)
	{
		TCE_ImpactCenter(&value, control);
		TCE_ImpactScale(&value, mode == 3 ? 720.0f : 90.0f, control);
	}
	*stored = TCE_RotateStore(value, control);
	return 1;
}

static __inline int TCE_ImpactFiniteDirection(int sample, float base, int dust,
                                             float *stored)
{
	tceTriggerSum_t value, offset;
	unsigned control;
	if (!TCE_ImpactRandomBegin(sample, &control, &value) ||
	    !TCE_TriggerProduct(&offset, base, 1.0f, control))
	{
		return 0;
	}
	TCE_ImpactCenter(&value, control);
	if (dust)
	{
		TCE_ImpactScale(&value, 0.25f, control);
	}
	TCE_RotateAdd(&value, &offset, control);
	*stored = TCE_RotateStore(value, control);
	return 1;
}

static __inline int TCE_ImpactFiniteLifetime(int sample, int *stored)
{
	tceTriggerSum_t value, one;
	unsigned control, integer = 0;
	int bit;
	if (!TCE_ImpactRandomBegin(sample, &control, &value))
	{
		return 0;
	}
	TCE_TriggerProduct(&one, 1.0f, 1.0f, control);
	TCE_RotateAdd(&value, &one, control);
	TCE_ImpactScale(&value, 500.0f, control);
	/* The positive retained value is bounded by 1001. Native __ftol
	 * truncates it directly, without an intermediate Float store. */
	for (bit = 0; bit < 10; ++bit)
	{
		integer |= TCE_TriggerBit(&value, 298 + bit) << bit;
	}
	*stored = (int)integer;
	return 1;
}

/* Unlike the constant random chains, scaled geometry can span arbitrary
 * Float exponents. Never silently discard bits outside the fixed grid. */
static __inline int TCE_ImpactMultiply(tceTriggerSum_t *value, float factor,
                                       unsigned control)
{
	tceTriggerSum_t term;
	int lowValue, lowTerm, highValue, highTerm;
	if (!TCE_TriggerProduct(&term, factor, 1.0f, control))
	{
		return 0;
	}
	for (lowValue = 0; lowValue < 576 && !TCE_TriggerBit(value, lowValue); ++lowValue)
	{
	}
	for (lowTerm = 0; lowTerm < 576 && !TCE_TriggerBit(&term, lowTerm); ++lowTerm)
	{
	}
	if (lowValue < 576 && lowTerm < 576)
	{
		for (highValue = 575; !TCE_TriggerBit(value, highValue); --highValue)
		{
		}
		for (highTerm = 575; !TCE_TriggerBit(&term, highTerm); --highTerm)
		{
		}
		if (lowValue + lowTerm < 298 || highValue + highTerm > 866)
		{
			return 0;
		}
	}
	TCE_GustMultiply(value, &term, control);
	return 1;
}

static __inline int TCE_ImpactFiniteChipSpeed(int sample, float component,
                                             float scale, float *stored)
{
	tceTriggerSum_t value, base;
	unsigned control;
	if (!TCE_ImpactRandomBegin(sample, &control, &value))
	{
		return 0;
	}
	TCE_ImpactScale(&value, 24.0f, control);
	TCE_TriggerProduct(&base, 12.0f, 1.0f, control);
	TCE_RotateAdd(&value, &base, control);
	if (!TCE_ImpactMultiply(&value, scale, control) ||
	    !TCE_ImpactMultiply(&value, 2.0f / 3.0f, control) ||
	    !TCE_ImpactMultiply(&value, component, control))
	{
		return 0;
	}
	*stored = TCE_RotateStore(value, control);
	return 1;
}

static __inline int TCE_ImpactFiniteSmokeSpeed(float randomScale, float scale,
                                              float *stored)
{
	tceTriggerSum_t value, base;
	unsigned control;
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&value, randomScale, 16.0f, control))
	{
		return 0;
	}
	TCE_TriggerProduct(&base, 26.0f, 1.0f, control);
	TCE_RotateAdd(&value, &base, control);
	if (!TCE_ImpactMultiply(&value, scale, control))
	{
		return 0;
	}
	*stored = TCE_RotateStore(value, control);
	return 1;
}

static __inline int TCE_ImpactFiniteSmokeLife(int sample, float factor,
                                             float life, float scale, float *stored)
{
	tceTriggerSum_t value, base;
	unsigned control;
	if (!TCE_ImpactRandomBegin(sample, &control, &value) ||
	    !TCE_ImpactMultiply(&value, life, control) ||
	    !TCE_ImpactMultiply(&value, 1.2f, control) ||
	    !TCE_TriggerProduct(&base, factor, life, control))
	{
		return 0;
	}
	TCE_RotateAdd(&value, &base, control);
	if (!TCE_ImpactMultiply(&value, scale, control))
	{
		return 0;
	}
	*stored = TCE_RotateStore(value, control);
	return 1;
}

static __inline int TCE_ImpactFiniteGeometry(int index, float spread,
                                            const float *origin, const float *direction,
                                            float *position, float *factor)
{
	tceTriggerSum_t distance, value, base;
	float positions[3], storedFactor;
	unsigned control;
	int axis;
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) || index < 0 || index > 2 ||
	    !TCE_TriggerProduct(&distance, spread, (float)(index + 1), control))
	{
		return 0;
	}
	for (axis = 0; axis < 3; ++axis)
	{
		value = distance;
		if (!TCE_ImpactMultiply(&value, direction[axis], control) ||
		    !TCE_TriggerProduct(&base, origin[axis], 1.0f, control))
		{
			return 0;
		}
		TCE_RotateAdd(&value, &base, control);
		positions[axis] = TCE_RotateStore(value, control);
	}
	TCE_TriggerProduct(&value, (float)(index + 2), 1.0f / 3.0f, control);
	TCE_TriggerProduct(&base, 1.0f, 1.0f, control);
	TCE_RotateAdd(&value, &base, control);
	storedFactor = TCE_RotateStore(value, control);
	memcpy(position, positions, sizeof(positions));
	*factor = storedFactor;
	return 1;
}

static __inline int TCE_ImpactFiniteTrajectory(int index, float life, float gravity,
                                              int *duration, float *scaledGravity)
{
	tceTriggerSum_t factor, one, value;
	unsigned control, integer = 0;
	int bit;
	float storedGravity;
	__control87_2(0, 0, &control, NULL);
	/* The producer supplies only indices0..2 and life200..500. */
	if (!TCE_FiniteExceptionsMasked(control) || index < 0 || index > 2 ||
	    !(life >= 0.0f && life <= 500.0f))
	{
		return 0;
	}
	TCE_TriggerProduct(&factor, (float)(index + 1), 0.5f, control);
	TCE_TriggerProduct(&one, 1.0f, 1.0f, control);
	TCE_RotateAdd(&factor, &one, control);
	value = factor;
	if (!TCE_ImpactMultiply(&value, life, control))
	{
		return 0;
	}
	/* Retained positive duration <=1250, so eleven integer bits suffice. */
	for (bit = 0; bit < 11; ++bit)
	{
		integer |= TCE_TriggerBit(&value, 298 + bit) << bit;
	}
	value = factor;
	if (!TCE_ImpactMultiply(&value, gravity, control))
	{
		return 0;
	}
	storedGravity = TCE_RotateStore(value, control);
	*duration = (int)integer;
	*scaledGravity = storedGravity;
	return 1;
}
#endif
#endif
