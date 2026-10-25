#ifndef TCE_RADIUS_PRECISION_H
#define TCE_RADIUS_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_gust_precision.h"
#include "tce_length_precision.h"

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

/* X keeps its sum in the register; Y and Z store/reload binary32 sums
 * before all three axes multiply by the original binary64 one-half. */
static __inline int TCE_RadiusFiniteMidpoint(const float *mins, const float *maxs,
                                            float *midpoint)
{
	tceTriggerSum_t sums[3], other, half = { { 0 }, 0 };
	float spilled[3], halfY, halfZ;
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&sums[axis], maxs[axis], 1.0f, control) ||
		    !TCE_TriggerProduct(&other, mins[axis], 1.0f, control))
		{
			return 0;
		}
		TCE_RotateAdd(&sums[axis], &other, control);
		if (axis)
		{
			spilled[axis] = TCE_RotateStore(sums[axis], control);
			if (!TCE_TriggerProduct(&sums[axis], spilled[axis], 1.0f, control))
			{
				/* Preserve the prior FP path for an overflowing Y/Z sum. */
				return 0;
			}
		}
	}
	/* Finite Float sums have quantum >= 2^-149 and magnitude < 2^129.
 * Halving changes the lowest possible bit to 2^-150, within the grid. */
	TCE_TriggerPlace(&half, 1, 297);
	TCE_GustMultiply(&sums[0], &half, control);
	TCE_GustMultiply(&sums[1], &half, control);
	halfY = TCE_RotateStore(sums[1], control);
	TCE_GustMultiply(&sums[2], &half, control);
	midpoint[1] = halfY;
	halfZ = TCE_RotateStore(sums[2], control);
	midpoint[0] = TCE_RotateStore(sums[0], control);
	midpoint[2] = halfZ;
	return 1;
}

/* VectorLength retains sqrt((X*X + Y*Y) + Z*Z) in ST0. The close gate
 * compares against a separately rounded radius * binary32(0.2), also held. */
static __inline int TCE_RadiusFiniteLengthGate(const float *delta, float radius,
                                              int closeGate, int *accepted)
{
	tceTriggerSum_t squared, product, root, boundary;
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	if (!TCE_TriggerProduct(&squared, delta[0], delta[0], control))
	{
		return 0;
	}
	for (axis = 1; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&product, delta[axis], delta[axis], control))
		{
			return 0;
		}
		TCE_RotateAdd(&squared, &product, control);
	}
	if (!TCE_TriggerProduct(&boundary, radius, closeGate ? 0.2f : 1.0f, control))
	{
		return 0;
	}
	root = TCE_LengthRoot(&squared, control);
	*accepted = TCE_TriggerCompare(&root, &boundary) < 0;
	return 1;
}
#endif

#endif
