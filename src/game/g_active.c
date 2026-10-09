
#include "g_local.h"
#ifdef FEATURE_OMNIBOT
#include "g_etbot_interface.h"
#endif
#include "tce_node_editor.h"

/*
===============
G_DamageFeedback

Called just before a snapshot is sent to the given player.
Totals up all damage and generates both the player_state_t
damage values to that client for pain blends and kicks, and
global pain sound events for all clients.
===============
*/
void P_DamageFeedback( gentity_t *player ) {
	gclient_t	*client;
	float	count;
	vec3_t	angles;

	client = player->client;
	if ( client->ps.pm_type == PM_DEAD ) {
		return;
	}

	// total points of damage shot at the player this frame
	count = client->damage_blood;
	if ( count == 0 ) {
		return;		// didn't take any damage
	}

	if ( count > 127 ) {
		count = 127;
	}

	// send the information to the client

	// world damage (falling, slime, etc) uses a special code
	// to make the blend blob centered instead of positional
	if ( client->damage_fromWorld ) {
		client->ps.damagePitch = 255;
		client->ps.damageYaw = 255;

		client->damage_fromWorld = qfalse;
	} else {
		vectoangles( client->damage_from, angles );
		/* Windows20044849/20044861 multiply the stored double256/360
		 * before truncation; do not introduce an intermediate division. */
		client->ps.damagePitch = (double)angles[PITCH] * (256.0 / 360.0);
		client->ps.damageYaw = (double)angles[YAW] * (256.0 / 360.0);
	}

	// play an apropriate pain sound
	if ( (level.time > player->pain_debounce_time) && !(player->flags & FL_GODMODE) && !(player->s.powerups & PW_INVULNERABLE) ) {	//----(SA)	
		player->pain_debounce_time = level.time + 700;
		G_AddEvent( player, EV_PAIN, player->health );
	}

	client->ps.damageEvent++;	// Ridah, always increment this since we do multiple view damage anims

	client->ps.damageCount = count;

	//
	// clear totals
	//
	client->damage_blood = 0;
	client->damage_knockback = 0;
}


#define MIN_BURN_INTERVAL 399 // JPW NERVE set burn timeinterval so we can do more precise damage (was 199 old model)

/*
=============
P_WorldEffects

Check for lava / slime contents and drowning
=============
*/
void P_WorldEffects( gentity_t *ent ) {
	int			waterlevel;

	if ( ent->client->noclip ) {
		ent->client->airOutTime = level.time + 12000;	// don't need air
		return;
	}

	waterlevel = ent->waterlevel;

	//
	// check for drowning
	//
	if ( waterlevel == 3 ) {
		// if out of air, start drowning
		if ( ent->client->airOutTime < level.time) {

			if(ent->client->ps.powerups[PW_BREATHER]) {	// take air from the breather now that we need it
				ent->client->ps.powerups[PW_BREATHER] -= (level.time - ent->client->airOutTime);
				ent->client->airOutTime = level.time + (level.time - ent->client->airOutTime);
			}
			else {


				// drown!
				ent->client->airOutTime += 1000;
				if ( ent->health > 0 ) {
					// take more damage the longer underwater
					ent->damage += 2;
					if (ent->damage > 15)
						ent->damage = 15;

					// play a gurp sound instead of a normal pain sound
					if (ent->health <= ent->damage) {
						G_Sound(ent, G_SoundIndex("*drown.wav"));
					} else if (rand()&1) {
						G_Sound(ent, G_SoundIndex("sound/player/gurp1.wav"));
					} else {
						G_Sound(ent, G_SoundIndex("sound/player/gurp2.wav"));
					}

					// don't play a normal pain sound
					ent->pain_debounce_time = level.time + 200;

					G_Damage (ent, NULL, NULL, NULL, NULL, ent->damage, 0, MOD_WATER);
				}
			}
		}
	} else {
		ent->client->airOutTime = level.time + 12000;
		ent->damage = 2;
	}

	//
	// check for sizzle damage (move to pmove?)
	//
	if (waterlevel && (ent->watertype&CONTENTS_LAVA) ) {
		if (ent->health > 0	&& ent->pain_debounce_time <= level.time ) {

				if (ent->watertype & CONTENTS_LAVA) {
					G_Damage (ent, NULL, NULL, NULL, NULL, 
						30*waterlevel, 0, MOD_LAVA);
				}

		}
	}

	//
	// check for burning from flamethrower
	//
	// JPW NERVE MP way
	if (ent->s.onFireEnd && ent->client) {
		if (level.time - ent->client->lastBurnTime >= MIN_BURN_INTERVAL) { 

			// JPW NERVE server-side incremental damage routine / player damage/health is int (not float)
			// so I can't allocate 1.5 points per server tick, and 1 is too weak and 2 is too strong.  
			// solution: allocate damage far less often (MIN_BURN_INTERVAL often) and do more damage.
			// That way minimum resolution (1 point) damage changes become less critical.

			ent->client->lastBurnTime = level.time;
			if ((ent->s.onFireEnd > level.time) && (ent->health > 0)) {
				gentity_t *attacker;
   				attacker = g_entities + ent->flameBurnEnt;
				G_Damage (ent, attacker, attacker, NULL, NULL, 5, DAMAGE_NO_KNOCKBACK, MOD_FLAMETHROWER); // JPW NERVE was 7
			}
		}
	}
	// jpw
}



/*
===============
G_SetClientSound
===============
*/
void G_SetClientSound( gentity_t *ent ) {
/*	if (ent->waterlevel && (ent->watertype & CONTENTS_LAVA) )	//----(SA)	modified since slime is no longer deadly
		ent->s.loopSound = level.snd_fry;
	else*/ // Gordon: doesnt exist
		ent->s.loopSound = 0;
}

/*
==============
PushBot
==============
*/
void BotVoiceChatAfterIdleTime( int client, const char *id, int mode, int delay, qboolean voiceonly, int idleTime, qboolean forceIfDead );

