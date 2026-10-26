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

/* Each of the three movement factors is zero or within 2^-64..2^24.
 * Their exact product has no bits below 2^-261 and magnitude at most 2^72;
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
		if (bits && (bits < 0x1f800000u || bits > 0x4b800000u))
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
	if (bits && (bits < 0x20800000u || bits > 0x4b800000u))
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
 * Any finite Float fraction gives a product quantum of at least 2^-150. */
static __inline int TCE_WeaponPositionLeanPitch(int magnitude, float fraction,
                                               float pitch, float *result)
{
	tceTriggerSum_t value = { { 0 }, 0 }, factor, start;
	unsigned control, absolute;

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

/* Reciprocal of the rounded, nonzero 200/elapsed posture denominator.
 * Its at most 64 significant bits are exact in the fixed representation.
 * Restoring division keeps the extra remainder bit explicitly. */
static __inline tceTriggerSum_t TCE_WeaponPositionPostureReciprocal(
    const tceTriggerSum_t *denominator, unsigned control)
{
	tceTriggerSum_t value = { { 0 }, 0 };
	unsigned long long divisor = 0, remainder, significand = 1;
	unsigned precision, rounding, carry, increment;
	int highest, bit, exponent;

	for (highest = 575; !TCE_TriggerBit(denominator, highest); --highest)
	{
	}
	for (bit = 0; bit < 64; ++bit)
	{
		divisor = (divisor << 1) | TCE_TriggerBit(denominator, highest - bit);
	}
	value.negative = denominator->negative;
	exponent = 298 - highest;
	if (divisor == (1ULL << 63))
	{
		remainder = 0;
	}
	else
	{
		/* The normalized numerator is 2^64, so its remainder is -divisor
		 * modulo 2^64. It is strictly smaller than divisor. */
		remainder = 0ULL - divisor;
		--exponent;
	}
	precision = (control & _MCW_PC) == _PC_24 ? 24u :
	            (control & _MCW_PC) == _PC_53 ? 53u : 64u;
	for (bit = 1; bit < (int)precision; ++bit)
	{
		carry = (unsigned)(remainder >> 63);
		remainder <<= 1;
		significand <<= 1;
		if (carry || remainder >= divisor)
		{
			remainder -= divisor;
			significand |= 1;
		}
	}
	rounding = control & _MCW_RC;
	increment = rounding == _RC_NEAR ?
	    (remainder > divisor - remainder ||
	     (remainder == divisor - remainder && (significand & 1))) :
	    (remainder && ((rounding == _RC_UP && !value.negative) ||
	                   (rounding == _RC_DOWN && value.negative)));
	if (increment)
	{
		if (significand == ~0ULL)
		{
			significand = 1ULL << 63;
			++exponent;
		}
		else
		{
			++significand;
			if (precision < 64 && (significand >> precision))
			{
				significand >>= 1;
				++exponent;
			}
		}
	}
	TCE_TriggerPlace(&value, significand, 298 + exponent - ((int)precision - 1));
	return value;
}

/* Posture kind: prone, stance, duck. Retain the two native divisions and
 * each scaled direction until its addition to the stored Float origin.
 * Every finite Float axis fits; publish only after all stages succeed. */
static __inline int TCE_WeaponPositionPosture(int elapsed, int active, int kind,
                                             const float axis[3][3],
                                             float origin[3], float angles[3])
{
	tceTriggerSum_t fraction = { { 0 }, 0 }, denominator, one = { { 0 }, 0 };
	tceTriggerSum_t scale, amount, direction, start, value;
	float positions[3], rotations[3], coefficient;
	unsigned control, magnitude;
	int stage, stages, component, directionIndex, rotationIndex;

	if ((active && elapsed <= 0) || (!active && elapsed >= 200))
	{
		return 0;
	}
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	TCE_TriggerPlace(&fraction, 1, 298);
	if (elapsed && (!active || elapsed <= 200))
	{
		magnitude = elapsed < 0 ? 0u - (unsigned)elapsed : (unsigned)elapsed;
		denominator = TCE_RatioValue(200, magnitude, 0, elapsed < 0, control);
		fraction = TCE_WeaponPositionPostureReciprocal(&denominator, control);
		if (!active)
		{
			TCE_TriggerPlace(&one, 1, 298);
			fraction.negative ^= 1u;
			TCE_RotateAdd(&one, &fraction, control);
			fraction = one;
		}
	}
	memcpy(positions, origin, sizeof(positions));
	memcpy(rotations, angles, sizeof(rotations));
	stages = kind == 0 ? 3 : kind == 2 ? 2 : 1;
	for (stage = 0; stage < stages; ++stage)
	{
		directionIndex = kind == 0 ? stage : kind == 2 && !stage ? 0 : 2;
		coefficient = kind == 0 ? (stage == 0 ? -25.0f : stage == 1 ? 5.0f : -5.0f) :
		              kind == 2 && stage ? -20.0f : -5.0f;
		TCE_TriggerProduct(&scale, coefficient, 1.0f, control);
		amount = fraction;
		TCE_GustMultiply(&amount, &scale, control);
		for (component = 0; component < 3; ++component)
		{
			if (!TCE_TriggerProduct(&direction, axis[directionIndex][component], 1.0f, control) ||
			    !TCE_TriggerProduct(&start, positions[component], 1.0f, control))
			{
				return 0;
			}
			value = amount;
			TCE_GustMultiply(&value, &direction, control);
			TCE_RotateAdd(&value, &start, control);
			positions[component] = TCE_RotateStore(value, control);
		}
	}
	for (stage = 0; stage < (kind == 0 ? 1 : 2); ++stage)
	{
		rotationIndex = stage ? 0 : 1;
		coefficient = kind == 0 ? 60.0f : stage ? 30.0f : 10.0f;
		if (!TCE_TriggerProduct(&start, rotations[rotationIndex], 1.0f, control))
		{
			return 0;
		}
		TCE_TriggerProduct(&scale, coefficient, 1.0f, control);
		value = fraction;
		TCE_GustMultiply(&value, &scale, control);
		TCE_RotateAdd(&value, &start, control);
		rotations[rotationIndex] = TCE_RotateStore(value, control);
	}
	memcpy(origin, positions, sizeof(positions));
	memcpy(angles, rotations, sizeof(rotations));
	return 1;
}

/* Native bob uses three Float factors, then a stored Double coefficient.
 * Factors in 2^-56..2^24 give a product quantum no smaller than 2^-297. */
static __inline int TCE_WeaponPositionBobFinite(float bob, float speed, float scale,
                                               int reverse, float angles[3])
{
	tceTriggerSum_t value, speedValue, signedSpeed, scaleValue, coefficient = { { 0 }, 0 }, start;
	float factors[3], result[3];
	unsigned bits, control;
	int i, component;

	factors[0] = bob;
	factors[1] = speed;
	factors[2] = scale;
	for (i = 0; i < 3; ++i)
	{
		memcpy(&bits, &factors[i], sizeof(bits));
		bits &= 0x7fffffffu;
		if (bits && (bits < 0x23800000u || bits > 0x4b800000u))
		{
			return 0;
		}
	}
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&speedValue, speed, 1.0f, control) ||
	    !TCE_TriggerProduct(&scaleValue, scale, 1.0f, control))
	{
		return 0;
	}
	for (i = 0; i < 3; ++i)
	{
		component = 2 - i;
		if (!TCE_TriggerProduct(&value, bob, 1.0f, control) ||
		    !TCE_TriggerProduct(&start, angles[component], 1.0f, control))
		{
			return 0;
		}
		/* Roll/yaw use the signed speed; pitch uses the original speed. */
		signedSpeed = speedValue;
		if (reverse && component)
		{
			signedSpeed.negative ^= 1u;
		}
		TCE_GustMultiply(&value, &signedSpeed, control);
		TCE_GustMultiply(&value, &scaleValue, control);
		memset(&coefficient, 0, sizeof(coefficient));
		TCE_TriggerPlace(&coefficient, 0x147ae147ae147bULL, component == 1 ? 239 : 238);
		TCE_GustMultiply(&value, &coefficient, control);
		TCE_RotateAdd(&value, &start, control);
		result[component] = TCE_RotateStore(value, control);
	}
	memcpy(angles, result, sizeof(result));
	return 1;
}

