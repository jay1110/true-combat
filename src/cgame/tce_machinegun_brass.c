/* TC:E brass callbacks: Windows 3006bdd0/3006c0d0, Linux b9660/b99ec. */
#include "cg_local.h"
#if defined(__linux__)
#include "../game/tce_numeric.h"
#endif

/* Retain original float reciprocal and x87 precision until assignment. */
static double brassRandom(void)
{
	return (double)(rand() & 0x7fff) * (1.0f / 32767.0f);
}
static double brassCenteredRandom(void)
{
	return 2.0 * (brassRandom() - 0.5);
}
static int brassEndTime(int start)
{
#if defined(__linux__)
	float base = (float)TCE_AddInt32(start, cg_brassTime.integer);
	float quarter = (float)(cg_brassTime.integer / 4);
	return TCE_TruncateNativeInteger((long double)quarter * brassRandom() + base);
#else
	int base = (int)((unsigned int)start + (unsigned int)cg_brassTime.integer);
	return (int)(base + (cg_brassTime.integer / 4) * brassRandom());
#endif
}

#if defined(__linux__)
/* The original retains X while Y/Z cross a float store before scaling. */
static void brassTransformVelocity(const vec3_t velocity, vec3_t axes[3],
                                   float scale, vec3_t result)
{
	int i;
	for (i = 0; i < 3; ++i)
	{
		long double component = (long double)velocity[0] * axes[0][i] +
		                        (long double)velocity[1] * axes[1][i];
		component += (long double)velocity[2] * axes[2][i];
		if (i != 0)
		{
			component = (float)component;
		}
		result[i] = (float)(component * scale);
	}
}

static void brassTransformOrigin(const vec3_t offset, vec3_t axes[3],
                                 const vec3_t origin, vec3_t result)
{
	int i;
	for (i = 0; i < 3; ++i)
	{
		long double component = (long double)offset[0] * axes[0][i] +
		                        (long double)offset[1] * axes[1][i];
		component += (long double)offset[2] * axes[2][i];
		if (i != 0)
		{
			component = (float)component;
		}
		result[i] = (float)(component + origin[i]);
	}
}
#endif

void CG_MachineGunEjectBrassNew(centity_t *cent)
{
	localEntity_t *le;
	refEntity_t   *re;
#if defined(__linux__)
	vec3_t        velocity;
#else
	vec3_t        velocity, xvelocity;
#endif
	float         waterScale = 1.0f;
	vec3_t        v[3];

	if (cg_brassTime.integer <= 0)
	{
		return;
	}

	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	velocity[0] = -50 + 25 * brassCenteredRandom();
	velocity[1] = -100 + 40 * brassCenteredRandom();
#if defined(__linux__)
	velocity[2] = (float)((long double)(rand() & 0x7fff) * (50.0f / 32767.0f) + 200.0f);
#else
	velocity[2] = 200 + 50 * brassRandom();
#endif

	le->leType    = LE_FRAGMENT;
	le->startTime = cg.time;
	le->endTime   = brassEndTime(le->startTime);

	le->pos.trType = TR_GRAVITY;
#if defined(__linux__)
	le->pos.trTime = TCE_SubInt32(cg.time, rand() & 15);
#else
	le->pos.trTime = cg.time - (rand() & 15);
#endif

	AnglesToAxis(cent->lerpAngles, v);

	VectorCopy(ejectBrassCasingOrigin, re->origin);

	VectorCopy(re->origin, le->pos.trBase);

	if (CG_PointContents(re->origin, -1) & (CONTENTS_WATER | CONTENTS_SLIME))
	{

		waterScale = 0.10;
	}

#if defined(__linux__)
	brassTransformVelocity(velocity, v, waterScale, le->pos.trDelta);
#else
	xvelocity[0] = velocity[0] * v[0][0] + velocity[1] * v[1][0] + velocity[2] * v[2][0];
	xvelocity[1] = velocity[0] * v[0][1] + velocity[1] * v[1][1] + velocity[2] * v[2][1];
	xvelocity[2] = velocity[0] * v[0][2] + velocity[1] * v[1][2] + velocity[2] * v[2][2];
	VectorScale(xvelocity, waterScale, le->pos.trDelta);
#endif

	AxisCopy(axisDefault, re->axis);
	re->hModel = cgs.media.smallgunBrassModel;

#if defined(__linux__)
	le->bounceFactor = (float)((long double)0.4 * waterScale);
#else
	le->bounceFactor = 0.4 * waterScale;
#endif

	le->angles.trType     = TR_LINEAR;
	le->angles.trTime     = cg.time;
	le->angles.trBase[0]  = (rand() & 31) + 60;
	le->angles.trBase[1]  = rand() & 255;
	le->angles.trBase[2]  = rand() & 31;
	le->angles.trDelta[0] = 2;
	le->angles.trDelta[1] = 1;
	le->angles.trDelta[2] = 0;

	le->leFlags = LEF_TUMBLE;

	{
		int    contents;
		vec3_t end;
		VectorCopy(cent->lerpOrigin, end);
		end[2]  -= 24;
		contents = CG_PointContents(end, 0);
		if (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA))
		{
			le->leBounceSoundType = LEBS_NONE;
		}
		else
		{
			le->leBounceSoundType = LEBS_BRASS;
		}
	}

	le->leMarkType = LEMT_NONE;
}

