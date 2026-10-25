#ifndef TCE_BOT_ANGLE_PRECISION_H
#define TCE_BOT_ANGLE_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_interpolation_precision.h"

/* Windows AngleDifference returns its rounded difference in ST0 directly to
 * signed64 truncation. Only a single +/-360 correction is performed.
 * Exception flags and nonfinite operands retain separate caller handling. */
static __inline int TCE_FiniteBotAngleValue(float first, float second,
                                            tceTriggerSum_t *result,
                                            unsigned control)
{
	tceTriggerSum_t left, right, difference, boundary, correction;

	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	if (!TCE_TriggerProduct(&left, first, 1.0f, control) ||
	    !TCE_TriggerProduct(&right, second, 1.0f, control))
	{
		return 0;
	}
	difference = left;
	right.negative ^= 1u;
	TCE_RotateAdd(&difference, &right, control);
	right.negative ^= 1u;
	TCE_TriggerProduct(&boundary, 180.0f, 1.0f, control);
	TCE_TriggerProduct(&correction, 360.0f, 1.0f, control);
	if (TCE_TriggerCompare(&left, &right) > 0)
	{
		if (TCE_TriggerCompare(&difference, &boundary) > 0)
		{
			correction.negative = 1;
			TCE_RotateAdd(&difference, &correction, control);
		}
	}
	else
	{
		boundary.negative = 1;
		if (TCE_TriggerCompare(&difference, &boundary) < 0)
		{
			TCE_RotateAdd(&difference, &correction, control);
		}
	}
	*result = difference;
	return 1;
}

/* Keep the difference through the absolute-value and sign gates. The chosen
 * boundary is rounded at the active precision before its Float argument store. */
static __inline int TCE_FiniteFirearcArgument(float first, float second,
                                             float arc, int *outside,
                                             float *argument)
{
	tceTriggerSum_t difference, magnitude, boundary, start;
	tceTriggerSum_t zero = { { 0 }, 0 };
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_FiniteBotAngleValue(first, second, &difference, control) ||
	    !TCE_TriggerProduct(&boundary, arc, 1.0f, control) ||
	    !TCE_TriggerProduct(&start, first, 1.0f, control))
	{
		return 0;
	}
	magnitude = difference;
	magnitude.negative = 0;
	if (TCE_TriggerCompare(&boundary, &magnitude) >= 0)
	{
		*outside = 0;
		return 1;
	}
	if (TCE_TriggerCompare(&difference, &zero) > 0)
	{
		boundary.negative ^= 1u;
	}
	TCE_RotateAdd(&start, &boundary, control);
	*argument = TCE_RotateStore(start, control);
	*outside = 1;
	return 1;
}

static __inline int TCE_FiniteBotAngleLow32(float first, float second, int *result)
{
	tceTriggerSum_t difference;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteBotAngleValue(first, second, &difference, control))
	{
		return 0;
	}
	*result = TCE_FixedSigned64Low32(&difference);
	return 1;
}

/* Clamp the retained movement, then round its sum with the current angle
 * before the final normalization call. Negative speeds are not sanitized. */
static __inline int TCE_FiniteBotViewArgument(float current, float ideal,
                                              float speed, float *result)
{
	tceTriggerSum_t move, boundary, start, zero = { { 0 }, 0 };
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_TriggerProduct(&boundary, speed, 1.0f, control) ||
	    !TCE_TriggerProduct(&start, current, 1.0f, control) ||
	    !TCE_FiniteBotAngleValue(ideal, current, &move, control))
	{
		return 0;
	}
	if (TCE_TriggerCompare(&move, &zero) > 0)
	{
		if (TCE_TriggerCompare(&move, &boundary) > 0)
		{
			move = boundary;
		}
	}
	else
	{
		boundary.negative ^= 1u;
		if (TCE_TriggerCompare(&move, &boundary) < 0)
		{
			move = boundary;
		}
	}
	TCE_RotateAdd(&move, &start, control);
	*result = TCE_RotateStore(move, control);
	return 1;
}

/* The original stores the candidate speed before comparing the retained
 * product to the Float maximum. This helper preserves both value stages. */
static __inline int TCE_FiniteBotAngleSpeed(float first, float second,
                                            float factor, float maximum,
                                            float *result)
{
	tceTriggerSum_t value, boundary;
	unsigned control, factorBits;

	memcpy(&factorBits, &factor, sizeof(factorBits));
	if (((factorBits >> 23) & 255u) == 255u)
	{
		return 0;
	}
	__control87_2(0, 0, &control, NULL);
	if (!TCE_TriggerProduct(&boundary, maximum, 1.0f, control) ||
	    !TCE_FiniteBotAngleValue(first, second, &value, control))
	{
		return 0;
	}
	value.negative = 0;
	TCE_InterpolationMultiply(&value, factorBits, control);
	*result = TCE_RotateStore(value, control);
	if (TCE_TriggerCompare(&value, &boundary) > 0)
	{
		*result = maximum;
	}
	return 1;
}
#endif
#endif
