#ifndef TCE_TRAIN_PRECISION_H
#define TCE_TRAIN_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_length_precision.h"
#include "tce_interpolation_precision.h"

typedef char tceTrainFramesLayout_t[(sizeof(double) == 8 && DBL_MANT_DIG == 53 &&
                                     DBL_MAX_EXP == 1024) ? 1 : -1];

static __inline float TCE_TrainMoveComponent(float destination, float source)
{
	float stored;
	if (TCE_FiniteDifferenceStore(destination, source, &stored))
	{
		return stored;
	}
	return destination - source;
}

static __inline float TCE_TrainTargetAngle(float current, float rotation)
{
	float stored;
	if (TCE_MuzzleFiniteComponent(current, 1.0f, rotation, 0, &stored))
	{
		return stored;
	}
	return current + rotation;
}

/* The floor result is an exact small integer, or the exact Float 0.001.
 * Keep rotation/frames held before multiplying by ten and storing Float. */
static __inline int TCE_TrainAngularVelocityFinite(float rotation, double frames,
                                                  float *result)
{
	tceTriggerSum_t velocity;
	unsigned rotationBits, exponent, mantissa, control, negative;
	unsigned long long frameBits, magnitude, denominator;
	int scale;

	memcpy(&rotationBits, &rotation, sizeof(rotationBits));
	memcpy(&frameBits, &frames, sizeof(frameBits));
	exponent = (rotationBits >> 23) & 255u;
	magnitude = frameBits & 0x7fffffffffffffffULL;
	/* This interval contains every original nonzero duration/100 and 0.001. */
	if (exponent == 255u || magnitude < 0x3ef0000000000000ULL ||
	    magnitude > 0x41f0000000000000ULL)
	{
		return 0;
	}
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	mantissa = (rotationBits & 0x7fffffu) | (exponent ? 0x800000u : 0u);
	denominator = (frameBits & 0xfffffffffffffULL) | 0x10000000000000ULL;
	scale = (exponent ? (int)exponent - 150 : -149)
	      - ((int)((frameBits >> 52) & 2047u) - 1075);
	negative = (rotationBits >> 31) ^ (unsigned)(frameBits >> 63);
	velocity = TCE_RatioValue(mantissa, denominator, scale, negative, control);
	TCE_InterpolationMultiply(&velocity, 0x41200000u, control); /* 10 */
	*result = TCE_RotateStore(velocity, control);
	return 1;
}

/* Keep the length through FSQRT, multiplication and division before __ftol.
 * Rotating trains still calculate the length when duration is overridden. */
static __inline int TCE_TrainDurationFinite(const float move[3], float speed,
                                           float override, int *result)
{
	tceTriggerSum_t squared, product, duration, quotient;
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&squared, move[0], move[0], control))
	{
		return 0;
	}
	for (axis = 1; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&product, move[axis], move[axis], control))
		{
			return 0;
		}
		TCE_RotateAdd(&squared, &product, control);
	}
	duration = TCE_LengthRoot(&squared, control);
	if (!TCE_FloatIsZeroOrNaN(override))
	{
		if (!TCE_TriggerProduct(&duration, override, 1000.0f, control))
		{
			return 0;
		}
	}
	else
	{
		TCE_InterpolationMultiply(&duration, 0x447a0000u, control); /* 1000 */
		if (!TCE_FixedDivideFloat(&duration, speed, control, &quotient))
		{
			return 0;
		}
		duration = quotient;
	}
	*result = TCE_FixedSigned64Low32(&duration);
	return 1;
}

/* FMUL Float 1000; FIADD exact server time; truncate to signed64/low32.
 * The finite path preserves numeric rounding, not FP exception status. */
static __inline int TCE_TrainWaitDeadline(float wait, int time)
{
	tceTriggerSum_t deadline, clock = { { 0 }, 0 };
	unsigned control, magnitude;

	__control87_2(0, 0, &control, NULL);
	if (TCE_FiniteExceptionsMasked(control) &&
	    TCE_TriggerProduct(&deadline, wait, 1000.0f, control))
	{
		magnitude = time < 0 ? 0u - (unsigned)time : (unsigned)time;
		clock.negative = time < 0;
		TCE_TriggerPlace(&clock, magnitude, 298);
		TCE_RotateAdd(&deadline, &clock, control);
		return TCE_FixedSigned64Low32(&deadline);
	}
	return TCE_TruncateNativeInteger((long double)wait * 1000.0L + time);
}
#endif
#endif
