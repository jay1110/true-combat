#ifndef TCE_STOPWATCH_PRECISION_H
#define TCE_STOPWATCH_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_interpolation_precision.h"

/* FILD elapsed; FMUL the original binary64 reciprocal of 60000.
 * Its significand is 0x1179ec9cbd821e and binary scale is -68. */
static __inline tceTriggerSum_t TCE_StopwatchMinutes(int elapsed, unsigned control)
{
	tceTriggerSum_t value = { { 0 }, 0 }, high = { { 0 }, 0 };
	unsigned magnitude = elapsed < 0 ? 0u - (unsigned)elapsed : (unsigned)elapsed;

	TCE_TriggerPlace(&value, (unsigned long long)magnitude * 0x9cbd821eu, 230);
	TCE_TriggerPlace(&high, (unsigned long long)magnitude * 0x1179ecu, 262);
	TCE_TriggerAddExact(&value, &high);
	value.negative = elapsed < 0;
	TCE_TriggerRound(&value, control);
	return value;
}

/* The original subtracts an exact FILD integer from a held value. */
static __inline void TCE_StopwatchSubtractInt(tceTriggerSum_t *value,
                                             int integer, unsigned control)
{
	tceTriggerSum_t negative = { { 0 }, 0 };
	unsigned magnitude = integer < 0 ? 0u - (unsigned)integer : (unsigned)integer;

	negative.negative = integer >= 0;
	TCE_TriggerPlace(&negative, magnitude, 298);
	TCE_RotateAdd(value, &negative, control);
}

/* Finite masked numerical values only; FP status and unmasked trap timing
 * remain separate. Reject unsupported inputs before writing caller outputs. */
static __inline int TCE_StopwatchFiniteValues(int elapsed, float limitValue,
                                              int limitWhole, int calculateElapsed,
                                              int *minutes, int *seconds,
                                              int *limitSeconds, int *reached)
{
	tceTriggerSum_t limit, fraction, elapsedMinutes;
	unsigned control;
	int whole = 0, second = 0, limitSecond, inTime = 0;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&limit, limitValue, 1.0f, control))
	{
		return 0;
	}
	fraction = limit;
	TCE_StopwatchSubtractInt(&fraction, limitWhole, control);
	TCE_InterpolationMultiply(&fraction, 0x42700000u, control); /* 60 */
	limitSecond = TCE_FixedSigned64Low32(&fraction);
	if (calculateElapsed)
	{
		elapsedMinutes = TCE_StopwatchMinutes(elapsed, control);
		inTime = TCE_TriggerCompare(&elapsedMinutes, &limit) < 0;
		if (inTime)
		{
			whole = TCE_FixedSigned64Low32(&elapsedMinutes);
			fraction = elapsedMinutes;
			TCE_StopwatchSubtractInt(&fraction, whole, control);
			TCE_InterpolationMultiply(&fraction, 0x42700000u, control);
			second = TCE_FixedSigned64Low32(&fraction);
		}
	}
	*limitSeconds = limitSecond;
	if (calculateElapsed)
	{
		*minutes = whole;
		*seconds = second;
	}
	*reached = inTime;
	return 1;
}
#endif
#endif
