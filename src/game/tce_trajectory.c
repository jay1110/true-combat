#include "q_shared.h"
#include "bg_public.h"
#include "tce_trajectory.h"

void BG_CalculateSpline_r(splinePath_t *, vec3_t, vec3_t, float);
qboolean BG_TraverseSpline(float *, splinePath_t **);
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
typedef char tcePositionWordLayout_t[(sizeof(int) == 4 && sizeof(unsigned) == 4 &&
                                      sizeof(float) == 4) ? 1 : -1];

static int TCE_PositionSignedWord(unsigned bits)
{
	int value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

static void TCE_CopyPositionBase(const trajectory_t *tr, vec3_t result)
{
	unsigned word;
	int      i;
	for (i = 0; i < 3; ++i)
	{
		memcpy(&word, &tr->trBase[i], sizeof(word));
		memcpy(&result[i], &word, sizeof(word));
	}
}

static void TCE_LinearPositionVector(const trajectory_t *tr, vec3_t result,
                                     long double factor)
{
	long double product;
#if defined(_WIN32)
	product = factor * (long double)tr->trDelta[0];
#else
	product = (long double)tr->trDelta[0] * factor;
#endif
	result[0] = (float)(product + (long double)tr->trBase[0]);
#if defined(_WIN32)
	product = factor * (long double)tr->trDelta[1];
#else
	product = (long double)tr->trDelta[1] * factor;
#endif
	result[1] = (float)(product + (long double)tr->trBase[1]);
	product   = factor * (long double)tr->trDelta[2];
	result[2] = (float)(product + (long double)tr->trBase[2]);
}

static void TCE_LinearPosition(const trajectory_t *tr, int milliseconds,
                               qboolean clampNegative, vec3_t result)
{
	long double factor = (long double)milliseconds * (long double)0.001;
#if !defined(_WIN32)
	volatile float roundedTime = (float)factor;
	factor = (long double)roundedTime;
#endif
	if (clampNegative)
	{
#if defined(_WIN32)
		if (!(factor >= 0.0L))
		{
			factor = 0.0L;
		}
#else
		if (factor < 0.0L)
		{
			factor = 0.0L;
		}
#endif
	}
	TCE_LinearPositionVector(tr, result, factor);
}
#endif

#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
#if !defined(_WIN32)
static void TCE_SplineHeldPosition(const vec3_t a, const vec3_t b, float t,
                                   long double position[3])
{
	long double startX = (long double)a[0], startY = (long double)a[1];
	long double startZ = (long double)a[2], endX = (long double)b[0];
	long double endY = (long double)b[1], endZ = (long double)b[2];
	long double differenceX, differenceY, differenceZ, heldTime;
	long double productX, productY, productZ;
	differenceX = endX - startX;
	differenceY = endY - startY;
	heldTime    = (long double)t;
	differenceZ = endZ - startZ;
	productX    = differenceX * heldTime;
	productY    = differenceY * heldTime;
	productZ    = heldTime * differenceZ;
	position[0] = startX + productX;
	position[1] = startY + productY;
	position[2] = startZ + productZ;
}
#endif

static void TCE_SplinePosition(const vec3_t a, const vec3_t b, float t,
                               vec3_t result)
{
#if defined(_WIN32)
	long double difference, product;
	result[2]  = (float)((long double)b[2] - (long double)a[2]);
	difference = (long double)b[0] - (long double)a[0];
	product    = difference * (long double)t;
	result[0]  = (float)(product + (long double)a[0]);
	difference = (long double)b[1] - (long double)a[1];
	product    = difference * (long double)t;
	result[1]  = (float)(product + (long double)a[1]);
	product    = (long double)t * (long double)result[2];
	result[2]  = (float)(product + (long double)a[2]);
#else
	long double position[3];
	TCE_SplineHeldPosition(a, b, t, position);
	result[0] = (float)position[0];
	result[1] = (float)position[1];
	result[2] = (float)position[2];
#endif
}
#endif

static int elapsed(int a, int b)
{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	return TCE_PositionSignedWord((unsigned)a - (unsigned)b);
#else
	return (int)((unsigned)a - (unsigned)b);
#endif
}
static void interpolate(const vec3_t a, const vec3_t b, float fraction, vec3_t result)
{
	result[2] = b[2] - a[2];
	result[0] = (float)(((double)b[0] - a[0]) * fraction + a[0]);
	result[1] = (float)(((double)b[1] - a[1]) * fraction + a[1]);
	result[2] = (float)((double)fraction * result[2] + a[2]);
}
void TCE_BG_EvaluateTrajectory(const trajectory_t *tr, int atTime, vec3_t result,
                               qboolean isAngle, int splinePath, float gravityScale)
{
	double       time, scale, roll;
	float        t, acceleration, frac, t2;
	int          i, index, end;
	vec3_t       a, b, v;
	splinePath_t *path, *other;
	qboolean     backwards = qfalse, dampIn = qfalse, dampOut = qfalse;
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	end = TCE_PositionSignedWord((unsigned)tr->trTime + (unsigned)tr->trDuration);
	if ((int)tr->trType != 0 && (int)tr->trType != 1 && (int)tr->trType != 2 &&
	    (int)tr->trType != 3 && (int)tr->trType != 5 && (int)tr->trType != 9 &&
	    (int)tr->trType != 6 && (int)tr->trType != 7 && (int)tr->trType != 8 &&
	    (int)tr->trType != 13 && (int)tr->trType != 14 &&
	    (int)tr->trType != 10 && (int)tr->trType != 11 &&
	    (int)tr->trType != 12 && (int)tr->trType != 15 && (int)tr->trType != 16)
	{
		time = elapsed(atTime, tr->trTime) * .001;
	}
#else
	time = elapsed(atTime, tr->trTime) * .001;
	end  = (int)((unsigned)tr->trTime + (unsigned)tr->trDuration);
#endif
	switch ((int)tr->trType)
	{
	case 0: case 1: case 9:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
		TCE_CopyPositionBase(tr, result);
#else
		VectorCopy(tr->trBase, result);
#endif
		return;
	case 2:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
		TCE_LinearPosition(tr, elapsed(atTime, tr->trTime), qfalse, result);
		return;
#else
		break;
#endif
	case 3:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
		if (atTime > end)
		{
			atTime = end;
		}
		TCE_LinearPosition(tr, elapsed(atTime, tr->trTime), qtrue, result);
		return;
#else
		if (atTime > end)
		{
			time = elapsed(end, tr->trTime) * .001;
		}
		if (time < 0)
		{
			time = 0;
		}
		break;
#endif
	case 5:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		int         milliseconds = elapsed(atTime, tr->trTime);
		long double phase, factor;
#if defined(_WIN32)
		phase  = (long double)milliseconds / (long double)tr->trDuration;
		phase *= (long double)6.2831854820251465f;
		factor = sinl(phase);
#else
		long double    reciprocal = 1.0L / (long double)tr->trDuration;
		long double    fraction   = reciprocal * (long double)milliseconds;
		volatile float fractionSpill, wave;
		phase = (long double)3.141592653589793 * fraction;
		/* The original spills fraction but retains the wide phase. */
		fractionSpill = (float)fraction;
		phase         = phase + phase;
		wave          = (float)sinl(phase);
		factor        = (long double)wave;
#endif
		/* sinl/FSIN argument reduction and FPU exception parity is open. */
		TCE_LinearPositionVector(tr, result, factor);
		return;
	}
#else
		time = sin(((double)elapsed(atTime, tr->trTime) / tr->trDuration) * (double)6.2831854820251465f); break;
#endif
	case 6: case 7: case 8: case 13:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		int         kind   = (int)tr->trType;
		long double factor = (long double)elapsed(atTime, tr->trTime) * (long double)0.001;
		long double product, positionZ, fall;
#if !defined(_WIN32)
		long double velocityZ;
#endif
		long double halfGravity = kind == 6 ? 400.0L : kind == 7 ? 200.0L : kind == 8 ? 100.0L : 250.0L;
#if !defined(_WIN32)
		volatile float roundedTime = (float)factor;
		factor = (long double)roundedTime;
#endif
#if defined(_WIN32)
		product = factor * (long double)tr->trDelta[0];
#else
		product = (long double)tr->trDelta[0] * factor;
#endif
		result[0] = (float)(product + (long double)tr->trBase[0]);
#if defined(_WIN32)
		product = factor * (long double)tr->trDelta[1];
#else
		product = (long double)tr->trDelta[1] * factor;
#endif
		result[1] = (float)(product + (long double)tr->trBase[1]);
#if defined(_WIN32)
		product   = factor * (long double)tr->trDelta[2];
		positionZ = product + (long double)tr->trBase[2];
		fall      = factor * factor;
		fall     *= halfGravity;
#else
		fall      = factor * factor;
		velocityZ = (long double)tr->trDelta[2];
		fall     *= halfGravity;
		product   = factor * velocityZ;
		positionZ = product + (long double)tr->trBase[2];
#endif
		result[2] = (float)(positionZ - fall);
		return;
	}
#else
		for (i = 0; i < 2; ++i)
			result[i] = (float)(time * tr->trDelta[i] + tr->trBase[i]);
		scale     = tr->trType == 6?400:tr->trType == 7?200:tr->trType == 8?100:250;
		result[2] = (float)((time * tr->trDelta[2] + tr->trBase[2]) - time * time * scale);
		return;
#endif
	case 14:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		long double    rawTime = (long double)elapsed(atTime, tr->trTime) * (long double)0.001;
		long double    maximum, factor, product, positionZ, gravity;
		volatile float movement = (float)rawTime;
		volatile float *storedZ = &result[2];
#if defined(_WIN32)
		if (!(rawTime >= 0.0L))
		{
			movement = 0.0f;
		}
		else
		{
			maximum = (long double)tr->trDuration * (long double)0.001;
			if ((long double)movement > maximum)
			{
				movement = (float)maximum;
			}
		}
#else
		volatile float unboundedTime;
		if (!((long double)movement >= 0.0L))
		{
			movement = 0.0f;
		}
		else
		{
			maximum = (long double)tr->trDuration * (long double)0.001;
			if (!(maximum >= (long double)movement))
			{
				movement = (float)maximum;
			}
		}
#endif
		factor = (long double)movement;
#if defined(_WIN32)
		product = factor * (long double)tr->trDelta[0];
#else
		product = (long double)tr->trDelta[0] * factor;
#endif
		result[0] = (float)(product + (long double)tr->trBase[0]);
#if defined(_WIN32)
		product = factor * (long double)tr->trDelta[1];
#else
		product = (long double)tr->trDelta[1] * factor;
#endif
		result[1] = (float)(product + (long double)tr->trBase[1]);
		product   = factor * (long double)tr->trDelta[2];
		positionZ = product + (long double)tr->trBase[2];
		*storedZ  = (float)positionZ;
		/* Original resamples time/duration after the position stores. */
		rawTime = (long double)elapsed(atTime, tr->trTime) * (long double)0.001;
#if defined(_WIN32)
		gravity  = (long double)tr->trDuration * (long double)0.005f;
		gravity *= rawTime;
		gravity *= (long double)8.0f;
		*storedZ = (float)((long double)*storedZ - gravity);
#else
		unboundedTime = (float)rawTime;
		gravity       = (long double)unboundedTime * (long double)tr->trDuration;
		gravity      *= (long double)0.04f;
		*storedZ      = (float)(positionZ - gravity);
#endif
		return;
	}
