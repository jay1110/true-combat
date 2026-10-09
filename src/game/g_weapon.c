/*
 * name:		g_weapon.c
 *
 * desc:		perform the server side effects of a weapon firing
 *
*/


#include "g_local.h"
#include "tce_lua.h"
#ifdef FEATURE_OMNIBOT
#include "g_etbot_interface.h"
#endif
#include "tce_bullet.h"


vec3_t	forward, right, up;
vec3_t	muzzleEffect;
vec3_t	muzzleTrace;

// forward dec
void Bullet_Fire (gentity_t *ent, float spread, int damage, qboolean distance_falloff);
qboolean Bullet_Fire_Extended(gentity_t *source, gentity_t *attacker,
    vec3_t start, vec3_t end, float spread, int damage, int penetration,
    int maxDistance, int totalDistance, int wallsRemaining, int bodiesRemaining,
    int passesRemaining, int seed, int suppressWallEvents);

int G_GetWeaponDamage( int weapon ); // JPW

qboolean G_WeaponIsExplosive( meansOfDeath_t mod )
{
	switch( mod ) {
		case MOD_GRENADE_LAUNCHER:
		case MOD_GRENADE_PINEAPPLE:
		case MOD_PANZERFAUST:
		case MOD_LANDMINE:
		case MOD_GPG40:
		case MOD_M7:
		case MOD_ARTY:
		case MOD_AIRSTRIKE:
		case MOD_MORTAR:
		case MOD_SATCHEL:
		case MOD_DYNAMITE:
		// map entity based explosions
		case MOD_GRENADE:
		case MOD_MAPMORTAR:
		case MOD_MAPMORTAR_SPLASH:
		case MOD_EXPLOSIVE:
		case MOD_TELEFRAG: // Gordon: yes this _SHOULD_ be here, kthxbye
		case MOD_CRUSH:
			return qtrue;
		default:
			return qfalse;
	}
}

int G_GetWeaponClassForMOD( meansOfDeath_t mod )
{
	switch( mod ) {
		case MOD_GRENADE_LAUNCHER:
		case MOD_GRENADE_PINEAPPLE:
		case MOD_PANZERFAUST:
		case MOD_LANDMINE:
		case MOD_GPG40:
		case MOD_M7:
		case MOD_ARTY:
		case MOD_AIRSTRIKE:
		case MOD_MORTAR:
		// map entity based explosions
		case MOD_GRENADE:
		case MOD_MAPMORTAR:
		case MOD_MAPMORTAR_SPLASH:
		case MOD_EXPLOSIVE:
			return 0;
		case MOD_SATCHEL:
			return 1;
		case MOD_DYNAMITE:
			return 2;
		default:
			return -1;
	}
}

#define NUM_NAILSHOTS 10

/*
======================================================================

KNIFE/GAUNTLET (NOTE: gauntlet is now the Zombie melee)

======================================================================
*/

#define KNIFE_DIST 48

// Let's use the same angle between function we've used before
extern float sAngleBetweenVectors(vec3_t a, vec3_t b);

/*
==============
Weapon_Knife
==============
*/
void Weapon_Knife( gentity_t *ent ) {
	trace_t		tr;
	gentity_t	*traceEnt, *tent;
	int			damage, mod;
	vec3_t		pforward, eforward;

	vec3_t		end;

	mod = 6; /* TC knife MOD; not the SDK protocol value. */

	AngleVectors (ent->client->ps.viewangles, forward, right, up);
	CalcMuzzlePoint ( ent, ent->s.weapon, forward, right, up, muzzleTrace );
	VectorMA (muzzleTrace, KNIFE_DIST, forward, end);
	trap_Trace(&tr, muzzleTrace, NULL, NULL, end, ent->s.number, MASK_SHOT);

	if ( tr.surfaceFlags & SURF_NOIMPACT )
		return;

	// no contact
	if(tr.fraction == 1.0f)
		return;

	if(tr.entityNum >= MAX_CLIENTS) {	// world brush or non-player entity (no blood)
		tent = G_TempEntity( tr.endpos, EV_MISSILE_MISS );
	} else {							// other player
		tent = G_TempEntity( tr.endpos, EV_MISSILE_HIT );
	}

	tent->s.otherEntityNum = tr.entityNum;
	tent->s.eventParm = DirToByte( tr.plane.normal );
	tent->s.weapon = ent->s.weapon;
	tent->s.clientNum = ent->r.ownerNum;
	tent->s.otherEntityNum2 = BG_SurfaceFlag2Type( tr.surfaceFlags );
	tent->s.modelindex2 = 0;

	if(tr.entityNum == ENTITYNUM_WORLD) {
		tent->s.modelindex2 = 1;
		return;
	}

	traceEnt = &g_entities[ tr.entityNum ];

	if(!(traceEnt->takedamage))
		return;

	damage = G_GetWeaponDamage(ent->s.weapon); // JPW		// default knife damage for frontal attacks

	/* CHECK WITH PAUL */
	if( ent->client->sess.playerType == PC_COVERTOPS )
		damage *= 2;	// Watch it - you could hurt someone with that thing!

	if(traceEnt->client) 
	{
		AngleVectors (ent->client->ps.viewangles,		pforward, NULL, NULL);
		AngleVectors (traceEnt->client->ps.viewangles,	eforward, NULL, NULL);

		if( DotProduct( eforward, pforward ) > 0.6f )		// from behind(-ish)
		{
			damage = 100;	// enough to drop a 'normal' (100 health) human with one jab
			mod = 6;

			// rain - only do this if they have a positive health
			if ( traceEnt->health > 0 && ent->client->sess.skill[SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS] >= 4 ) {
				damage = traceEnt->health;
			}
		}
	}

	G_Damage( traceEnt, ent, ent, vec3_origin, tr.endpos, (damage + rand()%5), 0, mod);
}

// JPW NERVE

//make it TR_LINEAR so it doesnt chew bandwidth...
void MagicSink( gentity_t *self ) {
    self->clipmask = 0;
    self->r.contents = 0;

    self->nextthink = level.time + 4000;
    self->think = G_FreeEntity;

    self->s.pos.trType = TR_LINEAR;
    self->s.pos.trTime = level.time;
    VectorCopy( self->r.currentOrigin, self->s.pos.trBase );
    VectorSet( self->s.pos.trDelta, 0, 0, -5 );
}

/*
======================
  Weapon_Class_Special
	class-specific in multiplayer
======================
*/
// JPW NERVE
void Weapon_Medic( gentity_t *ent ) {
	gitem_t *item;
	gentity_t *ent2;
	vec3_t	velocity, offset;
	vec3_t	angles,mins,maxs;
	vec3_t	tosspos, viewpos;
	trace_t	tr;

	if (level.time - ent->client->ps.classWeaponTime > level.medicChargeTime[ent->client->sess.sessionTeam-1]) {
		ent->client->ps.classWeaponTime = level.time - level.medicChargeTime[ent->client->sess.sessionTeam-1];
	}
	
	if( ent->client->sess.skill[SK_FIRST_AID] >= 2 ) {
		ent->client->ps.classWeaponTime += level.medicChargeTime[ent->client->sess.sessionTeam-1]*0.15;
	} else {
		ent->client->ps.classWeaponTime += level.medicChargeTime[ent->client->sess.sessionTeam-1]*0.25;
	}

	item = BG_FindItemForClassName("item_health");
	VectorCopy( ent->client->ps.viewangles, angles );

	// clamp pitch
	if ( angles[PITCH] < -30 ) {
		angles[PITCH] = -30;
	} else if ( angles[PITCH] > 30 ) {
		angles[PITCH] = 30;
	}

	AngleVectors( angles, velocity, NULL, NULL );
	VectorScale( velocity, 64, offset);
	offset[2] += ent->client->ps.viewheight/2;
	VectorScale( velocity, 75, velocity );
	velocity[2] += 50 + crandom() * 25;

	VectorCopy( muzzleEffect, tosspos );
	VectorMA( tosspos, 48, forward, tosspos );
	VectorCopy( ent->client->ps.origin, viewpos );

	VectorSet( mins, -(ITEM_RADIUS + 8), -(ITEM_RADIUS+8), 0 );
	VectorSet( maxs, (ITEM_RADIUS + 8), (ITEM_RADIUS+8), 2*(ITEM_RADIUS+8) );

	trap_EngineerTrace( &tr, viewpos, mins, maxs, tosspos, ent->s.number, MASK_MISSILESHOT );
	if( tr.startsolid ) {
		// Arnout: this code is a bit more solid than the previous code
		VectorCopy( forward, viewpos );
		VectorNormalizeFast( viewpos );
		VectorMA( ent->r.currentOrigin, -24.f, viewpos, viewpos ); 

		trap_EngineerTrace(&tr, viewpos, mins, maxs, tosspos, ent->s.number, MASK_MISSILESHOT);

		VectorCopy( tr.endpos, tosspos );
	} else if( tr.fraction < 1 ) {	// oops, bad launch spot
		VectorCopy( tr.endpos, tosspos );
		SnapVectorTowards( tosspos, viewpos );
	}

    ent2 = LaunchItem( item, tosspos, velocity, ent->s.number );
    ent2->think = MagicSink;
    ent2->nextthink = level.time + 30000;
//	ent2->timestamp = level.time + 31200;

	ent2->parent = ent; // JPW NERVE so we can score properly later
	//ent2->count = 20;
}

/*
==========
G_PlaceTripmine
==========
*/
void G_PlaceTripmine(gentity_t* ent) {
	vec3_t start, end;
	trace_t trace;
	gentity_t* bomb;
	vec3_t forward;

	VectorCopy( ent->client->ps.origin, start );
	start[2] += ent->client->ps.viewheight;

	AngleVectors(ent->client->ps.viewangles, forward, NULL, NULL);

	VectorMA(start, 64, forward, end);

	/* TC 200983b0: placement and the beam callback both trace 0x6000081. */
	trap_Trace(&trace, start, NULL, NULL, end, ent->s.number, MASK_MISSILESHOT);

	bomb = G_Spawn();
	bomb->r.svFlags	= SVF_BROADCAST;
	bomb->s.eType = ET_BOMB;
	bomb->s.eFlags = 0;
	bomb->s.weapon = WP_TRIPMINE;
	bomb->parent = ent;
	bomb->think = G_TripMinePrime;
	bomb->nextthink = level.time + 2000;
	bomb->splashDamage = 300;
	bomb->splashRadius = 300;
	bomb->methodOfDeath = MOD_TRIPMINE;
	bomb->splashMethodOfDeath = MOD_TRIPMINE;
	bomb->r.contents = CONTENTS_CORPSE;	// (player can walk through)

	VectorSet(bomb->r.mins, -12, -12, 0);
	VectorCopy(bomb->r.mins, bomb->r.absmin);
	VectorSet(bomb->r.maxs, 12, 12, 20);
	VectorCopy(bomb->r.maxs, bomb->r.absmax);

	VectorMA(trace.endpos, 1, trace.plane.normal, start);
	G_SetOrigin(bomb, start);
	G_SetAngle(bomb, vec3_origin);

	VectorCopy(trace.plane.normal, bomb->s.origin2);

	trap_LinkEntity(bomb);
}

/*void Weapon_SatchelCharge(gentity_t *ent) {
	gitem_t *item;
	gentity_t *ent2;
	vec3_t	velocity, org, offset;
	vec3_t	angles,mins,maxs;
	trace_t	tr;

	item = BG_FindItem("Satchel Charge");
	VectorCopy( ent->client->ps.viewangles, angles );

	// clamp pitch
	if ( angles[PITCH] < -30 )
		angles[PITCH] = -30;
	else if ( angles[PITCH] > 30 )
		angles[PITCH] = 30;

	AngleVectors( angles, velocity, NULL, NULL );
	VectorScale( velocity, 64, offset);
	offset[2] += ent->client->ps.viewheight/2;
	VectorScale( velocity, 75, velocity );
	velocity[2] += 50 + crandom() * 25;

	VectorAdd(ent->client->ps.origin,offset,org);

	VectorSet( mins, -ITEM_RADIUS, -ITEM_RADIUS, 0 );
	VectorSet( maxs, ITEM_RADIUS, ITEM_RADIUS, 2*ITEM_RADIUS );

	trap_Trace (&tr, ent->client->ps.origin, mins, maxs, org, ent->s.number, MASK_SOLID);
	VectorCopy( tr.endpos, org );

	ent2 = LaunchItem( item, org, velocity, ent->s.number );
	ent2->think = MagicSink;
	ent2->timestamp = level.time + 4000;
	ent2->parent = ent;
	ent2->s.eType = ET_MISSILE;
	ent2->methodOfDeath = MOD_SATCHEL;
	ent2->splashMethodOfDeath = MOD_SATCHEL;
	ent2->s.weapon = WP_SATCHEL;
	ent2->touch = 0;
}
*/

// JPW NERVE
/*
==================
Weapon_MagicAmmo
==================
*/
void Weapon_MagicAmmo( gentity_t *ent )  {
	gitem_t *item;
	gentity_t *ent2;
	vec3_t	velocity, offset;
	vec3_t	tosspos, viewpos;
	vec3_t	angles,mins,maxs;
	trace_t	tr;

	if (level.time - ent->client->ps.classWeaponTime > level.lieutenantChargeTime[ent->client->sess.sessionTeam-1])
		ent->client->ps.classWeaponTime = level.time - level.lieutenantChargeTime[ent->client->sess.sessionTeam-1];

	if( ent->client->sess.skill[SK_SIGNALS] >= 1 ) {
		ent->client->ps.classWeaponTime += level.lieutenantChargeTime[ent->client->sess.sessionTeam-1]*0.15;
	} else {
		ent->client->ps.classWeaponTime += level.lieutenantChargeTime[ent->client->sess.sessionTeam-1]*0.25;
	}

	item = BG_FindItem( ent->client->sess.skill[SK_SIGNALS] >= 1 ? "Mega Ammo Pack" : "Ammo Pack" );	
	VectorCopy( ent->client->ps.viewangles, angles );

	// clamp pitch
	if ( angles[PITCH] < -30 )
		angles[PITCH] = -30;
	else if ( angles[PITCH] > 30 )
		angles[PITCH] = 30;

	AngleVectors( angles, velocity, NULL, NULL );
	VectorScale( velocity, 64, offset);
	offset[2] += ent->client->ps.viewheight/2;
	VectorScale( velocity, 75, velocity );
	velocity[2] += 50 + crandom() * 25;

	VectorCopy( muzzleEffect, tosspos );
	VectorMA( tosspos, 48, forward, tosspos );
	VectorCopy( ent->client->ps.origin, viewpos );

	VectorSet( mins, -(ITEM_RADIUS + 8), -(ITEM_RADIUS+8), 0 );
	VectorSet( maxs, (ITEM_RADIUS + 8), (ITEM_RADIUS+8), 2*(ITEM_RADIUS+8) );

	trap_EngineerTrace( &tr, viewpos, mins, maxs, tosspos, ent->s.number, MASK_MISSILESHOT );
	if( tr.startsolid ) {
		// Arnout: this code is a bit more solid than the previous code
		VectorCopy( forward, viewpos );
		VectorNormalizeFast( viewpos );
		VectorMA( ent->r.currentOrigin, -24.f, viewpos, viewpos ); 

		trap_EngineerTrace (&tr, viewpos, mins, maxs, tosspos, ent->s.number, MASK_MISSILESHOT);

		VectorCopy( tr.endpos, tosspos );
	} else if( tr.fraction < 1 ) {	// oops, bad launch spot
		VectorCopy( tr.endpos, tosspos );
		SnapVectorTowards( tosspos, viewpos );
	}

    ent2 = LaunchItem( item, tosspos, velocity, ent->s.number );
    ent2->think = MagicSink;
    ent2->nextthink = level.time + 30000;
//	ent2->timestamp = level.time + 31200;

	ent2->parent = ent;

	if( ent->client->sess.skill[SK_SIGNALS] >= 1 ) {
		ent2->count = 2;
		ent2->s.density = 2;
	} else {
		ent2->count = 1;
		ent2->s.density = 1;
	}
}
// jpw



