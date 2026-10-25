#include "../game/q_shared.h"
#include "../game/tce_bg.h"
#include "tce_weapon_position.h"
#include "tce_weapon_position_precision.h"
#if defined(__linux__) && !defined(Q3_VM)
#include "../game/tce_numeric.h"
#endif

#if defined(_MSC_VER) && defined(_M_IX86)
/* Initial view state, before posture and weapon-motion adjustments. */
static void TCE_WeaponPositionInitialView(const tce_weaponPosition_t *s,
                                          float *origin, float *angles)
{
	memcpy(origin, s->viewOrigin, sizeof(vec3_t));
	memcpy(angles, s->viewAngles, sizeof(vec3_t));
	if (s->eFlags & 0x8000)
	{
#if !defined(Q3_VM)
		if (!TCE_WeaponPositionTankPitch(s->viewAngles[0], &angles[0]))
#endif
		{
			angles[0] = (float)((long double)s->viewAngles[0] *
			                    (long double)0.8333333333333334);
		}
	}
	if (!s->thirdPerson && (s->weapon == 60 || s->weapon == 62) &&
	    s->weaponState != 1)
	{
		memcpy(&angles[0], &s->mountedPitch, sizeof(float));
	}
}

static void TCE_WeaponPositionProne(tce_weaponPosition_t *s, float *origin,
                                    float *angles)
{
	unsigned int elapsed;
	int          delta, i;
	long double  fraction = 1.0L, amount;

	s->postureModified = 0;
	elapsed            = (s->eFlags & 0x100000)
	    ? (unsigned int)s->time - (unsigned int)s->proneMovingTime
	    : (unsigned int)s->proneMovingTime + (unsigned int)s->time;
	memcpy(&delta, &elapsed, sizeof(delta));
#if !defined(Q3_VM)
	if (TCE_WeaponPositionPosture(delta, s->eFlags & 0x100000, 0,
	                            s->viewAxis, origin, angles))
	{
		s->postureModified = 1;
		return;
	}
#endif
	if (s->eFlags & 0x100000)
	{
		if (delta <= 0)
		{
			s->postureModified = 1;
			return;
		}
		if (delta <= 200)
		{
			fraction = 1.0L / (200.0L / delta);
		}
	}
	else
	{
		if (delta >= 200)
		{
			return;
		}
		if (delta != 0)
		{
			fraction = 1.0L - 1.0L / (200.0L / delta);
		}
	}
	amount = fraction * -25.0L;
	for (i = 0; i < 3; ++i)
		origin[i] = (float)(amount * s->viewAxis[0][i] + origin[i]);
	amount = fraction * 5.0L;
	for (i = 0; i < 3; ++i)
		origin[i] = (float)(amount * s->viewAxis[1][i] + origin[i]);
	amount = fraction * -5.0L;
	for (i = 0; i < 3; ++i)
		origin[i] = (float)(amount * s->viewAxis[2][i] + origin[i]);
	angles[1]          = (float)(fraction * 60.0L + angles[1]);
	s->postureModified = 1;
}

static void TCE_WeaponPositionStance(tce_weaponPosition_t *s, float *origin,
                                     float *angles, int active, int timer, int duck)
{
	unsigned int elapsed = active ? (unsigned int)s->time - (unsigned int)timer
	                              : (unsigned int)timer + (unsigned int)s->time;
	int         delta, i;
	long double fraction = 1.0L, amount;

	memcpy(&delta, &elapsed, sizeof(delta));
#if !defined(Q3_VM)
	if (TCE_WeaponPositionPosture(delta, active, duck ? 2 : 1,
	                            s->viewAxis, origin, angles))
	{
		s->postureModified = 1;
		return;
	}
#endif
	if (active)
	{
		if (delta <= 0)
		{
			s->postureModified = 1;
			return;
		}
		if (delta <= 200)
		{
			fraction = 1.0L / (200.0L / delta);
		}
	}
	else
	{
		if (delta >= 200)
		{
			return;
		}
		if (delta != 0)
		{
			fraction = 1.0L - 1.0L / (200.0L / delta);
		}
	}
	if (duck)
	{
		amount = fraction * -5.0L;
		for (i = 0; i < 3; ++i)
			origin[i] = (float)(amount * s->viewAxis[0][i] + origin[i]);
	}
	amount = fraction * (duck ? -20.0L : -5.0L);
	for (i = 0; i < 3; ++i)
		origin[i] = (float)(amount * s->viewAxis[2][i] + origin[i]);
	angles[1]          = (float)(fraction * 10.0L + angles[1]);
	angles[0]          = (float)(fraction * 30.0L + angles[0]);
	s->postureModified = 1;
}

