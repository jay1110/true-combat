#include "cg_local.h"
#include "tce_impact_sparks.h"
#include "../game/tce_trajectory.h"
#if defined(__linux__) && !defined(Q3_VM)
#include "../game/tce_numeric.h"
#endif
extern void CG_FreeLocalEntity(localEntity_t *);
static double sparkRandom(void)
{
	return (rand() & 32767) * (double)(1.0f / 32767.0f);
}
static void normalizeSpark(vec3_t v)
{
	double d = sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]); int j; if (d)
	{
		for (j = 0; j < 3; j++)
			v[j] = (float)(v[j] / d);
	}
}
void TCE_CG_FlatSparks(vec3_t origin, vec3_t direction, int count, float intensity, qhandle_t shader)
{
#if defined(__linux__) && !defined(Q3_VM)
	int i, j;
	(void)intensity;
	(void)shader;
	for (i = 0; i < count; ++i)
	{
		vec3_t v;
		long double random, dot;
		int lifetime;
		localEntity_t *le = CG_AllocLocalEntity();
		le->leType = (leType_t)TCE_LE_FLAT_SPARK;
		le->startTime = cg.time;
		lifetime = TCE_TruncateNativeInteger(((long double)sparkRandom() + 1.0f) * 100.0f * 0.5f);
		le->endTime = TCE_AddInt32(lifetime, cg.time);
		le->fadeInTime = TCE_SubInt32(le->endTime, 50);
		random = (long double)sparkRandom() + 1.0f;
		le->color[2] = (float)(random * cg.tceSparkIntensity * 0.5f);
		le->color[1] = le->color[2];
		le->color[0] = le->color[2];
		random = (long double)sparkRandom() + 1.0f;
		le->color[2] = (float)(random * le->color[2] * 0.5f);
		VectorCopy(origin, le->refEntity.origin);
		for (j = 0; j < 3; ++j)
		{
			random = (long double)sparkRandom() - 0.5;
			v[j] = (float)(random + random);
		}
		VectorNormalize(v);
		dot = (long double)v[1] * direction[1] + (long double)v[0] * direction[0];
		dot += (long double)v[2] * direction[2];
		if (!(dot >= 0.0L))
		{
			VectorInverse(v);
		}
		for (j = 0; j < 3; ++j)
		{
			random = sparkRandom();
			v[j] = (float)((((random + random) + 1.0f) * 16.0f * v[j]) * (1.0f / 3.0f) + origin[j]);
		}
		VectorCopy(v, le->refEntity.oldorigin);
		le->refEntity.radius = 1.0f;
		le->refEntity.customShader = cgs.media.tceFlatSparkShader;
	}
#else
	int i, j; vec3_t v; localEntity_t *le;
	for (i = 0; i < count; i++)
	{
		le           = CG_AllocLocalEntity(); le->leType = (leType_t)TCE_LE_FLAT_SPARK; le->startTime = cg.time;
		le->endTime  = cg.time - (int)((sparkRandom() + 1) * .5 * -100); le->fadeInTime = le->endTime - 50;
		le->color[0] = le->color[1] = le->color[2] = (float)((sparkRandom() + 1) * intensity * .5);
		le->color[2] = (float)((sparkRandom() + 1) * le->color[2] * .5); VectorCopy(origin, le->refEntity.origin);
		for (j = 0; j < 3; j++)
			v[j] = (float)((sparkRandom() - .5) * 2);
		normalizeSpark(v);
		if ((double)v[0] * direction[0] + (double)v[1] * direction[1] + (double)v[2] * direction[2] < 0)
		{
			VectorInverse(v);
		}
		for (j = 0; j < 3; j++)
			v[j] = (float)((sparkRandom() * 2 + 1) * 5.333333492279053f * v[j] + origin[j]);
		VectorCopy(v, le->refEntity.oldorigin); le->refEntity.radius = 1; le->refEntity.customShader = shader;
	}
#endif
}
void TCE_CG_GlowSparks(vec3_t origin, vec3_t direction, int count, float intensity, qhandle_t shader)
{
#if defined(__linux__) && !defined(Q3_VM)
	int i, j;
	(void)intensity;
	(void)shader;
	for (i = 0; i < count; ++i)
	{
		vec3_t v;
		long double random;
		int lifetime, jitter;
		localEntity_t *le = CG_AllocLocalEntity();
		le->leType = (leType_t)TCE_LE_GLOW_SPARK;
		le->leFlags = 128;
		le->startTime = cg.time;
		lifetime = TCE_TruncateNativeInteger(((long double)sparkRandom() + 1.0f) * 2000.0f * 0.5f);
		le->endTime = TCE_AddInt32(lifetime, cg.time);
		le->fadeInTime = TCE_SubInt32(le->endTime, 1000);
		random = (long double)sparkRandom() + 1.0f;
		le->color[2] = (float)(random * cg.tceSparkIntensity * 0.5f);
		le->color[1] = le->color[2];
		le->color[0] = le->color[2];
		random = (long double)sparkRandom() + 1.0f;
		le->pos.trType = TR_GRAVITY_LOW;
		le->color[2] = (float)(random * le->color[2] * 0.5f);
		jitter = rand() & 15;
		le->pos.trTime = TCE_SubInt32(cg.time, jitter);
		le->bounceFactor = (float)((long double)(rand() & 32767) * 0.000006103702162363334f + 0.3f);
		VectorCopy(origin, le->refEntity.origin);
		VectorCopy(le->refEntity.origin, le->pos.trBase);
		for (j = 0; j < 3; ++j)
		{
			random = (long double)sparkRandom() - 0.5;
			v[j] = (float)(random + random);
		}
		VectorNormalize(v);
		VectorAdd(direction, v, v);
		for (j = 0; j < 3; ++j)
		{
			v[j] = (float)(((long double)sparkRandom() + 2.0f) * v[j] * 20.0f);
		}
		VectorCopy(v, le->pos.trDelta);
		le->refEntity.radius = 0.5f;
		le->refEntity.customShader = cgs.media.tceGlowSparkShader;
	}
#else
	int i, j; vec3_t v; localEntity_t *le;
	for (i = 0; i < count; i++)
	{
		le               = CG_AllocLocalEntity(); le->leType = (leType_t)TCE_LE_GLOW_SPARK; le->leFlags = 128; le->startTime = cg.time;
		le->endTime      = cg.time - (int)((sparkRandom() + 1) * .5 * -2000); le->fadeInTime = le->endTime - 1000;
		le->color[0]     = le->color[1] = le->color[2] = (float)((sparkRandom() + 1) * intensity * .5);
		le->color[2]     = (float)((sparkRandom() + 1) * le->color[2] * .5); le->pos.trType = TR_GRAVITY_LOW; le->pos.trTime = cg.time - (rand() & 15);
		le->bounceFactor = (float)(sparkRandom() * .2f + .3f); VectorCopy(origin, le->refEntity.origin); VectorCopy(origin, le->pos.trBase);
		for (j = 0; j < 3; j++)
			v[j] = (float)((sparkRandom() - .5) * 2);
		normalizeSpark(v); VectorAdd(v, direction, v);
		for (j = 0; j < 3; j++)
			le->pos.trDelta[j] = (float)((sparkRandom() + 2) * 20 * v[j]);
		le->refEntity.radius = .5f; le->refEntity.customShader = shader;
	}
#endif
}
static float sparkFade(localEntity_t *le)
{
#if defined(__linux__) && !defined(Q3_VM)
	float fade = 1.0f;
	if (cg.time > le->fadeInTime)
	{
		float rate = (float)(1.0L / TCE_SubInt32(le->endTime, le->fadeInTime));
		le->lifeRate = rate;
		fade = (float)(1.0L - (long double)TCE_SubInt32(cg.time, le->fadeInTime) * rate);
		if (fade < 0.0f)
		{
			fade = 0.0f;
		}
	}
	return fade;
#else
	float f = 1; if (cg.time > le->fadeInTime)
	{
		double rate = 1.0 / (le->endTime - le->fadeInTime); le->lifeRate = (float)rate; f = (float)(1 - (cg.time - le->fadeInTime) * rate); if (f < 0)
		{
			f = 0;
		}
	}
	return f;
#endif
}
static void sparkQuad(polyVert_t *p, localEntity_t *le, float fade, int glow)
{
#if defined(__linux__) && !defined(Q3_VM)
	int i, j;
	byte color[4];
	float factors[4] = { fade, fade, fade, fade };
	const float st[4][2] = { { 1, 1 }, { 0, 1 }, { 0, 0 }, { 1, 0 } };
	if (glow && (le->leFlags & 128) && cg.time > le->fadeInTime)
	{
		factors[0] = (float)sqrtl(fade);
		factors[2] = (float)((long double)fade * fade);
	}
	for (j = 0; j < 4; ++j)
	{
		long double value = (long double)factors[j] * le->color[j] * 255.0f;
		color[j] = value > -32769.0L && value < 32768.0L ? (byte)(int)value : 0;
	}
	for (i = 0; i < 4; ++i)
	{
		p[i].st[0] = st[i][0];
		p[i].st[1] = st[i][1];
		memcpy(p[i].modulate, color, sizeof(color));
	}
	trap_R_AddPolyToScene(le->refEntity.customShader, 4, p);
#else
	int i, j; float factors[4] = { fade, fade, fade, fade }; const float st[4][2] = { { 1, 1 }, { 0, 1 }, { 0, 0 }, { 1, 0 } }; if (glow && (le->leFlags & 128) && cg.time > le->fadeInTime)
	{
		factors[0] = (float)sqrt(fade); factors[2] = fade * fade;
	}
	for (i = 0; i < 4; i++)
	{
		p[i].st[0] = st[i][0]; p[i].st[1] = st[i][1]; for (j = 0; j < 4; j++)
			p[i].modulate[j] = (byte)(int)((double)factors[j] * le->color[j] * 255);
	}
	trap_R_AddPolyToScene(le->refEntity.customShader, 4, p);
#endif
}
void TCE_CG_AddFlatSpark(localEntity_t *le)
{
#if defined(__linux__) && !defined(Q3_VM)
	polyVert_t p[4];
	vec3_t start, end, a, b, width;
	float fade = sparkFade(le);
	float t = (float)sqrtl((1.0L / TCE_SubInt32(le->endTime, le->startTime))
	                      * TCE_SubInt32(cg.time, le->startTime));
	int j;
	for (j = 0; j < 3; ++j)
	{
		long double delta = (long double)le->refEntity.oldorigin[j] - le->refEntity.origin[j];
		start[j] = (float)(((long double)t * delta) * 0.5f + le->refEntity.origin[j]);
		end[j] = (float)(((long double)t + 0.5f) * delta + le->refEntity.origin[j]);
	}
	VectorSubtract(le->refEntity.origin, cg.refdef.vieworg, a);
	VectorNormalize(a);
	VectorSubtract(le->refEntity.oldorigin, cg.refdef.vieworg, b);
	VectorNormalize(b);
	CrossProduct(a, b, width);
	VectorNormalize(width);
	memset(p, 0, sizeof(p));
	for (j = 0; j < 3; ++j)
	{
		long double positive = (long double)width[j] * le->refEntity.radius;
		long double negative = (long double)width[j] * -(long double)le->refEntity.radius;
		p[0].xyz[j] = (float)(positive + start[j]);
		p[1].xyz[j] = (float)(negative + start[j]);
		p[2].xyz[j] = (float)(negative + end[j]);
		/* Native reuses a FLOAT spill only for the fourth vertex. */
		p[3].xyz[j] = (float)((long double)(float)positive + end[j]);
	}
	sparkQuad(p, le, fade, 0);
#else
	polyVert_t p[4]; vec3_t delta, start, end, a, b, width; float fade = sparkFade(le); double t = sqrt((double)(cg.time - le->startTime) / (le->endTime - le->startTime)); int j;
	VectorSubtract(le->refEntity.oldorigin, le->refEntity.origin, delta);
	for (j = 0; j < 3; j++)
	{
		start[j] = (float)(t * .5 * delta[j] + le->refEntity.origin[j]); end[j] = (float)((t + .5) * delta[j] + le->refEntity.origin[j]);
	}
	VectorSubtract(le->refEntity.origin, cg.refdef.vieworg, a); normalizeSpark(a); VectorSubtract(le->refEntity.oldorigin, cg.refdef.vieworg, b); normalizeSpark(b); CrossProduct(a, b, width); normalizeSpark(width);
	memset(p, 0, sizeof(p)); for (j = 0; j < 3; j++)
	{
		p[0].xyz[j] = (float)((double)le->refEntity.radius * width[j] + start[j]); p[1].xyz[j] = (float)(-(double)le->refEntity.radius * width[j] + start[j]); p[2].xyz[j] = (float)(-(double)le->refEntity.radius * width[j] + end[j]); p[3].xyz[j] = (float)((double)le->refEntity.radius * width[j] + end[j]);
	}
	sparkQuad(p, le, fade, 0);
#endif
}
void TCE_CG_AddGlowSpark(localEntity_t *le)
{
#if defined(__linux__) && !defined(Q3_VM)
	polyVert_t p[4];
	vec3_t end, origin, top;
	trace_t tr;
	float fade;
	int j;
	if (le->pos.trType != TR_STATIONARY)
	{
		TCE_BG_EvaluateTrajectory(&le->pos, cg.time, end, qfalse, -1, 1.0f);
		CG_Trace(&tr, le->refEntity.origin, NULL, NULL, end, -1, CONTENTS_SOLID);
		if (tr.fraction == 1.0f || TCE_FloatIsNaN(tr.fraction))
		{
			VectorCopy(end, le->refEntity.origin);
		}
		else
		{
			if (trap_CM_PointContents(tr.endpos, 0) & CONTENTS_NODROP)
			{
				CG_FreeLocalEntity(le);
				return;
			}
			CG_ReflectVelocity(le, &tr);
		}
	}
	fade = sparkFade(le);
	VectorCopy(le->refEntity.origin, origin);
	for (j = 0; j < 3; ++j)
	{
		long double height = (long double)le->refEntity.radius + le->refEntity.radius;
		top[j] = (float)((long double)cg.refdef.viewaxis[2][j] * height + origin[j]);
	}
	memset(p, 0, sizeof(p));
	for (j = 0; j < 3; ++j)
	{
		long double positive = (long double)cg.refdef.viewaxis[1][j] * le->refEntity.radius;
		long double negative = (long double)cg.refdef.viewaxis[1][j] * -(long double)le->refEntity.radius;
		p[0].xyz[j] = (float)(positive + origin[j]);
		p[1].xyz[j] = (float)(negative + origin[j]);
		p[2].xyz[j] = (float)(negative + top[j]);
		p[3].xyz[j] = (float)(positive + top[j]);
	}
	sparkQuad(p, le, fade, 1);
#else
	polyVert_t p[4]; vec3_t end, top; trace_t tr; float fade; int j;
	if (le->pos.trType != TR_STATIONARY)
	{
		TCE_BG_EvaluateTrajectory(&le->pos, cg.time, end, qfalse, -1, 1.0f); CG_Trace(&tr, le->refEntity.origin, NULL, NULL, end, -1, CONTENTS_SOLID); if (tr.fraction == 1)
		{
			VectorCopy(end, le->refEntity.origin);
		}
		else
		{
			if (trap_CM_PointContents(tr.endpos, 0) & CONTENTS_NODROP)
			{
				CG_FreeLocalEntity(le); return;
			}
			CG_ReflectVelocity(le, &tr);
		}
	}
	fade = sparkFade(le); memset(p, 0, sizeof(p)); for (j = 0; j < 3; j++)
	{
		top[j] = (float)((double)(le->refEntity.radius + le->refEntity.radius) * cg.refdef.viewaxis[2][j] + le->refEntity.origin[j]); p[0].xyz[j] = (float)((double)le->refEntity.radius * cg.refdef.viewaxis[1][j] + le->refEntity.origin[j]); p[1].xyz[j] = (float)(-(double)le->refEntity.radius * cg.refdef.viewaxis[1][j] + le->refEntity.origin[j]); p[2].xyz[j] = (float)(-(double)le->refEntity.radius * cg.refdef.viewaxis[1][j] + top[j]); p[3].xyz[j] = (float)((double)le->refEntity.radius * cg.refdef.viewaxis[1][j] + top[j]);
	}
	sparkQuad(p, le, fade, 1);
#endif
}
void CG_FlatSparks(vec3_t origin, vec3_t direction, int count)
{
	TCE_CG_FlatSparks(origin, direction, count, cg.tceSparkIntensity, cgs.media.tceFlatSparkShader);
}
void CG_GlowSparks(vec3_t origin, vec3_t direction, int count)
{
	TCE_CG_GlowSparks(origin, direction, count, cg.tceSparkIntensity, cgs.media.tceGlowSparkShader);
}
