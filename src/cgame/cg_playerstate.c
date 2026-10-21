

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
void CG_CheckAmmo(void)
{
	unsigned int weapons = (unsigned int)cg.snap->ps.weapons[0];
	int          weapon, total = 0;
	if (!weapons && !cg.snap->ps.weapons[1])
	{
		return;
	}
	for (weapon = 0; weapon < 64; ++weapon)
	{
		if (!(weapons & (1u << (weapon & 31))))
		{
			continue;
		}
		total += cg.snap->ps.ammo[BG_FindAmmoForWeapon(weapon)] * 1000;
		if (total >= 5000)
		{
			cg.lowAmmoWarning = 0; return;
		}
	}
	cg.lowAmmoWarning = total ? 1 : 2;
}

/*
==============
CG_DamageFeedback
==============
*/
void CG_DamageFeedback(int yawByte, int pitchByte, int damage)
{
	float        left, front, up;
	float        kick;
	int          health;
#if defined(_WIN32)
	double       scale;
#else
	float        scale;
#endif
	vec3_t       dir;
	vec3_t       angles;
	float        dist;
	float        yaw, pitch;
	int          slot;
	viewDamage_t *vd;

	// show the attacking player's head and name in corner
	cg.attackerTime = cg.time;

	// the lower on health you are, the greater the view kick will be
	health = cg.snap->ps.stats[STAT_HEALTH];
	if (health < 40)
	{
		scale = 1;
	}
	else
	{
		scale = 40.0 / health;
	}
	kick = damage * scale;

	if (kick < 5)
	{
		kick = 5;
	}
	if (kick > 10)
	{
		kick = 10;
	}

	// find a free slot
	for (slot = 0; slot < MAX_VIEWDAMAGE; slot++)
	{
		if (cg.viewDamage[slot].damageTime + cg.viewDamage[slot].damageDuration < cg.time)
		{
			break;
		}
	}

	if (slot == MAX_VIEWDAMAGE)
	{
		return;     // no free slots, never override or splats will suddenly disappear

	}
	vd = &cg.viewDamage[slot];

	// if yaw and pitch are both 255, make the damage always centered (falling, etc)
	if (yawByte == 255 && pitchByte == 255)
	{
		vd->damageX    = 0;
		vd->damageY    = 0;
		cg.v_dmg_roll  = 0;
		cg.v_dmg_pitch = -kick;
	}
	else
	{
		// positional
#if defined(_WIN32)
		pitch = (float)(pitchByte * (360.0 / 255.0));
		yaw   = (float)(yawByte * (360.0 / 255.0));
#else
		pitch = pitchByte / 255.0 * 360;
		yaw   = yawByte / 255.0 * 360;
#endif

		angles[PITCH] = pitch;
		angles[YAW]   = yaw;
		angles[ROLL]  = 0;

		AngleVectors(angles, dir, NULL, NULL);
		VectorSubtract(vec3_origin, dir, dir);

#if defined(_WIN32)
		/* Original30056a68..ae1 sums Z,Y,X in x87 before storing each float. */
		front = (float)(((double)dir[2] * cg.refdef.viewaxis[0][2] + (double)dir[1] * cg.refdef.viewaxis[0][1]) + (double)dir[0] * cg.refdef.viewaxis[0][0]);
		left  = (float)(((double)dir[2] * cg.refdef.viewaxis[1][2] + (double)dir[1] * cg.refdef.viewaxis[1][1]) + (double)dir[0] * cg.refdef.viewaxis[1][0]);
		up    = (float)(((double)dir[2] * cg.refdef.viewaxis[2][2] + (double)dir[1] * cg.refdef.viewaxis[2][1]) + (double)dir[0] * cg.refdef.viewaxis[2][0]);
#else
		/* Linux retains the SDK expressions; Windows arithmetic is not shared. */
		front = DotProduct(dir, cg.refdef.viewaxis[0]);
		left  = DotProduct(dir, cg.refdef.viewaxis[1]);
		up    = DotProduct(dir, cg.refdef.viewaxis[2]);
#endif

		dir[0] = front;
		dir[1] = left;
		dir[2] = 0;
		dist   = VectorLength(dir);
		if (dist < 0.1)
		{
			dist = 0.1;
		}

		cg.v_dmg_roll = kick * left;

		cg.v_dmg_pitch = -kick * front;

		if (front <= 0.1)
		{
			front = 0.1;
		}
#if defined(_WIN32)
		vd->damageX = (float)((((double)(rand() & 32767) * (double)(1.0f / 32767.0f) - .5) * 2.0) * .3 - (double)left / front);
		vd->damageY = (float)((((double)(rand() & 32767) * (double)(1.0f / 32767.0f) - .5) * 2.0) * .3 + (double)up / dist);
#else
		vd->damageX = crandom() * 0.3 + -left / front;
		vd->damageY = crandom() * 0.3 + up / dist;
#endif
	}

	// clamp the position
	if (vd->damageX > 1.0)
	{
		vd->damageX = 1.0;
	}
	if (vd->damageX < -1.0)
	{
		vd->damageX = -1.0;
	}

	if (vd->damageY > 1.0)
	{
		vd->damageY = 1.0;
	}
	if (vd->damageY < -1.0)
	{
		vd->damageY = -1.0;
	}

	// don't let the screen flashes vary as much
	if (kick > 10)
	{
		kick = 10;
	}
	vd->damageValue    = kick;
	cg.v_dmg_time      = cg.time + DAMAGE_TIME;
	vd->damageTime     = cg.snap->serverTime;
	vd->damageDuration = kick * 50 * (1 + 2 * (!vd->damageX && !vd->damageY));
	cg.damageTime      = cg.snap->serverTime;
	cg.damageIndex     = slot;
}