#else
		t = (float)time;
		if (time < 0)
		{
			t = 0;
		}
		else if (t > tr->trDuration * .001)
		{
			t = (float)(tr->trDuration * .001);
		}
		for (i = 0; i < 3; ++i)
			result[i] = (float)((double)t * tr->trDelta[i] + tr->trBase[i]);
		result[2] = (float)(result[2] - ((double)tr->trDuration * .005f) * time * 8.0f);
		return;
#endif
	case 10:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		int            sampleTime = atTime;
		volatile float movement, accelerationValue;
		long double    length, duration, factor, product;
#if !defined(_WIN32)
		long double reciprocal, heldTime, normalY, normalZ, productY, productZ;
#endif
		if (sampleTime > end)
		{
			sampleTime = end;
		}
		movement = (float)((long double)elapsed(sampleTime, tr->trTime) * (long double)0.001);
		length   = TCE_VectorLengthValue(tr->trDelta);
		duration = (long double)tr->trDuration * (long double)0.001;
#if defined(_WIN32)
		accelerationValue = (float)(length / duration);
#else
		reciprocal        = 1.0L / duration;
		accelerationValue = (float)(length * reciprocal);
#endif
		(void)VectorNormalize2(tr->trDelta, result);
#if defined(_WIN32)
		factor    = (long double)accelerationValue * (long double)movement;
		factor   *= (long double)movement;
		factor   *= (long double)0.5;
		product   = (long double)result[0] * factor;
		result[0] = (float)(product + (long double)tr->trBase[0]);
		product   = (long double)result[1] * factor;
		result[1] = (float)(product + (long double)tr->trBase[1]);
		product   = (long double)result[2] * factor;
		result[2] = (float)(product + (long double)tr->trBase[2]);
