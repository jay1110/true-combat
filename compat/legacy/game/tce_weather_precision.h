#ifndef TCE_WEATHER_PRECISION_H
#define TCE_WEATHER_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_smoke_precision.h"

/* Integer-only bounds keep the common world-coordinate test off the wide
 * arithmetic path without adding floating-point status effects. */
static __inline int TCE_WeatherCoordinateQ4(const float *coordinate, int *value)
{
	unsigned bits, magnitude, exponent, integer;
	memcpy(&bits, coordinate, sizeof(bits));
	magnitude = bits & 0x7fffffffu;
	if (magnitude > 0x47800000u) /* finite |coordinate| <= 65536 */
	{
		return 0;
	}
	exponent = magnitude >> 23;
	/* Values below 1/16, including subnormals, truncate to zero. */
	integer = exponent <= 122u ? 0u :
	          ((magnitude & 0x7fffffu) | 0x800000u) >> (146u - exponent);
	*value = (bits >> 31) ? -(int)integer : (int)integer;
	return 1;
}

static __inline int TCE_WeatherOutsideFast(const float *position,
                                           const float *view, int *outside)
{
	unsigned long long lower = 0, upper = 0;
	int axis;
	for (axis = 0; axis < 2; ++axis)
	{
		int p, v, difference;
		unsigned magnitude, low, high;
		if (!TCE_WeatherCoordinateQ4(&position[axis], &p) ||
		    !TCE_WeatherCoordinateQ4(&view[axis], &v))
		{
			return 0;
		}
		difference = p - v;
		magnitude = difference < 0 ? (unsigned)-difference : (unsigned)difference;
		/* Q4 truncation loses <2 units across the subtraction; worst PC24
		 * directed rounding adds <=1/4 unit. Three units enclose both. */
		low = magnitude > 3u ? magnitude - 3u : 0u;
		high = magnitude + 3u;
		lower += (unsigned long long)low * low;
		upper += (unsigned long long)high * high;
	}
	/* |delta| <= 131072. At PC24 each squared value loses <=2048 world
	 * units squared, and their sum <=4096; finer PC modes lose less.
	 * Total 8192 becomes 2^21 in Q4 squared. Padded sums fit 44 bits. */
	if (upper + 2097152ull <= 256000000ull)
	{
		*outside = 0;
		return 1;
	}
	if (lower > 256000000ull + 2097152ull)
	{
		*outside = 1;
		return 1;
	}
	return 0;
}

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
	if (TCE_WeatherOutsideFast(position, view, outside))
	{
		return 1;
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
