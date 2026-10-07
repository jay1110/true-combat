/* CG_Missile, Windows cgame:30030a10. TC:E weapon IDs/media are explicit.
 * Runtime entity dispatch uses this caller with TC:E weapon registration.
 * Trail callbacks do not read weaponInfo; never cast TC:E media to SDK media.
 * Invalid weapon IDs are clamped (original leaves negative/64 unchecked).
 */
#include "cg_local.h"
#include "tce_weapon_media.h"
#include "tce_smoke_grenade.h"
#include "../game/tce_trajectory.h"
extern void CG_RocketTrail(centity_t *, const weaponInfo_t *);
extern void CG_PyroSmokeTrail(centity_t *, const weaponInfo_t *);

void TCE_CG_Missile( centity_t *cent ) {
	refEntity_t			ent;
	entityState_t		*s1;
	const tce_weaponInfo_t		*weapon;

	s1 = &cent->currentState;
	if ( (unsigned)s1->weapon >= 64 ) {
		s1->weapon = 0;
	}
	weapon = &tce_cg_weapons[s1->weapon];

	VectorCopy( s1->angles, cent->lerpAngles);

	/* Weapon 30 has no legacy ET smoke-bomb dispatch. */
	if( s1->weapon == 27 && s1->clientNum == cg.snap->ps.clientNum ) {

		cg.satchelCharge = cent;
	} else if( s1->weapon == 63 && s1->otherEntityNum2 && s1->teamNum == cgs.clientinfo[ cg.clientNum ].team ) {
		VectorCopy( cent->lerpOrigin, cg.artilleryRequestPos[s1->clientNum] );
		cg.artilleryRequestTime[s1->clientNum] = cg.time;
	}

	if (cent->currentState.eType == 22 
		|| cent->currentState.eType == 23
		|| cent->currentState.eType == 24
		|| cent->currentState.eType == 25) {
		CG_RocketTrail (cent, NULL);
	} else {
        switch (weapon->missileTrail) {
        case TCE_TRAIL_GRENADE: TCE_CG_GrenadeTrail(cent, NULL); break;
        case TCE_TRAIL_ROCKET: CG_RocketTrail(cent, NULL); break;
        case TCE_TRAIL_PYROSMOKE: CG_PyroSmokeTrail(cent, NULL); break;
        case TCE_TRAIL_DYNAMITE: TCE_CG_DynamiteTrail(cent, NULL); break;
        }
    }

	if ( weapon->missileDlight ) {
		trap_R_AddLightToScene( cent->lerpOrigin, weapon->missileDlight, 1.0,
			weapon->missileDlightColor[0], weapon->missileDlightColor[1], weapon->missileDlightColor[2], 0, 0 );
	}

	if ( weapon->missileSound ) {
		if( cent->currentState.weapon == 55 || cent->currentState.weapon == 56 ) {
			if( !cent->currentState.effect1Time ) {
				int flytime = cg.time - cent->currentState.pos.trTime;

				if( flytime > 300 ) {
					vec3_t velocity;
					int volume = flytime > 375 ? 255 : (int)(float)(75.0 / (flytime - 300) * 255.0);

					TCE_BG_EvaluateTrajectoryDelta( &cent->currentState.pos, cg.time, velocity, qfalse, -1, 1.f );
					trap_S_AddLoopingSound( cent->lerpOrigin, velocity, weapon->missileSound, volume, 0 );
				}
			}
		} else {
			vec3_t	velocity;

			TCE_BG_EvaluateTrajectoryDelta( &cent->currentState.pos, cg.time, velocity, qfalse, -1, 1.f );
			trap_S_AddLoopingSound( cent->lerpOrigin, velocity, weapon->missileSound, 255, 0 );
		}
	}

	if ( cent->currentState.weapon == 15 ) {
		if ( cent->currentState.teamNum < 4 ) {
			vec3_t	velocity;

			TCE_BG_EvaluateTrajectoryDelta( &cent->currentState.pos, cg.time, velocity, qfalse, -1, 1.f );
			trap_S_AddRealLoopingSound( cent->lerpOrigin, velocity, weapon->spindownSound, 512, 48, 0 );
		}
	}

	memset (&ent, 0, sizeof(ent));
	VectorCopy( cent->lerpOrigin, ent.origin);
	VectorCopy( cent->lerpOrigin, ent.oldorigin);

	ent.skinNum = cg.clientFrame & 1;

	if (cent->currentState.eType == 22) {
		ent.hModel = cgs.gameModels[cent->currentState.modelindex];
	} else if (cent->currentState.eType == 27) {
		ent.hModel = cgs.gameModels[cent->currentState.modelindex];
	} else if (cent->currentState.eType == 21) {
		ent.hModel = cgs.media.flamebarrel;
	} else if (cent->currentState.eType == 23 || cent->currentState.eType == 24) {
		ent.hModel = 0;
	} else if (cent->currentState.eType == 25) {
		ent.hModel = 0;
	} else {
		team_t missileTeam = cent->currentState.weapon == 26 ? cent->currentState.teamNum % 4 : cent->currentState.teamNum;

		ent.hModel = weapon->missileModel;

		if( missileTeam == TEAM_ALLIES ) {
			ent.customSkin = weapon->missileAlliedSkin;
		} else if( missileTeam == TEAM_AXIS ) {
			ent.customSkin = weapon->missileAxisSkin;
		}
	}
	ent.renderfx = weapon->missileRenderfx | RF_NOSHADOW;

	if( cent->currentState.weapon == 26 ) {
		if( cgs.clientinfo[ cg.clientNum ].team == TEAM_SPECTATOR ) {
			return;
		}

		VectorCopy( ent.origin, ent.lightingOrigin );
		ent.renderfx |= RF_LIGHTING_ORIGIN;

		if(cent->currentState.teamNum < 4) {
			ent.origin[2] -= 8;
			ent.oldorigin[2] -= 8;

			if((cgs.clientinfo[cg.snap->ps.clientNum].team != (!cent->currentState.otherEntityNum2 ? TEAM_ALLIES : TEAM_AXIS))) {
				if(cent->currentState.density-1 == cg.snap->ps.clientNum) {
					ent.customShader = cgs.media.genericConstructionShader;
				} else if (!cent->currentState.modelindex2) {
					if( cgs.clientinfo[cg.snap->ps.clientNum].skill[SK_BATTLE_SENSE] >= 4 ) {
						vec_t distSquared = DistanceSquared( cent->lerpOrigin, cg.predictedPlayerEntity.lerpOrigin );

						if( distSquared > Square(256) )
							return;
						else
							ent.customShader = cgs.media.genericConstructionShader;
					} else {
						return;
					}
				} else {
					CG_DrawMineMarkerFlag( cent, &ent, weapon->modModels );
				}
			} else {
				CG_DrawMineMarkerFlag( cent, &ent, weapon->modModels );

			}
		}

		if(cent->currentState.teamNum >= 8) {
			ent.origin[2] -= 8;
			ent.oldorigin[2] -= 8;			
		}
	}

	if( cent->currentState.weapon == 60 ) {
        vec3_t delta;

		if( VectorCompare( cent->rawOrigin, vec3_origin ) ) {
			VectorSubtract( cent->lerpOrigin, s1->pos.trBase, delta );
			VectorCopy( cent->lerpOrigin, cent->rawOrigin );
		} else {
			VectorSubtract( cent->lerpOrigin, cent->rawOrigin, delta );
			if( !VectorCompare( cent->lerpOrigin, cent->rawOrigin ) ) {
				VectorCopy( cent->lerpOrigin, cent->rawOrigin );
			}
		}
		if ( VectorNormalize2( delta, ent.axis[0] ) == 0 ) {
			ent.axis[0][2] = 1;
		}
	} else if ( VectorNormalize2( s1->pos.trDelta, ent.axis[0] ) == 0 ) {
		ent.axis[0][2] = 1;
	}

	if ( s1->pos.trType != TR_STATIONARY ) {
		RotateAroundDirection( ent.axis, cg.time / 4 );
	} else {
		RotateAroundDirection( ent.axis, s1->time );
	}

	if (ent.hModel) 
		CG_AddRefEntityWithPowerups( &ent, s1->powerups, TEAM_FREE, s1, vec3_origin );
	if (s1->otherEntityNum == 17)
		TCE_CG_DrawSmokeGrenade(cent);

}