#else
		heldTime  = (long double)movement;
		factor    = (long double)accelerationValue * heldTime;
		factor    = heldTime * factor;
		product   = factor * (long double)result[0];
		product  *= (long double)0.5f;
		result[0] = (float)(product + (long double)tr->trBase[0]);
		normalY   = (long double)result[1];
		normalZ   = (long double)result[2];
		productY  = factor * normalY;
		productZ  = factor * normalZ;
		productY *= (long double)0.5f;
		productZ  = (long double)0.5f * productZ;
		result[1] = (float)(productY + (long double)tr->trBase[1]);
		result[2] = (float)(productZ + (long double)tr->trBase[2]);
#endif
		return;
	}
#endif
	/* The nonnative mode10 keeps the shared fallback below. */
	case 11:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		int            sampleTime = atTime;
		volatile float movement, accelerationValue;
		long double    length, duration, factor, product, baseX, baseZ;
#if defined(_WIN32)
		volatile float baseY;
#else
		long double     reciprocal, heldTime, negativeAcceleration, accelerationTime;
		long double     baseY, normalY, normalZ, productY, productZ;
		volatile vec3_t baseSpill;
#endif
		if (sampleTime > end)
		{
			sampleTime = end;
		}
		movement = (float)((long double)elapsed(sampleTime, tr->trTime) * (long double)0.001);
		length   = TCE_VectorLengthValue(tr->trDelta);
		duration = (long double)tr->trDuration * (long double)0.001;
