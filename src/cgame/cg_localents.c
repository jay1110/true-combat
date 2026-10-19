
// cg_localents.c -- every frame, generate renderer commands for locally
// processed entities, like smoke puffs, gibs, shells, etc.

#include "cg_local.h"
#include <stddef.h>
#include "tce_add_fragment.h"
#include "tce_impact_sparks.h"
#include "tce_fragment_sound.h"
#include "tce_flash.h"
#include "tce_smoke_grenade.h"
#include "../game/tce_trajectory.h"
#include "../game/tce_numeric.h"

// Ridah, increased this
//#define	MAX_LOCAL_ENTITIES	512
#define MAX_LOCAL_ENTITIES  768     // renderer can only handle 1024 entities max, so we should avoid
                                    // overwriting game entities
// done.

localEntity_t cg_localEntities[MAX_LOCAL_ENTITIES];
localEntity_t cg_activeLocalEntities;       // double linked list
localEntity_t *cg_freeLocalEntities;        // single linked list

// Ridah, debugging
int localEntCount = 0;

/*
===================
CG_InitLocalEntities

This is called at startup and for tournement restarts
===================
*/
void    CG_InitLocalEntities(void)
{
	int i;

	memset(cg_localEntities, 0, sizeof(cg_localEntities));
	cg_activeLocalEntities.next = &cg_activeLocalEntities;
	cg_activeLocalEntities.prev = &cg_activeLocalEntities;
	cg_freeLocalEntities        = cg_localEntities;
	for ( i = 0 ; i < MAX_LOCAL_ENTITIES - 1 ; i++ )
	{
		cg_localEntities[i].next = &cg_localEntities[i + 1];
	}

	// Ridah, debugging
	localEntCount = 0;
}


/*
==================
CG_FreeLocalEntity
==================
*/
void CG_FreeLocalEntity(localEntity_t *le)
{
	if (!le->prev)
	{
		CG_Error("CG_FreeLocalEntity: not active");
	}

	// Ridah, debugging
	localEntCount--;
//	trap_Print( va("FreeLocalEntity: locelEntCount = %d\n", localEntCount) );
	// done.

	// remove from the doubly linked active list
	le->prev->next = le->next;
	le->next->prev = le->prev;

	// the free list is only singly linked
	le->next             = cg_freeLocalEntities;
	cg_freeLocalEntities = le;
}

/*
===================
CG_AllocLocalEntity

Will allways succeed, even if it requires freeing an old active entity
===================
*/
localEntity_t *CG_AllocLocalEntity(void)
{
	localEntity_t *le;

	if (!cg_freeLocalEntities)
	{
		// no free entities, so free the one at the end of the chain
		// remove the oldest active entity
		CG_FreeLocalEntity(cg_activeLocalEntities.prev);
	}

	// Ridah, debugging
	localEntCount++;
//	trap_Print( va("AllocLocalEntity: locelEntCount = %d\n", localEntCount) );
	// done.

	le                   = cg_freeLocalEntities;
	cg_freeLocalEntities = cg_freeLocalEntities->next;

	memset(le, 0, sizeof(*le));

	// link into the active list
	le->next                          = cg_activeLocalEntities.next;
	le->prev                          = &cg_activeLocalEntities;
	cg_activeLocalEntities.next->prev = le;
	cg_activeLocalEntities.next       = le;
	return le;
}


/*
====================================================================================

FRAGMENT PROCESSING

A fragment localentity interacts with the environment in some way (hitting walls),
or generates more localentities along a trail.

====================================================================================
*/

/*
================
CG_BloodTrail

Leave expanding blood puffs behind gibs
================
*/
/* CG_BloodTrail and CG_FragmentBounceMark: tce_fragment_blood.c. */

/*
================
CG_FragmentBounceSound
================
*/
/* Original TC bounce controller, all native fragment producers. */
void CG_FragmentBounceSound(localEntity_t *le, trace_t *trace)
{
	tce_fragmentSoundContext_t context;
	memset(&context, 0, sizeof(context));
	VectorCopy(cg.refdef_current->vieworg, context.listener);
	memcpy(context.casing, cgs.media.tceCasingSounds, sizeof(context.casing));
	/* TC registration clears all four legacy brass banks (325888a8..d4).
	   Their SDK shell sounds must not leak into the TC material controller. */
	memcpy(context.rubble, cgs.media.sfx_rubbleBounce, sizeof(context.rubble));
	memcpy(context.type6, cgs.media.tceGlassFallSounds, sizeof(context.type6));
	/* Original CG_RegisterSounds explicitly leaves type12 handle zero. */
	context.type12          = 0;
	context.soundVariant    = cg.tceSoundEnvironment;
	context.attenuation     = tceFlash.deafness;
	context.distanceVariant = tceSmokeNewBBox;
	context.disabled        = cg.tcePortalScopeRendering;
	TCE_CG_FragmentBounceSound(le, trace, &context);
}

/*
================
CG_ReflectVelocity
================
*/
/* CG_ReflectVelocity is implemented in tce_reflect_velocity.c. */

//----(SA)	added

/*
==============
CG_AddEmitter
==============
*/
void CG_AddEmitter(localEntity_t *le)
{
	vec3_t dir;

	if (le->breakCount > cg.time)    // using 'breakCount' for 'wait'
	{
		return;
	}

	VectorScale(le->angles.trBase, 30, dir);
	CG_Particle_OilParticle(cgs.media.oilParticle, le->pos.trBase, dir, 15000, le->ownerNum);

	le->breakCount = cg.time + 50;
}

//----(SA)	end


void CG_Explodef(vec3_t origin, vec3_t dir, int mass, int type, qhandle_t sound, int forceLowGrav, qhandle_t shader);