void PushBot( gentity_t *ent, gentity_t *other ) {
	vec3_t dir, ang, f, r;
	float oldspeed;
#if defined(_MSC_VER) && defined(_M_IX86)
	float *pushVelocity = other->client->ps.velocity;
	float pushMinimum = 200.0f;
	float pushVoiceLimit = 10.0f;
	double pushSideScale = 100.0, pushSign;
	float *pushComponent, *pushOrigin, *pushOtherOrigin;
	int pushAxis, pushPhase;
	unsigned short pushCompare;
#endif
	//
#if defined(_MSC_VER) && defined(_M_IX86)
	/* TC 20044b18 stores f32, but compares the retained return in ST0. */
	__asm {
		push pushVelocity
		call VectorLength
		add esp, 4
		fst oldspeed
		fcomp pushMinimum
		fnstsw ax
		mov pushCompare, ax
	}
	if (pushCompare & 0x100) oldspeed = 200.0f;
#else
	oldspeed = VectorLength( other->client->ps.velocity );
	if (oldspeed < 200)
		oldspeed = 200;
#endif
	//
#if defined(_MSC_VER) && defined(_M_IX86)
	for (pushAxis = 0; pushAxis < 3; ++pushAxis) {
		pushOrigin = &ent->r.currentOrigin[pushAxis];
		pushOtherOrigin = &other->r.currentOrigin[pushAxis];
		pushComponent = &dir[pushAxis];
		__asm {
			mov eax, pushOtherOrigin
			fld dword ptr [eax]
			mov eax, pushOrigin
			fsub dword ptr [eax]
			mov eax, pushComponent
			fstp dword ptr [eax]
		}
	}
	pushComponent = dir;
	__asm {
		push pushComponent
		call VectorNormalize
		add esp, 4
		fstp st(0)
	}
#else
	VectorSubtract( other->r.currentOrigin, ent->r.currentOrigin, dir );
	VectorNormalize( dir );
#endif
	vectoangles( dir, ang );
	AngleVectors( ang, f, r, NULL );
	f[2] = 0;
	r[2] = 0;
	//
#if defined(_MSC_VER) && defined(_M_IX86)
	/* TC: each multiply/add remains in ST0 until its component store. */
	for (pushAxis = 0; pushAxis < 3; ++pushAxis) {
		pushComponent = &f[pushAxis];
		pushVelocity = &other->client->ps.velocity[pushAxis];
		__asm {
			mov eax, pushComponent
			fld dword ptr [eax]
			fmul pushMinimum
			mov eax, pushVelocity
			fadd dword ptr [eax]
			fstp dword ptr [eax]
		}
	}
	for (pushAxis = 0; pushAxis < 3; ++pushAxis) {
		/* Original LEA arithmetic wraps at32 bits before signed IDIV. */
		pushPhase = (int)((unsigned int)level.time + (unsigned int)ent->s.number * 1000u);
		pushSign = pushPhase % 4000 < 2000 ? 1.0 : -1.0;
		pushComponent = &r[pushAxis];
		pushVelocity = &other->client->ps.velocity[pushAxis];
		__asm {
			fld pushSign
			fmul pushSideScale
			mov eax, pushComponent
			fmul dword ptr [eax]
			mov eax, pushVelocity
			fadd dword ptr [eax]
			fstp dword ptr [eax]
		}
	}
#else
	VectorMA( other->client->ps.velocity, 200, f, other->client->ps.velocity );
	VectorMA( other->client->ps.velocity, 100 * ((level.time+(ent->s.number*1000))%4000 < 2000 ? 1.0 : -1.0), r, other->client->ps.velocity );
#endif
	//
#if defined(_MSC_VER) && defined(_M_IX86)
	/* TC 20044cce..20044ce5: both operands remain in x87, C0 also
	 * includes unordered. Do not narrow the returned squared length. */
	pushVelocity = other->client->ps.velocity;
	__asm {
		push pushVelocity
		call VectorLengthSquared
		add esp, 4
		fld oldspeed
		fmul oldspeed
		fcompp
		fnstsw ax
		mov pushCompare, ax
	}
	if (pushCompare & 0x100) {
#else
	if (VectorLengthSquared( other->client->ps.velocity ) > SQR(oldspeed)) {
#endif
#if defined(_MSC_VER) && defined(_M_IX86)
		pushVelocity = other->client->ps.velocity;
		__asm {
			push pushVelocity
			call VectorNormalize
			add esp, 4
			fstp st(0)
		}
		for (pushAxis = 0; pushAxis < 3; ++pushAxis) {
			pushVelocity = &other->client->ps.velocity[pushAxis];
			__asm {
				fld oldspeed
				mov eax, pushVelocity
				fmul dword ptr [eax]
				fstp dword ptr [eax]
			}
		}
#else
		VectorNormalize( other->client->ps.velocity );
		VectorScale( other->client->ps.velocity, oldspeed, other->client->ps.velocity );
#endif
	}
	//
	// also, if "ent" is a bot, tell "other" to move!
	if (rand()%50 == 0 && (ent->r.svFlags & SVF_BOT)) {
#if defined(_MSC_VER) && defined(_M_IX86)
		__asm {
			fld oldspeed
			fcomp pushVoiceLimit
			fnstsw ax
			mov pushCompare, ax
		}
		if (!(pushCompare & 0x100)) return;
#else
		if (!(oldspeed < 10)) return;
#endif
		BotVoiceChatAfterIdleTime( ent->s.number, "Move", SAY_TEAM, 1000, qfalse, 20000, qfalse );
	}
}

/*
==============
ClientNeedsAmmo
==============
*/
qboolean ClientNeedsAmmo( int client ) {
	return AddMagicAmmo( &g_entities[client], 0 ) ? qtrue : qfalse;
}

// Does ent have enough "energy" to call artillery?
qboolean ReadyToCallArtillery( gentity_t* ent ) {
	if( ent->client->sess.skill[SK_SIGNALS] >= 2 ) {
		/* TC 20044ddd: integer loads remain wide through the comparison. */
		if( (double)(level.time - ent->client->ps.classWeaponTime) <= (double)level.lieutenantChargeTime[ent->client->sess.sessionTeam-1] * (double)0.66f )
			return qfalse;
	} else if( level.time - ent->client->ps.classWeaponTime <= level.lieutenantChargeTime[ent->client->sess.sessionTeam-1] ) {
		return qfalse;
	}

	return qtrue;
}


// Are we ready to construct?  Optionally, will also update the time while we are constructing
qboolean ReadyToConstruct(gentity_t *ent, gentity_t *constructible, qboolean updateState)
{
	int weaponTime = ent->client->ps.classWeaponTime;
	double charge;

	// "Ammo" for this weapon is time based
	if( weaponTime + level.engineerChargeTime[ent->client->sess.sessionTeam-1] < level.time ) {
		weaponTime = level.time - level.engineerChargeTime[ent->client->sess.sessionTeam-1];
	}

	/* TC 20044e69..20044eb4: retain the original float reciprocal and
	 * multiplication order, adding the integer timestamp before conversion. */
	charge = (double)level.engineerChargeTime[ent->client->sess.sessionTeam-1] /
		((double)constructible->constructibleStats.duration * (double)0.01f);
	if (g_debugConstruct.integer) {
		charge *= (double)0.5f;
	} else {
		charge *= (double)constructible->constructibleStats.chargebarreq;
		if (ent->client->sess.skill[SK_EXPLOSIVES_AND_CONSTRUCTION] >= 3)
			charge *= (double)0.66f;
	}
	weaponTime = (int)(charge + (double)weaponTime);

	// if the time is in the future, we have NO energy left
	if (weaponTime > level.time)
	{
		// if we're supposed to update the state, reset the time to now
//		if( updateState )
//			ent->client->ps.classWeaponTime = level.time;

		return qfalse;
	}

	// only set the actual weapon time for this entity if they want us to
	if( updateState )
		ent->client->ps.classWeaponTime = weaponTime;

	return qtrue;
}

void BotSetBlockEnt( int client, int blocker );
/*
==============
CheckBotImpacts
==============
*/
void CheckBotImpacts( gentity_t *ent, gentity_t *other ) {
	char *blockEnts[] = {"func_explosive", NULL};
	int j;

	for (j=0; blockEnts[j]; j++) {
		if (other->classname && !Q_stricmp( other->classname, blockEnts[j] )) {
			BotSetBlockEnt( ent->s.number, other->s.number );
		}
	}
}

//==============================================================

/*
==============
ClientImpacts
==============
*/
void ClientImpacts( gentity_t *ent, pmove_t *pm ) {
	int		i, j;
	gentity_t	*other;
	trace_t	trace;

	memset( &trace, 0, sizeof(trace) );
	for (i=0 ; i<pm->numtouch ; i++) {
		for (j=0 ; j<i ; j++) {
			if (pm->touchents[j] == pm->touchents[i] ) {
				break;
			}
		}
		if (j != i) {
			continue;	// duplicated
		}
		other = &g_entities[ pm->touchents[i] ];

		if ( ( ent->r.svFlags & SVF_BOT ) && ( ent->touch ) ) {
			ent->touch( ent, other, &trace );
		}

		// RF, bot should get pushed out the way
		if ( (ent->client) /*&& !(ent->r.svFlags & SVF_BOT)*/ && (other->r.svFlags & SVF_BOT)
#ifdef FEATURE_OMNIBOT
            && !Bot_Interface_IsOmnibot((int)(other-g_entities))
#endif
        ) {
/*			vec3_t dir;
			// if we are not heading for them, ignore
			VectorSubtract( other->r.currentOrigin, ent->r.currentOrigin, dir );
			VectorNormalize( dir );
			if (DotProduct( ent->client->ps.velocity, dir ) > 0) {
				PushBot( ent, other );
			}
*/
			PushBot( ent, other );
		}

		// if we are standing on their head, then we should be pushed also
		if ( (ent->r.svFlags & SVF_BOT) && ent->s.groundEntityNum == other->s.number && other->client
#ifdef FEATURE_OMNIBOT
            && !Bot_Interface_IsOmnibot((int)(ent-g_entities))
#endif
        ) {
			PushBot( other, ent );
		}

		if ( (ent->r.svFlags & SVF_BOT)
#ifdef FEATURE_OMNIBOT
            && !Bot_Interface_IsOmnibot((int)(ent-g_entities))
#endif
        ) {
			CheckBotImpacts( ent, other );
		}

		if ( !other->touch ) {
			continue;
		}

		other->touch( other, ent, &trace );
	}

}

/*
============
G_TouchTriggers

Find all trigger entities that ent's current position touches.
Spectators will only interact with teleporters.
============
*/
void	G_TouchTriggers( gentity_t *ent ) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	trace_t		trace;
	vec3_t		mins, maxs;
	static vec3_t	range = { 40, 40, 52 };

	if ( !ent->client ) {
		return;
	}

	// Arnout: reset the pointer that keeps track of trigger_objective_info tracking
	ent->client->touchingTOI = NULL;
	ent->client->tceDefuseActive = qfalse;
	ent->client->tceObjectiveContact = qfalse;

	// dead clients don't activate triggers!
	if ( ent->client->ps.stats[STAT_HEALTH] <= 0 ) {
		return;
	}

	VectorSubtract( ent->client->ps.origin, range, mins );
	VectorAdd( ent->client->ps.origin, range, maxs );

	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	// can't use ent->absmin, because that has a one unit pad
	VectorAdd( ent->client->ps.origin, ent->r.mins, mins );
	VectorAdd( ent->client->ps.origin, ent->r.maxs, maxs );

	for ( i=0 ; i<num ; i++ ) {
		hit = &g_entities[touch[i]];
		/* TC G_TouchTriggers 20045060: enemy bomb acquisition precedes
		 * trigger contact and is independent of the SDK engineer class. */
		if ((hit->s.eType == ET_MISSILE || hit->s.eType == 35) && hit->s.weapon == 15) {
			if (hit->s.teamNum != ent->client->sess.sessionTeam) {
				vec3_t eye, direction, facing;
				float distance;
				VectorCopy(ent->client->ps.origin, eye);
				eye[2] += ent->client->ps.viewheight;
				VectorSubtract(hit->r.currentOrigin, eye, direction);
				distance = VectorNormalize(direction);
				AngleVectors(ent->client->ps.viewangles, facing, NULL, NULL);
				if (DotProduct(facing, direction) > 0.707f && distance < 72.0f) {
					ent->client->ps.serverCursorHint = HINT_DISARM;
					ent->client->ps.serverCursorHintVal = 0;
					ent->client->tceDefuseActive = qtrue;
					ent->client->tceDefuseEntity = touch[i];
				}
			}
			continue;
		}
		if ((hit->r.contents & CONTENTS_TRIGGER) &&
			!strcmp(hit->classname, "trigger_objective_info") &&
			(hit->spawnflags & (AXIS_OBJECTIVE | ALLIED_OBJECTIVE)) &&
			ent->client->ps.origin[2] + ent->client->ps.mins[2] + 4.0f <
				hit->r.maxs[2] + hit->s.pos.trBase[2]) {
			ent->client->tceObjectiveContact = qtrue;
		}

		if ( !hit->touch && !ent->touch ) {
			continue;
		}
		if ( !( hit->r.contents & CONTENTS_TRIGGER ) ) {
			continue;
		}

		// Arnout: invisible entities can't be touched
		// Gordon: radiant tabs arnout! ;)
		if( hit->entstate == STATE_INVISIBLE ||
			hit->entstate == STATE_UNDERCONSTRUCTION ) {
			continue;
		}

		// ignore most entities if a spectator
		if ( ent->client->sess.sessionTeam == TEAM_SPECTATOR ) {
			if ( hit->s.eType != ET_TELEPORT_TRIGGER ) {
				continue;
			}
		}

		// use seperate code for determining if an item is picked up
		// so you don't have to actually contact its bounding box
		if ( hit->s.eType == ET_ITEM ) {
			if ( !BG_PlayerTouchesItem( &ent->client->ps, &hit->s, level.time ) ) {
				continue;
			}
		} else {
			// MrE: always use capsule for player
			if ( !trap_EntityContactCapsule( mins, maxs, hit ) ) {
			//if ( !trap_EntityContact( mins, maxs, hit ) ) {
				continue;
			}
		}

		memset( &trace, 0, sizeof(trace) );

		if ( hit->touch ) {
			hit->touch (hit, ent, &trace);
		}

		if ( ( ent->r.svFlags & SVF_BOT ) && ( ent->touch ) ) {
			ent->touch( ent, hit, &trace );
		}
	}
}

