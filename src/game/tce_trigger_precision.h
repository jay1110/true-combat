#ifndef TCE_TRIGGER_PRECISION_H
#define TCE_TRIGGER_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include <float.h>
#include <string.h>

/* Finite binary32 products and sums in units of 2^-298. Eighteen words
 * contain every such product, including three maximal products added.
 * Round each native arithmetic step to the current x87 precision/mode.
 * No dependency on MSVC's binary64 long double; exception flags are not
 * reproduced. Also used by the finite geometry and reinforcement arithmetic
 * in tce_rotate_precision.h and tce_reinforcement_precision.h. */
typedef struct
{
	unsigned word[18];
	unsigned negative;
} tceTriggerSum_t;

/* Software arithmetic does not raise x87 traps. Preserve each caller's
 * existing floating-point fallback when an exception is unmasked.
 * Masked exception status flags still require separate reconstruction. */
static __inline int TCE_FiniteExceptionsMasked(unsigned control)
{
	return (control & _MCW_EM) == _MCW_EM;
}

static __inline unsigned TCE_TriggerBit(const tceTriggerSum_t *value, int bit)
{
	return (value->word[bit / 32] >> (bit % 32)) & 1u;
}

static __inline void TCE_TriggerPlace(tceTriggerSum_t *value,
                                     unsigned long long significand, int shift)
{
	int bit;
	for (bit = 0; bit < 64; ++bit)
	{
		if ((significand >> bit) & 1u)
		{
			value->word[(shift + bit) / 32] |= 1u << ((shift + bit) % 32);
		}
	}
}

static __inline void TCE_TriggerRound(tceTriggerSum_t *value, unsigned control)
{
	int highest, drop, bit, word;
	unsigned precision, rounding, discarded = 0, belowHalf = 0, increment = 0;
	unsigned long long wide;
	unsigned carry;

	for (highest = 575; highest >= 0; --highest)
	{
		if (TCE_TriggerBit(value, highest))
		{
			break;
		}
	}
	precision = (control & _MCW_PC) == _PC_24 ? 24u :
	            (control & _MCW_PC) == _PC_53 ? 53u : 64u;
	drop = highest + 1 - (int)precision;
	if (drop <= 0)
	{
		return;
	}
	for (bit = 0; bit < drop; ++bit)
	{
		if (TCE_TriggerBit(value, bit))
		{
			discarded = 1;
			if (bit < drop - 1)
			{
				belowHalf = 1;
			}
		}
	}
	rounding = control & _MCW_RC;
	if (rounding == _RC_NEAR)
	{
		increment = TCE_TriggerBit(value, drop - 1) &&
		            (belowHalf || TCE_TriggerBit(value, drop));
	}
	else if (rounding == _RC_UP)
	{
		increment = discarded && !value->negative;
	}
	else if (rounding == _RC_DOWN)
	{
		increment = discarded && value->negative;
	}
	for (bit = 0; bit < drop; ++bit)
	{
		value->word[bit / 32] &= ~(1u << (bit % 32));
	}
	if (increment)
	{
		word = drop / 32;
		wide = (unsigned long long)value->word[word] + (1u << (drop % 32));
		value->word[word] = (unsigned)wide;
		carry = (unsigned)(wide >> 32);
		while (carry && ++word < 18)
		{
			++value->word[word];
			carry = value->word[word] == 0;
		}
	}
}

static __inline int TCE_TriggerMagnitude(const tceTriggerSum_t *a,
                                        const tceTriggerSum_t *b)
{
	int word;
	for (word = 17; word >= 0; --word)
	{
		if (a->word[word] != b->word[word])
		{
			return a->word[word] > b->word[word] ? 1 : -1;
		}
	}
	return 0;
}

static __inline int TCE_TriggerCompare(const tceTriggerSum_t *a,
                                      const tceTriggerSum_t *b)
{
	int order;
	tceTriggerSum_t zero = { { 0 }, 0 };
	unsigned aNegative = a->negative && TCE_TriggerMagnitude(a, &zero);
	unsigned bNegative = b->negative && TCE_TriggerMagnitude(b, &zero);
	if (aNegative != bNegative)
	{
		return aNegative ? -1 : 1;
	}
	order = TCE_TriggerMagnitude(a, b);
	return aNegative ? -order : order;
}

static __inline void TCE_TriggerAddExact(tceTriggerSum_t *a,
                                        const tceTriggerSum_t *b)
{
	tceTriggerSum_t result = { { 0 }, 0 };
	const tceTriggerSum_t *large, *small;
	unsigned long long wide;
	unsigned carry = 0, borrow = 0;
	int word, order;
	if (a->negative == b->negative)
	{
		result.negative = a->negative;
		for (word = 0; word < 18; ++word)
		{
			wide = (unsigned long long)a->word[word] + b->word[word] + carry;
			result.word[word] = (unsigned)wide;
			carry = (unsigned)(wide >> 32);
		}
	}
	else
	{
		order = TCE_TriggerMagnitude(a, b);
		large = order >= 0 ? a : b;
		small = order >= 0 ? b : a;
		result.negative = large->negative;
		for (word = 0; word < 18; ++word)
		{
			wide = (unsigned long long)small->word[word] + borrow;
			result.word[word] = large->word[word] - (unsigned)wide;
			borrow = (unsigned long long)large->word[word] < wide;
		}
	}
	*a = result;
}

static __inline void TCE_TriggerAdd(tceTriggerSum_t *a,
                                   const tceTriggerSum_t *b, unsigned control)
{
	TCE_TriggerAddExact(a, b);
	TCE_TriggerRound(a, control);
}