/*
================
CG_AddFragment
================
*/
/* All original LE_FRAGMENT producers share TC300450f0. The transport marker
   tceFragment remains readable in old fixtures but does not select SDK physics. */
void CG_AddFragment(localEntity_t *le)
{
	tce_fragmentContext_t context;
	memset(&context, 0, sizeof(context));
	context.evaluateTrajectory = TCE_BG_EvaluateTrajectory;
	context.bounceSound        = CG_FragmentBounceSound;
	context.gravityScale       = le->tceGravity;
	context.sizeVariant        = tceSmokeNewBBox;
	TCE_CG_AddFragment(le, &context);
}

/*
================
CG_AddMovingTracer
================
*/
/* Whole TC30045a80 / Linux0007f16e. */
void CG_AddMovingTracer(localEntity_t *le)
{
	vec3_t start, end, direction;
	int    axis;
	TCE_BG_EvaluateTrajectory(&le->pos, cg.time, start, qfalse, -1, 1.0f);
	VectorNormalize2(le->pos.trDelta, direction);
	/* Original x87 rounds only after the multiply and add (30045ab6..af2). */
	for (axis = 0; axis < 3; axis++)
		end[axis] = (float)((double)cg_tracerLength.value * direction[axis] + start[axis]);
	CG_DrawTracer(start, end);
}

/*
================
CG_AddSparkElements
================
*/
/* Whole Windows30045b10 / Linux0007f226. */
void CG_AddSparkElements(localEntity_t *le)
{
	vec3_t endpoint; trace_t trace;
	float  time = (float)(cg.time - cg.frametime), width;
	double fraction;
	int    duration = le->endTime - le->startTime;
	do
	{
		TCE_BG_EvaluateTrajectory(&le->pos, cg.time, endpoint, qfalse, -1, 1.0f);
		CG_Trace(&trace, le->refEntity.origin, NULL, NULL, endpoint, -1, 0x6000081);
		if (trace.startsolid)
		{
			VectorCopy(endpoint, trace.endpos); trace.fraction = 1;
		}
		VectorCopy(trace.endpos, le->refEntity.origin);
		time = (float)cg.frametime * trace.fraction + time;
		/* Original x87 retains the quotient through both final argument stores. */
		fraction          = (double)(cg.time - le->startTime) / duration;
		width             = (float)(fraction * ((duration > 400) + 1) * 3.0);
		le->headJuncIndex = CG_AddSparkJunc(le->headJuncIndex, le, le->refEntity.customShader,
		                                    le->refEntity.origin, 200, 1.0f - fraction, 0, width, width);
		if (trace.fraction < 1)
		{
			CG_FreeLocalEntity(le); return;
		}
		if (trace.fraction == 1)
		{
			return;
		}
	}
	while (time < (float)cg.time);
}

/*
================
CG_AddFuseSparkElements
================
*/
/* Whole TC30045c90 / Linux0007f458: retain the original float clock. */
void CG_AddFuseSparkElements(localEntity_t *le)
{
	static vec3_t white = { 1, 1, 1 };
	float         time  = (float)le->lastTrailTime;
	while (time < (float)cg.time)
	{
		/* x87 keeps the quotient until the final width store (30045d07..15). */
		double life  = ((double)time - le->startTime) / (le->endTime - le->startTime);
		float  width = (float)(1.0 - life);
		TCE_BG_EvaluateTrajectory(&le->pos, (int)time, le->refEntity.origin, qfalse, -1, 1.0f);
		le->headJuncIndex = CG_AddTrailJunc(le->headJuncIndex, le,
		                                    cgs.media.sparkParticleShader, (int)time, STYPE_STRETCH, le->refEntity.origin,
		                                    (int)(life * (le->endTime - le->startTime) * 0.5),
		                                    1.0f, 0.0f, width, width, TJFL_SPARKHEADFLARE, white, white, 0, 0);
		time             += 10.0f;
		le->lastTrailTime = (int)time;
	}
}

/*
================
CG_AddBloodElements
================
*/
/* Whole TC300464d0 / Linux0007ff7a, including original five-bounce cap. */
void CG_AddBloodElements(localEntity_t *le)
{
	vec3_t  endpoint;
	trace_t trace;
	float   time = (float)(cg.time - cg.frametime), alpha;
	int     bounce;
	for (bounce = 0; bounce < 5; bounce++)
	{
		TCE_BG_EvaluateTrajectory(&le->pos, cg.time, endpoint, qfalse, -1, 1.0f);
		CG_Trace(&trace, le->refEntity.origin, NULL, NULL, endpoint, -1, 0x6000081);
		if (trace.startsolid)
		{
			VectorCopy(endpoint, trace.endpos); trace.fraction = 1.0f;
		}
		VectorCopy(trace.endpos, le->refEntity.origin);
		time             += (float)cg.frametime * trace.fraction;
		alpha             = 1.0f - (float)(cg.time - le->startTime) / (float)(le->endTime - le->startTime);
		le->headJuncIndex = CG_AddTrailJunc(le->headJuncIndex, le, cgs.media.bloodTrailShader,
		                                    cg.time, STYPE_STRETCH, le->refEntity.origin, 200, alpha, alpha, 3.0f, 5.0f, 4,
		                                    le->color, le->color, 0, 0);
		if (trace.fraction < 1.0f)
		{
			CG_ReflectVelocity(le, &trace); le->pos.trTime = (int)time;
		}
		if (trace.fraction == 1.0f || time >= (float)cg.time)
		{
			return;
		}
	}
}

