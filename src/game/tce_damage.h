/* Original Windows G_Damage200589c0 body-volume and weighting calculations.
 * Included by the damage controller and its original-binary comparison test.
 * This is an internal decomposition, not an independently completed original function. */
#ifndef TCE_DAMAGE_H
#define TCE_DAMAGE_H
#include "tce_numeric.h"
typedef struct
{
	vec3_t origin, angles;
	float lean;
	int weapon, prone, ducked, aiming, enlarged;
} tceDamagePose_t;
typedef struct { vec3_t origin, mins, maxs; } tceDamageVolume_t;
#if defined(__linux__)
typedef double tce_damageScale_t;
#else
typedef float tce_damageScale_t;
#endif

static void TCE_DamageVolumes(const tceDamagePose_t *p, tceDamageVolume_t out[3])
{
	float  s = p->enlarged?1.25f:1.0f;
	float  hf, hr, hu, base, tf = 0, side = 0, lf = 0, tw, lw, low, high, offset;
	vec3_t angles, f, r, u, flatF, flatR;
	int    i, knife = p->weapon == 1;
	int    heavy = p->weapon == 4 || p->weapon == 9 || p->weapon == 30;
	int    light = p->weapon == 2 || p->weapon == 39 || p->weapon == 40 || p->weapon == 7 || p->weapon == 37 || p->weapon == 38 ||
	               p->weapon == 10 || p->weapon == 3 || p->weapon == 52 || p->weapon == 14 || p->weapon == 53 || p->weapon == 54;
	if (p->prone)
	{
		hf = 12 * s; hr = 0; hu = 12 * s; lf = -16 * s; side = 6 * s; lw = 16 * s; base = -18 * s; tf = 12 * s; tw = 12 * s;
	}
	else
	{
		hf = 4 * s; hu = 21 * s; lw = 12 * s; tw = 12 * s;
		if (p->ducked)
		{
			hr = 4 * s; base = s; if (p->aiming)
			{
				base = 6 * s; tf = 6 * s; hr += 2 * s; hf += 8 * s;
			}
		}
		else
		{
			hr = 3 * s; base = hu;
			if (p->aiming)
			{
				base += 2 * s; hf += lw; hu -= 2 * s;
			}
			if (knife)
			{
				hf += 6 * s; hr -= 3 * s; hu -= 2 * s; base += 2 * s;
			}
			else if (heavy)
			{
				hf -= 3 * s; hr -= 4 * s; base += 2 * s;
			}
			else if (light)
			{
				hf -= 2 * s; hr -= s; base += 2 * s;
			}
		}
		if (p->lean > 0)
		{
#if defined(__linux__)
			long double lean = (long double)s * p->lean;
			hr = (float)(lean * (p->ducked ? 0.2857142984867096f : 0.24390244483947754f) + hr);
			hu = (float)((long double)hu - lean * 0.1538461595773697f);
			side = (float)(lean * 0.20000000298023224f);
#else
			side = s * p->lean; hr += (p->ducked?0.2857142984867096f:0.24390244483947754f) * side;
			hu  -= side * 0.1538461595773697f; side *= 0.20000000298023224f;
#endif
		}
#if defined(__linux__)
		else if (!(p->lean >= 0))
#else
		else if (p->lean < 0)
#endif
		{
#if defined(__linux__)
			long double lean = (long double)s * p->lean;
			hr = (float)(lean * (p->ducked ? 0.4000000059604645f : 0.5555555820465088f) + hr);
			hu = (float)(lean * 0.1428571492433548f + hu);
			side = (float)(lean * 0.1666666716337204f);
#else
			side = s * p->lean; hr += side * (p->ducked?0.4000000059604645f:0.5555555820465088f);
			hu  += side * 0.1428571492433548f; side *= 0.1666666716337204f;
#endif
		}
	}
	VectorCopy(p->angles, angles); if (angles[0] > 180)
	{
		angles[0] -= 360;
	}
	angles[0] *= 0.75f; if (p->prone)
	{
		angles[0] *= 0.5f;
	}
	AngleVectors(angles, f, r, u); VectorCopy(f, flatF); flatF[2] = 0; VectorNormalizeFast(flatF);
	VectorCopy(r, flatR); flatR[2]                                = 0; VectorNormalizeFast(flatR);
	for (i = 0; i < 3; ++i)
	{
#if defined(__linux__)
		long double wideOffset = (long double)p->origin[i] + (i == 2 ? base : 0);
		wideOffset += (long double)flatF[i] * tf;
		wideOffset += (long double)flatR[i] * side;
		wideOffset += (long double)f[i] * hf;
		wideOffset += (long double)r[i] * hr;
		wideOffset += (long double)u[i] * hu;
		offset = (float)wideOffset;
#else
		offset           = p->origin[i] + (i == 2?base:0);
		offset          += flatF[i] * tf; offset += flatR[i] * side; offset += f[i] * hf; offset += r[i] * hr; offset += u[i] * hu;
#endif
		out[0].origin[i] = offset; out[0].mins[i] = -6; out[0].maxs[i] = 6;
	}
#if defined(__linux__)
	{
		long double baseZ = (long double)base + p->origin[2];
		low = (float)(baseZ - (long double)6.0f * s);
		high = p->prone ? (float)(baseZ + (long double)6.0f * s) :
		       (float)(((baseZ + hu) - (long double)4.0f * s) -
		               fabsl((long double)f[2]) * s * 7.0f);
		for (i = 0; i < 3; ++i)
		{
			long double center = i == 2 ? ((long double)low + high) * 0.5f : p->origin[i];
			center += (long double)flatF[i] * tf;
			center += (long double)flatR[i] * side;
			out[1].origin[i] = (float)center;
			out[1].mins[i] = -tw;
			out[1].maxs[i] = tw;
		}
		out[1].mins[2] = (float)(((long double)low - high) * 0.5f);
		out[1].maxs[2] = (float)(((long double)high - low) * 0.5f);
		if (!p->prone)
		{
			high = low;
			low = (float)((long double)p->origin[2] - (long double)s * 24.0f);
		}
		for (i = 0; i < 3; ++i)
		{
			long double center = i == 2 ? ((long double)low + high) * 0.5f : p->origin[i];
			center += (long double)flatF[i] * lf;
			center += (long double)flatR[i] * 0.0f;
			out[2].origin[i] = (float)center;
			out[2].mins[i] = -lw;
			out[2].maxs[i] = lw;
		}
		out[2].mins[2] = (float)(((long double)low - high) * 0.5f);
		out[2].maxs[2] = (float)(((long double)high - low) * 0.5f);
	}
#else
	low  = base + p->origin[2] - 6 * s;
	high = p->prone?6 * s + p->origin[2] + base:(hu + base + p->origin[2] - 4 * s) - (float)(fabs(f[2]) * s * 7.0);
	for (i = 0; i < 3; ++i)
	{
		out[1].origin[i]  = i == 2?(high + low) * 0.5f:p->origin[i];
		out[1].origin[i] += flatF[i] * tf; out[1].origin[i] += flatR[i] * side;
		out[1].mins[i]    = -tw; out[1].maxs[i] = tw;
	}
	out[1].mins[2] = (high - low) * -0.5f; out[1].maxs[2] = (high - low) * 0.5f;
	if (p->prone)
	{
		low = base + p->origin[2] - 6 * s; high = 6 * s + p->origin[2] + base;
	}
	else
	{
		high = low; low = p->origin[2] - 24 * s;
	}
	for (i = 0; i < 3; ++i)
	{
		out[2].origin[i] = i == 2?(high + low) * 0.5f:p->origin[i]; out[2].origin[i] += flatF[i] * lf;
		out[2].mins[i]   = -lw; out[2].maxs[i] = lw;
	}
	out[2].mins[2] = (high - low) * -0.5f; out[2].maxs[2] = (high - low) * 0.5f;
#endif
}

