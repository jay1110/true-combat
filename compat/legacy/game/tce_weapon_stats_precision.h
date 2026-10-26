#ifndef TCE_WEAPON_STATS_PRECISION_H
#define TCE_WEAPON_STATS_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_ratio_precision.h"

/* TC widens hits to unsigned32/FILD64, multiplies at PC, then FIDIVs signed
 * attempts. The rounded product is still an integer smaller than 2^39. */
static __inline int TCE_WeaponStatsRatio(unsigned hits, int attempts,
                                        unsigned control, tceTriggerSum_t *accuracy)
{
	tceTriggerSum_t product = { { 0 }, 0 };
	unsigned long long numerator = 0;
	unsigned denominator;
	int bit;

	if (!attempts)
	{
		return 0;
	}
	TCE_TriggerPlace(&product, (unsigned long long)hits * 100u, 298);
	TCE_TriggerRound(&product, control);
	for (bit = 0; bit < 39; ++bit)
	{
		numerator |= (unsigned long long)TCE_TriggerBit(&product, 298 + bit) << bit;
	}
	denominator = attempts < 0 ? 0u - (unsigned)attempts : (unsigned)attempts;
	*accuracy = TCE_RatioValue(numerator, denominator, 0, attempts < 0, control);
	return 1;
}

/* The first pass compares the held quotient, then truncates only a new best.
 * Exception status flags and unmasked traps retain a separate FP contract. */
static __inline int TCE_WeaponStatsReplacement(unsigned hits, int attempts,
                                               int best, int top,
                                               int *replace, int *nextBest)
{
	tceTriggerSum_t accuracy, threshold = { { 0 }, 0 };
	unsigned control, magnitude;
	int order;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_WeaponStatsRatio(hits, attempts, control, &accuracy))
	{
		return 0;
	}
	magnitude = best < 0 ? 0u - (unsigned)best : (unsigned)best;
	threshold.negative = best < 0;
	TCE_TriggerPlace(&threshold, magnitude, 298);
	order = TCE_TriggerCompare(&accuracy, &threshold);
	*replace = top ? order > 0 : order < 0;
	*nextBest = *replace ? TCE_FixedSigned64Low32(&accuracy) : best;
	return 1;
}

/* The second pass stores/reloads Float accuracy before either comparison.
 * Bottom mode instead adds the original binary64 .999 to the held best. */
static __inline int TCE_WeaponStatsSelection(unsigned hits, int attempts,
                                             int best, int top, int *selected)
{
	tceTriggerSum_t accuracy, threshold = { { 0 }, 0 }, slack = { { 0 }, 0 };
	unsigned control, magnitude;
	float stored;
	int order;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_WeaponStatsRatio(hits, attempts, control, &accuracy))
	{
		return 0;
	}
	stored = TCE_RotateStore(accuracy, control);
	TCE_TriggerProduct(&accuracy, stored, 1.0f, control);
	magnitude = best < 0 ? 0u - (unsigned)best : (unsigned)best;
	threshold.negative = best < 0;
	TCE_TriggerPlace(&threshold, magnitude, 298);
	if (!top)
	{
		TCE_TriggerPlace(&slack, 0x1ff7ced916872bULL, 245);
		TCE_RotateAdd(&threshold, &slack, control);
	}
	order = TCE_TriggerCompare(&accuracy, &threshold);
	*selected = top ? order >= 0 : order <= 0;
	return 1;
}
#endif
#endif