// START - Mad Doc - TDF
// took this out of Weapon_Syringe so we can use it from other places
qboolean ReviveEntity(gentity_t *ent, gentity_t *traceEnt)
{
	vec3_t		org;
	trace_t		tr;
	int			healamt, headshot, oldweapon,oldweaponstate,oldclasstime=0;
	qboolean	usedSyringe = qfalse;		// DHM - Nerve
	int			ammo[MAX_WEAPONS];		// JPW NERVE total amount of ammo
	int			ammoclip[MAX_WEAPONS];	// JPW NERVE ammo in clip
	int			weapons[MAX_WEAPONS/(sizeof(int)*8)];	// JPW NERVE 64 bits for weapons held
//	gentity_t	*traceEnt,
	gentity_t	*te;




	// heal the dude
	// copy some stuff out that we'll wanna restore
	VectorCopy(traceEnt->client->ps.origin, org);
	headshot = traceEnt->client->ps.eFlags & EF_HEADSHOT;
	if( ent->client->sess.skill[SK_FIRST_AID] >= 3 )
		healamt = traceEnt->client->ps.stats[STAT_MAX_HEALTH];
	else
		healamt = traceEnt->client->ps.stats[STAT_MAX_HEALTH] * 0.5;
	oldweapon = traceEnt->client->ps.weapon;
	oldweaponstate = traceEnt->client->ps.weaponstate;

	// keep class special weapon time to keep them from exploiting revives
	oldclasstime = traceEnt->client->ps.classWeaponTime;

	memcpy(ammo,traceEnt->client->ps.ammo,sizeof(int)*MAX_WEAPONS);
	memcpy(ammoclip,traceEnt->client->ps.ammoclip,sizeof(int)*MAX_WEAPONS);
	memcpy(weapons,traceEnt->client->ps.weapons,sizeof(int)*(MAX_WEAPONS/(sizeof(int)*8)));

	ClientSpawn(traceEnt, qtrue, qfalse);

	traceEnt->client->ps.stats[STAT_PLAYER_CLASS] = traceEnt->client->sess.playerType;
	memcpy(traceEnt->client->ps.ammo,ammo,sizeof(int)*MAX_WEAPONS);
	memcpy(traceEnt->client->ps.ammoclip,ammoclip,sizeof(int)*MAX_WEAPONS);
	memcpy(traceEnt->client->ps.weapons,weapons,sizeof(int)*(MAX_WEAPONS/(sizeof(int)*8)));

	if( headshot ) {
		traceEnt->client->ps.eFlags |= EF_HEADSHOT;
	}
	traceEnt->client->ps.weapon = oldweapon;
	traceEnt->client->ps.weaponstate = oldweaponstate;

	// set idle animation on weapon
	traceEnt->client->ps.weapAnim = ( ( traceEnt->client->ps.weapAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | PM_IdleAnimForWeapon( traceEnt->client->ps.weapon );

	traceEnt->client->ps.classWeaponTime = oldclasstime;

	traceEnt->health = healamt;
	VectorCopy(org,traceEnt->s.origin);
	VectorCopy(org,traceEnt->r.currentOrigin);
	VectorCopy(org,traceEnt->client->ps.origin);

	trap_Trace(&tr, traceEnt->client->ps.origin, traceEnt->client->ps.mins, traceEnt->client->ps.maxs, traceEnt->client->ps.origin, traceEnt->s.number, MASK_PLAYERSOLID);
	if ( tr.allsolid ) {
		traceEnt->client->ps.pm_flags |= PMF_DUCKED;
	}

	traceEnt->r.contents = CONTENTS_CORPSE;
	trap_LinkEntity( ent );


	// DHM - Nerve :: Let the person being revived know about it
	trap_SendServerCommand( traceEnt-g_entities, va("cp \"You have been revived by [lof]%s[lon] [lof]%s!\n\"", ent->client->sess.sessionTeam == TEAM_ALLIES ? rankNames_Allies[ ent->client->sess.rank ] : rankNames_Axis[ ent->client->sess.rank ], ent->client->pers.netname) );
	traceEnt->props_frame_state = ent->s.number;

	// DHM - Nerve :: Mark that the medicine was indeed dispensed
	usedSyringe = qtrue;

	// sound
	te = G_TempEntity( traceEnt->r.currentOrigin, EV_GENERAL_SOUND );
	te->s.eventParm = G_SoundIndex( "sound/misc/vo_revive.wav" );

	// Xian -- This was gay and I always hated it.
	if ( g_fastres.integer > 0 )
		BG_AnimScriptEvent( &traceEnt->client->ps, traceEnt->client->pers.character->animModelInfo, ANIM_ET_JUMP, qfalse, qtrue );
	else {
		// DHM - Nerve :: Play revive animation
		BG_AnimScriptEvent( &traceEnt->client->ps, traceEnt->client->pers.character->animModelInfo, ANIM_ET_REVIVE, qfalse, qtrue );
		traceEnt->client->ps.pm_flags |= PMF_TIME_LOCKPLAYER;
		traceEnt->client->ps.pm_time = 2100;
	}

	// Tell the caller if we actually used a syringe
	return usedSyringe;

}
// END - Mad Doc


// JPW NERVE Weapon_Syringe:
/*
======================
  Weapon_Syringe
	shoot the syringe, do the old lazarus bit
======================
*/
void Weapon_Syringe(gentity_t *ent) {
	vec3_t		end;
	trace_t		tr;
	qboolean	usedSyringe = qfalse;		// DHM - Nerve
	gentity_t	*traceEnt;

	AngleVectors (ent->client->ps.viewangles, forward, right, up);
	CalcMuzzlePointForActivate( ent, forward, right, up, muzzleTrace );
	VectorMA (muzzleTrace, 48, forward, end);			// CH_ACTIVATE_DIST
	//VectorMA (muzzleTrace, -16, forward, muzzleTrace);	// DHM - Back up the start point in case medic is
														// right on top of intended revivee.
	// TC 20098ca0: both syringe probes include CONTENTS_MISSILECLIP.
	trap_Trace (&tr, muzzleTrace, NULL, NULL, end, ent->s.number, MASK_MISSILESHOT);

	if (tr.startsolid) {
		VectorMA (muzzleTrace, 8, forward, end);			// CH_ACTIVATE_DIST
		trap_Trace(&tr, muzzleTrace, NULL, NULL, end, ent->s.number, MASK_MISSILESHOT);
	}

	if (tr.fraction < 1.0) {
		traceEnt = &g_entities[ tr.entityNum ];
		if (traceEnt->client != NULL) {

			if ( (traceEnt->client->ps.pm_type == PM_DEAD) && (traceEnt->client->sess.sessionTeam == ent->client->sess.sessionTeam)) {
				// Mad Doc - TDF moved all the revive stuff into its own function
				usedSyringe = ReviveEntity( ent, traceEnt );

				// OSP - syringe "hit"
				if(g_gamestate.integer == GS_PLAYING) ent->client->sess.aWeaponStats[WS_SYRINGE].hits++;
				if(ent && ent->client) G_LogPrintf("Medic_Revive: %d %d\n", ent - g_entities, traceEnt - g_entities);	// OSP

				if( !traceEnt->isProp ) { // Gordon: flag for if they were teamkilled or not
					AddScore(ent, WOLF_MEDIC_BONUS); // JPW NERVE props to the medic for the swift and dexterous bit o healitude

					G_AddSkillPoints( ent, SK_FIRST_AID, 4.f );
					G_DebugAddSkillPoints( ent, SK_FIRST_AID, 4.f, "reviving a player" );
				}

				// Arnout: calculate ranks to update numFinalDead arrays. Have to do it manually as addscore has an early out
				if( g_gametype.integer == GT_WOLF_LMS ) {
					CalculateRanks();
				}
			}
		}
	}

	// DHM - Nerve :: If the medicine wasn't used, give back the ammo
	if (!usedSyringe)
		ent->client->ps.ammoclip[BG_FindClipForWeapon(WP_MEDIC_SYRINGE)] += 1;
}
// jpw

/*
======================
  Weapon_AdrenalineSyringe
	Hmmmm. Needles. With stuff in it. Woooo.
======================
*/
void Weapon_AdrenalineSyringe(gentity_t *ent) {
	ent->client->ps.powerups[PW_ADRENALINE] = level.time + 10000;
}

void G_ExplodeMissile( gentity_t *ent );
void DynaSink(gentity_t* self );

/* TC20098f1a..98f97: clamp by C0 and retain the enlarged radius in ST0. */
static void G_TCERadiusCandidateBounds( const vec3_t radiusOrigin, float *candidateRadius,
                                      vec3_t candidateMins, vec3_t candidateMaxs ) {
#if defined(_MSC_VER) && defined(_M_IX86)
    const float radiusMinimum = 1.0f;
    const double radiusExpansion = 1.41421356;
    __asm {
        mov edx, candidateRadius
        fld dword ptr [edx]
        fcomp dword ptr radiusMinimum
        fnstsw ax
        test ah, 1
        jz radius_bounds_ready
        mov dword ptr [edx], 03f800000h
radius_bounds_ready:
        fld dword ptr [edx]
        fmul qword ptr radiusExpansion
        mov eax, radiusOrigin
        mov ecx, candidateMins
        mov edx, candidateMaxs
        fld dword ptr [eax]
        fsub st(0), st(1)
        fstp dword ptr [ecx]
        fld st(0)
        fadd dword ptr [eax]
        fstp dword ptr [edx]
        fld dword ptr [eax+4]
        fsub st(0), st(1)
        fstp dword ptr [ecx+4]
        fld st(0)
        fadd dword ptr [eax+4]
        fstp dword ptr [edx+4]
        fld dword ptr [eax+8]
        fsub st(0), st(1)
        fstp dword ptr [ecx+8]
        fld st(0)
        fadd dword ptr [eax+8]
        fstp dword ptr [edx+8]
        fstp st(0)
    }
#else
    int radiusAxis;
    float expandedRadius;
    if( *candidateRadius < 1.0f ) *candidateRadius = 1.0f;
    expandedRadius = 1.41421356 * *candidateRadius;
    for( radiusAxis = 0; radiusAxis < 3; ++radiusAxis ) {
        candidateMins[radiusAxis] = radiusOrigin[radiusAxis] - expandedRadius;
        candidateMaxs[radiusAxis] = radiusOrigin[radiusAxis] + expandedRadius;
    }
#endif
}

/* Windows2009901e..56: unordered first comparison takes the lower-bound arm. */
static void G_TCERadiusAxisDistance( float axisOrigin, float axisMin, float axisMax, float *axisDistance ) {
#if defined(_MSC_VER) && defined(_M_IX86)
    __asm {
        mov edx, axisDistance
        fld dword ptr axisOrigin
        fcomp dword ptr axisMin
        fnstsw ax
        test ah, 1
        jz radius_axis_upper
        fld dword ptr axisMin
        fsub dword ptr axisOrigin
        fstp dword ptr [edx]
        jmp radius_axis_done
radius_axis_upper:
        fld dword ptr axisOrigin
        fcomp dword ptr axisMax
        fnstsw ax
        test ah, 041h
        jnz radius_axis_zero
        fld dword ptr axisOrigin
        fsub dword ptr axisMax
        fstp dword ptr [edx]
        jmp radius_axis_done
radius_axis_zero:
        mov dword ptr [edx], 0
radius_axis_done:
    }
#else
    *axisDistance = axisOrigin < axisMin ? axisMin - axisOrigin :
                    axisOrigin > axisMax ? axisOrigin - axisMax : 0.0f;
#endif
}

/* Consume the actual VectorLength ST0 return without a C float-result spill. */
static qboolean G_TCERadiusLengthGate( const vec3_t radiusDelta, float radiusLimit, qboolean closeGate ) {
#if defined(_MSC_VER) && defined(_M_IX86)
    const float closeScale = 0.2f;
    unsigned short radiusStatus;
    if( closeGate ) {
        __asm {
            push radiusDelta
            call VectorLength
            fld dword ptr radiusLimit
            fmul dword ptr closeScale
            add esp, 4
            fcompp
            fnstsw ax
            mov radiusStatus, ax
        }
        return (radiusStatus & 0x4100) == 0;
    }
    __asm {
        push radiusDelta
        call VectorLength
        fcomp dword ptr radiusLimit
        add esp, 4
        fnstsw ax
        mov radiusStatus, ax
    }
    return (radiusStatus & 0x100) != 0;
#else
    float radiusDistance = VectorLength( radiusDelta );
    return closeGate ? radiusDistance < radiusLimit * 0.2f : !(radiusDistance >= radiusLimit);
#endif
}

static void G_TCERadiusMidpoint( const vec3_t radiusMin, const vec3_t radiusMax, vec3_t radiusMidpoint ) {
#if defined(_MSC_VER) && defined(_M_IX86)
    const double midpointHalf = 0.5;
    float midpointY, midpointZ;
    __asm {
        mov eax, radiusMax
        mov ecx, radiusMin
        mov edx, radiusMidpoint
        fld dword ptr [eax]
        fadd dword ptr [ecx]
        fld dword ptr [eax+4]
        fadd dword ptr [ecx+4]
        fstp dword ptr midpointY
        fld dword ptr [eax+8]
        fadd dword ptr [ecx+8]
        fstp dword ptr midpointZ
        fmul qword ptr midpointHalf
        fld dword ptr midpointY
        fmul qword ptr midpointHalf
        fstp dword ptr midpointY
        fld dword ptr midpointZ
        fmul qword ptr midpointHalf
        mov eax, midpointY
        mov dword ptr [edx+4], eax
        fstp dword ptr midpointZ
        mov ecx, midpointZ
        fstp dword ptr [edx]
        mov dword ptr [edx+8], ecx
    }
#else
    VectorAdd( radiusMin, radiusMax, radiusMidpoint );
    VectorScale( radiusMidpoint, 0.5f, radiusMidpoint );
#endif
}

static qboolean G_TCERadiusTraceBlocked( float radiusFraction ) {
#if defined(_MSC_VER) && defined(_M_IX86)
    const double traceComplete = 1.0;
    unsigned short radiusStatus;
    __asm {
        fld dword ptr radiusFraction
        fcomp qword ptr traceComplete
        fnstsw ax
        mov radiusStatus, ax
    }
    return (radiusStatus & 0x100) != 0;
#else
    return radiusFraction < 1.0f;
#endif
}

// Arnout: crude version of G_RadiusDamage to see if the dynamite can damage a func_constructible
int EntsThatRadiusCanDamage( vec3_t origin, float radius, int *damagedList ) {
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	int			i, e;
	vec3_t		dest; 
	trace_t		tr;
	int			numDamaged = 0;

	G_TCERadiusCandidateBounds( origin, &radius, mins, maxs );

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for ( e = 0 ; e < numListedEntities ; e++ ) {
		ent = &g_entities[entityList[ e ]];

		if (!ent->r.bmodel)
			VectorSubtract(ent->r.currentOrigin,origin,v);
		else {
			for ( i = 0 ; i < 3 ; i++ ) {
				G_TCERadiusAxisDistance( origin[i], ent->r.absmin[i], ent->r.absmax[i], &v[i] );
			}
		}

		if ( !G_TCERadiusLengthGate( v, radius, qfalse ) ) {
			continue;
		}

		if( CanDamage (ent, origin) ) {
			damagedList[numDamaged++] = entityList[e];
		} else {
			G_TCERadiusMidpoint( ent->r.absmin, ent->r.absmax, dest );
			
			trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID);
			if (G_TCERadiusTraceBlocked( tr.fraction )) {
				VectorSubtract(dest,origin,dest);
				if (G_TCERadiusLengthGate( dest, radius, qtrue )) { // closer than 1/4 dist
					damagedList[numDamaged++] = entityList[e];
				}
			}
		}
	}

	return( numDamaged );
}

void G_LandminePrime( gentity_t *self );
extern void explosive_indicator_think( gentity_t *ent );

#define MIN_BLOCKINGWARNING_INTERVAL 5000

static void MakeTemporarySolid( gentity_t *ent ) {
	if( ent->entstate == STATE_UNDERCONSTRUCTION ) {
		ent->clipmask = ent->realClipmask;
		ent->r.contents = ent->realContents;
		if( !ent->realNonSolidBModel )
			ent->s.eFlags &= ~EF_NONSOLID_BMODEL;
	}

	trap_LinkEntity( ent );
}

static void UndoTemporarySolid( gentity_t *ent ) {
	ent->entstate = STATE_UNDERCONSTRUCTION;
	ent->s.powerups = STATE_UNDERCONSTRUCTION;
	ent->realClipmask = ent->clipmask;
	ent->clipmask = 0;
	ent->realContents = ent->r.contents;
	ent->r.contents = 0;
	if( ent->s.eFlags & EF_NONSOLID_BMODEL )
		ent->realNonSolidBModel = qtrue;
	else
		ent->s.eFlags |= EF_NONSOLID_BMODEL;

	trap_LinkEntity( ent );
}

// handleBlockingEnts = kill players, return flags, remove entities
// warnBlockingPlayers = warn any players that are in the constructible area
static void HandleEntsThatBlockConstructible( gentity_t *constructor, gentity_t *constructible, qboolean handleBlockingEnts, qboolean warnBlockingPlayers ) {
	// check if something blocks us
	int constructibleList[MAX_GENTITIES];
	int entityList[MAX_GENTITIES];
	int blockingList[MAX_GENTITIES];
	int constructibleEntities = 0;
	int listedEntities, e;
	int blockingEntities = 0;
	gentity_t *check, *block;

	// backup...
	int constructibleModelindex = constructible->s.modelindex;
	int constructibleClipmask = constructible->clipmask;
	int constructibleContents = constructible->r.contents;
	int constructibleNonSolidBModel = (constructible->s.eFlags & EF_NONSOLID_BMODEL);

	trap_SetBrushModel( constructible, va( "*%i", constructible->s.modelindex2 ) );

	// ...and restore
	constructible->clipmask = constructibleClipmask;
	constructible->r.contents = constructibleContents;
	if( !constructibleNonSolidBModel )
		constructible->s.eFlags &= ~EF_NONSOLID_BMODEL;
	trap_LinkEntity( constructible );

	// store our origin
	VectorCopy( constructible->r.absmin, constructible->s.origin2 );
	VectorAdd( constructible->r.absmax, constructible->s.origin2, constructible->s.origin2 );
	VectorScale( constructible->s.origin2, 0.5, constructible->s.origin2 );

	// get all the entities that make up the constructible
	if( constructible->track && constructible->track[0] ) {
		vec3_t mins, maxs;

		VectorCopy( constructible->r.absmin, mins );
		VectorCopy( constructible->r.absmax, maxs );

		check = NULL;

		while(1) {
			check = G_Find( check, FOFS(track), constructible->track );

			if( check == constructible )
				continue;

			if (!check ) {
				break;
			}

			if( constructible->count2 ) {
				if( check->partofstage != constructible->grenadeFired )
					continue;
			}

			// get the bounding box of all entities in the constructible together
			AddPointToBounds( check->r.absmin, mins, maxs );
			AddPointToBounds( check->r.absmax, mins, maxs );

			constructibleList[constructibleEntities++] = check->s.number;
		}

		listedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

		// make our constructible entities solid so we can check against them
		//trap_LinkEntity( constructible );
		MakeTemporarySolid( constructible );
		for( e = 0; e < constructibleEntities; e++ ) {
			check = &g_entities[constructibleList[e]];

			//trap_LinkEntity( check );
			MakeTemporarySolid( check );
		}

	} else {
		// Gordon: changed * to abs*
		listedEntities = trap_EntitiesInBox( constructible->r.absmin, constructible->r.absmax, entityList, MAX_GENTITIES );

		// make our constructible solid so we can check against it
		//trap_LinkEntity( constructible );
		MakeTemporarySolid( constructible );
	}

	for( e = 0; e < listedEntities; e++ ) {
		check = &g_entities[entityList[e]];

		// ignore everything but items, players and missiles (grenades too)
		if( check->s.eType != ET_MISSILE && check->s.eType != ET_ITEM && check->s.eType != ET_PLAYER && !check->physicsObject ) {
			continue;
		}

		// remove any corpses, this includes dynamite
		if( check->r.contents == CONTENTS_CORPSE ) {
			blockingList[blockingEntities++] = entityList[e];
			continue;
		}

		// FIXME : dynamite seems to test out of position?
		// see if the entity is in a solid now
		if((block = G_TestEntityPosition( check )) == NULL)
			continue;

		// the entity is blocked and it is a player, then warn the player
		if( warnBlockingPlayers && check->s.eType == ET_PLAYER ) {
			if( (level.time - check->client->lastConstructibleBlockingWarnTime) >= MIN_BLOCKINGWARNING_INTERVAL ) {
				trap_SendServerCommand( check->s.number, "cp \"Warning, leave the construction area...\" 1" );
				// Gordon: store the entity num to warn the bot
				check->client->lastConstructibleBlockingWarnEnt = constructible - g_entities;
				check->client->lastConstructibleBlockingWarnTime = level.time;
			}

			// unlink our entities again
			/*trap_UnlinkEntity( constructible );

			if( constructible->track && constructible->track[0] ) {
				for( e = 0; e < constructibleEntities; e++ ) {
					check = &g_entities[constructibleList[e]];

					trap_UnlinkEntity( check );
				}
			}
			return;*/
		}

		blockingList[blockingEntities++] = entityList[e];
	}

	// undo the temporary solid for our entities
	UndoTemporarySolid( constructible );
	if( constructible->track && constructible->track[0] ) {
		for( e = 0; e < constructibleEntities; e++ ) {
			check = &g_entities[constructibleList[e]];

			//trap_UnlinkEntity( check );
			UndoTemporarySolid( check );
		}
	}

	if( handleBlockingEnts ) {
		for( e = 0; e < blockingEntities; e++ ) {
			block = &g_entities[blockingList[e]];

			if( block->client || block->s.eType == ET_CORPSE ) {
				G_Damage( block, constructible, constructor, NULL, NULL, 9999, DAMAGE_NO_PROTECTION, MOD_CRUSH_CONSTRUCTION );
			} else if( block->s.eType == ET_ITEM && block->item->giType == IT_TEAM ) {
				// see if it's a critical entity, one that we can't just simply kill (basically flags)
				Team_DroppedFlagThink( block );
			} else {
				// remove the landmine from both teamlists
				if ( block->s.eType == ET_MISSILE && block->methodOfDeath == MOD_LANDMINE ) {
					mapEntityData_t	*mEnt;

					if((mEnt = G_FindMapEntityData(&mapEntityData[0], block-g_entities)) != NULL) {
						G_FreeMapEntityData( &mapEntityData[0], mEnt );
					}

					if((mEnt = G_FindMapEntityData(&mapEntityData[1], block-g_entities)) != NULL) {
						G_FreeMapEntityData( &mapEntityData[1], mEnt );
					}
				}

				// just get rid of it
				G_TempEntity( block->s.origin, EV_ITEM_POP );
				G_FreeEntity( block );
			}
		}
	}

	if( constructibleModelindex ) {
		trap_SetBrushModel( constructible, va( "*%i", constructibleModelindex ) );
		// ...and restore
		constructible->clipmask = constructibleClipmask;
		constructible->r.contents = constructibleContents;
		if( !constructibleNonSolidBModel )
			constructible->s.eFlags &= ~EF_NONSOLID_BMODEL;
		trap_LinkEntity( constructible );
	} else {
		constructible->s.modelindex = 0;
		//constructible->clipmask = constructibleClipmask;
		//constructible->r.contents = constructibleContents;
		//if( !constructibleNonSolidBModel )
		//	constructible->s.eFlags &= ~EF_NONSOLID_BMODEL;
		trap_LinkEntity( constructible );
	}
}

#define CONSTRUCT_POSTDECAY_TIME 500

// !! NOTE !!: if the conditions here of a buildable constructible change, then BotIsConstructible() must reflect those changes

// returns qfalse when it couldn't build
/* TC2009b379..2009b3a2: store progress but compare the retained ST0 sum. */
static qboolean G_TCEAdvanceConstruction(int constructionDuration, float *constructionProgress) {
#if defined(_MSC_VER) && defined(_M_IX86)
	const float constructionScale = 0.01f;
	const float constructionAmount = 255.0f, constructionThreshold = 250.0f;
	int constructionComplete;
	__asm {
		mov edx, constructionProgress
		fild constructionDuration
		fmul constructionScale
		fdivr constructionAmount
		fadd dword ptr [edx]
		fst dword ptr [edx]
		fcomp constructionThreshold
		fnstsw ax
		test ah, 1
		setz al
		movzx eax, al
		mov constructionComplete, eax
	}
	return constructionComplete;
#else
	*constructionProgress += 255.f / (constructionDuration / (float)FRAMETIME);
	return *constructionProgress >= 250.f;
#endif
}

#if defined(_MSC_VER) && defined(_M_IX86)
/* Original __ftol200a23e0 consumes ST0 and returns the low signed dword.
 * Keep any older x87 stack entries alive (the updated marker's Z sum). */
static __declspec(naked) int G_TCEConstructionFtol(void) {
	__asm {
		sub esp, 12
		fwait
		fnstcw word ptr [esp + 10]
		fwait
		mov ax, word ptr [esp + 10]
		or ax, 0c00h
		mov word ptr [esp + 8], ax
		fldcw word ptr [esp + 8]
		fistp qword ptr [esp]
		fldcw word ptr [esp + 10]
		mov eax, dword ptr [esp]
		add esp, 12
		ret
	}
}
#endif

static int G_TCEConstructionScore(float constructionScore) {
#if defined(_MSC_VER) && defined(_M_IX86)
	int scoreInteger;
	__asm {
		fld constructionScore
		call G_TCEConstructionFtol
		mov scoreInteger, eax
	}
	return scoreInteger;
#else
	return (int)constructionScore;
#endif
}

static void G_TCEConstructionSoundCenter(const vec3_t centerMins, const vec3_t centerMaxs, vec3_t centerResult) {
#if defined(_MSC_VER) && defined(_M_IX86)
	const float centerHalf = 0.5f;
	__asm {
		mov edx, centerMins
		mov ecx, centerMaxs
		mov eax, centerResult
		fld dword ptr [ecx]
		fadd dword ptr [edx]
		fstp dword ptr [eax]
		fld dword ptr [ecx + 4]
		fadd dword ptr [edx + 4]
		fstp dword ptr [eax + 4]
		fld dword ptr [ecx + 8]
		fadd dword ptr [edx + 8]
		fld dword ptr [eax]
		fmul centerHalf
		fstp dword ptr [eax]
		fld dword ptr [eax + 4]
		fmul centerHalf
		fstp dword ptr [eax + 4]
		fmul centerHalf
		fstp dword ptr [eax + 8]
	}
#else
	VectorAdd(centerMins, centerMaxs, centerResult);
	VectorScale(centerResult, 0.5f, centerResult);
#endif
}

static void G_TCEConstructionSnap(vec3_t snapPosition) {
#if defined(_MSC_VER) && defined(_M_IX86)
	int snapInteger;
	__asm {
		mov ecx, snapPosition
		fld dword ptr [ecx]
		call G_TCEConstructionFtol
		mov snapInteger, eax
		fild snapInteger
		fstp dword ptr [ecx]
		fld dword ptr [ecx + 4]
		call G_TCEConstructionFtol
		mov snapInteger, eax
		fild snapInteger
		fstp dword ptr [ecx + 4]
		fld dword ptr [ecx + 8]
		call G_TCEConstructionFtol
		mov snapInteger, eax
		fild snapInteger
		fstp dword ptr [ecx + 8]
	}
#else
	SnapVector(snapPosition);
#endif
}

/* Updated markers retain their Z sum through the X/Y conversions. */
static void G_TCEConstructionUpdateCenter(const vec3_t markerMins, const vec3_t markerMaxs, vec3_t markerPosition) {
#if defined(_MSC_VER) && defined(_M_IX86)
	const double markerHalf = 0.5;
	int markerInteger;
	__asm {
		mov edx, markerMins
		mov ecx, markerMaxs
		mov eax, markerPosition
		fld dword ptr [ecx]
		fadd dword ptr [edx]
		fstp dword ptr [eax]
		fld dword ptr [ecx + 4]
		fadd dword ptr [edx + 4]
		fstp dword ptr [eax + 4]
		fld dword ptr [ecx + 8]
		fadd dword ptr [edx + 8]
		fst dword ptr [eax + 8]
		mov ecx, eax
		fld dword ptr [ecx]
		fmul markerHalf
		call G_TCEConstructionFtol
		mov markerInteger, eax
		fild markerInteger
		fstp dword ptr [ecx]
		fld dword ptr [ecx + 4]
		fmul markerHalf
		call G_TCEConstructionFtol
		mov markerInteger, eax
		fild markerInteger
		fstp dword ptr [ecx + 4]
		fmul markerHalf
		call G_TCEConstructionFtol
		mov markerInteger, eax
		fild markerInteger
		fstp dword ptr [ecx + 8]
	}
#else
	VectorAdd(markerMins, markerMaxs, markerPosition);
	VectorScale(markerPosition, 0.5f, markerPosition);
	SnapVector(markerPosition);
#endif
}

/* Original prefix branches select C0/C3 directly, including unordered. */
static unsigned int G_TCEConstructionCompare(float constructionLeft, float constructionRight) {
#if defined(_MSC_VER) && defined(_M_IX86)
	unsigned short constructionStatus;
	__asm {
		fld constructionLeft
		fcomp constructionRight
		fnstsw constructionStatus
	}
	return constructionStatus;
#else
	if (constructionLeft != constructionLeft || constructionRight != constructionRight) return 0x4500u;
	if (constructionLeft < constructionRight) return 0x100u;
	return constructionLeft == constructionRight ? 0x4000u : 0u;
#endif
}

static qboolean TryConstructing( gentity_t *ent ) {
	gentity_t *check;
	gentity_t *constructible = ent->client->touchingTOI->target_ent;
	int i;

	// no construction during prematch
	if( level.warmupTime )
		return( qfalse );

	// see if we are in a trigger_objective_info targetting multiple func_constructibles
	if( constructible->s.eType == ET_CONSTRUCTIBLE && ent->client->touchingTOI->chain ) {
		gentity_t *otherconstructible = NULL;

		// use the target that has the same team as the player
		if( constructible->s.teamNum != ent->client->sess.sessionTeam ) {
			constructible = ent->client->touchingTOI->chain;
		}

		otherconstructible = constructible->chain;

		// make sure the other constructible isn't built/underconstruction/something
		if( !(G_TCEConstructionCompare(otherconstructible->s.angles2[0], 0.0f) & 0x4000u) ||
			!(G_TCEConstructionCompare(otherconstructible->s.angles2[1], 0.0f) & 0x4000u) ||
			( otherconstructible->count2 && otherconstructible->grenadeFired ) ) {

			return( qfalse );
		}
	}

	// see if we are in a trigger_objective_info targetting a func_constructible
	if( constructible->s.eType == ET_CONSTRUCTIBLE &&
		constructible->s.teamNum == ent->client->sess.sessionTeam ) {

		if( !(G_TCEConstructionCompare(constructible->s.angles2[0], 250.0f) & 0x100u) )
			return( qfalse );

		if( !(G_TCEConstructionCompare(constructible->s.angles2[1], 0.0f) & 0x4000u) )
			return( qfalse );

		// Check if we can construct - updates the classWeaponTime as well
		if (!ReadyToConstruct(ent, constructible, qtrue))
			return qtrue;

		// try to start building
		if( G_TCEConstructionCompare(constructible->s.angles2[0], 0.0f) & 0x4100u ) {
			// wait a bit, this prevents network spam
			if( (int)((unsigned int)level.time - (unsigned int)constructible->lastHintCheckTime) < CONSTRUCT_POSTDECAY_TIME )
				return( qtrue );	// likely will come back soon - so override other plier bits anyway

			// Gordon: are we scripted only?
			if( !(ent->spawnflags & CONSTRUCTIBLE_AAS_SCRIPTED) ) {
				if ( !(ent->spawnflags & CONSTRUCTIBLE_NO_AAS_BLOCKING) ) {
					// RF, if we are blocking AAS areas when built, then clear AAS blocking so we can set it again after the stage has been increased
					if( constructible->spawnflags & CONSTRUCTIBLE_BLOCK_PATHS_WHEN_BUILD ) {
						G_SetAASBlockingEntity( ent, AAS_AREA_ENABLED );
					}
				}
			}

			// swap brushmodels if staged
			if( constructible->count2 ) {
				constructible->grenadeFired++;
				constructible->s.modelindex2 = constructible->conbmodels[constructible->grenadeFired-1];
				//trap_SetBrushModel( constructible, va( "*%i", constructible->conbmodels[constructible->grenadeFired-1] ) );
			}

			G_SetEntState( constructible, STATE_UNDERCONSTRUCTION );

			if( !constructible->count2 ) {
				// call script
				G_Script_ScriptEvent( constructible, "buildstart", "final" );
				constructible->s.frame = 1;
			} else {
				if( constructible->grenadeFired == constructible->count2 ) {
					G_Script_ScriptEvent( constructible, "buildstart", "final" );
					constructible->s.frame = constructible->grenadeFired;
				} else {
					switch( constructible->grenadeFired ) {
					case 1: G_Script_ScriptEvent( constructible, "buildstart", "stage1" ); constructible->s.frame = 1; break;
					case 2: G_Script_ScriptEvent( constructible, "buildstart", "stage2" ); constructible->s.frame = 2; break;
					case 3: G_Script_ScriptEvent( constructible, "buildstart", "stage3" ); constructible->s.frame = 3; break;
					}
				}
			}

			{
				vec3_t mid;
				gentity_t* te;

				G_TCEConstructionSoundCenter(constructible->parent->r.absmin, constructible->parent->r.absmax, mid);

				te = G_TempEntity( mid, EV_GENERAL_SOUND );
				te->s.eventParm = G_SoundIndex( "sound/world/build.wav" );
			}
			

			// Play sound
/*			if( constructible->parent->spawnflags & 8 ) {
				constructible->parent->s.loopSound = G_SoundIndex( va( "sound/world/build_stage%i.wav", constructible->s.frame ) );
			} else {
				constructible->s.loopSound = G_SoundIndex( va( "sound/world/build_stage%i.wav", constructible->s.frame ) );
			}*/

			if( ent->client->touchingTOI->chain && ent->client->touchingTOI->count2 ) {
				// find the constructible indicator and change team
				mapEntityData_t	*mEnt;
				mapEntityData_Team_t *teamList;
				gentity_t *indicator = &g_entities[ent->client->touchingTOI->count2];

				indicator->s.teamNum = constructible->s.teamNum;

				// update the map for the other team
				teamList = indicator->s.teamNum == TEAM_AXIS ? &mapEntityData[1] : &mapEntityData[0]; // inversed
				if((mEnt = G_FindMapEntityData( teamList, indicator-g_entities)) != NULL) {
					G_FreeMapEntityData( teamList, mEnt );
				}
			}

			if( !constructible->count2 || constructible->grenadeFired == 1 ) {
				// link in if we just started building
				G_UseEntity( constructible, ent->client->touchingTOI, ent );
			}

			// setup our think function for decaying
			constructible->think = func_constructible_underconstructionthink;
			constructible->nextthink = level.time + FRAMETIME;

			G_PrintClientSpammyCenterPrint( ent-g_entities, "Constructing..." );
		}

		// Give health until it is full, don't continue
		if ( G_TCEAdvanceConstruction(constructible->constructibleStats.duration, &constructible->s.angles2[0]) ) {
			constructible->s.angles2[0] = 0;
			HandleEntsThatBlockConstructible( ent, constructible, qtrue, qfalse );
		} else {
			constructible->lastHintCheckTime = level.time;
			HandleEntsThatBlockConstructible( ent, constructible, qfalse, qtrue );
            return( qtrue );	// properly constructed
		}

		//trap_SendServerCommand( ent-g_entities, "cp \"Job's done!\" 1");

		// eeeh no point in doing this twice
		//HandleEntsThatBlockConstructible( ent, constructible, qtrue, qfalse );
		if( constructible->count2 ) {
			// backup...
			//int constructibleModelindex = constructible->s.modelindex;
			int constructibleClipmask = constructible->clipmask;
			int constructibleContents = constructible->r.contents;
			int constructibleNonSolidBModel = (constructible->s.eFlags & EF_NONSOLID_BMODEL);

			constructible->s.modelindex2 = 0;
			trap_SetBrushModel( constructible, va( "*%i", constructible->conbmodels[constructible->grenadeFired-1] ) );

			// ...and restore
			constructible->clipmask = constructibleClipmask;
			constructible->r.contents = constructibleContents;
			if( !constructibleNonSolidBModel )
				constructible->s.eFlags &= ~EF_NONSOLID_BMODEL;

			if( constructible->grenadeFired == constructible->count2 ) {
				constructible->s.angles2[1] = 1;
			}
		} else {
			// backup...
			//int constructibleModelindex = constructible->s.modelindex;
			int constructibleClipmask = constructible->clipmask;
			int constructibleContents = constructible->r.contents;
			int constructibleNonSolidBModel = (constructible->s.eFlags & EF_NONSOLID_BMODEL);

			constructible->s.modelindex2 = 0;
			trap_SetBrushModel( constructible, constructible->model );

			// ...and restore
			constructible->clipmask = constructibleClipmask;
			constructible->r.contents = constructibleContents;
			if( !constructibleNonSolidBModel )
				constructible->s.eFlags &= ~EF_NONSOLID_BMODEL;

			constructible->s.angles2[1] = 1;
		}

		// Gordon: removing messages for now
/*		if( ent->client->touchingTOI->spawnflags & 4 ) { // MESSAGE_OVERRIDE
			gentity_t* pm = G_PopupMessage( PM_CONSTRUCTION );
			pm->s.effect3Time = G_StringIndex( ent->client->touchingTOI->spawnitem );
			pm->s.effect2Time = TEAM_AXIS;
			pm->s.density = -1; // -1 = built (custom msg)
		} else {
			gentity_t* pm = G_PopupMessage( PM_CONSTRUCTION );
			pm->s.density = ent->client->sess.sessionTeam;
			pm->s.effect2Time = 0; // 0 = built
			pm->s.effect3Time = ent->client->touchingTOI->s.teamNum;
		}*/

		AddScore( ent, G_TCEConstructionScore(constructible->accuracy) ); // give drop score to guy who built it

		G_AddSkillPoints( ent, SK_EXPLOSIVES_AND_CONSTRUCTION, constructible->constructibleStats.constructxpbonus );
		G_DebugAddSkillPoints( ent, SK_EXPLOSIVES_AND_CONSTRUCTION, constructible->constructibleStats.constructxpbonus, "finishing a construction" );

		// unlink the objective info to get rid of the indicator for now
		// Arnout: don't unlink, we still want the location popup. Instead, constructible_indicator_think got changed to free
		// the indicator when the constructible is constructed
//			if( constructible->parent )
//				trap_UnlinkEntity( constructible->parent );

		G_SetEntState( constructible, STATE_DEFAULT );

		// make destructable
		if( !(constructible->spawnflags & 2) ) {
			constructible->takedamage = qtrue;
			constructible->health = constructible->sound1to2;
		}

		// Stop thinking
		constructible->think = NULL;
		constructible->nextthink = 0;

		if( !constructible->count2 ) {
			// call script
			G_Script_ScriptEvent( constructible, "built", "final" );
		} else {
			if( constructible->grenadeFired == constructible->count2 ) {
				G_Script_ScriptEvent( constructible, "built", "final" );
			} else {
				switch( constructible->grenadeFired ) {
				case 1: G_Script_ScriptEvent( constructible, "built", "stage1" ); break;
				case 2: G_Script_ScriptEvent( constructible, "built", "stage2" ); break;
				case 3: G_Script_ScriptEvent( constructible, "built", "stage3" ); break;
				}
			}
		}

		// Stop sound
		if( constructible->parent->spawnflags & 8 ) {
			constructible->parent->s.loopSound = 0;
		} else {
			constructible->s.loopSound = 0;
		}

		//ent->client->ps.classWeaponTime = level.time; // Out of "ammo"

		// if not invulnerable and dynamite-able, create a 'destructable' marker for the other team
		if( !(constructible->spawnflags & CONSTRUCTIBLE_INVULNERABLE) && (constructible->constructibleStats.weaponclass >= 1) ) {
			if( !constructible->count2 || constructible->grenadeFired == 1 ) {
				gentity_t* tent = NULL;
				gentity_t *e;
				e = G_Spawn();

				e->r.svFlags = SVF_BROADCAST;
				e->classname = "explosive_indicator";
				e->s.pos.trType = TR_STATIONARY;
				e->s.eType = ET_EXPLOSIVE_INDICATOR;

				while((tent = G_Find (tent, FOFS(target), constructible->targetname)) != NULL) {
					if(tent->s.eType == ET_OID_TRIGGER) {
						if(tent->spawnflags & 8) {
							e->s.eType = ET_TANK_INDICATOR;
						}
					}
				}

				// Find the trigger_objective_info that targets us (if not set before)
				{
					gentity_t* tent = NULL;
					while((tent = G_Find (tent, FOFS(target), constructible->targetname)) != NULL) {
						if((tent->s.eType == ET_OID_TRIGGER)) {
							e->parent = tent;
						}
					}
				}

				if ( constructible->spawnflags & AXIS_CONSTRUCTIBLE )
					e->s.teamNum = TEAM_AXIS;
				else if ( constructible->spawnflags & ALLIED_CONSTRUCTIBLE )
					e->s.teamNum = TEAM_ALLIES;

				e->s.modelindex2 = ent->client->touchingTOI->s.teamNum;
				e->r.ownerNum = constructible->s.number;
				e->think = explosive_indicator_think;
				e->nextthink = level.time + FRAMETIME;

				e->s.effect1Time = constructible->constructibleStats.weaponclass;

				if(constructible->parent->tagParent) {
					e->tagParent = constructible->parent->tagParent;
					Q_strncpyz( e->tagName, constructible->parent->tagName, MAX_QPATH );
				} else {
					VectorCopy( constructible->r.absmin, e->s.pos.trBase );
					VectorAdd( constructible->r.absmax, e->s.pos.trBase, e->s.pos.trBase );
					VectorScale( e->s.pos.trBase, 0.5, e->s.pos.trBase );
				}

				G_TCEConstructionSnap( e->s.pos.trBase );

				trap_LinkEntity( e );
			} else {
				// find our marker and update it's coordinates
				for( i = 0, check = g_entities; i < level.num_entities; i++, check++) {
					if( check->s.eType != ET_EXPLOSIVE_INDICATOR && check->s.eType != ET_TANK_INDICATOR && check->s.eType != ET_TANK_INDICATOR_DEAD )
						continue;

					if( check->r.ownerNum == constructible->s.number ) {
						// found it!
						if(constructible->parent->tagParent) {
							check->tagParent = constructible->parent->tagParent;
							Q_strncpyz( check->tagName, constructible->parent->tagName, MAX_QPATH );
						} else {
							G_TCEConstructionUpdateCenter(constructible->r.absmin, constructible->r.absmax, check->s.pos.trBase);
						}

						trap_LinkEntity( check );
						break;
					}
				}
			}
		}

		// Gordon: are we scripted only?
		if( !(ent->spawnflags & CONSTRUCTIBLE_AAS_SCRIPTED) ) {
			if ( !(ent->spawnflags & CONSTRUCTIBLE_NO_AAS_BLOCKING) ) {
				// RF, a stage has been completed, either enable or disable AAS areas appropriately
				if( !(constructible->spawnflags & CONSTRUCTIBLE_BLOCK_PATHS_WHEN_BUILD) ) {
					// builing creates AAS paths
					// Gordon: HACK from ryan
	//				if( !constructible->count2 || ( constructible->grenadeFired == constructible->count2 ) )
					{
						// completely built, enable paths
						G_SetAASBlockingEntity( constructible, AAS_AREA_ENABLED );
					}
				} else {
					// builing blocks AAS paths
					G_SetAASBlockingEntity( constructible, AAS_AREA_DISABLED );
				}
			}
		}

		return( qtrue );	// building
	}

	return( qfalse );
}

void AutoBuildConstruction( gentity_t* constructible ) {
	int i;
	gentity_t* check;

	HandleEntsThatBlockConstructible( NULL, constructible, qtrue, qfalse );
	if( constructible->count2 ) {
		// backup...
		//int constructibleModelindex = constructible->s.modelindex;
		int constructibleClipmask = constructible->clipmask;
		int constructibleContents = constructible->r.contents;
		int constructibleNonSolidBModel = (constructible->s.eFlags & EF_NONSOLID_BMODEL);

		constructible->s.modelindex2 = 0;
		trap_SetBrushModel( constructible, va( "*%i", constructible->conbmodels[constructible->grenadeFired-1] ) );

		// ...and restore
		constructible->clipmask = constructibleClipmask;
		constructible->r.contents = constructibleContents;
		if( !constructibleNonSolidBModel )
			constructible->s.eFlags &= ~EF_NONSOLID_BMODEL;

		if( constructible->grenadeFired == constructible->count2 ) {
			constructible->s.angles2[1] = 1;
		}
	} else {
		// backup...
		//int constructibleModelindex = constructible->s.modelindex;
		int constructibleClipmask = constructible->clipmask;
		int constructibleContents = constructible->r.contents;
		int constructibleNonSolidBModel = (constructible->s.eFlags & EF_NONSOLID_BMODEL);

		constructible->s.modelindex2 = 0;
		trap_SetBrushModel( constructible, constructible->model );

		// ...and restore
		constructible->clipmask = constructibleClipmask;
		constructible->r.contents = constructibleContents;
		if( !constructibleNonSolidBModel )
			constructible->s.eFlags &= ~EF_NONSOLID_BMODEL;

		constructible->s.angles2[1] = 1;
	}

	// unlink the objective info to get rid of the indicator for now
	// Arnout: don't unlink, we still want the location popup. Instead, constructible_indicator_think got changed to free
	// the indicator when the constructible is constructed
//			if( constructible->parent )
//				trap_UnlinkEntity( constructible->parent );

	G_SetEntState( constructible, STATE_DEFAULT );

	// make destructable
	if( !(constructible->spawnflags & CONSTRUCTIBLE_INVULNERABLE) ) {
		constructible->takedamage = qtrue;
		constructible->health = constructible->constructibleStats.health;
	}

	// Stop thinking
	constructible->think = NULL;
	constructible->nextthink = 0;

	if( !constructible->count2 ) {
		// call script
		G_Script_ScriptEvent( constructible, "built", "final" );
	} else {
		if( constructible->grenadeFired == constructible->count2 ) {
			G_Script_ScriptEvent( constructible, "built", "final" );
		} else {
			switch( constructible->grenadeFired ) {
			case 1: G_Script_ScriptEvent( constructible, "built", "stage1" ); break;
			case 2: G_Script_ScriptEvent( constructible, "built", "stage2" ); break;
			case 3: G_Script_ScriptEvent( constructible, "built", "stage3" ); break;
			}
		}
	}

	// Stop sound
	if( constructible->parent->spawnflags & 8 ) {
		constructible->parent->s.loopSound = 0;
	} else {
		constructible->s.loopSound = 0;
	}

	//ent->client->ps.classWeaponTime = level.time; // Out of "ammo"

	// if not invulnerable and dynamite-able, create a 'destructable' marker for the other team
	if( !(constructible->spawnflags & CONSTRUCTIBLE_INVULNERABLE) && (constructible->constructibleStats.weaponclass >= 1) ) {
		if( !constructible->count2 || constructible->grenadeFired == 1 ) {
			gentity_t* tent = NULL;
			gentity_t *e;
			e = G_Spawn();

			e->r.svFlags = SVF_BROADCAST;
			e->classname = "explosive_indicator";
			e->s.pos.trType = TR_STATIONARY;
			e->s.eType = ET_EXPLOSIVE_INDICATOR;

			while((tent = G_Find(tent, FOFS(target), constructible->targetname)) != NULL) {
				if((tent->s.eType == ET_OID_TRIGGER)) {
					if(tent->spawnflags & 8) {
						e->s.eType = ET_TANK_INDICATOR;
					}
				}
			}

			// Find the trigger_objective_info that targets us (if not set before)
			{
				gentity_t* tent = NULL;
				while((tent = G_Find (tent, FOFS(target), constructible->targetname)) != NULL) {
					if((tent->s.eType == ET_OID_TRIGGER)) {
						e->parent = tent;
					}
				}
			}

			if ( constructible->spawnflags & AXIS_CONSTRUCTIBLE )
				e->s.teamNum = TEAM_AXIS;
			else if ( constructible->spawnflags & ALLIED_CONSTRUCTIBLE )
				e->s.teamNum = TEAM_ALLIES;

			e->s.modelindex2 = constructible->parent->s.teamNum == TEAM_AXIS ? TEAM_ALLIES : TEAM_AXIS;
			e->r.ownerNum = constructible->s.number;
			e->think = explosive_indicator_think;
			e->nextthink = level.time + FRAMETIME;

			e->s.effect1Time = constructible->constructibleStats.weaponclass;

			if(constructible->parent->tagParent) {
				e->tagParent = constructible->parent->tagParent;
				Q_strncpyz( e->tagName, constructible->parent->tagName, MAX_QPATH );
			} else {
				VectorCopy( constructible->r.absmin, e->s.pos.trBase );
				VectorAdd( constructible->r.absmax, e->s.pos.trBase, e->s.pos.trBase );
				VectorScale( e->s.pos.trBase, 0.5, e->s.pos.trBase );
			}

			SnapVector( e->s.pos.trBase );

			trap_LinkEntity( e );
		} else {
			// find our marker and update it's coordinates
			for( i = 0, check = g_entities; i < level.num_entities; i++, check++) {
				if( check->s.eType != ET_EXPLOSIVE_INDICATOR && check->s.eType != ET_TANK_INDICATOR && check->s.eType != ET_TANK_INDICATOR_DEAD )
					continue;

				if( check->r.ownerNum == constructible->s.number ) {
					// found it!
					if(constructible->parent->tagParent) {
						check->tagParent = constructible->parent->tagParent;
						Q_strncpyz( check->tagName, constructible->parent->tagName, MAX_QPATH );
					} else {
						VectorCopy( constructible->r.absmin, check->s.pos.trBase );
						VectorAdd( constructible->r.absmax, check->s.pos.trBase, check->s.pos.trBase );
						VectorScale( check->s.pos.trBase, 0.5, check->s.pos.trBase );

						SnapVector( check->s.pos.trBase );
					}

					trap_LinkEntity( check );
					break;
				}
			}
		}
	}

	// Gordon: are we scripted only?
	if( !(constructible->spawnflags & CONSTRUCTIBLE_AAS_SCRIPTED) ) {
		if ( !(constructible->spawnflags & CONSTRUCTIBLE_NO_AAS_BLOCKING) ) {
			// RF, a stage has been completed, either enable or disable AAS areas appropriately
			if( !(constructible->spawnflags & CONSTRUCTIBLE_BLOCK_PATHS_WHEN_BUILD) ) {
				// builing creates AAS paths
				if( !constructible->count2 || ( constructible->grenadeFired == constructible->count2 ) ) {
					// completely built, enable paths
					G_SetAASBlockingEntity( constructible, AAS_AREA_ENABLED );
				}
			} else {
				// builing blocks AAS paths
				G_SetAASBlockingEntity( constructible, AAS_AREA_DISABLED );
			}
		}
	}
}

qboolean G_LandmineTriggered( gentity_t* ent ) {
	switch( ent->s.teamNum ) {
		case TEAM_AXIS + 8:
		case TEAM_ALLIES + 8:
			return qtrue;
	}

	return qfalse;
}

qboolean G_LandmineArmed( gentity_t* ent ) {
	switch( ent->s.teamNum ) {
		case TEAM_AXIS:
		case TEAM_ALLIES:
			return qtrue;
	}
	return qfalse;
}

qboolean G_LandmineUnarmed( gentity_t* ent ) {
	return (!G_LandmineArmed( ent ) && !G_LandmineTriggered( ent ));
}

team_t G_LandmineTeam( gentity_t* ent ) {
	return (ent->s.teamNum % 4);
}

qboolean G_LandmineSpotted( gentity_t* ent ) {
	return ent->s.modelindex2 ? qtrue : qfalse;
}

void trap_EngineerTrace( trace_t *results, const vec3_t start, const vec3_t mins, const vec3_t maxs, const vec3_t end, int passEntityNum, int contentmask ) {
	G_TempTraceIgnorePlayersAndBodies();
	trap_Trace( results, start, mins, maxs, end, passEntityNum, contentmask );
	G_ResetTempTraceIgnoreEnts();
}

// DHM - Nerve
/* TC 20099d80 / Linux0010b338: the engineer completes a locked defuse
 * immediately; the client-event producer owns the defuse progress timer. */
void Weapon_Engineer( gentity_t *ent ) {
	trace_t tr;
	gentity_t *traceEnt, *hit, *pm;
	vec3_t end, origin, mins, maxs, org;
	int touch[MAX_GENTITIES], radial[MAX_GENTITIES];
	int i, num, count, points;
	qboolean friendlyObj, enemyObj;
	mapEntityData_t *mEnt;

	if (ent->client->ps.persistant[PERS_HWEAPON_USE]) return;
	if (ent->client->touchingTOI && TryConstructing(ent)) return;
	AngleVectors(ent->client->ps.viewangles, forward, right, up);
	VectorCopy(ent->client->ps.origin, muzzleTrace);
	muzzleTrace[2] += ent->client->ps.viewheight;
	VectorMA(muzzleTrace, 64, forward, end);
	if (!ent->client->tceDefuseActive) {
		trap_EngineerTrace(&tr, muzzleTrace, NULL, NULL, end, ent->s.number, MASK_SHOT);
		if ((tr.surfaceFlags & SURF_NOIMPACT) || tr.fraction == 1.0f ||
			tr.entityNum == ENTITYNUM_NONE || tr.entityNum == ENTITYNUM_WORLD) return;
		traceEnt = &g_entities[tr.entityNum];
	} else {
		traceEnt = &g_entities[ent->client->tceDefuseEntity];
	}

	if (traceEnt->methodOfDeath == MOD_LANDMINE) {
		if (G_CountTeamLandmines(ent->client->sess.sessionTeam) >= MAX_TEAM_LANDMINES &&
			G_LandmineTeam(traceEnt) == ent->client->sess.sessionTeam) {
			if (G_LandmineUnarmed(traceEnt)) {
				trap_SendServerCommand(ent-g_entities, "cp \"Your team has too many landmines placed...\" 1");
				G_FreeEntity(traceEnt);
				Add_Ammo(ent, WP_LANDMINE, 1, qfalse);
				ent->client->ps.classWeaponTime -=
					(ent->client->sess.skill[SK_EXPLOSIVES_AND_CONSTRUCTION] >= 3 ? .33f : .5f) *
					level.engineerChargeTime[ent->client->sess.sessionTeam-1];
				ent->client->sess.aWeaponStats[WS_LANDMINE].atts--;
				return;
			}
		} else if (G_LandmineUnarmed(traceEnt)) {
			if (G_LandmineTeam(traceEnt) != ent->client->sess.sessionTeam) return;
			G_PrintClientSpammyCenterPrint(ent-g_entities, "Arming landmine...");
			traceEnt->health += ent->client->sess.skill[SK_EXPLOSIVES_AND_CONSTRUCTION] >= 2 ? 24 : 12;
			if (traceEnt->health < 250) return;
			trap_SendServerCommand(ent-g_entities, "cp \"Landmine armed...\" 1");
			traceEnt->r.contents = 0;
			trap_LinkEntity(traceEnt);
			traceEnt->timestamp = level.time + 1000;
			traceEnt->health = 0;
			traceEnt->s.teamNum = ent->client->sess.sessionTeam;
			traceEnt->s.modelindex2 = 0;
			traceEnt->nextthink = level.time + 2000;
			traceEnt->think = G_LandminePrime;
			return;
		}
		if (traceEnt->timestamp > level.time || traceEnt->health >= 250) return;
		traceEnt->health += ent->client->sess.skill[SK_EXPLOSIVES_AND_CONSTRUCTION] >= 2 ? 6 : 3;
		G_PrintClientSpammyCenterPrint(ent-g_entities, "Defusing landmine");
		if (traceEnt->health < 250) return;
		trap_SendServerCommand(ent-g_entities, "cp \"Landmine defused...\" 1");
		Add_Ammo(ent, WP_LANDMINE, 1, qfalse);
		if (G_LandmineTeam(traceEnt) != ent->client->sess.sessionTeam) {
			G_AddSkillPoints(ent, SK_EXPLOSIVES_AND_CONSTRUCTION, 4.f);
			G_DebugAddSkillPoints(ent, SK_EXPLOSIVES_AND_CONSTRUCTION, 4.f, "defusing an enemy landmine");
		}
		if ((mEnt = G_FindMapEntityData(&mapEntityData[0], traceEnt-g_entities)) != NULL)
			G_FreeMapEntityData(&mapEntityData[0], mEnt);
		if ((mEnt = G_FindMapEntityData(&mapEntityData[1], traceEnt-g_entities)) != NULL)
			G_FreeMapEntityData(&mapEntityData[1], mEnt);
		G_FreeEntity(traceEnt);
		return;
	}
	if (traceEnt->methodOfDeath == MOD_SATCHEL) {
		if (traceEnt->health >= 250) return;
		traceEnt->health += 3;
		G_PrintClientSpammyCenterPrint(ent-g_entities, "Disarming satchel charge...");
		if (traceEnt->health < 250) return;
		traceEnt->health = 255;
		traceEnt->think = G_FreeEntity;
		traceEnt->nextthink = level.time + FRAMETIME;
		G_PrintClientSpammyCenterPrint(ent-g_entities, "Satchel charge disarmed...");
		G_AddSkillPoints(ent, SK_EXPLOSIVES_AND_CONSTRUCTION, 6.f);
		G_DebugAddSkillPoints(ent, SK_EXPLOSIVES_AND_CONSTRUCTION, 6.f, "disarming satchel charge");
		return;
	}
	if (traceEnt->methodOfDeath != MOD_DYNAMITE) return;

	if (traceEnt->s.teamNum >= 4) {
		if (traceEnt->s.teamNum - 4 != ent->client->sess.sessionTeam) return;
		G_PrintClientSpammyCenterPrint(ent-g_entities, "Arming dynamite...");
		traceEnt->health += ent->client->sess.skill[SK_EXPLOSIVES_AND_CONSTRUCTION] >= 2 ? 14 : 7;
		friendlyObj = enemyObj = qfalse;
		VectorCopy(traceEnt->r.currentOrigin, org);
		org[2] += 4;
		G_TempTraceIgnorePlayersAndBodies();
		count = EntsThatRadiusCanDamage(org, traceEnt->splashRadius, radial);
		G_ResetTempTraceIgnoreEnts();
		for (i = 0; i < count; i++) {
			hit = &g_entities[radial[i]];
			if (hit->s.eType == ET_CONSTRUCTIBLE && !(hit->spawnflags & CONSTRUCTIBLE_INVULNERABLE) &&
				(!hit->parent || !(hit->parent->spawnflags & 8)) && G_ConstructionIsPartlyBuilt(hit) &&
				hit->s.teamNum == traceEnt->s.teamNum - 4) friendlyObj = qtrue;
		}
		VectorCopy(traceEnt->r.currentOrigin, origin);
		SnapVector(origin);
		VectorAdd(origin, traceEnt->r.mins, mins);
		VectorAdd(origin, traceEnt->r.maxs, maxs);
		num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);
		for (i = 0; i < num; i++) {
			hit = &g_entities[touch[i]];
			if (!(hit->r.contents & CONTENTS_TRIGGER) || hit->s.eType != ET_OID_TRIGGER ||
				!(hit->spawnflags & (AXIS_OBJECTIVE|ALLIED_OBJECTIVE)) ||
				(hit->target_ent && Q_stricmp(hit->target_ent->classname, "func_explosive"))) continue;
			if (((hit->spawnflags & AXIS_OBJECTIVE) && ent->client->sess.sessionTeam == TEAM_AXIS) ||
				((hit->spawnflags & ALLIED_OBJECTIVE) && ent->client->sess.sessionTeam == TEAM_ALLIES)) friendlyObj = qtrue;
			if (((hit->spawnflags & AXIS_OBJECTIVE) && ent->client->sess.sessionTeam == TEAM_ALLIES) ||
				((hit->spawnflags & ALLIED_OBJECTIVE) && ent->client->sess.sessionTeam == TEAM_AXIS)) enemyObj = qtrue;
		}
		if (friendlyObj && !enemyObj) {
			G_FreeEntity(traceEnt);
			trap_SendServerCommand(ent-g_entities, "cp \"You cannot arm dynamite near a friendly objective!\" 1");
			return;
		}
		if (traceEnt->health < 250) return;
		traceEnt->health = 5;
		traceEnt->timestamp = level.time + 1000;
		traceEnt->s.teamNum = ent->client->sess.sessionTeam;
		traceEnt->s.effect1Time = level.time;
		traceEnt->nextthink = level.time + 30000;
		traceEnt->think = G_ExplodeMissile;
		VectorCopy(traceEnt->r.currentOrigin, origin);
		SnapVector(origin);
		VectorAdd(origin, traceEnt->r.mins, mins);
		VectorAdd(origin, traceEnt->r.maxs, maxs);
		num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);
		for (i = 0; i < num; i++) {
			hit = &g_entities[touch[i]];
			if (!(hit->r.contents & CONTENTS_TRIGGER) || hit->s.eType != ET_OID_TRIGGER ||
				!(hit->spawnflags & (AXIS_OBJECTIVE|ALLIED_OBJECTIVE)) ||
				(hit->target_ent && Q_stricmp(hit->target_ent->classname, "func_explosive"))) continue;
			if (hit->spawnflags & AXIS_OBJECTIVE) {
				if (ent->client->sess.sessionTeam == TEAM_ALLIES) traceEnt->accuracy = hit->accuracy;
			} else if ((hit->spawnflags & ALLIED_OBJECTIVE) && ent->client->sess.sessionTeam == TEAM_AXIS) {
				traceEnt->accuracy = hit->accuracy;
			}
			if (((hit->spawnflags & AXIS_OBJECTIVE) && ent->client->sess.sessionTeam == TEAM_ALLIES) ||
				((hit->spawnflags & ALLIED_OBJECTIVE) && ent->client->sess.sessionTeam == TEAM_AXIS)) {
				pm = G_PopupMessage(PM_DYNAMITE);
				pm->s.effect2Time = 0;
				pm->s.effect3Time = hit->s.teamNum;
				pm->s.teamNum = ent->client->sess.sessionTeam;
				G_Script_ScriptEvent(hit, "dynamited", "");
				AddScore(traceEnt->parent, 10);
				if (traceEnt->parent && traceEnt->parent->client)
					G_LogPrintf("Dynamite_Plant: %d\n", traceEnt->parent-g_entities);
				traceEnt->parent = ent;
				return;
			}
			/* Original stops examining triggers after the first qualifying one,
			 * but still performs the constructible pass when it is friendly. */
			break;
		}
		VectorCopy(traceEnt->r.currentOrigin, org);
		org[2] += 4;
		G_TempTraceIgnorePlayersAndBodies();
		count = EntsThatRadiusCanDamage(org, traceEnt->splashRadius, radial);
		G_ResetTempTraceIgnoreEnts();
		for (i = 0; i < count; i++) {
			hit = &g_entities[radial[i]];
			if (hit->s.eType != ET_CONSTRUCTIBLE || (hit->spawnflags & CONSTRUCTIBLE_INVULNERABLE) ||
				!G_ConstructionIsPartlyBuilt(hit)) continue;
			if (hit->s.teamNum == traceEnt->s.teamNum) {
				G_FreeEntity(traceEnt);
				trap_SendServerCommand(ent-g_entities, "cp \"You cannot arm dynamite near a friendly construction!\" 1");
				return;
			}
			if (hit->constructibleStats.weaponclass < 1) continue;
			if (!hit->parent) return;
			pm = G_PopupMessage(PM_DYNAMITE);
			pm->s.effect2Time = 0;
			pm->s.effect3Time = hit->parent->s.teamNum;
			pm->s.teamNum = ent->client->sess.sessionTeam;
			G_Script_ScriptEvent(hit, "dynamited", "");
			if (hit->s.teamNum && hit->s.teamNum == ent->client->sess.sessionTeam) {
				AddScore(traceEnt->parent, 10);
				if (traceEnt->parent && traceEnt->parent->client)
					G_LogPrintf("Dynamite_Plant: %d\n", traceEnt->parent-g_entities);
				traceEnt->parent = ent;
			}
			return;
		}
		trap_SendServerCommand(ent-g_entities, "cp \"Dynamite is now armed with a 30 second timer!\" 1");
		return;
	}

	if (traceEnt->timestamp > level.time || traceEnt->health >= 248) return;
	traceEnt->health = 255;
	traceEnt->think = G_FreeEntity;
	traceEnt->nextthink = level.time + FRAMETIME;
	VectorCopy(traceEnt->r.currentOrigin, origin);
	SnapVector(origin);
	VectorAdd(origin, traceEnt->r.mins, mins);
	VectorAdd(origin, traceEnt->r.maxs, maxs);
	num = trap_EntitiesInBox(mins, maxs, touch, MAX_GENTITIES);
	for (i = 0; i < num; i++) {
		hit = &g_entities[touch[i]];
		if (!(hit->r.contents & CONTENTS_TRIGGER) || hit->s.eType != ET_OID_TRIGGER ||
			!(hit->spawnflags & (AXIS_OBJECTIVE|ALLIED_OBJECTIVE)) ||
			!hit->target_ent || hit->target_ent->s.eType != ET_EXPLOSIVE) continue;
		if ((ent->client->sess.sessionTeam == TEAM_AXIS && (hit->spawnflags & AXIS_OBJECTIVE)) ||
			(ent->client->sess.sessionTeam != TEAM_AXIS && (hit->spawnflags & ALLIED_OBJECTIVE))) {
			points = hit->target_ent->tceObjectiveScore ? hit->target_ent->tceObjectiveScore : 10;
			if (g_gametype.integer == 5) AddKillScore(ent, points);
			else AddScore(ent, points);
			G_AddSkillPoints(ent, SK_EXPLOSIVES_AND_CONSTRUCTION, 6.f);
			G_DebugAddSkillPoints(ent, SK_EXPLOSIVES_AND_CONSTRUCTION, 6.f, "defusing enemy dynamite");
		}
		level.tceExitRulesNotBefore = level.time + 1000;
		if (hit->target_ent) G_Script_ScriptEvent(hit->target_ent, "defused", "");
		pm = G_PopupMessage(PM_DYNAMITE);
		pm->s.effect2Time = 1;
		pm->s.effect3Time = hit->s.teamNum;
		pm->s.teamNum = ent->client->sess.sessionTeam;
		return;
	}
	VectorCopy(traceEnt->r.currentOrigin, org);
	org[2] += 4;
	count = EntsThatRadiusCanDamage(org, traceEnt->splashRadius, radial);
	for (i = 0; i < count; i++) {
		hit = &g_entities[radial[i]];
		if (hit->s.eType != ET_CONSTRUCTIBLE || (hit->spawnflags & CONSTRUCTIBLE_INVULNERABLE) ||
			hit->constructibleStats.weaponclass < 1) continue;
		if (hit->s.teamNum == (ent->client->sess.sessionTeam == TEAM_AXIS ? TEAM_AXIS : TEAM_ALLIES)) {
			AddScore(ent, 10);
			if (ent && ent->client) G_LogPrintf("Dynamite_Diffuse: %d\n", ent-g_entities);
			G_AddSkillPoints(ent, SK_EXPLOSIVES_AND_CONSTRUCTION, 6.f);
			G_DebugAddSkillPoints(ent, SK_EXPLOSIVES_AND_CONSTRUCTION, 6.f, "defusing enemy dynamite");
		}
		G_Script_ScriptEvent(hit, "defused", "");
		pm = G_PopupMessage(PM_DYNAMITE);
		pm->s.effect2Time = 1;
		pm->s.effect3Time = hit->parent->s.teamNum;
		pm->s.teamNum = ent->client->sess.sessionTeam;
		return;
	}
}


