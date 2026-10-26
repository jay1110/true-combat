#ifndef TCE_TRACEMAP_PRECISION_H
#define TCE_TRACEMAP_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_ratio_precision.h"

typedef struct
{
	unsigned long long divisor;
	int shift;
	unsigned negative, control;
	tceTriggerSum_t minimum;
} tceTraceMapScale_t;

/* Preserve held 254 / signed-delta, then sample / scale + signed-min.
 * Loaded nonsentinel samples are bytes 1..255. Exception status is separate. */
static __inline int TCE_TraceMapPrepareScale(int delta, int minimum,
                                            tceTraceMapScale_t *state)
{
	tceTriggerSum_t scale;
	unsigned magnitude;
	int highest, drop, bit;

	__control87_2(0, 0, &state->control, NULL);
	if (!TCE_FiniteExceptionsMasked(state->control))
	{
		return 0;
	}
	magnitude = delta < 0 ? 0u - (unsigned)delta : (unsigned)delta;
	scale = delta ? TCE_RatioValue(254, magnitude, 0, delta < 0, state->control) :
	                TCE_RatioValue(1, 1, 0, 0, state->control);
	for (highest = 575; highest >= 0 && !TCE_TriggerBit(&scale, highest); --highest)
	{
	}
	drop = highest > 63 ? highest - 63 : 0;
	state->divisor = 0;
	for (bit = 0; bit < 64; ++bit)
	{
		state->divisor |= (unsigned long long)TCE_TriggerBit(&scale, drop + bit) << bit;
	}
	state->shift = 298 - drop;
	state->negative = scale.negative;
	memset(&state->minimum, 0, sizeof(state->minimum));
	state->minimum.negative = minimum < 0;
	magnitude = minimum < 0 ? 0u - (unsigned)minimum : (unsigned)minimum;
	/* FILD/FIADD imports the complete int32, without an early PC rounding. */
	TCE_TriggerPlace(&state->minimum, magnitude, 298);
	return 1;
}

static __inline int TCE_TraceMapFiniteHeight(const tceTraceMapScale_t *state,
                                            float sample, float *height)
{
	tceTriggerSum_t value;
	unsigned byteValue;

	if (!(sample >= 1.0f && sample <= 255.0f))
	{
		return 0;
	}
	byteValue = (unsigned)sample;
	if ((float)byteValue != sample)
	{
		return 0;
	}
	value = TCE_RatioValue(byteValue, state->divisor, state->shift,
	                       state->negative, state->control);
	TCE_RotateAdd(&value, &state->minimum, state->control);
	*height = TCE_RotateStore(value, state->control);
	return 1;
}

/* (max-min) and the reciprocal both stay in x87 precision; the power-of-two
 * multiplication by 1/256 is exact and does not narrow the difference. */
static __inline int TCE_TraceMapFiniteGrid(float maximum, float minimum,
                                          float *inverseGrid)
{
	tceTriggerSum_t span, lower, result;
	unsigned control;
	unsigned long long divisor = 0;
	int highest, drop, bit;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&span, maximum, 1.0f, control) ||
	    !TCE_TriggerProduct(&lower, minimum, 1.0f, control))
	{
		return 0;
	}
	lower.negative ^= 1u;
	TCE_RotateAdd(&span, &lower, control);
	for (highest = 575; highest >= 0 && !TCE_TriggerBit(&span, highest); --highest)
	{
	}
	if (highest < 0)
	{
		return 0;
	}
	drop = highest > 63 ? highest - 63 : 0;
	for (bit = 0; bit < 64; ++bit)
	{
		divisor |= (unsigned long long)TCE_TriggerBit(&span, drop + bit) << bit;
	}
	result = TCE_RatioValue(1, divisor, 306 - drop, span.negative, control);
	*inverseGrid = TCE_RotateStore(result, control);
	return 1;
}
#endif
#endif