/*
================
CG_Respawn

A respawn happened this snapshot
================
*/
/* The two original builds reset the same TC state in different orders.
 * Keep those orders around the real animation, command, cvar and fog calls. */
static void CG_ResetRespawnEffects(void)
{
	int firemodeTime;
	int preservedZoneEffect;

#if defined(_WIN32)
	firemodeTime                  = cg.time;
	preservedZoneEffect           = cg.tceSoundZoneEffects[1][1];
	cg.tceShotHoldUntil           = firemodeTime;
	cg.tceSoundZoneEffects[1][0]  = preservedZoneEffect;
	cg.tceSoundZoneEffects[0][1]  = preservedZoneEffect;
	cg.tceSoundZoneEffects[0][0]  = preservedZoneEffect;
	cg.tceFiremodeTime            = firemodeTime;
	cg.binocZoomTime              = 0;
	cg.tceScopeBlocked            = 0;
	cg.tceAimRequested            = 0;
	cg.tceAimComplete             = 0;
	cg.tceAimActive               = 0;
	cg.tceTacticalScale           = 0.0f;
	cg.tceAimWeaponLatch          = 0;
	cg.tceRoomDistance            = 0.0f;
	cg.tceSoundSampleTime         = 0;
	cg.tceRoomExtent[0]           = 0.0f;
	cg.tceRoomExtent[1]           = 0.0f;
	cg.tceRoomExtent[2]           = 0.0f;
	cg.tceSoundLastOrigin[0]      = 0.0f;
	cg.tceSoundLastOrigin[1]      = 0.0f;
	cg.tceSoundLastOrigin[2]      = 0.0f;
	cg.tceRoomWeights[3]          = 0.0f;
	cg.tceRoomWeights[2]          = 0.0f;
	cg.tceRoomWeights[1]          = 0.0f;
	cg.tceRoomWeights[0]          = 0.0f;
	cg.tceSoundEnvironment        = 0;
	tceFlash.blindActive          = 0;
	tceFlash.blindUntil           = 0;
	cg.tceCoronaBlendAlpha        = 0.0f;
	cg.tceEyeSmooth               = 0.0f;
	cg.tceEyeLinear               = 0.0f;
	cg.tceScopeLightBoost         = 1.0f;
	cg.tceEyeFlareFrame           = 0;
	cg.tceEyeFlare                = 0.0f;
	cg.tceEyeSampleTime           = 0;
	tceFlash.deafUntil            = 0;
	tceFlash.deafness             = 0.0f;
	cg.tceSoundZoneTransitionTime = 0;
	cg.tceSoundZoneActive         = 0;
	cg.tceSoundZoneSounds[1][1]   = 0;
	cg.tceSoundZoneSounds[1][0]   = 0;
	cg.tceSoundZoneSounds[0][1]   = 0;
	cg.tceSoundZoneSounds[0][0]   = 0;
	cg.tceSoundZoneVolume[1]      = 0;
	cg.tceSoundZoneVolume[0]      = 0;
	cg.tceFiremodeAnimationTime   = -1000;
#else
	cg.tceScopeBlocked            = 0;
	cg.binocZoomTime              = 0;
	cg.tceAimRequested            = 0;
	cg.tceAimActive               = 0;
	cg.tceAimComplete             = 0;
	cg.tceAimWeaponLatch          = 0;
	cg.tceSoundSampleTime         = 0;
	cg.tceTacticalScale           = 0.0f;
	cg.tceRoomDistance            = 0.0f;
	cg.tceRoomExtent[0]           = 0.0f;
	cg.tceRoomExtent[1]           = 0.0f;
	cg.tceRoomExtent[2]           = 0.0f;
	cg.tceSoundLastOrigin[0]      = 0.0f;
	cg.tceSoundLastOrigin[1]      = 0.0f;
	cg.tceSoundLastOrigin[2]      = 0.0f;
	cg.tceRoomWeights[3]          = 0.0f;
	cg.tceRoomWeights[2]          = 0.0f;
	cg.tceRoomWeights[1]          = 0.0f;
	cg.tceRoomWeights[0]          = 0.0f;
	cg.tceSoundEnvironment        = 0;
	tceFlash.blindUntil           = 0;
	firemodeTime                  = cg.time;
	tceFlash.blindActive          = 0;
	cg.tceCoronaBlendAlpha        = 0.0f;
	cg.tceEyeSmooth               = 0.0f;
	cg.tceEyeLinear               = 0.0f;
	cg.tceShotHoldUntil           = firemodeTime;
	cg.tceScopeLightBoost         = 1.0f;
	cg.tceEyeFlareFrame           = 0;
	cg.tceEyeSampleTime           = 0;
	tceFlash.deafUntil            = 0;
	cg.tceSoundZoneTransitionTime = 0;
	cg.tceSoundZoneSounds[1][1]   = 0;
	cg.tceSoundZoneSounds[1][0]   = 0;
	cg.tceSoundZoneSounds[0][1]   = 0;
	cg.tceSoundZoneVolume[1]      = 0;
	preservedZoneEffect           = cg.tceSoundZoneEffects[1][1];
	cg.tceSoundZoneSounds[0][0]   = 0;
	cg.tceSoundZoneVolume[0]      = 0;
	cg.tceEyeFlare                = 0.0f;
	tceFlash.deafness             = 0.0f;
	cg.tceSoundZoneActive         = 0;
	cg.tceSoundZoneEffects[1][0]  = preservedZoneEffect;
	cg.tceSoundZoneEffects[0][1]  = preservedZoneEffect;
	cg.tceSoundZoneEffects[0][0]  = preservedZoneEffect;
	cg.tceFiremodeTime            = firemodeTime;
	cg.tceFiremodeAnimationTime   = -1000;
#endif
}

