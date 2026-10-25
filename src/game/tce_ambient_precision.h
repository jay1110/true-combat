#ifndef TCE_AMBIENT_PRECISION_H
#define TCE_AMBIENT_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_gust_precision.h"
#include "tce_interpolation_precision.h"

/* The native ray sampler stores only after scaling, subtracting one half,
 * and doubling. The masked random integer is exactly representable in Float. */
static __inline int TCE_AmbientRoomRandom(unsigned sample, float *result)
{
	tceTriggerSum_t value, half, doubled;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (sample > 32767u || !TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	TCE_TriggerProduct(&value, (float)sample, 1.0f / 32767.0f, control);
	TCE_TriggerProduct(&half, -0.5f, 1.0f, control);
	TCE_RotateAdd(&value, &half, control);
	doubled = value;
	TCE_RotateAdd(&value, &doubled, control);
	*result = TCE_RotateStore(value, control);
	return 1;
}

static __inline int TCE_AmbientOrigin(float origin, float axis, int subtract,
                                     float *result)
{
	tceTriggerSum_t value, start;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&value, axis, 64.0f, control) ||
	    !TCE_TriggerProduct(&start, origin, 1.0f, control))
	{
		return 0;
	}
	value.negative ^= subtract != 0;
	TCE_RotateAdd(&value, &start, control);
	*result = TCE_RotateStore(value, control);
	return 1;
}

static __inline int TCE_AmbientBlend(int elapsed, float *result)
{
	tceTriggerSum_t value = { { 0 }, 0 };
	unsigned control, magnitude;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	value.negative = elapsed < 0;
	magnitude = value.negative ? 0u - (unsigned)elapsed : (unsigned)elapsed;
	TCE_TriggerPlace(&value, (unsigned long long)magnitude * 0x83126fu, 265);
	TCE_TriggerRound(&value, control);
	*result = TCE_RotateStore(value, control);
	return 1;
}

/* All Windows gains come from stored Float fades of a signed32 clock.
 * The bound leaves room for every such gain; larger inputs use the caller's
 * floating-point fallback rather than exceeding fixed arithmetic capacity. */
static __inline int TCE_AmbientVolume(int amount, float deafness, float gain,
                                     int *result)
{
	tceTriggerSum_t hearing, attenuation, volume = { { 0 }, 0 };
	unsigned control, gainBits, magnitude;

	memcpy(&gainBits, &gain, sizeof(gainBits));
	if ((gainBits & 0x7fffffffu) > 0x4b800000u)
	{
		return 0;
	}
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&hearing, 1.0f, 1.0f, control) ||
	    !TCE_TriggerProduct(&attenuation, deafness, 1.0f, control))
	{
		return 0;
	}
	attenuation.negative ^= 1u;
	TCE_RotateAdd(&hearing, &attenuation, control);
	volume.negative = amount < 0;
	magnitude = volume.negative ? 0u - (unsigned)amount : (unsigned)amount;
	TCE_TriggerPlace(&volume, magnitude, 298);
	TCE_GustMultiply(&hearing, &volume, control);
	TCE_InterpolationMultiply(&hearing, gainBits, control);
	*result = TCE_FixedSigned64Low32(&hearing);
	return 1;
}
#endif
#endif
