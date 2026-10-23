#include "cg_local.h"
#include "tce_add_fragment.h"
#if defined(__linux__) && !defined(Q3_VM)
#include "tce_smoke_grenade.h"
#include "../game/tce_numeric.h"
#endif

void CG_Explodef(vec3_t, vec3_t, int, int, qhandle_t, int, qhandle_t);
void CG_FreeLocalEntity(localEntity_t *le);
#if !defined(__linux__) || defined(Q3_VM)
static int difference(int a, int b)
{
	return (int)((unsigned)a - (unsigned)b);
}

#endif

static void fragmentFlame(localEntity_t *le, float alpha, vec3_t direction)
{
	refEntity_t backup = le->refEntity;
#if defined(__linux__) && !defined(Q3_VM)
	long double value = (long double)alpha * 255.0f;
	le->refEntity.shaderRGBA[3] = value > -32769.0L && value < 32768.0L ? (byte)(int)value : 0;
#else
	le->refEntity.shaderRGBA[3] = (byte)(int)(255.0 * alpha);
#endif
	VectorCopy(direction, le->refEntity.fireRiseDir);
	le->refEntity.customShader = cgs.media.onFireShader;
	trap_R_AddRefEntityToScene(&le->refEntity);
	le->refEntity.customShader = cgs.media.onFireShader2;
	trap_R_AddRefEntityToScene(&le->refEntity);
	le->refEntity = backup;
}