void CG_Respawn(qboolean revived)
{
	snapshot_t *snapshot = cg.snap;
	int        animation;
	int        idleAnimation;
	int        selectedWeapon;
#if !defined(_WIN32)
	int selectionTime;
#endif
	int autoReload;

	cg.thisFrameTeleport = qtrue;
#if defined(_WIN32)
	cg.serverRespawning                 = qfalse;
	idleAnimation                       = PM_IdleAnimForWeapon(snapshot->ps.weapon);
	animation                           = cg.predictedPlayerState.weapAnim;
	snapshot                            = cg.snap;
	cg.predictedPlayerState.weaponstate = WEAPON_READY;
	cg.cursorHintIcon                   = 0;
	cg.predictedPlayerState.weapAnim    = idleAnimation | (~animation & ANIM_TOGGLEBIT);
	cg.weaponSelectTime                 = cg.time;
	cg.cursorHintTime                   = 0;
	cg.cameraMode                       = qfalse;
	selectedWeapon                      = snapshot->ps.weapon;
	cg.weaponSelect                     = selectedWeapon;
	cg.zoomedBinoc                      = qfalse;
#else
	/* Linux retains the toggle sampled before the IdleAnim callback. */
	animation                        = cg.predictedPlayerState.weapAnim;
	cg.serverRespawning              = qfalse;
	idleAnimation                    = PM_IdleAnimForWeapon(snapshot->ps.weapon);
	cg.predictedPlayerState.weapAnim =
		((animation & ANIM_TOGGLEBIT) ^ ANIM_TOGGLEBIT) | idleAnimation;
	cg.cursorHintIcon                   = 0;
	selectionTime                       = cg.time;
	snapshot                            = cg.snap;
	cg.predictedPlayerState.weaponstate = WEAPON_READY;
	cg.cursorHintTime                   = 0;
	cg.cameraMode                       = qfalse;
	cg.weaponSelectTime                 = selectionTime;
	selectedWeapon                      = snapshot->ps.weapon;
	cg.zoomedBinoc                      = qfalse;
	cg.weaponSelect                     = selectedWeapon;
#endif
	cg.zoomedScope = qfalse;
	cg.zoomTime    = 0;
	cg.zoomval     = 0.0f;
	trap_SendConsoleCommand("-zoom\n");

	CG_ResetRespawnEffects();
	/* Reset the original movement state and our pending freelook-recoil tail. */
	memset(&cg.pmext, 0, sizeof(cg.pmext));
	autoReload           = cg_autoReload.integer;
	cg.pmext.sprintTime  = SPRINTTIME;
	cg.pmext.bAutoReload = autoReload > 0;
	if (!revived)
	{
		cgs.limboLoadoutSelected = qfalse;
	}

	cg.proneMovingTime = 0;
#if defined(_WIN32)
	cg.tceStanceTime           = 0;
	cg.tceActionTransitionTime = 0;
#else
	cg.tceActionTransitionTime = 0;
	cg.tceStanceTime           = 0;
#endif
	cg.tceHeartbeatNext  = 0;
	cg.tceHeartbeatUntil = 0;
	cg.tceAdsBreathTime  = 0;
	trap_Cvar_Set("r_ambientscale", "1.3");
	trap_R_SetFog(FOG_CMD_SWITCHFOG, FOG_MAP, 20, 0.0f, 0.0f, 0.0f, 0.0f);
}