#if defined(__linux__)
static float TCE_DamageRayWeightWide(const long double center[3], const vec3_t point,
                                     const vec3_t dir, float radius, int root)
{
	long double delta[3], dot, length, weight;
	vec3_t perpendicular;
	float storedWeight;
	int i;
	for (i = 0; i < 3; ++i)
	{
		delta[i] = center[i] - point[i];
	}
	dot = (delta[1] * dir[1] + delta[0] * dir[0]) + delta[2] * dir[2];
	for (i = 0; i < 3; ++i)
	{
		perpendicular[i] = (float)(delta[i] - dot * dir[i]);
	}
	length = VectorLength(perpendicular);
	if (root)
	{
		weight = (1.0L / radius) * ((long double)radius - length);
		storedWeight = (float)weight;
		if (weight < 0)
		{
			storedWeight = 0;
		}
		return (float)sqrtl((long double)storedWeight);
	}
	storedWeight = (float)((long double)radius - length);
	storedWeight = (float)((1.0L / radius) * storedWeight);
	return storedWeight < 0 ? 0 : storedWeight;
}
#endif

static float TCE_DamageRayWeight(const vec3_t center, const vec3_t point, const vec3_t dir, float radius, int root)
{
#if defined(__linux__)
	long double wideCenter[3];
	int i;
	for (i = 0; i < 3; ++i)
	{
		wideCenter[i] = center[i];
	}
	return TCE_DamageRayWeightWide(wideCenter, point, dir, radius, root);
#else
	vec3_t delta; float dot, length, w; VectorSubtract(center, point, delta);
	dot       = (float)((double)delta[1] * dir[1] + (double)delta[2] * dir[2] + (double)delta[0] * dir[0]);
	delta[0] -= dot * dir[0]; delta[1] -= dot * dir[1]; delta[2] -= dot * dir[2]; length = VectorLength(delta);
	w         = (radius - length) / radius; if (w < 0)
	{
		w = 0;
	}
	return root?(float)sqrt(w):w;
#endif
}

