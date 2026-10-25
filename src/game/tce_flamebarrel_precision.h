#ifndef TCE_FLAMEBARREL_PRECISION_H
#define TCE_FLAMEBARREL_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_interpolation_precision.h"

/* The masked draw fits Float exactly. Keep every native arithmetic stage
 * held until the velocity's Float store; exception status remains separate. */
static __inline int TCE_FlameBarrelVelocityFinite(int draw, float direction,
                                                float *result)
{
	tceTriggerSum_t value, term;
	unsigned bits, control;

	memcpy(&bits, &direction, sizeof(bits));
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) || ((bits >> 23) & 255u) == 255u)
	{
		return 0;
	}
	TCE_TriggerProduct(&value, (float)draw, 3.0518509447574615e-05f, control);
	TCE_TriggerProduct(&term, -0.5f, 1.0f, control);
	TCE_RotateAdd(&value, &term, control);
	term = value;
	TCE_RotateAdd(&value, &term, control);
	TCE_InterpolationMultiply(&value, 0x42c80000u, control); /* 100 */
	TCE_TriggerProduct(&term, 900.0f, 1.0f, control);
	TCE_RotateAdd(&value, &term, control);
	TCE_InterpolationMultiply(&value, bits, control);
	*result = TCE_RotateStore(value, control);
	return 1;
}

/* The original reloads Float, truncates signed64/low32, then FILD32/FSTP32.
 * FILD imports all 32 integer bits before the final Float rounding. */
static __inline float TCE_FlameBarrelSnap(float velocity)
{
	tceTriggerSum_t value = { { 0 }, 0 };
	unsigned control, magnitude;
	int integer;

	__control87_2(0, 0, &control, NULL);
	integer = TCE_TruncateNativeInteger((long double)velocity);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return (float)integer;
	}
	magnitude = integer < 0 ? 0u - (unsigned)integer : (unsigned)integer;
	value.negative = integer < 0;
	TCE_TriggerPlace(&value, magnitude, 298);
	return TCE_RotateStore(value, control);
}
#endif
#endif
