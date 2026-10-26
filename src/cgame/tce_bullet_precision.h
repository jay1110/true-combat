#ifndef TCE_BULLET_PRECISION_H
#define TCE_BULLET_PRECISION_H

#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "../game/tce_length_precision.h"
#include "../game/tce_ambient_precision.h"

/* Q_crandom(seed)*100 has at most 20 significant bits. Both spread inputs
 * are therefore exact Float values even though the second native result
 * stays on the x87 stack. Round each subsequent operation at current PC/RC. */
static __inline int TCE_BulletAntilagEnd(const float *forward, const float *right,
                                        const float *up, const float *muzzle,
                                        float horizontal, float vertical, float *end)
{
	tceTriggerSum_t spread, rise, distance, origin;
	float result[3];
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&spread, right[axis], horizontal, control) ||
		    !TCE_TriggerProduct(&rise, vertical, up[axis], control) ||
		    !TCE_TriggerProduct(&distance, forward[axis], 8192.0f, control) ||
		    !TCE_TriggerProduct(&origin, muzzle[axis], 1.0f, control))
		{
			return 0;
		}
		TCE_RotateAdd(&spread, &rise, control);
		TCE_RotateAdd(&spread, &distance, control);
		TCE_RotateAdd(&spread, &origin, control);
		result[axis] = TCE_RotateStore(spread, control);
	}
	memcpy(end, result, sizeof(result));
	return 1;
}

/* Windows stores each root*direction product before adding the impact point.
 * Keep the root in the existing integer representation instead of narrowing
 * it through MSVC's binary64 long double return. Commit only complete output. */
static __inline int TCE_BulletProjectedOffset(const float *delta,
                                             const float *direction, float *offset)
{
	tceTriggerSum_t squared, term, root, factor, product;
	float result[3];
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&squared, delta[0], delta[0], control))
	{
		return 0;
	}
	for (axis = 1; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&term, delta[axis], delta[axis], control))
		{
			return 0;
		}
		TCE_RotateAdd(&squared, &term, control);
	}
	root = TCE_LengthRoot(&squared, control);
	for (axis = 0; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&factor, direction[axis], 1.0f, control) ||
		    !TCE_AmbientProductFits(&root, &factor))
		{
			return 0;
		}
		product = root;
		TCE_GustMultiply(&product, &factor, control);
		result[axis] = TCE_RotateStore(product, control);
	}
	memcpy(offset, result, sizeof(result));
	return 1;
}
#endif
#endif
