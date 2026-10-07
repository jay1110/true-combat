

// cg_playerstate.c -- this file acts on changes in a new playerState_t
// With normal play, this will be done after local prediction, but when
// following another player or playing back a demo, it will be checked
// when the snapshot transitions like all the other entities

#include "cg_local.h"
#include "../game/tce_bg.h"
#include "tce_flash.h"

/*
==============
CG_CheckAmmo

Update the original TC low-ammo state without the SDK warning sound.
==============
*/
/* TC300568b0: no SDK low-ammo click. Preserve original x86 mask wrap. */
void CG_CheckAmmo( void ) {
    unsigned int weapons = (unsigned int)cg.snap->ps.weapons[0];
    int weapon, total = 0;
    if (!weapons && !cg.snap->ps.weapons[1]) return;
    for (weapon = 0; weapon < 64; ++weapon) {
        if (!(weapons & (1u << (weapon & 31)))) continue;
        total += cg.snap->ps.ammo[BG_FindAmmoForWeapon(weapon)] * 1000;
        if (total >= 5000) { cg.lowAmmoWarning = 0; return; }
    }
    cg.lowAmmoWarning = total ? 1 : 2;
}

/*
==============
CG_DamageFeedback
==============
*/
void CG_DamageFeedback( int yawByte, int pitchByte, int damage ) {
	float		left, front, up;
	float		kick;
	int			health;
	double		scale;
	vec3_t		dir;
	vec3_t		angles;
	float		dist;
	float		yaw, pitch;
	int			slot;
	viewDamage_t *vd;

	// show the attacking player's head and name in corner
	cg.attackerTime = cg.time;

	// the lower on health you are, the greater the view kick will be
	health = cg.snap->ps.stats[STAT_HEALTH];
	if ( health < 40 ) {
		scale = 1;
	} else {
		scale = 40.0 / health;
	}
	kick = damage * scale;

	if (kick < 5)
		kick = 5;
	if (kick > 10)
		kick = 10;

	// find a free slot
	for (slot=0; slot<MAX_VIEWDAMAGE; slot++) {
		if (cg.viewDamage[slot].damageTime + cg.viewDamage[slot].damageDuration < cg.time)
			break;
	}

	if (slot==MAX_VIEWDAMAGE)
		return;		// no free slots, never override or splats will suddenly disappear

	vd = &cg.viewDamage[slot];

	// if yaw and pitch are both 255, make the damage always centered (falling, etc)
	if ( yawByte == 255 && pitchByte == 255 ) {
		vd->damageX = 0;
		vd->damageY = 0;
		cg.v_dmg_roll = 0;
		cg.v_dmg_pitch = -kick;
	} else {
		// positional
		pitch = (float)(pitchByte * (360.0 / 255.0));
		yaw = (float)(yawByte * (360.0 / 255.0));

		angles[PITCH] = pitch;
		angles[YAW] = yaw;
		angles[ROLL] = 0;

		AngleVectors( angles, dir, NULL, NULL );
		VectorSubtract( vec3_origin, dir, dir );

		/* Original30056a68..ae1 sums Z,Y,X in x87 before storing each float. */
		front = (float)(((double)dir[2]*cg.refdef.viewaxis[0][2] + (double)dir[1]*cg.refdef.viewaxis[0][1]) + (double)dir[0]*cg.refdef.viewaxis[0][0]);
		left = (float)(((double)dir[2]*cg.refdef.viewaxis[1][2] + (double)dir[1]*cg.refdef.viewaxis[1][1]) + (double)dir[0]*cg.refdef.viewaxis[1][0]);
		up = (float)(((double)dir[2]*cg.refdef.viewaxis[2][2] + (double)dir[1]*cg.refdef.viewaxis[2][1]) + (double)dir[0]*cg.refdef.viewaxis[2][0]);

		dir[0] = front;
		dir[1] = left;
		dir[2] = 0;
		dist = VectorLength( dir );
		if ( dist < 0.1 ) {
			dist = 0.1;
		}

		cg.v_dmg_roll = kick * left;
		
		cg.v_dmg_pitch = -kick * front;

		if ( front <= 0.1 ) {
			front = 0.1;
		}
		vd->damageX = (float)((((double)(rand()&32767)*(double)(1.0f/32767.0f)-.5)*2.0)*.3 - (double)left/front);
		vd->damageY = (float)((((double)(rand()&32767)*(double)(1.0f/32767.0f)-.5)*2.0)*.3 + (double)up/dist);
	}

	// clamp the position
	if ( vd->damageX > 1.0 ) {
		vd->damageX = 1.0;
	}
	if ( vd->damageX < - 1.0 ) {
		vd->damageX = -1.0;
	}

	if ( vd->damageY > 1.0 ) {
		vd->damageY = 1.0;
	}
	if ( vd->damageY < - 1.0 ) {
		vd->damageY = -1.0;
	}

	// don't let the screen flashes vary as much
	if ( kick > 10 ) {
		kick = 10;
	}
	vd->damageValue = kick;
	cg.v_dmg_time = cg.time + DAMAGE_TIME;
	vd->damageTime = cg.snap->serverTime;
	vd->damageDuration = kick * 50 * (1 + 2*(!vd->damageX && !vd->damageY));
	cg.damageTime = cg.snap->serverTime;
	cg.damageIndex = slot;
}




