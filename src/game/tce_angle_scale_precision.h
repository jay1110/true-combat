#ifndef TCE_ANGLE_SCALE_PRECISION_H
#define TCE_ANGLE_SCALE_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_trigger_precision.h"

/* The native double scale is 0x16c16c16c16c17 * 2^-45. Its product
 * with a Float mantissa can need 77 bits before current-PC rounding.
 * Signed64 truncation precedes the low16 mask; exception flags are separate. */
static __inline int TCE_FiniteAngleScale(float angle, float *result)
{
	tceTriggerSum_t value = { { 0 }, 0 };
	unsigned bits, exponent, mantissa, control, shortAngle;
	unsigned long long low, high;
	int shift;

	memcpy(&bits, &angle, sizeof(bits));
	exponent = (bits >> 23) & 255u;
	if (exponent == 255u)
	{
		return 0;
	}
	mantissa = (bits & 0x7fffffu) | (exponent ? 0x800000u : 0u);
	value.negative = bits >> 31;
	low = (unsigned long long)mantissa * 0x16c16c17u;
	high = (unsigned long long)mantissa * 0x16c16cu + (low >> 32);
	shift = (int)(exponent ? exponent - 1 : 0) + 104;
	TCE_TriggerPlace(&value, (unsigned)low, shift);
	TCE_TriggerPlace(&value, high, shift + 32);
	__control87_2(0, 0, &control, NULL);
	TCE_TriggerRound(&value, control);
	shortAngle = (unsigned)TCE_FixedSigned64Low32(&value) & 65535u;
	/* Both the integer conversion and this dyadic product are exact. */
	*result = (float)(45u * shortAngle) * (1.0f / 8192.0f);
	return 1;
}
#endif
#endif
