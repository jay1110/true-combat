#ifndef TCE_TRAIN_PRECISION_H
#define TCE_TRAIN_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_length_precision.h"
#include "tce_interpolation_precision.h"

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