/*
================
CG_AddDebrisElements
================
*/
/* Whole Windows30046670 / Linux00080200, original 50ms trail stepping. */
void CG_AddDebrisElements(localEntity_t *le)
{
	vec3_t endpoint; trace_t trace;
	int    time;
	double fraction, fade;
	for (time = le->lastTrailTime + 50; time < cg.time; time += 50)
	{
		TCE_BG_EvaluateTrajectory(&le->pos, time, endpoint, qfalse, -1, 1.0f);
		CG_Trace(&trace, le->refEntity.origin, NULL, NULL, endpoint, -1, 0x6000081);
		if (trace.startsolid)
		{
			VectorCopy(endpoint, trace.endpos); trace.fraction = 1;
		}
		VectorCopy(trace.endpos, le->refEntity.origin);
		/* 30046720..a1: no float store before fade, width/lifetime conversions. */
		fraction = (double)(time - le->startTime) / (le->endTime - le->startTime);
		fade     = ((1.0 - fraction) + 1.0) * .5;
		if (le->effectFlags & 1)
		{
			le->headJuncIndex2 = CG_AddSmokeJunc(le->headJuncIndex2, le,
			                                     cgs.media.smokeTrailShader, le->refEntity.origin, (int)(2000.0 * fade),
			                                     (trace.fraction == 1) * fade, 1, (float)(int)(60.0 * fade));
		}
		if (trace.fraction < 1)
		{
			CG_ReflectVelocity(le, &trace);
			if (VectorLengthSquared(le->pos.trDelta) < 1)
			{
				CG_FreeLocalEntity(le); return;
			}
			le->pos.trTime = time;
		}
		le->lastTrailTime = time;
	}
}

// Rafael Shrapnel
/*
===============
CG_AddShrapnel
===============
*/
void CG_AddShrapnel(localEntity_t *le)
{
	vec3_t  newOrigin;
	trace_t trace;

	if (le->pos.trType == TR_STATIONARY)
	{
		// sink into the ground if near the removal time
		int   t;
		float oldZ;

		t = le->endTime - cg.time;
		if (t < SINK_TIME)
		{
			// we must use an explicit lighting origin, otherwise the
			// lighting would be lost as soon as the origin went
			// into the ground
			VectorCopy(le->refEntity.origin, le->refEntity.lightingOrigin);
			le->refEntity.renderfx  |= RF_LIGHTING_ORIGIN;
			oldZ                     = le->refEntity.origin[2];
			le->refEntity.origin[2] -= 16 * (1.0 - (float)t / SINK_TIME);
			trap_R_AddRefEntityToScene(&le->refEntity);
			le->refEntity.origin[2] = oldZ;
		}
		else
		{
			trap_R_AddRefEntityToScene(&le->refEntity);
			CG_AddParticleShrapnel(le);
		}

		return;
	}

	// calculate new position
	BG_EvaluateTrajectory(&le->pos, cg.time, newOrigin, qfalse, -1);

	// trace a line from previous position to new position
	CG_Trace(&trace, le->refEntity.origin, NULL, NULL, newOrigin, -1, CONTENTS_SOLID);
	if (trace.fraction == 1.0)
	{
		// still in free fall
		VectorCopy(newOrigin, le->refEntity.origin);

		if (le->leFlags & LEF_TUMBLE)
		{
			vec3_t angles;

			BG_EvaluateTrajectory(&le->angles, cg.time, angles, qtrue, -1);
			AnglesToAxis(angles, le->refEntity.axis);
		}

		trap_R_AddRefEntityToScene(&le->refEntity);
		CG_AddParticleShrapnel(le);
		return;
	}

	// if it is in a nodrop zone, remove it
	// this keeps gibs from waiting at the bottom of pits of death
	// and floating levels
	if (CG_PointContents(trace.endpos, 0) & CONTENTS_NODROP)
	{
		CG_FreeLocalEntity(le);
		return;
	}

	// leave a mark
	CG_FragmentBounceMark(le, &trace);

	// do a bouncy sound
	CG_FragmentBounceSound(le, &trace);

	// reflect the velocity on the trace plane
	CG_ReflectVelocity(le, &trace);

	trap_R_AddRefEntityToScene(&le->refEntity);
	CG_AddParticleShrapnel(le);
}
// done.

/*
=====================================================================

TRIVIAL LOCAL ENTITIES

These only do simple scaling or modulation before passing to the renderer
=====================================================================
*/

/*
====================
CG_AddFadeRGB
====================
*/
void CG_AddFadeRGB(localEntity_t *le)
{
	refEntity_t *re;
	double      c;

	re = &le->refEntity;

	c  = (double)(le->endTime - cg.time) * le->lifeRate;
	c *= 0xff;

	re->shaderRGBA[0] = le->color[0] * c;
	re->shaderRGBA[1] = le->color[1] * c;
	re->shaderRGBA[2] = le->color[2] * c;
	re->shaderRGBA[3] = le->color[3] * c;

	trap_R_AddRefEntityToScene(re);
}

/*
==================
CG_AddMoveScaleFade
==================
*/
/* Original local-entity type 16, emitted by CG_ParticleTest. */
void CG_AddMoveGravityFade(localEntity_t *le)
{
	refEntity_t *re = &le->refEntity; vec3_t delta; int i;
	float       remaining = (float)((double)(le->endTime - cg.time) * le->lifeRate);
	double      progress = 1.0 - remaining, elapsed = (float)(cg.time - le->startTime), length;
	if (le->fadeInTime < cg.time)
	{
		re->shaderRGBA[3] = (byte)(int)((double)(le->endTime - cg.time) / (le->endTime - le->fadeInTime) * le->color[3] * 255.0f);
	}
	for (i = 0; i < 3; ++i)
		re->origin[i] = (float)(progress * le->pos.trDelta[i] + le->pos.trBase[i]);
	re->origin[2] = (float)(re->origin[2] - elapsed * le->tceGravity * elapsed * .0004f);
	if (le->leFlags & 2)
	{
		re->rotation = (float)((double)remaining * le->angles.trDelta[0] + le->angles.trBase[0]);
	}
	VectorSubtract(re->origin, cg.refdef.vieworg, delta);
	length = sqrt((double)delta[0] * delta[0] + (double)delta[1] * delta[1] + (double)delta[2] * delta[2]);
	if (length < le->radius)
	{
		CG_FreeLocalEntity(le); return;
	}
	trap_R_AddRefEntityToScene(re);
}