#if defined(_WIN32)
		accelerationValue = (float)(length / duration);
#else
		reciprocal        = 1.0L / duration;
		accelerationValue = (float)(length * reciprocal);
#endif
		(void)VectorNormalize2(tr->trDelta, result);
#if defined(_WIN32)
		product   = (long double)movement * (long double)tr->trDelta[0];
		baseX     = product + (long double)tr->trBase[0];
		product   = (long double)movement * (long double)tr->trDelta[1];
		baseY     = (float)(product + (long double)tr->trBase[1]);
		product   = (long double)movement * (long double)tr->trDelta[2];
		baseZ     = product + (long double)tr->trBase[2];
		factor    = -(long double)accelerationValue;
		factor   *= (long double)movement;
		factor   *= (long double)movement;
		factor   *= (long double)0.5;
		product   = (long double)result[0] * factor;
		result[0] = (float)(product + baseX);
		product   = (long double)result[1] * factor;
		result[1] = (float)(product + (long double)baseY);
		product   = (long double)result[2] * factor;
		result[2] = (float)(product + baseZ);
#else
		heldTime             = (long double)movement;
		product              = (long double)tr->trDelta[0] * heldTime;
		negativeAcceleration = -(long double)accelerationValue;
		baseX                = product + (long double)tr->trBase[0];
		baseSpill[0]         = (float)baseX;
		product              = (long double)tr->trDelta[1] * heldTime;
		baseY                = product + (long double)tr->trBase[1];
		baseSpill[1]         = (float)baseY;
		product              = (long double)tr->trDelta[2] * heldTime;
		accelerationTime     = negativeAcceleration * heldTime;
		baseZ                = product + (long double)tr->trBase[2];
		factor               = heldTime * accelerationTime;
		/* Original Float spills do not replace the held base sums. */
		baseSpill[2] = (float)baseZ;
		product      = factor * (long double)result[0];
		product     *= (long double)0.5f;
		result[0]    = (float)(baseX + product);
		normalY      = (long double)result[1];
		normalZ      = (long double)result[2];
		productY     = factor * normalY;
		productZ     = factor * normalZ;
		productY    *= (long double)0.5f;
		productZ     = (long double)0.5f * productZ;
		result[1]    = (float)(baseY + productY);
		result[2]    = (float)(baseZ + productZ);
#endif
		return;
	}
#endif
	/* The nonnative mode11 keeps the shared fallback below. */
	case 15:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		volatile float *storedZ = &result[2];
		long double    rawTime, maximum, gravity;
#if !defined(_WIN32)
		volatile float unboundedTime, positionZ;
#endif
		volatile float movement, accelerationValue;
		long double    length, duration, factor, product, baseX, baseZ;
#if defined(_WIN32)
		volatile float baseY;