// JPW NERVE -- launch airstrike as line of bombs mostly-perpendicular to line of grenade travel
// (close air support should *always* drop parallel to friendly lines, tho accidents do happen)
extern void G_ExplodeMissile( gentity_t *ent );

void G_AirStrikeExplode( gentity_t *self ) {

	self->r.svFlags &= ~SVF_NOCLIENT;
	self->r.svFlags |= SVF_BROADCAST;

	self->think = G_ExplodeMissile;
	self->nextthink = level.time + 50;
}

qboolean G_AvailableAirstrikes( gentity_t* ent ) {
	if( ent->client->sess.sessionTeam == TEAM_AXIS ) {
		if( level.axisBombCounter >= 60 * 1000 ) {
			return qfalse;
		}
	} else {
		if( level.alliedBombCounter >= 60 * 1000 ) {
			return qfalse;
		}
	}

	return qtrue;
}

void G_AddAirstrikeToCounters( gentity_t* ent ) {
	int product, max;
	double capacity;

	/* TC2009b990 folds the percentage into one binary32 .01 constant,
	 * then passes the retained product to double ceil. The uncapped arm
	 * evaluates the team count again, as in the original min expansion. */
	product = g_heavyWeaponRestriction.integer * G_TeamCount(ent, -1);
	capacity = 2.0 * ceil((double)product * (double)0.01f);
	if (capacity > 6.0) {
		max = 6;
	} else {
		product = g_heavyWeaponRestriction.integer * G_TeamCount(ent, -1);
		max = (int)(2.0 * ceil((double)product * (double)0.01f));
	}

	if (ent->client->sess.sessionTeam == TEAM_AXIS) {
		level.axisBombCounter = (int)(60000.0 / (double)max + level.axisBombCounter);
	} else {
		level.alliedBombCounter = (int)(60000.0 / (double)max + level.alliedBombCounter);
	}
}

