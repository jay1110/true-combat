#ifndef TCE_ENDFRAME_PRECISION_H
#define TCE_ENDFRAME_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_rotate_precision.h"
#include "tce_numeric.h"

/* FILD imports the integer exactly; only the following FADD rounds to PC.
 * Masked finite numerical results are supported, not exception status flags. */
static __inline int TCE_EndFrameFiniteSum(int integer, float scalar,
                                         tceTriggerSum_t *sum, unsigned *control)
{
	tceTriggerSum_t value = { { 0 }, 0 };
	unsigned magnitude;

	__control87_2(0, 0, control, NULL);
	if (!TCE_FiniteExceptionsMasked(*control) ||
	    !TCE_TriggerProduct(sum, scalar, 1.0f, *control))
	{
		return 0;
	}
	magnitude = integer < 0 ? 0u - (unsigned)integer : (unsigned)integer;
	value.negative = integer < 0;
	TCE_TriggerPlace(&value, magnitude, 298);
	TCE_RotateAdd(sum, &value, *control);
	return 1;
}

static __inline int TCE_EndFrameSkillTotal(int total, float points)
{
	tceTriggerSum_t sum;
	unsigned control;

	if (TCE_EndFrameFiniteSum(total, points, &sum, &control))
	{
		/* Native __ftol converts to signed64, then the caller stores EAX. */
		return TCE_FixedSigned64Low32(&sum);
	}
	return TCE_TruncateNativeInteger((double)total + (double)points);
}

static __inline float TCE_EndFramePausedClock(int delta, float clock)
{
	tceTriggerSum_t sum;
	unsigned control;

	if (TCE_EndFrameFiniteSum(delta, clock, &sum, &control))
	{
		return TCE_RotateStore(sum, control);
	}
	return (float)((long double)delta + (long double)clock);
}
#endif
#endif
