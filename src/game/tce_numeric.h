#ifndef TCE_NUMERIC_H
#define TCE_NUMERIC_H

#include <limits.h>
#include <float.h>
#include <string.h>

typedef char tceNumericLayout_t[(sizeof(int) == 4 && sizeof(unsigned) == 4 &&
    sizeof(float) == 4 && INT_MAX == 2147483647 && INT_MIN == (-2147483647-1) &&
    FLT_RADIX == 2 && FLT_MANT_DIG == 24 && FLT_MAX_EXP == 128) ? 1 : -1];

/* Preserve the original branch decisions without relying on FPU status bits. */
static __inline int TCE_FloatIsNaN(float value) {
    unsigned bits;
    memcpy(&bits, &value, sizeof(bits));
    return (bits & 0x7fffffffu) > 0x7f800000u;
}

static __inline int TCE_FloatIsZeroOrNaN(float value) {
    return value == 0.0f || TCE_FloatIsNaN(value);
}

/* Windows consumes the low word of signed64 truncation; Linux uses signed32.
 * Explicit bounds also cover NaNs, infinities and integer-indefinite results.
 * This models numeric results, not processor exception flags or control words. */
static __inline int TCE_TruncateNativeInteger(long double value) {
#if defined(_WIN32)
    unsigned low;
    int result;
    if (!(value >= -9223372036854775808.0L && value < 9223372036854775808.0L))
        return 0;
    low = (unsigned)(unsigned long long)(long long)value;
    memcpy(&result, &low, sizeof(result));
    return result;
#else
    if (!(value >= -2147483648.0L && value < 2147483648.0L))
        return INT_MIN;
    return (int)value;
#endif
}

#endif