/* Landing uses an exact signed integer import, then a Float product and
 * the native Double reciprocal. Any finite Float change fits; no Float
 * spill occurs before adding Z. */
static __inline int TCE_WeaponPositionLandingFinite(int elapsed, float change,
                                                   float origin, float *result)
{
	tceTriggerSum_t value = { { 0 }, 0 }, factor, coefficient = { { 0 }, 0 }, start;
	unsigned control, magnitude;
	int time = elapsed < 150 ? elapsed : 450 - elapsed;

	if (elapsed >= 450)
	{
		return 0;
	}
	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&factor, change, 1.0f, control) ||
	    !TCE_TriggerProduct(&start, origin, 1.0f, control))
	{
		return 0;
	}
	value.negative = time < 0;
	magnitude = time < 0 ? 0u - (unsigned)time : (unsigned)time;
	TCE_TriggerPlace(&value, magnitude, 298);
	TCE_GustMultiply(&value, &factor, control);
	TCE_TriggerPlace(&coefficient, 0x1b4e81b4e81b4fULL, elapsed < 150 ? 236 : 235);
	TCE_GustMultiply(&value, &coefficient, control);
	TCE_RotateAdd(&value, &start, control);
	*result = TCE_RotateStore(value, control);
	return 1;
}

/* Tactical offsets retain FILD32, subtraction of 2000, two Float products,
 * and angle addition until the final Float store. Any finite scale fits. */