#else
		long double     reciprocal, heldTime, negativeAcceleration, accelerationTime;
		long double     baseY, normalY, normalZ, productY, productZ;
		volatile vec3_t baseSpill;
#endif
		rawTime  = (long double)elapsed(atTime, tr->trTime) * (long double)0.001;
		movement = (float)rawTime;
#if !defined(_WIN32)
		rawTime = (long double)movement;
#endif
		if (!(rawTime >= 0.0L))
		{
			movement = 0.0f;
		}
		else
		{
			maximum = (long double)tr->trDuration * (long double)0.001;
			if ((long double)movement > maximum)
			{
				movement = (float)maximum;
			}
		}
		length   = TCE_VectorLengthValue(tr->trDelta);
		duration = (long double)tr->trDuration * (long double)0.001;
#if defined(_WIN32)
		accelerationValue = (float)(length / duration);
#else
		reciprocal        = 1.0L / duration;
		accelerationValue = (float)(length * reciprocal);
#endif
		(void)VectorNormalize2(tr->trDelta, result);
#if defined(_WIN32)
		product   = (long double)movement * (long double)tr->trDelta[0];
		baseX     = product + (long double)tr->trBase[0];
		product   = (long double)movement * (long double)tr->trDelta[1];
		baseY     = (float)(product + (long double)tr->trBase[1]);
		product   = (long double)movement * (long double)tr->trDelta[2];
		baseZ     = product + (long double)tr->trBase[2];
		factor    = -(long double)accelerationValue;
		factor   *= (long double)movement;
		factor   *= (long double)movement;
		factor   *= (long double)0.5;
		product   = (long double)result[0] * factor;
		result[0] = (float)(product + baseX);
		product   = (long double)result[1] * factor;
		result[1] = (float)(product + (long double)baseY);
		product   = (long double)result[2] * factor;
		*storedZ  = (float)(product + baseZ);
#else
		heldTime             = (long double)movement;
		product              = (long double)tr->trDelta[0] * heldTime;
		negativeAcceleration = -(long double)accelerationValue;
		baseX                = product + (long double)tr->trBase[0];
		baseSpill[0]         = (float)baseX;
		product              = (long double)tr->trDelta[1] * heldTime;
		baseY                = product + (long double)tr->trBase[1];
		baseSpill[1]         = (float)baseY;
		product              = (long double)tr->trDelta[2] * heldTime;
		accelerationTime     = negativeAcceleration * heldTime;
		baseZ                = product + (long double)tr->trBase[2];
		factor               = heldTime * accelerationTime;
		/* Original Float spills do not replace the held base sums. */
		baseSpill[2] = (float)baseZ;
		product      = factor * (long double)result[0];
		product     *= (long double)0.5f;
		result[0]    = (float)(baseX + product);
		normalY      = (long double)result[1];
		normalZ      = (long double)result[2];
		productY     = factor * normalY;
		productZ     = factor * normalZ;
		productY    *= (long double)0.5f;
		productZ     = (long double)0.5f * productZ;
		result[1]    = (float)(baseY + productY);
		positionZ    = (float)(baseZ + productZ);
		*storedZ     = positionZ;
#endif
		/* Resample after XYZ stores: result may alias trajectory fields. */
		rawTime = (long double)elapsed(atTime, tr->trTime) * (long double)0.001;
#if defined(_WIN32)
		gravity  = rawTime * (long double)gravityScale;
		gravity *= (long double)16.0f;
		*storedZ = (float)((long double)*storedZ - gravity);
#else
		unboundedTime = (float)rawTime;
		gravity       = (long double)unboundedTime * (long double)gravityScale;
		gravity      *= (long double)16.0f;
		*storedZ      = (float)((long double)positionZ - gravity);
#endif
		return;
	}