/* Original 30047090 / Linux CG_AddMoveGravityFadeRGB, local type 17. */
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
static int CG_LocalEffectTimeDifference(int a, int b)
{
	unsigned bits = (unsigned)a - (unsigned)b;
	int      value;
	memcpy(&value, &bits, sizeof(value));
	return value;
}
#endif

static void CG_AddMoveGravityFadeRGB(localEntity_t *le)
{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	refEntity_t    *re = &le->refEntity;
	long double    remaining, colorFraction, movement, value, positionZ, elapsed, gravity, length;
	vec3_t         delta;
	int            remainingTime, numerator, denominator, i;
	unsigned       numeratorBits;
	volatile float *storedZ = &re->origin[2];
	remainingTime = CG_LocalEffectTimeDifference(le->endTime, cg.time);
	remaining     = (long double)remainingTime * (long double)le->lifeRate;
	if (cg.time > le->fadeInTime)
	{
		numeratorBits = ((unsigned)remainingTime << 8) - (unsigned)remainingTime;
		memcpy(&numerator, &numeratorBits, sizeof(numerator));
		denominator = CG_LocalEffectTimeDifference(le->endTime, le->fadeInTime);
#if defined(_WIN32)
		colorFraction = (long double)numerator / (long double)denominator;
#else
		colorFraction  = 1.0L / (long double)denominator;
		colorFraction *= (long double)numerator;
#endif
		for (i = 0; i < 4; ++i)
		{
			value = colorFraction * (long double)le->color[i];
#if defined(_WIN32)
			re->shaderRGBA[i] = (byte)(unsigned)TCE_TruncateNativeInteger(value);
#else
			if (value > -32769.0L && value < 32768.0L)
			{
				re->shaderRGBA[i] = (byte)(unsigned)(int)value;
			}
			else
			{
				re->shaderRGBA[i] = 0;
			}
#endif
		}
	}
	movement = 1.0L - remaining;
#if defined(_WIN32)
	value = (long double)le->pos.trDelta[0] * movement;
#else
	value = movement * (long double)le->pos.trDelta[0];
#endif
	re->origin[0] = (float)(value + (long double)le->pos.trBase[0]);
#if defined(_WIN32)
	value = (long double)le->pos.trDelta[1] * movement;
#else
	value = movement * (long double)le->pos.trDelta[1];
#endif
	re->origin[1] = (float)(value + (long double)le->pos.trBase[1]);
	value         = movement * (long double)le->pos.trDelta[2];
	positionZ     = value + (long double)le->pos.trBase[2];
#if !defined(_WIN32)
	{
		volatile float roundedZ = (float)positionZ;
		positionZ = (long double)roundedZ;
	}
#endif
	*storedZ = (float)positionZ;
	elapsed  = (long double)CG_LocalEffectTimeDifference(cg.time, le->startTime);
#if defined(_WIN32)
	gravity  = elapsed * (long double)le->tceGravity;
	gravity *= elapsed;
#else
	gravity = (long double)le->tceGravity * elapsed;
	gravity = elapsed * gravity;
#endif
	gravity *= (long double)0.0004f;
	*storedZ = (float)(positionZ - gravity);
	if (le->leFlags & 2)
	{
		value        = remaining * (long double)le->angles.trDelta[0];
		re->rotation = (float)(value + (long double)le->angles.trBase[0]);
	}
	delta[0] = (float)((long double)re->origin[0] - (long double)cg.refdef.vieworg[0]);
	delta[1] = (float)((long double)re->origin[1] - (long double)cg.refdef.vieworg[1]);
	delta[2] = (float)((long double)re->origin[2] - (long double)cg.refdef.vieworg[2]);
	length   = TCE_VectorLengthValue(delta);
	if (!(length >= (long double)le->radius))
	{
		CG_FreeLocalEntity(le);
		return;
	}
	trap_R_AddRefEntityToScene(re);
#else


	refEntity_t *re = &le->refEntity;
	vec3_t      delta;
	double      remaining = (double)(le->endTime - cg.time) * le->lifeRate;
	double      elapsed   = (float)(cg.time - le->startTime);
	int         i;
	if (le->fadeInTime < cg.time)
	{
		for (i = 0; i < 4; i++)
			re->shaderRGBA[i] = (byte)(int)((double)(le->endTime - cg.time) / (le->endTime - le->fadeInTime) * le->color[i] * 255.0);
	}
	for (i = 0; i < 3; i++)
		re->origin[i] = (float)((1.0 - remaining) * le->pos.trDelta[i] + le->pos.trBase[i]);
	re->origin[2] = (float)(re->origin[2] - elapsed * le->tceGravity * elapsed * .0004f);
	if (le->leFlags & 2)
	{
		re->rotation = (float)(remaining * le->angles.trDelta[0] + le->angles.trBase[0]);
	}
	VectorSubtract(re->origin, cg.refdef.vieworg, delta);
	if (VectorLength(delta) < le->radius)
	{
		CG_FreeLocalEntity(le); return;
	}
	trap_R_AddRefEntityToScene(re);
#endif
}



