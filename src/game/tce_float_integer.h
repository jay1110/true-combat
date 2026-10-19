#ifndef TCE_FLOAT_INTEGER_H
#define TCE_FLOAT_INTEGER_H

#if defined(_MSC_VER) && defined(_M_IX86)
#include <string.h>

/* Numeric low word of the original masked x87 float-to-int64 conversion.
 * This does not reproduce floating-point exception flags or traps. */
static int TCE_FloatToInt64Low(float value)
{
	unsigned int bits, magnitude;
	int          exponent, result;

	memcpy(&bits, &value, sizeof(bits));
	exponent = (int)((bits >> 23) & 255u) - 127;
	/* Fractions truncate to zero. Exponents 55..62 are multiples of
	 * 2^32; larger values and masked invalid conversions also have a
	 * zero low word in the original signed 64-bit conversion. */
	if (exponent < 0 || exponent >= 55)
	{
		return 0;
	}

	magnitude = (bits & 0x7fffffu) | 0x800000u;
	if (exponent < 23)
	{
		magnitude >>= 23 - exponent;
	}
	else
	{
		magnitude <<= exponent - 23;
	}
	if (bits & 0x80000000u)
	{
		magnitude = 0u - magnitude;
	}
	memcpy(&result, &magnitude, sizeof(result));
	return result;
}
#endif

#endif