void TCE_CG_AddFragment(localEntity_t *le, const tce_fragmentContext_t *context)
{
#if defined(__linux__) && !defined(Q3_VM)
	refEntity_t *re = &le->refEntity;
	vec3_t newOrigin, flameDir;
	trace_t trace;
	float flameAlpha = 0.0f, gravity;
	qboolean hasFlame = qfalse;
	int i, j;

	if (!re->fadeStartTime || re->fadeEndTime < le->endTime)
	{
		re->fadeStartTime = TCE_SubInt32(le->endTime,
		                               TCE_SubInt32(le->endTime, cg.time) > 5000 ? 5000 : 1000);
		re->fadeEndTime = le->endTime;
	}
	if (le->onFireStart && le->onFireStart < cg.time && cg.time < le->onFireEnd)
	{
		hasFlame = qtrue;
		flameAlpha = (float)(1.0L - (1.0L / TCE_SubInt32(le->onFireEnd, le->onFireStart))
		                    * TCE_SubInt32(cg.time, le->onFireStart));
		if (flameAlpha < 0.0f)
		{
			flameAlpha = 0.0f;
		}
		if (!(flameAlpha <= 1.0f))
		{
			flameAlpha = 1.0f;
		}
		trap_S_AddLoopingSound(re->origin, vec3_origin, cgs.media.flameCrackSound,
		                       TCE_TruncateNativeInteger((long double)flameAlpha * 20.0f), 0);
	}
	if (le->leFlags & 0x200)
	{
		if (cg.time < le->onFireEnd)
		{
			long double remaining = TCE_SubInt32(le->onFireEnd, cg.time);
			le->sizeScale = tceSmokeNewBBox
			    ? (float)(remaining * 0.0004000000189989805f + 0.6f)
			    : (float)(remaining * 0.00025f + 0.75f);
		}
		else
		{
			le->sizeScale = tceSmokeNewBBox ? 0.75f : 0.6f;
			le->leFlags &= ~0x200;
		}
	}
	if ((le->leFlags & LEF_SMOKING) && rand() % 5 == 0)
	{
		refEntity_t flash;
		float alpha = (float)(1.0L - (1.0L / TCE_SubInt32(le->endTime, le->startTime))
		                      * TCE_SubInt32(cg.time, le->startTime));
		alpha = (float)((long double)alpha * 0.25f);
		memset(&flash, 0, sizeof(flash));
		CG_PositionEntityOnTag(&flash, re, "tag_flash", 0, NULL);
		CG_ParticleImpactSmokePuffExtended(cgs.media.smokeParticleShader,
		                                   flash.origin, 1000, 8, 20, 20, alpha, 8);
	}
	if (le->pos.trType == TR_STATIONARY)
	{
		if (hasFlame)
		{
			VectorSet(flameDir, 0, 0, 1);
			fragmentFlame(le, flameAlpha, flameDir);
		}
		trap_R_AddRefEntityToScene(re);
		return;
	}
	if (le->pos.trType == TR_GRAVITY_PAUSED)
	{
		if (hasFlame)
		{
			VectorSet(flameDir, 0, 0, 1);
			fragmentFlame(le, flameAlpha, flameDir);
		}
		trap_R_AddRefEntityToScene(re);
		VectorCopy(re->origin, newOrigin);
		newOrigin[2] -= 5.0f;
		CG_Trace(&trace, re->origin, NULL, NULL, newOrigin, -1, 0x10081);
		if (trace.fraction != 1.0f && !TCE_FloatIsNaN(trace.fraction))
		{
			return;
		}
		le->pos.trType = TR_GRAVITY;
		VectorClear(le->pos.trDelta);
		VectorClear(le->angles.trDelta);
	}
	gravity = le->tceGravity;
	if (TCE_FloatIsZeroOrNaN(gravity))
	{
		gravity = 1.0f;
	}
	context->evaluateTrajectory(&le->pos, cg.time, newOrigin, qfalse, -1, gravity);
	if (hasFlame)
	{
		long double length;
		sfxHandle_t sound;
		VectorSubtract(re->origin, newOrigin, flameDir);
		length = TCE_VectorLengthSquaredValue(flameDir);
		if (length == 0.0L || length != length)
		{
			flameDir[2] = 1.0f;
			sound = cgs.media.flameSound;
		}
		else
		{
			VectorNormalize(flameDir);
			sound = cgs.media.flameBlowSound;
		}
		trap_S_AddLoopingSound(newOrigin, vec3_origin, sound,
		                       TCE_TruncateNativeInteger(76.5f * (long double)flameAlpha), 0);
	}
	CG_Trace(&trace, re->origin, NULL, NULL, newOrigin, -1, CONTENTS_SOLID);
	if (trace.fraction == 1.0f || TCE_FloatIsNaN(trace.fraction))
	{
		VectorCopy(newOrigin, re->origin);
		if ((le->leFlags & LEF_TUMBLE) || le->angles.trType == TR_LINEAR)
		{
			vec3_t angles;
			context->evaluateTrajectory(&le->angles, cg.time, angles, qtrue, -1, 1.0f);
			AnglesToAxis(angles, re->axis);
		}
		else
		{
			AnglesToAxis(le->angles.trBase, re->axis);
		}
		if (!TCE_FloatIsZeroOrNaN(le->sizeScale) && le->sizeScale != 1.0f)
		{
			float scale = le->sizeScale;
			for (i = 0; i < 3; ++i)
			{
				for (j = 0; j < 3; ++j)
				{
					re->axis[i][j] = (float)((long double)re->axis[i][j] * scale);
				}
			}
			re->nonNormalizedAxes = qtrue;
		}
		if (hasFlame)
		{
			fragmentFlame(le, flameAlpha, flameDir);
		}
		trap_R_AddRefEntityToScene(re);
		if (le->leBounceSoundType == LEBS_BLOOD)
		{
			CG_BloodTrail(le);
		}
		return;
	}
	if (CG_PointContents(trace.endpos, 0) & CONTENTS_NODROP)
	{
		CG_FreeLocalEntity(le);
		return;
	}
	context->bounceSound(le, &trace);
	CG_ReflectVelocity(le, &trace);
	if (le->leFlags & LEF_TUMBLE_SLOW)
	{
		for (i = 0; i < 3; ++i)
		{
			le->angles.trDelta[i] = (float)((long double)(double)0.8 * le->angles.trDelta[i]);
		}
	}
	if (le->breakCount)
	{
		if (le->leFlags & LEF_TUMBLE_SLOW)
		{
			vec3_t origin, dir;
			float size = (float)((long double)(double)0.8 * le->sizeScale);
			qhandle_t shader;
			if (!((long double)size >= (double)0.7))
			{
				size = 0.7f;
			}
			VectorNormalize2(le->pos.trDelta, dir);
			for (i = 0; i < 3; ++i)
			{
				origin[i] = (float)(((long double)size * dir[i]) * 4.0f + trace.endpos[i]);
			}
			for (i = 0; i < 3; ++i)
			{
				float base = le->pos.trDelta[i];
				int index = rand() % NUMVERTEXNORMALS;
				float jitter = bytedirs[index][i];
				long double length = TCE_VectorLengthValue(le->pos.trDelta);
				dir[i] = (float)((length * jitter) * (double)0.3 + base);
			}
			shader = trap_R_GetShaderFromModel(re->hModel, 0, 0);
			CG_Explodef(origin, dir, TCE_TruncateNativeInteger((long double)size * 50.0f),
			            0, 0, qfalse, shader);
			CG_FreeLocalEntity(le);
		}
		return;
	}
	if (le->pos.trType == TR_STATIONARY && le->leMarkType == LEMT_BLOOD)
	{
		CG_FragmentBounceMark(le, &trace);
	}
	if (hasFlame)
	{
		fragmentFlame(le, flameAlpha, flameDir);
	}
	trap_R_AddRefEntityToScene(re);
#else
	refEntity_t *re = &le->refEntity;
	vec3_t      newOrigin, flameDir;
	trace_t     trace;
	float       flameAlpha = 0;
	qboolean    hasFlame   = qfalse;
	int         i;
	if (!re->fadeStartTime || re->fadeEndTime < le->endTime)
	{
		re->fadeStartTime = difference(le->endTime,
		                               difference(le->endTime, cg.time) > 5000 ? 5000 : 1000);
		re->fadeEndTime = le->endTime;
	}
	if (le->onFireStart && le->onFireStart < cg.time && cg.time < le->onFireEnd)
	{
		hasFlame   = qtrue;
		flameAlpha = (float)(1.0 - (double)difference(cg.time, le->onFireStart) /
		                     difference(le->onFireEnd, le->onFireStart));
		if (flameAlpha < 0)
		{
			flameAlpha = 0;
		}
		if (flameAlpha > 1)
		{
			flameAlpha = 1;
		}
		trap_S_AddLoopingSound(re->origin, vec3_origin, cgs.media.flameCrackSound,
		                       (int)(20.0 * flameAlpha), 0);
	}
	if (le->leFlags & 0x200)
	{
		if (cg.time < le->onFireEnd)
		{
			le->sizeScale = context->sizeVariant
			    ? (float)(difference(le->onFireEnd, cg.time) * (double).0004f + .6f)
			    : (float)(difference(le->onFireEnd, cg.time) * (double).00025f + .75f);
		}
		else
		{
			le->sizeScale = context->sizeVariant ? .75f : .6f;
			le->leFlags  &= ~0x200;
		}
	}
	if ((le->leFlags & LEF_SMOKING) && rand() % 5 == 0)
	{
		refEntity_t flash;
		float       alpha = (float)((1.0 - (double)difference(cg.time, le->startTime) /
		                             difference(le->endTime, le->startTime)) * .25f);
		memset(&flash, 0, sizeof(flash));
		CG_PositionEntityOnTag(&flash, re, "tag_flash", 0, NULL);
		CG_ParticleImpactSmokePuffExtended(cgs.media.smokeParticleShader,
		                                   flash.origin, 1000, 8, 20, 20, alpha, 8);
	}
	if (le->pos.trType == TR_STATIONARY || le->pos.trType == TR_GRAVITY_PAUSED)
	{
		if (hasFlame)
		{
			VectorSet(flameDir, 0, 0, 1);
			fragmentFlame(le, flameAlpha, flameDir);
		}
		trap_R_AddRefEntityToScene(re);
		if (le->pos.trType == TR_STATIONARY)
		{
			return;
		}
		VectorCopy(re->origin, newOrigin); newOrigin[2] -= 5;
		CG_Trace(&trace, re->origin, NULL, NULL, newOrigin, -1, 0x10081);
		if (trace.fraction != 1)
		{
			return;
		}
		VectorClear(le->pos.trDelta); VectorClear(le->angles.trDelta);
		le->pos.trType = TR_GRAVITY;
	}
	context->evaluateTrajectory(&le->pos, cg.time, newOrigin, qfalse, -1,
	                            context->gravityScale ? context->gravityScale : 1);
	if (hasFlame)
	{
		VectorSubtract(re->origin, newOrigin, flameDir);
		if (VectorLengthSquared(flameDir) == 0)
		{
			flameDir[2] = 1;
			trap_S_AddLoopingSound(newOrigin, vec3_origin, cgs.media.flameSound,
			                       (int)(76.5 * flameAlpha), 0);
		}
		else
		{
			VectorNormalize(flameDir);
			trap_S_AddLoopingSound(newOrigin, vec3_origin, cgs.media.flameBlowSound,
			                       (int)(76.5 * flameAlpha), 0);
		}
	}
	CG_Trace(&trace, re->origin, NULL, NULL, newOrigin, -1, CONTENTS_SOLID);
	if (trace.fraction == 1)
	{
		VectorCopy(newOrigin, re->origin);
		if ((le->leFlags & LEF_TUMBLE) || le->angles.trType == TR_LINEAR)
		{
			vec3_t angles;
			context->evaluateTrajectory(&le->angles, cg.time, angles, qtrue, -1, 1);
			AnglesToAxis(angles, re->axis);
		}
		else
		{
			AnglesToAxis(le->angles.trBase, re->axis);
		}
		if (le->sizeScale && le->sizeScale != 1)
		{
			for (i = 0; i < 3; ++i)
				VectorScale(re->axis[i], le->sizeScale, re->axis[i]);
			re->nonNormalizedAxes = qtrue;
		}
		if (hasFlame)
		{
			fragmentFlame(le, flameAlpha, flameDir);
		}
		trap_R_AddRefEntityToScene(re);
		if (le->leBounceSoundType == LEBS_BLOOD)
		{
			CG_BloodTrail(le);
		}
		return;
	}
	if (CG_PointContents(trace.endpos, 0) & CONTENTS_NODROP)
	{
		CG_FreeLocalEntity(le); return;
	}
	context->bounceSound(le, &trace);
	CG_ReflectVelocity(le, &trace);
	if (le->leFlags & LEF_TUMBLE_SLOW)
	{
		for (i = 0; i < 3; ++i)
			le->angles.trDelta[i] = (float)(le->angles.trDelta[i] * .8);
	}
	if (le->breakCount)
	{
		if (le->leFlags & LEF_TUMBLE_SLOW)
		{
			vec3_t    origin, dir;
			float     size = (float)(le->sizeScale * .8);
			double    length;
			qhandle_t shader;
			if (le->sizeScale * .8 < .7)
			{
				size = .7f;
			}
			VectorNormalize2(le->pos.trDelta, dir);
			for (i = 0; i < 3; ++i)
				origin[i] = (float)(trace.endpos[i] + dir[i] * (4.0 * size));
			/* The expanded original macro consumes a different rand per axis. */
			for (i = 0; i < 3; ++i)
			{
				length = sqrt((double)le->pos.trDelta[0] * le->pos.trDelta[0] +
				              (double)le->pos.trDelta[1] * le->pos.trDelta[1] +
				              (double)le->pos.trDelta[2] * le->pos.trDelta[2]);
				dir[i] = (float)(le->pos.trDelta[i] + bytedirs[rand() % NUMVERTEXNORMALS][i] * (length * .3));
			}
			shader = trap_R_GetShaderFromModel(re->hModel, 0, 0);
			CG_Explodef(origin, dir, (int)(size * 50.0), 0, 0, qfalse, shader);
			CG_FreeLocalEntity(le);
		}
		return;
	}
	if (le->pos.trType == TR_STATIONARY && le->leMarkType == LEMT_BLOOD)
	{
		CG_FragmentBounceMark(le, &trace);
	}
	if (hasFlame)
	{
		fragmentFlame(le, flameAlpha, flameDir);
	}
	trap_R_AddRefEntityToScene(re);
#endif
}