static void TCE_WeaponPositionLean(const tce_weaponPosition_t *s, float *origin,
                                   float *angles)
{
	unsigned int elapsed, magnitudeBits;
	int          delta, magnitude, i;
	float        fraction = 1.0f;
	vec3_t       right, up;
	long double  amount;

	if (!(s->lean < 0.0f || s->lean > 0.0f))
	{
		return;
	}
	elapsed = (unsigned int)s->time - (unsigned int)s->leanTime;
	memcpy(&delta, &elapsed, sizeof(delta));
	if (s->aiming)
	{
		if (delta >= 200)
		{
			return;
		}
#if !defined(Q3_VM)
		if (!TCE_WeaponPositionLeanFraction(delta, 1, &fraction))
#endif
		{
			fraction = (float)(1.0L - (long double)delta * (long double)0.005f);
		}
	}
	else if (delta < 200)
	{
#if !defined(Q3_VM)
		if (!TCE_WeaponPositionLeanFraction(delta, 0, &fraction))
#endif
		{
			fraction = (float)((long double)delta * (long double)0.005f);
		}
	}
	if (!(fraction > 0.0f))
	{
		return;
	}
#if !defined(Q3_VM)
	if (!TCE_WeaponPositionLeanScaledOffset(s->lean, fraction, 1.0f,
	                                      angles[2], 0, &angles[2]))
#endif
	{
		amount    = (long double)s->lean * 0.5L * fraction;
		angles[2] = (float)((long double)angles[2] - amount);
	}
	AngleVectors(angles, NULL, right, up);
	for (i = 0; i < 3; ++i)
	{
#if !defined(Q3_VM)
		if (TCE_WeaponPositionLeanOffset(fraction, angles[2], right[i],
		                               origin[i], &origin[i]))
		{
			continue;
		}
#endif
		origin[i] = (float)(((long double)fraction * angles[2]) * right[i] + origin[i]);
	}

	magnitudeBits = 0;
	if ((double)s->lean >= -9223372036854775808.0 &&
	    (double)s->lean < 9223372036854775808.0)
	{
		magnitudeBits = (unsigned int)(long long)s->lean;
	}
	memcpy(&magnitude, &magnitudeBits, sizeof(magnitude));
	if (magnitude < 0)
	{
		magnitudeBits = 0u - magnitudeBits;
	}
	memcpy(&magnitude, &magnitudeBits, sizeof(magnitude));
#if !defined(Q3_VM)
	if (!TCE_WeaponPositionLeanPitch(magnitude, fraction, angles[0], &angles[0]))
#endif
	{
		amount    = (long double)magnitude * 0.5L * fraction;
		angles[0] = (float)(amount + angles[0]);
	}
	AngleVectors(s->viewAngles, NULL, right, NULL);
	for (i = 0; i < 3; ++i)
	{
#if !defined(Q3_VM)
		if (TCE_WeaponPositionLeanScaledOffset(s->lean, fraction, right[i],
		                                      origin[i], 1, &origin[i]))
		{
			continue;
		}
#endif
		amount    = -((long double)s->lean * 0.25L * fraction);
		origin[i] = (float)(amount * right[i] + origin[i]);
	}
}

static void TCE_WeaponPositionRecoil(const tce_weaponPosition_t *s, float *origin,
                                     float *angles)
{
	const tce_weaponDef_t *w      = &weaponDef[s->weapon];
	int                   scoped  = w->scoped > 1.0f && s->scopeEnabled && s->aiming;
	unsigned int          elapsed = (unsigned int)s->time - (unsigned int)s->shotTime;
	int                   delta, pitch = 0, i;
	long double           rate, wave, amount;

	memcpy(&delta, &elapsed, sizeof(delta));
	if (w->usesRecoilAnimMod && !w->pump && !scoped && s->time > s->shotTime)
	{
		if (delta >= 300)
		{
			return;
		}
		rate  = (long double)(1.0f / 300.0f);
		pitch = 1;
	}
	else if (w->usesRecoilAnimMod && w->pump && w->semiauto &&
	         s->count_3407dff8 > 0 && s->time > s->shotTime)
	{
		if (delta >= 500)
		{
			return;
		}
		rate  = (long double)0.002f;
		pitch = 1;
	}
	else if (w->usesPistolAnimMod && s->time > s->shotTime)
	{
		if (delta >= 50)
		{
			return;
		}
		rate = (long double)0.02f;
	}
	else
	{
		return;
	}
	wave = sinl(sqrtl(sqrtl((long double)delta * rate)) *
	            (long double)3.1415927410125732);
	amount = wave * -4.0L;
	for (i = 0; i < 3; ++i)
		origin[i] = (float)(amount * s->viewAxis[0][i] + origin[i]);
	if (pitch)
	{
		angles[0] = (float)((long double)angles[0] - wave * 4.0L);
	}
}

static void TCE_WeaponPositionFiremode(const tce_weaponPosition_t *s, float *origin,
                                       float *angles)
{
	unsigned int elapsed;
	int          delta, i;
	long double  wave, amount;

	if (!BG_FiremodeWeapon(s->weapon))
	{
		return;
	}
	elapsed = (unsigned int)s->time - (unsigned int)s->firemodeTime;
	memcpy(&delta, &elapsed, sizeof(delta));
	if (delta >= 750)
	{
		return;
	}
	if (weaponDef[s->weapon].scoped > 1.0f && s->aiming && s->scopeEnabled)
	{
		return;
	}
	wave = (1.0L - cosl((long double)delta * (long double)(1.0f / 750.0f) *
	                    (long double)6.2831854820251465f)) * 0.5L;
	amount = wave * -2.0L;
	for (i = 0; i < 3; ++i)
		origin[i] = (float)(amount * s->viewAxis[2][i] + origin[i]);
	amount    = wave * 4.0L;
	angles[0] = (float)((long double)angles[0] - amount);
	if (s->weapon == 43 || s->weapon == 33 || s->weapon == 45)
	{
		angles[2] = (float)((long double)angles[2] - amount);
		angles[1] = (float)(amount + angles[1]);
	}
	else
	{
		angles[2] = (float)(amount + angles[2]);
		angles[1] = (float)((long double)angles[1] - amount);
	}
}

