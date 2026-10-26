#ifndef TCE_WEAPON_FPS_PRECISION_H
#define TCE_WEAPON_FPS_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "../game/tce_ratio_precision.h"

typedef char tceWeaponFpsDoubleLayout_t[(sizeof(double) == 8 && DBL_MANT_DIG == 53 &&
                                        DBL_MAX_EXP == 1024) ? 1 : -1];

/* Windows divides by the binary64 atof result at current PC/RC, then uses
 * __ftol's signed64 truncation and low32 result. FP status is separate. */
static __inline int TCE_WeaponFrameLerp(double fps, int *result)
{
	unsigned long long bits, mantissa;
	unsigned exponent, control;
	int power;
	tceTriggerSum_t quotient;

	memcpy(&bits, &fps, sizeof(bits));
	exponent = (unsigned)((bits >> 52) & 2047u);
	mantissa = bits & 0x000fffffffffffffULL;
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) || !(bits & 0x7fffffffffffffffULL) ||
	    (exponent == 2047u && mantissa))
	{
		return 0;
	}
	power = (int)exponent - 1023;
	/* These quotients truncate to zero, or exceed signed64 even after
	 * rounding at PC24. Infinity is a zero quotient; subnormal FPS overflows. */
	if (exponent == 2047u || !exponent || power < -55 || power > 10)
	{
		*result = 0;
		return 1;
	}
	mantissa |= 0x0010000000000000ULL;
	quotient = TCE_RatioValue(1000u, mantissa, 52 - power, (unsigned)(bits >> 63), control);
	*result = TCE_FixedSigned64Low32(&quotient);
	return 1;
}
#endif
#endif