/* Original 30046d80 / Linux CG_AddMoveScaleFadeRGB, local type 18. */
static void CG_AddMoveScaleFadeRGB(localEntity_t *le)
{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	refEntity_t *re = &le->refEntity;
	long double fraction, value, length;
	vec3_t      delta;
	float       gravity;
	int         numerator, denominator, i;
	if (le->fadeInTime > le->startTime && cg.time < le->fadeInTime)
	{
		numerator   = CG_LocalEffectTimeDifference(le->fadeInTime, cg.time);
		denominator = CG_LocalEffectTimeDifference(le->fadeInTime, le->startTime);
#if defined(_WIN32)
		fraction = (long double)numerator / (long double)denominator;
		fraction = 1.0L - fraction;
#else
		{
			volatile float roundedFraction;
			fraction        = 1.0L / (long double)denominator;
			fraction        = (long double)numerator * fraction;
			fraction        = 1.0L - fraction;
			roundedFraction = (float)fraction;
			fraction        = (long double)roundedFraction;
		}
#endif
	}
	else
	{
		numerator = CG_LocalEffectTimeDifference(le->endTime, cg.time);
		fraction  = (long double)numerator * (long double)le->lifeRate;
	}
	for (i = 0; i < 4; ++i)
	{
		value = fraction * (long double)le->color[i];
#if defined(_WIN32)
		value            *= (long double)255.0f;
		re->shaderRGBA[i] = (byte)(unsigned)TCE_TruncateNativeInteger(value);
#else
		if (i == 3)
		{
			value = (long double)255.0f * value;
		}
		else
		{
			value *= (long double)255.0f;
		}
		if (value > -32769.0L && value < 32768.0L)
		{
			re->shaderRGBA[i] = (byte)(unsigned)(int)value;
		}
		else
		{
			re->shaderRGBA[i] = 0;
		}
#endif
	}
	if (le->leFlags & 2)
	{
#if defined(_WIN32)
		value = fraction * (long double)le->angles.trDelta[0];
#else
		value = (long double)le->angles.trDelta[0] * fraction;
#endif
		re->rotation = (float)(value + (long double)le->angles.trBase[0]);
	}
	if (le->leFlags & 0x40)
	{
		value      = 1.0L - fraction;
		value     += 1.0L;
		value     *= (long double)le->radius;
		value     *= 0.5L;
		re->radius = (float)value;
	}
	else if (le->leFlags & 0x20)
	{
		value  = 1.0L - fraction;
		value *= (long double)le->radius;
#if defined(_WIN32)
		re->radius = (float)(value + 1.0L);
#else
		re->radius = (float)(1.0L + value);
#endif
	}
	else if (le->leFlags & 0x100)
	{
		re->renderfx = 8;
	}
	else if (!(le->leFlags & 1))
	{
		value      = 1.0L - fraction;
		value     *= (long double)le->radius;
		re->radius = (float)(value + 8.0L);
	}
	gravity = le->tceGravity;
	if (gravity == 0.0f || gravity != gravity)
	{
		gravity = 1.0f;
	}
	TCE_BG_EvaluateTrajectory(&le->pos, cg.time, re->origin, qfalse, -1, gravity);
	delta[0] = (float)((long double)re->origin[0] - (long double)cg.refdef.vieworg[0]);
	delta[1] = (float)((long double)re->origin[1] - (long double)cg.refdef.vieworg[1]);
	delta[2] = (float)((long double)re->origin[2] - (long double)cg.refdef.vieworg[2]);
	length   = TCE_VectorLengthValue(delta);
	if (!(length >= (long double)le->radius))
	{
		CG_FreeLocalEntity(le);
		return;
	}
	trap_R_AddRefEntityToScene(re);
#else

	refEntity_t *re = &le->refEntity;
	vec3_t      delta;
	double      fraction;
	int         i;
	if (le->fadeInTime > le->startTime && cg.time < le->fadeInTime)
	{
		fraction = 1.0 - (double)(le->fadeInTime - cg.time) / (le->fadeInTime - le->startTime);
	}
	else
	{
		fraction = (double)(le->endTime - cg.time) * le->lifeRate;
	}
	for (i = 0; i < 4; i++)
		re->shaderRGBA[i] = (byte)(int)(fraction * le->color[i] * 255.0);
	if (le->leFlags & 2)
	{
		re->rotation = (float)(fraction * le->angles.trDelta[0] + le->angles.trBase[0]);
	}
	if (le->leFlags & 0x40)
	{
		re->radius = (float)((2.0 - fraction) * le->radius * .5);
	}
	else if (le->leFlags & 0x20)
	{
		re->radius = (float)((1.0 - fraction) * le->radius + 1.0);
	}
	else if (le->leFlags & 0x100)
	{
		re->renderfx = 8;
	}
	else if (!(le->leFlags & 1))
	{
		re->radius = (float)((1.0 - fraction) * le->radius + 8.0);
	}
	TCE_BG_EvaluateTrajectory(&le->pos, cg.time, re->origin, qfalse, -1, le->tceGravity?le->tceGravity:1.0f);
	VectorSubtract(re->origin, cg.refdef.vieworg, delta);
	if (VectorLength(delta) < le->radius)
	{
		CG_FreeLocalEntity(le); return;
	}
	trap_R_AddRefEntityToScene(re);
#endif
}

/* Original 30046bf0 / Linux 00080d1a, type19. The original retains these
   diagnostic prints; do not substitute a sprite fade for the model axis. */
