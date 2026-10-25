#ifndef TCE_WEAPON_POSITION_PRECISION_H
#define TCE_WEAPON_POSITION_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "../game/tce_rotate_precision.h"

/* Native tank pitch: Float * Double(0x3feaaaaaaaaaaaab), then FSTP32.
 * Keep the product's possible 77 bits until current-PC rounding. Even the
 * smallest subnormal product fits the existing fixed 2^-298 representation. */
static __inline int TCE_WeaponPositionTankPitch(float pitch, float *result)
{
	tceTriggerSum_t value = { { 0 }, 0 };
	unsigned bits, exponent, mantissa, control;
	unsigned long long low, high;
	int shift;

	memcpy(&bits, &pitch, sizeof(bits));
	exponent = (bits >> 23) & 255u;
	if (exponent == 255u)
	{
		return 0;
	}
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	mantissa = (bits & 0x7fffffu) | (exponent ? 0x800000u : 0u);
	value.negative = bits >> 31;
	low = (unsigned long long)mantissa * 0xaaaaaaabu;
	high = (unsigned long long)mantissa * 0x1aaaaau + (low >> 32);
	shift = (int)(exponent ? exponent - 1 : 0) + 96;
	TCE_TriggerPlace(&value, (unsigned)low, shift);
	TCE_TriggerPlace(&value, high, shift + 32);
	TCE_TriggerRound(&value, control);
	*result = TCE_RotateStore(value, control);
	return 1;
}

/* FILD32, multiply by the stored Float 0.005, optionally subtract from
 * one, then FSTP32. The integer import is exact even with PC24 selected. */
static __inline int TCE_WeaponPositionLeanFraction(int elapsed, int fading,
                                                  float *result)
{
	tceTriggerSum_t value = { { 0 }, 0 }, one = { { 0 }, 0 };
	unsigned control, magnitude;
	unsigned long long product;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	value.negative = elapsed < 0;
	magnitude = elapsed < 0 ? 0u - (unsigned)elapsed : (unsigned)elapsed;
	product = (unsigned long long)magnitude * 0xa3d70au;
	TCE_TriggerPlace(&value, product, 267);
	TCE_TriggerRound(&value, control);
	if (fading)
	{
		value.negative ^= 1u;
		TCE_TriggerPlace(&one, 1, 298);
		TCE_RotateAdd(&one, &value, control);
		value = one;
	}
	*result = TCE_RotateStore(value, control);
	return 1;
}
#endif
#endif