extern char *eventnames[];

/*
==============
CG_CheckPlayerstateEvents
==============
*/
void CG_CheckPlayerstateEvents_wolf(playerState_t *ps, playerState_t *ops)
{
	int       i;
	int       event;
	centity_t *cent;
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
	for ( i = ps->eventSequence - MAX_EVENTS ; i < ps->eventSequence ; i++ )
	{
		if (ps->events[i & (MAX_EVENTS - 1)] != ops->events[i & (MAX_EVENTS - 1)]
		    || i >= ops->eventSequence)
		{
			event = ps->events[i & (MAX_EVENTS - 1)];

			cent->currentState.event     = event;
			cent->currentState.eventParm = ps->eventParms[i & (MAX_EVENTS - 1)];
			CG_EntityEvent(cent, cent->lerpOrigin);
		}
	}
}

void CG_CheckPlayerstateEvents(playerState_t *ps, playerState_t *ops)
{
	int       i;
	int       event;
	centity_t *cent;

	if (ps->externalEvent && ps->externalEvent != ops->externalEvent)
	{
		cent                         = &cg_entities[ps->clientNum];
		cent->currentState.event     = ps->externalEvent;
		cent->currentState.eventParm = ps->externalEventParm;
		CG_EntityEvent(cent, cent->lerpOrigin);
	}

	cent = &cg.predictedPlayerEntity; // cg_entities[ ps->clientNum ];
	// go through the predictable events buffer
	for ( i = ps->eventSequence - MAX_EVENTS ; i < ps->eventSequence ; i++ )
	{
		// if we have a new predictable event
		if (i >= ops->eventSequence
		    // or the server told us to play another event instead of a predicted event we already issued
		    // or something the server told us changed our prediction causing a different event
		    || (i > ops->eventSequence - MAX_EVENTS && ps->events[i & (MAX_EVENTS - 1)] != ops->events[i & (MAX_EVENTS - 1)]))
		{

			event                        = ps->events[i & (MAX_EVENTS - 1)];
			cent->currentState.event     = event;
			cent->currentState.eventParm = ps->eventParms[i & (MAX_EVENTS - 1)];
			CG_EntityEvent(cent, cent->lerpOrigin);

			cg.predictableEvents[i & (MAX_PREDICTED_EVENTS - 1)] = event;

			cg.eventSequence++;
		}
	}
}

