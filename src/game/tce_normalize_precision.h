#ifndef TCE_NORMALIZE_PRECISION_H
#define TCE_NORMALIZE_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_length_precision.h"
#include "tce_gust_precision.h"

/* Windows retained FSQRT, PC-rounded reciprocal and three Float stores.
 * Outputs are committed together after all finite/grid checks succeed.
 * Floating exception status and unmasked traps are not reproduced. */
static __inline int TCE_FiniteNormalizeVector(const float *direction, float *output,
                                              tceTriggerSum_t *length, unsigned control)
{
	tceTriggerSum_t squared, term, root, inverse;
	unsigned long long significand = 0;
	float normalized[3];
	int axis, bit, highest, drop, lowInverse, highInverse, lowTerm, highTerm;

	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&squared, direction[0], direction[0], control))
	{
		return 0;
	}
	for (axis = 1; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&term, direction[axis], direction[axis], control))
		{
			return 0;
		}
		TCE_RotateAdd(&squared, &term, control);
	}
	root = TCE_LengthRoot(&squared, control);
	for (highest = 575; highest >= 0 && !TCE_TriggerBit(&root, highest); --highest)
	{
	}
	memcpy(normalized, direction, sizeof(normalized));
	if (highest >= 0)
	{
		/* The root has at most 64 significant bits on the 2^-298 grid.
		 * Extract it exactly for 1/root, without MSVC's Double return. */
		drop = highest > 63 ? highest - 63 : 0;
		for (bit = 0; bit < 64; ++bit)
		{
			significand |= (unsigned long long)TCE_TriggerBit(&root, drop + bit) << bit;
		}
		inverse = TCE_RatioValue(1, significand, 298 - drop, 0, control);
		for (lowInverse = 0; lowInverse < 576 && !TCE_TriggerBit(&inverse, lowInverse); ++lowInverse)
		{
		}
		for (highInverse = 575; highInverse >= 0 && !TCE_TriggerBit(&inverse, highInverse); --highInverse)
		{
		}
		for (axis = 0; axis < 3; ++axis)
		{
			TCE_TriggerProduct(&term, direction[axis], 1.0f, control);
			for (lowTerm = 0; lowTerm < 576 && !TCE_TriggerBit(&term, lowTerm); ++lowTerm)
			{
			}
			if (lowTerm < 576)
			{
				for (highTerm = 575; !TCE_TriggerBit(&term, highTerm); --highTerm)
				{
				}
				if (lowTerm + lowInverse < 298 || highTerm + highInverse > 866)
				{
					return 0;
				}
			}
			TCE_GustMultiply(&term, &inverse, control);
			normalized[axis] = TCE_RotateStore(term, control);
		}
	}
	memcpy(output, normalized, sizeof(normalized));
	*length = root;
	return 1;
}
#endif
#endif
