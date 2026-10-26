#ifndef TCE_AWARD_PRECISION_H
#define TCE_AWARD_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_ratio_precision.h"

/* Award comparisons consume retained x87 results, without a Float or Double
 * store. These finite, masked paths preserve PC/RC numerical decisions;
 * exception status flags remain a separate contract. */
static __inline int TCE_AwardFiniteSkillBetter(float points, float start,
                                               float bestPoints, float bestStart,
                                               int *better)
{
	tceTriggerSum_t candidate, candidateStart, best, bestBeginning;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&candidate, points, 1.0f, control) ||
	    !TCE_TriggerProduct(&candidateStart, start, 1.0f, control) ||
	    !TCE_TriggerProduct(&best, bestPoints, 1.0f, control) ||
	    !TCE_TriggerProduct(&bestBeginning, bestStart, 1.0f, control))
	{
		return 0;
	}
	candidateStart.negative ^= 1u;
	TCE_RotateAdd(&candidate, &candidateStart, control);
	bestBeginning.negative ^= 1u;
	TCE_RotateAdd(&best, &bestBeginning, control);
	*better = TCE_TriggerCompare(&candidate, &best) > 0;
	return 1;
}

static __inline int TCE_AwardFiniteRateBetter(int score, int elapsed,
                                              int bestScore, int bestElapsed,
                                              int *better)
{
	tceTriggerSum_t candidate, best;
	unsigned control, numerator, denominator;

	__control87_2(0, 0, &control, NULL);
	if (!elapsed || !bestElapsed || !TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	/* FILD imports signed32 exactly before FIDIV rounds the quotient. */
	numerator = score < 0 ? 0u - (unsigned)score : (unsigned)score;
	denominator = elapsed < 0 ? 0u - (unsigned)elapsed : (unsigned)elapsed;
	candidate = TCE_RatioValue(numerator, denominator, 0,
	                           (score < 0) ^ (elapsed < 0), control);
	numerator = bestScore < 0 ? 0u - (unsigned)bestScore : (unsigned)bestScore;
	denominator = bestElapsed < 0 ? 0u - (unsigned)bestElapsed : (unsigned)bestElapsed;
	best = TCE_RatioValue(numerator, denominator, 0,
	                      (bestScore < 0) ^ (bestElapsed < 0), control);
	*better = TCE_TriggerCompare(&candidate, &best) > 0;
	return 1;
}

static __inline int TCE_AwardFiniteLossAllows(float startXP, int score, int *allows)
{
	tceTriggerSum_t loss, negativeScore = { { 0 }, 0 }, boundary;
	unsigned control, magnitude;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&loss, startXP, 1.0f, control))
	{
		return 0;
	}
	/* FILD score followed by FSUBR startXP: do not first convert score to Float. */
	magnitude = score < 0 ? 0u - (unsigned)score : (unsigned)score;
	TCE_TriggerPlace(&negativeScore, magnitude, 298);
	negativeScore.negative = score >= 0;
	TCE_RotateAdd(&loss, &negativeScore, control);
	TCE_TriggerProduct(&boundary, 100.0f, 1.0f, control);
	*allows = TCE_TriggerCompare(&loss, &boundary) < 0;
	return 1;
}
#endif
#endif