/*
=================
SpectatorThink
=================
*/
void SpectatorThink( gentity_t *ent, usercmd_t *ucmd ) {
	pmove_t	pm;
	gclient_t	*client;
	gentity_t *crosshairEnt = NULL; // rain - #480

	client = ent->client;

	// rain - #480 - sanity check - check .active in case the client sends us
	// something completely bogus
	crosshairEnt = &g_entities[ent->client->ps.identifyClient];

	if (crosshairEnt->inuse && crosshairEnt->client &&
		(ent->client->sess.sessionTeam == crosshairEnt->client->sess.sessionTeam ||
		crosshairEnt->client->ps.powerups[PW_OPS_DISGUISED])) {

		// rain - identifyClientHealth sent as unsigned char, so we
		// can't transmit negative numbers
		if (crosshairEnt->health >= 0)
			ent->client->ps.identifyClientHealth = crosshairEnt->health;
		else
			ent->client->ps.identifyClientHealth = 0;
	}

	if ( client->sess.spectatorState != SPECTATOR_FOLLOW ) {
		client->ps.pm_type = PM_SPECTATOR;
		client->ps.speed = 800;	// was: 400 // faster than normal
		if (client->ps.sprintExertTime)
			client->ps.speed *= 3;	// (SA) allow sprint in free-cam mode


		// OSP - dead players are frozen too, in a timeout
		if((client->ps.pm_flags & PMF_LIMBO) && level.match_pause != PAUSE_NONE) {
			client->ps.pm_type = PM_FREEZE;
		} else if( client->noclip ) {
			client->ps.pm_type = PM_NOCLIP;
		}

		// set up for pmove
		memset (&pm, 0, sizeof(pm));
		pm.ps = &client->ps;
		pm.pmext = &client->pmext;
		pm.character = client->pers.character;
		pm.cmd = *ucmd;
		pm.skill = client->sess.skill;
		pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;	// spectators can fly through bodies
		pm.trace = trap_TraceCapsuleNoEnts;
		pm.pointcontents = trap_PointContents;

#ifdef SAVEGAME_SUPPORT
		if( g_gametype.integer == GT_SINGLE_PLAYER && g_reloading.integer )
			pm.reloading = qtrue;
#endif // SAVEGAME_SUPPORT

		Pmove( &pm ); // JPW NERVE

		// Rafael - Activate
		// Ridah, made it a latched event (occurs on keydown only)
		if (client->latched_buttons & BUTTON_ACTIVATE)
		{
			Cmd_Activate_f (ent);
		}

		// save results of pmove
		VectorCopy( client->ps.origin, ent->s.origin );

		G_TouchTriggers( ent );
		trap_UnlinkEntity( ent );
	}

	if (ent->flags & FL_NOFATIGUE)
		ent->client->pmext.sprintTime = SPRINTTIME;


	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;

//----(SA)	added
	client->oldwbuttons = client->wbuttons;
	client->wbuttons = ucmd->wbuttons;

	// MV clients use these buttons locally for other things
	if(client->pers.mvCount < 1) {
		// attack button cycles through spectators
		if ( ( client->buttons & BUTTON_ATTACK ) && ! ( client->oldbuttons & BUTTON_ATTACK ) ) {
			Cmd_FollowCycle_f( ent, 1 );
		}
		// activate button swaps places with bot
		else if( client->sess.sessionTeam != TEAM_SPECTATOR &&
				( ( client->buttons & BUTTON_ACTIVATE ) && ! ( client->oldbuttons & BUTTON_ACTIVATE ) ) &&
				( g_entities[ent->client->sess.spectatorClient].client ) &&
				( g_entities[ent->client->sess.spectatorClient].r.svFlags & SVF_BOT ) )
		{
			Cmd_SwapPlacesWithBot_f( ent, ent->client->sess.spectatorClient );
		} else if ( 
			( client->sess.sessionTeam == TEAM_SPECTATOR ) && // don't let dead team players do free fly
			( client->sess.spectatorState == SPECTATOR_FOLLOW ) && 
			( ( ( client->buttons & BUTTON_ACTIVATE ) && 
			! ( client->oldbuttons & BUTTON_ACTIVATE )) || ucmd->upmove > 0 ) &&
			G_allowFollow(ent, TEAM_AXIS) && G_allowFollow(ent, TEAM_ALLIES) )
		{
			// code moved to StopFollowing
			StopFollowing(ent);
		}
	}
}


/*
=================
ClientInactivityTimer

Returns qfalse if the client is dropped
=================
*/
/* Whole TC200458d0 objective inactivity controller. */
void G_TCEObjectiveActivity(gclient_t *client) {
    if(g_gametype.integer==5 && ((client->ps.stats[STAT_TCE_WEAPON_FLAGS]|client->ps.stats[STAT_TCE_FLAGS])&0x100)) {
        if(client->pers.cmd.forwardmove || client->pers.cmd.rightmove || client->pers.cmd.upmove ||
            (client->pers.cmd.wbuttons&0x31) || (client->pers.cmd.buttons&BUTTON_ATTACK) ||
            client->ps.pm_type==PM_DEAD || client->ps.stats[STAT_TCE_MOVEMENT_INSTABILITY]>0)
            client->tceObjectiveActivityUntil=level.time+20000;
    } else client->tceObjectiveActivityUntil=level.time+60000;
}

qboolean ClientInactivityTimer( gclient_t *client ) {
	// OSP - modified
	if( ( g_inactivity.integer == 0 && client->sess.sessionTeam != TEAM_SPECTATOR ) || ( g_spectatorInactivity.integer == 0 && client->sess.sessionTeam == TEAM_SPECTATOR ) ) {

		// give everyone some time, so if the operator sets g_inactivity during
		// gameplay, everyone isn't kicked
		client->inactivityTime = level.time + 60 * 1000;
		client->inactivityWarning = qfalse;
	} else if ( client->pers.cmd.forwardmove || 
		client->pers.cmd.rightmove || 
		client->pers.cmd.upmove ||
		(client->pers.cmd.wbuttons & WBUTTON_ATTACK2) ||
		(client->pers.cmd.buttons & BUTTON_ATTACK) ||
		(client->pers.cmd.wbuttons & WBUTTON_LEANLEFT) ||
		(client->pers.cmd.wbuttons & WBUTTON_LEANRIGHT)
		|| client->ps.pm_type == PM_DEAD ) {

		client->inactivityWarning = qfalse;
		client->inactivityTime = level.time + 1000 *
								 ((client->sess.sessionTeam != TEAM_SPECTATOR) ?
												g_inactivity.integer :
												g_spectatorInactivity.integer);

	} else if ( !client->pers.localClient ) {
		if ( level.time > client->inactivityTime && client->inactivityWarning) {
			client->inactivityWarning = qfalse;
			client->inactivityTime = level.time + 60 * 1000;
			trap_DropClient(client - level.clients, "Dropped due to inactivity", 0 );
			return(qfalse);
		}

		if ( !client->inactivityWarning && level.time > client->inactivityTime - 10000 ) {
			CPx(client - level.clients, "cp \"^310 seconds until inactivity drop!\n\"");
			CPx(client - level.clients, "print \"^310 seconds until inactivity drop!\n\"");
			G_Printf("10s inactivity warning issued to: %s\n", client->pers.netname);

			client->inactivityWarning = qtrue;
			client->inactivityTime = level.time + 10000;	// Just for safety
		}
	}
	return qtrue;
}

/*
==================
ClientTimerActions

Actions that happen once a second
==================
*/
/* TC20045960: timed debug damage, waypoint editor and location updates. */
void ClientTimerActions( gentity_t *ent, int msec ) {
    gclient_t *client = ent->client;
    client->timeResidual += msec;
    while(client->timeResidual >= 1000) {
        gentity_t *location;
        client->timeResidual -= 1000;
        if(g_debugBullets.integer > 3) {
            vec3_t direction = {1,0,0};
            G_Damage(ent,ent,ent,direction,ent->client->ps.origin,1,0,MOD_MP40);
        }
        if(bot_editWaypoints.integer>1 && client->ps.clientNum==0) {
            gentity_t *player=&g_entities[0];
            short node=TCE_FindClosestNodeToPoint(player,player->r.currentOrigin);
            TCE_ShowNode((int)node,0);
            if(bot_editWaypoints.integer>2) {
                vec3_t start,end,forward;
                trace_t tr;
                int axis;
                AngleVectors(player->client->ps.viewangles,forward,NULL,NULL);
                VectorCopy(player->r.currentOrigin,start);
                start[2]=(float)((double)player->client->ps.viewheight+player->r.currentOrigin[2]);
                for(axis=0;axis<3;axis++)end[axis]=(float)((double)forward[axis]*8192.0+start[axis]);
                trap_Trace(&tr,start,NULL,NULL,end,player->s.number,1);
                node=TCE_FindClosestNodeToPoint(player,tr.endpos);
                TCE_ShowNode((int)node,1);
            }
        }
        location=Team_GetLocation(ent);
        ent->client->ps.holdable[13]=location?location->health:0;
    }
}