static void CG_AddShrinkWiden(localEntity_t *le)
{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	refEntity_t    *re = &le->refEntity;
	vec3_t         angles, delta;
	volatile float fraction, envelope;
	long double    value, scale, productY, productZ, length;
	int            numerator, denominator;
	if (le->fadeInTime > le->startTime && cg.time < le->fadeInTime)
	{
		numerator   = CG_LocalEffectTimeDifference(le->fadeInTime, cg.time);
		denominator = CG_LocalEffectTimeDifference(le->fadeInTime, le->startTime);
#if defined(_WIN32)
		value = (long double)numerator / (long double)denominator;
#else
		value = 1.0L / (long double)denominator;
		value = (long double)numerator * value;
#endif
		fraction = (float)(1.0L - value);
	}
	else
	{
		numerator = CG_LocalEffectTimeDifference(le->endTime, cg.time);
#if !defined(_WIN32)
		{
			volatile float integerSpill = (float)(long double)numerator;
		}
#endif
		value    = (long double)numerator * (long double)le->lifeRate;
		fraction = (float)value;
	}
	CG_Printf("here we are reType %i\n", re->reType);
	if ((le->leFlags & 0x400) && re->reType == RT_MODEL && fraction > 0.0f)
	{
		value = 1.0L - (long double)fraction;
#if !defined(_WIN32)
		{
			volatile float roundedDifference = (float)value;
			value = (long double)roundedDifference;
		}
#endif
		VectorCopy(cg.predictedPlayerState.viewangles, angles);
		value    = value * value;
		envelope = (float)(1.0L - value);
		CG_Printf("here we are oriented, pitch %f\n", angles[0]);
#if defined(_WIN32)
		value = (long double)angles[0] * (long double)0.33f;
#else
		value = (long double)0.33f * (long double)angles[0];
#endif
		angles[0] = (float)value;
		AnglesToAxis(angles, re->axis);
		scale = (long double)envelope * (long double)le->radius;
#if defined(_WIN32)
		re->axis[2][0] = (float)(scale * (long double)re->axis[2][0]);
		re->axis[2][1] = (float)(scale * (long double)re->axis[2][1]);
		re->axis[2][2] = (float)(scale * (long double)re->axis[2][2]);
#else
		re->axis[2][0] = (float)((long double)re->axis[2][0] * scale);
		productY       = (long double)re->axis[2][1] * scale;
		productZ       = scale * (long double)re->axis[2][2];
		re->axis[2][1] = (float)productY;
		re->axis[2][2] = (float)productZ;
#endif
	}
	delta[0] = (float)((long double)re->origin[0] - (long double)cg.refdef_current->vieworg[0]);
	delta[1] = (float)((long double)re->origin[1] - (long double)cg.refdef_current->vieworg[1]);
	delta[2] = (float)((long double)re->origin[2] - (long double)cg.refdef_current->vieworg[2]);
	length   = TCE_VectorLengthValue(delta);
	if (!(length >= (long double)le->radius))
	{
		CG_FreeLocalEntity(le);
		return;
	}
	trap_R_AddRefEntityToScene(re);
#else


	refEntity_t *re = &le->refEntity;
	vec3_t      angles, delta;
	float       fraction, scale;
	if (le->fadeInTime > le->startTime && cg.time < le->fadeInTime)
	{
		fraction = 1.0f - (float)(le->fadeInTime - cg.time) / (le->fadeInTime - le->startTime);
	}
	else
	{
		fraction = (le->endTime - cg.time) * le->lifeRate;
	}
	CG_Printf("here we are reType %i\n", re->reType);
	if ((le->leFlags & 0x400) && re->reType == RT_MODEL && fraction > 0)
	{
		VectorCopy(cg.predictedPlayerState.viewangles, angles);
		CG_Printf("here we are oriented, pitch %f\n", angles[0]);
		angles[0] *= .33f;
		AnglesToAxis(angles, re->axis);
		scale = (1.0f - (1.0f - fraction) * (1.0f - fraction)) * le->radius;
		VectorScale(re->axis[2], scale, re->axis[2]);
	}
	VectorSubtract(re->origin, cg.refdef_current->vieworg, delta);
	if (VectorLength(delta) < le->radius)
	{
		CG_FreeLocalEntity(le); return;
	}
	trap_R_AddRefEntityToScene(re);
#endif
}

