#include "cg_local.h"
#if defined(__linux__) && !defined(Q3_VM)
#include "../game/tce_numeric.h"
#include "../game/tce_trajectory.h"
#endif

void CG_BloodTrail(localEntity_t *le)
{
#if defined(__linux__) && !defined(Q3_VM)
	long double speed;
	int step, time, end;
	unsigned bits;
	vec3_t origin;
	static vec3_t color = { 1, 1, 1 };
	if (!cg_blood.integer)
	{
		return;
	}
	speed = TCE_VectorLengthValue(le->pos.trDelta);
	if (!(speed >= FLT_EPSILON))
	{
		return;
	}
	step = TCE_TruncateNativeInteger((1.0L / speed) * 3000.0f);
	if (step <= 0)
	{
		return;
	}
	time = TCE_AddInt32(TCE_SubInt32(cg.time, cg.frametime), step);
	bits = (unsigned)(time / step) * (unsigned)step;
	memcpy(&time, &bits, sizeof(time));
	bits = (unsigned)(cg.time / step) * (unsigned)step;
	memcpy(&end, &bits, sizeof(end));
	for (; time <= end; time = TCE_AddInt32(time, step))
	{
		TCE_BG_EvaluateTrajectory(&le->pos, time, origin, qfalse, -1, 1.0f);
		le->headJuncIndex = CG_AddTrailJunc(le->headJuncIndex, le,
		                                    cgs.media.bloodTrailShader, time, STYPE_STRETCH, origin,
		                                    180, 1, 0, 12, 12, TJFL_NOCULL, color, color, 0, 0);
	}
#else
	double        speed;
	int           step, t, end;
	vec3_t        origin;
	static vec3_t color = { 1, 1, 1 };
	if (!cg_blood.integer)
	{
		return;
	}
	speed = sqrt((double)le->pos.trDelta[0] * le->pos.trDelta[0]
	             + (double)le->pos.trDelta[1] * le->pos.trDelta[1]
	             + (double)le->pos.trDelta[2] * le->pos.trDelta[2]);
	if (speed < FLT_EPSILON)
	{
		return;
	}
	/* __ftol returns the low 32 bits of its signed 64-bit conversion. */
	step = (int)(unsigned)(long long)(3000.0 / speed);
	if (step <= 0)
	{
		return;
	}
	t   = (int)((unsigned)cg.time - (unsigned)cg.frametime + (unsigned)step);
	t   = (int)((unsigned)(t / step) * (unsigned)step);
	end = (int)((unsigned)(cg.time / step) * (unsigned)step);
	for (; t <= end; t = (int)((unsigned)t + (unsigned)step))
	{
		BG_EvaluateTrajectory(&le->pos, t, origin, qfalse, -1);
		le->headJuncIndex = CG_AddTrailJunc(le->headJuncIndex, le,
		                                    cgs.media.bloodTrailShader, t, STYPE_STRETCH, origin, 180,
		                                    1, 0, 12, 12, TJFL_NOCULL, color, color, 0, 0);
	}
#endif
}

/* Explicit state permits deterministic replay of the global rate limiter. */
void TCE_CG_FragmentBounceMark(localEntity_t *le, trace_t *trace, int *lastBloodMark)
{
#if defined(__linux__) && !defined(Q3_VM)
	if (le->leMarkType == LEMT_BLOOD && *lastBloodMark <= cg.time &&
	    *lastBloodMark <= TCE_SubInt32(cg.time, 100))
	{
		vec4_t projection, color;
		int choice, life, fade;
		unsigned lifeBits;
		VectorSet(projection, 0, 0, -1);
		projection[3] = 16 + (rand() & 31);
		Vector4Set(color, 1, 1, 1, 1);
		choice = rand() % 5;
		lifeBits = (unsigned)cg_bloodTime.integer * 1000u;
		memcpy(&life, &lifeBits, sizeof(life));
		fade = life >= 0 ? life / 16 : -1 - (int)((~lifeBits) >> 4);
		trap_R_ProjectDecal(cgs.media.bloodDotShaders[choice], 1,
		                    (vec3_t *)trace->endpos, projection, color, life, fade);
		*lastBloodMark = cg.time;
	}
	le->leMarkType = LEMT_NONE;
#else
	vec4_t projection, color;
	int    life;
	if (le->leMarkType == LEMT_BLOOD && *lastBloodMark <= cg.time &&
	    *lastBloodMark <= (int)((unsigned)cg.time - 100u))
	{
		VectorSet(projection, 0, 0, -1);
		projection[3] = 16 + (rand() & 31);
		Vector4Set(color, 1, 1, 1, 1);
		life = (int)((unsigned)cg_bloodTime.integer * 1000u);
		trap_R_ProjectDecal(cgs.media.bloodDotShaders[rand() % 5], 1,
		                    (vec3_t *)trace->endpos, projection, color, life, life >> 4);
		*lastBloodMark = cg.time;
	}
	le->leMarkType = LEMT_NONE;
#endif
}

void CG_FragmentBounceMark(localEntity_t *le, trace_t *trace)
{
	static int lastBloodMark;
	TCE_CG_FragmentBounceMark(le, trace, &lastBloodMark);
}
