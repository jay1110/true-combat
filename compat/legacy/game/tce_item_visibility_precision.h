#ifndef TCE_ITEM_VISIBILITY_PRECISION_H
#define TCE_ITEM_VISIBILITY_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_gust_precision.h"
#include "tce_normalize_precision.h"

/* Consume the shared retained root before rounding the distance snapshot. */
static __inline int TCE_ItemVisibilityNormalize(float *direction,
                                               float *distance, int *tooFar)
{
	tceTriggerSum_t root, boundary;
	float normalized[3];
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteNormalizeVector(direction, normalized, &root, control))
	{
		return 0;
	}
	TCE_TriggerProduct(&boundary, 255.0f, 1.0f, control);
	memcpy(direction, normalized, sizeof(normalized));
	*distance = TCE_RotateStore(root, control);
	*tooFar = TCE_TriggerCompare(&root, &boundary) > 0;
	return 1;
}

/* FILD keeps the signed view height exact until FADD; only the eye Z and
 * the three coordinate differences are stored as Float before normalize. */
static __inline int TCE_ItemVisibilityOrigin(const float *origin, int viewHeight,
                                            const float *target, float *eye,
                                            float *direction)
{
	tceTriggerSum_t height = { { 0 }, 0 }, value, term;
	float storedEye[3], storedDirection[3];
	unsigned control, magnitude;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&value, origin[2], 1.0f, control))
	{
		return 0;
	}
	height.negative = viewHeight < 0;
	magnitude = viewHeight < 0 ? 0u - (unsigned)viewHeight : (unsigned)viewHeight;
	TCE_TriggerPlace(&height, magnitude, 298);
	TCE_RotateAdd(&height, &value, control);
	storedEye[0] = origin[0];
	storedEye[1] = origin[1];
	storedEye[2] = TCE_RotateStore(height, control);
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&value, storedEye[axis], 1.0f, control) ||
		    !TCE_TriggerProduct(&term, target[axis], -1.0f, control))
		{
			return 0;
		}
		TCE_RotateAdd(&value, &term, control);
		storedDirection[axis] = TCE_RotateStore(value, control);
	}
	memcpy(eye, storedEye, sizeof(storedEye));
	memcpy(direction, storedDirection, sizeof(storedDirection));
	return 1;
}

/* The native item cone compares retained Z+Y+X products against
 * -0.94f - (distance * (1.0f / 255.0f)) * 0.057f, without Float spills.
 * Nonfinite operands and unmasked exceptions keep the caller's fallback.
 * Masked floating-point status flags are not reproduced. */
static __inline int TCE_ItemVisibilityCone(const float *facing,
                                          const float *direction,
                                          float distance, int *visible)
{
	tceTriggerSum_t dot, term, threshold, slope;
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&dot, facing[2], direction[2], control))
	{
		return 0;
	}
	for (axis = 1; axis >= 0; --axis)
	{
		if (!TCE_TriggerProduct(&term, facing[axis], direction[axis], control))
		{
			return 0;
		}
		TCE_RotateAdd(&dot, &term, control);
	}
	if (!TCE_TriggerProduct(&term, distance, 1.0f / 255.0f, control))
	{
		return 0;
	}
	TCE_TriggerProduct(&slope, 0.057f, 1.0f, control);
	/* The reciprocal has quantum 2^-31 and slope has quantum 2^-28.
	 * Even a subnormal Float distance leaves the triple product on the
	 * fixed 2^-298 grid; its magnitude is below 2^117. */
	TCE_GustMultiply(&term, &slope, control);
	term.negative ^= 1u;
	TCE_TriggerProduct(&threshold, -0.94f, 1.0f, control);
	TCE_RotateAdd(&threshold, &term, control);
	*visible = TCE_TriggerCompare(&threshold, &dot) >= 0;
	return 1;
}
#endif
#endif