/*
====================
ClientIntermissionThink
====================
*/
void ClientIntermissionThink( gclient_t *client ) {
	client->ps.eFlags &= ~EF_TALK;
	client->ps.eFlags &= ~EF_FIRING;

	// the level will exit when everyone wants to or after timeouts

	// swap and latch button actions
	client->oldbuttons = client->buttons;
	client->buttons = client->pers.cmd.buttons;

//----(SA)	added
	client->oldwbuttons = client->wbuttons;
	client->wbuttons = client->pers.cmd.wbuttons;
}


/*
================
ClientEvents

Events will be passed on to the clients for presentation,
but any server game effects are handled here
================
*/
void Weapon_Engineer(gentity_t *ent);

/* Original 20045b80, with private source event IDs translated by name. */
void ClientEvents( gentity_t *ent, int oldEventSequence ) {
	int			i;
	int			event;
	gclient_t	*client;
	int			damage;
	gentity_t *objective;

	client = ent->client;

	if ( oldEventSequence < client->ps.eventSequence - MAX_EVENTS ) {
		oldEventSequence = client->ps.eventSequence - MAX_EVENTS;
	}
	for ( i = oldEventSequence ; i < client->ps.eventSequence ; i++ ) {
		event = client->ps.events[ i & (MAX_EVENTS-1) ];

		switch ( event ) {
		case EV_FALL_NDIE:
		//case EV_FALL_SHORT:
		case EV_FALL_DMG_10:
		case EV_FALL_DMG_15:
		case EV_FALL_DMG_25:
		//case EV_FALL_DMG_30:
		case EV_FALL_DMG_50:
		case EV_TCE_FALL_DMG_75:
		
			// rain - VectorClear() used to be done here whenever falling
			// damage occured, but I moved it to bg_pmove where it belongs.
			
			if ( ent->s.eType != ET_PLAYER ) {
				break;		// not in the player model
			}
			if ( event == EV_FALL_NDIE ) 
			{
				damage = 9999;
			}
			else if (event == EV_TCE_FALL_DMG_75)
			{
				damage = 75;
			}
			else if (event == EV_FALL_DMG_50)
			{
				damage = 50;
			}
			else if (event == EV_FALL_DMG_25)
			{
				damage = 25;
			}
			else if (event == EV_FALL_DMG_15)
			{
				damage = 15;
			}
			else if (event == EV_FALL_DMG_10)
			{
				damage = 10;
			}
			else
				damage = 5; // never used
			ent->pain_debounce_time = level.time + 200;	// no normal pain sound
			G_Damage (ent, NULL, NULL, NULL, NULL, damage, 0, MOD_FALLING);
			break;

		case EV_FIRE_WEAPON_MG42:

			// Gordon: reset player disguise on stealing docs
			ent->client->ps.powerups[PW_OPS_DISGUISED] = 0;

			mg42_fire( ent );

			// Only 1 stats bin for mg42
			if(g_gamestate.integer == GS_PLAYING)
				ent->client->sess.aWeaponStats[BG_WeapStatForWeapon(WP_MOBILE_MG42)].atts++;

			break;
		case EV_FIRE_WEAPON_MOUNTEDMG42:
			// Gordon: reset player disguise on stealing docs
			ent->client->ps.powerups[PW_OPS_DISGUISED] = 0;

			mountedmg42_fire( ent );
			// Only 1 stats bin for mg42
			if(g_gamestate.integer == GS_PLAYING)
				ent->client->sess.aWeaponStats[BG_WeapStatForWeapon(WP_MOBILE_MG42)].atts++;

			break;

		case EV_FIRE_WEAPON_AAGUN:

			// Gordon: reset player disguise on stealing docs
			ent->client->ps.powerups[PW_OPS_DISGUISED] = 0;

			aagun_fire( ent );
			break;

		case EV_FIRE_WEAPON:
		case EV_FIRE_WEAPONB:
		case EV_FIRE_WEAPON_LASTSHOT:
			FireWeapon( ent );
			break;

		case EV_TCE_PLANT:
			G_TouchTriggers(ent);
			if (client->tceObjectiveContact && client->ps.serverCursorHint == HINT_BREAKABLE_DYNAMITE) {
				weapon_grenadelauncher_fire(ent, 15);
				client->ps.stats[STAT_TCE_WEAPON_FLAGS] &= ~0x100;
				client->tceBombPossessionOrder = 0;
			}
			break;
		case EV_TCE_DEFUSE:
			Weapon_Engineer(ent);
			break;
		case EV_TCE_OBJECTIVE_START:
		case EV_TCE_OBJECTIVE_COMPLETE:
			G_TouchTriggers(ent);
			if (!client->tceObjectiveContact || client->ps.serverCursorHint != HINT_ACTIVATE) break;
			if (client->tceObjectiveEntity < 0 || client->tceObjectiveEntity >= MAX_GENTITIES) break;
			objective = &g_entities[client->tceObjectiveEntity];
			if (objective->s.eType != 65) break;
			if (event == EV_TCE_OBJECTIVE_START) {
				G_Script_ScriptEvent(objective, "activated", "");
			} else {
				G_Script_ScriptEvent(objective, "completed", "");
				if (client->sess.sessionTeam == TEAM_AXIS) {
					G_Script_ScriptEvent(objective, "completed", "terrorists");
					if (objective->parent) objective->parent->spawnflags = (objective->parent->spawnflags & ~1) | 2;
				} else if (client->sess.sessionTeam == TEAM_ALLIES) {
					G_Script_ScriptEvent(objective, "completed", "specops");
					if (objective->parent) objective->parent->spawnflags = (objective->parent->spawnflags | 1) & ~2;
				}
				G_UseTargets(objective, ent);
			}
			break;
		case EV_TCE_OBJECTIVE_STOP:
			if (client->tceObjectiveEntity >= 0 && client->tceObjectiveEntity < MAX_GENTITIES) {
				objective = &g_entities[client->tceObjectiveEntity];
				if (objective->s.eType == 65) G_Script_ScriptEvent(objective, "stopped", "");
			}
			break;

		default:
			break;
		}
	}

}

/*
==============
SendPendingPredictableEvents
==============
*/
void SendPendingPredictableEvents( playerState_t *ps ) {
	/*
	gentity_t *t;
	int event, seq;
	int extEvent, number;

	// if there are still events pending
	if ( ps->entityEventSequence < ps->eventSequence ) {
		// create a temporary entity for this event which is sent to everyone
		// except the client generated the event
		seq = ps->entityEventSequence & (MAX_EVENTS-1);
		event = ps->events[ seq ] | ( ( ps->entityEventSequence & 3 ) << 8 );
		// set external event to zero before calling BG_PlayerStateToEntityState
		extEvent = ps->externalEvent;
		ps->externalEvent = 0;
		// create temporary entity for event
		t = G_TempEntity( ps->origin, event );
		number = t->s.number;
		BG_PlayerStateToEntityState( ps, &t->s, qtrue );
		t->s.number = number;
		t->s.eType = ET_EVENTS + event;
		t->s.eFlags |= EF_PLAYER_EVENT;
		t->s.otherEntityNum = ps->clientNum;
		// send to everyone except the client who generated the event
		t->r.svFlags |= SVF_NOTSINGLECLIENT;
		t->r.singleClient = ps->clientNum;
		// set back external event
		ps->externalEvent = extEvent;
	}
	*/
}

// DHM - Nerve
/* Windows20047e30 / Linux G_StoreClientPosition0009d8f4.
 * Kept distinct from Windows2004e560's80-byte antialag trail. */
static void G_TCEStoreFramePosition(gentity_t *ent) {
	gclient_t *client;
	int index;
	if (!ent->inuse) return;
	client = ent->client;
	if ((client->sess.sessionTeam != TEAM_AXIS && client->sess.sessionTeam != TEAM_ALLIES) ||
	    !ent->r.linked || ent->health <= 0 || (client->ps.pm_flags & PMF_LIMBO) ||
	    client->ps.pm_type != PM_NORMAL) return;
	index = ++client->tcePositionHistoryIndex;
	if (index > 9) index = client->tcePositionHistoryIndex = 0;
	VectorCopy(ent->r.mins, client->tcePositionHistory[index].mins);
	VectorCopy(ent->r.maxs, client->tcePositionHistory[index].maxs);
	VectorCopy(ent->s.pos.trBase, client->tcePositionHistory[index].origin);
	client->tcePositionHistory[index].time = level.time;
}

void WolfFindMedic( gentity_t *self ) {
	int i, medic=-1;
	gclient_t	*cl;
	vec3_t	start, end;
//	vec3_t	temp;	// rain - unused
	trace_t	tr;
	float	bestdist=1024, dist;

	self->client->ps.viewlocked_entNum = 0;
	self->client->ps.viewlocked = 0;
	self->client->ps.stats[STAT_DEAD_YAW] = 999;

	VectorCopy( self->s.pos.trBase, start );
	start[2] += self->client->ps.viewheight;

	for( i = 0; i < level.numConnectedClients; i++ ) {
		cl = &level.clients[ level.sortedClients[i] ];

		if( level.sortedClients[i] == self->client->ps.clientNum ) {
			continue;
		}

		if( cl->sess.sessionTeam != self->client->sess.sessionTeam ) {
			continue;
		}

		if( cl->ps.pm_type == PM_DEAD ) {
			continue;
		}

		// zinx - limbo'd players are not PM_DEAD or STAT_HEALTH <= 0.
		// and we certainly don't want to lock to them
		// fix for bug #345
		if( cl->ps.pm_flags & PMF_LIMBO ) {
			continue;
		}

		if( cl->ps.stats[ STAT_HEALTH ] <= 0 ) {
			continue;
		}

		if( cl->ps.stats[ STAT_PLAYER_CLASS ] != PC_MEDIC ) {
			continue;
		}

		VectorCopy( g_entities[level.sortedClients[i]].s.pos.trBase, end );
		end[2] += cl->ps.viewheight;

		trap_Trace (&tr, start, NULL, NULL, end, self->s.number, CONTENTS_SOLID);
		if( tr.fraction < 0.95 ) {
			continue;
		}

		VectorSubtract( end, start, end );
		dist = VectorNormalize( end );

		if ( dist < bestdist ) {
			medic = cl->ps.clientNum;
#if 0 // rain - not sure what the point of this is
			vectoangles( end, temp );
			self->client->ps.stats[STAT_DEAD_YAW] = temp[YAW];
#endif
			bestdist = dist;
		}
	}

	if ( medic >= 0 ) {
		self->client->ps.viewlocked_entNum = medic;
		self->client->ps.viewlocked = 7;
	}
}


