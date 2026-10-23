#ifndef TCE_NUMERIC_H
#define TCE_NUMERIC_H

#include <limits.h>
#include <float.h>
#include <string.h>

typedef char tceNumericLayout_t[(sizeof(int) == 4 && sizeof(unsigned) == 4 &&
                                 sizeof(float) == 4 && UINT_MAX == 4294967295u &&
                                 INT_MAX == 2147483647 && INT_MIN == (-2147483647 - 1) &&
                                 FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128) ? 1 : -1];

/* Interpret a native counter's low 32 bits as a signed value. */
static __inline int TCE_Int32FromUnsigned(unsigned value)
{
	if (value <= (unsigned)INT_MAX)
	{
		return (int)value;
	}
	return -1 - (int)(UINT_MAX - value);
}

/* Model a native 32-bit ADD without signed overflow or out-of-range casts. */
static __inline int TCE_AddInt32(int left, int right)
{
	unsigned sum = (unsigned)left + (unsigned)right;
	if (sum <= (unsigned)INT_MAX)
	{
		return (int)sum;
	}
	return -1 - (int)(UINT_MAX - sum);
}

/* Model a native 32-bit SUB, including subtraction of INT_MIN. */
static __inline int TCE_SubInt32(int left, int right)
{
	unsigned difference = (unsigned)left - (unsigned)right;
	if (difference <= (unsigned)INT_MAX)
	{
		return (int)difference;
	}
	return -1 - (int)(UINT_MAX - difference);
}

/* Preserve the original branch decisions without relying on FPU status bits. */
static __inline int TCE_FloatIsNaN(float value)
{
	unsigned bits;
	memcpy(&bits, &value, sizeof(bits));
	return (bits & 0x7fffffffu) > 0x7f800000u;
}

static __inline int TCE_FloatIsZeroOrNaN(float value)
{
	return value == 0.0f || TCE_FloatIsNaN(value);
}

/* Windows consumes the low word of signed64 truncation; Linux uses signed32.
 * Explicit bounds also cover NaNs, infinities and integer-indefinite results.
 * This models numeric results, not processor exception flags or control words. */
static __inline int TCE_TruncateNativeInteger(long double value)
{
#if defined(_WIN32)
	unsigned low;
	int      result;
	if (!(value >= -9223372036854775808.0L && value < 9223372036854775808.0L))
	{
		return 0;
	}
	low = (unsigned)(unsigned long long)(long long)value;
	memcpy(&result, &low, sizeof(result));
	return result;
#else
	if (!(value >= -2147483648.0L && value < 2147483648.0L))
	{
		return INT_MIN;
	}
	return (int)value;
#endif
}

#endif