static __inline int TCE_WeaponPositionTacticalAngle(int input, float scale,
                                                   float angle, float *result)
{
	tceTriggerSum_t value = { { 0 }, 0 }, center, factor, coefficient, start;
	unsigned control, magnitude;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&factor, scale, 1.0f, control) ||
	    !TCE_TriggerProduct(&start, angle, 1.0f, control))
	{
		return 0;
	}
	value.negative = input < 0;
	magnitude = input < 0 ? 0u - (unsigned)input : (unsigned)input;
	TCE_TriggerPlace(&value, magnitude, 298);
	TCE_TriggerProduct(&center, -2000.0f, 1.0f, control);
	TCE_RotateAdd(&value, &center, control);
	TCE_GustMultiply(&value, &factor, control);
	TCE_TriggerProduct(&coefficient, 0.01f, 1.0f, control);
	TCE_GustMultiply(&value, &coefficient, control);
	TCE_RotateAdd(&value, &start, control);
	*result = TCE_RotateStore(value, control);
	return 1;
}

/* Apply the two stored sway values after their filter/clamp stages.
 * Moving the exact quarter factor into a bounded normal Float axis
 * preserves both retained product roundings without an extra Float spill. */
static __inline int TCE_WeaponPositionSwayOffsets(float horizontal, float vertical,
                                                 float strength, const float axis[3][3],
                                                 float origin[3], float angles[3])
{
	float positions[3], rotations[3], quarterAxis, sway;
	unsigned control, bits;
	int stage, component;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	memcpy(positions, origin, sizeof(positions));
	memcpy(rotations, angles, sizeof(rotations));
	for (stage = 0; stage < 2; ++stage)
	{
		sway = stage ? vertical : horizontal;
		for (component = 0; component < 3; ++component)
		{
			memcpy(&bits, &axis[stage + 1][component], sizeof(bits));
			bits &= 0x7fffffffu;
			if (bits && (bits < 0x20800000u || bits > 0x4b800000u))
			{
				return 0;
			}
			quarterAxis = axis[stage + 1][component] * 0.25f;
			if (!TCE_WeaponPositionLeanOffset(sway, strength, quarterAxis,
			                                positions[component], &positions[component]))
			{
				return 0;
			}
		}
	}
	if (!TCE_WeaponPositionLeanOffset(horizontal, strength, -1.0f,
	                                rotations[1], &rotations[1]) ||
	    !TCE_WeaponPositionLeanOffset(vertical, strength, 1.0f,
	                                rotations[0], &rotations[0]))
	{
		return 0;
	}
	memcpy(origin, positions, sizeof(positions));
	memcpy(angles, rotations, sizeof(rotations));
	return 1;
}