//void ClientDamage( gentity_t *clent, int entnum, int enemynum, int id );		// NERVE - SMF

/*
==============
ClientThink

This will be called once for each client frame, which will
usually be a couple times for each server frame on fast clients.

If "g_synchronousClients 1" is set, this will be called exactly
once for each server frame, which makes for smooth demo recording.
==============
*/
void ClientThink_real( gentity_t *ent ) {
	int			msec, oldEventSequence, monsterslick = 0;
	pmove_t		pm;
	usercmd_t	*ucmd;
	gclient_t	*client = ent->client;


	// don't think if the client is not yet connected (and thus not yet spawned in)
	if (client->pers.connected != CON_CONNECTED) {
		return;
	}

	if( ent->s.eFlags & EF_MOUNTEDTANK ) {
		client->pmext.centerangles[YAW] = ent->tagParent->r.currentAngles[ YAW ];
		client->pmext.centerangles[PITCH] = ent->tagParent->r.currentAngles[ PITCH ];
	}

/*	if (client->cameraPortal) {
		G_SetOrigin( client->cameraPortal, client->ps.origin );
		trap_LinkEntity(client->cameraPortal);
		VectorCopy( client->cameraOrigin, client->cameraPortal->s.origin2);
	}*/

	// mark the time, so the connection sprite can be removed
	ucmd = &ent->client->pers.cmd;

	ent->client->ps.identifyClient = ucmd->identClient;		// NERVE - SMF

	// sanity check the command time to prevent speedup cheating
	if ( ucmd->serverTime > level.time + 200 ) {
		ucmd->serverTime = level.time + 200;
//		G_Printf("serverTime <<<<<\n" );
	}
	if ( ucmd->serverTime < level.time - 1000 ) {
		ucmd->serverTime = level.time - 1000;
//		G_Printf("serverTime >>>>>\n" );
	} 

	msec = ucmd->serverTime - client->ps.commandTime;
	// following others may result in bad times, but we still want
	// to check for follow toggles
	if ( msec < 1 && client->sess.spectatorState != SPECTATOR_FOLLOW ) {
		return;
	}
	if ( msec > 200 ) {
		msec = 200;
	}

	if ( pmove_fixed.integer || client->pers.pmoveFixed ) {
		ucmd->serverTime = ((ucmd->serverTime + pmove_msec.integer-1) / pmove_msec.integer) * pmove_msec.integer;
	}

	if( client->wantsscore ) {
		G_SendScore( ent );
		client->wantsscore = qfalse;
	}

	//
	// check for exiting intermission
	//
	if ( level.intermissiontime ) {
		ClientIntermissionThink( client );
		return;
	}

	// check for inactivity timer, but never drop the local client of a non-dedicated server
	// OSP - moved here to allow for spec inactivity checks as well
	if ( !ClientInactivityTimer( client ) ) {
		return;
	}
	G_TCEObjectiveActivity(client);
	/* TC client+0xdec persists the pre-command fire mode (ps+0x138). */
	client->sess.tceSessionValues[0] = client->ps.persistant[10];
	
	if( !(ent->r.svFlags & SVF_BOT) && level.time - client->pers.lastCCPulseTime > 2000 ) {
		G_SendMapEntityInfo( ent );
		client->pers.lastCCPulseTime = level.time;
	}

	if( !(ucmd->flags & 0x01) || ucmd->forwardmove || ucmd->rightmove || ucmd->upmove || ucmd->wbuttons || ucmd->doubleTap ) {
		ent->r.svFlags &= ~(SVF_SELF_PORTAL_EXCLUSIVE|SVF_SELF_PORTAL);
	}

	// spectators don't do much
	// DHM - Nerve :: In limbo use SpectatorThink
	if ( client->sess.sessionTeam == TEAM_SPECTATOR || client->ps.pm_flags & PMF_LIMBO ) {
		/*if ( client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
			return;
		}*/
		SpectatorThink( ent, ucmd );
		return;
	}

	/* TC20045f80: camera/pause are the only input-freeze producers here;
	 * SDK single-player savegame reloading is not part of this controller. */
	if((client->ps.eFlags & EF_VIEWING_CAMERA) || level.match_pause != PAUSE_NONE) {
		ucmd->buttons = 0;
		ucmd->forwardmove = 0;
		ucmd->rightmove = 0;
		ucmd->upmove = 0;
		ucmd->wbuttons = 0;
		ucmd->doubleTap = 0;

		// Pausing retains velocity; camera-only freezing clears it.
		if(level.match_pause != PAUSE_NONE) {
			client->ps.pm_type = PM_FREEZE;
		} else if(client->ps.eFlags & EF_VIEWING_CAMERA) {
			VectorClear(client->ps.velocity);
			client->ps.pm_type = PM_FREEZE;
		}
	} else if ( client->noclip ) {
		client->ps.pm_type = PM_NOCLIP;
	} else if ( client->ps.stats[STAT_HEALTH] <= 0 ) {
		client->ps.pm_type = PM_DEAD;
	} else {
		client->ps.pm_type = PM_NORMAL;
	}

	/* TC:E ClientThink_real 20045f80: transmit the warmup input lock. */
	if ((g_gametype.integer == 5 || g_gametype.integer == 7 || g_gametype.integer == 2) &&
	    (g_gamestate.integer == GS_WARMUP || g_gamestate.integer == GS_WARMUP_COUNTDOWN))
		client->ps.stats[STAT_TCE_FLAGS] |= TCE_STAT_WARMUP_LOCK;
	else
		client->ps.stats[STAT_TCE_FLAGS] &= ~TCE_STAT_WARMUP_LOCK;

	client->ps.aiState = AISTATE_COMBAT;
	/* TC 200461f5..20046273: developer overrides precede the enlarged
	 * collision-hull scale. These authoritative values also feed prediction. */
	if (g_developer.integer) {
		client->ps.speed = (int)g_speed.value;
		client->ps.gravity = (int)g_gravity.value;
	} else {
		client->ps.speed = 320;
		client->ps.gravity = 800;
	}
	if (g_newbbox.integer) {
		client->ps.speed = (int)((double)client->ps.speed * 1.25);
		client->ps.gravity = (int)((double)client->ps.gravity * 1.25);
	}
	if (client->speedScale) /* Goalitem modifier follows hull scaling. */
		client->ps.speed = (int)(((double)client->speedScale * 0.01) * client->ps.speed);

	// set up for pmove
	oldEventSequence = client->ps.eventSequence;

	/* Original20046276: integer -> double reciprocal -> one float store. */
	client->currentAimSpreadScale = (float)((double)client->ps.aimSpreadScale * 0.00392156862745098);

	memset (&pm, 0, sizeof(pm));

	pm.ps = &client->ps;
	pm.pmext = &client->pmext;
	pm.character = client->pers.character;
	pm.cmd = *ucmd;
	pm.oldcmd = client->pers.oldcmd;
	// MrE: always use capsule for AI and player
	pm.trace = trap_TraceCapsule;
	if ( pm.ps->pm_type == PM_DEAD ) {
		pm.tracemask = MASK_PLAYERSOLID & ~CONTENTS_BODY;
		// DHM-Nerve added:: EF_DEAD is checked for in Pmove functions, but wasn't being set
		//              until after Pmove
		pm.ps->eFlags |= EF_DEAD;
		// dhm-Nerve end
	} else if( pm.ps->pm_type == PM_SPECTATOR ) {
		pm.trace = trap_TraceCapsuleNoEnts;
	} else {
		pm.tracemask = MASK_PLAYERSOLID;
	}
	//DHM - Nerve :: We've gone back to using normal bbox traces
	//pm.trace = trap_Trace;
	pm.pointcontents = trap_PointContents;
	pm.debugLevel = g_debugMove.integer;
	pm.noFootsteps = qfalse;

	pm.pmove_fixed = pmove_fixed.integer | client->pers.pmoveFixed;
	pm.pmove_msec = pmove_msec.integer;

	pm.noWeapClips = qfalse;

	VectorCopy( client->ps.origin, client->oldOrigin );

	// NERVE - SMF
	pm.gametype = g_gametype.integer;
	pm.ltChargeTime = level.lieutenantChargeTime[client->sess.sessionTeam-1];
	pm.soldierChargeTime = level.soldierChargeTime[client->sess.sessionTeam-1];
	pm.engineerChargeTime = level.engineerChargeTime[client->sess.sessionTeam-1];
	pm.medicChargeTime = level.medicChargeTime[client->sess.sessionTeam-1];
	// -NERVE - SMF

	pm.skill = client->sess.skill;
	/* TC ps+0x3c0 publishes the truncated battle-sense skillpoints. */
	client->ps.holdable[12] = (int)ent->client->sess.skillpoints[0];

	client->pmext.airleft = ent->client->airOutTime - level.time;

	pm.covertopsChargeTime = level.covertopsChargeTime[client->sess.sessionTeam-1];

	if( client->ps.pm_type != PM_DEAD && level.timeCurrent - client->pers.lastBattleSenseBonusTime > 45000 ) {
		/*switch( client->combatState )
		{
		case COMBATSTATE_COLD:	G_AddSkillPoints( ent, SK_BATTLE_SENSE, 0.f ); G_DebugAddSkillPoints( ent, SK_BATTLE_SENSE, 0.f, "combatstate cold" ); break;
		case COMBATSTATE_WARM:	G_AddSkillPoints( ent, SK_BATTLE_SENSE, 2.f ); G_DebugAddSkillPoints( ent, SK_BATTLE_SENSE, 2.f, "combatstate warm" ); break;
		case COMBATSTATE_HOT:	G_AddSkillPoints( ent, SK_BATTLE_SENSE, 5.f ); G_DebugAddSkillPoints( ent, SK_BATTLE_SENSE, 5.f, "combatstate hot" ); break;
		case COMBATSTATE_SUPERHOT:	G_AddSkillPoints( ent, SK_BATTLE_SENSE, 8.f ); G_DebugAddSkillPoints( ent, SK_BATTLE_SENSE, 8.f, "combatstate super-hot" ); break;
		}*/

		if( client->combatState != COMBATSTATE_COLD ) {
			if( client->combatState & (1<<COMBATSTATE_KILLEDPLAYER) && client->combatState & (1<<COMBATSTATE_DAMAGERECEIVED) ) {
				G_AddSkillPoints( ent, SK_BATTLE_SENSE, 8.f ); G_DebugAddSkillPoints( ent, SK_BATTLE_SENSE, 8.f, "combatstate super-hot" );
			} else if( client->combatState & (1<<COMBATSTATE_DAMAGEDEALT) && client->combatState & (1<<COMBATSTATE_DAMAGERECEIVED) ) {
				G_AddSkillPoints( ent, SK_BATTLE_SENSE, 5.f ); G_DebugAddSkillPoints( ent, SK_BATTLE_SENSE, 5.f, "combatstate hot" );
			} else {
				G_AddSkillPoints( ent, SK_BATTLE_SENSE, 2.f ); G_DebugAddSkillPoints( ent, SK_BATTLE_SENSE, 2.f, "combatstate warm" );
			}
		}

		client->pers.lastBattleSenseBonusTime = level.timeCurrent;
		client->combatState = COMBATSTATE_COLD;	// cool down again
	}

	pm.leadership = qfalse;
	/*for ( i = 0 ; i < level.numConnectedClients; i++ ) {
		gclient_t *cl = &level.clients[level.sortedClients[i]];
		vec3_t dist;

		if( cl->sess.sessionTeam != client->sess.sessionTeam ) {
			continue;
		}

		if( cl->sess.skill[SK_SIGNALS] < 5 ) {
			continue;
		}

		if( !trap_InPVS( g_entities[level.sortedClients[i]].r.currentOrigin, ent->r.currentOrigin ) ) {
			continue;
		}

		VectorSubtract( g_entities[level.sortedClients[i]].r.currentOrigin, ent->r.currentOrigin, dist );
		if( VectorLengthSquared( dist ) > SQR(512) )
			continue;

		pm.leadership = qtrue;

        break;        
	}*/

	// Gordon: bit hacky, stop the slight lag from client -> server even on locahost, switching back to the weapon you were holding
	//			and then back to what weapon you should have, became VERY noticible for the kar98/carbine + gpg40, esp now i've added the
	//			animation locking
	if( level.time - client->pers.lastSpawnTime < 1000 ) {
		pm.cmd.weapon = client->ps.weapon;
	}

	monsterslick = Pmove( &pm );

	// Gordon: thx to bani for this
	// ikkyo - fix leaning players bug                                       
	VectorCopy( client->ps.velocity, ent->s.pos.trDelta );                  
	SnapVector( ent->s.pos.trDelta );
	// end

	// server cursor hints
	// TAT 1/10/2003 - bots don't need to check for cursor hints
	if ( !(ent->r.svFlags & SVF_BOT) && ent->lastHintCheckTime < level.time )
	{
		G_CheckForCursorHints(ent);

		ent->lastHintCheckTime = level.time + FRAMETIME;
	}

	// DHM - Nerve :: Set animMovetype to 1 if ducking
	if ( ent->client->ps.pm_flags & PMF_DUCKED )
		ent->s.animMovetype = 1;
	else
		ent->s.animMovetype = 0;

	// save results of pmove
	if ( ent->client->ps.eventSequence != oldEventSequence ) {
		ent->eventTime = level.time;
		ent->r.eventTime = level.time;
	}

	// Ridah, fixes jittery zombie movement
	if (g_smoothClients.integer) {
		BG_PlayerStateToEntityStateExtraPolate( &ent->client->ps, &ent->s, level.time, qfalse );
	} else {
		BG_PlayerStateToEntityState( &ent->client->ps, &ent->s, qfalse );
	}

	if ( !( ent->client->ps.eFlags & EF_FIRING ) ) {
		client->fireHeld = qfalse;		// for grapple
	}

//
//	// use the precise origin for linking
//	VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );
//
//	// use the snapped origin for linking so it matches client predicted versions
	VectorCopy( ent->s.pos.trBase, ent->r.currentOrigin );

	VectorCopy (pm.mins, ent->r.mins);
	VectorCopy (pm.maxs, ent->r.maxs);

	ent->waterlevel = pm.waterlevel;
	ent->watertype = pm.watertype;

	// execute client events
	if(level.match_pause == PAUSE_NONE) {
		ClientEvents( ent, oldEventSequence );
	}

	// link entity now, after any personal teleporters have been used
	trap_LinkEntity (ent);
	if ( !ent->client->noclip ) {
		G_TouchTriggers( ent );
	}

	// NOTE: now copy the exact origin over otherwise clients can be snapped into solid
	VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );

	/* TC records the exact post-trigger pose before impact callbacks. */
	G_StoreClientPosition(ent);

	// touch other objects
	ClientImpacts( ent, &pm );

	// save results of triggers and client events
	if (ent->client->ps.eventSequence != oldEventSequence) {
		ent->eventTime = level.time;
	}

	// swap and latch button actions
	client->oldbuttons = client->buttons;
	client->buttons = ucmd->buttons;
	client->latched_buttons = client->buttons & ~client->oldbuttons;
