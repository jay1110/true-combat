#include "q_shared.h"
#include "bg_public.h"
#include "tce_trajectory.h"

#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
typedef char tceDeltaWordLayout_t[(sizeof(int) == 4 && sizeof(unsigned) == 4 &&
                                   sizeof(float) == 4) ? 1 : -1];

static int TCE_DeltaSignedWord(unsigned bits)
{
	int value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

/* These special modes clear in a different order on Windows and Linux. */
static void TCE_ClearSpecialDelta(vec3_t result)
{
#if defined(_WIN32)
	result[1] = 0.0f;
	result[0] = 0.0f;
	result[2] = 0.0f;
#else
	result[2] = 0.0f;
	result[1] = 0.0f;
	result[0] = 0.0f;
#endif
}

static void TCE_ClearDeltaWords(vec3_t result)
{
	unsigned zero = 0;
	int      i;
	for (i = 2; i >= 0; --i)
		memcpy(&result[i], &zero, sizeof(zero));
}

static void TCE_CopyDeltaXY(const trajectory_t *tr, vec3_t result)
{
	unsigned word;
	int      i;
	for (i = 0; i < 2; ++i)
	{
		memcpy(&word, &tr->trDelta[i], sizeof(word));
		memcpy(&result[i], &word, sizeof(word));
	}
}

static void TCE_ScaleNativeDelta(const trajectory_t *tr, vec3_t result,
                                 long double factor)
{
#if defined(_WIN32)
	result[0] = (float)(factor * (long double)tr->trDelta[0]);
	result[1] = (float)(factor * (long double)tr->trDelta[1]);
#else
	result[0] = (float)((long double)tr->trDelta[0] * factor);
	result[1] = (float)((long double)tr->trDelta[1] * factor);
#endif
	result[2] = (float)(factor * (long double)tr->trDelta[2]);
}

static void TCE_CopyDeltaWords(const trajectory_t *tr, vec3_t result)
{
	unsigned word;
	int      i;
	for (i = 0; i < 3; ++i)
	{
		memcpy(&word, &tr->trDelta[i], sizeof(word));
		memcpy(&result[i], &word, sizeof(word));
	}
}
#endif

/* TC IDs and x87 intermediate precision; this is not the derivative of every
 * corresponding position formula. Preserve the original's special cases. */
void TCE_BG_EvaluateTrajectoryDelta(const trajectory_t *tr, int atTime,
                                    vec3_t result, qboolean isAngle, int splinePath, float gravityScale)
{
	int end, milliseconds;
#if !((defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM))
	int    i;
	double time, scale;
#endif
	(void)isAngle;
	(void)splinePath;
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	end          = TCE_DeltaSignedWord((unsigned)tr->trTime + (unsigned)tr->trDuration);
	milliseconds = TCE_DeltaSignedWord((unsigned)atTime - (unsigned)tr->trTime);
#else
	end          = (int)((unsigned)tr->trTime + (unsigned)tr->trDuration);
	milliseconds = (int)((unsigned)atTime - (unsigned)tr->trTime);
	time         = milliseconds * .001;
#endif

	switch ((int)tr->trType)
	{
	case 0: case 1: case 12: case 16:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
		TCE_ClearDeltaWords(result);
#else
		VectorClear(result);
#endif
		return;
	case 3:
		if (atTime > end)
		{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
			TCE_ClearDeltaWords(result);
#else
			VectorClear(result);
#endif
			return;
		}
	/* Before the start, the original still returns the full velocity. */
	case 2:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
		TCE_CopyDeltaWords(tr, result);
#else
		VectorCopy(tr->trDelta, result);
#endif
		return;
	case 5:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		long double phase, factor;
#if defined(_WIN32)
		phase  = (long double)milliseconds / (long double)tr->trDuration;
		phase *= (long double)6.2831854820251465f;
		factor = cosl(phase) * (long double)0.5;
#else
		long double    reciprocal = 1.0L / (long double)tr->trDuration;
		volatile float wave, roundedFactor;
		phase         = reciprocal * (long double)milliseconds;
		phase        *= (long double)3.141592653589793;
		phase         = phase + phase;
		wave          = (float)cosl(phase);
		roundedFactor = (float)((long double)wave * (long double)0.5f);
		factor        = (long double)roundedFactor;
#endif
		/* cosl/FCOS exception and argument-reduction parity stays open. */
		TCE_ScaleNativeDelta(tr, result, factor);
		return;
	}
#else
		scale = cos(((double)milliseconds / tr->trDuration) *
		            (double)6.2831854820251465f) * .5;
		break;
#endif
	case 6: case 7: case 8: case 13:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		int         kind = (int)tr->trType;
		long double heldTime, acceleration, gravity;
#if defined(_WIN32)
		heldTime = (long double)milliseconds;
		TCE_CopyDeltaXY(tr, result);
		heldTime *= (long double)0.001;
#else
		volatile float roundedTime = (float)((long double)milliseconds * (long double)0.001);
		heldTime = (long double)roundedTime;
		TCE_CopyDeltaXY(tr, result);
#endif
		acceleration = kind == 6 ? 800.0L : kind == 7 ? 400.0L : kind == 8 ? 200.0L : 500.0L;
		gravity      = heldTime * acceleration;
		result[2]    = (float)((long double)tr->trDelta[2] - gravity);
		return;
	}
#else
		scale = tr->trType == 6 ? 800 : tr->trType == 7 ? 400 :
		        tr->trType == 8 ? 200 : 500;
		VectorCopy(tr->trDelta, result);
		result[2] = (float)(tr->trDelta[2] - time * scale);
		return;
#endif
	case 10: case 11:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		long double heldTime, factor;
#if !defined(_WIN32)
		volatile float roundedTime;
#endif
		if (atTime > end)
		{
			result[2] = 0.0f;
			result[1] = 0.0f;
			result[0] = 0.0f;
			return;
		}
		heldTime = (long double)milliseconds * (long double)0.001;
#if !defined(_WIN32)
		roundedTime = (float)heldTime;
		heldTime    = (long double)roundedTime;
#endif
		factor = (int)tr->trType == 10 ? heldTime * heldTime : heldTime;
		TCE_ScaleNativeDelta(tr, result, factor);
		return;
	}
