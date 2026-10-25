#ifndef TCE_FRICTION_PRECISION_H
#define TCE_FRICTION_PRECISION_H

#if defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
#include "tce_length_precision.h"
#include "tce_gust_precision.h"

/* Windows retains FSQRT for the low-speed gate while storing a separate
 * Float speed. Do not pass that comparison through MSVC's Double return. */
static __inline int TCE_FrictionFiniteSpeed(const float *vector,
                                            volatile float *stored, int *lowSpeed)
{
	tceTriggerSum_t squared, product, root, one;
	unsigned control;
	int axis;

	__control87_2(0, 0, &control, NULL);
	if (!TCE_FiniteExceptionsMasked(control) ||
	    !TCE_TriggerProduct(&squared, vector[0], vector[0], control))
	{
		return 0;
	}
	for (axis = 1; axis < 3; ++axis)
	{
		if (!TCE_TriggerProduct(&product, vector[axis], vector[axis], control))
		{
			return 0;
		}
		TCE_RotateAdd(&squared, &product, control);
	}
	root = TCE_LengthRoot(&squared, control);
	TCE_TriggerProduct(&one, 1.0f, 1.0f, control);
	*stored = TCE_RotateStore(root, control);
	*lowSpeed = TCE_TriggerCompare(&root, &one) < 0;
	return 1;
}

typedef struct
{
	tceTriggerSum_t drop;
	unsigned control;
	int active;
} tceFrictionArithmetic_t;

/* Reserve six high bits for additions and rounding carries. Reject rather
 * than silently discard significant product bits outside the fixed grid. */
static __inline int TCE_FrictionFits(const tceTriggerSum_t *value)
{
	int bit;
	for (bit = 570; bit < 576; ++bit)
	{
		if (TCE_TriggerBit(value, bit))
		{
			return 0;
		}
	}
	return 1;
}

static __inline int TCE_FrictionMultiply(tceTriggerSum_t *value,
                                         const tceTriggerSum_t *factor, unsigned control)
{
	int lowA, lowB, highA, highB;
	for (lowA = 0; lowA < 576 && !TCE_TriggerBit(value, lowA); ++lowA)
	{
	}
	for (lowB = 0; lowB < 576 && !TCE_TriggerBit(factor, lowB); ++lowB)
	{
	}
	if (lowA < 576 && lowB < 576)
	{
		for (highA = 575; !TCE_TriggerBit(value, highA); --highA)
		{
		}
		for (highB = 575; !TCE_TriggerBit(factor, highB); --highB)
		{
		}
		if (lowA + lowB < 298 || highA + highB > 866)
		{
			return 0;
		}
	}
	TCE_GustMultiply(value, factor, control);
	return TCE_FrictionFits(value);
}

static __inline int TCE_FrictionMultiplyFloat(tceTriggerSum_t *value,
                                              float factor, unsigned control)
{
	tceTriggerSum_t stored;
	return TCE_TriggerProduct(&stored, factor, 1.0f, control) &&
	       TCE_FrictionMultiply(value, &stored, control);
}

static __inline void TCE_FrictionBegin(tceFrictionArithmetic_t *state)
{
	memset(state, 0, sizeof(*state));
	__control87_2(0, 0, &state->control, NULL);
	state->active = TCE_FiniteExceptionsMasked(state->control);
}

static __inline void TCE_FrictionAppend(tceFrictionArithmetic_t *state,
                                        const tceTriggerSum_t *term)
{
	TCE_RotateAdd(&state->drop, term, state->control);
	state->active = TCE_FrictionFits(&state->drop);
}

/* Dodge replaces the initial zero; the other three-factor terms add to it. */
static __inline void TCE_FrictionTerm(tceFrictionArithmetic_t *state,
                                      float first, float second, float third, int replace)
{
	tceTriggerSum_t term;
	if (!state->active)
	{
		return;
	}
	if (!TCE_TriggerProduct(&term, first, second, state->control) ||
	    !TCE_FrictionMultiplyFloat(&term, third, state->control))
	{
		state->active = 0;
		return;
	}
	if (replace)
	{
		state->drop = term;
	}
	else
	{
		TCE_FrictionAppend(state, &term);
	}
}

static __inline void TCE_FrictionGround(tceFrictionArithmetic_t *state,
                                        float friction, float frame, float stop, float speed)
{
	tceTriggerSum_t threshold, storedSpeed, term;
	if (!state->active)
	{
		return;
	}
	if (!TCE_TriggerProduct(&threshold, stop, 0.5f, state->control) ||
	    !TCE_TriggerProduct(&storedSpeed, speed, 1.0f, state->control) ||
	    !TCE_TriggerProduct(&term, friction, frame, state->control))
	{
		state->active = 0;
		return;
	}
	if (TCE_TriggerCompare(&storedSpeed, &threshold) >= 0)
	{
		threshold = storedSpeed;
	}
	if (!TCE_FrictionMultiply(&term, &threshold, state->control))
	{
		state->active = 0;
		return;
	}
	TCE_FrictionAppend(state, &term);
}

static __inline void TCE_FrictionWater(tceFrictionArithmetic_t *state,
                                       int level, float friction, float frame, float speed)
{
	tceTriggerSum_t term = { { 0 }, 0 };
	unsigned magnitude;
	if (!state->active)
	{
		return;
	}
	term.negative = level < 0;
	magnitude = level < 0 ? 0u - (unsigned)level : (unsigned)level;
	TCE_TriggerPlace(&term, magnitude, 298);
	if (!TCE_FrictionMultiplyFloat(&term, friction, state->control) ||
	    !TCE_FrictionMultiplyFloat(&term, frame, state->control) ||
	    !TCE_FrictionMultiplyFloat(&term, speed, state->control))
	{
		state->active = 0;
		return;
	}
	TCE_FrictionAppend(state, &term);
}

static __inline int TCE_FrictionFinish(const tceFrictionArithmetic_t *state,
                                       float speed, int freeMovement, volatile float *stored)
{
	tceTriggerSum_t remaining, negativeDrop, fraction, one;
	tceTriggerSum_t zero = { { 0 }, 0 };
	float result;
	if (!state->active || !(speed > 0.0f) ||
	    !TCE_TriggerProduct(&remaining, speed, 1.0f, state->control))
	{
		return 0;
	}
	negativeDrop = state->drop;
	negativeDrop.negative ^= 1u;
	TCE_RotateAdd(&remaining, &negativeDrop, state->control);
	if (!TCE_FrictionFits(&remaining))
	{
		return 0;
	}
	if (remaining.negative && TCE_TriggerMagnitude(&remaining, &zero))
	{
		remaining = zero;
	}
	if (!TCE_FixedDivideFloat(&remaining, speed, state->control, &fraction))
	{
		return 0;
	}
	result = TCE_RotateStore(fraction, state->control);
	TCE_TriggerProduct(&one, 1.0f, 1.0f, state->control);
	if (freeMovement && TCE_TriggerCompare(&state->drop, &one) < 0 && speed < 3.0f)
	{
		result = 0.0f;
	}
	*stored = result;
	return 1;
}

static __inline float TCE_FrictionScale(float ratio, float component)
{
	float stored;
	if (TCE_FiniteProductStore(ratio, component, &stored))
	{
		return stored;
	}
	return (float)((long double)ratio * component);
}
#endif

#endif