//	client->latched_buttons |= client->buttons & ~client->oldbuttons;	// FIXME:? (SA) MP method (causes problems for us.  activate 'sticks')

	//----(SA)	added
	client->oldwbuttons = client->wbuttons;
	client->wbuttons = ucmd->wbuttons;
	client->latched_wbuttons = client->wbuttons & ~client->oldwbuttons;
//	client->latched_wbuttons |= client->wbuttons & ~client->oldwbuttons;	// FIXME:? (SA) MP method

	// Rafael - Activate
	// Ridah, made it a latched event (occurs on keydown only)
	if( client->latched_buttons & BUTTON_ACTIVATE ) {
		Cmd_Activate_f( ent );
	}

	if (ent->flags & FL_NOFATIGUE)
		ent->client->pmext.sprintTime = SPRINTTIME;

	if ( g_entities[ent->client->ps.identifyClient].team == ent->team && g_entities[ent->client->ps.identifyClient].client ) {
		ent->client->ps.identifyClientHealth = g_entities[ent->client->ps.identifyClient].health;
	} else {
		ent->client->ps.identifyClient = -1;
		ent->client->ps.identifyClientHealth = 0;
	}

	// check for respawning
	if( client->ps.stats[STAT_HEALTH] <= 0 ) {

		// See if we need to hop to limbo
		if( level.timeCurrent > client->respawnTime && !(ent->client->ps.pm_flags & PMF_LIMBO) ) {
			if( ucmd->upmove > 0 ) {
				if( g_gametype.integer == GT_WOLF_LMS || client->ps.persistant[PERS_RESPAWNS_LEFT] >= 0 ) {
					trap_SendServerCommand( ent-g_entities, "reqforcespawn" );
				} else {
					limbo( ent, ( client->ps.stats[STAT_HEALTH] > GIB_HEALTH ) );
				}
			}

			if((g_forcerespawn.integer > 0 && level.timeCurrent - client->respawnTime > g_forcerespawn.integer * 1000) || client->ps.stats[STAT_HEALTH] <= GIB_HEALTH) {
				limbo(ent, (client->ps.stats[STAT_HEALTH] > GIB_HEALTH));
			}
		}

		return;
	}

	if( level.gameManager && level.timeCurrent - client->pers.lastHQMineReportTime > 20000 ) {	// NOTE: 60 seconds? bit much innit
		if( level.gameManager->s.modelindex && client->sess.sessionTeam == TEAM_AXIS ) {
			if( G_SweepForLandmines( ent->r.currentOrigin, 256.f, TEAM_AXIS ) ) {
				client->pers.lastHQMineReportTime = level.timeCurrent;
				trap_SendServerCommand(ent-g_entities, "cp \"Mines have been reported in this area.\" 1");
			}
		} else if( level.gameManager->s.modelindex2 && client->sess.sessionTeam == TEAM_ALLIES ) {
			if( G_SweepForLandmines( ent->r.currentOrigin, 256.f, TEAM_ALLIES ) ) {
				client->pers.lastHQMineReportTime = level.timeCurrent;
				trap_SendServerCommand(ent-g_entities, "cp \"Mines have been reported in this area.\" 1");
			}
		}
	}

	// perform once-a-second actions
	if(level.match_pause == PAUSE_NONE) {
		ClientTimerActions( ent, msec );
	}
}

/*
==================
ClientThink

A new command has arrived from the client
==================
*/
void ClientThink( int clientNum ) {
	gentity_t *ent;

	ent = g_entities + clientNum;
	ent->client->pers.oldcmd = ent->client->pers.cmd;
	trap_GetUsercmd( clientNum, &ent->client->pers.cmd );

	// mark the time we got info, so we can display the
	// phone jack if they don't get any for a while
	ent->client->lastCmdTime = level.time;

	/* TC20046980 dispatches every received command here. G_RunClient has
	 * no synchronous fallback, so the SDK ALLOW_GSYNC gate drops commands. */
	ClientThink_real( ent );

	// if this is the locally playing client, do bot thinks
#ifndef NO_BOT_SUPPORT
	if( bot_enable.integer && !g_dedicated.integer && clientNum == 0
#ifdef FEATURE_OMNIBOT
        && !g_OmniBotEnable.integer
#endif
    ) {
		BotAIThinkFrame(ent->client->pers.cmd.serverTime);
		level.lastClientBotThink = level.time;
	}
#endif // NO_BOT_SUPPORT
}


