/* TC:E cgame: CG_GetWindVector 3006c610, CG_PyroSmokeTrail 3006c670. */
#include "cg_local.h"
#if defined(__linux__)
#include "../game/tce_numeric.h"
#endif

/* The original multiplies by the float reciprocal, rather than dividing. */
static float smokeRandom(void)
{
	return (rand() & 0x7fff) * (1.0f / 32767.0f);
}

#if defined(__linux__)
static long double smokeRandomHeld(void)
{
	return (long double)(rand() & 0x7fff) * (1.0f / 32767.0f);
}
#endif

void CG_GetWindVector(vec3_t direction)
{
	direction[0] = smokeRandom() * 0.25f;
	direction[1] = cgs.smokeWindDir;
	direction[2] = smokeRandom();
	VectorNormalize(direction);
}

void CG_PyroSmokeTrail(centity_t *ent, const weaponInfo_t *wi)
{
	entityState_t *es = &ent->currentState;
	vec3_t        origin, lastPos, direction;
	int           team, time;
	float         randomSize, tint, speed;
	(void)wi;

	if (es->weapon == WP_LANDMINE)
	{
		if (es->teamNum < 8)
		{
			ent->miscTime = 0;
			return;
		}
		if (es->teamNum < 12 && !ent->miscTime)
		{
			ent->trailTime = ent->miscTime = cg.time;
			trap_S_StartSound(NULL, es->number, CHAN_WEAPON, cgs.media.minePrimedSound);
		}
#if defined(__linux__)
		if (TCE_SubInt32(cg.time, ent->miscTime) > 1000)
#else
		if ((int)((unsigned int)cg.time - (unsigned int)ent->miscTime) > 1000)
#endif
		{
			return;
		}
		team = es->otherEntityNum2 ? TEAM_AXIS : TEAM_ALLIES;
	}
	else
	{
		team = es->teamNum;
	}

#if defined(__linux__)
	time = (TCE_AddInt32(ent->trailTime, 30) / 30) * 30;
#else
	time = ((int)((unsigned int)ent->trailTime + 30u) / 30) * 30;
#endif
	BG_EvaluateTrajectory(&es->pos, cg.time, origin, qfalse, es->effect2Time);
	(void)CG_PointContents(origin, -1);
	BG_EvaluateTrajectory(&es->pos, ent->trailTime, lastPos, qfalse, es->effect2Time);
	(void)CG_PointContents(lastPos, -1);
	ent->trailTime = cg.time;

	/* Contents queries are retained but do not suppress dye/smoke in water. */
#if defined(__linux__)
	for (; time <= ent->trailTime; time = TCE_AddInt32(time, 30))
#else
	for (; time <= ent->trailTime; time = (int)((unsigned int)time + 30u))
#endif
	{
		BG_EvaluateTrajectory(&es->pos, time, lastPos, qfalse, es->effect2Time);
		(void)rand(); /* Original consumes an otherwise unused random value. */
		if (es->density)
		{
			vec3_t angles, right;
			VectorCopy(es->apos.trBase, angles);
			angles[ROLL] += cg.time % 360;
			AngleVectors(angles, NULL, right, NULL);
#if defined(__linux__)
			{
				int axis;
				for (axis = 0; axis < 3; ++axis)
					lastPos[axis] = (float)((long double)es->density * right[axis] + lastPos[axis]);
			}
#else
			VectorMA(lastPos, es->density, right, lastPos);
#endif
		}
#if defined(__linux__)
		direction[0] = (float)(2.0L * (smokeRandomHeld() - 0.5) * 5.0);
		direction[1] = (float)(2.0L * (smokeRandomHeld() - 0.5) * 5.0);
#else
		direction[0] = (float)(2.0 * (smokeRandom() - 0.5) * 5.0);
		direction[1] = (float)(2.0 * (smokeRandom() - 0.5) * 5.0);
#endif
		direction[2] = 0;
		VectorAdd(lastPos, direction, origin);
		randomSize = smokeRandom();
		CG_GetWindVector(direction);
		speed = es->weapon == WP_LANDMINE ? 45.0f : 65.0f;
		VectorScale(direction, speed, direction);
#if defined(__linux__)
		tint = (float)((long double)0.5 * randomSize + (long double)0.5);
		CG_SmokePuff(origin, direction, (float)((long double)randomSize * 110.0f + 25.0f),
#else
		tint = (randomSize + 1.0f) * 0.5f;
		CG_SmokePuff(origin, direction, 25.0f + randomSize * 110.0f,
#endif
		             team == TEAM_ALLIES ? tint : 1.0f, tint,
		             team == TEAM_ALLIES ? 1.0f : tint, 0.5f,
		             (float)(4800 + rand() % 2800), time, 0, 0,
		             cgs.media.smokePuffShader);
	}
}