static void TCE_WeaponPositionFinalAngles(const tce_weaponPosition_t *s, float *angles)
{
	int         i;
	float       idleScale = 1.0f;
	long double amount;

	for (i = 0; i < 3; ++i)
		angles[i] = (float)((long double)s->gunViewAngles[i] + angles[i]);
	if (s->developer)
	{
		for (i = 0; i < 3; ++i)
			angles[i] = (float)((long double)s->developerAngles[i] + angles[i]);
	}
	if (!weaponDef[s->weapon].noTacMode &&
	    (s->tacticalScale < 0.0f || s->tacticalScale > 0.0f))
	{
		for (i = 0; i < 2; ++i)
		{
#if !defined(Q3_VM)
			if (TCE_WeaponPositionTacticalAngle(i ? s->tacticalYaw : s->tacticalPitch,
			                                   s->tacticalScale, angles[i], &angles[i]))
			{
				continue;
			}
#endif
			amount    = (long double)(i ? s->tacticalYaw : s->tacticalPitch) - 2000.0L;
			amount   *= (long double)s->tacticalScale;
			amount   *= (long double)0.01f;
			angles[i] = (float)(amount + angles[i]);
		}
	}
	if ((s->eFlags & 0x8000) || s->weapon == 60 || s->weapon == 62)
	{
		return;
	}
	if (s->aiming && s->weapon != 30 && s->weapon != 9 && s->weapon != 4)
	{
		idleScale = 0.0f;
	}
	amount  = sinl((long double)s->time * (long double)0.001);
	amount *= 80.0L;
	amount *= (long double)idleScale;
	amount *= (long double)0.01;
	for (i = 2; i >= 0; --i)
		angles[i] = (float)((long double)angles[i] + amount);
}

static void TCE_WeaponPositionLanding(const tce_weaponPosition_t *s, float *origin)
{
	unsigned int elapsed;
	int          delta;
	long double  amount;

	if (s->aiming && s->weapon != 30 && s->weapon != 9 && s->weapon != 4 &&
	    weaponDef[s->weapon].scoped > 1.0f && s->scopeEnabled)
	{
		return;
	}
	elapsed = (unsigned int)s->time - (unsigned int)s->landTime;
	memcpy(&delta, &elapsed, sizeof(delta));
#if !defined(Q3_VM)
	if (TCE_WeaponPositionLandingFinite(delta, s->landChange, origin[2], &origin[2]))
	{
		return;
	}
#endif
	if (delta < 150)
	{
		amount = (long double)s->landChange * delta * (long double)(1.0 / 600.0);
	}
	else if (delta < 450)
	{
		amount = (long double)(450 - delta) * s->landChange * (long double)(1.0 / 1200.0);
	}
	else
	{
		return;
	}
	origin[2] = (float)(amount + origin[2]);
}

static void TCE_WeaponPositionBob(const tce_weaponPosition_t *s, float *angles)
{
	float       scale       = 1.0f;
	long double signedSpeed = (long double)s->speed;
	long double amount;

	if (s->bobCycle & 1)
	{
		signedSpeed = -signedSpeed;
	}
	if (s->aiming && s->weapon != 30 && s->weapon != 9 && s->weapon != 4)
	{
		scale = 0.15f;
		if (weaponDef[s->weapon].scoped > 1.0f && s->scopeEnabled)
		{
			scale = 0.0f;
		}
	}
#if !defined(Q3_VM)
	if (TCE_WeaponPositionBobFinite(s->bobSin, s->speed, scale,
	                              s->bobCycle & 1, angles))
	{
		return;
	}
#endif
	amount    = (long double)s->bobSin * signedSpeed * scale * (long double)0.005;
	angles[2] = (float)(amount + angles[2]);
	amount    = (long double)s->bobSin * signedSpeed * scale * (long double)0.01;
	angles[1] = (float)(amount + angles[1]);
	amount    = (long double)s->bobSin * s->speed * scale * (long double)0.005;
	angles[0] = (float)(amount + angles[0]);
}

