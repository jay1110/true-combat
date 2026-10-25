#ifndef TCE_SPLINE_TRAVERSE_PRECISION_H
#define TCE_SPLINE_TRAVERSE_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_interpolation_precision.h"

/* Divide a PC-rounded fixed value by a finite nonzero Float without an
 * intermediate Double store. Reject results outside the fixed grid. */
static __inline int TCE_SplineDivideValue(const tceTriggerSum_t *value,
                                          float divisor, unsigned control,
                                          tceTriggerSum_t *result)
{
	unsigned bits, exponent, mantissa, negative;
	unsigned long long significand = 0, probe;
	int highest, drop, bit, numeratorBit = 0, denominatorBit = 0, scale, power;

	memcpy(&bits, &divisor, sizeof(bits));
	exponent = (bits >> 23) & 255u;
	if (exponent == 255u || !(bits & 0x7fffffffu))
	{
		return 0;
	}
	mantissa = (bits & 0x7fffffu) | (exponent ? 0x800000u : 0u);
	negative = value->negative ^ (bits >> 31);
	for (highest = 575; highest >= 0 && !TCE_TriggerBit(value, highest); --highest)
	{
	}
	drop = highest > 63 ? highest - 63 : 0;
	for (bit = 0; bit < 64; ++bit)
	{
		significand |= (unsigned long long)TCE_TriggerBit(value, drop + bit) << bit;
	}
	scale = drop - 298 + (exponent ? 150 - (int)exponent : 149);
	if (significand)
	{
		for (probe = significand; probe >> 1; probe >>= 1)
		{
			++numeratorBit;
		}
		for (probe = mantissa; probe >> 1; probe >>= 1)
		{
			++denominatorBit;
		}
		power = numeratorBit - denominatorBit;
		if ((power >= 0 && significand < ((unsigned long long)mantissa << power)) ||
		    (power < 0 && (significand << -power) < mantissa))
		{
			--power;
		}
		power += scale;
		/* PC64 needs at most 63 fractional significand bits. Keep one
		 * high bit available for a rounding carry. */
		if (power < -235 || power > 276)
		{
			return 0;
		}
	}
	*result = TCE_RatioValue(significand, mantissa, scale, negative, control);
	return 1;
}

/* The finite transaction delays caller writes until it knows no fallback
 * is needed. Reject aliases into any spline row before using that row. */
static __inline int TCE_SplineIndependentInputs(float *fraction,
                                                splinePath_t **path,
                                                const splinePath_t *node)
{
	return node &&
	       (unsigned)fraction - (unsigned)node >= sizeof(*node) &&
	       (unsigned)path - (unsigned)node >= sizeof(*node) &&
	       (unsigned)fraction - (unsigned)path >= sizeof(*path) &&
	       (unsigned)path - (unsigned)fraction >= sizeof(*fraction);
}

static __inline int TCE_TraverseSplineFinite(float *deltaTime,
                                             splinePath_t **pSpline,
                                             int *accepted)
{
	tceTriggerSum_t fraction, one = { { 0 }, 0 }, zero = { { 0 }, 0 }, negativeOne, distance;
	splinePath_t *node = NULL, *neighbor;
	float stored = *deltaTime;
	unsigned control, bits;
	int fractionChanged = 0, pathChanged = 0, success = 1;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&fraction, stored, 1.0f, control))
	{
		return 0;
	}
	TCE_TriggerPlace(&one, 1, 298);
	negativeOne = one;
	negativeOne.negative = 1;
	while (TCE_TriggerCompare(&fraction, &one) > 0)
	{
		if (!pSpline || (!node && !(node = *pSpline)) ||
		    !TCE_SplineIndependentInputs(deltaTime, pSpline, node) ||
		    !TCE_TriggerProduct(&fraction, stored, 1.0f, control))
		{
			return 0;
		}
		TCE_RotateAdd(&fraction, &negativeOne, control);
		stored = TCE_RotateStore(fraction, control);
		fractionChanged = 1;
		neighbor = node->next;
		memcpy(&bits, &node->length, sizeof(bits));
		if ((bits & 0x7f800000u) == 0x7f800000u)
		{
			return 0;
		}
		distance = fraction;
		TCE_InterpolationMultiply(&distance, bits, control);
		if (!neighbor)
		{
			success = 0;
			goto finish;
		}
		if (!TCE_SplineIndependentInputs(deltaTime, pSpline, neighbor))
		{
			return 0;
		}
		memcpy(&bits, &neighbor->length, sizeof(bits));
		if (!(bits & 0x7fffffffu))
		{
			success = 0;
			goto finish;
		}
		if (!TCE_SplineDivideValue(&distance, neighbor->length, control, &fraction))
		{
			return 0;
		}
		node = neighbor;
		pathChanged = 1;
		stored = TCE_RotateStore(fraction, control);
	}
	/* Windows reloads the Float store before its first backward gate. */
	if (!TCE_TriggerProduct(&fraction, stored, 1.0f, control))
	{
		return 0;
	}
	while (TCE_TriggerCompare(&fraction, &zero) < 0)
	{
		if (!pSpline || (!node && !(node = *pSpline)) ||
		    !TCE_SplineIndependentInputs(deltaTime, pSpline, node) ||
		    !TCE_TriggerProduct(&distance, stored, 1.0f, control))
		{
			return 0;
		}
		memcpy(&bits, &node->length, sizeof(bits));
		if ((bits & 0x7f800000u) == 0x7f800000u)
		{
			return 0;
		}
		TCE_InterpolationMultiply(&distance, bits, control);
		neighbor = node->prev;
		distance.negative ^= 1u;
		if (!neighbor)
		{
			success = 0;
			goto finish;
		}
		if (!TCE_SplineIndependentInputs(deltaTime, pSpline, neighbor))
		{
			return 0;
		}
		memcpy(&bits, &neighbor->length, sizeof(bits));
		if (!(bits & 0x7fffffffu))
		{
			success = 0;
			goto finish;
		}
		if (!TCE_SplineDivideValue(&distance, neighbor->length, control, &fraction))
		{
			return 0;
		}
		fraction.negative ^= 1u;
		distance = one;
		TCE_RotateAdd(&distance, &fraction, control);
		fraction = distance;
		node = neighbor;
		pathChanged = fractionChanged = 1;
		stored = TCE_RotateStore(fraction, control);
	}
finish:
	if (fractionChanged)
	{
		*deltaTime = stored;
	}
	if (pathChanged)
	{
		*pSpline = node;
	}
	*accepted = success;
	return 1;
}
#endif
#endif
