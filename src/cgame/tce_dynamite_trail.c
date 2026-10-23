/* TC:E Windows CG_DynamiteTrail @ 3006e6c0. */
#include "cg_local.h"

void TCE_CG_DynamiteTrail(centity_t *ent, const weaponInfo_t *wi)
{
	vec3_t  origin, direction;
	trace_t trace;
	int     phase;
#if defined(__linux__) && !defined(Q3_VM)
	long double facing;
#else
	double  facing;
#endif
	(void)wi;

	/* The original's extra trajectory multiplier is 1.0 here; the SDK
	 * trajectory interface expresses this unscaled case without that arg.
	 */
	BG_EvaluateTrajectory(&ent->currentState.pos, cg.time, origin, qfalse,
	                      ent->currentState.effect2Time);
#if defined(__linux__) && !defined(Q3_VM)
	{
		long double raised = (long double)origin[2] + 6.0L;

		origin[2] = (float)raised;
		direction[0] = origin[0] - cg.refdef.vieworg[0];
		direction[1] = origin[1] - cg.refdef.vieworg[1];
		direction[2] = (float)(raised - cg.refdef.vieworg[2]);
	}
#else
	origin[2] += 6.0f;
	VectorSubtract(origin, cg.refdef.vieworg, direction);
#endif
	VectorNormalize(direction);
#if defined(__linux__) && !defined(Q3_VM)
	facing = (long double)cg.refdef.viewaxis[0][1] * direction[1] +
	         (long double)cg.refdef.viewaxis[0][0] * direction[0];
	facing += (long double)cg.refdef.viewaxis[0][2] * direction[2];
#else
	/* Original x87 sums z,y,x before comparing with double 0.5. */
	facing = (double)cg.refdef.viewaxis[0][2] * direction[2] +
	         (double)cg.refdef.viewaxis[0][1] * direction[1] +
	         (double)cg.refdef.viewaxis[0][0] * direction[0];
#endif
	if (facing < 0.5)
	{
		return;
	}

	CG_Trace(&trace, cg.refdef.vieworg, NULL, NULL, origin,
	         ent->currentState.number, CONTENTS_SOLID | CONTENTS_BODY);
	if (trace.fraction < 1.0f)
	{
		return;
	}
	if (ent->currentState.teamNum >= 4)
	{
		return;
	}

	/* Unsigned arithmetic preserves the original 32-bit clock wraparound.
	 * Division/remainder remain signed (negative phases are not clamped).
	 */
	if (ent->currentState.effect3Time)
	{
		phase = (int)((unsigned int)ent->currentState.effect3Time -
		              (unsigned int)cg.time);
		if (phase > 20000)
		{
			phase /= 2;
		}
		else if (phase <= 5000)
		{
			phase = (int)((unsigned int)phase * 2u);
		}
	}
	else
	{
		phase = (int)((unsigned int)cg.time -
		              (unsigned int)ent->currentState.effect1Time);
		if (phase < 40000)
		{
			phase /= 2;
		}
		else if (phase >= 55000)
		{
			phase = (int)((unsigned int)phase * 2u);
		}
	}
	trap_R_AddCoronaToScene(origin, 1.0f, 0.0f, 0.0f, 0.2f,
	                        ent->currentState.number, phase % 1000 < 100);
}
