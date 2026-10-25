#ifndef TCE_BOT_MOUNT_PRECISION_H
#define TCE_BOT_MOUNT_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_interpolation_precision.h"

typedef enum
{
	TCE_BOT_SCAN_YAW,
	TCE_BOT_SCAN_PITCH,
	TCE_BOT_MOUNT_JITTER
} tceBotMountAngle_t;

static __inline void TCE_BotMountMultiplyWord(tceTriggerSum_t *value, unsigned factor)
{
	unsigned long long carry = 0, wide;
	int word;

	for (word = 0; word < 18; ++word)
	{
		wide = (unsigned long long)value->word[word] * factor + carry;
		value->word[word] = (unsigned)wide;
		carry = wide >> 32;
	}
}

/* Inputs have magnitude below 2^129 and quantum at least 2^-187.
 * Multiplying by these 53-bit coefficients fits the fixed grid exactly. */
static __inline void TCE_BotMountScaleDouble(tceTriggerSum_t *value,
                                             unsigned long long coefficient,
                                             int drop, unsigned control)
{
	tceTriggerSum_t high = *value;
	unsigned part, adjacent;
	int word, words = drop / 32, bits = drop % 32;

	TCE_BotMountMultiplyWord(value, (unsigned)coefficient);
	TCE_BotMountMultiplyWord(&high, (unsigned)(coefficient >> 32));
	for (word = 17; word > 0; --word)
	{
		high.word[word] = high.word[word - 1];
	}
	high.word[0] = 0;
	TCE_TriggerAddExact(value, &high);
	for (word = 0; word < 18; ++word)
	{
		part = word + words < 18 ? value->word[word + words] : 0;
		adjacent = bits && word + words + 1 < 18 ?
		           value->word[word + words + 1] << (32 - bits) : 0;
		value->word[word] = (part >> bits) | adjacent;
	}
	TCE_TriggerRound(value, control);
}

static __inline void TCE_BotMountRandomFraction(unsigned sample, unsigned control,
                                                tceTriggerSum_t *value)
{
	unsigned reciprocalBits = 0x38000100u;
	float reciprocal;

	memcpy(&reciprocal, &reciprocalBits, sizeof(reciprocal));
	TCE_TriggerProduct(value, (float)sample, reciprocal, control);
}

/* Preserve the held random value through each PC/RC arithmetic step and
 * store only the final Float angle. FP status flags remain separate. */
static __inline int TCE_BotMountRandomAngle(unsigned sample, float base, float arc,
                                            tceBotMountAngle_t mode, float *result)
{
	tceTriggerSum_t value, half, doubled, start;
	unsigned control, arcBits;

	__control87_2(0, 0, &control, NULL);
	memcpy(&arcBits, &arc, sizeof(arcBits));
	if (sample > 32767u || !TCE_FiniteExceptionsMasked(control) ||
	    mode < TCE_BOT_SCAN_YAW || mode > TCE_BOT_MOUNT_JITTER ||
	    (mode != TCE_BOT_MOUNT_JITTER && (arcBits & 0x7f800000u) == 0x7f800000u) ||
	    !TCE_TriggerProduct(&start, base, 1.0f, control))
	{
		return 0;
	}
	TCE_BotMountRandomFraction(sample, control, &value);
	TCE_TriggerProduct(&half, -0.5f, 1.0f, control);
	TCE_RotateAdd(&value, &half, control);
	doubled = value;
	TCE_RotateAdd(&value, &doubled, control);
	if (mode == TCE_BOT_MOUNT_JITTER)
	{
		TCE_InterpolationMultiply(&value, 0x41200000u, control); /* 10 */
	}
	else
	{
		TCE_InterpolationMultiply(&value, arcBits, control);
		if (mode == TCE_BOT_SCAN_YAW)
		{
			TCE_BotMountScaleDouble(&value, 0x1ccccccccccccdULL, 54, control); /* .45 */
		}
		else
		{
			TCE_BotMountScaleDouble(&value, 0x13333333333333ULL, 55, control); /* .15 */
		}
	}
	TCE_RotateAdd(&value, &start, control);
	*result = TCE_RotateStore(value, control);
	return 1;
}

static __inline int TCE_BotMountMoveValue(unsigned sample, int centered, float *result)
{
	tceTriggerSum_t value, constant, doubled;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (sample > 32767u || !TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	TCE_BotMountRandomFraction(sample, control, &value);
	if (centered)
	{
		TCE_TriggerProduct(&constant, -0.5f, 1.0f, control);
		TCE_RotateAdd(&value, &constant, control);
		doubled = value;
		TCE_RotateAdd(&value, &doubled, control);
	}
	else
	{
		TCE_InterpolationMultiply(&value, 0x43480000u, control); /* 200 */
		TCE_TriggerProduct(&constant, 100.0f, 1.0f, control);
		TCE_RotateAdd(&value, &constant, control);
	}
	*result = TCE_RotateStore(value, control);
	return 1;
}

static __inline int TCE_BotMountWait(unsigned sample, float thinkTime, int *wait)
{
	tceTriggerSum_t sampleValue, threshold;
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (sample > 32767u || !TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&threshold, thinkTime, 1.0f, control))
	{
		return 0;
	}
	TCE_BotMountRandomFraction(sample, control, &sampleValue);
	TCE_BotMountScaleDouble(&threshold, 0x1999999999999aULL, 53, control); /* .8 */
	*wait = TCE_TriggerCompare(&sampleValue, &threshold) < 0;
	return 1;
}

static __inline int TCE_BotMountReloadValue(int maxClip, int *result)
{
	tceTriggerSum_t value = { { 0 }, 0 };
	unsigned control, magnitude;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control))
	{
		return 0;
	}
	magnitude = maxClip < 0 ? 0u - (unsigned)maxClip : (unsigned)maxClip;
	value.negative = maxClip < 0;
	TCE_TriggerPlace(&value, magnitude, 298);
	TCE_BotMountScaleDouble(&value, 0x1999999999999aULL, 53, control);
	*result = TCE_FixedSigned64Low32(&value);
	return 1;
}
#endif
#endif