void CG_AddMoveScaleFade(localEntity_t *le)
{
#if (defined(_WIN32) || defined(__linux__)) && !defined(Q3_VM)
	refEntity_t *re = &le->refEntity;
	long double fraction, value, length;
	vec3_t      delta;
	float       gravity;
	int         numerator, denominator;
	if (le->fadeInTime > le->startTime && cg.time < le->fadeInTime)
	{
		numerator   = CG_LocalEffectTimeDifference(le->fadeInTime, cg.time);
		denominator = CG_LocalEffectTimeDifference(le->fadeInTime, le->startTime);
#if defined(_WIN32)
		fraction = (long double)numerator / (long double)denominator;
		fraction = 1.0L - fraction;
#else
		{
			volatile float roundedFraction;
			fraction        = 1.0L / (long double)denominator;
			fraction        = (long double)numerator * fraction;
			fraction        = 1.0L - fraction;
			roundedFraction = (float)fraction;
			fraction        = (long double)roundedFraction;
		}
#endif
	}
	else
	{
		numerator = CG_LocalEffectTimeDifference(le->endTime, cg.time);
		fraction  = (long double)numerator * (long double)le->lifeRate;
	}
	if (!(le->leFlags & 4))
	{
		value  = fraction * (long double)le->color[3];
		value *= (long double)255.0f;
#if defined(_WIN32)
		re->shaderRGBA[3] = (byte)(unsigned)TCE_TruncateNativeInteger(value);
#else
		/* Signed16 integer-indefinite has a zero low byte. */
		if (value > -32769.0L && value < 32768.0L)
		{
			re->shaderRGBA[3] = (byte)(unsigned)(int)value;
		}
		else
		{
			re->shaderRGBA[3] = 0;
		}
#endif
	}
	if (le->leFlags & 2)
	{
#if defined(_WIN32)
		value = fraction * (long double)le->angles.trDelta[0];
#else
		value = (long double)le->angles.trDelta[0] * fraction;
#endif
		re->rotation = (float)(value + (long double)le->angles.trBase[0]);
	}
	if (le->leFlags & 0x40)
	{
		value      = 1.0L - fraction;
		value     += 1.0L;
		value     *= (long double)le->radius;
		value     *= 0.5L;
		re->radius = (float)value;
	}
	else if (le->leFlags & 0x20)
	{
		value  = 1.0L - fraction;
		value *= (long double)le->radius;
#if defined(_WIN32)
		re->radius = (float)(value + 1.0L);
#else
		re->radius = (float)(1.0L + value);
#endif
	}
	else if (le->leFlags & 0x100)
	{
		re->renderfx = 8;
	}
	else if (!(le->leFlags & 1))
	{
		numerator  = CG_LocalEffectTimeDifference(le->endTime, cg.time);
		value      = (long double)numerator * (long double)le->lifeRate;
		value      = 1.0L - value;
		value     *= (long double)le->radius;
		re->radius = (float)(value + 8.0L);
	}
	gravity = le->tceGravity;
	if (gravity == 0.0f || gravity != gravity)
	{
		gravity = 1.0f;
	}
	TCE_BG_EvaluateTrajectory(&le->pos, cg.time, re->origin, qfalse, -1, gravity);
	delta[0] = (float)((long double)re->origin[0] - (long double)cg.refdef_current->vieworg[0]);
	delta[1] = (float)((long double)re->origin[1] - (long double)cg.refdef_current->vieworg[1]);
	delta[2] = (float)((long double)re->origin[2] - (long double)cg.refdef_current->vieworg[2]);
	length   = TCE_VectorLengthValue(delta);
	if (!(length >= (long double)le->radius))
	{
		CG_FreeLocalEntity(le);
		return;
	}
	trap_R_AddRefEntityToScene(re);
#else

	refEntity_t *re = &le->refEntity;
	vec3_t      delta;
	double      fraction, length;
	if (le->fadeInTime > le->startTime && cg.time < le->fadeInTime)
	{
		fraction = 1.0 - (double)(le->fadeInTime - cg.time) / (le->fadeInTime - le->startTime);
	}
	else
	{
		fraction = (double)(le->endTime - cg.time) * le->lifeRate;
	}
	if (!(le->leFlags & 4))
	{
		re->shaderRGBA[3] = (byte)(int)(255.0 * fraction * le->color[3]);
	}
	if (le->leFlags & 2)
	{
		re->rotation = (float)(fraction * le->angles.trDelta[0] + le->angles.trBase[0]);
	}
	if (le->leFlags & 0x40)
	{
		re->radius = (float)((2.0 - fraction) * le->radius * 0.5);
	}
	else if (le->leFlags & 0x20)
	{
		re->radius = (float)((1.0 - fraction) * le->radius + 1.0);
	}
	else if (le->leFlags & 0x100)
	{
		re->renderfx = 8;
	}
	else if (!(le->leFlags & 1))
	{
		re->radius = (float)((1.0 - (double)(le->endTime - cg.time) * le->lifeRate) * le->radius + 8.0);
	}
	TCE_BG_EvaluateTrajectory(&le->pos, cg.time, re->origin, qfalse, -1, le->tceGravity?le->tceGravity:1.0f);
	VectorSubtract(re->origin, cg.refdef_current->vieworg, delta);
	length = sqrt((double)delta[0] * delta[0] + (double)delta[1] * delta[1] + (double)delta[2] * delta[2]);
	if (length < le->radius)
	{
		CG_FreeLocalEntity(le); return;
	}
	trap_R_AddRefEntityToScene(re);
#endif
}


/*
===================
CG_AddScaleFade

For rocket smokes that hang in place, fade out, and are
removed if the view passes through them.
There are often many of these, so it needs to be simple.
===================
*/
static void CG_AddScaleFade(localEntity_t *le)
{
	refEntity_t *re;
	double      c;
	vec3_t      delta;
	float       len;

	re = &le->refEntity;

	// fade / grow time
	c = (double)(le->endTime - cg.time) * le->lifeRate;

	re->shaderRGBA[3] = (c * le->color[3]) * 255.0;
	if (!(le->leFlags & LEF_PUFF_DONT_SCALE))
	{
		re->radius = le->radius * (1.0 - c) + 8;
	}

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract(re->origin, cg.refdef_current->vieworg, delta);
	len = VectorLength(delta);
	if (len < le->radius)
	{
		CG_FreeLocalEntity(le);
		return;
	}

	trap_R_AddRefEntityToScene(re);
}


/*
=================
CG_AddFallScaleFade

This is just an optimized CG_AddMoveScaleFade
For blood mists that drift down, fade out, and are
removed if the view passes through them.
There are often 100+ of these, so it needs to be simple.
=================
*/
static void CG_AddFallScaleFade(localEntity_t *le)
{
	refEntity_t *re;
	double      c;
	vec3_t      delta;
	float       len;

	re = &le->refEntity;

	// fade time
	c = (double)(le->endTime - cg.time) * le->lifeRate;

	re->shaderRGBA[3] = (c * le->color[3]) * 255.0;

	re->origin[2] = le->pos.trBase[2] - (1.0 - c) * le->pos.trDelta[2];

	re->radius = le->radius * (1.0 - c) + 16;

	// if the view would be "inside" the sprite, kill the sprite
	// so it doesn't add too much overdraw
	VectorSubtract(re->origin, cg.refdef_current->vieworg, delta);
	len = VectorLength(delta);
	if (len < le->radius)
	{
		CG_FreeLocalEntity(le);
		return;
	}

	trap_R_AddRefEntityToScene(re);
}