static void TCE_WeaponPositionSway(tce_weaponPosition_t *s, float *origin, float *angles)
{
	const tce_weaponDef_t *w = &weaponDef[s->weapon];
	unsigned int          elapsed;
	int                   delta, stepDelta, proneDelta, i;
	float                 target[2], strength;
	long double           dt, difference[3], rightDelta, upDelta, rightVelocity, forwardVelocity;
	long double           limit, shotScale, vertical, amount;

	if (w->scoped > 1.0f && s->aiming && s->scopeEnabled)
	{
		return;
	}
	elapsed = (unsigned int)s->swayTime - (unsigned int)s->time;
	memcpy(&delta, &elapsed, sizeof(delta));
#if !defined(Q3_VM)
	if (TCE_WeaponPositionSwayTargets(delta, s->viewAxis, s->priorForward,
	                                s->velocity, target))
	{
		s->swayTime = s->time;
		memcpy(s->priorForward, s->viewAxis[0], sizeof(vec3_t));
		goto sway_step;
	}
#endif
	dt = (long double)delta * (long double)0.001f;
	if (!(dt >= (long double)0.005f))
	{
		dt = (long double)0.005f;
	}
	s->swayTime = s->time;
	for (i = 0; i < 3; ++i)
		difference[i] = (long double)s->viewAxis[0][i] - s->priorForward[i];
	memcpy(s->priorForward, s->viewAxis[0], sizeof(vec3_t));

	rightDelta = difference[0] * s->viewAxis[1][0]
	             + difference[2] * s->viewAxis[1][2];
	rightDelta += difference[1] * s->viewAxis[1][1];
	target[0]   = (float)-(rightDelta / dt);
	upDelta     = difference[2] * s->viewAxis[2][2]
	              + difference[1] * s->viewAxis[2][1];
	upDelta      += difference[0] * s->viewAxis[2][0];
	upDelta       = -(upDelta / dt);
	rightVelocity = (long double)s->velocity[1] * s->viewAxis[1][1]
	                + (long double)s->velocity[0] * s->viewAxis[1][0];
	rightVelocity  += (long double)s->velocity[2] * s->viewAxis[1][2];
	target[0]       = (float)(rightVelocity * (long double)0.1f + target[0]);
	forwardVelocity = (long double)s->velocity[2] * s->viewAxis[0][2]
	                  + (long double)s->velocity[0] * s->viewAxis[0][0];
	forwardVelocity += (long double)s->velocity[1] * s->viewAxis[0][1];
	target[1]        = (float)(forwardVelocity * (long double)0.1f + upDelta);

#if !defined(Q3_VM)
sway_step:
#endif
	elapsed = (unsigned int)s->time - (unsigned int)s->stepTime;
	memcpy(&stepDelta, &elapsed, sizeof(stepDelta));
	elapsed = (unsigned int)s->proneTime + (unsigned int)s->time;
	memcpy(&proneDelta, &elapsed, sizeof(proneDelta));
	if (stepDelta < (((s->eFlags & 0x80000) || proneDelta < 750) ? 750 : 250))
	{
		target[1] = (float)((long double)s->stepChange + target[1]);
	}

#if !defined(Q3_VM)
	if (TCE_WeaponPositionSwayState(target, w->unknown_0f8[3], s->aiming,
	                              s->scale_3407dfbc, s->scale_3407dfc0,
	                              s->swayHorizontal, s->swayVertical, &strength,
	                              &s->swayHorizontal, &s->swayVertical))
	{
		goto sway_offsets;
	}
#endif
	limit = s->aiming ? (long double)w->unknown_0f8[3] *
	        (long double)(1.0f / 600.0f) : 3.0L;
	shotScale = s->aiming ? 0.5L : 1.0L;
	strength  = (float)((long double)s->scale_3407dfbc * (long double)0.001f +
	                    ((long double)s->scale_3407dfc0 * shotScale) * (long double)0.001f);
	if (strength > 1.0f)
	{
		strength = 1.0f;
	}
	if (!s->aiming && !(strength >= 0.5f))
	{
		strength = 0.5f;
	}
	for (i = 0; i < 2; ++i)
	{
		if (!(fabsl((long double)target[i]) >= (long double)0.2))
		{
			target[i] = 0.0f;
		}
		else if (limit < (long double)target[i])
		{
			target[i] = (float)limit;
		}
		else if (-limit > (long double)target[i])
		{
			target[i] = (float)-limit;
		}
	}
	s->swayHorizontal = (float)(((long double)target[0] * (long double)0.1f +
	                             s->swayHorizontal) * (long double)0.9090908765792847f);
	vertical = ((long double)target[1] * (long double)0.1f + s->swayVertical) *
	           (long double)0.9090908765792847f;
	s->swayVertical = (float)vertical;
	if (vertical > limit)
	{
		s->swayVertical = (float)limit;
	}
	else if (!((long double)s->swayVertical >= -limit))
	{
		s->swayVertical = (float)-limit;
	}
	if ((long double)s->swayHorizontal > limit)
	{
		s->swayHorizontal = (float)limit;
	}
	else if (!((long double)s->swayHorizontal >= -limit))
	{
		s->swayHorizontal = (float)-limit;
	}
#if !defined(Q3_VM)
sway_offsets:
	if (TCE_WeaponPositionSwayOffsets(s->swayHorizontal, s->swayVertical,
	                                strength, s->viewAxis, origin, angles))
	{
		return;
	}
#endif
	for (i = 0; i < 3; ++i)
	{
		amount    = (long double)s->swayHorizontal * strength * 0.25L;
		origin[i] = (float)(amount * s->viewAxis[1][i] + origin[i]);
	}
	for (i = 0; i < 3; ++i)
	{
		amount    = (long double)s->swayVertical * strength * 0.25L;
		origin[i] = (float)(amount * s->viewAxis[2][i] + origin[i]);
	}
	angles[1] = (float)((long double)angles[1] - (long double)s->swayHorizontal * strength);
	angles[0] = (float)((long double)s->swayVertical * strength + angles[0]);
}

#endif

/* Portable fallback; its evaluation schedule is not a Windows x87 claim. */
static void translate(float *v, const float *axis, double scale)
{
	int i; for (i = 0; i < 3; ++i)
		v[i] = (float)(v[i] + scale * axis[i]);
}
static double dot(const float *a, const float *b)
{
	return (double)a[0] * b[0] + (double)a[1] * b[1] + (double)a[2] * b[2];
}
static float clampSway(float value, float limit)
{
	if (value > limit)
	{
		return limit;
	}
	return value < -limit ? -limit : value;
}
#if defined(__linux__) && !defined(Q3_VM)
static int nativePositionTime(unsigned int bits)
{
	int value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}

static int nativeLeanMagnitude(float lean)
{
	int          value = TCE_TruncateNativeInteger((long double)lean);
	unsigned int bits  = (unsigned int)value;
	if (value < 0)
	{
		bits = 0u - bits;
	}
	memcpy(&value, &bits, sizeof(value));
	return value;
}
static void nativeRecoilPosition(tce_weaponPosition_t *s, float origin[3],
                                 float angles[3], int scoped)
{
	const tce_weaponDef_t *w    = &weaponDef[s->weapon];
	int                   delta = nativePositionTime((unsigned int)s->time - (unsigned int)s->shotTime);
	int                   pitch = 0, i;
	long double           rate = 0.0L;
	float                 recoil;

	if (w->usesRecoilAnimMod && !w->pump && !scoped && s->time > s->shotTime)
	{
		if (delta <= 299)
		{
			rate  = (long double)(1.0f / 300.0f);
			pitch = 1;
		}
	}
	else if (w->usesRecoilAnimMod && w->pump && w->semiauto &&
	         s->count_3407dff8 > 0 && s->time > s->shotTime)
	{
		if (delta <= 499)
		{
			rate  = (long double)0.002f;
			pitch = 1;
		}
	}
	else if (w->usesPistolAnimMod && s->time > s->shotTime && delta <= 49)
	{
		rate = (long double)0.02f;
	}
	if (rate == 0.0L)
	{
		return;
	}

	recoil = (float)sinl(sqrtl(sqrtl((long double)delta * rate)) *
	                     (long double)3.141592653589793);
	for (i = 0; i < 3; ++i)
	{
		origin[i] = (float)((long double)s->viewAxis[0][i] *
		                    (long double)recoil * -4.0L + (long double)origin[i]);
	}
	if (pitch)
	{
		angles[0] = (float)((long double)recoil * -4.0L + (long double)angles[0]);
	}
}

