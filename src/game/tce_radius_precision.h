#ifndef TCE_RADIUS_PRECISION_H
#define TCE_RADIUS_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_gust_precision.h"

/* The native search bounds retain radius * binary64(1.41421356) between
 * all six binary32 stores. MSVC long double cannot hold the PC64 result.
 * This finite path rounds each arithmetic operation with the x87 control
 * settings; masked exception status flags remain outside its contract. */
static __inline int TCE_RadiusFiniteBounds(const float *origin, float *radius,
                                          float *mins, float *maxs)
{
	tceTriggerSum_t expansion, factor = { { 0 }, 0 };
	tceTriggerSum_t coordinates[3], lower, upper, negativeExpansion;
	unsigned control, radiusBits;
	float clampedRadius;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	memcpy(&radiusBits, radius, sizeof(radiusBits));
	if ((radiusBits & 0x7f800000u) == 0x7f800000u)
	{
		return 0;
	}
	clampedRadius = *radius < 1.0f ? 1.0f : *radius;
	/* Preflight every input before changing the caller's radius or bounds. */
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&coordinates[axis], origin[axis], 1.0f, control))
		{
			return 0;
		}
	}
	TCE_TriggerProduct(&expansion, clampedRadius, 1.0f, control);
	/* Original double bits 3ff6a09e65dc27df: significand * 2^-52.
 * Radius >= 1 has quantum >= 2^-23, so the exact product's lowest bit
 * is >= 2^-75. Bounds have quantum >= 2^-149 and magnitude < 2^130;
 * all intermediates fit the existing 2^-298, 576-bit representation. */
	TCE_TriggerPlace(&factor, 0x16a09e65dc27dfULL, 246);
	TCE_GustMultiply(&expansion, &factor, control);
	negativeExpansion = expansion;
	negativeExpansion.negative = 1;
	*radius = clampedRadius;
	for (axis = 0; axis < 3; ++axis)
	{
		lower = coordinates[axis];
		TCE_RotateAdd(&lower, &negativeExpansion, control);
		mins[axis] = TCE_RotateStore(lower, control);
		upper = expansion;
		TCE_RotateAdd(&upper, &coordinates[axis], control);
		maxs[axis] = TCE_RotateStore(upper, control);
	}
	return 1;
}
#endif

#endif
