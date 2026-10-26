#include "tce_bullet_renderer.h"
#include "tce_lightgrid.h"
#include "../game/tce_numeric.h"
#include "../game/tce_normalize_precision.h"
#include "../game/tce_ambient_precision.h"
extern qboolean CG_CalcMuzzlePoint(int, vec3_t);
extern void SnapVectorTowards(vec3_t, vec3_t);
static double bulletRandom(void)
{
	return (rand() & 32767) * (double)(1.f / 32767.f);
}
static float bulletParticleAlpha(void)
{
#if defined(__linux__)
	const float coefficient = .75f * (1.0f / 32767.0f);
	return (float)((long double)(rand() & 32767) * coefficient + .75f);
#else
	return (float)((bulletRandom() + 1) * .75f);
#endif
}
static float bulletMarkRadius(void)
{
#if defined(__linux__)
	const float coefficient = 8.0f * (1.0f / 32767.0f);
	return (float)((long double)(rand() & 32767) * coefficient + 8.0f);
#else
	return (float)((bulletRandom() + 1) * 8);
#endif
}
static float bulletMarkRotation(void)
{
#if defined(__linux__)
	const float coefficient = 360.0f * (1.0f / 32767.0f);
	return (float)((long double)(rand() & 32767) * coefficient);
#else
	return (float)(bulletRandom() * 360);
#endif
}
static long double bulletLength(const vec3_t v)
{
#if defined(_WIN32) || defined(__linux__)
	return TCE_VectorLengthValue(v);
#else
	return sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]);