static void nativeSwayTargets(tce_weaponPosition_t *s, float target[2],
                              long double *verticalTarget)
{
	long double dt, inverse, difference[3], rightDelta, upDelta;
	long double rightVelocity, forwardVelocity;
	int         i;

	dt = (long double)nativePositionTime((unsigned int)s->swayTime -
	                                     (unsigned int)s->time) *
	     (long double)0.0010000000474974513f;
	if (!(dt >= (long double)0.004999999888241291f))
	{
		dt = (long double)0.004999999888241291f;
	}
	s->swayTime = s->time;
	for (i = 0; i < 3; ++i)
	{
		difference[i] = (long double)s->viewAxis[0][i] - (long double)s->priorForward[i];
	}
	VectorCopy(s->viewAxis[0], s->priorForward);
	inverse    = 1.0L / dt;
	rightDelta = difference[0] * (long double)s->viewAxis[1][0] +
	             difference[1] * (long double)s->viewAxis[1][1];
	rightDelta += difference[2] * (long double)s->viewAxis[1][2];
	upDelta     = difference[0] * (long double)s->viewAxis[2][0] +
	              difference[1] * (long double)s->viewAxis[2][1];
	upDelta      += difference[2] * (long double)s->viewAxis[2][2];
	rightVelocity = (long double)s->viewAxis[1][0] * (long double)s->velocity[0] +
	                (long double)s->viewAxis[1][1] * (long double)s->velocity[1];
	rightVelocity  += (long double)s->viewAxis[1][2] * (long double)s->velocity[2];
	forwardVelocity = (long double)s->viewAxis[0][0] * (long double)s->velocity[0] +
	                  (long double)s->viewAxis[0][1] * (long double)s->velocity[1];
	forwardVelocity += (long double)s->viewAxis[0][2] * (long double)s->velocity[2];
	target[0]        = (float)(-rightDelta * inverse + rightVelocity * (long double)0.1f);
	*verticalTarget  = -upDelta * inverse + forwardVelocity * (long double)0.1f;
	target[1]        = (float)*verticalTarget;
}

#endif