/*
================
CG_Respawn

A respawn happened this snapshot
================
*/
void CG_Respawn( qboolean revived ) {
	cg.serverRespawning = qfalse;	// Arnout: just in case

	// no error decay on player movement
	cg.thisFrameTeleport = qtrue;

	// need to reset client-side weapon animations
	cg.predictedPlayerState.weapAnim = ( ( cg.predictedPlayerState.weapAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | PM_IdleAnimForWeapon(cg.snap->ps.weapon);	// reset weapon animations
	cg.predictedPlayerState.weaponstate = WEAPON_READY;	// hmm, set this?  what to?

	// display weapons available
	cg.weaponSelectTime = cg.time;

	cg.cursorHintIcon = 0;
	cg.cursorHintTime = 0;

	cg.cameraMode = qfalse;	//----(SA)	get out of camera for sure

	// select the weapon the server says we are using
	cg.weaponSelect = cg.snap->ps.weapon;
	// DHM - Nerve :: Clear even more things on respawn
	cg.zoomedBinoc = qfalse;
	cg.zoomedScope = qfalse;
	cg.zoomTime = 0;
	cg.zoomval = 0;

	trap_SendConsoleCommand( "-zoom\n" );
	cg.binocZoomTime = 0;


	// clear pmext
	memset( &cg.pmext, 0, sizeof(cg.pmext) );
	
	cg.pmext.bAutoReload = (cg_autoReload.integer > 0);

	cg.pmext.sprintTime = SPRINTTIME;

	if( !revived ) {
		cgs.limboLoadoutSelected = qfalse;
	}


    /* TC respawn state that already has productive native consumers. */
    cg.tceAimActive = cg.tceAimComplete = cg.tceAimRequested = 0;
    cg.tceAimWeaponLatch = 0;
    cg.tceTacticalScale = 0; /* Original34846e6c; offset producer and weapon-position consumer share this state. */
    cg.tceActionTransitionTime = 0;
    cg.tceShotHoldUntil=cg.time;
    cg.tceHeartbeatNext=cg.tceHeartbeatUntil=0;
    cg.tceAdsBreathTime=0;
    cg.tceCoronaBlendAlpha=0;
    cg.tceSoundSampleTime=cg.tceSoundEnvironment=0;
    VectorClear(cg.tceSoundLastOrigin);
    VectorClear(cg.tceRoomExtent);
    cg.tceRoomDistance=0;
    memset(cg.tceRoomWeights,0,sizeof(cg.tceRoomWeights));
    cg.tceSoundZoneEffects[0][0]=cg.tceSoundZoneEffects[0][1]=
        cg.tceSoundZoneEffects[1][0]=cg.tceSoundZoneEffects[1][1];
    memset(cg.tceSoundZoneSounds,0,sizeof(cg.tceSoundZoneSounds));
    memset(cg.tceSoundZoneVolume,0,sizeof(cg.tceSoundZoneVolume));
    cg.tceSoundZoneActive=cg.tceSoundZoneTransitionTime=0;
    cg.tceFiremodeTime = cg.time;
    cg.tceFiremodeAnimationTime = -1000;
    tceFlash.blindUntil = tceFlash.blindActive = tceFlash.deafUntil = 0;
    tceFlash.deafness = 0;
    trap_Cvar_Set("r_ambientscale", "1.3");
	cg.proneMovingTime = 0;
	/* TC30056cb0 resets stance4950 and scopeBlocked4b0c, but preserves
	 * duck4948/prone4958, sway4a04.. and the captured scope entity. */
	cg.tceStanceTime=cg.tceScopeBlocked=0;
	cg.tceEyeSampleTime=0; /* original340a4bd0 */
	cg.tceEyeSmooth=cg.tceEyeLinear=cg.tceEyeFlare=0;
	cg.tceEyeFlareFrame=0;cg.tceScopeLightBoost=1;

	// reset fog to world fog (if present)
	trap_R_SetFog(FOG_CMD_SWITCHFOG, FOG_MAP,20,0,0,0,0);
	// dhm - end
}

extern char *eventnames[];

/*
==============
CG_CheckPlayerstateEvents
==============
*/
void CG_CheckPlayerstateEvents_wolf( playerState_t *ps, playerState_t *ops ) {
	int			i;
	int			event;
	centity_t	*cent;
/*
	if ( ps->externalEvent && ps->externalEvent != ops->externalEvent ) {
		cent = &cg_entities[ ps->clientNum ];
		cent->currentState.event = ps->externalEvent;
		cent->currentState.eventParm = ps->externalEventParm;
		CG_EntityEvent( cent, cent->lerpOrigin );
	}
*/
	cent = &cg.predictedPlayerEntity; // cg_entities[ ps->clientNum ];
	// go through the predictable events buffer
	for ( i = ps->eventSequence - MAX_EVENTS ; i < ps->eventSequence ; i++ ) {
		if ( ps->events[i & (MAX_EVENTS-1)] != ops->events[i & (MAX_EVENTS-1)]
			|| i >= ops->eventSequence ) {
			event = ps->events[ i & (MAX_EVENTS-1) ];

			cent->currentState.event = event;
			cent->currentState.eventParm = ps->eventParms[ i & (MAX_EVENTS-1) ];
			CG_EntityEvent( cent, cent->lerpOrigin );
		}
	}
}

void CG_CheckPlayerstateEvents( playerState_t *ps, playerState_t *ops ) {
	int			i;
	int			event;
	centity_t	*cent;

	if ( ps->externalEvent && ps->externalEvent != ops->externalEvent ) {
		cent = &cg_entities[ ps->clientNum ];
		cent->currentState.event = ps->externalEvent;
		cent->currentState.eventParm = ps->externalEventParm;
		CG_EntityEvent( cent, cent->lerpOrigin );
	}

	cent = &cg.predictedPlayerEntity; // cg_entities[ ps->clientNum ];
	// go through the predictable events buffer
	for ( i = ps->eventSequence - MAX_EVENTS ; i < ps->eventSequence ; i++ ) {
		// if we have a new predictable event
		if ( i >= ops->eventSequence
			// or the server told us to play another event instead of a predicted event we already issued
			// or something the server told us changed our prediction causing a different event
			|| (i > ops->eventSequence - MAX_EVENTS && ps->events[i & (MAX_EVENTS-1)] != ops->events[i & (MAX_EVENTS-1)]) ) {

			event = ps->events[ i & (MAX_EVENTS-1) ];
			cent->currentState.event = event;
			cent->currentState.eventParm = ps->eventParms[ i & (MAX_EVENTS-1) ];
			CG_EntityEvent( cent, cent->lerpOrigin );

			cg.predictableEvents[ i & (MAX_PREDICTED_EVENTS-1) ] = event;

			cg.eventSequence++;
		}
	}
}

/*
==================
CG_CheckChangedPredictableEvents
==================
*/
void CG_CheckChangedPredictableEvents( playerState_t *ps ) {
	int i;
	int event;
	centity_t	*cent;

	cent = &cg.predictedPlayerEntity;
	for ( i = ps->eventSequence - MAX_EVENTS ; i < ps->eventSequence ; i++ ) {
		//
		if (i >= cg.eventSequence) {
			continue;
		}
		// if this event is not further back in than the maximum predictable events we remember
		if (i > cg.eventSequence - MAX_PREDICTED_EVENTS) {
			// if the new playerstate event is different from a previously predicted one
			if ( ps->events[i & (MAX_EVENTS-1)] != cg.predictableEvents[i & (MAX_PREDICTED_EVENTS-1) ] ) {

				event = ps->events[ i & (MAX_EVENTS-1) ];
				cent->currentState.event = event;
				cent->currentState.eventParm = ps->eventParms[ i & (MAX_EVENTS-1) ];
				CG_EntityEvent( cent, cent->lerpOrigin );

				cg.predictableEvents[ i & (MAX_PREDICTED_EVENTS-1) ] = event;

				if ( cg_showmiss.integer ) {
					CG_Printf("WARNING: changed predicted event\n");
				}
			}
		}
	}
}

/*
==================
CG_CheckLocalSounds
==================
*/
/* TC30056fc0: injury pulse replaces the SDK timelimit announcements. */
void CG_CheckLocalSounds(playerState_t *ps, playerState_t *ops) {
    if (ps->stats[STAT_HEALTH] < ops->stats[STAT_HEALTH] - 1 &&
        ps->stats[STAT_HEALTH] > 0) {
        CG_PainEvent(&cg.predictedPlayerEntity, ps->stats[STAT_HEALTH], qfalse);
        cg.painTime = cg.time;
    }
    if ((ps->holdable[2] > ops->holdable[2] + 60 ||
         ps->holdable[3] > ops->holdable[3] + 30 ||
         ps->holdable[4] > ops->holdable[4] + 30) && ps->stats[STAT_HEALTH] > 0) {
        cg.tceHeartbeatNext = cg.time;
        cg.tceHeartbeatUntil = cg.time + 7500;
    }
}

/*
===============
CG_TransitionPlayerState

===============
*/
void CG_TransitionPlayerState( playerState_t *ps, playerState_t *ops )
{
	// OSP - MV client handling
	if(cg.mvTotalClients > 0) {
		if (ps->clientNum != ops->clientNum) {
			cg.thisFrameTeleport = qtrue;

			// clear voicechat
			cg.predictedPlayerEntity.voiceChatSpriteTime = 0;	// CHECKME: should we do this here?
			cg_entities[ps->clientNum].voiceChatSpriteTime = 0;

			*ops = *ps;
		}
		CG_CheckLocalSounds( ps, ops );
		return;
	}

	// check for changing follow mode
	if ( ps->clientNum != ops->clientNum ) {
		cg.thisFrameTeleport = qtrue;

		// clear voicechat
		cg.predictedPlayerEntity.voiceChatSpriteTime = 0;
		cg_entities[ps->clientNum].voiceChatSpriteTime = 0;

		// make sure we don't get any unwanted transition effects
		*ops = *ps;

		// DHM - Nerve :: After Limbo, make sure and do a CG_Respawn
		if ( ps->clientNum == cg.clientNum )
			ops->persistant[PERS_SPAWN_COUNT]--;
	}

	if( ps->eFlags & EF_FIRING ) {
		cg.lastFiredWeaponTime = 0;
		cg.weaponFireTime += cg.frametime;
	} else {
		if( cg.weaponFireTime > 500 && cg.weaponFireTime ) {
			cg.lastFiredWeaponTime = cg.time;
		}

		cg.weaponFireTime = 0;
	}

	// damage events (player is getting wounded)
	if( ps->damageEvent != ops->damageEvent && ps->damageCount ) {
		CG_DamageFeedback( ps->damageYaw, ps->damagePitch, ps->damageCount );
	}

	// respawning
	if( ps->persistant[PERS_SPAWN_COUNT] != ops->persistant[PERS_SPAWN_COUNT] ) {
		CG_Respawn( ps->persistant[PERS_REVIVE_COUNT] != ops->persistant[PERS_REVIVE_COUNT] ? qtrue : qfalse );
	}

	if ( cg.mapRestart ) {
		CG_Respawn( qfalse );
		cg.mapRestart = qfalse;
	}

	if ( cg.snap->ps.pm_type != PM_INTERMISSION 
		&& ps->persistant[PERS_TEAM] != TEAM_SPECTATOR ) {
		CG_CheckLocalSounds( ps, ops );
	}

	// check for going low on ammo
	CG_CheckAmmo();

	if( ps->eFlags & EF_PRONE_MOVING ) {
		if( ps->weapon == 20 ) {
			if( ps->eFlags & EF_ZOOMING ) {
				trap_SendConsoleCommand( "-zoom\n" );
			}
		}

		if( !(ops->eFlags & EF_PRONE_MOVING) ) {
			// ydnar: this screws up auto-switching when dynamite planted or grenade thrown/out of ammo
			//%	CG_FinishWeaponChange( cg.weaponSelect, ps->nextWeapon );

			cg.proneMovingTime = cg.time;
		}
	} else if( ops->eFlags & EF_PRONE_MOVING ) {
		cg.proneMovingTime = -cg.time;
	}

	if( !(ps->eFlags & EF_PRONE) && ops->eFlags & EF_PRONE ) {
		if( cg.weaponSelect == 62 )
			CG_FinishWeaponChange( cg.weaponSelect, ps->nextWeapon );
	}

	// run events
	CG_CheckPlayerstateEvents( ps, ops );

	// smooth the ducking viewheight change
	if ( ps->viewheight != ops->viewheight && ps->persistant[PERS_TEAM] != TEAM_SPECTATOR ) {
		cg.duckChange = ps->viewheight - ops->viewheight;
		cg.duckTime = cg.time;
	}
	/* Original CG_TransitionPlayerState30057060: signed posture timers
	 * feed the complete weapon-position controller, independently of viewheight. */
	{
		if ((ps->pm_flags ^ ops->pm_flags) & PMF_LADDER) {
			if (ps->pm_flags & PMF_LADDER) cg.tceWeaponDuckTime=cg.time+50;
			else if (cg.tceWeaponDuckTime < cg.time) {
				int remaining=cg.tceWeaponDuckTime-cg.time+200;
				if (remaining<0) remaining=-50;
				cg.tceWeaponDuckTime=remaining-cg.time;
			}
		}
		if ((ps->eFlags ^ ops->eFlags)&EF_PRONE)
			cg.tceProneTime=(ps->eFlags&EF_PRONE)?cg.time:-cg.time;
		if ((ps->stats[STAT_TCE_WEAPON_FLAGS]^ops->stats[STAT_TCE_WEAPON_FLAGS])&0x4000)
			cg.tceStanceTime=(ps->stats[STAT_TCE_WEAPON_FLAGS]&0x4000)?cg.time:-cg.time;
	}

    if (!cg.tceAimActive) {
        if ((ps->stats[STAT_TCE_WEAPON_FLAGS] ^ ops->stats[STAT_TCE_WEAPON_FLAGS]) & 0x20) {
            if (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x20)
                cg.tceActionTransitionTime = cg.time + 200;
            else if (cg.tceActionTransitionTime < cg.time) {
                int remaining = cg.tceActionTransitionTime - cg.time + 200;
                if (remaining < 0) remaining = 0;
                cg.tceActionTransitionTime = remaining - cg.time;
            }
        }
    }

}