/* Consume the stored derivative targets. The vertical upper clamp tests
 * the retained filtered value; the horizontal clamp tests its Float store. */
static __inline int TCE_WeaponPositionSwayState(const float target[2], int limitInput,
                                               int aiming, int movement, int shot,
                                               float horizontal, float vertical,
                                               float *strengthResult,
                                               float *horizontalResult, float *verticalResult)
{
	tceTriggerSum_t limit = { { 0 }, 0 }, lower, factor, weight, current;
	tceTriggerSum_t previous[2], targets[2], products[2], filtered[2];
	float stored[2], strength;
	unsigned control, magnitude, bits;
	int input, i;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&previous[0], horizontal, 1.0f, control) ||
	    !TCE_TriggerProduct(&previous[1], vertical, 1.0f, control))
	{
		return 0;
	}
	for (i = 0; i < 2; ++i)
	{
		if (!TCE_TriggerProduct(&targets[i], target[i], 1.0f, control))
		{
			return 0;
		}
	}
	if (aiming)
	{
		limit.negative = limitInput < 0;
		magnitude = limitInput < 0 ? 0u - (unsigned)limitInput : (unsigned)limitInput;
		TCE_TriggerPlace(&limit, magnitude, 298);
		TCE_TriggerProduct(&factor, 1.0f / 600.0f, 1.0f, control);
		TCE_GustMultiply(&limit, &factor, control);
	}
	else
	{
		TCE_TriggerPlace(&limit, 3, 298);
	}
	lower = limit;
	lower.negative ^= 1u;
	TCE_TriggerProduct(&factor, 0.001f, 1.0f, control);
	for (i = 0; i < 2; ++i)
	{
		memset(&products[i], 0, sizeof(products[i]));
		input = i ? shot : movement;
		products[i].negative = input < 0;
		magnitude = input < 0 ? 0u - (unsigned)input : (unsigned)input;
		TCE_TriggerPlace(&products[i], magnitude, i && aiming ? 297 : 298);
		if (i)
		{
			/* Native shot input first multiplies by half or one. */
			TCE_TriggerRound(&products[i], control);
		}
		TCE_GustMultiply(&products[i], &factor, control);
	}
	TCE_RotateAdd(&products[0], &products[1], control);
	strength = TCE_RotateStore(products[0], control);
	if (strength > 1.0f)
	{
		strength = 1.0f;
	}
	if (!aiming && !(strength >= 0.5f))
	{
		strength = 0.5f;
	}
	TCE_TriggerProduct(&factor, 0.1f, 1.0f, control);
	TCE_TriggerProduct(&weight, 0.9090908765792847f, 1.0f, control);
	for (i = 0; i < 2; ++i)
	{
		memcpy(&bits, &target[i], sizeof(bits));
		/* For a finite Float, this is exactly abs(target) < Double(0.2). */
		if ((bits & 0x7fffffffu) < 0x3e4ccccdu)
		{
			TCE_TriggerProduct(&targets[i], 0.0f, 1.0f, control);
		}
		else if (TCE_TriggerCompare(&limit, &targets[i]) < 0)
		{
			stored[i] = TCE_RotateStore(limit, control);
			TCE_TriggerProduct(&targets[i], stored[i], 1.0f, control);
		}
		else if (TCE_TriggerCompare(&lower, &targets[i]) > 0)
		{
			stored[i] = TCE_RotateStore(lower, control);
			TCE_TriggerProduct(&targets[i], stored[i], 1.0f, control);
		}
		filtered[i] = targets[i];
		TCE_GustMultiply(&filtered[i], &factor, control);
		TCE_RotateAdd(&filtered[i], &previous[i], control);
		TCE_GustMultiply(&filtered[i], &weight, control);
		stored[i] = TCE_RotateStore(filtered[i], control);
	}
	/* Vertical is compared before reloading its Float store. */
	if (TCE_TriggerCompare(&filtered[1], &limit) > 0)
	{
		stored[1] = TCE_RotateStore(limit, control);
	}
	else
	{
		TCE_TriggerProduct(&current, stored[1], 1.0f, control);
		if (TCE_TriggerCompare(&current, &lower) < 0)
		{
			stored[1] = TCE_RotateStore(lower, control);
		}
	}
	TCE_TriggerProduct(&current, stored[0], 1.0f, control);
	if (TCE_TriggerCompare(&current, &limit) > 0)
	{
		stored[0] = TCE_RotateStore(limit, control);
	}
	else if (TCE_TriggerCompare(&current, &lower) < 0)
	{
		stored[0] = TCE_RotateStore(lower, control);
	}
	*strengthResult = strength;
	*horizontalResult = stored[0];
	*verticalResult = stored[1];
	return 1;
}