/*
==================
CG_CheckChangedPredictableEvents
==================
*/
void CG_CheckChangedPredictableEvents(playerState_t *ps)
{
	int       i;
	int       event;
	centity_t *cent;

	cent = &cg.predictedPlayerEntity;
	for ( i = ps->eventSequence - MAX_EVENTS ; i < ps->eventSequence ; i++ )
	{
		//
		if (i >= cg.eventSequence)
		{
			continue;
		}
		// if this event is not further back in than the maximum predictable events we remember
		if (i > cg.eventSequence - MAX_PREDICTED_EVENTS)
		{
			// if the new playerstate event is different from a previously predicted one
			if (ps->events[i & (MAX_EVENTS - 1)] != cg.predictableEvents[i & (MAX_PREDICTED_EVENTS - 1)])
			{

				event                        = ps->events[i & (MAX_EVENTS - 1)];
				cent->currentState.event     = event;
				cent->currentState.eventParm = ps->eventParms[i & (MAX_EVENTS - 1)];
				CG_EntityEvent(cent, cent->lerpOrigin);

				cg.predictableEvents[i & (MAX_PREDICTED_EVENTS - 1)] = event;

				if (cg_showmiss.integer)
				{
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
void CG_CheckLocalSounds(playerState_t *ps, playerState_t *ops)
{
	if (ps->stats[STAT_HEALTH] < ops->stats[STAT_HEALTH] - 1 &&
	    ps->stats[STAT_HEALTH] > 0)
	{
		CG_PainEvent(&cg.predictedPlayerEntity, ps->stats[STAT_HEALTH], qfalse);
		cg.painTime = cg.time;
	}
	if ((ps->holdable[2] > ops->holdable[2] + 60 ||
	     ps->holdable[3] > ops->holdable[3] + 30 ||
	     ps->holdable[4] > ops->holdable[4] + 30) && ps->stats[STAT_HEALTH] > 0)
	{
		cg.tceHeartbeatNext  = cg.time;
		cg.tceHeartbeatUntil = cg.time + 7500;
	}
}

/*
===============
CG_TransitionPlayerState

===============
*/
void CG_TransitionPlayerState(playerState_t *ps, playerState_t *ops)
{
	// OSP - MV client handling
	if (cg.mvTotalClients > 0)
	{
		if (ps->clientNum != ops->clientNum)
		{
			cg.thisFrameTeleport = qtrue;

			// clear voicechat
			cg.predictedPlayerEntity.voiceChatSpriteTime   = 0; // CHECKME: should we do this here?
			cg_entities[ps->clientNum].voiceChatSpriteTime = 0;

			*ops = *ps;
		}
		CG_CheckLocalSounds(ps, ops);
		return;
	}

	// check for changing follow mode
	if (ps->clientNum != ops->clientNum)
	{
		cg.thisFrameTeleport = qtrue;

		// clear voicechat
		cg.predictedPlayerEntity.voiceChatSpriteTime   = 0;
		cg_entities[ps->clientNum].voiceChatSpriteTime = 0;

		// make sure we don't get any unwanted transition effects
		*ops = *ps;

		// DHM - Nerve :: After Limbo, make sure and do a CG_Respawn
		if (ps->clientNum == cg.clientNum)
		{
			ops->persistant[PERS_SPAWN_COUNT]--;
		}
	}

	if (ps->eFlags & EF_FIRING)
	{
		cg.lastFiredWeaponTime = 0;
		cg.weaponFireTime     += cg.frametime;
	}
	else
	{
		if (cg.weaponFireTime > 500 && cg.weaponFireTime)
		{
			cg.lastFiredWeaponTime = cg.time;
		}

		cg.weaponFireTime = 0;
	}

	// damage events (player is getting wounded)
	if (ps->damageEvent != ops->damageEvent && ps->damageCount)
	{
		CG_DamageFeedback(ps->damageYaw, ps->damagePitch, ps->damageCount);
	}

	// respawning
	if (ps->persistant[PERS_SPAWN_COUNT] != ops->persistant[PERS_SPAWN_COUNT])
	{
		CG_Respawn(ps->persistant[PERS_REVIVE_COUNT] != ops->persistant[PERS_REVIVE_COUNT] ? qtrue : qfalse);
	}

	if (cg.mapRestart)
	{
		CG_Respawn(qfalse);
		cg.mapRestart = qfalse;
	}

	if (cg.snap->ps.pm_type != PM_INTERMISSION
	    && ps->persistant[PERS_TEAM] != TEAM_SPECTATOR)
	{
		CG_CheckLocalSounds(ps, ops);
	}

	// check for going low on ammo
	CG_CheckAmmo();

	if (ps->eFlags & EF_PRONE_MOVING)
	{
		if (ps->weapon == 20)
		{
			if (ps->eFlags & EF_ZOOMING)
			{
				trap_SendConsoleCommand("-zoom\n");
			}
		}

		if (!(ops->eFlags & EF_PRONE_MOVING))
		{
			// ydnar: this screws up auto-switching when dynamite planted or grenade thrown/out of ammo
			//%	CG_FinishWeaponChange( cg.weaponSelect, ps->nextWeapon );

			cg.proneMovingTime = cg.time;
		}
	}
	else if (ops->eFlags & EF_PRONE_MOVING)
	{
		cg.proneMovingTime = -cg.time;
	}

	if (!(ps->eFlags & EF_PRONE) && ops->eFlags & EF_PRONE)
	{
		if (cg.weaponSelect == 62)
		{
			CG_FinishWeaponChange(cg.weaponSelect, ps->nextWeapon);
		}
	}

	// run events
	CG_CheckPlayerstateEvents(ps, ops);

	// smooth the ducking viewheight change
	if (ps->viewheight != ops->viewheight && ps->persistant[PERS_TEAM] != TEAM_SPECTATOR)
	{
		cg.duckChange = ps->viewheight - ops->viewheight;
		cg.duckTime   = cg.time;
	}
	/* Original CG_TransitionPlayerState30057060: signed posture timers
	 * feed the complete weapon-position controller, independently of viewheight. */
	{
		if ((ps->pm_flags ^ ops->pm_flags) & PMF_LADDER)
		{
			if (ps->pm_flags & PMF_LADDER)
			{
				cg.tceWeaponDuckTime = cg.time + 50;
			}
			else if (cg.tceWeaponDuckTime < cg.time)
			{
				int remaining = cg.tceWeaponDuckTime - cg.time + 200;
				if (remaining < 0)
				{
					remaining = -50;
				}
				cg.tceWeaponDuckTime = remaining - cg.time;
			}
		}
		if ((ps->eFlags ^ ops->eFlags) & EF_PRONE)
		{
			cg.tceProneTime = (ps->eFlags & EF_PRONE)?cg.time:-cg.time;
		}
		if ((ps->stats[STAT_TCE_WEAPON_FLAGS] ^ ops->stats[STAT_TCE_WEAPON_FLAGS]) & 0x4000)
		{
			cg.tceStanceTime = (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x4000)?cg.time:-cg.time;
		}
	}

	if (!cg.tceAimActive)
	{
		if ((ps->stats[STAT_TCE_WEAPON_FLAGS] ^ ops->stats[STAT_TCE_WEAPON_FLAGS]) & 0x20)
		{
			if (ps->stats[STAT_TCE_WEAPON_FLAGS] & 0x20)
			{
				cg.tceActionTransitionTime = cg.time + 200;
			}
			else if (cg.tceActionTransitionTime < cg.time)
			{
				int remaining = cg.tceActionTransitionTime - cg.time + 200;
				if (remaining < 0)
				{
					remaining = 0;
				}
				cg.tceActionTransitionTime = remaining - cg.time;
			}
		}
	}

}
