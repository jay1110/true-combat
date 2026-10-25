#ifndef TCE_FRICTION_PRECISION_H
#define TCE_FRICTION_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_length_precision.h"

/* Windows retains FSQRT for the low-speed gate while storing a separate
 * Float speed. Do not pass that comparison through MSVC's Double return. */
static __inline int TCE_FrictionFiniteSpeed(const float *vector,
                                            volatile float *stored, int *lowSpeed)
{
	tceTriggerSum_t squared, product, root, one;
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&squared, vector[0], vector[0], control))
	{
		return 0;
	}
	for (axis = 1; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&product, vector[axis], vector[axis], control))
		{
			return 0;
		}
		TCE_RotateAdd(&squared, &product, control);
	}
	root = TCE_LengthRoot(&squared, control);
	TCE_TriggerProduct(&one, 1.0f, 1.0f, control);
	*stored = TCE_RotateStore(root, control);
	*lowSpeed = TCE_TriggerCompare(&root, &one) < 0;
	return 1;
}
#endif

#endif