#define NUMBOMBS 10
#define BOMBSPREAD 150
extern void G_SayTo( gentity_t *ent, gentity_t *other, int mode, int color, const char *name, const char *message, qboolean localize );

void weapon_checkAirStrikeThink1( gentity_t *ent ) {
	if( !weapon_checkAirStrike( ent ) ) {
		ent->think = G_ExplodeMissile;
		ent->nextthink = level.time + 1000;
		return;
	}

	ent->think = weapon_callAirStrike;
	ent->nextthink = level.time + 1500;
}

void weapon_checkAirStrikeThink2( gentity_t *ent ) {
	if( !weapon_checkAirStrike( ent ) ) {
		ent->think = G_ExplodeMissile;
		ent->nextthink = level.time + 1000;
		return;
	}

	ent->think = weapon_callSecondPlane;
	ent->nextthink = level.time + 500;
}

void weapon_callSecondPlane( gentity_t *ent ) {
	gentity_t* te;
	
	te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_SOUND );
	te->s.eventParm = G_SoundIndex( "sound/weapons/airstrike/airstrike_plane.wav" );
	te->r.svFlags |= SVF_BROADCAST;

	ent->nextthink = level.time + 1000;
	ent->think = weapon_callAirStrike;
}

qboolean weapon_checkAirStrike( gentity_t *ent ) {
	if( ent->s.teamNum == TEAM_AXIS ) {
		level.numActiveAirstrikes[0]++;
	} else {
		level.numActiveAirstrikes[1]++;
	}

	// cancel the airstrike if FF off and player joined spec
	// FIXME: this is a stupid workaround. Just store the parent team in the enitity itself and use that - no need to look up the parent
	if (!g_friendlyFire.integer && ent->parent->client && ent->parent->client->sess.sessionTeam == TEAM_SPECTATOR)
	{
		ent->splashDamage = 0;	// no damage
		ent->think = G_ExplodeMissile;
		/* TC2009bbbc..2009bbd6 retains RNG arithmetic until integer conversion. */
		ent->nextthink = (int)((((double)(rand() & 0x7fff) *
			(double)0.000030518509447574615f - 0.5) * 2.0) * 50.0 + level.time);
		
		ent->active = qfalse;
		if( ent->s.teamNum == TEAM_AXIS ) {
			level.numActiveAirstrikes[0]--;
		} else {
			level.numActiveAirstrikes[1]--; 
		}
		return qfalse; // do nothing, don't hurt anyone 
	}

	if( ent->s.teamNum == TEAM_AXIS ) {
		if( level.numActiveAirstrikes[0] > 6 || !G_AvailableAirstrikes( ent->parent ) ) {
			G_SayTo( ent->parent, ent->parent, 2, COLOR_YELLOW, "HQ: ", "All available planes are already en-route.", qtrue );

			G_GlobalClientEvent( EV_AIRSTRIKEMESSAGE, 0, ent->parent-g_entities );

/*			te = G_TempEntity( ent->parent->s.pos.trBase, EV_GLOBAL_CLIENT_SOUND );
			te->s.eventParm = G_SoundIndex( "axis_hq_airstrike_denied" );
			te->s.teamNum = ent->parent->s.clientNum;*/

//			te->s.effect1Time = 1;	// don't buffer

			ent->active = qfalse;
			if( ent->s.teamNum == TEAM_AXIS ) {
				level.numActiveAirstrikes[0]--;
			} else {
				level.numActiveAirstrikes[1]--;
			}
			return qfalse;
		}
	} else {
		if( level.numActiveAirstrikes[1] > 6 || !G_AvailableAirstrikes( ent->parent ) ) {
			G_SayTo( ent->parent, ent->parent, 2, COLOR_YELLOW, "HQ: ", "All available planes are already en-route.", qtrue );

			G_GlobalClientEvent( EV_AIRSTRIKEMESSAGE, 0, ent->parent-g_entities );

/*			te = G_TempEntity( ent->parent->s.pos.trBase, EV_GLOBAL_CLIENT_SOUND );
			te->s.eventParm = G_SoundIndex( "allies_hq_airstrike_denied" );
			te->s.teamNum = ent->parent->s.clientNum;*/

//			te->s.effect1Time = 1;	// don't buffer

			ent->active = qfalse;
			if( ent->s.teamNum == TEAM_AXIS ) {
				level.numActiveAirstrikes[0]--;
			} else {
				level.numActiveAirstrikes[1]--;
			}
			return qfalse;
		}
	}

	return qtrue;
}

