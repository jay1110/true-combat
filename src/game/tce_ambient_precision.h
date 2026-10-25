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

/* The retained multiplier shifts an exact product right by 298 bits. Check
 * that this discards only zeros and leaves room for the following addition. */
static __inline int TCE_AmbientProductFits(const tceTriggerSum_t *a,
                                         const tceTriggerSum_t *b)
{
	int lowA, lowB, highA, highB;

	for (lowA = 0; lowA < 576 && !TCE_TriggerBit(a, lowA); ++lowA)
	{
	}
	for (lowB = 0; lowB < 576 && !TCE_TriggerBit(b, lowB); ++lowB)
	{
	}
	if (lowA == 576 || lowB == 576)
	{
		return 1;
	}
	for (highA = 575; !TCE_TriggerBit(a, highA); --highA)
	{
	}
	for (highB = 575; !TCE_TriggerBit(b, highB); --highB)
	{
	}
	return lowA + lowB >= 298 && highA + highB <= 870;
}

static __inline int TCE_AmbientReflect(float *direction, const float *normal)
{
	tceTriggerSum_t dot, term, doubled, value;
	tceTriggerSum_t normals[3], starts[3];
	float reflected[3];
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&normals[axis], normal[axis], 1.0f, control) ||
		    !TCE_TriggerProduct(&starts[axis], direction[axis], 1.0f, control))
		{
			return 0;
		}
	}
	TCE_TriggerProduct(&dot, normal[2], direction[2], control);
	TCE_TriggerProduct(&term, normal[1], direction[1], control);
	TCE_RotateAdd(&dot, &term, control);
	TCE_TriggerProduct(&term, normal[0], direction[0], control);
	TCE_RotateAdd(&dot, &term, control);
	doubled = dot;
	TCE_RotateAdd(&dot, &doubled, control);
	dot.negative ^= 1u;
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_AmbientProductFits(&dot, &normals[axis]))
		{
			return 0;
		}
		value = dot;
		TCE_GustMultiply(&value, &normals[axis], control);
		TCE_RotateAdd(&value, &starts[axis], control);
		reflected[axis] = TCE_RotateStore(value, control);
	}
	for (axis = 0; axis < 3; ++axis)
	{
		direction[axis] = reflected[axis];
	}
	return 1;
}

/* X retains high-low through scaling; Y/Z first store that difference.
 * Both scaled terms are stored separately before their final Float sum. */
static __inline int TCE_AmbientRoomExtent(float previous, float high, float low,
                                         int axis, float *result)
{
	tceTriggerSum_t difference, start, term;
	unsigned control;
	float stored;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&difference, high, 1.0f, control) ||
	    !TCE_TriggerProduct(&term, low, -1.0f, control) ||
	    !TCE_TriggerProduct(&start, previous, .6667f, control))
	{
		return 0;
	}
	TCE_RotateAdd(&difference, &term, control);
	if (axis != 0)
	{
		stored = TCE_RotateStore(difference, control);
		if (!TCE_TriggerProduct(&difference, stored, 1.0f, control))
		{
			return 0;
		}
	}
	TCE_TriggerProduct(&term, .3333f, 1.0f, control);
	TCE_GustMultiply(&difference, &term, control);
	stored = TCE_RotateStore(difference, control);
	if (!TCE_TriggerProduct(&difference, stored, 1.0f, control))
	{
		return 0;
	}
	stored = TCE_RotateStore(start, control);
	TCE_TriggerProduct(&start, stored, 1.0f, control);
	TCE_RotateAdd(&start, &difference, control);
	*result = TCE_RotateStore(start, control);
	return 1;
}

/* Distance and hardness keep both scaled products until their final sum. */
static __inline int TCE_AmbientRoomBlend(float previous, float sample,
                                        float scale, float *result)
{
	tceTriggerSum_t incoming, start, coefficient;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&incoming, sample, scale, control) ||
	    !TCE_TriggerProduct(&start, previous, .6667f, control))
	{
		return 0;
	}
	TCE_TriggerProduct(&coefficient, .3333f, 1.0f, control);
	if (!TCE_AmbientProductFits(&incoming, &coefficient))
	{
		return 0;
	}
	TCE_GustMultiply(&incoming, &coefficient, control);
	TCE_RotateAdd(&incoming, &start, control);
	*result = TCE_RotateStore(incoming, control);
	return 1;
}

/* Weight zero remains held after its Float store, including the default-room
 * increment. Other weights are reloaded from Float for the winner comparisons. */
static __inline int TCE_AmbientRoomClassify(float distance, float hardness,
                                          const float *extent, int sky,
                                          float *skyState, float *weights,
                                          int *environment)
{
	tceTriggerSum_t incoming = { { 0 }, 0 }, start, coefficient, term;
	tceTriggerSum_t extentSum, limit, firstWeight, winner;
	float storedWeights[4], storedSky;
	unsigned control, magnitude;
	int axis, kind, selected = 0;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&extentSum, extent[0], 1.0f, control) ||
	    !TCE_TriggerProduct(&term, extent[1], 1.0f, control))
	{
		return 0;
	}
	TCE_RotateAdd(&extentSum, &term, control);
	if (!TCE_TriggerProduct(&term, extent[2], 1.0f, control) ||
	    !TCE_TriggerProduct(&term, distance, 1.0f, control) ||
	    !TCE_TriggerProduct(&term, hardness, 1.0f, control) ||
	    !TCE_TriggerProduct(&start, *skyState, .6667f, control))
	{
		return 0;
	}
	TCE_TriggerProduct(&coefficient, .3333f, 1.0f, control);
	incoming.negative = sky < 0;
	magnitude = sky < 0 ? 0u - (unsigned)sky : (unsigned)sky;
	TCE_TriggerPlace(&incoming, magnitude, 298);
	TCE_GustMultiply(&incoming, &coefficient, control);
	TCE_RotateAdd(&incoming, &start, control);
	storedSky = TCE_RotateStore(incoming, control);
	for (axis = 0; axis < 4; ++axis)
	{
		if (!TCE_TriggerProduct(&term, weights[axis], .6667f, control))
		{
			return 0;
		}
		if (axis == 0)
		{
			firstWeight = term;
		}
		storedWeights[axis] = TCE_RotateStore(term, control);
	}
	TCE_TriggerProduct(&limit, 600.0f, 1.0f, control);
	if (distance < 200.0f && hardness > .8f)
	{
		kind = 1;
	}
	else if (storedSky >= 1.0f && distance > 400.0f)
	{
		kind = 3;
	}
	else if (extent[2] < 150.0f &&
	         TCE_TriggerCompare(&extentSum, &limit) > 0 && hardness > .8f)
	{
		kind = 2;
	}
	else
	{
		kind = 0;
	}
	if (kind == 0)
	{
		TCE_RotateAdd(&firstWeight, &coefficient, control);
		storedWeights[0] = TCE_RotateStore(firstWeight, control);
	}
	else
	{
		TCE_TriggerProduct(&term, storedWeights[kind], 1.0f, control);
		TCE_RotateAdd(&term, &coefficient, control);
		storedWeights[kind] = TCE_RotateStore(term, control);
	}
	winner = firstWeight;
	for (axis = 1; axis < 4; ++axis)
	{
		TCE_TriggerProduct(&term, storedWeights[axis], 1.0f, control);
		if (TCE_TriggerCompare(&term, &winner) > 0)
		{
			winner = term;
			selected = axis == 2 ? 0 : axis;
		}
	}
	*skyState = storedSky;
	for (axis = 0; axis < 4; ++axis)
	{
		weights[axis] = storedWeights[axis];
	}
	*environment = selected;
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
