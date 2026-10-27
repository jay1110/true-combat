/* Private active TC damage geometry, shared by selection and damage weighting.
 * Include after g_local.h; no engine-facing structures are extended here. */
#ifndef TCE_DAMAGE_TRACE_H
#define TCE_DAMAGE_TRACE_H
#include "tce_damage.h"

static void TCE_ClientDamagePose(const gentity_t *target, tceDamagePose_t *pose)
{
	VectorCopy(target->r.currentOrigin, pose->origin);
	VectorCopy(target->client->ps.viewangles, pose->angles);
	pose->lean = target->client->tceRewind.active ? target->client->tceRewind.lean : target->client->ps.leanf;
	pose->weapon = target->client->tceRewind.active ? target->client->tceRewind.weapon : target->client->ps.weapon;
	pose->prone = !!(target->client->ps.eFlags & EF_PRONE);
	pose->ducked = !!(target->client->ps.pm_flags & PMF_DUCKED);
	pose->aiming = !!(target->client->ps.stats[STAT_TCE_WEAPON_FLAGS] & 0x20);
	pose->enlarged = !!(target->client->ps.stats[STAT_TCE_FLAGS] & 0x200);
}

/* These are the same axis-aligned boxes linked by TCE_ApplyBodyDamage.
 * A finite segment intersection orders candidates without doing damage or
 * adding another set of engine traces/headshot classifications. */
static qboolean TCE_DamageVolumeFraction(const tceDamageVolume_t *volume,
                                        const vec3_t start, const vec3_t end,
                                        float *fraction)
{
	float nearPoint = 0, farPoint = 1;
	int axis;
	for (axis = 0; axis < 3; ++axis)
	{
		float low = volume->origin[axis] + volume->mins[axis];
		float high = volume->origin[axis] + volume->maxs[axis];
		float delta = end[axis] - start[axis], enter, leave;
		if (!(low <= high) || TCE_FloatIsNaN(delta) || TCE_FloatIsNaN(start[axis]))
		{
			return qfalse;
		}
		if (delta == 0)
		{
			if (start[axis] < low || start[axis] > high)
			{
				return qfalse;
			}
			continue;
		}
		enter = (low - start[axis]) / delta;
		leave = (high - start[axis]) / delta;
		if (enter > leave)
		{
			float swap = enter; enter = leave; leave = swap;
		}
		if (enter > nearPoint) nearPoint = enter;
		if (leave < farPoint) farPoint = leave;
		if (!(nearPoint <= farPoint)) return qfalse;
	}
	*fraction = nearPoint;
	return qtrue;
}
#endif
