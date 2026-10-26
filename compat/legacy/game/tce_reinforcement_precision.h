#ifndef TCE_REINFORCEMENT_PRECISION_H
#define TCE_REINFORCEMENT_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_trigger_precision.h"
#include "tce_numeric.h"

/* FILD milliseconds; FMUL binary32 .001; FADD one; __ftol, with no
 * binary32 store between the arithmetic and integer conversion.
 * Exception flags and unmasked traps are not reproduced. */
static __inline int TCE_ReinforcementSecondsLow32(int milliseconds)
{
	tceTriggerSum_t value = { { 0 }, 0 }, one = { { 0 }, 0 };
	unsigned magnitude, control, low = 0;
	int bit, result;

	value.negative = milliseconds < 0;
	magnitude = value.negative ? 0u - (unsigned)milliseconds :
	                            (unsigned)milliseconds;
	/* .001f is exactly 0x83126f * 2^-33. */
	TCE_TriggerPlace(&value, (unsigned long long)magnitude * 0x83126fu, 265);
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return TCE_TruncateNativeInteger((long double)milliseconds * (long double)0.001f +
		                                  1.0L);
	}
	TCE_TriggerRound(&value, control);
	TCE_TriggerPlace(&one, 1u, 298);
	TCE_TriggerAdd(&value, &one, control);

	/* Every int32 input yields a result strictly within signed32 range.
	 * Discard fractions toward zero only after both native arithmetic steps. */
	for (bit = 0; bit < 32; ++bit)
	{
		low |= TCE_TriggerBit(&value, 298 + bit) << bit;
	}
	if (value.negative)
	{
		low = 0u - low;
	}
	memcpy(&result, &low, sizeof(result));
	return result;
}
#endif
#endif
