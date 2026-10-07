/* TC:E Windows brass callbacks 3006bdd0 and 3006c0d0. */
#include "cg_local.h"

/* Retain original float reciprocal and x87 precision until assignment. */
static double brassRandom(void) {
    return (double)(rand() & 0x7fff) * (1.0f / 32767.0f);
}
static double brassCenteredRandom(void) { return 2.0 * (brassRandom() - 0.5); }
static int brassEndTime(int start) {
    int base = (int)((unsigned int)start + (unsigned int)cg_brassTime.integer);
    return (int)(base + (cg_brassTime.integer / 4) * brassRandom());
}

void CG_MachineGunEjectBrassNew( centity_t *cent ) {
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t			velocity, xvelocity;
	float			waterScale = 1.0f;
	vec3_t			v[3];

	if ( cg_brassTime.integer <= 0 ) {
		return;
	}

	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	velocity[0] = -50+25*brassCenteredRandom();
	velocity[1] = -100+40*brassCenteredRandom();
	velocity[2] = 200+50*brassRandom();

	le->leType = LE_FRAGMENT;
	le->startTime = cg.time;
	le->endTime = brassEndTime(le->startTime);

	le->pos.trType = TR_GRAVITY;
	le->pos.trTime = cg.time - (rand()&15);

	AnglesToAxis( cent->lerpAngles, v );

	VectorCopy (ejectBrassCasingOrigin, re->origin);

	VectorCopy( re->origin, le->pos.trBase );

	if ( CG_PointContents( re->origin, -1 ) & (CONTENTS_WATER | CONTENTS_SLIME) ) {

		waterScale = 0.10;
	}

	xvelocity[0] = velocity[0] * v[0][0] + velocity[1] * v[1][0] + velocity[2] * v[2][0];
	xvelocity[1] = velocity[0] * v[0][1] + velocity[1] * v[1][1] + velocity[2] * v[2][1];
	xvelocity[2] = velocity[0] * v[0][2] + velocity[1] * v[1][2] + velocity[2] * v[2][2];
	VectorScale( xvelocity, waterScale, le->pos.trDelta );

	AxisCopy( axisDefault, re->axis );
	re->hModel = cgs.media.smallgunBrassModel;

	le->bounceFactor = 0.4 * waterScale;

	le->angles.trType = TR_LINEAR;
	le->angles.trTime = cg.time;
	le->angles.trBase[0] = (rand()&31) + 60;
	le->angles.trBase[1] = rand()&255;
	le->angles.trBase[2] = rand()&31;
	le->angles.trDelta[0] = 2;
	le->angles.trDelta[1] = 1;
	le->angles.trDelta[2] = 0;

	le->leFlags = LEF_TUMBLE;

	{
		int		contents;
		vec3_t	end;
		VectorCopy( cent->lerpOrigin, end );
		end[2] -= 24;
		contents = CG_PointContents( end, 0 );
		if (contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ))
			le->leBounceSoundType = LEBS_NONE;
		else
			le->leBounceSoundType = LEBS_BRASS;
	}

	le->leMarkType = LEMT_NONE;
}

void CG_MachineGunEjectBrass( centity_t *cent ) {
	localEntity_t	*le;
	refEntity_t		*re;
	vec3_t			velocity, xvelocity;
	vec3_t			offset, xoffset;
	float			waterScale = 1.0f;
	vec3_t			v[3];

	if ( cg_brassTime.integer <= 0 ) {
		return;
	}

	if (!(cg.snap->ps.persistant[PERS_HWEAPON_USE]) && (cent->currentState.clientNum == cg.snap->ps.clientNum) && (!(cent->currentState.eFlags & EF_MG42_ACTIVE || cent->currentState.eFlags & EF_AAGUN_ACTIVE))) {
		CG_MachineGunEjectBrassNew (cent);
		return;
	}

	le = CG_AllocLocalEntity();
	re = &le->refEntity;

	le->leType = LE_FRAGMENT;
	le->startTime = cg.time;
	le->endTime = brassEndTime(le->startTime);

	le->pos.trType = TR_GRAVITY;
	le->pos.trTime = cg.time - (rand()&15);

	AnglesToAxis( cent->lerpAngles, v );

	if ( cent->currentState.eFlags & EF_MG42_ACTIVE || cent->currentState.eFlags & EF_AAGUN_ACTIVE ) {
		offset[0] = 25;
		offset[1] = -4;
		offset[2] = 28;
		velocity[0] = -20+40*brassCenteredRandom();
		velocity[1] = -150 + 40 * brassCenteredRandom();
		velocity[2] = 100 + 50 * brassCenteredRandom();
		re->hModel = cgs.media.machinegunBrassModel;
		le->angles.trBase[0] = 90;
		le->angles.trBase[1] = rand()&255;
		le->angles.trBase[2] = rand()&31;
		le->angles.trDelta[0] = 2;
		le->angles.trDelta[1] = 1;
		le->angles.trDelta[2] = 0;
	}
	else {
		re->hModel = cgs.media.smallgunBrassModel;
		/* Original 64-slot TC:E IDs; do not substitute SDK weapon enums. */
		switch (cent->currentState.weapon) {
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
			offset[0] = 12;
			offset[1] = -4;
			offset[2] = 24;
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
		velocity[0] = -50+25*brassCenteredRandom();
		velocity[1] = -100+40*brassCenteredRandom();
		velocity[2] = 200+50*brassRandom();
		le->angles.trBase[0] = (rand()&15) + 82;
		le->angles.trBase[1] = rand()&255;
		le->angles.trBase[2] = rand()&31;
		le->angles.trDelta[0] = 2;
		le->angles.trDelta[1] = 1;
		le->angles.trDelta[2] = 0;
	}

	xoffset[0] = offset[0] * v[0][0] + offset[1] * v[1][0] + offset[2] * v[2][0];
	xoffset[1] = offset[0] * v[0][1] + offset[1] * v[1][1] + offset[2] * v[2][1];
	xoffset[2] = offset[0] * v[0][2] + offset[1] * v[1][2] + offset[2] * v[2][2];
	VectorAdd( cent->lerpOrigin, xoffset, re->origin );

	VectorCopy( re->origin, le->pos.trBase );

	if ( CG_PointContents( re->origin, -1 ) & (CONTENTS_WATER | CONTENTS_SLIME )) {

		waterScale = 0.10;
	}

	xvelocity[0] = velocity[0] * v[0][0] + velocity[1] * v[1][0] + velocity[2] * v[2][0];
	xvelocity[1] = velocity[0] * v[0][1] + velocity[1] * v[1][1] + velocity[2] * v[2][1];
	xvelocity[2] = velocity[0] * v[0][2] + velocity[1] * v[1][2] + velocity[2] * v[2][2];
	VectorScale( xvelocity, waterScale, le->pos.trDelta );

	AxisCopy( axisDefault, re->axis );

	le->bounceFactor = 0.4 * waterScale;

	le->angles.trType = TR_LINEAR;
	le->angles.trTime = cg.time;

	le->leFlags = LEF_TUMBLE;

	{
		int		contents;
		vec3_t	end;
		VectorCopy( cent->lerpOrigin, end );
		end[2] -= 24;
		contents = CG_PointContents( end, 0 );
		if (contents & ( CONTENTS_WATER | CONTENTS_SLIME | CONTENTS_LAVA ))
			le->leBounceSoundType = LEBS_NONE;
		else
			le->leBounceSoundType = LEBS_BRASS;
	}

	le->leMarkType = LEMT_NONE;
}