void CG_MachineGunEjectBrass(centity_t *cent)
{
	localEntity_t *le;
	refEntity_t   *re;
#if defined(__linux__)
	vec3_t        velocity, offset;
#else
	vec3_t        velocity, xvelocity;
	vec3_t        offset, xoffset;
#endif
	float         waterScale = 1.0f;
	vec3_t        v[3];

	if (cg_brassTime.integer <= 0)
	{
		return;
	}

	if (!(cg.snap->ps.persistant[PERS_HWEAPON_USE]) && (cent->currentState.clientNum == cg.snap->ps.clientNum) && (!(cent->currentState.eFlags & EF_MG42_ACTIVE || cent->currentState.eFlags & EF_AAGUN_ACTIVE)))
	{
		CG_MachineGunEjectBrassNew(cent);
		return;
	}

	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	le->leType    = LE_FRAGMENT;
	le->startTime = cg.time;
	le->endTime   = brassEndTime(le->startTime);

	le->pos.trType = TR_GRAVITY;
#if defined(__linux__)
	le->pos.trTime = TCE_SubInt32(cg.time, rand() & 15);
#else
	le->pos.trTime = cg.time - (rand() & 15);
#endif

	AnglesToAxis(cent->lerpAngles, v);

	if (cent->currentState.eFlags & EF_MG42_ACTIVE || cent->currentState.eFlags & EF_AAGUN_ACTIVE)
	{
		offset[0]             = 25;
		offset[1]             = -4;
		offset[2]             = 28;
		velocity[0]           = -20 + 40 * brassCenteredRandom();
		velocity[1]           = -150 + 40 * brassCenteredRandom();
		velocity[2]           = 100 + 50 * brassCenteredRandom();
		re->hModel            = cgs.media.machinegunBrassModel;
		le->angles.trBase[0]  = 90;
		le->angles.trBase[1]  = rand() & 255;
		le->angles.trBase[2]  = rand() & 31;
		le->angles.trDelta[0] = 2;
		le->angles.trDelta[1] = 1;
		le->angles.trDelta[2] = 0;
	}
	else
	{
		re->hModel = cgs.media.smallgunBrassModel;
		/* Original 64-slot TC:E IDs; do not substitute SDK weapon enums. */
		switch (cent->currentState.weapon)
		{
		case 2:
		case 7:
		case 14:
		case 52:
			offset[0] = 24;
			offset[1] = -4;
			offset[2] = 36;
			break;
		case 31:
		case 62:
			offset[0]  = 12;
			offset[1]  = -4;
			offset[2]  = 24;
			re->hModel = cgs.media.machinegunBrassModel;
			break;
		case 23:
		case 24:
		case 32:
			re->hModel = cgs.media.machinegunBrassModel;
		default:
			offset[0] = 16;
			offset[1] = -4;
			offset[2] = 24;
			break;
		}
		velocity[0]           = -50 + 25 * brassCenteredRandom();
		velocity[1]           = -100 + 40 * brassCenteredRandom();
#if defined(__linux__)
		velocity[2] = (float)((long double)(rand() & 0x7fff) * (50.0f / 32767.0f) + 200.0f);
#else
		velocity[2]           = 200 + 50 * brassRandom();
#endif
		le->angles.trBase[0]  = (rand() & 15) + 82;
		le->angles.trBase[1]  = rand() & 255;
		le->angles.trBase[2]  = rand() & 31;
		le->angles.trDelta[0] = 2;
		le->angles.trDelta[1] = 1;
		le->angles.trDelta[2] = 0;
	}

#if defined(__linux__)
	brassTransformOrigin(offset, v, cent->lerpOrigin, re->origin);
#else
	xoffset[0] = offset[0] * v[0][0] + offset[1] * v[1][0] + offset[2] * v[2][0];
	xoffset[1] = offset[0] * v[0][1] + offset[1] * v[1][1] + offset[2] * v[2][1];
	xoffset[2] = offset[0] * v[0][2] + offset[1] * v[1][2] + offset[2] * v[2][2];
	VectorAdd(cent->lerpOrigin, xoffset, re->origin);
#endif

	VectorCopy(re->origin, le->pos.trBase);

	if (CG_PointContents(re->origin, -1) & (CONTENTS_WATER | CONTENTS_SLIME))
	{

		waterScale = 0.10;
	}

#if defined(__linux__)
	brassTransformVelocity(velocity, v, waterScale, le->pos.trDelta);
#else
	xvelocity[0] = velocity[0] * v[0][0] + velocity[1] * v[1][0] + velocity[2] * v[2][0];
	xvelocity[1] = velocity[0] * v[0][1] + velocity[1] * v[1][1] + velocity[2] * v[2][1];
	xvelocity[2] = velocity[0] * v[0][2] + velocity[1] * v[1][2] + velocity[2] * v[2][2];
	VectorScale(xvelocity, waterScale, le->pos.trDelta);
#endif

	AxisCopy(axisDefault, re->axis);

#if defined(__linux__)
	le->bounceFactor = (float)((long double)0.4 * waterScale);
#else
	le->bounceFactor = 0.4 * waterScale;
#endif

	le->angles.trType = TR_LINEAR;
	le->angles.trTime = cg.time;

	le->leFlags = LEF_TUMBLE;

	{
		int    contents;
		vec3_t end;
		VectorCopy(cent->lerpOrigin, end);
		end[2]  -= 24;
		contents = CG_PointContents(end, 0);
		if (contents & (CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA))
		{
			le->leBounceSoundType = LEBS_NONE;
		}
		else
		{
			le->leBounceSoundType = LEBS_BRASS;
		}
	}

	le->leMarkType = LEMT_NONE;
}
