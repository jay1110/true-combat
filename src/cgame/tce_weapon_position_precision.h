#ifndef TCE_WEAPON_POSITION_PRECISION_H
#define TCE_WEAPON_POSITION_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "../game/tce_rotate_precision.h"
#include "../game/tce_gust_precision.h"

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

/* Each of the three movement factors is zero or within 2^-32..2^24.
 * Their exact product has no bits below 2^-165 and magnitude at most 2^72;
 * this leaves room for a finite Float origin in the fixed representation. */
static __inline int TCE_WeaponPositionLeanOffset(float fraction, float roll,
                                                float axis, float origin,
                                                float *result)
{
	tceTriggerSum_t value, factor, start;
	float factors[3];
	unsigned control, bits;
	int i;

	factors[0] = fraction;
	factors[1] = roll;
	factors[2] = axis;
	for (i = 0; i < 3; ++i)
	{
		memcpy(&bits, &factors[i], sizeof(bits));
		bits &= 0x7fffffffu;
		if (bits && (bits < 0x2f800000u || bits > 0x4b800000u))
		{
			return 0;
		}
	}
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&value, fraction, roll, control) ||
	    !TCE_TriggerProduct(&factor, axis, 1.0f, control) ||
	    !TCE_TriggerProduct(&start, origin, 1.0f, control))
	{
		return 0;
	}
	TCE_GustMultiply(&value, &factor, control);
	TCE_RotateAdd(&value, &start, control);
	*result = TCE_RotateStore(value, control);
	return 1;
}

/* Scaling a bounded normal Float by one half or one quarter is exact.
 * Negating the axis before the last product gives the native FCHS stage
 * without changing its directed rounding or adding a Float spill. */
static __inline int TCE_WeaponPositionLeanScaledOffset(float lean, float fraction,
                                                      float axis, float origin,
                                                      int quarter, float *result)
{
	unsigned bits, control;
	float scaled, negativeAxis;

	memcpy(&bits, &lean, sizeof(bits));
	bits &= 0x7fffffffu;
	if (bits && (bits < 0x30800000u || bits > 0x4b800000u))
	{
		return 0;
	}
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	scaled = lean * (quarter ? 0.25f : 0.5f);
	memcpy(&bits, &axis, sizeof(bits));
	bits ^= 0x80000000u;
	memcpy(&negativeAxis, &bits, sizeof(negativeAxis));
	return TCE_WeaponPositionLeanOffset(scaled, fraction, negativeAxis,
	                                  origin, result);
}

/* The wrapped integer magnitude is imported by FILD32 before FMUL half.
 * Its product with the bounded fraction is a multiple of 2^-56 at worst. */
static __inline int TCE_WeaponPositionLeanPitch(int magnitude, float fraction,
                                               float pitch, float *result)
{
	tceTriggerSum_t value = { { 0 }, 0 }, factor, start;
	unsigned bits, control, absolute;

	memcpy(&bits, &fraction, sizeof(bits));
	bits &= 0x7fffffffu;
	if (bits && (bits < 0x2f800000u || bits > 0x4b800000u))
	{
		return 0;
	}
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&factor, fraction, 1.0f, control) ||
	    !TCE_TriggerProduct(&start, pitch, 1.0f, control))
	{
		return 0;
	}
	value.negative = magnitude < 0;
	absolute = magnitude < 0 ? 0u - (unsigned)magnitude : (unsigned)magnitude;
	TCE_TriggerPlace(&value, absolute, 297);
	TCE_TriggerRound(&value, control);
	TCE_GustMultiply(&value, &factor, control);
	TCE_RotateAdd(&value, &start, control);
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