static void posture(tce_weaponPosition_t *s, float *origin, float *angles, int active, int timer, int kind)
{
#if defined(__linux__) && !defined(Q3_VM)
	unsigned int timeBits = (unsigned int)s->time;
	int          delta    = nativePositionTime(active ? timeBits - (unsigned int)timer :
	                                           timeBits + (unsigned int)timer);
	int         i;
	long double fraction = 1.0L, held[3];

	if (active)
	{
		if (delta <= 0)
		{
			s->postureModified = 1;
			return;
		}
		if (delta <= 200)
		{
			fraction = (long double)delta * (1.0L / 200.0L);
		}
	}
	else
	{
		if (delta >= 200)
		{
			return;
		}
		if (delta)
		{
			fraction = 1.0L - (long double)delta * (1.0L / 200.0L);
		}
	}
	for (i = 0; i < 3; ++i)
		held[i] = (long double)origin[i];
	if (kind == 0)
	{
		for (i = 0; i < 3; ++i)
		{
			held[i]  += (long double)s->viewAxis[0][i] * fraction * -25.0L;
			origin[i] = (float)held[i];
		}
		for (i = 0; i < 3; ++i)
		{
			held[i]  += (long double)s->viewAxis[1][i] * fraction * 5.0L;
			origin[i] = (float)held[i];
		}
		for (i = 0; i < 3; ++i)
		{
			held[i]  += (long double)s->viewAxis[2][i] * fraction * -5.0L;
			origin[i] = (float)held[i];
		}
		angles[1] = (float)(fraction * 60.0L + (long double)angles[1]);
	}
	else
	{
		if (kind == 2)
		{
			for (i = 0; i < 3; ++i)
			{
				held[i]  += (long double)s->viewAxis[0][i] * fraction * -5.0L;
				origin[i] = (float)held[i];
			}
		}
		for (i = 0; i < 3; ++i)
		{
			held[i] += (long double)s->viewAxis[2][i] * fraction *
			           (kind == 1 ? -5.0L : -20.0L);
			origin[i] = (float)held[i];
		}
		angles[1] = (float)(fraction * 10.0L + (long double)angles[1]);
		angles[0] = (float)(fraction * 30.0L + (long double)angles[0]);
	}
	s->postureModified = 1;
#else
	int   delta = active?s->time - timer:s->time + timer;
	float f;
	if (active)
	{
		s->postureModified = 1;
		if (delta <= 0)
		{
			return;
		}
		f = delta > 200?1.0f:(float)(1.0 / (200.0 / delta));
	}
	else
	{
		if (delta >= 200)
		{
			return;
		}
		s->postureModified = 1;
		f                  = delta == 0?1.0f:(float)(1.0 - 1.0 / (200.0 / delta));
	}
	if (kind == 0)
	{
		translate(origin, s->viewAxis[0], (float)(f * -25.0));
		translate(origin, s->viewAxis[1], (float)(f * 5.0));
		translate(origin, s->viewAxis[2], (float)(f * -5.0));
		angles[1] = (float)(angles[1] + f * 60.0);
	}
	else
	{
		if (kind == 2)
		{
			translate(origin, s->viewAxis[0], (float)(f * -5.0));
		}
		translate(origin, s->viewAxis[2], (float)(f * (kind == 1?-5.0:-20.0)));
		angles[1] = (float)(angles[1] + f * 10.0);
		angles[0] = (float)(angles[0] + f * 30.0);
	}
#endif
}
void TCE_CG_CalculateWeaponPosition(tce_weaponPosition_t *s, float *origin, float *angles)
{
#if defined(_MSC_VER) && defined(_M_IX86)
	TCE_WeaponPositionInitialView(s, origin, angles);
	TCE_WeaponPositionProne(s, origin, angles);
	TCE_WeaponPositionStance(s, origin, angles, s->flags_3407dfb0 & 0x4000,
	                         s->stanceTime, 0);
	TCE_WeaponPositionStance(s, origin, angles, s->pmFlags & 4, s->duckTime, 1);
	TCE_WeaponPositionLean(s, origin, angles);
	TCE_WeaponPositionRecoil(s, origin, angles);
	TCE_WeaponPositionFiremode(s, origin, angles);
	TCE_WeaponPositionSway(s, origin, angles);
	TCE_WeaponPositionBob(s, angles);
	TCE_WeaponPositionLanding(s, origin);
	TCE_WeaponPositionFinalAngles(s, angles);
	VectorMA(angles, -1.0, s->kickAngles, angles);
#else
	const tce_weaponDef_t *w = &weaponDef[s->weapon];
	int                   i, delta, scoped = s->aiming && s->scopeEnabled && w->scoped > 1;
	float                 f, dt, limit, strength, secondary, bobScale = 1, idleScale = 1, signedSpeed;
	float                 right[3], up[3], difference[3], target[2];
	double                wave, recoilRate = 0; int recoilPitch = 0;
#if defined(__linux__) && !defined(Q3_VM)
	long double nativeLimit, nativeStrength, vertical, verticalTarget;
	long double heldOrigin[3];
#endif
	VectorCopy(s->viewOrigin, origin); VectorCopy(s->viewAngles, angles);
	if (s->eFlags & 0x8000)
	{
		angles[0] = (float)(angles[0] * (5.0 / 6.0));
	}
	if (!s->thirdPerson && (s->weapon == 60 || s->weapon == 62) && s->weaponState != 1)
	{
		angles[0] = s->mountedPitch;
	}
	s->postureModified = 0;
	posture(s, origin, angles, s->eFlags & 0x100000, s->proneMovingTime, 0);
	posture(s, origin, angles, s->flags_3407dfb0 & 0x4000, s->stanceTime, 1);
	posture(s, origin, angles, s->pmFlags & 4, s->duckTime, 2);
#if defined(__linux__) && !defined(Q3_VM)
	delta = nativePositionTime((unsigned int)s->time - (unsigned int)s->leanTime);
#else
	delta = s->time - s->leanTime;
#endif
	f = s->aiming?(delta < 200?(float)(1.0 - delta * (double).005f):0.0f):
	    (delta < 200?(float)(delta * (double).005f):1.0f);
	if (
#if defined(__linux__) && !defined(Q3_VM)
		!TCE_FloatIsZeroOrNaN(s->lean)
#else
		s->lean != 0
#endif
		&& f > 0)
	{
		angles[2] = (float)(angles[2] - s->lean * .5 * f);
		AngleVectors(angles, NULL, right, up);
		translate(origin, right, (double)f * angles[2]);
#if defined(__linux__) && !defined(Q3_VM)
		angles[0] = (float)((long double)angles[0] +
		                    (long double)nativeLeanMagnitude(s->lean) * (long double)f * 0.5L);
#else
		angles[0] = (float)(angles[0] + abs((int)s->lean) * .5 * f);
#endif
		AngleVectors(s->viewAngles, NULL, right, NULL);
		translate(origin, right, -s->lean * .25 * f);
	}
#if defined(__linux__) && !defined(Q3_VM)
	nativeRecoilPosition(s, origin, angles, scoped);
#else
	delta = s->time - s->shotTime;
	if (w->usesRecoilAnimMod && !w->pump && !scoped && delta > 0)
	{
		if (delta < 300)
		{
			recoilRate = (double)(1.0f / 300.0f); recoilPitch = 1;
		}
	}
	else if (w->usesRecoilAnimMod && w->pump && w->semiauto && s->count_3407dff8 > 0 && delta > 0)
	{
		if (delta < 500)
		{
			recoilRate = (double).002f; recoilPitch = 1;
		}
	}
	else if (w->usesPistolAnimMod && delta > 0 && delta < 50)
	{
		recoilRate = (double).02f;
	}
	if (recoilRate)
	{
		wave = sin(sqrt(sqrt(delta * recoilRate)) * 3.1415927410125732);
		translate(origin, s->viewAxis[0], wave * -4.0);
		if (recoilPitch)
		{
			angles[0] = (float)(angles[0] - wave * 4.0);
		}
	}
#endif
#if defined(__linux__) && !defined(Q3_VM)
	if (BG_FiremodeWeapon(s->weapon))
	{
		delta = nativePositionTime((unsigned int)s->time - (unsigned int)s->firemodeTime);
		if (delta <= 749 && !scoped)
		{
			f = (float)((1.0L - cosl((long double)delta *
			                         (long double)(1.0f / 750.0f) *
			                         (long double)6.283185307179586)) * 0.5L);
			for (i = 0; i < 3; ++i)
			{
				origin[i] = (float)((long double)s->viewAxis[2][i] *
				                    (long double)f * -2.0L + (long double)origin[i]);
			}
			vertical  = (long double)f * -4.0L;
			angles[0] = (float)((long double)angles[0] + vertical);
			if (s->weapon == 43 || s->weapon == 33 || s->weapon == 45)
			{
				angles[2] = (float)(vertical + (long double)angles[2]);
				angles[1] = (float)((long double)f * 4.0L + (long double)angles[1]);
			}
			else
			{
				angles[1] = (float)(vertical + (long double)angles[1]);
				angles[2] = (float)((long double)f * 4.0L + (long double)angles[2]);
			}
		}
	}
#else
	delta = s->time - s->firemodeTime;
	if (BG_FiremodeWeapon(s->weapon) && delta < 750 && !scoped)
	{
		wave = (1.0 - cos(delta * (double)(1.0f / 750.0f) * (double)6.2831854820251465f)) * .5;
		translate(origin, s->viewAxis[2], wave * -2.0);
		angles[0] = (float)(angles[0] - wave * 4);
		if (s->weapon == 43 || s->weapon == 33 || s->weapon == 45)
		{
			wave = -wave;
		}
		angles[2] = (float)(angles[2] + wave * 4); angles[1] = (float)(angles[1] - wave * 4);
	}
#endif
	signedSpeed = (s->bobCycle & 1)?-s->speed:s->speed;
	if (s->aiming && s->weapon != 30 && s->weapon != 9 && s->weapon != 4)
	{
		bobScale = scoped?0.0f:.15f; idleScale = 0;
	}
	if (!scoped)
	{
		/* The original subtracts current time from previous time. */
#if defined(__linux__) && !defined(Q3_VM)
		nativeSwayTargets(s, target, &verticalTarget);
#else
		dt = (float)((s->swayTime - s->time) * (double).001f); if (dt < .005f)
		{
			dt = .005f;
		}
		for (i = 0; i < 3; ++i)
		{
			difference[i] = s->viewAxis[0][i] - s->priorForward[i]; s->priorForward[i] = s->viewAxis[0][i];
		}
		target[0] = (float)(dot(s->velocity, s->viewAxis[1]) * (double).1f - dot(difference, s->viewAxis[1]) / dt);
		target[1] = (float)(dot(s->velocity, s->viewAxis[0]) * (double).1f - dot(difference, s->viewAxis[2]) / dt);
#endif
#if defined(__linux__) && !defined(Q3_VM)
		f = (!(s->eFlags & 0x80000) &&
		     nativePositionTime((unsigned int)s->proneTime + (unsigned int)s->time) > 749) ?
		    250.0f : 750.0f;
		if ((float)nativePositionTime((unsigned int)s->time - (unsigned int)s->stepTime) < f)
		{
			verticalTarget += (long double)s->stepChange;
			target[1]       = (float)verticalTarget;
		}
#else
		f = (!(s->eFlags & 0x80000) && s->proneTime + s->time > 749)?250.0f:750.0f;
		if ((float)(s->time - s->stepTime) < f)
		{
			target[1] += s->stepChange;
		}
#endif
#if defined(__linux__) && !defined(Q3_VM)
		nativeLimit = s->aiming ? (long double)w->unknown_0f8[3] *
		              (long double)0.0016666667070239782f : 3.0L;
#else
		limit = s->aiming?(float)(w->unknown_0f8[3] * (double)(1.0f / 600.0f)):3.0f;
#endif
#if defined(__linux__) && !defined(Q3_VM)
		nativeStrength = (long double)s->scale_3407dfc0 *
		                 (s->aiming ? 0.5L : 1.0L) * (long double)0.001f +
		                 (long double)s->scale_3407dfbc * (long double)0.001f;
		if (nativeStrength > 1.0L)
		{
			nativeStrength = 1.0L;
		}
		if (!s->aiming && nativeStrength < 0.5L)
		{
			nativeStrength = 0.5L;
		}
#else
		secondary = s->aiming?.5f:1.0f;
		strength  = (float)(s->scale_3407dfc0 * (double)secondary * (double).001f + s->scale_3407dfbc * (double).001f);
		if (strength > 1)
		{
			strength = 1;
		}
		if (!s->aiming && strength < .5f)
		{
			strength = .5f;
		}
#endif
#if defined(__linux__) && !defined(Q3_VM)
		for (i = 0; i < 2; ++i)
		{
			/* Original JNC keeps only ordered magnitudes at least 0.2. */
			if (!(fabs((double)target[i]) >= 0.2))
			{
				target[i] = 0.0f;
			}
			else if ((long double)target[i] > nativeLimit)
			{
				target[i] = (float)nativeLimit;
			}
			else if (!((long double)target[i] >= -nativeLimit))
			{
				target[i] = (float)-nativeLimit;
			}
		}
#else
		for (i = 0; i < 2; ++i)
			target[i] = fabs(target[i]) < .2?0.0f:clampSway(target[i], limit);
#endif
#if defined(__linux__) && !defined(Q3_VM)
		s->swayHorizontal = (float)(((long double)target[0] * (long double)0.1f +
		                             (long double)s->swayHorizontal) *
		                            (long double)0.9090908765792847f);
		vertical = ((long double)target[1] * (long double)0.1f +
		            (long double)s->swayVertical) * (long double)0.9090908765792847f;
		s->swayVertical = (float)vertical;
		/* Upper vertical comparison precedes the binary32 reload. */
		if (vertical <= nativeLimit)
		{
			if (!((long double)s->swayVertical >= -nativeLimit))
			{
				s->swayVertical = (float)-nativeLimit;
			}
		}
		else
		{
			s->swayVertical = (float)nativeLimit;
		}
		if ((long double)s->swayHorizontal > nativeLimit)
		{
			s->swayHorizontal = (float)nativeLimit;
		}
		else if (!((long double)s->swayHorizontal >= -nativeLimit))
		{
			s->swayHorizontal = (float)-nativeLimit;
		}
#else
		s->swayHorizontal = clampSway((float)((target[0] * (double).1f + s->swayHorizontal) * (double).9090908765792847f), limit);
		s->swayVertical   = clampSway((float)((target[1] * (double).1f + s->swayVertical) * (double).9090908765792847f), limit);
#endif
#if !defined(__linux__) || defined(Q3_VM)
		s->swayTime = s->time;
#endif
#if defined(__linux__) && !defined(Q3_VM)
		for (i = 0; i < 3; ++i)
		{
			heldOrigin[i] = (long double)s->swayHorizontal * nativeStrength *
			                (long double)s->viewAxis[1][i] * 0.25L +
			                (long double)origin[i];
			origin[i] = (float)heldOrigin[i];
		}
		for (i = 0; i < 3; ++i)
		{
			origin[i] = (float)(heldOrigin[i] + (long double)s->swayVertical *
			                    nativeStrength * (long double)s->viewAxis[2][i] * 0.25L);
		}
		angles[1] = (float)((long double)angles[1] -
		                    (long double)s->swayHorizontal * nativeStrength);
		angles[0] = (float)((long double)s->swayVertical * nativeStrength +
		                    (long double)angles[0]);
#else
		translate(origin, s->viewAxis[1], s->swayHorizontal * (double)strength * .25);
		translate(origin, s->viewAxis[2], s->swayVertical * (double)strength * .25);
		angles[1] = (float)(angles[1] - s->swayHorizontal * (double)strength);
		angles[0] = (float)(angles[0] + s->swayVertical * (double)strength);
#endif
	}
#if defined(__linux__) && !defined(Q3_VM)
	{
		volatile long double bobMotion;

		/* ELF retains each product until the individual angle Float store. */
		bobMotion = (long double)signedSpeed * (long double)s->bobSin;
		bobMotion = bobMotion * (long double)bobScale;
		bobMotion = bobMotion * (long double)0.005;
		angles[2] = (float)(bobMotion + (long double)angles[2]);
		bobMotion = (long double)signedSpeed * (long double)s->bobSin;
		bobMotion = bobMotion * (long double)bobScale;
		bobMotion = bobMotion * (long double)0.01;
		angles[1] = (float)(bobMotion + (long double)angles[1]);
		bobMotion = (long double)s->bobSin * (long double)s->speed;
		bobMotion = bobMotion * (long double)bobScale;
		bobMotion = bobMotion * (long double)0.005;
		angles[0] = (float)(bobMotion + (long double)angles[0]);
	}
#else
	angles[2] = (float)(angles[2] + s->bobSin * (double)signedSpeed * bobScale * .005);
	angles[1] = (float)(angles[1] + s->bobSin * (double)signedSpeed * bobScale * .01);
	angles[0] = (float)(angles[0] + s->bobSin * (double)s->speed * bobScale * .005);
#endif
#if defined(__linux__) && !defined(Q3_VM)
	delta = nativePositionTime((unsigned int)s->time - (unsigned int)s->landTime);
	if (bobScale > 0.0f && delta <= 449)
	{
		vertical = delta <= 149 ?
		           (long double)delta * (long double)s->landChange * (long double)(1.0 / 600.0) :
		           (long double)(450 - delta) * (long double)s->landChange * (long double)(1.0 / 1200.0);
		origin[2] = (float)(vertical + (long double)origin[2]);
	}
	for (i = 0; i < 3; ++i)
	{
		heldOrigin[i] = (long double)s->gunViewAngles[i] + (long double)angles[i];
		angles[i]     = (float)heldOrigin[i];
	}
	if (s->developer)
	{
		for (i = 0; i < 3; ++i)
		{
			angles[i] = (float)(heldOrigin[i] + (long double)s->developerAngles[i]);
		}
	}
#else
	delta = s->time - s->landTime;
	if (bobScale > 0 && delta < 450)
	{
		f          = (float)(delta < 150?s->landChange * (double)delta / 600.0:(450 - delta) * (double)s->landChange / 1200.0);
		origin[2] += f;
	}
	for (i = 0; i < 3; ++i)
		angles[i] += s->gunViewAngles[i];
	if (s->developer)
	{
		for (i = 0; i < 3; ++i)
			angles[i] += s->developerAngles[i];
	}
#endif
	if (!w->noTacMode &&
#if defined(__linux__) && !defined(Q3_VM)
	    !TCE_FloatIsZeroOrNaN(s->tacticalScale)
#else
	    s->tacticalScale != 0
#endif
	    )
	{
#if defined(__linux__) && !defined(Q3_VM)
		vertical = ((long double)s->tacticalPitch - 2000.0L) *
		           (long double)s->tacticalScale * (long double)0.01f;
		angles[0] = (float)(vertical + (long double)angles[0]);
		vertical  = ((long double)s->tacticalYaw - 2000.0L) *
		            (long double)s->tacticalScale * (long double)0.01f;
		angles[1] = (float)(vertical + (long double)angles[1]);
#else
		angles[0] = (float)(angles[0] + (s->tacticalPitch - 2000.0) * s->tacticalScale * (double).01f);
		angles[1] = (float)(angles[1] + (s->tacticalYaw - 2000.0) * s->tacticalScale * (double).01f);
#endif
	}
	if (!(s->eFlags & 0x8000) && s->weapon != 60 && s->weapon != 62)
	{
#if defined(__linux__) && !defined(Q3_VM)
		/* The SDK fracsin local is binary32; retain that boundary here. */
		f         = (float)sinl((long double)s->time * (long double)0.001);
		vertical  = (long double)f * 80.0L * (long double)idleScale * (long double)0.01;
		angles[2] = (float)(vertical + (long double)angles[2]);
		angles[1] = (float)(vertical + (long double)angles[1]);
		angles[0] = (float)(vertical + (long double)angles[0]);
#else
		wave = sin(s->time * .001) * 80.0 * idleScale * .01;
		for (i = 0; i < 3; ++i)
			angles[i] = (float)(angles[i] + wave);
#endif
	}
	for (i = 0; i < 3; ++i)
		angles[i] -= s->kickAngles[i];
#endif
}