void G_RailTrail( vec_t* start, vec_t* end );


void weapon_callAirStrike( gentity_t *ent ) {
	int i, j;
	vec3_t bombaxis, lookaxis, pos, bomboffset, fallaxis, temp, dir, skypoint;
	gentity_t *bomb;
	trace_t	tr;
	float traceheight, bottomtraceheight;

	VectorCopy( ent->s.pos.trBase,bomboffset );
	bomboffset[2] += 4096.f;

	// turn off smoke grenade
	ent->think = G_ExplodeMissile;
	ent->nextthink = level.time + 950 + NUMBOMBS*100 + crandom()*50; // 950 offset is for aircraft flyby

	ent->active = qtrue;

	G_AddAirstrikeToCounters( ent->parent );

	{
		gentity_t* te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_SOUND );
		te->s.eventParm = G_SoundIndex( "sound/weapons/airstrike/airstrike_plane.wav");
		te->r.svFlags |= SVF_BROADCAST;
	}

	trap_Trace( &tr, ent->s.pos.trBase, NULL, NULL, bomboffset, ent->s.number, MASK_SHOT );
	if ((tr.fraction < 1.0) && (!(tr.surfaceFlags & SURF_NOIMPACT)) ) { //SURF_SKY)) ) { // JPW NERVE changed for trenchtoast foggie prollem
		G_SayTo( ent->parent, ent->parent, 2, COLOR_YELLOW, "Pilot: ", "Aborting, can't see target.", qtrue );

		G_GlobalClientEvent( EV_AIRSTRIKEMESSAGE, 1, ent->parent-g_entities );

/*		te = G_TempEntity( ent->parent->s.pos.trBase, EV_GLOBAL_CLIENT_SOUND );
		if ( ent->s.teamNum == TEAM_ALLIES ) {
			te->s.eventParm = G_SoundIndex( "allies_hq_airstrike_abort" );
		} else {
			te->s.eventParm = G_SoundIndex( "axis_hq_airstrike_abort" );
		}
		te->s.teamNum = ent->parent->s.clientNum;*/

//		te->s.effect1Time = 1;	// don't buffer

		if( ent->s.teamNum == TEAM_AXIS ) {
			level.numActiveAirstrikes[0]--;
		} else {
			level.numActiveAirstrikes[1]--;
		}
		ent->active = qfalse;
		return;
	}

	G_GlobalClientEvent( EV_AIRSTRIKEMESSAGE, 2, ent->parent-g_entities );

/*	te = G_TempEntity( ent->parent->s.pos.trBase, EV_GLOBAL_CLIENT_SOUND );
	if ( ent->parent->client->sess.sessionTeam == TEAM_ALLIES ) {
		te->s.eventParm = G_SoundIndex( "allies_hq_airstrike" );
	} else {
		te->s.eventParm = G_SoundIndex( "axis_hq_airstrike" );
	}
	te->s.teamNum = ent->parent->s.clientNum;*/
//	te->s.effect1Time = 1;	// don't buffer

	VectorCopy( tr.endpos, bomboffset );
	VectorCopy( tr.endpos, skypoint );	
	traceheight = bomboffset[2];
	bottomtraceheight = traceheight - 8192.f;

	VectorSubtract( ent->s.pos.trBase, ent->parent->client->ps.origin, lookaxis );
	lookaxis[2] = 0;
	VectorNormalize( lookaxis );

	dir[0] = 0;
	dir[1] = 0;
	dir[2] = crandom(); // generate either up or down vector
	VectorNormalize( dir ); // which adds randomness to pass direction below

	for( j = 0; j < ent->count; j++ ) {		
		RotatePointAroundVector( bombaxis, dir, lookaxis, 90 + crandom() * 30 ); // munge the axis line a bit so it's not totally perpendicular
		VectorNormalize( bombaxis );

		VectorCopy( bombaxis, pos );
		VectorScale( pos,(float)(-.5f * BOMBSPREAD * NUMBOMBS ), pos );
		VectorAdd( ent->s.pos.trBase, pos, pos ); // first bomb position
		VectorScale( bombaxis, BOMBSPREAD, bombaxis ); // bomb drop direction offset

		for( i = 0; i < NUMBOMBS; i++ ) {
			bomb = G_Spawn();
			bomb->nextthink		= level.time + i * 100 + crandom() * 50 + 1000 + ( j * 2000 ); // 1000 for aircraft flyby, other term for tumble stagger
			bomb->think			= G_AirStrikeExplode;
			bomb->s.eType		= ET_MISSILE;
			bomb->r.svFlags		= SVF_NOCLIENT;
			bomb->s.weapon		= WP_SMOKE_MARKER; // might wanna change this
			bomb->r.ownerNum	= ent->s.number;
			bomb->parent		= ent->parent;
			bomb->s.teamNum		= ent->s.teamNum;
			bomb->damage		= 400; // maybe should un-hard-code these?
			bomb->splashDamage  = 400;

			// Gordon: for explosion type
			bomb->accuracy				= 2;
			bomb->classname				= "air strike";
			bomb->splashRadius			= 400;
			bomb->methodOfDeath			= MOD_AIRSTRIKE;
			bomb->splashMethodOfDeath	= MOD_AIRSTRIKE;
			bomb->clipmask		= MASK_MISSILESHOT;
			bomb->s.pos.trType	= TR_STATIONARY; // was TR_GRAVITY,  might wanna go back to this and drop from height
			//bomb->s.pos.trTime = level.time;		// move a bit on the very first frame
			bomboffset[0]		= crandom() * .5f * BOMBSPREAD;
			bomboffset[1]		= crandom() * .5f * BOMBSPREAD;
			bomboffset[2]		= 0.f;
			VectorAdd( pos, bomboffset, bomb->s.pos.trBase );

			VectorCopy( bomb->s.pos.trBase, bomboffset ); // make sure bombs fall "on top of" nonuniform scenery
			bomboffset[2]		= traceheight;

			VectorCopy( bomboffset, fallaxis );
			fallaxis[2]			= bottomtraceheight;


			trap_Trace( &tr, bomboffset, NULL, NULL, fallaxis, ent-g_entities, bomb->clipmask );
			if( tr.fraction != 1.0 ) {
				VectorCopy(tr.endpos,bomb->s.pos.trBase);

				// Snap origin!
				VectorMA( bomb->s.pos.trBase, 2.f, tr.plane.normal, temp );
				SnapVectorTowards( bomb->s.pos.trBase, temp );			// save net bandwidth

//				G_RailTrail( skypoint, bomb->s.pos.trBase );
				trap_TraceNoEnts( &tr, skypoint, NULL, NULL, bomb->s.pos.trBase, 0, CONTENTS_SOLID );
				if( tr.fraction < 1.f ) {
					G_FreeEntity( bomb );

					// move pos for next bomb
					VectorAdd( pos, bombaxis, pos );

					continue;
				}
			}

			VectorCopy( bomb->s.pos.trBase, bomb->r.currentOrigin );

			// move pos for next bomb
			VectorAdd( pos, bombaxis, pos );
		}
	}
}

// JPW NERVE -- sound effect for spotter round, had to do this as half-second bomb warning

void artilleryThink_real( gentity_t *ent ) {
	ent->freeAfterEvent = qtrue;
	trap_LinkEntity(ent);
	{
		int sfx = rand()%3;

		switch( sfx ) {
		case 0:	G_AddEvent( ent, EV_GENERAL_SOUND, G_SoundIndex( "sound/weapons/artillery/artillery_fly_1.wav" )); break;
		case 1: G_AddEvent( ent, EV_GENERAL_SOUND, G_SoundIndex( "sound/weapons/artillery/artillery_fly_2.wav" )); break;
		case 2: G_AddEvent( ent, EV_GENERAL_SOUND, G_SoundIndex( "sound/weapons/artillery/artillery_fly_3.wav" )); break;
		}
	}
}
void artilleryThink( gentity_t *ent ) {
	ent->think = artilleryThink_real;
	ent->nextthink = level.time + 100;

	ent->r.svFlags = SVF_BROADCAST;
}

// JPW NERVE -- makes smoke disappear after a bit (just unregisters stuff)
void artilleryGoAway(gentity_t *ent) {
	ent->freeAfterEvent = qtrue;
	trap_LinkEntity(ent);
}

// JPW NERVE -- generates some smoke debris
void artillerySpotterThink( gentity_t *ent ) {
	gentity_t *bomb;
	vec3_t tmpdir;
	int i;
	ent->think = G_ExplodeMissile;
	ent->nextthink = level.time + 1;
	SnapVector( ent->s.pos.trBase );

	for( i = 0; i < 7; i++ ) {
		bomb = G_Spawn();
		bomb->s.eType		= ET_MISSILE;
		bomb->r.svFlags		= 0;
		bomb->r.ownerNum	= ent->s.number;
		bomb->parent		= ent;
		bomb->s.teamNum		= ent->s.teamNum;
		bomb->nextthink		= level.time + 1000 + random() * 300;
		bomb->classname		= "WP";				// WP == White Phosphorous, so we can check for bounce noise in grenade bounce routine
		bomb->damage		= 000;				// maybe should un-hard-code these?
		bomb->splashDamage  = 000;
		bomb->splashRadius	= 000;
		bomb->s.weapon		= WP_SMOKETRAIL;
		bomb->think			= artilleryGoAway;
		bomb->s.eFlags		|= EF_BOUNCE;
		bomb->clipmask		= MASK_MISSILESHOT;
		bomb->s.pos.trType	= TR_GRAVITY;		// was TR_GRAVITY,  might wanna go back to this and drop from height
		bomb->s.pos.trTime	= level.time;		// move a bit on the very first frame
		bomb->s.otherEntityNum2	= ent->s.otherEntityNum2;
		VectorCopy( ent->s.pos.trBase, bomb->s.pos.trBase );
		tmpdir[0]			= crandom();
		tmpdir[1]			= crandom();
		tmpdir[2]			= 1;
		VectorNormalize( tmpdir );
		tmpdir[2]			= 1; // extra up
		VectorScale( tmpdir, 500 + random() * 500, tmpdir );
		VectorCopy( tmpdir,bomb->s.pos.trDelta );
		SnapVector( bomb->s.pos.trDelta );			// save net bandwidth
		VectorCopy( ent->s.pos.trBase, bomb->s.pos.trBase );
		VectorCopy( ent->s.pos.trBase, bomb->r.currentOrigin );
	}
}

void G_GlobalClientEvent( int event, int param, int client ) {
	gentity_t* tent = G_TempEntity( vec3_origin, event );
	tent->s.density = param;
	tent->r.singleClient = client;
	tent->r.svFlags = SVF_SINGLECLIENT | SVF_BROADCAST;
}

/*
==================
Weapon_Artillery
==================
*/
/* TC2009cea5..2009cebd retains the charge product until __ftol64. */
static int G_TCEArtilleryChargeTime( int artilleryTime, int artilleryCharge ) {
#if defined(_MSC_VER) && defined(_M_IX86)
	const float artilleryFraction = 0.66f;
	int artilleryResult;
	__asm {
		fild dword ptr artilleryCharge
		fmul artilleryFraction
		fiadd dword ptr artilleryTime
		call G_TCEConstructionFtol
		mov artilleryResult, eax
	}
	return artilleryResult;
#else
	return artilleryTime + 0.66f * artilleryCharge;
#endif
}

/* TC2009cb43..cbda; rand state belongs to the existing CRT boundary. */
static int G_TCEArtilleryShellTime( int artilleryIndex ) {
	int artilleryRandom = rand() & 0x7fff;
	int artilleryBase = (int)((unsigned int)level.time + 8950u + 2000u * (unsigned int)artilleryIndex);
#if defined(_MSC_VER) && defined(_M_IX86)
	const float artilleryReciprocal = 3.0518509447574615e-05f;
	const double artilleryHalf = 0.5, artilleryJitter = 800.0;
	int artilleryResult;
	__asm {
		fild dword ptr artilleryRandom
		fmul artilleryReciprocal
		fsub qword ptr artilleryHalf
		fadd st(0), st(0)
		fmul qword ptr artilleryJitter
		fild dword ptr artilleryBase
		faddp st(1), st(0)
		call G_TCEConstructionFtol
		mov artilleryResult, eax
	}
	return artilleryResult;
#else
	return artilleryBase + (2.0f * ((float)artilleryRandom / 32767.0f - 0.5f)) * 800.0f;
#endif
}

/* TC2009cc35..ccd7 stores each scaled offset once, after the f64 multiply. */
static void G_TCEArtilleryOffset( float *artilleryDestination, double artilleryRadius ) {
	int artilleryRandom = rand() & 0x7fff;
#if defined(_MSC_VER) && defined(_M_IX86)
	const float artilleryReciprocal = 3.0518509447574615e-05f;
	const double artilleryHalf = 0.5;
	__asm {
		fild dword ptr artilleryRandom
		fmul artilleryReciprocal
		fsub qword ptr artilleryHalf
		fadd st(0), st(0)
		fmul qword ptr artilleryRadius
		mov eax, artilleryDestination
		fstp dword ptr [eax]
	}
#else
	*artilleryDestination = (2.0f * ((float)artilleryRandom / 32767.0f - 0.5f)) * artilleryRadius;
#endif
}

