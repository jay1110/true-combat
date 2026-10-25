#ifndef TCE_MORTAR_PRECISION_H
#define TCE_MORTAR_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "../game/tce_ratio_precision.h"

/* Preserve the held division and addition before the final Float store.
 * The caller retains native division for zero and unmasked exceptions;
 * this finite path does not reproduce floating-point status flags. */
static __inline int TCE_MortarLightFinite(int sample, float *result)
{
	tceTriggerSum_t value, offset;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (sample <= 0 || sample > 31 || !TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	value = TCE_RatioValue(8u, (unsigned)sample, 0, 0, control);
	TCE_TriggerProduct(&offset, 0.75f, 1.0f, control);
	TCE_RotateAdd(&value, &offset, control);
	*result = TCE_RotateStore(value, control);
	return 1;
}
#endif
#endif