void G_RunClient( gentity_t *ent ) {
	// Gordon: special case for uniform grabbing
	if( ent->client->pers.cmd.buttons & BUTTON_ACTIVATE ) {
		Cmd_Activate2_f( ent );
	}

	if( ent->health <= 0 && ent->client->ps.pm_flags & PMF_LIMBO ) {
		if( ent->r.linked ) {
			trap_UnlinkEntity( ent );
		}
	}

	/* TC20046a20 ends here, including synchronous-client builds. The
	 * SDK tail would run ClientThink a second time from the entity frame. */
}

/*
==================
SpectatorClientEndFrame

==================
*/
void SpectatorClientEndFrame( gentity_t *ent )
{
	// OSP - specs periodically get score updates for useful demo playback info
	if(/*ent->client->pers.mvCount > 0 &&*/ ent->client->pers.mvScoreUpdate < level.time) {
		ent->client->pers.mvScoreUpdate = level.time + MV_SCOREUPDATE_INTERVAL;
		ent->client->wantsscore = qtrue;
//		G_SendScore(ent);
	}

	// if we are doing a chase cam or a remote view, grab the latest info
	if((ent->client->sess.spectatorState == SPECTATOR_FOLLOW) || (ent->client->ps.pm_flags & PMF_LIMBO)) {
		int clientNum, testtime;
		gclient_t *cl;
		qboolean do_respawn = qfalse; // JPW NERVE

        /* TC 20046a70: reinforcement timing is additionally gated by the
         * per-client death delay. GT5 has its own all-dead/hostage rules. */
        if (g_gamestate.integer != GS_PLAYING &&
            ent->client->respawnTime <= level.timeCurrent &&
            ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
            do_respawn = qtrue;
        } else if (ent->client->sess.sessionTeam == TEAM_AXIS &&
                   ent->client->tceRespawnNotBefore <= level.timeCurrent) {
            testtime = (level.dwRedReinfOffset + level.timeCurrent - level.startTime) % g_redlimbotime.integer;
            do_respawn = testtime < ent->client->pers.lastReinforceTime;
            ent->client->pers.lastReinforceTime = testtime;
        } else if (ent->client->sess.sessionTeam == TEAM_ALLIES &&
                   ent->client->tceRespawnNotBefore <= level.timeCurrent) {
            testtime = (level.dwBlueReinfOffset + level.timeCurrent - level.startTime) % g_bluelimbotime.integer;
            do_respawn = testtime < ent->client->pers.lastReinforceTime;
            ent->client->pers.lastReinforceTime = testtime;
        }

        if (g_gametype.integer != 5) {
            if ((g_maxlives.integer > 0 || g_alliedmaxlives.integer > 0 || g_axismaxlives.integer > 0) &&
                ent->client->ps.persistant[PERS_RESPAWNS_LEFT] == 0 && do_respawn) {
                if (!g_maxlivesRespawnPenalty.integer) {
                    do_respawn = qfalse;
                } else if (ent->client->ps.persistant[PERS_RESPAWNS_PENALTY] > 0) {
                    ent->client->ps.persistant[PERS_RESPAWNS_PENALTY]--;
                    do_respawn = qfalse;
                }
            }
        } else if (g_gamestate.integer == GS_PLAYING) {
            if (!level.teamEliminateTime && !level.tceBombPlanted &&
                !level.tceExitRulesNotBefore && !level.tceEndRoundTime &&
                level.numTeamClients[0] == level.numFinalDead[0] &&
                level.numTeamClients[1] == level.numFinalDead[1] &&
                ent->client->respawnTime <= level.timeCurrent &&
                ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
                reinforce(ent, qfalse);
                return;
            }
            if (level.tceHostageActive && !level.tceHostageSecured && level.tceHostageTeam &&
                !level.teamEliminateTime && !level.tceExitRulesNotBefore && !level.tceEndRoundTime &&
                ent->client->sess.sessionTeam != TEAM_SPECTATOR) {
                int teamIndex = level.tceHostageTeam - 1;
                if (teamIndex == 0 || teamIndex == 1) {
                    if (!level.numFinalDead[teamIndex]) {
                        level.tceExitRulesNotBefore = level.time + 1000;
                        G_Script_ScriptEvent(level.gameManager, "trigger", "hostage_area_secured");
                        level.tceHostageSecured = qtrue;
                    } else if (ent->client->sess.sessionTeam == level.tceHostageTeam &&
                               (float)(rand() & 0x7fff) * 3.0518509447574615e-05f <
                               1.f / (float)level.numFinalDead[teamIndex]) {
                        reinforce(ent, qtrue);
                        return;
                    }
                }
                /* Original preserves the previously computed wave decision
                 * when the hostage branch is active but did not deploy. */
            } else {
                do_respawn = qfalse;
            }
        }

        if (do_respawn) {
            reinforce(ent, qfalse);
            return;
        }

		// Limbos aren't following while in MV
		if((ent->client->ps.pm_flags & PMF_LIMBO) && ent->client->pers.mvCount > 0) {
			return;
		}

		clientNum = ent->client->sess.spectatorClient;

		// team follow1 and team follow2 go to whatever clients are playing
		if ( clientNum == -1 ) {
			clientNum = level.follow1;
		} else if ( clientNum == -2 ) {
			clientNum = level.follow2;
		}

		if ( clientNum >= 0 ) {
			cl = &level.clients[ clientNum ];
			if(cl->pers.connected == CON_CONNECTED && cl->sess.sessionTeam != TEAM_SPECTATOR) {
				int flags = (cl->ps.eFlags & ~(EF_VOTED)) | (ent->client->ps.eFlags & (EF_VOTED));
				int ping = ent->client->ps.ping;

				if(ent->client->sess.sessionTeam != TEAM_SPECTATOR && (ent->client->ps.pm_flags & PMF_LIMBO)) {
					int savedScore = ent->client->ps.persistant[PERS_SCORE];
					int savedRespawns = ent->client->ps.persistant[PERS_RESPAWNS_LEFT];
					int savedRespawnPenalty = ent->client->ps.persistant[PERS_RESPAWNS_PENALTY];
					int savedClass = ent->client->ps.stats[STAT_PLAYER_CLASS];
					int savedMVList = ent->client->ps.powerups[PW_MVCLIENTLIST];

					do_respawn = ent->client->ps.pm_time;

					ent->client->ps = cl->ps;
					ent->client->ps.pm_flags |= PMF_FOLLOW;
					ent->client->ps.pm_flags |= PMF_LIMBO;

					ent->client->ps.pm_time = do_respawn;							// put pm_time back
					ent->client->ps.persistant[PERS_RESPAWNS_LEFT] = savedRespawns;
					ent->client->ps.persistant[PERS_RESPAWNS_PENALTY] = savedRespawnPenalty;
					ent->client->ps.persistant[PERS_SCORE] = savedScore;			// put score back
					ent->client->ps.powerups[PW_MVCLIENTLIST] = savedMVList;
					ent->client->ps.stats[STAT_PLAYER_CLASS] = savedClass;			// NERVE - SMF - put player class back
				} else {
					ent->client->ps = cl->ps;
					ent->client->ps.pm_flags |= PMF_FOLLOW;
				}

				// DHM - Nerve :: carry flags over
				ent->client->ps.eFlags = flags;
				ent->client->ps.ping = ping;

				return;
			} else {
				// drop them to free spectators unless they are dedicated camera followers
				if ( ent->client->sess.spectatorClient >= 0 ) {
					ent->client->sess.spectatorState = SPECTATOR_FREE;
					ClientBegin( ent->client - level.clients );
				}
			}
		}
	}

	/*if ( ent->client->sess.spectatorState == SPECTATOR_SCOREBOARD ) {
		ent->client->ps.pm_flags |= PMF_SCOREBOARD;
	} else {
		ent->client->ps.pm_flags &= ~PMF_SCOREBOARD;
	}*/

	// we are at a free-floating spec state for a player,
	// set speclock status, as appropriate
	//	 --> Can we use something besides a powerup slot?
    if (ent->client->pers.mvCount < 1) {
        int alliedBlockout = G_blockoutTeam(ent, TEAM_ALLIES);
        int axisBlockout = G_blockoutTeam(ent, TEAM_AXIS);
        ent->client->ps.powerups[PW_BLACKOUT] = (alliedBlockout << 1) | axisBlockout;
    }
}


// DHM - Nerve :: After reviving a player, their contents stay CONTENTS_CORPSE until it is determined
//					to be safe to return them to PLAYERSOLID

qboolean StuckInClient( gentity_t *self ) {
	int i;
	vec3_t	hitmin, hitmax;
	vec3_t	selfmin, selfmax;
	gentity_t *hit;

	for(i=0; i<level.numConnectedClients; i++) {
		hit = g_entities + level.sortedClients[i];

		if(!hit->inuse || hit == self || !hit->client ||
		  !hit->s.solid || hit->health <= 0) {
			continue;
		}

		VectorAdd(hit->r.currentOrigin, hit->r.mins, hitmin);
		VectorAdd(hit->r.currentOrigin, hit->r.maxs, hitmax);
		VectorAdd(self->r.currentOrigin, self->r.mins, selfmin);
		VectorAdd(self->r.currentOrigin, self->r.maxs, selfmax);

		/* Windows20047014/200470a0 retain these two X sums in x87
		 * until FCOMP200470ce; the remaining axis sums spill to float. */
		if((double)hit->r.currentOrigin[0] + hit->r.mins[0] >
		   (double)self->r.currentOrigin[0] + self->r.maxs[0]) continue;
		if(hitmax[0] < selfmin[0]) continue;
		if(hitmin[1] > selfmax[1]) continue;
		if(hitmax[1] < selfmin[1]) continue;
		if(hitmin[2] > selfmax[2]) continue;
		if(hitmax[2] < selfmin[2]) continue;

		return(qtrue);
	}

	return(qfalse);
}