/* TC2009c88e..c8f6: integer height and retained direction multiply/add. */
static void G_TCEArtilleryAim( vec3_t artilleryMuzzle, int artilleryHeight,
							const vec3_t artilleryForward, vec3_t artilleryEnd ) {
#if defined(_MSC_VER) && defined(_M_IX86)
	const float artilleryRange = 8192.0f;
	int artilleryAxis;
	float *artilleryZ = &artilleryMuzzle[2];
	__asm {
		mov eax, artilleryZ
		fild dword ptr artilleryHeight
		fadd dword ptr [eax]
		fstp dword ptr [eax]
	}
	for (artilleryAxis = 0; artilleryAxis < 3; ++artilleryAxis) {
		const float *artilleryDirection = &artilleryForward[artilleryAxis];
		const float *artilleryOrigin = &artilleryMuzzle[artilleryAxis];
		float *artilleryTarget = &artilleryEnd[artilleryAxis];
		__asm {
			mov eax, artilleryDirection
			fld dword ptr [eax]
			fmul artilleryRange
			mov eax, artilleryOrigin
			fadd dword ptr [eax]
			mov eax, artilleryTarget
			fstp dword ptr [eax]
		}
	}
#else
	artilleryMuzzle[2] += artilleryHeight;
	VectorMA(artilleryMuzzle, 8192, artilleryForward, artilleryEnd);
#endif
}

static unsigned int G_TCEArtilleryTraceStatus( float artilleryFraction ) {
#if defined(_MSC_VER) && defined(_M_IX86)
	const double artilleryFullTrace = 1.0;
	unsigned short artilleryStatus;
	__asm {
		fld artilleryFraction
		fcomp qword ptr artilleryFullTrace
		fnstsw artilleryStatus
	}
	return artilleryStatus;
#else
	if (artilleryFraction != artilleryFraction) return 0x4500u;
	if (artilleryFraction < 1.0f) return 0x100u;
	return artilleryFraction == 1.0f ? 0x4000u : 0u;
#endif
}

void Weapon_Artillery(gentity_t *ent) {
	trace_t trace;
	int i, count;	
	vec3_t muzzlePoint, end, bomboffset, pos, fallaxis;
	float traceheight, bottomtraceheight;
	gentity_t *bomb, *bomb2;

	if( ent->client->ps.stats[STAT_PLAYER_CLASS] != PC_FIELDOPS ) {
		G_Printf("not a fieldops, you can't shoot this!\n");
		return;
	}

	// TAT - 10/27/2002 - moved energy check into a func, so I can use same check in bot code
	if( !ReadyToCallArtillery(ent) ) {
		return;
	}

	if( ent->client->sess.sessionTeam == TEAM_AXIS ) {
		if( !G_AvailableAirstrikes( ent ) ) {
			G_SayTo( ent, ent, 2, COLOR_YELLOW, "Fire Mission: ", "Insufficient fire support.", qtrue );
			ent->active = qfalse;

			G_GlobalClientEvent( EV_ARTYMESSAGE, 0, ent-g_entities );

/*			te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_CLIENT_SOUND );
			te->s.eventParm = G_SoundIndex( "axis_hq_airstrike_denied" );
			te->s.teamNum = ent-g_entities;*/

			return;
		}
	} else {
		if( !G_AvailableAirstrikes( ent ) ) {
			G_SayTo( ent, ent, 2, COLOR_YELLOW, "Fire Mission: ", "Insufficient fire support.", qtrue );
			ent->active = qfalse;

			G_GlobalClientEvent( EV_ARTYMESSAGE, 0, ent-g_entities );

/*			te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_CLIENT_SOUND );
			te->s.eventParm = G_SoundIndex( "allies_hq_airstrike_denied" );
			te->s.teamNum = ent-g_entities;*/

			return;
		}
	}

	AngleVectors (ent->client->ps.viewangles, forward, right, up);

	VectorCopy( ent->r.currentOrigin, muzzlePoint );
	G_TCEArtilleryAim(muzzlePoint, ent->client->ps.viewheight, forward, end);
	trap_Trace (&trace, muzzlePoint, NULL, NULL, end, ent->s.number, MASK_MISSILESHOT);

	if (trace.surfaceFlags & SURF_NOIMPACT)
		return;

	VectorCopy(trace.endpos,pos);
	VectorCopy(pos,bomboffset);
	bomboffset[2] += 4096;

	trap_Trace(&trace, pos, NULL, NULL, bomboffset, ent->s.number, MASK_MISSILESHOT);
	if ((G_TCEArtilleryTraceStatus(trace.fraction) & 0x100u) && (!(trace.surfaceFlags & SURF_NOIMPACT)) ) {
		G_SayTo( ent, ent, 2, COLOR_YELLOW, "Fire Mission: ", "Aborting, can't see target.", qtrue );

		G_GlobalClientEvent( EV_ARTYMESSAGE, 1, ent-g_entities );

/*		te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_CLIENT_SOUND );
		if ( ent->client->sess.sessionTeam == TEAM_ALLIES ) {
			te->s.eventParm = G_SoundIndex( "allies_hq_ffe_abort" );
		} else {
			te->s.eventParm = G_SoundIndex( "axis_hq_ffe_abort" );
		}
		te->s.teamNum = ent->s.clientNum;*/

//		te->s.effect1Time = 1;	// don't buffer
		return;
	}

	G_AddAirstrikeToCounters( ent );

	G_SayTo( ent, ent, 2, COLOR_YELLOW, "Fire Mission: ", "Firing for effect!", qtrue );

	G_GlobalClientEvent( EV_ARTYMESSAGE, 2, ent-g_entities );

/*	te = G_TempEntity( ent->s.pos.trBase, EV_GLOBAL_CLIENT_SOUND );
	if ( ent->client->sess.sessionTeam == TEAM_ALLIES ) {
		te->s.eventParm = G_SoundIndex( "allies_hq_ffe" );
	} else {
		te->s.eventParm = G_SoundIndex( "axis_hq_ffe" );
	}
	te->s.teamNum = ent->s.clientNum;*/

//	te->s.effect1Time = 1;	// don't buffer

	VectorCopy( trace.endpos, bomboffset );
	traceheight = bomboffset[2];
	bottomtraceheight = traceheight - 8192;


// "spotter" round (i == 0)
// i == 1->4 is regular explosives
	if( ent->client->sess.skill[SK_SIGNALS] >= 3 ) {
		count = 9;
	} else {
		count = 5;
	}

	for( i = 0; i < count; i++ ) {
		bomb				= G_Spawn();
		bomb->think			= G_AirStrikeExplode;
		bomb->s.eType		= ET_MISSILE;
		bomb->r.svFlags		= SVF_NOCLIENT;
		bomb->s.weapon		= 63; // TC2009c730 artillery effect weapon; SDK WP_ARTY is13
		bomb->r.ownerNum	= ent->s.number;
		bomb->s.clientNum	= ent->s.number;
		bomb->parent		= ent;
		bomb->s.teamNum		= ent->client->sess.sessionTeam;

		if (i == 0) {
			bomb->nextthink		= (int)((unsigned int)level.time + 5000u);
			bomb->r.svFlags		= SVF_BROADCAST;
			bomb->classname		= "props_explosion"; // was "air strike"
			bomb->damage		= 0; // maybe should un-hard-code these?
			bomb->splashDamage  = 90;
			bomb->splashRadius	= 50;
			bomb->count			= 7;
			bomb->count2		= 1000;
			bomb->delay			= 300;
			bomb->s.otherEntityNum2 = 1;	// first bomb

			bomb->think = artillerySpotterThink;
		} else {
			if( ent->client->sess.skill[SK_SIGNALS] >= 3 )
				bomb->nextthink		= G_TCEArtilleryShellTime(i);
			else
				bomb->nextthink		= G_TCEArtilleryShellTime(i);

			// Gordon: for explosion type
			bomb->accuracy		= 2;
			bomb->classname		= "air strike";
			bomb->damage		= 0;
			bomb->splashDamage  = 400;
			bomb->splashRadius	= 400;
		}
		bomb->methodOfDeath			= MOD_ARTY;
		bomb->splashMethodOfDeath	= MOD_ARTY;
		bomb->clipmask				= MASK_MISSILESHOT;
		bomb->s.pos.trType			= TR_STATIONARY; // was TR_GRAVITY,  might wanna go back to this and drop from height
		bomb->s.pos.trTime			= level.time;		// move a bit on the very first frame
		if( i ) { // spotter round is always dead on (OK, unrealistic but more fun)
			G_TCEArtilleryOffset(&bomboffset[0], 250.0);
			G_TCEArtilleryOffset(&bomboffset[1], 250.0);
		} else {
			G_TCEArtilleryOffset(&bomboffset[0], 50.0);
			G_TCEArtilleryOffset(&bomboffset[1], 50.0);
		}
		bomboffset[2] = 0;
		VectorAdd(pos,bomboffset,bomb->s.pos.trBase);

		VectorCopy(bomb->s.pos.trBase,bomboffset); // make sure bombs fall "on top of" nonuniform scenery
		bomboffset[2] = traceheight;

		VectorCopy(bomboffset, fallaxis);
		fallaxis[2] = bottomtraceheight;

		trap_Trace(&trace, bomboffset, NULL, NULL, fallaxis, ent->s.number, MASK_MISSILESHOT);
		if (!(G_TCEArtilleryTraceStatus(trace.fraction) & 0x4000u))
			VectorCopy(trace.endpos,bomb->s.pos.trBase);	

		bomb->s.pos.trDelta[0] = 0; // might need to change this
		bomb->s.pos.trDelta[1] = 0;
		bomb->s.pos.trDelta[2] = 0;
		SnapVector( bomb->s.pos.trDelta );			// save net bandwidth
		VectorCopy (bomb->s.pos.trBase, bomb->r.currentOrigin);

// build arty falling sound effect in front of bomb drop
		bomb2 = G_Spawn();
		bomb2->think = artilleryThink;
		bomb2->s.eType	= ET_MISSILE;
		bomb2->r.svFlags	= SVF_NOCLIENT;
		bomb2->r.ownerNum	= ent->s.number;
		bomb2->parent		= ent;
		bomb2->s.teamNum	= ent->s.teamNum;
		bomb2->damage		= 0;
		bomb2->nextthink = (int)((unsigned int)bomb->nextthink - 600u);
		bomb2->classname = "air strike";
		bomb2->clipmask = MASK_MISSILESHOT;
		bomb2->s.pos.trType = TR_STATIONARY; // was TR_GRAVITY,  might wanna go back to this and drop from height
		bomb2->s.pos.trTime = level.time;		// move a bit on the very first frame
		VectorCopy(bomb->s.pos.trBase,bomb2->s.pos.trBase);
		VectorCopy(bomb->s.pos.trDelta,bomb2->s.pos.trDelta);
		VectorCopy(bomb->s.pos.trBase,bomb2->r.currentOrigin);
	}

	if( ent->client->sess.skill[SK_SIGNALS] >= 2 ) {
		if ((int)((unsigned int)level.time - (unsigned int)ent->client->ps.classWeaponTime) > level.lieutenantChargeTime[ent->client->sess.sessionTeam-1])
			ent->client->ps.classWeaponTime = (int)((unsigned int)level.time - (unsigned int)level.lieutenantChargeTime[ent->client->sess.sessionTeam-1]);
		
		ent->client->ps.classWeaponTime = G_TCEArtilleryChargeTime(ent->client->ps.classWeaponTime, level.lieutenantChargeTime[ent->client->sess.sessionTeam-1]);
	} else {
		ent->client->ps.classWeaponTime = level.time;
	}

	// OSP -- weapon stats
/* Original2009ced1 gates the counter in every build. */
	if(g_gamestate.integer == GS_PLAYING)
		ent->client->sess.aWeaponStats[WS_ARTILLERY].atts++;

}


#define SMOKEBOMB_GROWTIME 1000
#define SMOKEBOMB_SMOKETIME 15000
#define SMOKEBOMB_POSTSMOKETIME 2000	
// xkan, 11/25/2002 - increases postsmoke time from 2000->32000, this way, the entity 
// is still around while the smoke is around, so we can check if it blocks bot's vision 
// Arnout: eeeeeh this is wrong. 32 seconds is way too long. Also - we shouldn't be
// rendering the grenade anymore after the smoke stops and definately not send it to the client
// xkan, 12/06/2002 - back to the old value 2000, now that it looks like smoke disappears more
// quickly

void weapon_smokeBombExplode( gentity_t *ent ) {
	int lived = 0;

	if( !ent->grenadeExplodeTime )
		ent->grenadeExplodeTime = level.time;

	lived = level.time - ent->grenadeExplodeTime;
	ent->nextthink = level.time + FRAMETIME;

	if( lived < SMOKEBOMB_GROWTIME ) {
		// Just been thrown, increase radius
		ent->s.effect1Time = 16 + lived * ((640.f-16.f)/(float)SMOKEBOMB_GROWTIME);
	} else if( lived < SMOKEBOMB_SMOKETIME + SMOKEBOMB_GROWTIME ) {
		// Smoking
		ent->s.effect1Time = 640;
	} else if( lived < SMOKEBOMB_SMOKETIME + SMOKEBOMB_GROWTIME + SMOKEBOMB_POSTSMOKETIME ) {
		// Dying out
		ent->s.effect1Time = -1;
	} else {
		// Poof and it's gone
		G_FreeEntity( ent );
	}
}

gentity_t *LaunchItem( gitem_t *item, vec3_t origin, vec3_t velocity, int ownerNum );
// jpw

/*
======================================================================

MACHINEGUN

======================================================================
*/

/*
======================
SnapVectorTowards

Round a vector to integers for more efficient network
transmission, but make sure that it rounds towards a given point
rather than blindly truncating.  This prevents it from truncating 
into a wall.
======================
*/

// (SA) modified so it doesn't have trouble with negative locations (quadrant problems)
//			(this was causing some problems with bullet marks appearing since snapping
//			too far off the target surface causes the the distance between the transmitted impact
//			point and the actual hit surface larger than the mark radius.  (so nothing shows) )

void SnapVectorTowards( vec3_t v, vec3_t to ) {
	int		i;

	for ( i = 0 ; i < 3 ; i++ ) {
		if ( to[i] <= v[i] ) {
//			v[i] = (int)v[i];
			v[i] = floor(v[i]);
		} else {
//			v[i] = (int)v[i] + 1;
			v[i] = ceil(v[i]);
		}
	}
}

// JPW
// mechanism allows different weapon damage for single/multiplayer; we want "balanced" weapons
// in multiplayer but don't want to alter the existing single-player damage items that have already
// been changed
//
// KLUDGE/FIXME: also modded #defines below to become macros that call this fn for minimal impact elsewhere
//
/* Windows 2009cf40; TC protocol IDs, never SDK enum aliases. */
int G_GetWeaponDamage(int weapon) {
    switch (weapon) {
    case 1: case 57: case 58: return 50;
    case 2: case 3: case 7: case 8: case 14: case 31:
    case 37: case 38: case 39: case 40: case 52: case 53: case 54: case 62: return 18;
    case 4: case 30: return 60;
    case 9: case 17: case 26: case 27: case 55: case 56: return 250;
    case 10: return 14;
    case 15: case 60: case 65: return 400;
    case 22: return 140;
    case 23: case 24: case 25: case 32: return 34;
    case 29: return 300;
    case 33: return 15;
    case 59: return 30;
    default: return 1;
    }
}

/* Windows 2009d020. FireWeapon deliberately requests pistol spread for all
 * gear firearms; their actual cone is constructed by Bullet_Endpos. */
float G_GetWeaponSpread(int weapon) {
    switch (weapon) {
    case 2: case 7: case 14: case 37: case 38: case 39: case 40:
    case 52: case 53: case 54: return 600;
    case 3: case 8: return 400;
    case 10: case 59: return 200;
    case 23: case 24: case 25: case 32: return 250;
    case 31: case 62: return 2500;
    case 33: return 500;
    case 57: case 58: return 700;
    default:
        G_Printf("shouldn't ever get here (weapon %d)\n", weapon);
        return 0;
    }
}

#define LUGER_SPREAD	G_GetWeaponSpread(WP_LUGER)
#define LUGER_DAMAGE	G_GetWeaponDamage(WP_LUGER) // JPW

#define SILENCER_DAMAGE		G_GetWeaponDamage(WP_SILENCER)
#define SILENCER_SPREAD		G_GetWeaponSpread(WP_SILENCER)

#define AKIMBO_LUGER_DAMAGE			G_GetWeaponDamage(WP_AKIMBO_LUGER)
#define AKIMBO_LUGER_SPREAD			G_GetWeaponSpread(WP_AKIMBO_LUGER)

#define AKIMBO_SILENCEDLUGER_DAMAGE	G_GetWeaponDamage(WP_AKIMBO_SILENCEDLUGER)
#define AKIMBO_SILENCEDLUGER_SPREAD	G_GetWeaponSpread(WP_AKIMBO_SILENCEDLUGER)

#define COLT_SPREAD		G_GetWeaponSpread(WP_COLT)
#define	COLT_DAMAGE		G_GetWeaponDamage(WP_COLT) // JPW

#define SILENCED_COLT_DAMAGE	G_GetWeaponDamage(WP_SILENCED_COLT)
#define SILENCED_COLT_SPREAD	G_GetWeaponSpread(WP_SILENCED_COLT)

#define AKIMBO_COLT_DAMAGE	G_GetWeaponDamage(WP_AKIMBO_COLT)
#define AKIMBO_COLT_SPREAD	G_GetWeaponSpread(WP_AKIMBO_COLT)

#define AKIMBO_SILENCEDCOLT_DAMAGE	G_GetWeaponDamage(WP_AKIMBO_SILENCEDCOLT)
#define AKIMBO_SILENCEDCOLT_SPREAD	G_GetWeaponSpread(WP_AKIMBO_SILENCEDCOLT)

#define MP40_SPREAD		G_GetWeaponSpread(WP_MP40)
#define	MP40_DAMAGE		G_GetWeaponDamage(WP_MP40) // JPW
#define THOMPSON_SPREAD	G_GetWeaponSpread(WP_THOMPSON)
#define	THOMPSON_DAMAGE	G_GetWeaponDamage(WP_THOMPSON) // JPW
#define STEN_SPREAD		G_GetWeaponSpread(WP_STEN)
#define	STEN_DAMAGE		G_GetWeaponDamage(WP_STEN) // JPW

#define GARAND_SPREAD	G_GetWeaponSpread(WP_GARAND)
#define	GARAND_DAMAGE	G_GetWeaponDamage(WP_GARAND) // JPW

#define KAR98_SPREAD	G_GetWeaponSpread(WP_KAR98)
#define	KAR98_DAMAGE	G_GetWeaponDamage(WP_KAR98)

#define CARBINE_SPREAD	G_GetWeaponSpread(WP_CARBINE)
#define	CARBINE_DAMAGE	G_GetWeaponDamage(WP_CARBINE)

#define	KAR98_GREN_DAMAGE	G_GetWeaponDamage(WP_GREN_KAR98)

#define MOBILE_MG42_SPREAD	G_GetWeaponSpread(WP_MOBILE_MG42)
#define	MOBILE_MG42_DAMAGE	G_GetWeaponDamage(WP_MOBILE_MG42)

#define FG42_SPREAD		G_GetWeaponSpread(WP_FG42)
#define	FG42_DAMAGE		G_GetWeaponDamage(WP_FG42) // JPW

#define FG42SCOPE_SPREAD	G_GetWeaponSpread(WP_FG42SCOPE)
#define	FG42SCOPE_DAMAGE	G_GetWeaponDamage(WP_FG42SCOPE) // JPW
#define K43_SPREAD	G_GetWeaponSpread(WP_K43)
#define	K43_DAMAGE	G_GetWeaponDamage(WP_K43)

#define GARANDSCOPE_SPREAD	G_GetWeaponSpread(WP_GARAND_SCOPE)
#define GARANDSCOPE_DAMAGE	G_GetWeaponDamage(WP_GARAND_SCOPE)

#define K43SCOPE_SPREAD	G_GetWeaponSpread(WP_K43_SCOPE)
#define K43SCOPE_DAMAGE G_GetWeaponDamage(WP_K43_SCOPE)

