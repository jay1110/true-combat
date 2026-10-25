#ifndef TCE_TRAIN_PRECISION_H
#define TCE_TRAIN_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_rotate_precision.h"
#include "tce_numeric.h"

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