#else
		if (atTime > end)
		{
			VectorClear(result); return;
		}
		scale = tr->trType == 10 ? time * time : time;
		break;
#endif
	case 14:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		long double gravity;
		if (atTime > end)
		{
			TCE_ClearSpecialDelta(result);
		}
		else
		{
			TCE_CopyDeltaWords(tr, result);
		}
#if defined(_WIN32)
		gravity  = (long double)tr->trDuration * (long double)0.005f;
		gravity *= (long double)8.0f;
#else
		gravity = (long double)tr->trDuration * (long double)0.04f;
#endif
		result[2] = (float)((long double)result[2] - gravity);
		return;
	}
#else
		if (atTime > end)
		{
			VectorClear(result);
		}
		else
		{
			VectorCopy(tr->trDelta, result);
		}
		result[2] = (float)(result[2] - (double)tr->trDuration * .005f * 8);
		return;
#endif
	case 15:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		long double    heldTime = (long double)milliseconds * (long double)0.001;
		long double    gravity;
		volatile float *storedZ = &result[2];
#if !defined(_WIN32)
		volatile float roundedTime = (float)heldTime;
		heldTime = (long double)roundedTime;
#endif
		if (atTime > end)
		{
#if defined(_WIN32)
			gravity = (long double)gravityScale * (long double)16.0f;
			TCE_ClearSpecialDelta(result);
#else
			TCE_ClearSpecialDelta(result);
			gravity = (long double)gravityScale * (long double)16.0f;
#endif
		}
		else
		{
#if defined(_WIN32)
			result[0] = (float)(heldTime * (long double)tr->trDelta[0]);
			result[1] = (float)(heldTime * (long double)tr->trDelta[1]);
#else
			result[0] = (float)((long double)tr->trDelta[0] * heldTime);
			result[1] = (float)((long double)tr->trDelta[1] * heldTime);
#endif
			*storedZ = (float)(heldTime * (long double)tr->trDelta[2]);
			gravity  = (long double)gravityScale * (long double)16.0f;
		}
		*storedZ = (float)((long double)*storedZ - gravity);
		return;
	}
#else
		if (atTime > end)
		{
			VectorClear(result);
		}
		else
		{
			for (i = 0; i < 3; ++i)
				result[i] = (float)(time * tr->trDelta[i]);
		}
		/* Z velocity is stored as float before subtracting the gravity term. */
		result[2] = (float)(result[2] - (double)gravityScale * 16);
		return;
#endif
	default:
		/* Including paused gravity (9); original diagnostic prints trTime. */
		Com_Error(ERR_DROP, "BG_EvaluateTrajectoryDelta: unknown trType: %i", tr->trTime);
		return;
	}
#if !((defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM))
	for (i = 0; i < 3; ++i)
		result[i] = (float)(scale * tr->trDelta[i]);
#endif
}