static int TCE_DamageWeights(const tceDamageVolume_t volumes[3], const float fractions[3],
                             const vec3_t point, const vec3_t dir, float scale, int damage, int amounts[3], tce_damageScale_t *scales)
{
	float h = 0, t = 0, l = 0, ht = 0, tl = 0, head = 0, legs = 0
#if !defined(__linux__)
	, torso
#endif
	;
#if !defined(__linux__)
	vec3_t center;
#endif
	int i;
#if defined(__linux__)
	if (!(fractions[0] >= 1))
#else
	if (fractions[0] < 1)
#endif
	{
		h = TCE_DamageRayWeight(volumes[0].origin, point, dir, scale * 7, 1);
	}
#if defined(__linux__)
	if (!(fractions[1] >= 1))
#else
	if (fractions[1] < 1)
#endif
	{
		t = TCE_DamageRayWeight(volumes[1].origin, point, dir, scale * 14, 1);
	}
#if defined(__linux__)
	if (!(fractions[2] >= 1))
#else
	if (fractions[2] < 1)
#endif
	{
		l = TCE_DamageRayWeight(volumes[2].origin, point, dir, scale * 14, 1);
	}
#if defined(__linux__)
	if (!(fractions[0] >= 1) || !(fractions[1] >= 1))
#else
	if (fractions[0] < 1 || fractions[1] < 1)
#endif
	{
#if defined(__linux__)
		long double wideCenter[3];
		for (i = 0; i < 3; ++i)
		{
			wideCenter[i] = ((long double)volumes[1].origin[i] - volumes[0].origin[i]) *
			                0.33000001311302185f + volumes[0].origin[i];
		}
		ht = TCE_DamageRayWeightWide(wideCenter, point, dir, scale * 7, 0);
#else
		for (i = 0; i < 3; ++i)
			center[i] = (volumes[1].origin[i] - volumes[0].origin[i]) * 0.33000001311302185f + volumes[0].origin[i];
		ht = TCE_DamageRayWeight(center, point, dir, scale * 7, 0);
#endif
	}
#if defined(__linux__)
	if (!(fractions[2] >= 1) || !(fractions[1] >= 1))
#else
	if (fractions[2] < 1 || fractions[1] < 1)
#endif
	{
#if defined(__linux__)
		long double wideCenter[3];
		for (i = 0; i < 3; ++i)
		{
			wideCenter[i] = ((long double)volumes[2].origin[i] - volumes[1].origin[i]) *
			                0.33000001311302185f + volumes[1].origin[i];
		}
		tl = TCE_DamageRayWeightWide(wideCenter, point, dir, scale * 14, 0);
#else
		for (i = 0; i < 3; ++i)
			center[i] = (volumes[2].origin[i] - volumes[1].origin[i]) * 0.33000001311302185f + volumes[1].origin[i];
		tl = TCE_DamageRayWeight(center, point, dir, scale * 14, 0);
#endif
	}
#if defined(__linux__)
	{
		long double wideTorso;
		if (!(fractions[0] >= fractions[1]) && !(fractions[0] >= fractions[2]))
		{
			long double sum = (long double)h + ht;
			head = (float)sum;
			if (sum > 1)
			{
				head = 1;
			}
			wideTorso = (1.0L - head) * t;
		}
		else if (!(fractions[2] >= fractions[0]) && !(fractions[2] >= fractions[1]))
		{
			legs = l;
			wideTorso = (1.0L - l) * t + tl;
		}
		else
		{
			legs = (float)((1.0L - t) * l);
			head = (float)((1.0L - t) * h);
			wideTorso = ((long double)tl + t) + ht;
		}
		amounts[0] = TCE_FloatIsZeroOrNaN(head) ? 0 :
		             TCE_TruncateNativeInteger(((long double)damage * head) * 2.0f);
		amounts[1] = (wideTorso == 0 || wideTorso != wideTorso) ? 0 :
		             TCE_TruncateNativeInteger((long double)damage * wideTorso);
		amounts[2] = TCE_FloatIsZeroOrNaN(legs) ? 0 :
		             TCE_TruncateNativeInteger((long double)damage * legs * 0.5f);
		if (scales)
		{
			scales[0] = (double)((long double)head + head);
			scales[1] = (double)wideTorso;
			scales[2] = (double)((long double)legs * 0.5f);
		}
	}
#else
	if (fractions[0] < fractions[1] && fractions[0] < fractions[2])
	{
		head = h + ht; if (head > 1)
		{
			head = 1;
		}
		torso = (1 - head) * t;
	}
	else if (fractions[2] < fractions[0] && fractions[2] < fractions[1])
	{
		legs = l; torso = (1 - l) * t + tl;
	}
	else
	{
		legs = (1 - t) * l; head = (1 - t) * h; torso = tl + t + ht;
	}
	amounts[0] = (int)((double)damage * head * 2); amounts[1] = (int)((double)damage * torso); amounts[2] = (int)((double)damage * legs * 0.5);
#endif
#if !defined(__linux__)
	if (scales)
	{
		scales[0] = head * 2; scales[1] = torso; scales[2] = legs * 0.5f;
	}
#endif
#if defined(__linux__)
	return TCE_AddInt32(TCE_AddInt32(amounts[2], amounts[0]), amounts[1]);
#else
	return amounts[0] + amounts[1] + amounts[2];
#endif
}
#endif