void RubbleFlagCheck (gentity_t *ent, trace_t tr)
{
	qboolean	is_valid = qfalse;
	int			type = 0;

	// (SA) moving client-side

	return;




	if (tr.surfaceFlags & SURF_RUBBLE || tr.surfaceFlags & SURF_GRAVEL)
	{
		is_valid = qtrue;
		type = 4;
	}
	else if (tr.surfaceFlags & SURF_METAL)
	{
//----(SA)	removed
//		is_valid = qtrue;
//		type = 2;
	}
	else if (tr.surfaceFlags & SURF_WOOD)
	{
		is_valid = qtrue;
		type = 1;
	}
	
	if (is_valid && ent->client && ( ent->client->ps.persistant[PERS_HWEAPON_USE] ) )
	{
		if (rand()%100 > 75)
		{
			gentity_t	*sfx;
			vec3_t		start;
			vec3_t		dir;

			sfx = G_Spawn ();

			sfx->s.density = type; 

			VectorCopy (tr.endpos, start);

			VectorCopy (muzzleTrace, dir);
			VectorNegate (dir, dir);

			G_SetOrigin (sfx, start); 
			G_SetAngle (sfx, dir);

			G_AddEvent( sfx, EV_SHARD, DirToByte( dir ));

			sfx->think = G_FreeEntity;
			sfx->nextthink = level.time + 1000;

			sfx->s.frame = 3 + (rand()%3) ;
								
			trap_LinkEntity (sfx);

		}
	}
}

/*
==============
EmitterCheck
	see if a new particle emitter should be created at the bullet impact point
==============
*/
void EmitterCheck(gentity_t *ent, gentity_t *attacker, trace_t *tr) {
	gentity_t *tent;
	vec3_t	origin;

	VectorCopy(tr->endpos, origin);
	SnapVectorTowards( tr->endpos, attacker->s.origin);

	if(Q_stricmp(ent->classname, "func_explosive") == 0) {
	} else if(Q_stricmp(ent->classname, "func_leaky") == 0) {


		tent = G_TempEntity (origin, EV_EMITTER);
		VectorCopy (origin, tent->s.origin);
		tent->s.time = 1234;
		tent->s.density = 9876;
		VectorCopy (tr->plane.normal, tent->s.origin2);

	}
}


/*
==============
Bullet_Endpos
	find target end position for bullet trace based on entities weapon and accuracy
==============
*/
void Bullet_Endpos(gentity_t *ent, float spread, vec3_t *end) {
    /* TC 2009d1a0: every shot uses PM_Weapon's pre-recoil snapshot. */
    {
        tce_bulletAim_t aim;
        memset(&aim, 0, sizeof(aim));
        aim.seed = ent->client->ps.stats[STAT_TCE_SHOT_SEED];
        aim.aiming = !!(ent->client->ps.stats[STAT_TCE_WEAPON_FLAGS] & 4);
        aim.prone = !!(ent->client->ps.eFlags & EF_PRONE);
        aim.ducked = !!(ent->client->ps.pm_flags & PMF_DUCKED);
        aim.movementSpread = ent->client->ps.holdable[0];
        aim.shotSpread = ent->client->ps.holdable[1];
        aim.phase = ent->client->ps.stats[STAT_TCE_AIM_PHASE];
        VectorCopy(ent->client->pmext.tceShotAngles, aim.shotAngles);
        VectorCopy(muzzleTrace, aim.muzzle);
        VectorCopy(right, aim.right);
        VectorCopy(up, aim.up);
        TCE_BulletEndpos(&weaponDef[ent->s.weapon], &aim, *end);
        return;
    }
}

/*
==============
Bullet_Fire
==============
*/
/* Windows 2009d410: SDK damage/falloff arguments are retained only for the
 * existing C callers; TC obtains both damage and penetration from gear. */
void Bullet_Fire (gentity_t *ent, float spread, int damage, qboolean distance_falloff) {
    vec3_t end;
    const tce_weaponDef_t *def;
    int seed;
    if (g_antilag.integer && ent->client && !(ent->r.svFlags & SVF_BOT))
        G_TimeShiftAllClients(ent, qtrue);
    seed = ent->client->ps.stats[STAT_TCE_SHOT_SEED];
    Bullet_Endpos(ent, spread, &end);
    def = &weaponDef[ent->client->ps.weapon];
    damage = def->unknown_12c;
    if (def->unknown_1c0 && (ent->client->ps.stats[STAT_TCE_WEAPON_FLAGS] & 4))
        damage = (int)(damage * 1.5);
    Bullet_Fire_Extended(ent, ent, muzzleTrace, end, spread, damage,
        def->unknown_134, 0, 0, 4, 4, 8, seed, 0);
    if (g_antilag.integer && ent->client && !(ent->r.svFlags & SVF_BOT))
        G_UnTimeShiftAllClients(ent);
}


/*
==============
Bullet_Fire_Extended
	A modified Bullet_Fire with more parameters.
	The original Bullet_Fire still passes through here and functions as it always has.

	uses for this include shooting through entities (windows, doors, other players, etc.) and reflecting bullets
==============
*/
/* Windows 2009d500 / Linux 0010eae8. TC's fourteen-argument trace
 * controller. Counters independently bound wall, flesh and bounding-box passes. */
qboolean Bullet_Fire_Extended(gentity_t *source, gentity_t *attacker,
    vec3_t start, vec3_t end, float spread, int damage, int penetration,
    int maxDistance, int totalDistance, int wallsRemaining, int bodiesRemaining,
    int passesRemaining, int seed, int suppressWallEvents) {
    trace_t tr, next;
    gentity_t *hit, *event;
    vec3_t entry, direction, probe, eye, point, low, high;
    const tce_weaponDef_t *def;
    const tce_pierceMaterial_t *material, *exitMaterial;
    float bboxScale = g_newbbox.integer ? 1.25f : 1.0f;
    float result, fraction, energy, range, value;
    int materialType, exitType, applied, i;
    double thickness, retained;

    trap_Trace(&tr, start, NULL, NULL, end, source->s.number, MASK_SHOT);
    if (maxDistance && maxDistance < tr.fraction * 8192.0f) return qtrue;
    if (g_debugBullets.integer & 1) {
        event = G_TempEntity(start, EV_RAILTRAIL);
        VectorCopy(tr.endpos, event->s.origin2);
        event->s.otherEntityNum2 = attacker->s.number;
    }
    RubbleFlagCheck(attacker, tr);
    hit = &g_entities[tr.entityNum];
    EmitterCheck(hit, attacker, &tr);
    SnapVectorTowards(tr.endpos, start);
    totalDistance = (int)(Distance(tr.endpos, start) + (float)totalDistance);

    if ((hit->takedamage && hit->client && g_debugBullets.integer > 1) ||
        (!(hit->takedamage && hit->client) && g_debugBullets.integer < -1)) {
        VectorAdd(hit->r.currentOrigin, hit->r.mins, low);
        VectorAdd(hit->r.currentOrigin, hit->r.maxs, high);
        event = G_TempEntity(low, EV_RAILTRAIL);
        VectorCopy(high, event->s.origin2);
        event->s.dmgFlags = 1;
    }
    if (hit->takedamage && hit->client) {
        def = &weaponDef[attacker->client->ps.weapon];
        applied = TCE_BulletClientDamage(def, damage, totalDistance, g_newbbox.integer);
        result = G_Damage(hit, attacker, attacker, forward, tr.endpos, applied,
                          0, weaponDef[attacker->s.weapon].mod);
        if (result == 1.0f) {
            trap_Trace(&next, tr.endpos, NULL, NULL, end, hit->s.number, MASK_SHOT);
            if (Distance(next.endpos, tr.endpos) > bboxScale * 48.0f) {
                VectorSubtract(end, tr.endpos, direction);
                VectorNormalize(direction);
                VectorMA(tr.endpos, 32.0f, direction, point);
                VectorCopy(hit->client->ps.origin, eye);
                eye[2] += hit->client->ps.viewheight;
                if (Distance(tr.endpos, eye) < Distance(point, eye))
                    VectorCopy(tr.endpos, point);
                VectorSubtract(point, eye, direction);
                VectorNormalize(direction);
                event = G_TempEntity(point, EV_TCE_BULLET_NEAR_MISS);
                VectorCopy(direction, event->s.origin2);
                event->s.eventParm = hit->s.number;
                event->r.singleClient = hit->s.number;
                event->r.svFlags = SVF_SINGLECLIENT;
            }
            if (--passesRemaining > 0)
                return Bullet_Fire_Extended(hit, attacker, tr.endpos, end, 0, damage,
                    penetration, 0, totalDistance, wallsRemaining, bodiesRemaining,
                    passesRemaining, seed, suppressWallEvents);
        } else if (hit->client) {
            if (hit->client->tceLastAppliedDamage > 5) {
                event = G_TempEntity(tr.endpos, EV_BULLET_HIT_FLESH);
                event->s.eventParm = hit->s.number;
                VectorCopy(start, event->s.origin2);
                AccuracyHit(hit, attacker);
                event->s.otherEntityNum = attacker->s.number;
                event->s.density = hit->client->tceLastAppliedDamage;
            }
            if (result != 0.0f) {
                material = &tcePierceTable[16];
                fraction = (penetration - material->resistance * (1.1f - result) *
                    (16.0f / (material->thicknessScale * (1.1f - result)) + 1.0f)) /
                    penetration;
                energy = fraction * penetration;
                if (fraction > 0.0f && energy >= 5.0f && --bodiesRemaining > 0)
                    return Bullet_Fire_Extended(hit, attacker, tr.endpos, end, 0,
                        (int)(energy / def->unknown_134 * damage), (int)energy, 0,
                        totalDistance, wallsRemaining, bodiesRemaining, passesRemaining,
                        seed, suppressWallEvents);
            }
        }
        return qfalse;
    }

    materialType = BG_SurfaceFlag2Type(tr.surfaceFlags);
    if (!suppressWallEvents) {
        event = G_TempEntity(tr.endpos, EV_BULLET_HIT_WALL);
        VectorCopy(start, event->s.origin2);
        event->s.eventParm = DirToByte(tr.plane.normal);
        event->s.otherEntityNum2 = materialType;
        event->s.otherEntityNum = attacker->s.number;
    }
    if (wallsRemaining < 1 || !source->client) return qfalse;
    def = &weaponDef[source->client->ps.weapon];
    material = &tcePierceTable[materialType];
    /* The original leaves this point uninitialized when a hard material is an
     * explosive. Use the actual snapped hit rather than leaking stack values. */
    VectorCopy(tr.endpos, entry);
    if (!(tr.surfaceFlags & 0x80000) && material->resistance < 9999 &&
        (material->resistance < 1000 || !Q_stricmp(def->caliberClass, "338LAPUA") ||
         !Q_stricmp(def->caliberClass, "50BMG"))) {
        VectorSubtract(end, start, direction);
        VectorNormalize(direction);
        for (i = 0; i < 3; ++i)
            probe[i] = (entry[i] - tr.plane.normal[i]) + direction[i];
        trap_Trace(&next, probe, NULL, NULL, end, source->s.number, CONTENTS_SOLID);
        if (next.fraction >= 1.0f) goto damageExplosive;
        if (Distance(next.endpos, entry) < 4.0f) {
            for (i = 0; i < 3; ++i)
                probe[i] = (float)(((double)entry[i] - tr.plane.normal[i]) +
                                  direction[i] * 4.0);
            trap_Trace(&next, probe, NULL, NULL, end, source->s.number, CONTENTS_SOLID);
            if (next.fraction >= 1.0f) goto damageExplosive;
        }
        SnapVectorTowards(next.endpos, probe);
        VectorCopy(next.endpos, probe);
        trap_Trace(&next, probe, NULL, NULL, start, source->s.number, CONTENTS_SOLID);
        SnapVectorTowards(next.endpos, probe);
        if (hit->s.eType == ET_EXPLOSIVE) {
            range = def->unknown_130 * bboxScale * 39.370079f;
            if (range <= 0.0f) range = 999999.0f;
            value = damage / (totalDistance / range + 1.0f);
            if (value < 0.0f) value = 0.0f;
            G_Damage(hit, attacker, attacker, forward, entry, (int)value, 0,
                     weaponDef[attacker->s.weapon].mod);
        }
        if (Distance(next.endpos, start) <= Distance(entry, start)) return qfalse;
        exitType = BG_SurfaceFlag2Type(next.surfaceFlags);
        exitMaterial = &tcePierceTable[exitType];
        if (exitMaterial->resistance >= material->resistance) material = exitMaterial;
        thickness = Distance(next.endpos, entry);
        if (thickness < 0) thickness = 0;
        retained = (penetration -
            (thickness / material->thicknessScale + 1.0) * material->resistance) /
            penetration;
        fraction = (float)retained;
        energy = fraction * penetration;
        if (retained > 0 && energy >= 5.0f) {
            if (!suppressWallEvents) {
                event = G_TempEntity(next.endpos, EV_TCE_BULLET_PIERCED_WALL);
                event->s.eventParm = DirToByte(next.plane.normal);
                event->s.otherEntityNum2 = exitType;
                event->s.otherEntityNum = attacker->s.number;
            }
            TCE_BulletDeflect(start, end, fraction, seed, wallsRemaining);
            return Bullet_Fire_Extended(source, attacker, next.endpos, end, 0,
                (int)(energy / def->unknown_134 * damage), (int)energy, 0,
                totalDistance, wallsRemaining - 1, bodiesRemaining, passesRemaining,
                seed, suppressWallEvents);
        }
        return qfalse;
    }
damageExplosive:
    if (hit->s.eType == ET_EXPLOSIVE) {
        range = def->unknown_130 * bboxScale * 39.370079f;
        if (range <= 0.0f) range = 999999.0f;
        value = damage / (totalDistance / range + 1.0f);
        if (value < 0.0f) value = 0.0f;
        G_Damage(hit, attacker, attacker, forward, entry, (int)value, 0,
                 weaponDef[attacker->s.weapon].mod);
    }
    return qfalse;
}



/*
======================================================================

GRENADE LAUNCHER

  700 has been the standard direction multiplier in fire_grenade()

======================================================================
*/
extern void G_ExplodeMissilePoisonGas (gentity_t *ent);

gentity_t *weapon_gpg40_fire (gentity_t *ent, int grenType) {
	gentity_t	*m/*, *te*/; // JPW NERVE
	trace_t		tr;
	vec3_t		viewpos;
//	float		upangle = 0, pitch;			//	start with level throwing and adjust based on angle
	vec3_t		tosspos;
	//bani - to prevent nade-through-teamdoor sploit
	vec3_t	orig_viewpos;

	AngleVectors(ent->client->ps.viewangles, forward, NULL, NULL);

	VectorCopy(muzzleEffect, tosspos);

	// check for valid start spot (so you don't throw through or get stuck in a wall)
	VectorCopy( ent->s.pos.trBase, viewpos );
	viewpos[2] += ent->client->ps.viewheight;
	VectorCopy( viewpos, orig_viewpos );	//bani - to prevent nade-through-teamdoor sploit
	VectorMA( viewpos, 32, forward, viewpos);

	//bani - to prevent nade-through-teamdoor sploit
	trap_Trace( &tr, orig_viewpos, tv( -4.f, -4.f, 0.f ), tv( 4.f, 4.f, 6.f ), viewpos, ent->s.number, MASK_MISSILESHOT );
	if( !(tr.fraction >= 1.f) ) { // oops, bad launch spot ) {
		VectorCopy( tr.endpos, tosspos );
		SnapVectorTowards( tosspos, orig_viewpos );
	} else {
		trap_Trace (&tr, viewpos, tv(-4.f, -4.f, 0.f), tv(4.f, 4.f, 6.f), tosspos, ent->s.number, MASK_MISSILESHOT );
		if( !(tr.fraction >= 1.f) ) { // oops, bad launch spot
			VectorCopy(tr.endpos, tosspos);
			SnapVectorTowards( tosspos, viewpos );
		}
	}

	VectorScale(forward, 2000, forward);

	m = fire_grenade (ent, tosspos, forward, grenType);

	m->damage = 0;
	
	// Ridah, return the grenade so we can do some prediction before deciding if we really want to throw it or not
	return m;
}

gentity_t *weapon_mortar_fire( gentity_t *ent, int grenType ) {
	gentity_t	*m;
	trace_t		tr;
	vec3_t		launchPos, testPos;
	vec3_t		angles;

	VectorCopy( ent->client->ps.viewangles, angles );
	angles[PITCH] -= 60.f;
/*	if( angles[PITCH] < -89.f )
		angles[PITCH] = -89.f;*/
	AngleVectors( angles, forward, NULL, NULL );

	VectorCopy( muzzleEffect, launchPos );

	// check for valid start spot (so you don't throw through or get stuck in a wall)
	VectorMA( launchPos, 32, forward, testPos);

	// Gordon: hack so i can do inverse trajectory calcs easily :p
	if(G_IsSinglePlayerGame() && ent->r.svFlags & SVF_BOT) {
/*		forward[0] *= 3000;
		forward[1] *= 3000;
		forward[2] *= 3000;*/
		VectorCopy( ent->gDelta, forward );
	} else {
		forward[0] *= 3000*1.1f;
		forward[1] *= 3000*1.1f;
		forward[2] *= 1500*1.1f;
	}

	trap_Trace (&tr, testPos, tv(-4.f, -4.f, 0.f), tv(4.f, 4.f, 6.f), launchPos, ent->s.number, MASK_MISSILESHOT);

	if( tr.fraction < 1 ) {	// oops, bad launch spot
		VectorCopy( tr.endpos, launchPos );
		SnapVectorTowards( launchPos, testPos );
	}

	m = fire_grenade( ent, launchPos, forward, grenType );

	return m;
}

gentity_t *weapon_grenadelauncher_fire (gentity_t *ent, int grenType) {
	gentity_t	*m;
	trace_t		tr;
	vec3_t viewpos, tosspos;
	float speed;

	/* Original2009ea00: aim flag selects underhand versus overhand. */
	if(ent->client->ps.stats[STAT_TCE_WEAPON_FLAGS]&4) {
		forward[2]=(float)((double)forward[2]-.1);speed=300;
	} else {
		forward[2]=(float)((double)forward[2]+.1);speed=650;
	}
	VectorNormalizeFast(forward);
	VectorCopy(muzzleEffect,tosspos);
	if(grenType==15) {
		VectorCopy(ent->s.pos.trBase,tosspos);
		VectorSet(forward,0,0,-1);speed=1;
	}
	if(g_newbbox.integer)speed*=1.25f;
	VectorScale(forward,speed,forward);

	// check for valid start spot (so you don't throw through or get stuck in a wall)
	VectorCopy( ent->s.pos.trBase, viewpos );
	viewpos[2] += ent->client->ps.viewheight;

	if( grenType == WP_DYNAMITE || grenType == WP_SATCHEL )
		trap_Trace (&tr, viewpos, tv(-12.f, -12.f, 0.f), tv(12.f, 12.f, 20.f), tosspos, ent->s.number, MASK_MISSILESHOT);
	else if( grenType == WP_LANDMINE )
		trap_Trace (&tr, viewpos, tv(-16.f, -16.f, 0.f), tv(16.f, 16.f, 16.f), tosspos, ent->s.number, MASK_MISSILESHOT);
	else
		trap_Trace (&tr, viewpos, tv(-4.f, -4.f, 0.f), tv(4.f, 4.f, 6.f), tosspos, ent->s.number, MASK_MISSILESHOT);

	if( tr.startsolid ) {
		// Arnout: this code is a bit more solid than the previous code
		VectorCopy( forward, viewpos );
		VectorNormalizeFast( viewpos );
		VectorMA( ent->r.currentOrigin, -24.f, viewpos, viewpos ); 

		if( grenType == WP_DYNAMITE || grenType == WP_SATCHEL )
			trap_Trace (&tr, viewpos, tv(-12.f, -12.f, 0.f), tv(12.f, 12.f, 20.f), tosspos, ent->s.number, MASK_MISSILESHOT);
		else if( grenType == WP_LANDMINE )
			trap_Trace (&tr, viewpos, tv(-16.f, -16.f, 0.f), tv(16.f, 16.f, 16.f), tosspos, ent->s.number, MASK_MISSILESHOT);
		else
			trap_Trace (&tr, viewpos, tv(-4.f, -4.f, 0.f), tv(4.f, 4.f, 6.f), tosspos, ent->s.number, MASK_MISSILESHOT);

		VectorCopy( tr.endpos, tosspos );
	} else if( tr.fraction < 1 ) {	// oops, bad launch spot
		VectorCopy( tr.endpos, tosspos );
		SnapVectorTowards( tosspos, viewpos );
	}

	m = fire_grenade (ent, tosspos, forward, grenType);

	m->damage = 0;	// Ridah, grenade's don't explode on contact
	
	if (grenType == WP_LANDMINE) {
		if (ent->client->sess.sessionTeam == TEAM_AXIS) // store team so we can generate red or blue smoke
			m->s.otherEntityNum2 = 1;
		else
			m->s.otherEntityNum2 = 0;
	}

	// JPW NERVE
	if (grenType == WP_SMOKE_MARKER) {
		m->s.teamNum = ent->client->sess.sessionTeam;	// store team so we can generate red or blue smoke
		if( ent->client->sess.skill[SK_SIGNALS] >= 3 ) {
			m->count = 2;
			m->nextthink = level.time + 3500;
			m->think = weapon_checkAirStrikeThink2;
		} else {
			m->count = 1;
			m->nextthink = level.time + 2500;
			m->think = weapon_checkAirStrikeThink1;
		}
	}
	// jpw

	//----(SA)	adjust for movement of character.  TODO: Probably comment in later, but only for forward/back not strafing
	//VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics

	// Ridah, return the grenade so we can do some prediction before deciding if we really want to throw it or not
	return m;
}