#endif
		/* Nonnative modes10/11/15 retain their shared fallback below. */

		if (tr->trType != 15 && atTime > end)
		{
			time = elapsed(end, tr->trTime) * .001;
		}
		t = (float)time;
		if (tr->trType == 15)
		{
			if (time < 0)
			{
				t = 0;
			}
			else if (t > tr->trDuration * .001)
			{
				t = (float)(tr->trDuration * .001);
			}
		}
		scale        = sqrt((double)tr->trDelta[0] * tr->trDelta[0] + (double)tr->trDelta[1] * tr->trDelta[1] + (double)tr->trDelta[2] * tr->trDelta[2]);
		acceleration = (float)(scale / (tr->trDuration * .001));
		VectorNormalize2(tr->trDelta, result);
		scale = (tr->trType == 10?(double)acceleration:-(double)acceleration) * t * t * .5;
		for (i = 0; i < 3; ++i)
		{
			double base = tr->trBase[i];
			if (tr->trType != 10)
			{
				base += (double)t * tr->trDelta[i]; if (i == 1)
				{
					base = (float)base;
				}
			}
			result[i] = (float)(base + result[i] * scale);
		}
		if (tr->trType == 15)
		{
			result[2] = (float)(result[2] - elapsed(atTime, tr->trTime) * .001 * gravityScale * 16.0f);
		}
		return;
	case 16:
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	{
		int            duration, segment;
		long double    fraction = 0.0L, scaled, distance, product;
		volatile float fractionSpill;
		float          offset;
		qboolean       reverse;
		path = BG_GetSplineData(splinePath, &backwards);
		if (!path)
		{
			return;
		}
		duration = tr->trDuration;
		if (duration != 0)
		{
#if defined(_WIN32)
			fraction = (long double)elapsed(atTime, tr->trTime) / (long double)duration;
#else
			fraction  = 1.0L / (long double)duration;
			fraction *= (long double)elapsed(atTime, tr->trTime);
#endif
		}
		fractionSpill = (float)fraction;
#if defined(_WIN32)
		fraction = (long double)fractionSpill;
#endif
		if (!(fraction >= 0.0L))
		{
			fraction = 0.0L;
		}
		else
		{
#if defined(_WIN32)
			if (fraction > 1.0L)
			{
				fraction = 1.0L;
			}
#else
			if (!(1.0L >= fraction))
			{
				fraction = 1.0L;
			}
#endif
		}
		if (backwards)
		{
			fraction = 1.0L - fraction;
		}
		t = (float)fraction;
#if defined(_WIN32)
		fraction = (long double)t;
#endif
		scaled = fraction * (long double)16.0f;
#if defined(_WIN32)
		{
			volatile double floorInput = (double)scaled;
			segment = (int)floor(floorInput);
		}
#else
		{
			volatile double integral = (double)floorl(scaled);
			segment = (int)integral;
		}
#endif
		if (segment >= 16)
		{
			segment  = 15;
			distance = (long double)path->segments[15].length;
		}
		else
		{
#if defined(_WIN32)
			/* The Windows branch reloads Float t after the CRT call. */
			scaled = (long double)t * (long double)16.0f;
#endif
			distance  = scaled - (long double)segment;
			distance *= (long double)path->segments[segment].length;
		}
		if (isAngle)
		{
			offset = tr->trBase[0];
			/* The original equality flag also takes this path for NaN. */
			if (offset == 0.0f || offset != offset)
			{
				vectoangles(path->segments[segment].v_norm, result);
				return;
			}
		}
#if defined(_WIN32)
		product = distance * (long double)path->segments[segment].v_norm[0];
#else
		product = (long double)path->segments[segment].v_norm[0] * distance;
#endif
		result[0] = (float)(product + (long double)path->segments[segment].start[0]);
#if defined(_WIN32)
		product = distance * (long double)path->segments[segment].v_norm[1];
#else
		product = (long double)path->segments[segment].v_norm[1] * distance;
#endif
		result[1] = (float)(product + (long double)path->segments[segment].start[1]);
		product   = distance * (long double)path->segments[segment].v_norm[2];
		result[2] = (float)(product + (long double)path->segments[segment].start[2]);
		if (!isAngle)
		{
			return;
		}
		VectorCopy(result, v);
		BG_LinearPathOrigin2(tr->trBase[0], &path, &t, v, backwards);
		offset = tr->trBase[0];
#if defined(_WIN32)
		reverse = !(offset >= 0.0f);
#else
		reverse = offset < 0.0f;
#endif
		if (reverse)
		{
			for (i = 0; i < 3; ++i)
				result[i] = (float)((long double)v[i] - (long double)result[i]);
		}
		else
		{
			for (i = 0; i < 3; ++i)
				result[i] = (float)((long double)result[i] - (long double)v[i]);
		}
		vectoangles(result, result);
		return;
	}