/*
================
CG_AddExplosion
================
*/
static void CG_AddExplosion(localEntity_t *ex)
{
	refEntity_t *ent;

	ent = &ex->refEntity;

	// add the entity
	// RF, don't add if shader is invalid
	if (ent->customShader >= 0)
	{
		trap_R_AddRefEntityToScene(ent);
	}

	// add the dlight
	if (ex->light || 1)
	{
		float light;

		light = (float)(cg.time - ex->startTime) / (ex->endTime - ex->startTime);
		if (light < 0.5)
		{
			light = 1.0;
		}
		else
		{
			light = 1.0 - (light - 0.5) * 2;
		}
		light = ex->light * light;
		//%	trap_R_AddLightToScene(ent->origin, light, ex->lightColor[0], ex->lightColor[1], ex->lightColor[2], 0 );
		trap_R_AddLightToScene(ent->origin, 512, light, ex->lightColor[0], ex->lightColor[1], ex->lightColor[2], 0, 0);
	}
}

/*
================
CG_AddSpriteExplosion
================
*/
static void CG_AddSpriteExplosion(localEntity_t *le)
{
	refEntity_t re;
	double      c;

	re = le->refEntity;

	c = (double)(le->endTime - cg.time) / (le->endTime - le->startTime);
	if (c > 1)
	{
		c = 1.0;    // can happen during connection problems
	}

	re.shaderRGBA[0] = 0xff;
	re.shaderRGBA[1] = 0xff;
	re.shaderRGBA[2] = 0xff;
	re.shaderRGBA[3] = 0xff * c * 0.33;

	re.reType = RT_SPRITE;
	re.radius = 42 * (1.0 - c) + 30;

	// Ridah, move away from surface
	VectorMA(le->pos.trBase, (1.0 - c), le->pos.trDelta, re.origin);
	// done.

	// RF, don't add if shader is invalid
	if (re.customShader >= 0)
	{
		trap_R_AddRefEntityToScene(&re);
	}

	// add the dlight
	if (le->light || 1)
	{
		float light;

		// Ridah, modified this so the light fades out rather than shrinking
		/*
		light = (float)( cg.time - le->startTime ) / ( le->endTime - le->startTime );
		if ( light < 0.5 ) {
			light = 1.0;
		} else {
			light = 1.0 - ( light - 0.5 ) * 2;
		}
		light = le->light * light;
		trap_R_AddLightToScene(re.origin, light, le->lightColor[0], le->lightColor[1], le->lightColor[2], 0 );
		*/
		light = (float)(cg.time - le->startTime) / (le->endTime - le->startTime);
		if (light < 0.5)
		{
			light = 1.0;
		}
		else
		{
			light = 1.0 - (light - 0.5) * 2;
		}
		//%	trap_R_AddLightToScene(re.origin, le->light, light*le->lightColor[0], light*le->lightColor[1], light*le->lightColor[2], 0 );
		trap_R_AddLightToScene(re.origin, 320, light, le->lightColor[0], le->lightColor[1], le->lightColor[2], 0, 0);
		// done.
	}
}


//==============================================================================

/*
===================
CG_AddLocalEntities

===================
*/
void CG_AddLocalEntities(void)
{
	localEntity_t *le, *next;

	// walk the list backwards, so any new local entities generated
	// (trails, marks, etc) will be present this frame
	le = cg_activeLocalEntities.prev;
	for ( ; le != &cg_activeLocalEntities ; le = next )
	{
		// grab next now, so if the local entity is freed we
		// still have it
		next = le->prev;

		if (cg.time >= le->endTime)
		{
			CG_FreeLocalEntity(le);
			continue;
		}
		switch (le->leType)
		{
		case TCE_LE_FLAT_SPARK: TCE_CG_AddFlatSpark(le); break;
		case TCE_LE_GLOW_SPARK: TCE_CG_AddGlowSpark(le); break;
		default:
			CG_Error("Bad leType: %i", le->leType);
			break;

		// Ridah
		case LE_MOVING_TRACER:
			CG_AddMovingTracer(le);
			break;
		case LE_SPARK:
			CG_AddSparkElements(le);
			break;
		case LE_FUSE_SPARK:
			CG_AddFuseSparkElements(le);
			break;
		case LE_DEBRIS:
			CG_AddDebrisElements(le);
			break;
		case LE_BLOOD:
			CG_AddBloodElements(le);
			break;
        /*		case LE_ZOMBIE_SPIRIT:
				case LE_ZOMBIE_BAT:
					CG_AddClientCritter( le );
					break;*/
		// done.

		case LE_MARK:
			break;

		case LE_SPRITE_EXPLOSION:
			CG_AddSpriteExplosion(le);
			break;

		case LE_EXPLOSION:
			CG_AddExplosion(le);
			break;

		case LE_FRAGMENT:
			CG_AddFragment(le);
			break;

		case 19:
			CG_AddShrinkWiden(le);
			break;
		case 17:
			CG_AddMoveGravityFadeRGB(le);
			break;
		case 18:
			CG_AddMoveScaleFadeRGB(le);
			break;
		case 16: /* TC:E CG_ParticleTest chips */
			CG_AddMoveGravityFade(le);
			break;

		case LE_MOVE_SCALE_FADE:        // water bubbles
			CG_AddMoveScaleFade(le);
			break;

		case LE_FADE_RGB:               // teleporters, railtrails
			CG_AddFadeRGB(le);
			break;

		case LE_FALL_SCALE_FADE: // gib blood trails
			CG_AddFallScaleFade(le);
			break;

		case LE_SCALE_FADE:     // rocket trails
			CG_AddScaleFade(le);
			break;

		case LE_EMITTER:
			CG_AddEmitter(le);
			break;

		}
	}
}