/*
======================================================================

ROCKET

======================================================================
*/

void Weapon_Panzerfaust_Fire( gentity_t *ent ) {
	gentity_t	*m;

	m = fire_rocket (ent, muzzleEffect, forward);

//	VectorAdd( m->s.pos.trDelta, ent->client->ps.velocity, m->s.pos.trDelta );	// "real" physics
}


/*
======================================================================

SPEARGUN

======================================================================
*/
/*void Weapon_Speargun_Fire (gentity_t *ent) {
	gentity_t	*m;

	m = fire_speargun (ent, muzzleEffect, forward);
}*/


/*
======================================================================

LIGHTNING GUN

======================================================================
*/

void G_BurnMeGood( gentity_t *self, gentity_t *body )
{
	// add the new damage
	body->flameQuota += 5;
	body->flameQuotaTime = level.time;
	
	// JPW NERVE -- yet another flamethrower damage model, trying to find a feels-good damage combo that isn't overpowered
	if (body->lastBurnedFrameNumber != level.framenum) {
		G_Damage( body, self->parent, self->parent, vec3_origin, self->r.currentOrigin, 5, 0, MOD_FLAMETHROWER ); // was 2 dmg in release ver, hit avg. 2.5 times per frame
		body->lastBurnedFrameNumber = level.framenum;
	}
	// jpw
	
	// make em burn
	if( body->client && (body->health <= 0 || body->flameQuota > 0) ) { // JPW NERVE was > FLAME_THRESHOLD
		if (body->s.onFireEnd < level.time)
			body->s.onFireStart = level.time;

		body->s.onFireEnd = level.time + FIRE_FLASH_TIME;
		body->flameBurnEnt = self->r.ownerNum;
		// add to playerState for client-side effect
		body->client->ps.onFireStart = level.time;
	}
}

// TTimo - for traces calls
static vec3_t	flameChunkMins = {-4, -4, -4};
static vec3_t	flameChunkMaxs = { 4,  4,  4};

void Weapon_FlamethrowerFire( gentity_t *ent ) {
	gentity_t	*traceEnt;
	vec3_t		start;
	vec3_t		trace_start;
	vec3_t		trace_end;
	trace_t 	trace;

	VectorCopy( ent->r.currentOrigin, start );
	start[2] += ent->client->ps.viewheight;
	VectorCopy( start, trace_start );

	VectorMA( start, -8, forward, start );
	VectorMA( start, 10, right, start );
	VectorMA( start, -6, up, start );
	
	// prevent flame thrower cheat, run & fire while aiming at the ground, don't get hurt
	// 72 total box height, 18 xy -> 77 trace radius (from view point towards the ground) is enough to cover the area around the feet
	VectorMA( trace_start, 77.0, forward, trace_end);
	trap_Trace( &trace, trace_start, flameChunkMins, flameChunkMaxs, trace_end, ent->s.number, MASK_SHOT | MASK_WATER );	
	if (trace.fraction != 1.0)
	{
		// additional checks to filter out false positives
		if (trace.endpos[2] > (ent->r.currentOrigin[2]+ent->r.mins[2]-8) && trace.endpos[2] < ent->r.currentOrigin[2]) 
		{
			// trigger in a 21 radius around origin
			trace_start[0] -= trace.endpos[0];
			trace_start[1] -= trace.endpos[1];
			if (trace_start[0]*trace_start[0]+trace_start[1]*trace_start[1] < 441)
			{
				// set self in flames
				G_BurnMeGood( ent, ent );
			}
		}
	}

	traceEnt = fire_flamechunk ( ent, start, forward );
}

//======================================================================


/*
==============
AddLean
	add leaning offset
==============
*/
void AddLean(gentity_t *ent, vec3_t point)
{
    /* Original Windows2009f400: asymmetric TC lean affects shot origin. */
    if(ent->client && ent->client->ps.leanf) {
        vec3_t angles, right;
        float lean=ent->client->ps.leanf;
        if(g_leanmode.integer>0) {
            float divisor=lean<0 ? 3.3f : 1.8f;
            VectorCopy(ent->client->ps.viewangles,angles);
            angles[ROLL]=(float)((double)lean/divisor+angles[ROLL]);
            AngleVectors(angles,NULL,right,NULL);
            point[0]=(float)((double)lean/divisor*right[0]+point[0]);
            point[1]=(float)((double)lean/divisor*right[1]+point[1]);
            point[2]=(float)((double)lean/divisor*right[2]+point[2]);
        } else {
            AngleVectors(ent->client->ps.viewangles,NULL,right,NULL);
            point[0]=(float)((double)lean*right[0]+point[0]);
            point[1]=(float)((double)lean*right[1]+point[1]);
            point[2]=(float)((double)lean*right[2]+point[2]);
        }
    }
}

/*
===============
AccuracyHit
===============
*/
qboolean AccuracyHit( gentity_t *target, gentity_t *attacker ) {
	if( !target->takedamage ) {
		return qfalse;
	}

	if( !attacker ) {
		return qfalse;
	}

	if ( target == attacker ) {
		return qfalse;
	}

	if( !target->client ) {
		return qfalse;
	}

	if( !attacker->client ) {
		return qfalse;
	}

	if( target->client->ps.stats[STAT_HEALTH] <= 0 ) {
		return qfalse;
	}

	if ( OnSameTeam( target, attacker ) ) {
		return qfalse;
	}

	return qtrue;
}


/*
===============
CalcMuzzlePoint

set muzzle location relative to pivoting eye
===============
*/
/* TC Windows2009f5d0 / Linux001114ae: eye-relative effect origin,
 * including lean. TC does not select offsets from SDK weapon IDs. */
void CalcMuzzlePoint(gentity_t *ent, int weapon, vec3_t forward,
                    vec3_t right, vec3_t up, vec3_t muzzlePoint) {
    VectorCopy(ent->r.currentOrigin, muzzlePoint);
    muzzlePoint[2] += ent->client->ps.viewheight;
    AddLean(ent, muzzlePoint);
    muzzlePoint[2] -= 8.0f;
    VectorMA(muzzlePoint, 4.0f, forward, muzzlePoint);
    VectorMA(muzzlePoint, 8.0f, up, muzzlePoint);
    SnapVector(muzzlePoint);
}

/* TC Windows2009f6c0 / Linux001115c2: use unsnapped authoritative
 * player origin. Only the effect origin above is integer snapped. */
void CalcMuzzlePointForActivate(gentity_t *ent, vec3_t forward,
                    vec3_t right, vec3_t up, vec3_t muzzlePoint) {
    VectorCopy(ent->client->ps.origin, muzzlePoint);
    muzzlePoint[2] += ent->client->ps.viewheight;
    AddLean(ent, muzzlePoint);
    muzzlePoint[2] -= 8.0f;
    VectorMA(muzzlePoint, 4.0f, forward, muzzlePoint);
    VectorMA(muzzlePoint, 8.0f, up, muzzlePoint);
}

/* TC Windows2009f780 / Linux00111682. Aim/recoil already belongs to the
 * PM_Weapon snapshot; the SDK's extra sinusoidal sniper sway is absent. */
void CalcMuzzlePoints(gentity_t *ent, int weapon) {
    vec3_t viewang;
    VectorCopy(ent->client->ps.viewangles, viewang);
    AngleVectors(viewang, forward, right, up);
    CalcMuzzlePointForActivate(ent, forward, right, up, muzzleTrace);
    CalcMuzzlePoint(ent, weapon, forward, right, up, muzzleEffect);
}

qboolean G_PlayerCanBeSeenByOthers( gentity_t *ent ) {
	int			i;
	gentity_t	*ent2;
	vec3_t		pos[3];

	VectorCopy( ent->client->ps.origin, pos[0] );
	pos[0][2] += ent->client->ps.mins[2];
	VectorCopy( ent->client->ps.origin, pos[1] );
	VectorCopy( ent->client->ps.origin, pos[2] );
	pos[2][2] += ent->client->ps.maxs[2];

	for( i = 0, ent2 = g_entities; i < level.maxclients; i++, ent2++ ) {
		if( !ent2->inuse || ent2 == ent ) {
			continue;
		}

		if( ent2->client->sess.sessionTeam == TEAM_SPECTATOR )
			continue;

		if( ent2->health <= 0 ||
			ent2->client->sess.sessionTeam == ent->client->sess.sessionTeam ) {
			continue;
		}

		if( ent2->client->ps.eFlags & EF_ZOOMING ) {
			G_SetupFrustum_ForBinoculars( ent2 );
		} else {
			G_SetupFrustum( ent2 );
		}

		if( G_VisibleFromBinoculars( ent2, ent, pos[0] ) ||
			G_VisibleFromBinoculars( ent2, ent, pos[1] ) ||
			G_VisibleFromBinoculars( ent2, ent, pos[2] ) ) {
			return qtrue;
		}
	}

	return qfalse;
}

/*
===============
FireWeapon
===============
*/
/* Windows 2009ee90. TC emits exactly nine pellets; pelletCount selects
 * this path but is not the loop bound. Client event161 renders wall impacts. */
void ShotgunPattern(vec3_t origin, vec3_t direction, int seed, gentity_t *ent) {
    vec3_t axis, side, vertical, end;
    int i;
    int damage = (int)(weaponDef[ent->client->ps.weapon].unknown_12c * (double)(1.0f / 9.0f));
    VectorNormalize2(direction, axis);
    PerpendicularVector(side, axis);
    CrossProduct(axis, side, vertical);
    if (g_antilag.integer && !(ent->r.svFlags & SVF_BOT)) G_TimeShiftAllClients(ent, qtrue);
    for (i = 0; i < 9; ++i) {
        float radius = (float)sqrt(Q_random(&seed));
        double angle = Q_crandom(&seed) * 3.141;
        float horizontal = (float)(cos(angle) * radius * 115.0);
        double elevation = sin(angle) * radius * 115.0;
        end[0] = (float)((double)vertical[0] * elevation +
            (double)side[0] * horizontal + (double)axis[0] * 8192.0 + origin[0]);
        end[1] = (float)((double)vertical[1] * elevation +
            (float)(side[1] * horizontal + axis[1] * 8192.0f + origin[1]));
        end[2] = (float)((double)vertical[2] * elevation +
            (float)(side[2] * horizontal + axis[2] * 8192.0f + origin[2]));
        Bullet_Fire_Extended(ent, ent, muzzleTrace, end, 0, damage,
            weaponDef[ent->client->ps.weapon].unknown_134, 0, 0, 1, 1, 8, seed, 1);
    }
    if (g_antilag.integer && !(ent->r.svFlags & SVF_BOT)) G_UnTimeShiftAllClients(ent);
}

/* Windows 2009f070. The event transports the snapped direction and low seed
 * byte, allowing the client to reproduce the same nine-pellet pattern. */
void Weapon_Shotgun_Fire(gentity_t *ent) {
    gentity_t *event = G_TempEntity(muzzleTrace, EV_TCE_SHOTGUN);
    vec3_t end;
    Bullet_Endpos(ent, 0, &end);
    VectorSubtract(end, muzzleTrace, event->s.origin2);
    SnapVector(event->s.origin2);
    event->s.eventParm = ent->client->ps.stats[STAT_TCE_SHOT_SEED] & 255;
    event->s.otherEntityNum = ent->s.number;
    ShotgunPattern(event->s.pos.trBase, event->s.origin2, event->s.eventParm, ent);
}

/* Windows 2009f950, Linux FireWeapon00111894. Numeric IDs here belong to
 * TC's wire protocol; SDK WP_GPG40 is TC's Glock (39), for example. */
void Weapon_Shotgun_Fire(gentity_t *ent);
void FireWeapon(gentity_t *ent) {
    float aimSpreadScale;
    int weapon = ent->s.weapon;
    int charge;
    gclient_t *client = ent->client;
    if (client->ps.pm_type == PM_DEAD) return;
    if (client->ps.persistant[PERS_HWEAPON_USE] && ent->active) return;
    /* Pmove has already consumed ammunition/played its predicted fire event.
     * Cancellation here suppresses only authoritative weapon effects. */
    if (TCE_LuaWeaponFire(ent->s.number, weapon)) return;
    CalcMuzzlePoints(ent, weapon);
    aimSpreadScale = g_userAim.integer ? client->currentAimSpreadScale + 0.15f : 1.0f;
    if (aimSpreadScale > 1) aimSpreadScale = 1;
    if ((client->ps.eFlags & EF_ZOOMING) && (client->ps.stats[STAT_KEYS] & (1 << INV_BINOCS)) &&
        client->sess.playerType == PC_FIELDOPS) {
        if (!client->ps.leanf) Weapon_Artillery(ent);
        return;
    }
    if (client->ps.groundEntityNum == ENTITYNUM_NONE) aimSpreadScale = 2;
    if (client->ps.powerups[PW_OPS_DISGUISED] && weapon != 30 && weapon != 27 && weapon != 28) {
        switch (weapon) {
        case 1: case 10: case 14: case 52: case 53: case 54: case 32:
        case 58: case 25: case 4: case 9: case 57:
            if (G_PlayerCanBeSeenByOthers(ent)) client->ps.powerups[PW_OPS_DISGUISED] = 0;
            break;
        default: client->ps.powerups[PW_OPS_DISGUISED] = 0; break;
        }
    }
    switch (weapon) {
    case 1: Weapon_Knife(ent); break;
    case 2: case 3: case 5: case 6: case 7: case 8: case 10: case 13: case 14:
    case 23: case 24: case 25: case 32: case 33: case 37: case 38: case 39:
    case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
    case 48: case 49: case 50: case 51: case 52: case 53: case 54:
        if (weaponDef[weapon].pelletCount) Weapon_Shotgun_Fire(ent);
        else Bullet_Fire(ent, G_GetWeaponSpread(2) * aimSpreadScale, weaponDef[weapon].unknown_12c, qtrue);
        if (weaponDef[ent->s.weapon].unknown_1a8) {
            client->ps.stats[STAT_TCE_WEAPON_FLAGS] &= ~4;
            if (!(client->ps.persistant[14] & 8)) client->ps.stats[STAT_TCE_WEAPON_FLAGS] &= ~8;
        }
        break;
    case 11: Weapon_Syringe(ent); break;
    case 12: Weapon_MagicAmmo(ent); break;
    case 19: Weapon_Medic(ent); break;
    case 21: Weapon_Engineer(ent); break;
    case 22:
        charge = level.lieutenantChargeTime[client->sess.sessionTeam - 1];
        if (level.time - client->ps.classWeaponTime > charge) client->ps.classWeaponTime = level.time - charge;
        if (client->sess.skill[SK_SIGNALS] >= 2) client->ps.classWeaponTime += .66f * charge;
        else client->ps.classWeaponTime = level.time;
        weapon_grenadelauncher_fire(ent, weapon);
        break;
    case 28:
        if (G_ExplodeSatchels(ent)) {
            client->ps.ammo[28] = 0;
            client->ps.ammoclip[28] = 0;
            client->ps.ammoclip[27] = 1;
            G_AddEvent(ent, EV_NOAMMO, 0);
        }
        break;
    case 29: G_PlaceTripmine(ent); break;
    case 31:
        Bullet_Fire(ent, G_GetWeaponSpread(31) * aimSpreadScale *
            ((client->ps.pm_flags & PMF_DUCKED || client->ps.eFlags & EF_PRONE) ? .6f : 1.f),
            G_GetWeaponDamage(31), qfalse);
        break;
    case 55: case 56:
        charge = level.engineerChargeTime[client->sess.sessionTeam - 1];
        if (level.time - client->ps.classWeaponTime > charge) client->ps.classWeaponTime = level.time - charge;
        client->ps.classWeaponTime += .5f * charge;
        weapon_gpg40_fire(ent, weapon);
        break;
    case 57: case 58: case 59:
        Bullet_Fire(ent, G_GetWeaponSpread(weapon) * aimSpreadScale, G_GetWeaponDamage(weapon), qfalse);
        break;
    case 60:
        charge = level.soldierChargeTime[client->sess.sessionTeam - 1];
        if (level.time - client->ps.classWeaponTime > charge) client->ps.classWeaponTime = level.time - charge;
        client->ps.classWeaponTime += (client->sess.skill[SK_HEAVY_WEAPONS] >= 1 ? .35f : .5f) * charge;
        weapon_mortar_fire(ent, weapon);
        break;
    case 61:
        client->ps.classWeaponTime = level.time;
        Weapon_AdrenalineSyringe(ent);
        break;
    case 62:
        Bullet_Fire(ent, G_GetWeaponSpread(31) * aimSpreadScale * .05f, G_GetWeaponDamage(31), qfalse);
        break;
    case 4: case 9: case 15: case 26: case 27: case 30:
        if (weapon == 27) {
            charge = level.covertopsChargeTime[client->sess.sessionTeam - 1];
            if (level.time - client->ps.classWeaponTime > charge) client->ps.classWeaponTime = level.time - charge;
            if (client->sess.skill[SK_MILITARY_INTELLIGENCE_AND_SCOPED_WEAPONS] >= 2)
                client->ps.classWeaponTime += .66f * charge;
            else client->ps.classWeaponTime = level.time;
        }
        if (weapon == 26) {
            charge = level.engineerChargeTime[client->sess.sessionTeam - 1];
            if (level.time - client->ps.classWeaponTime > charge) client->ps.classWeaponTime = level.time - charge;
            client->ps.classWeaponTime += (client->sess.skill[SK_EXPLOSIVES_AND_CONSTRUCTION] >= 3 ? .33f : .5f) * charge;
        }
        if (weapon == 15) {
            charge = level.engineerChargeTime[client->sess.sessionTeam - 1];
            if (level.time - client->ps.classWeaponTime > charge) client->ps.classWeaponTime = level.time - charge;
            if (client->sess.skill[SK_EXPLOSIVES_AND_CONSTRUCTION] >= 3) client->ps.classWeaponTime += .66f * charge;
            else client->ps.classWeaponTime = level.time;
        }
        weapon_grenadelauncher_fire(ent, weapon);
        break;
    case 65:
        charge = level.soldierChargeTime[client->sess.sessionTeam - 1];
        if (level.time - client->ps.classWeaponTime > charge) client->ps.classWeaponTime = level.time - charge;
        if (client->sess.skill[SK_HEAVY_WEAPONS] >= 1) client->ps.classWeaponTime += .66f * charge;
        else client->ps.classWeaponTime = level.time;
        Weapon_Panzerfaust_Fire(ent);
        if (ent->client) {
            vec3_t direction;
            AngleVectors(ent->client->ps.viewangles, direction, NULL, NULL);
            VectorMA(ent->client->ps.velocity, -64, direction, ent->client->ps.velocity);
        }
        break;
    case 66: Weapon_FlamethrowerFire(ent); break;
    default: break;
    }
    if (g_gamestate.integer == GS_PLAYING)
        ent->client->sess.aWeaponStats[BG_WeapStatForWeapon(ent->s.weapon)].atts++;
#ifdef FEATURE_OMNIBOT
    /* TC weapon IDs must pass through the bridge, never the ET SDK enum. */
    Bot_Event_FireWeapon(ent->s.number, Bot_WeaponGameToBot(weapon), NULL);
#endif
}


//
// IsSilencedWeapon
//
// Description: Is the specified weapon a silenced weapon?
// Written: 12/26/2002
//
qboolean IsSilencedWeapon
(
	// The type of weapon in question.  Is it silenced?
	int weaponType
)
{
	// Local Variables ////////////////////////////////////////////////////////
	///////////////////////////////////////////////////////////////////////////

	// Return true for any of the silenced types
	switch (weaponType)
	{
	case WP_SILENCED_COLT:
	case WP_STEN:
	case WP_SILENCER:
		return qtrue;
	};

	// Otherwise, not silenced
	return qfalse;
}
//
// IsSilencedWeapon
//


