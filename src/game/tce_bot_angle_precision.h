#ifndef TCE_BOT_ANGLE_PRECISION_H
#define TCE_BOT_ANGLE_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_rotate_precision.h"

/* Windows AngleDifference returns its rounded difference in ST0 directly to
 * signed64 truncation. Only a single +/-360 correction is performed.
 * Exception flags and nonfinite operands retain separate caller handling. */
static __inline int TCE_FiniteBotAngleLow32(float first, float second, int *result)
{
	tceTriggerSum_t left, right, difference, boundary, correction;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_TriggerProduct(&left, first, 1.0f, control) ||
	    !TCE_TriggerProduct(&right, second, 1.0f, control))
	{
		return 0;
	}
	difference = left;
	right.negative ^= 1u;
	TCE_RotateAdd(&difference, &right, control);
	right.negative ^= 1u;
	TCE_TriggerProduct(&boundary, 180.0f, 1.0f, control);
	TCE_TriggerProduct(&correction, 360.0f, 1.0f, control);
	if (TCE_TriggerCompare(&left, &right) > 0)
	{
		if (TCE_TriggerCompare(&difference, &boundary) > 0)
		{
			correction.negative = 1;
			TCE_RotateAdd(&difference, &correction, control);
		}
	}
	else
	{
		boundary.negative = 1;
		if (TCE_TriggerCompare(&difference, &boundary) < 0)
		{
			TCE_RotateAdd(&difference, &correction, control);
		}
	}
	*result = TCE_FixedSigned64Low32(&difference);
	return 1;
}
#endif
#endif