#endif
	/* Nonnative mode16 retains the shared fallback below. */
	case 12:
		path = BG_GetSplineData(splinePath, &backwards); if (!path)
		{
			return;
		}
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
		if ((int)tr->trType == 12)
		{
			int            duration = tr->trDuration;
			long double    fraction = 0.0L;
			volatile float fractionSpill;
			if (duration != 0)
			{
#if defined(_WIN32)
				fraction = (long double)elapsed(atTime, tr->trTime) / (long double)duration;
#else
				fraction  = 1.0L / (long double)duration;
				fraction *= (long double)elapsed(atTime, tr->trTime);
#endif
			}
			fractionSpill = (float)fraction;
#if defined(_WIN32)
			fraction = (long double)fractionSpill;
#endif
			if (!(fraction >= 0.0L))
			{
				fraction = 0.0L;
			}
			else
			{
#if defined(_WIN32)
				if (fraction > 1.0L)
				{
					fraction = 1.0L;
				}
#else
				if (!(1.0L >= fraction))
				{
					fraction = 1.0L;
				}
#endif
			}
			if (backwards)
			{
				fraction = 1.0L - fraction;
			}
			t = (float)fraction;
		}
		else
#endif
		{
			t = tr->trDuration?(float)((double)elapsed(atTime, tr->trTime) / tr->trDuration):0;
			if (t < 0)
			{
				t = 0;
			}
			else if (t > 1)
			{
				t = 1;
			}
			if (backwards)
			{
				t = 1 - t;
			}
		}
		if (tr->trType == 16)
		{
			index = (int)floor((double)t * 16.0f);
			if (index >= 16)
			{
				index = 15; frac = path->segments[index].length;
			}
			else
			{
				frac = (float)(((double)t * 16 - index) * path->segments[index].length);
			}
			if (isAngle && !tr->trBase[0])
			{
				vectoangles(path->segments[index].v_norm, result); return;
			}
			for (i = 0; i < 3; ++i)
				result[i] = (float)((double)frac * path->segments[index].v_norm[i] + path->segments[index].start[i]);
			if (!isAngle)
			{
				return;
			}
			VectorCopy(result, v);
			BG_LinearPathOrigin2(tr->trBase[0], &path, &t, v, backwards);
			if (tr->trBase[0] < 0)
			{
				VectorSubtract(v, result, result);
			}
			else
			{
				VectorSubtract(result, v, result);
			}
			vectoangles(result, result); return;
		}
		t2 = t; BG_CalculateSpline_r(path, a, b, t);
		if (!isAngle)
		{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
			TCE_SplinePosition(a, b, t, result);
#else
			interpolate(a, b, t, result);
#endif
			return;
		}
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
		{
			float       offset    = tr->trBase[0];
			qboolean    traversed = qfalse;
			long double position[3], timeOffset;
#if defined(_WIN32)
			volatile float differenceY, differenceZ, positionY, positionZ;
			long double    differenceX;
#else
			long double     reciprocal;
			volatile vec3_t positionSpill;
#endif
			/* The original equality flag skips both zero and unordered. */
			if (offset != 0.0f && offset == offset)
			{
				other = path;
#if defined(_WIN32)
				timeOffset = (long double)tr->trBase[0] / (long double)path->length;
#else
				reciprocal = 1.0L / (long double)path->length;
				timeOffset = (long double)offset * reciprocal;
#endif
				t2        = (float)(timeOffset + (long double)t2);
				traversed = BG_TraverseSpline(&t2, &other);
			}
			if (traversed)
			{
				TCE_SplinePosition(a, b, t, result);
				BG_CalculateSpline_r(other, a, b, t2);
#if defined(_WIN32)
				differenceX = (long double)b[0] - (long double)a[0];
				differenceY = (float)((long double)b[1] - (long double)a[1]);
				differenceZ = (float)((long double)b[2] - (long double)a[2]);
				position[0] = differenceX * (long double)t2 + (long double)a[0];
				positionY   = (float)((long double)differenceY * (long double)t2 + (long double)a[1]);
				positionZ   = (float)((long double)differenceZ * (long double)t2 + (long double)a[2]);
				position[1] = (long double)positionY;
				position[2] = (long double)positionZ;
#else
				TCE_SplineHeldPosition(a, b, t2, position);
				positionSpill[0] = (float)position[0];
				positionSpill[1] = (float)position[1];
				positionSpill[2] = (float)position[2];
#endif
				if (!(tr->trBase[0] >= 0.0f))
				{
					for (i = 0; i < 3; ++i)
						result[i] = (float)((long double)result[i] - position[i]);
				}
				else
				{
#if !defined(_WIN32)
					/* This branch reloads Float spills instead of held sums. */
					position[0] = (long double)positionSpill[0];
					position[1] = (long double)positionSpill[1];
					position[2] = (long double)positionSpill[2];
#endif
					for (i = 0; i < 3; ++i)
						result[i] = (float)(position[i] - (long double)result[i]);
				}
			}
			else
			{
				for (i = 0; i < 3; ++i)
					result[i] = (float)((long double)b[i] - (long double)a[i]);
			}
		}
