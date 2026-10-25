#ifndef TCE_IMPACT_PRECISION_H
#define TCE_IMPACT_PRECISION_H

#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_gust_precision.h"

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
#endif
#endif
