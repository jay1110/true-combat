#ifndef TCE_WEATHER_PRECISION_H
#define TCE_WEATHER_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_smoke_precision.h"

/* Optional height retains the new Z across its Float store, as rain does.
 * Snow uses the stored Z instead and supplies no height/bottom arguments. */
static __inline int TCE_FiniteWeatherMove(int milliseconds, float *position,
                                          const float *velocity,
                                          const float *height, float *bottom)
{
	tceTriggerSum_t elapsed = { { 0 }, 0 }, value, start, heightValue;
	unsigned control, magnitude, velocityBits[3];
	unsigned long long low, high;
	int i;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	for (i = 0; i < 3; ++i)
	{
		memcpy(&velocityBits[i], &velocity[i], sizeof(velocityBits[i]));
		if (((velocityBits[i] >> 23) & 255u) == 255u ||
		    !TCE_TriggerProduct(&start, position[i], 1.0f, control))
		{
			return 0;
		}
	}
	if (height && !TCE_TriggerProduct(&heightValue, *height, 1.0f, control))
	{
		return 0;
	}
	elapsed.negative = milliseconds < 0;
	magnitude = elapsed.negative ? 0u - (unsigned)milliseconds : (unsigned)milliseconds;
	/* The native binary64 .001 is 0x10624dd2f1a9fc * 2^-62. */
	low = (unsigned long long)magnitude * 0xd2f1a9fcu;
	high = (unsigned long long)magnitude * 0x10624du + (low >> 32);
	TCE_TriggerPlace(&elapsed, (unsigned)low, 236);
	TCE_TriggerPlace(&elapsed, high, 268);
	TCE_TriggerRound(&elapsed, control);
	for (i = 0; i < 3; ++i)
	{
		value = elapsed;
		TCE_InterpolationMultiply(&value, velocityBits[i], control);
		TCE_TriggerProduct(&start, position[i], 1.0f, control);
		TCE_RotateAdd(&value, &start, control);
		position[i] = TCE_RotateStore(value, control);
		if (i == 2 && height)
		{
			TCE_RotateAdd(&value, &heightValue, control);
			*bottom = TCE_RotateStore(value, control);
		}
	}
	return 1;
}

/* Weather retains both coordinate differences and adds Y squared first. */
static __inline int TCE_FiniteWeatherOutside(const float *position,
                                             const float *view, int *outside)
{
	tceTriggerSum_t difference[2], value, squared, sum, limit = { { 0 }, 0 };
	unsigned control;
	int i;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	for (i = 0; i < 2; ++i)
	{
		if (!TCE_TriggerProduct(&difference[i], position[i], 1.0f, control) ||
		    !TCE_TriggerProduct(&value, view[i], 1.0f, control))
		{
			return 0;
		}
		value.negative ^= 1u;
		TCE_RotateAdd(&difference[i], &value, control);
	}
	TCE_SmokeSquareDifference(&difference[1], &sum, control);
	TCE_SmokeSquareDifference(&difference[0], &squared, control);
	TCE_RotateAdd(&sum, &squared, control);
	TCE_TriggerPlace(&limit, 1000000u, 298);
	*outside = TCE_TriggerCompare(&sum, &limit) > 0;
	return 1;
}
#endif
#endif