#else
		if (tr->trBase[0])
		{
			other = path; t2 = (float)((double)tr->trBase[0] / path->length + t2);
			if (BG_TraverseSpline(&t2, &other))
			{
				interpolate(a, b, t, result); BG_CalculateSpline_r(other, a, b, t2); interpolate(a, b, t2, v);
				if (tr->trBase[0] < 0)
				{
					VectorSubtract(result, v, result);
				}
				else
				{
					VectorSubtract(v, result, result);
				}
			}
			else
			{
				VectorSubtract(b, a, result);
			}
		}
		else
		{
			VectorSubtract(b, a, result);
		}
#endif
		vectoangles(result, result);
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
		{
			long double rollValue = (long double)tr->trBase[1];
			long double phase, wave, halfPi;
			qboolean    easeIn = qfalse, easeOut = qfalse;
#if defined(_WIN32)
			halfPi = (long double)1.5707963705062866f;
#else
			halfPi = (long double)1.5707963267948966;
#endif
			if (rollValue >= 10000.0L || !(rollValue >= -10000.0L))
			{
				easeIn = qtrue;
				if (!(rollValue >= 0.0L))
				{
					rollValue += 10000.0L;
				}
				else
				{
					rollValue -= 10000.0L;
				}
			}
			if (rollValue >= 1000.0L || !(rollValue >= -1000.0L))
			{
				easeOut = qtrue;
				if (!(rollValue >= 0.0L))
				{
					rollValue += 1000.0L;
				}
				else
				{
					rollValue -= 1000.0L;
				}
			}
			if (easeIn || easeOut)
			{
				if (easeIn && easeOut)
				{
					phase  = (long double)t + (long double)t;
					phase -= 1.0L;
				}
				else if (easeIn)
				{
					phase = (long double)t;
				}
				else
				{
					phase = 1.0L - (long double)t;
				}
#if defined(_WIN32)
				phase = phase * halfPi;
#else
				phase = halfPi * phase;
#endif
				/* Hardware FSIN equivalence remains unverified. */
				wave = sinl(phase);
				if (easeIn && easeOut)
				{
					wave += 1.0L;
				}
				else if (!easeIn)
				{
					wave = 1.0L - wave;
				}
				wave *= (long double)tr->trBase[2];
				if (easeIn && easeOut)
				{
					wave *= 0.5L;
				}
			}
			else
			{
#if defined(_WIN32)
				wave = (long double)t * (long double)tr->trBase[2];
#else
				wave = (long double)tr->trBase[2] * (long double)t;
#endif
			}
#if defined(_WIN32)
			result[2] = (float)(wave + rollValue);
#else
			result[2] = (float)(rollValue + wave);
#endif
			return;
		}
#else
		roll = tr->trBase[1];
		if (roll >= 10000 || roll < -10000)
		{
			dampIn = qtrue; roll += roll < 0?10000:-10000;
		}
		if (roll >= 1000 || roll < -1000)
		{
			dampOut = qtrue; roll += roll < 0?1000:-1000;
		}
		if (dampIn && dampOut)
		{
			scale = (sin(((double)t * 2 - 1) * 1.5707963705062866f) + 1) * tr->trBase[2] * .5;
		}
		else if (dampIn)
		{
			scale = sin((double)t * 1.5707963705062866f) * tr->trBase[2];
		}
		else if (dampOut)
		{
			scale = (1 - sin((1.0 - t) * 1.5707963705062866f)) * tr->trBase[2];
		}
		else
		{
			scale = (double)t * tr->trBase[2];
		}
		result[2] = (float)(roll + scale); return;
#endif
	default: Com_Error(ERR_DROP, "BG_EvaluateTrajectory: unknown trType: %i", tr->trTime); return;
	}
	for (i = 0; i < 3; ++i)
		result[i] = (float)(time * tr->trDelta[i] + tr->trBase[i]);
}