/* An imported float is exact. Product mantissas need at most 48 bits. */
static __inline int TCE_TriggerProduct(tceTriggerSum_t *result, float a,
                                      float b, unsigned control)
{
	unsigned aBits, bBits, aExponent, bExponent, aMantissa, bMantissa;
	memset(result, 0, sizeof(*result));
	memcpy(&aBits, &a, sizeof(aBits));
	memcpy(&bBits, &b, sizeof(bBits));
	aExponent = (aBits >> 23) & 255u;
	bExponent = (bBits >> 23) & 255u;
	if (aExponent == 255u || bExponent == 255u)
	{
		return 0;
	}
	aMantissa = (aBits & 0x7fffffu) | (aExponent ? 0x800000u : 0u);
	bMantissa = (bBits & 0x7fffffu) | (bExponent ? 0x800000u : 0u);
	result->negative = (aBits ^ bBits) >> 31;
	TCE_TriggerPlace(result, (unsigned long long)aMantissa * bMantissa,
	                 (int)(aExponent ? aExponent - 1 : 0) +
	                 (int)(bExponent ? bExponent - 1 : 0));
	TCE_TriggerRound(result, control);
	return 1;
}

static __inline int TCE_TriggerDotAboveDefuse(const float *facing,
                                            const float *direction)
{
	tceTriggerSum_t sum, product, threshold = { { 0 }, 0 };
	unsigned control;
	int axis;
	__control87_2(0, 0, &control, NULL);
	if (!TCE_TriggerProduct(&sum, facing[2], direction[2], control))
	{
		return 0;
	}
	for (axis = 1; axis >= 0; --axis)
	{
		if (!TCE_TriggerProduct(&product, facing[axis], direction[axis], control))
		{
			return 0;
		}
		TCE_TriggerAdd(&sum, &product, control);
	}
	/* Exact binary64 0.707, loaded by FCOMP without arithmetic rounding. */
	TCE_TriggerPlace(&threshold, 0x169fbe76c8b439ULL, 245);
	return TCE_TriggerCompare(&sum, &threshold) > 0;
}

static __inline int TCE_TriggerObjectiveBelow(float minZ, float originZ,
                                             float maxZ, float baseZ)
{
	tceTriggerSum_t left, right, value;
	unsigned control;
	__control87_2(0, 0, &control, NULL);
	/* Multiplication by one imports binary32 exactly at every precision. */
	if (!TCE_TriggerProduct(&left, minZ, 1.0f, control) ||
	    !TCE_TriggerProduct(&value, originZ, 1.0f, control))
	{
		return 0;
	}
	TCE_TriggerAdd(&left, &value, control);
	TCE_TriggerProduct(&value, 4.0f, 1.0f, control);
	TCE_TriggerAdd(&left, &value, control);
	if (!TCE_TriggerProduct(&right, maxZ, 1.0f, control) ||
	    !TCE_TriggerProduct(&value, baseZ, 1.0f, control))
	{
		return 0;
	}
	TCE_TriggerAdd(&right, &value, control);
	return TCE_TriggerCompare(&left, &right) < 0;
}

static __inline int TCE_TriggerDistanceBelow72(const float *direction)
{
	tceTriggerSum_t squared, value, boundary;
	unsigned control, precision, rounding;
	int axis, deltaExponent, order;
	__control87_2(0, 0, &control, NULL);
	if (!TCE_TriggerProduct(&squared, direction[0], direction[0], control))
	{
		return 0;
	}
	for (axis = 1; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&value, direction[axis], direction[axis], control))
		{
			return 0;
		}
		TCE_TriggerAdd(&squared, &value, control);
	}
	TCE_TriggerProduct(&boundary, 5184.0f, 1.0f, control);
	rounding = control & _MCW_RC;
	if (rounding == _RC_DOWN || rounding == _RC_CHOP)
	{
		return TCE_TriggerCompare(&squared, &boundary) < 0;
	}
	precision = (control & _MCW_PC) == _PC_24 ? 24u :
	            (control & _MCW_PC) == _PC_53 ? 53u : 64u;
	/* sqrt rounds below 72 at its midpoint for nearest-even, or at the
	 * preceding representable value for upward rounding. Square that exact
	 * boundary: (72-d)^2 = 5184 - 144*d + d*d. Do not round the boundary. */
	deltaExponent = (rounding == _RC_UP ? 7 : 6) - (int)precision;
	memset(&value, 0, sizeof(value));
	value.negative = 1;
	TCE_TriggerPlace(&value, 144u, 298 + deltaExponent);
	TCE_TriggerAddExact(&boundary, &value);
	memset(&value, 0, sizeof(value));
	TCE_TriggerPlace(&value, 1u, 298 + 2 * deltaExponent);
	TCE_TriggerAddExact(&boundary, &value);
	order = TCE_TriggerCompare(&squared, &boundary);
	return rounding == _RC_UP ? order <= 0 : order < 0;
}
/* Numeric result of masked signed64 truncation, returned as low32. */
static __inline int TCE_FixedSigned64Low32(const tceTriggerSum_t *value)
{
	unsigned low = 0, high = 0;
	int bit, result;

	for (bit = 362; bit < 576; ++bit)
	{
		if (TCE_TriggerBit(value, bit))
		{
			return 0;
		}
	}
	for (bit = 0; bit < 32; ++bit)
	{
		low |= TCE_TriggerBit(value, 298 + bit) << bit;
		high |= TCE_TriggerBit(value, 330 + bit) << bit;
	}
	if (high > 0x80000000u ||
	    (high == 0x80000000u && (!value->negative || low)))
	{
		return 0;
	}
	if (value->negative)
	{
		low = 0u - low;
	}
	memcpy(&result, &low, sizeof(result));
	return result;
}
#endif
#endif