/* Divide already PC-rounded, at most 64-bit significands. Sway supplies
 * a nonzero denominator >= Float(0.005); the quotient fits the fixed grid. */
static __inline tceTriggerSum_t TCE_WeaponPositionQuotient(
    const tceTriggerSum_t *numerator, const tceTriggerSum_t *denominator,
    unsigned control)
{
	tceTriggerSum_t value = { { 0 }, 0 };
	unsigned long long n = 0, d = 0, remainder, significand = 1;
	unsigned precision, rounding, carry, increment;
	int topN, topD, bit, exponent;

	value.negative = numerator->negative ^ denominator->negative;
	for (topN = 575; topN >= 0 && !TCE_TriggerBit(numerator, topN); --topN)
	{
	}
	if (topN < 0)
	{
		return value;
	}
	for (topD = 575; !TCE_TriggerBit(denominator, topD); --topD)
	{
	}
	for (bit = 0; bit < 64; ++bit)
	{
		n = (n << 1) | (topN >= bit ? TCE_TriggerBit(numerator, topN - bit) : 0);
		d = (d << 1) | (topD >= bit ? TCE_TriggerBit(denominator, topD - bit) : 0);
	}
	exponent = topN - topD;
	if (n >= d)
	{
		remainder = n - d;
	}
	else
	{
		/* Here 2*n has a carry bit and 0 <= 2*n-d < d. */
		remainder = (n << 1) - d;
		--exponent;
	}
	precision = (control & _MCW_PC) == _PC_24 ? 24u :
	            (control & _MCW_PC) == _PC_53 ? 53u : 64u;
	for (bit = 1; bit < (int)precision; ++bit)
	{
		carry = (unsigned)(remainder >> 63);
		remainder <<= 1;
		significand <<= 1;
		if (carry || remainder >= d)
		{
			remainder -= d;
			significand |= 1;
		}
	}
	rounding = control & _MCW_RC;
	increment = rounding == _RC_NEAR ?
	    (remainder > d - remainder ||
	     (remainder == d - remainder && (significand & 1))) :
	    (remainder && ((rounding == _RC_UP && !value.negative) ||
	                   (rounding == _RC_DOWN && value.negative)));
	if (increment)
	{
		if (significand == ~0ULL)
		{
			significand = 1ULL << 63;
			++exponent;
		}
		else
		{
			++significand;
			if (precision < 64 && (significand >> precision))
			{
				significand >>= 1;
				++exponent;
			}
		}
	}
	TCE_TriggerPlace(&value, significand, 298 + exponent - ((int)precision - 1));
	return value;
}