#endif
}
/* The native x87 C0 branch also accepts unordered comparisons. */
static qboolean bulletBelow(long double value, long double limit)
{
#if defined(_WIN32) || defined(__linux__)
	return !(value >= limit);
#else
	return value < limit;
#endif
}
static void bulletNormalize(vec3_t v)
{
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	tceTriggerSum_t length;
	float normalized[3];
	unsigned control;

	__control87_2(0, 0, &control, NULL);
	if (TCE_FiniteNormalizeVector(v, normalized, &length, control))
	{
		VectorCopy(normalized, v);
		return;
	}
#endif
	VectorNormalize(v);
}
static int bulletLocalVolume(const tce_bulletRendererContext_t *context)
{
	float attenuation = context->liveAttenuation ? *context->liveAttenuation : context->attenuation;
#if defined(_WIN32) && defined(_MSC_VER) && defined(_M_IX86) && !defined(Q3_VM)
	int result;
	if (TCE_AmbientVolume(127, attenuation, 1.0f, &result))
	{
		return result;
	}
#endif
	return TCE_TruncateNativeInteger((1.0L - attenuation) * 127.0L);
}
void TCE_CG_Bullet(vec3_t end, int source, vec3_t normal, int flesh, int victim, int other,
                   float water, int seed, int damage, vec3_t suppliedStart, int predicted, const tce_bulletRendererContext_t *context)
{
	vec3_t    start, direction, opposite, projected, offset, tag, light, markEnd;
	trace_t   solid, liquid; int contents, destContents, particles, level, headshot, volume;
	centity_t *target = &cg_entities[victim];
	if (context->portal)
	{
		CG_Printf("ELITE PORTAL: CG_Bullet\n");
	}
	if (source == cg.snap->ps.clientNum && !predicted && cg_predictBullets.integer > 0)
	{
		return;
	}
	if (cg_entities[source].currentState.eFlags & 0x40000)
	{
		return;
	}
	/* Original leaves this local undefined when its tracer/muzzle gate fails.
	 * The retained source vector gives this otherwise undefined path a value. */
	VectorCopy(suppliedStart, start);
	if (cgs.antilag && other == cg.snap->ps.clientNum && (cg_entities[other].currentState.eFlags & 0x20))
	{
		vec3_t forward, right, up, muzzle; float horizontal;
#if defined(_WIN32) || defined(__linux__)
		long double vertical;
#else
		double vertical;
#endif
		int j;
		AngleVectors(cg.predictedPlayerState.viewangles, forward, right, up);
		VectorCopy(cg_entities[cg.snap->ps.viewlocked_entNum].currentState.pos.trBase, muzzle);
		if (cg_entities[cg.snap->ps.viewlocked_entNum].currentState.onFireStart)
		{
			VectorMA(muzzle, 16, up, muzzle);
		}
		horizontal = Q_crandom(&seed) * 100;
#if defined(_WIN32) || defined(__linux__)
		vertical = (long double)Q_crandom(&seed) * 100.0L;
#else
		vertical = Q_crandom(&seed) * 100.0;
#endif
		for (j = 0; j < 3; ++j)
		{
#if defined(_WIN32)
			long double spread = (long double)right[j] * horizontal + vertical * up[j];
			long double distance = (long double)forward[j] * 8192.0L;
			end[j] = (float)((spread + distance) + muzzle[j]);
#elif defined(__linux__)
			long double forwardPart = j == 2 ? 8192.0L * forward[j] : (long double)forward[j] * 8192.0L;
			long double base = forwardPart + muzzle[j];
			long double lateral = j == 2 ? (long double)horizontal * right[j] : (long double)right[j] * horizontal;
			long double rise = j == 2 ? vertical * up[j] : (long double)up[j] * vertical;
			end[j] = (float)((lateral + base) + rise);
#else
			end[j] = (float)((double)forward[j] * 8192 + vertical * up[j] + (double)right[j] * horizontal + muzzle[j]);
#endif
		}
		CG_Trace(&solid, muzzle, NULL, NULL, end, other, 0x6000081);
		SnapVectorTowards(solid.endpos, muzzle); VectorCopy(solid.endpos, end);
	}
	if (source >= 0 && cg_tracerChance.value > 0 && CG_CalcMuzzlePoint(source, start))
	{
		VectorCopy(suppliedStart, start); contents = CG_PointContents(start, 0); destContents = CG_PointContents(end, 0);
		if (contents == destContents && (contents & CONTENTS_WATER))
		{
			CG_BubbleTrail(start, end, .5f, 8);
		}
		else if (contents & CONTENTS_WATER)
		{
			trap_CM_BoxTrace(&liquid, end, start, NULL, NULL, 0, CONTENTS_WATER); CG_BubbleTrail(start, liquid.endpos, .5f, 8);
		}
		else if (destContents & CONTENTS_WATER)
		{
			VectorSubtract(cg.snap->ps.origin, end, offset);
			if (bulletBelow(bulletLength(offset), 1024))
			{
				trap_CM_BoxTrace(&liquid, start, end, NULL, NULL, 0, CONTENTS_WATER); CG_BubbleTrail(end, liquid.endpos, .5f, 8);
			}
		}
	}
	if (!flesh)
	{
		if (!CG_CalcMuzzlePoint(source, start) && !cg.snap->ps.persistant[PERS_HWEAPON_USE])
		{
			return;
		}
		VectorCopy(suppliedStart, start); VectorSubtract(end, start, direction); VectorNormalizeFast(direction);
		VectorMA(end, 4, direction, end);
		CG_Trace(&solid, start, NULL, NULL, end, 0, 0x81); CG_Trace(&liquid, start, NULL, NULL, end, -1, 0x38);
#if defined(_WIN32) || defined(__linux__)
		if (!(solid.fraction > liquid.fraction))
#else
		if (solid.fraction <= liquid.fraction)
#endif
		{
			if (bulletRandom() < .25f)
			{
				CG_Trace(&liquid, start, NULL, NULL, end, 0, 0x10000);
				if (liquid.fraction < solid.fraction && ((unsigned)liquid.surfaceFlags & 0xff000000) == 0x14000000)
				{
					context->wall(3, 1, liquid.endpos, liquid.plane.normal, direction, liquid.surfaceFlags, liquid.entityNum == ENTITYNUM_WORLD, 0);
				}
			}
			VectorNegate(direction, opposite); context->wall(3, 1, solid.endpos, solid.plane.normal, opposite, solid.surfaceFlags, solid.entityNum == ENTITYNUM_WORLD, 0);
		}
		else
		{
			volume = context->soundVolume(end, 127, 1200, 0);
			trap_S_StartSoundVControl(end, -1, 0, cgs.media.sfx_bullet_waterhit[rand() % 5], volume);
			context->wall(3, 2, liquid.endpos, liquid.plane.normal, liquid.plane.normal, liquid.surfaceFlags, liquid.entityNum == ENTITYNUM_WORLD, 0);
		}
		return;
	}
	if (damage < 10)
	{
		particles = level = 0;
	}
	else if (damage < 30)
	{
		particles = 2; level = 20;
	}
	else if (damage < 60)
	{
		particles = 4; level = 40;
	}
	else if (damage == 999)
	{
		particles = level = 0;
	}
	else
	{
		particles = 6; level = 60;
	}
	if (victim < 64 && level > 20)
	{
		/* Reference samples an uninitialized projected vector here. Keep the
		 * defined impact point; this explicit undefined-behavior repair keeps
		 * full equivalence status open until the controller is integrated. */
		TCE_CG_LightForParticleSimple(TCE_CG_MapLightGrid(), end, light); CG_Bleed(end, victim, light);
	}
	VectorSubtract(end, start, direction); bulletNormalize(direction);
	VectorSubtract(target->currentState.pos.trBase, end, offset); offset[2] = 0;
	{ long double distance = bulletLength(offset); int j; for (j = 0; j < 3; ++j)
		{
#if defined(__linux__)
			long double product = distance * direction[j];
			offset[j] = (float)product;
			projected[j] = (float)(product + end[j]);
#else
			offset[j] = (float)(distance * direction[j]);
#endif
		}
	}
#if !defined(__linux__)
	VectorAdd(offset, end, projected);
#endif
	CG_GetOriginForTag(target, &target->pe.headRefEnt, "tag_mouth", 0, tag, NULL); tag[2] += 5;
	VectorSubtract(tag, projected, offset); headshot = bulletBelow(bulletLength(offset), 10);
	VectorNegate(direction, opposite);
	context->particle(projected, opposite, particles, 16, bulletParticleAlpha());
	context->particle(projected, direction, 0, 16, bulletParticleAlpha());
	if (particles > 4)
	{
		context->particle(projected, opposite, 0, 16, bulletParticleAlpha());
	}
	if (level)
	{
		if (victim == cg.snap->ps.clientNum)
		{
#if defined(__linux__)
			/* Linux selects the sound before evaluating the current deafness. */
			int soundIndex = rand() % 5;
			volume = bulletLocalVolume(context);
			trap_S_StartSoundVControl(NULL, victim, 5, cgs.media.sfx_bullet_fleshhit[soundIndex], volume);
#else
			volume = bulletLocalVolume(context);
			trap_S_StartSoundVControl(NULL, victim, 5, cgs.media.sfx_bullet_fleshhit[rand() % 5], volume);
#endif
		}
		else
		{
			volume = context->soundVolume(target->currentState.origin, 127, 1200, 0);
			if (volume)
			{
				trap_S_StartSoundVControl(target->currentState.origin, ENTITYNUM_WORLD, 5, cgs.media.sfx_bullet_fleshhit[rand() % 5], volume);
			}
		}
	}
	/* Corrected original ABI and differential capture identify the shooter. */
	if (!cg_blood.integer || !CG_CalcMuzzlePoint(source, start))
	{
		return;
	}
	if (!(bulletRandom() < 1.0f || headshot))
	{
		return;
	}
	VectorCopy(suppliedStart, start); VectorSubtract(end, start, direction); bulletNormalize(direction);
	VectorMA(end, 64, direction, markEnd); trap_CM_BoxTrace(&solid, end, markEnd, NULL, NULL, 0, 0x4000081);
	if (solid.fraction >= 1.0f || (bulletRandom() <= .5f && !headshot))
	{
		VectorCopy(end, markEnd); markEnd[2] -= 64; trap_CM_BoxTrace(&solid, end, markEnd, NULL, NULL, 0, 0x4000081);
#if defined(__linux__)
		/* Linux compares 1 against the fraction and returns on C0 or C3. */
		if (!(solid.fraction < 1.0f))
#else
		if (solid.fraction >= 1.0f)
#endif
		{
			return;
		}
	}
	TCE_CG_LightForParticleDirected(TCE_CG_MapLightGrid(), solid.endpos, solid.plane.normal, light);
	{
#if defined(_WIN32)
		int markDuration = TCE_MulInt32(cg_bloodTime.integer, 1000);
#else
		int markDuration;
#endif
		float radius = bulletMarkRadius(), rotation = bulletMarkRotation();
		int shader = rand() % 5;
#if !defined(_WIN32)
		markDuration = TCE_MulInt32(cg_bloodTime.integer, 1000);
#endif
		context->mark(cgs.media.bloodDotShaders[shader], solid.endpos, solid.plane.normal, rotation,
		              light[0], light[1], light[2], 1, 1, radius, 0, markDuration);
	}
	*context->lastBloodSpat = cg.time;
}