extern vec3_t	playerMins, playerMaxs;
#define WR_PUSHAMOUNT 25

void WolfRevivePushEnt( gentity_t *self, gentity_t *other ) {
	vec3_t	dir, push;

	VectorSubtract( self->r.currentOrigin, other->r.currentOrigin, dir );
	dir[2] = 0;
	VectorNormalizeFast( dir );

	VectorScale( dir, WR_PUSHAMOUNT, push );

	if ( self->client ) {
		VectorAdd( self->s.pos.trDelta, push, self->s.pos.trDelta );
		VectorAdd( self->client->ps.velocity, push, self->client->ps.velocity );
	}

	VectorScale( dir, -WR_PUSHAMOUNT, push );
	push[2] = WR_PUSHAMOUNT/2;

	VectorAdd( other->s.pos.trDelta, push, other->s.pos.trDelta );
	VectorAdd( other->client->ps.velocity, push, other->client->ps.velocity );
}

// Arnout: completely revived for capsules
void WolfReviveBbox( gentity_t *self ) {
	int			touch[MAX_GENTITIES];
	int			num,i, touchnum=0;
	gentity_t	*hit = NULL; // TTimo: init
	vec3_t		mins, maxs;

	hit = G_TestEntityPosition( self );

	if( hit && ( hit->s.number == ENTITYNUM_WORLD || ( hit->client && (hit->client->ps.persistant[PERS_HWEAPON_USE] || (hit->client->ps.eFlags & EF_MOUNTEDTANK))) ) ) {
		G_DPrintf( "WolfReviveBbox: Player stuck in world or MG42 using player\n" );
		// Move corpse directly to the person who revived them
		if ( self->props_frame_state >= 0 ) {
//			trap_UnlinkEntity( self );
			VectorCopy( g_entities[self->props_frame_state].client->ps.origin, self->client->ps.origin );
			VectorCopy( self->client->ps.origin, self->r.currentOrigin );
			trap_LinkEntity( self );

			// Reset value so we don't continue to warp them
			self->props_frame_state = -1;
		}
		return;
	}

	/* Original TC playerMins/Maxs and newBBox alternatives200bd794..7c0. */
    if (g_newbbox.integer) {
        VectorSet(mins, -16.f, -16.f, -30.f);
        VectorSet(maxs, 16.f, 16.f, 57.f);
    } else {
        VectorSet(mins, -14.f, -14.f, -24.f);
        VectorSet(maxs, 14.f, 14.f, 46.f);
    }
    VectorAdd(self->r.currentOrigin, mins, mins);
    VectorAdd(self->r.currentOrigin, maxs, maxs);

	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for (i=0 ; i<num ; i++) {
		hit = &g_entities[touch[i]];

		// Always use capsule for player
		if ( !trap_EntityContactCapsule( mins, maxs, hit ) ) {
		//if ( !trap_EntityContact( mins, maxs, hit ) ) {
			continue;
		}

		if ( hit->client && hit->health > 0 ) {
			if ( hit->s.number != self->s.number ) {
				WolfRevivePushEnt( hit, self );
				touchnum++;
			}
		} else if ( hit->r.contents & ( CONTENTS_SOLID | CONTENTS_BODY | CONTENTS_PLAYERCLIP ) ) {
			WolfRevivePushEnt( hit, self );
			touchnum++;
		}
	}

	G_DPrintf( "WolfReviveBbox: Touchnum: %d\n", touchnum );

	if ( touchnum == 0 ) {
		G_DPrintf( "WolfReviveBbox:  Player is solid now!\n" );
		self->r.contents = CONTENTS_BODY;
	}
}

/*
==============
ClientEndFrame

Called at the end of each server frame for each connected client
A fast client will have multiple ClientThink for each ClientEndFrame,
while a slow client may have multiple ClientEndFrame between ClientThink.
==============
*/
/* Whole Windows20047530, Linux0009ced4: end-of-frame state pipeline. */
void ClientEndFrame(gentity_t *ent) {
    int i;
    gentity_t *popup;
    gclient_t *client = ent->client;

    if (ent->health <= 0 && ent->health > -175 && client->ps.pm_time <= 0 &&
        ((ent->r.svFlags & SVF_BOT) || g_gametype.integer != 7) &&
        client->sess.sessionTeam != TEAM_SPECTATOR && !(client->ps.pm_flags & PMF_LIMBO)) {
        limbo(ent, qtrue);
    }
    client->ps.powerups[PW_BLACKOUT] = 0;
    if (client->sess.sessionTeam == TEAM_SPECTATOR || (client->ps.pm_flags & PMF_LIMBO)) {
        SpectatorClientEndFrame(ent);
        return;
    }
    for (i = 0; i < 16; ++i) {
        if ((i >= 2 && i <= 5) || (i >= 8 && i <= 11) || !client->ps.powerups[i]) continue;
        if (level.match_pause != PAUSE_NONE && client->ps.powerups[i] != INT_MAX)
            client->ps.powerups[i] += level.time - level.previousTime;
        if (client->ps.powerups[i] < level.time) client->ps.powerups[i] = 0;
    }
    /* TC +e8: stats6 stores the skill total; stats8 remains weapon flags. */
    client->ps.stats[6] = 0;
    for (i = 0; i < 7; ++i)
        /* 20047662..2004766b: FILD integer, FADD binary32 skill, then
         * truncate directly; no intervening binary32 rounding. */
        client->ps.stats[6] = (int)((double)client->ps.stats[6] + (double)client->sess.skillpoints[i]);
    if (level.match_pause != PAUSE_NONE) {
        int delta = level.time - level.previousTime;
        client->airOutTime += delta;
        client->inactivityTime += delta;
        client->lastBurnTime += delta;
        client->pers.connectTime += delta;
        client->pers.enterTime += delta;
        client->pers.teamState.lastreturnedflag += (float)delta;
        client->pers.teamState.lasthurtcarrier += (float)delta;
        client->pers.teamState.lastfraggedcarrier += (float)delta;
        client->ps.classWeaponTime += delta;
        ent->lastHintCheckTime += delta;
        ent->pain_debounce_time += delta;
        ent->s.onFireEnd += delta;
        client->tceObjectiveActivityUntil += delta;
    }
    if (level.intermissiontime) return;
    P_WorldEffects(ent);
    P_DamageFeedback(ent);
    if (level.time - client->lastCmdTime > 1000) ent->s.eFlags |= EF_CONNECTION;
    else ent->s.eFlags &= ~EF_CONNECTION;

    if (g_gametype.integer == 5 && (client->ps.stats[STAT_TCE_WEAPON_FLAGS] & 0x100) &&
        level.tceBombAssigned && !level.tceBombPlanted && level.tceBombCarrier == client->ps.clientNum &&
        (level.time - client->lastCmdTime > 5000 || client->tceObjectiveActivityUntil < level.time) &&
        level.tceBombDropCount < 2) {
        G_TCEDropBomb(ent);
        client->ps.stats[STAT_TCE_WEAPON_FLAGS] &= ~0x100;
        level.tceBombCarrierCount--;
        level.tceBombCarrier = -1;
        client->tceBombPossessionOrder = 0;
        popup = G_PopupMessage(PM_MESSAGE);
        popup->s.effect2Time = client->ps.persistant[PERS_TEAM];
        popup->s.density = 1;
        popup->s.effect3Time = client->ps.clientNum;
    }
    if ((client->ps.stats[STAT_TCE_WEAPON_FLAGS] & 0x100) && level.tceBombCarrier != client->ps.clientNum) {
        client->tceBombPossessionOrder = 0;
        client->ps.stats[STAT_TCE_WEAPON_FLAGS] &= ~0x100;
        G_Printf("ELITE DEBUG: removed invalid bomb carrier\n");
    }
    if (g_gametype.integer == 5 && (client->ps.stats[STAT_TCE_FLAGS] & 0x100) &&
        level.tceVipAssigned && level.tceVipCarrier == client->ps.clientNum &&
        (level.time - client->lastCmdTime > 5000 || client->tceObjectiveActivityUntil < level.time)) {
        G_TCEDropVip(ent);
        client->ps.stats[STAT_TCE_FLAGS] &= ~0x100;
        level.tceVipCarrier = -1;
        popup = G_PopupMessage(PM_MESSAGE);
        popup->s.effect2Time = client->ps.persistant[PERS_TEAM];
        popup->s.density = 4;
        popup->s.effect3Time = client->ps.clientNum;
    }
    if ((client->ps.stats[STAT_TCE_FLAGS] & 0x100) && level.tceVipCarrier != client->ps.clientNum) {
        level.tceVipCarrier = -1;
        client->ps.stats[STAT_TCE_FLAGS] &= ~0x100;
        G_Printf("ELITE DEBUG: removed invalid VIP\n");
    }
    client->ps.stats[STAT_HEALTH] = ent->health;
    G_SetClientSound(ent);
    if (g_smoothClients.integer)
        BG_PlayerStateToEntityStateExtraPolate(&client->ps, &ent->s, level.time, qfalse);
    else BG_PlayerStateToEntityState(&client->ps, &ent->s, qfalse);
    if (ent->props_frame_state >= 0 && level.time - ent->s.effect3Time > 100)
        ent->props_frame_state = -1;
    if (ent->health > 0) {
        if (StuckInClient(ent)) {
            G_DPrintf("%s is stuck in a client.\n", client->pers.netname);
            ent->r.contents = CONTENTS_CORPSE;
        }
        if (ent->health > 0 && ent->r.contents == CONTENTS_CORPSE && !(ent->s.eFlags & EF_MOUNTEDTANK))
            WolfReviveBbox(ent);
    }
    if (!(client->buttons & BUTTON_ATTACK)) ent->count2 = 0;
    G_TouchTriggers(ent);
    G_Script_ScriptRun(ent);
    G_TCEStoreFramePosition(ent);
}