static __inline tceTriggerSum_t TCE_WeaponPositionDot(
    const tceTriggerSum_t a[3], const tceTriggerSum_t b[3],
    int first, int second, int third, unsigned control)
{
	tceTriggerSum_t value = a[first], term;

	TCE_GustMultiply(&value, &b[first], control);
	term = a[second];
	TCE_GustMultiply(&term, &b[second], control);
	TCE_RotateAdd(&value, &term, control);
	term = a[third];
	TCE_GustMultiply(&term, &b[third], control);
	TCE_RotateAdd(&value, &term, control);
	return value;
}

/* All direction/history/velocity factors are zero or in 2^-64..2^48.
 * Dot products have quantum >=2^-174; the clamped time factor is positive.
 * Horizontal derivative is stored before adding velocity, vertical is not. */
static __inline int TCE_WeaponPositionSwayTargets(int elapsed,
                                                 const float axis[3][3],
                                                 const float prior[3], const float velocity[3],
                                                 float target[2])
{
	tceTriggerSum_t axes[3][3], history[3], speeds[3], difference[3];
	tceTriggerSum_t time = { { 0 }, 0 }, minimum, coefficient, value, derivative;
	float input, result[2];
	unsigned control, bits, magnitude;
	int row, component;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	for (row = 0; row < 5; ++row)
	{
		for (component = 0; component < 3; ++component)
		{
			input = row < 3 ? axis[row][component] :
			        row == 3 ? prior[component] : velocity[component];
			memcpy(&bits, &input, sizeof(bits));
			bits &= 0x7fffffffu;
			if (bits && (bits < 0x1f800000u || bits > 0x57800000u))
			{
				return 0;
			}
			if (row < 3)
			{
				TCE_TriggerProduct(&axes[row][component], input, 1.0f, control);
			}
			else if (row == 3)
			{
				TCE_TriggerProduct(&history[component], input, 1.0f, control);
			}
			else
			{
				TCE_TriggerProduct(&speeds[component], input, 1.0f, control);
			}
		}
	}
	time.negative = elapsed < 0;
	magnitude = elapsed < 0 ? 0u - (unsigned)elapsed : (unsigned)elapsed;
	TCE_TriggerPlace(&time, magnitude, 298);
	TCE_TriggerProduct(&coefficient, 0.001f, 1.0f, control);
	TCE_GustMultiply(&time, &coefficient, control);
	TCE_TriggerProduct(&minimum, 0.005f, 1.0f, control);
	if (TCE_TriggerCompare(&time, &minimum) < 0)
	{
		time = minimum;
	}
	for (component = 0; component < 3; ++component)
	{
		difference[component] = axes[0][component];
		history[component].negative ^= 1u;
		TCE_RotateAdd(&difference[component], &history[component], control);
	}
	value = TCE_WeaponPositionDot(difference, axes[1], 0, 2, 1, control);
	derivative = TCE_WeaponPositionQuotient(&value, &time, control);
	derivative.negative ^= 1u;
	result[0] = TCE_RotateStore(derivative, control);
	TCE_TriggerProduct(&derivative, result[0], 1.0f, control);
	value = TCE_WeaponPositionDot(speeds, axes[1], 1, 0, 2, control);
	TCE_TriggerProduct(&coefficient, 0.1f, 1.0f, control);
	TCE_GustMultiply(&value, &coefficient, control);
	TCE_RotateAdd(&value, &derivative, control);
	result[0] = TCE_RotateStore(value, control);
	value = TCE_WeaponPositionDot(difference, axes[2], 2, 1, 0, control);
	derivative = TCE_WeaponPositionQuotient(&value, &time, control);
	derivative.negative ^= 1u;
	value = TCE_WeaponPositionDot(speeds, axes[0], 2, 0, 1, control);
	TCE_GustMultiply(&value, &coefficient, control);
	TCE_RotateAdd(&value, &derivative, control);
	result[1] = TCE_RotateStore(value, control);
	memcpy(target, result, sizeof(result));
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
