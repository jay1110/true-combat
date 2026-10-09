#include "g_local.h"
#include "tce_lua.h"
#ifdef FEATURE_OMNIBOT
#include "g_etbot_interface.h"
#endif
#include "tce_bg.h"
#include "../ui/menudef.h"

// g_client.c -- client functions that don't happen every frame

// Ridah, new bounding box
//static vec3_t	playerMins = {-15, -15, -24};
//static vec3_t	playerMaxs = {15, 15, 32};
vec3_t	playerMins = {-18, -18, -24};
vec3_t	playerMaxs = {18, 18, 48};
// done.

/*QUAKED info_player_deathmatch (1 0 1) (-18 -18 -24) (18 18 48)
potential spawning position for deathmatch games.
Targets will be fired when someone spawns in on them.
"nobots" will prevent bots from using this spot.
"nohumans" will prevent non-bots from using this spot.
If the start position is targeting an entity, the players camera will start out facing that ent (like an info_notnull)
*/
void SP_info_player_deathmatch( gentity_t *ent ) {
	int		i;
	vec3_t	dir;

	G_SpawnInt( "nobots", "0", &i);
	if ( i ) {
		ent->flags |= FL_NO_BOTS;
	}
	G_SpawnInt( "nohumans", "0", &i );
	if ( i ) {
		ent->flags |= FL_NO_HUMANS;
	}

	ent->enemy = G_PickTarget( ent->target );
	if(ent->enemy)
	{
		VectorSubtract( ent->enemy->s.origin, ent->s.origin, dir );
		vectoangles( dir, ent->s.angles );
	}

}

//----(SA) added
/*QUAKED info_player_checkpoint (1 0 0) (-16 -16 -24) (16 16 32) a b c d
these are start points /after/ the level start
the letter (a b c d) designates the checkpoint that needs to be complete in order to use this start position
*/
void SP_info_player_checkpoint(gentity_t *ent) {
	ent->classname = "info_player_checkpoint";
	SP_info_player_deathmatch( ent );
}

//----(SA) end


/*QUAKED info_player_start (1 0 0) (-18 -18 -24) (18 18 48)
equivelant to info_player_deathmatch
*/
void SP_info_player_start(gentity_t *ent) {
	ent->classname = "info_player_deathmatch";
	SP_info_player_deathmatch( ent );
}

/*QUAKED info_player_intermission (1 0 1) (-16 -16 -24) (16 16 32) AXIS ALLIED
The intermission will be viewed from this point.  Target an info_notnull for the view direction.
*/
void SP_info_player_intermission( gentity_t *ent ) {

}

extern void BotSpeedBonus(int clientNum);


/*
=======================================================================

  SelectSpawnPoint

=======================================================================
*/

/*
================
SpotWouldTelefrag

================
*/
qboolean SpotWouldTelefrag( gentity_t *spot ) {
	int			i, num;
	int			touch[MAX_GENTITIES];
	gentity_t	*hit;
	vec3_t		mins, maxs;
	/* TC spawn-overlap hull, PE200bd794/200bd7a0. */
	static const vec3_t spawnMins = {-14, -14, -24};
	static const vec3_t spawnMaxs = {14, 14, 46};

	VectorAdd( spot->r.currentOrigin, spawnMins, mins );
	VectorAdd( spot->r.currentOrigin, spawnMaxs, maxs );
	num = trap_EntitiesInBox( mins, maxs, touch, MAX_GENTITIES );

	for (i=0 ; i<num ; i++) {
		hit = &g_entities[touch[i]];
		if ( hit->client && hit->client->ps.stats[STAT_HEALTH] > 0 ) {
			return qtrue;
		}

	}

	return qfalse;
}

/*
================
SelectNearestDeathmatchSpawnPoint

Find the spot that we DON'T want to use
================
*/
#define	MAX_SPAWN_POINTS	128
gentity_t *SelectNearestDeathmatchSpawnPoint( vec3_t from ) {
	gentity_t	*spot;
	vec3_t		delta;
	float		dist, nearestDist;
	gentity_t	*nearestSpot;

	nearestDist = 999999;
	nearestSpot = NULL;
	spot = NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL) {

		VectorSubtract( spot->r.currentOrigin, from, delta );
#if defined(_MSC_VER) && defined(_M_IX86)
		/* TC2004abe8 compares retained ST0 before the winning float store. */
		{
			int nearer;
			__asm {
				lea eax, delta
				push eax
				call VectorLength
				add esp, 4
				fcom nearestDist
				fnstsw ax
				test ah, 1
				setnz al
				movzx eax, al
				mov nearer, eax
				test eax, eax
				jz nearestDiscardLength
				fstp dist
				jmp nearestLengthDone
			nearestDiscardLength:
				fstp st(0)
			nearestLengthDone:
			}
			if (nearer) {
				nearestDist = dist;
				nearestSpot = spot;
			}
		}
#else
		dist = VectorLength( delta );
		if ( dist < nearestDist ) {
			nearestDist = dist;
			nearestSpot = spot;
		}
#endif
	}

	return nearestSpot;
}


/*
================
SelectRandomDeathmatchSpawnPoint

go to a random point that doesn't telefrag
================
*/
#define	MAX_SPAWN_POINTS	128
gentity_t *SelectRandomDeathmatchSpawnPoint( void ) {
	gentity_t	*spot;
	int			count;
	int			selection;
	gentity_t	*spots[MAX_SPAWN_POINTS];

	count = 0;
	spot = NULL;

	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
		if ( SpotWouldTelefrag( spot ) ) {
			continue;
		}
		spots[ count ] = spot;
		count++;
	}

	if ( !count ) {	// no spots that won't telefrag
		return G_Find( NULL, FOFS(classname), "info_player_deathmatch");
	}

	selection = rand() % count;
	return spots[ selection ];
}


/*
===========
SelectSpawnPoint

Chooses a player start, deathmatch start, etc
============
*/
gentity_t *SelectSpawnPoint ( vec3_t avoidPoint, vec3_t origin, vec3_t angles ) {
	gentity_t	*spot;
	gentity_t	*nearestSpot;

	nearestSpot = SelectNearestDeathmatchSpawnPoint( avoidPoint );

	spot = SelectRandomDeathmatchSpawnPoint ( );
	if ( spot == nearestSpot ) {
		// roll again if it would be real close to point of death
		spot = SelectRandomDeathmatchSpawnPoint ( );
		if ( spot == nearestSpot ) { 
			// last try
			spot = SelectRandomDeathmatchSpawnPoint ( );
		}		
	}

	// find a single player start spot
	if (!spot) {
		G_Error( "Couldn't find a spawn point" );
	}

	VectorCopy (spot->r.currentOrigin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return spot;
}

/*
===========
SelectInitialSpawnPoint

Try to find a spawn point marked 'initial', otherwise
use normal spawn selection.
============
*/
/*gentity_t *SelectInitialSpawnPoint( vec3_t origin, vec3_t angles ) {
	gentity_t	*spot;

	spot = NULL;
	while ((spot = G_Find (spot, FOFS(classname), "info_player_deathmatch")) != NULL) {
		if ( spot->spawnflags & 1 ) {
			break;
		}
	}

	if ( !spot || SpotWouldTelefrag( spot ) ) {
		return SelectSpawnPoint( vec3_origin, origin, angles );
	}

	VectorCopy (spot->r.currentOrigin, origin);
	origin[2] += 9;
	VectorCopy (spot->s.angles, angles);

	return spot;
}*/

/*
===========
SelectSpectatorSpawnPoint

============
*/
gentity_t *SelectSpectatorSpawnPoint( vec3_t origin, vec3_t angles ) {
	FindIntermissionPoint();

	VectorCopy( level.intermission_origin, origin );
	VectorCopy( level.intermission_angle, angles );

	return NULL;
}

/*
=======================================================================

BODYQUE

=======================================================================
*/

/*
===============
InitBodyQue
===============
*/
void InitBodyQue (void) {
	int		i;
	gentity_t	*ent;

	level.bodyQueIndex = 0;
	for (i=0; i<BODY_QUEUE_SIZE ; i++) {
		ent = G_Spawn();
		ent->classname = "bodyque";
		ent->neverFree = qtrue;
		level.bodyQue[i] = ent;
	}
}

/*
=============
BodyUnlink
  
Called by BodySink
=============
*/
void BodyUnlink( gentity_t *ent ) {
	trap_UnlinkEntity( ent );
	ent->physicsObject = qfalse;
}
                
/*
=============
BodySink

After sitting around for five seconds, fall into the ground and dissapear 
=============
*/ 
void BodySink2( gentity_t *ent ) {
	ent->physicsObject = qfalse;
    ent->nextthink = level.time + BODY_TIME(BODY_TEAM(ent))+1500;
    ent->think = BodyUnlink;
    ent->s.pos.trType = TR_LINEAR;
    ent->s.pos.trTime = level.time;
    VectorCopy( ent->r.currentOrigin, ent->s.pos.trBase );
	VectorSet( ent->s.pos.trDelta, 0, 0, -0.8f ); /* TC BodySink2: 0xbf4ccccd. */
}

/*
=============
BodySink

After sitting around for five seconds, fall into the ground and dissapear
=============
*/
void BodySink( gentity_t *ent ) {
	if( ent->activator ) {
		// see if parent is still disguised
		if( ent->activator->client->ps.powerups[PW_OPS_DISGUISED] ) {
			ent->nextthink = level.time + 100;
			return;
		} else {
			ent->activator = NULL;
		}
	}

	BodySink2( ent );
}


/*
=============
CopyToBodyQue

A player is respawning, so make an entity that looks
just like the existing corpse to leave behind.
=============
*/
void CopyToBodyQue( gentity_t *ent ) {
	gentity_t		*body;
	int			contents, i;

	trap_UnlinkEntity (ent);

	// if client is in a nodrop area, don't leave the body
  	contents = trap_PointContents( ent->client->ps.origin, -1 );
	if ( contents & CONTENTS_NODROP ) {
		return;
	}

	// grab a body que and cycle to the next one
	body = level.bodyQue[ level.bodyQueIndex ];
	level.bodyQueIndex = (level.bodyQueIndex + 1) % BODY_QUEUE_SIZE;

	// Gordon: um, what on earth was this here for?
//	trap_UnlinkEntity (body);

	body->s = ent->s;
	body->s.eFlags = EF_DEAD;		// clear EF_TALK, etc

	if( ent->client->ps.eFlags & EF_HEADSHOT ) {
		body->s.eFlags |= EF_HEADSHOT;			// make sure the dead body draws no head (if killed that way)
	}

	body->s.eType = ET_CORPSE;
	body->classname = "corpse";
	body->s.powerups = 0;	// clear powerups
	body->s.loopSound = 0;	// clear lava burning
	body->s.number = body - g_entities;
	body->timestamp = level.time;
	body->physicsObject = qtrue;
	body->physicsBounce = 0;		// don't bounce
	if ( body->s.groundEntityNum == ENTITYNUM_NONE ) {
		body->s.pos.trType = TR_GRAVITY;
		body->s.pos.trTime = level.time;
		VectorCopy( ent->client->ps.velocity, body->s.pos.trDelta );
	} else {
		body->s.pos.trType = TR_STATIONARY;
	}
	body->s.event = 0;

	// DHM - Clear out event system
	for( i=0; i<MAX_EVENTS; i++ )
		body->s.events[i] = 0;
	body->s.eventSequence = 0;

	// DHM - Nerve
	// change the animation to the last-frame only, so the sequence
	// doesn't repeat anew for the body
	switch ( body->s.legsAnim & ~ANIM_TOGGLEBIT ) 
	{
	case 2:
	case 194:
		body->s.torsoAnim = body->s.legsAnim = 194;
		break;
	default:
		body->s.torsoAnim = body->s.legsAnim = 1;
		break;
	case 5:
	case 6:
		body->s.torsoAnim = body->s.legsAnim = 6;
		break;
	case 192:
	case 197:
		body->s.torsoAnim = body->s.legsAnim = 197;
		break;
	case 195:
	case 196:
		body->s.torsoAnim = body->s.legsAnim = 196;
		break;
	}

	body->r.svFlags = ent->r.svFlags & ~SVF_BOT;
	VectorCopy (ent->r.mins, body->r.mins);
	VectorCopy (ent->r.maxs, body->r.maxs);
	VectorCopy (ent->r.absmin, body->r.absmin);
	VectorCopy (ent->r.absmax, body->r.absmax);
	
	// ydnar: bodies have lower bounding box
	body->r.maxs[ 2 ] = 0;

	body->clipmask = CONTENTS_SOLID | CONTENTS_PLAYERCLIP;
	// DHM - Nerve :: allow bullets to pass through bbox
	// Gordon: need something to allow the hint for covert ops
	body->r.contents = 0; /* TC corpses do not retain the SDK corpse collision contents. */
	body->r.ownerNum = ent->r.ownerNum;

	BODY_TEAM(body) =		ent->client->sess.sessionTeam;
	BODY_CLASS(body) =		ent->client->sess.playerType;
	BODY_CHARACTER(body) =	ent->client->pers.characterIndex;
	BODY_VALUE(body) =		0;

	body->s.time2 =			0;

	body->activator = NULL;

	body->nextthink = level.time + BODY_TIME(ent->client->sess.sessionTeam);
	if( g_maxlives.integer == 1 || g_gametype.integer == 5 ) {
		/* Preserve the original 32-bit wrapping timer addition. */
		body->nextthink = (int)((unsigned int)level.time + 0x7fffffffu);
	}

	body->think = BodySink;

	body->die = body_die;

	// don't take more damage if already gibbed
	if ( ent->health <= GIB_HEALTH ) {
		body->takedamage = qfalse;
	} else {
		body->takedamage = qtrue;
	}


	VectorCopy ( body->s.pos.trBase, body->r.currentOrigin );
	trap_LinkEntity (body);
}

//======================================================================


/*
==================
SetClientViewAngle

==================
*/
void SetClientViewAngle( gentity_t *ent, vec3_t angle ) {
	int			i;

	// set the delta angle
	for (i=0 ; i<3 ; i++) {
		int		cmdAngle;

		/* TC 2004b32f: rounded f32 scale, retained product into __ftol.
		 * A double holds the exact product of these two binary32 values. */
		cmdAngle = (int)((long long)((double)angle[i] * (double)182.0444488525390625f) & 65535);
		ent->client->ps.delta_angles[i] = cmdAngle - ent->client->pers.cmd.angles[i];
	}
	VectorCopy( angle, ent->s.angles );
	VectorCopy (ent->s.angles, ent->client->ps.viewangles);
}

void SetClientViewAnglePitch( gentity_t *ent, vec_t angle ) {
	int	cmdAngle;

	/* TC 2004b3b4 uses the same f32 scale and truncating 64-bit __ftol. */
	cmdAngle = (int)((long long)((double)angle * (double)182.0444488525390625f) & 65535);
	ent->client->ps.delta_angles[PITCH] = cmdAngle - ent->client->pers.cmd.angles[PITCH];

	ent->s.angles[ PITCH ] = 0;
	VectorCopy( ent->s.angles, ent->client->ps.viewangles);
}

/* JPW NERVE
================
limbo
================
*/
/* TC Windows 2004b410; Linux 000a23f4. */
void limbo( gentity_t *ent, qboolean makeCorpse ) {
    int i;
    int startclient = ent->client->ps.clientNum;
    gclient_t *cl;
    if ((ent->r.svFlags & SVF_POW) || (ent->client->ps.pm_flags & PMF_LIMBO)) return;
    if (ent->client->ps.persistant[PERS_RESPAWNS_LEFT] == 0)
        ent->client->ps.persistant[PERS_RESPAWNS_PENALTY] =
            g_maxlivesRespawnPenalty.integer ? g_maxlivesRespawnPenalty.integer : -1;
    for (i = 0; i < MAX_PERSISTANT; i++)
        ent->client->saved_persistant[i] = ent->client->ps.persistant[i];
    ent->client->ps.pm_flags |= PMF_LIMBO | PMF_FOLLOW;
    if (makeCorpse && g_gamestate.integer == GS_PLAYING) CopyToBodyQue(ent);
    else trap_UnlinkEntity(ent);
    ent->client->ps.viewlocked = 0;
    ent->client->ps.viewlocked_entNum = 0;
    ent->r.maxs[2] = 0;
    ent->r.currentOrigin[2] += 8;
    trap_PointContents(ent->r.currentOrigin, -1);
    ent->s.weapon = ent->client->limboDropWeapon;
    if (ent->r.svFlags & SVF_BOT) {
        ent->client->sess.spectatorClient = ent->client->ps.clientNum;
        ent->client->sess.spectatorState = SPECTATOR_FREE;
    } else {
        ent->client->sess.spectatorClient = startclient;
        Cmd_FollowCycle_f(ent, 1);
        ent->client->sess.spectatorState =
            ent->client->sess.spectatorClient == startclient ? SPECTATOR_FREE : SPECTATOR_FOLLOW;
    }
    if (ent->client->sess.sessionTeam == TEAM_AXIS)
        ent->client->deployQueueNumber = level.redNumWaiting++;
    else if (ent->client->sess.sessionTeam == TEAM_ALLIES)
        ent->client->deployQueueNumber = level.blueNumWaiting++;
    for (i = 0; i < level.numConnectedClients; i++) {
        if (g_entities[level.sortedClients[i]].r.svFlags & SVF_BOT) continue;
        cl = &level.clients[level.sortedClients[i]];
        if (((cl->ps.pm_flags & PMF_LIMBO) ||
             (cl->sess.sessionTeam == TEAM_SPECTATOR && cl->sess.spectatorState == SPECTATOR_FOLLOW)) &&
            cl->sess.spectatorClient == ent - g_entities)
            Cmd_FollowCycle_f(&g_entities[level.sortedClients[i]], 1);
    }
}

/* TC Windows 2004b680; Linux 000a2680. */
void reinforce(gentity_t *ent, qboolean hostage) {
    int p;
    char userinfo[MAX_INFO_STRING], *respawnStr;
    if (ent->r.svFlags & SVF_BOT) {
        trap_GetUserinfo(ent->s.number, userinfo, sizeof(userinfo));
        respawnStr = Info_ValueForKey(userinfo, "respawn");
        if (!Q_stricmp(respawnStr, "no") || !Q_stricmp(respawnStr, "off")) return;
    }
    if (!(ent->client->ps.pm_flags & PMF_LIMBO)) {
        G_Printf("player already deployed, skipping\n");
        return;
    }
    if (ent->client->pers.mvCount > 0) {
        G_smvRemoveInvalidClients(ent, TEAM_AXIS);
        G_smvRemoveInvalidClients(ent, TEAM_ALLIES);
    }
    for (p = 0; p < MAX_PERSISTANT; p++)
        ent->client->ps.persistant[p] = ent->client->saved_persistant[p];
    respawn(ent, hostage);
}

/* TC Windows 2004b760; Linux 000a278a. */
void respawn(gentity_t *ent, qboolean hostage) {
    int team = ent->client->sess.sessionTeam;
    if (hostage) {
        if (!level.tceHostageTeam || !level.tceHostageActive || level.tceHostageSecured) return;
        level.tceHostageRespawnCount++;
        if ((team == TEAM_AXIS || team == TEAM_ALLIES) &&
            (level.tceHostageRespawnCount >= level.numFinalDead[team - TEAM_AXIS] ||
             level.tceHostageRespawnCount > 2)) {
            level.tceHostageActive = qfalse;
            level.tceHostageSecured = qtrue;
        }
    }
    ent->client->ps.pm_flags &= ~PMF_LIMBO;
    if (g_gametype.integer != 5 && ent->client->ps.persistant[PERS_RESPAWNS_LEFT] > 0 &&
        g_gamestate.integer == GS_PLAYING) {
        if (g_maxlives.integer > 0)
            ent->client->ps.persistant[PERS_RESPAWNS_LEFT]--;
        else {
            if (g_alliedmaxlives.integer > 0 && team == TEAM_ALLIES)
                ent->client->ps.persistant[PERS_RESPAWNS_LEFT]--;
            if (g_axismaxlives.integer > 0 && team == TEAM_AXIS)
                ent->client->ps.persistant[PERS_RESPAWNS_LEFT]--;
        }
    }
    G_DPrintf("Respawning %s, %i lives left\n", ent->client->pers.netname,
              ent->client->ps.persistant[PERS_RESPAWNS_LEFT]);
    ClientSpawn(ent, qfalse, hostage);
}

// NERVE - SMF - merge from team arena
/*
================
TeamCount

Returns number of players on a team
================
*/
team_t TeamCount(int ignoreClientNum, int team)
{
	int i, ref, count = 0;

	for(i=0; i<level.numConnectedClients; i++) {
		if((ref = level.sortedClients[i]) == ignoreClientNum) continue;
		if(level.clients[ref].sess.sessionTeam == team) count++;
	}

	return(count);
}
// -NERVE - SMF

/*
================
PickTeam

================
*/
team_t PickTeam(int ignoreClientNum)
{
	int counts[TEAM_NUM_TEAMS] = { 0, 0, 0 };

	counts[TEAM_ALLIES] = TeamCount(ignoreClientNum, TEAM_ALLIES);
	counts[TEAM_AXIS] = TeamCount(ignoreClientNum, TEAM_AXIS);

	if(counts[TEAM_ALLIES] > counts[TEAM_AXIS]) return(TEAM_AXIS);
	if(counts[TEAM_AXIS] > counts[TEAM_ALLIES]) return(TEAM_ALLIES);

	// equal team count, so join the team with the lowest score
	return(((level.teamScores[TEAM_ALLIES] > level.teamScores[TEAM_AXIS]) ? TEAM_AXIS : TEAM_ALLIES));
}

/*
===========
AddExtraSpawnAmmo
===========
*/
static void AddExtraSpawnAmmo( gclient_t *client, weapon_t weaponNum)
{
    /* Whole Windows2004b9b0 is RET, independent of Gear parse state. */
    (void)client;
    (void)weaponNum;
}

/* TC qagame2004b950: this SDK body matches once the TC ammo/clip mapping
 * and return-only AddExtraSpawnAmmo are active. Direct production tests cover
 * all64 protocol slots, including aliases and unchanged client fields. */
qboolean AddWeaponToPlayer( gclient_t *client, weapon_t weapon, int ammo, int ammoclip, qboolean setcurrent ) {
	COM_BitSet( client->ps.weapons, weapon );
	client->ps.ammoclip[BG_FindClipForWeapon(weapon)] = ammoclip;
	client->ps.ammo[BG_FindAmmoForWeapon(weapon)] = ammo;
	if( setcurrent )
		client->ps.weapon = weapon;

	// skill handling
	AddExtraSpawnAmmo( client, weapon );

	return qtrue;
}

void BotSetPOW(int entityNum, qboolean isPOW);

/*
===========
SetWolfSpawnWeapons
===========
*/
/* TC qagame 2004b9c0; all14 original team/class tables have these24 entries. */
static qboolean TCE_SpawnWeaponAvailable(int weapon, int tcClass, int skill, int team) {
    if (weapon < 0 || weapon >= TCE_MAX_WEAPONS) return qfalse;
    return BG_WeaponIsAvailable(weapon, gearDef.requiredSkill[weapon][tcClass],
                               gearDef.team[weapon], skill, team);
}

static void TCE_SetSpawnWeapons(gclient_t *client) {
    static const int botWeapons[24] = {
        10,3,8,45,41,33,42,5,44,43,50,24,48,49,47,51,46,23,32,25,6,13,0,0
    };
    int pc = client->sess.playerType, team = client->sess.sessionTeam;
    int tcClass = BG_WolfClassToTCE(pc);
    int skill, primary, secondary, weight;
    int flags = g_entities[client->ps.clientNum].r.svFlags;
    client->ps.classWeaponTime = -999999;
    client->ps.stats[STAT_PLAYER_CLASS] = pc;
    client->ps.teamNum = pc;
    memset(client->ps.ammo, 0, sizeof(client->ps.ammo));
    client->ps.weapons[0] = client->ps.weapons[1] = 0;
    client->ps.stats[15] = 0; /* Original +0x10c; meaning not yet recovered. */
    if (flags & SVF_BOT) {
#ifdef FEATURE_OMNIBOT
        if (!Bot_Interface_IsOmnibot(client->ps.clientNum))
#endif
        BotSetPOW(client->ps.clientNum, (flags & SVF_POW) ? qtrue : qfalse);
        if (flags & SVF_POW) return;
    }
    client->ps.weaponstate = WEAPON_READY;
    client->ps.holdable[9] = 0;
    if (client->ps.stats[STAT_TCE_FLAGS] & 0x400) {
        AddWeaponToPlayer(client, 0, 0, 0, qtrue);
        return;
    }
    if (client->ps.stats[STAT_TCE_FLAGS] & 0x100) {
        secondary = team == TEAM_ALLIES ? 2 : 39;
        client->sess.playerWeapon2 = secondary;
        AddWeaponToPlayer(client, secondary, weaponDef[secondary].startingClip,
                          weaponDef[secondary].startingClip, qtrue);
        return;
    }
    AddWeaponToPlayer(client, 1, 1, 0, qtrue);
    skill = (int)client->sess.skillpoints[tcClass] + 1;
    if ((flags & SVF_BOT)
#ifdef FEATURE_OMNIBOT
        && !Bot_Interface_IsOmnibot(client->ps.clientNum)
#endif
    ) {
        int choices[24], count = 0, i;
        for (i = 0; i < 24; ++i)
            if (TCE_SpawnWeaponAvailable(botWeapons[i], tcClass, skill, team))
                choices[count++] = botWeapons[i];
        if (count) {
            /* Preserve2004bc01..2004bc26: x87 product, negative truncation. */
            int index = -(int)((double)(rand() & 0x7fff) *
                (double)(1.0f / 32767.0f) * count * (double)-0.99f);
            client->sess.playerWeapon = choices[index];
        }
    }
    if (g_knifeonly.integer == 1) return;
    primary = client->sess.playerWeapon;
    if (!TCE_SpawnWeaponAvailable(primary, tcClass, skill, team)) {
        primary = primary >= 0 && primary < TCE_MAX_WEAPONS ? gearDef.equivalentWeapon[primary] : 0;
        if (!TCE_SpawnWeaponAvailable(primary, tcClass, skill, team))
            primary = BG_DefaultWeaponForClass(team, pc);
    }
    secondary = client->sess.playerWeapon2;
    if (!TCE_SpawnWeaponAvailable(secondary, tcClass, skill, team)) {
        secondary = secondary >= 0 && secondary < TCE_MAX_WEAPONS ? gearDef.equivalentWeapon[secondary] : 0;
        if (!TCE_SpawnWeaponAvailable(secondary, tcClass, skill, team))
            secondary = team == TEAM_ALLIES ? 2 : 39;
    }
    if (!BG_SidearmAvailableForPrimary(secondary, primary))
        secondary = team == TEAM_ALLIES ? 2 : 39;
    client->sess.playerWeapon = primary;
    client->sess.playerWeapon2 = secondary;
    if (secondary == 37 || secondary == 38 || secondary == 53 || secondary == 54)
        client->ps.ammoclip[BG_FindClipForWeapon(BG_AkimboSidearm(secondary))] = weaponDef[secondary].startingClip;
    AddWeaponToPlayer(client, secondary, weaponDef[secondary].startingAmmo,
                      weaponDef[secondary].startingClip, qfalse);
    AddWeaponToPlayer(client, primary, weaponDef[primary].startingAmmo,
                      weaponDef[primary].startingClip, qtrue);
    weight = weaponDef[primary].loadoutWeight;
    client->ps.holdable[9] = weight;
    client->ps.holdable[10] = BG_WeapToWeaponOnBack(primary);
    if (client->sess.playerWeapon3 == 36 && g_gametype.integer != 7 && tcClass == 1) {
        client->ps.stats[STAT_TCE_FLAGS] |= 2;
        return;
    }
    if (weight < 4) AddWeaponToPlayer(client, 4, 0, 1, qfalse);
    AddWeaponToPlayer(client, weight == 4 ? 9 : 30, 0, 1, qfalse);
}

/* Whole TC2004b9c0. Gear readiness never selects an ET-SDK loadout. */
void SetWolfSpawnWeapons(gclient_t *client) {
    if (client->sess.sessionTeam == TEAM_SPECTATOR ||
        client->sess.sessionTeam == TEAM_FREE) return;
    TCE_SetSpawnWeapons(client);
}

int G_CountTeamMedics( team_t team, qboolean alivecheck ) {
	int numMedics = 0;
	int i, j;

	for( i = 0; i < level.numConnectedClients; i++ ) {
		j = level.sortedClients[i];

		if( level.clients[j].sess.sessionTeam != team ) {
			continue;
		}

		if( level.clients[j].sess.playerType != PC_MEDIC ) {
			continue;
		}

		if( alivecheck ) {
			if( g_entities[j].health <= 0 ) {
				continue;
			}

			if( level.clients[j].ps.pm_type == PM_DEAD || level.clients[j].ps.pm_flags & PMF_LIMBO ) {
				continue;
			}
		}

		numMedics++;
	}

	return numMedics;
}

//
// AddMedicTeamBonus
//
void AddMedicTeamBonus( gclient_t *client ) {
	int numMedics = G_CountTeamMedics( client->sess.sessionTeam, qfalse );

	// compute health mod
	client->pers.maxHealth = 100 + 10 * numMedics;

	if( client->pers.maxHealth > 125 ) {
		client->pers.maxHealth = 125;
	}

	if( client->sess.skill[SK_BATTLE_SENSE] >= 3 ) {
		client->pers.maxHealth += 15;
	}

	client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;
}

/*
===========
ClientCheckName
============
*/
static void ClientCleanName( const char *in, char *out, int outSize )
{
	int		len, colorlessLen;
	char	ch;
	char	*p;
	int		spaces;

	//save room for trailing null byte
	outSize--;

	len = 0;
	colorlessLen = 0;
	p = out;
	*p = 0;
	spaces = 0;

	while( 1 ) {
		ch = *in++;
		if( !ch ) {
			break;
		}

		// don't allow leading spaces
		if( !*p && ch == ' ' ) {
			continue;
		}

		// check colors
		if( ch == Q_COLOR_ESCAPE ) {
			// solo trailing carat is not a color prefix
			if( !*in ) {
				break;
			}

			// don't allow black in a name, period
/*			if( ColorIndex(*in) == 0 ) {
				in++;
				continue;
			}
*/
			// make sure room in dest for both chars
			if( len > outSize - 2 ) {
				break;
			}

			*out++ = ch;
			*out++ = *in++;
			len += 2;
			continue;
		}

		// don't allow too many consecutive spaces
		if( ch == ' ' ) {
			spaces++;
			if( spaces > 3 ) {
				continue;
			}
		}
		else {
			spaces = 0;
		}

		if( len > outSize - 1 ) {
			break;
		}

		*out++ = ch;
		colorlessLen++;
		len++;
	}
	*out = 0;

	// don't allow empty names
	if( *p == 0 || colorlessLen == 0 ) {
		Q_strncpyz( p, "UnnamedPlayer", outSize );
	}
}

void G_StartPlayerAppropriateSound(gentity_t *ent, char *soundType) {
}

/*
===========
ClientUserInfoChanged

Called from ClientConnect when the player first connects and
directly by the server system when the player updates a userinfo variable.

The game can override any of the settings and call trap_SetUserinfo
if desired.
============
*/
/* Windows ClientUserinfoChanged2004bf70 and ClientSpawn2004cd90. */
void G_TCEUserinfoOptions(playerState_t *ps, const char *userinfo) {
    static const char *names[]={"cg_toggleCrouch","cg_toggleAiming","cg_freeAim"};
    int i;
    for(i=0;i<3;++i) {
        int bit=4<<i;
        if(atoi(Info_ValueForKey(userinfo,names[i])))ps->persistant[14]|=bit;
        else ps->persistant[14]&=~bit;
    }
}

void ClientUserinfoChanged( int clientNum ) {
	gentity_t *ent;
	char	*s;
	char	oldname[MAX_STRING_CHARS];
	char	userinfo[MAX_INFO_STRING];
	gclient_t	*client;
	int		i;
	char	skillStr[16] = "";
	char	medalStr[16] = "";
	int		characterIndex;


	ent = g_entities + clientNum;
	client = ent->client;

	client->ps.clientNum = clientNum;

	client->medals = 0;
	for( i = 0; i < SK_NUM_SKILLS; i++ ) {
		client->medals += client->sess.medals[ i ];
	}

	trap_GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );

	// check for malformed or illegal info strings
	if ( !Info_Validate(userinfo) ) {
		Q_strncpyz( userinfo, "\\name\\badinfo", sizeof(userinfo) );
	}

	if( g_developer.integer || *g_log.string || g_dedicated.integer ) 
	{
		G_Printf("Userinfo: %s\n", userinfo);
	}

	// check for local client
	s = Info_ValueForKey( userinfo, "ip" );
	if ( s && !strcmp( s, "localhost" ) ) {
		client->pers.localClient = qtrue;
		level.fLocalHost = qtrue;
		client->sess.referee = RL_REFEREE;
	}

	// OSP - extra client info settings
	//		 FIXME: move other userinfo flag settings in here
	if(ent->r.svFlags & SVF_BOT) {
		client->pers.autoActivate = PICKUP_TOUCH;
		client->pers.bAutoReloadAux = qtrue;
		client->pmext.bAutoReload = qtrue;
		client->pers.predictItemPickup = qfalse;
	} else {
		s = Info_ValueForKey(userinfo, "cg_uinfo");
		sscanf(s, "%i %i %i",
								&client->pers.clientFlags,
								&client->pers.clientTimeNudge,
								&client->pers.clientMaxPackets);

		client->pers.autoActivate = (client->pers.clientFlags & CGF_AUTOACTIVATE) ? PICKUP_TOUCH : PICKUP_ACTIVATE;
		client->pers.predictItemPickup = ((client->pers.clientFlags & CGF_PREDICTITEMS) != 0);

		if(client->pers.clientFlags & CGF_AUTORELOAD) {
			client->pers.bAutoReloadAux = qtrue;
			client->pmext.bAutoReload = qtrue;
		} else {
			client->pers.bAutoReloadAux = qfalse;
			client->pmext.bAutoReload = qfalse;
		}
	}

	// set name
	Q_strncpyz( oldname, client->pers.netname, sizeof( oldname ) );
	s = Info_ValueForKey (userinfo, "name");
	ClientCleanName( s, client->pers.netname, sizeof(client->pers.netname) );

	if ( client->pers.connected == CON_CONNECTED ) {
		if ( strcmp( oldname, client->pers.netname ) ) {
			trap_SendServerCommand( -1, va("print \"[lof]%s" S_COLOR_WHITE " [lon]renamed to[lof] %s\n\"", oldname, 
				client->pers.netname) );
		}
	}

	for( i = 0; i < SK_NUM_SKILLS; i++ ) {
		Q_strcat( skillStr, sizeof(skillStr), va("%i",client->sess.skill[i]) );
		Q_strcat( medalStr, sizeof(medalStr), va("%i",client->sess.medals[i]) );
		// FIXME: Gordon: wont this break if medals > 9 arnout? JK: Medal count is tied to skill count :() Gordon: er, it's based on >> skill per map, so for a huuuuuuge campaign it could break...
	}

	client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;

	// check for custom character
	s = Info_ValueForKey( userinfo, "ch" );
	if( *s ) {
		characterIndex = atoi(s);
	} else {
		characterIndex = -1;
	}

	G_TCEUserinfoOptions(&client->ps,userinfo);
	// To communicate it to cgame
	client->ps.stats[ STAT_PLAYER_CLASS ] = client->sess.playerType;
	// Gordon: Not needed any more as it's in clientinfo?

	// send over a subset of the userinfo keys so other clients can
	// print scoreboards, display models, and play custom sounds
	if ( ent->r.svFlags & SVF_BOT ) {
		// n: netname
		// t: sessionTeam
		// c1: color
		// hc: maxHealth
		// skill: skill
		// c: playerType (class?)
		// r: rank
		// f: fireteam
		// bot: botSlotNumber
		// nwp: noWeapon
		// m: medals
		// ch: character

		s = va( "n\\%s\\t\\%i\\skill\\%s\\c\\%i\\r\\%i\\m\\%s\\s\\%s%s\\dn\\%s\\dr\\%i\\w\\%i\\lw\\%i\\sw\\%i\\mu\\%i",
			client->pers.netname,
			client->sess.sessionTeam, 
			Info_ValueForKey( userinfo, "skill" ), 
			client->sess.playerType,
			client->sess.rank,
			medalStr,
            skillStr,
			characterIndex >= 0 ? va( "\\ch\\%i", characterIndex ) : "",
			client->disguiseNetname,
			client->disguiseRank,
			client->sess.playerWeapon,
			client->sess.latchPlayerWeapon,
			client->sess.latchPlayerWeapon2,
			client->sess.muted ? 1 : 0
		);
	} else {
		s = va( "n\\%s\\t\\%i\\c\\%i\\r\\%i\\m\\%s\\s\\%s\\dn\\%s\\dr\\%i\\w\\%i\\lw\\%i\\sw\\%i\\mu\\%i\\ref\\%i",
			client->pers.netname, 
			client->sess.sessionTeam, 
			client->sess.playerType, 
			client->sess.rank, 
			medalStr,
			skillStr,
			client->disguiseNetname,
			client->disguiseRank,
			client->sess.playerWeapon,
			client->sess.latchPlayerWeapon,
			client->sess.latchPlayerWeapon2,
			client->sess.muted ? 1 : 0,
			client->sess.referee
		);
	}

	trap_GetConfigstring( CS_PLAYERS + clientNum, oldname, sizeof( oldname ) );

	trap_SetConfigstring( CS_PLAYERS + clientNum, s );

	if( !Q_stricmp( oldname, s ) ) {
		TCE_LuaClientEvent( "et_ClientUserinfoChanged", clientNum );
		return;
	}

	G_LogPrintf( "ClientUserinfoChanged: %i %s\n", clientNum, s );
	G_DPrintf( "ClientUserinfoChanged: %i :: %s\n", clientNum, s );
	TCE_LuaClientEvent( "et_ClientUserinfoChanged", clientNum );
}


/*
===========
ClientConnect

Called when a player begins connecting to the server.
Called again for every map change or tournement restart.

The session information will be valid after exit.

Return NULL if the client should be allowed, otherwise return
a string with the reason for denial.

Otherwise, the client will be sent the current gamestate
and will eventually get to ClientBegin.

firstTime will be qtrue the very first time a client connects
to the server machine, but qfalse on map changes and tournement
restarts.
============
*/
/* Whole TC2004c520 / Linux000a3b0c. Keep original reconnect/session gates;
 * TC bot connections intentionally bypass the unrelated SDK AI setup. */
char *ClientConnect( int clientNum, qboolean firstTime, qboolean isBot ) {
	const char *luaRejection;
	char		*value;
	gclient_t	*client;
	char		userinfo[MAX_INFO_STRING];
	gentity_t	*ent;

#ifdef FEATURE_OMNIBOT
    Bot_Interface_RestoreClient(clientNum, isBot);
#endif
	ent = &g_entities[ clientNum ];

	trap_GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );

	// IP filtering
	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=500
	// recommanding PB based IP / GUID banning, the builtin system is pretty limited
	// check to see if they are on the banned IP list
	value = Info_ValueForKey (userinfo, "ip");
	if ( G_FilterIPBanPacket( value ) ) {
		return "You are banned from this server.";
	}

	// Xian - check for max lives enforcement ban
	if( g_gametype.integer != GT_WOLF_LMS ) {
		if( g_enforcemaxlives.integer && (g_maxlives.integer > 0 || g_axismaxlives.integer > 0 || g_alliedmaxlives.integer > 0) ) {
			if( trap_Cvar_VariableIntegerValue( "sv_punkbuster" ) ) {
				value = Info_ValueForKey ( userinfo, "cl_guid" );
				if ( G_FilterMaxLivesPacket ( value ) ) {
					return "Max Lives Enforcement Temp Ban. You will be able to reconnect when the next round starts. This ban is enforced to ensure you don't reconnect to get additional lives.";
				}
			} else {
				value = Info_ValueForKey ( userinfo, "ip" );	// this isn't really needed, oh well.
				if ( G_FilterMaxLivesIPPacket ( value ) ) {
					return "Max Lives Enforcement Temp Ban. You will be able to reconnect when the next round starts. This ban is enforced to ensure you don't reconnect to get additional lives.";
				}
			}
		}
	}
	// End Xian
	
	// we don't check password for bots and local client
	// NOTE: local client <-> "ip" "localhost"
	//   this means this client is not running in our current process
	if ( !isBot && !( ent->r.svFlags & SVF_BOT ) && (strcmp(Info_ValueForKey ( userinfo, "ip" ), "localhost") != 0)) {
		// check for a password
		value = Info_ValueForKey (userinfo, "password");
		if ( g_password.string[0] && Q_stricmp( g_password.string, "none" ) && strcmp( g_password.string, value) != 0) {
			if( !sv_privatepassword.string[ 0 ] || strcmp( sv_privatepassword.string, value ) ) {
				return "Invalid password";
			}
		}
	}

	luaRejection = TCE_LuaClientConnect( clientNum, firstTime, isBot );
	if ( luaRejection ) {
		return (char *)luaRejection;
	}

	// Gordon: porting q3f flag bug fix
	//			If a player reconnects quickly after a disconnect, the client disconnect may never be called, thus flag can get lost in the ether
	if( ent->inuse ) {
		G_LogPrintf( "Forcing disconnect on active client: %i\n", ent-g_entities );
		// so lets just fix up anything that should happen on a disconnect
		ClientDisconnect( ent-g_entities );
	}

	// they can connect
	ent->client = level.clients + clientNum;
	client = ent->client;



	memset( client, 0, sizeof(*client) );

	client->pers.connected = CON_CONNECTING;
	client->pers.connectTime = level.time;			// DHM - Nerve

	if( firstTime )
		client->pers.initialSpawn = qtrue;				// DHM - Nerve

	// read or initialize the session data
	if( firstTime ) {
		G_InitSessionData( client, userinfo );
		client->pers.enterTime = level.time;
		client->ps.persistant[PERS_SCORE] = 0;
	} else {
		G_ReadSessionData( client );
	}


	if( g_gametype.integer == GT_WOLF_CAMPAIGN ) {
		if( g_campaigns[level.currentCampaign].current == 0 || level.newCampaign ) {
			client->pers.enterTime = level.time;
		}
	} else {
		client->pers.enterTime = level.time;
	}

	if( isBot ) {
		// Set up the name for the bot client before initing the bot
		value = Info_ValueForKey ( userinfo, "scriptName" );
		if (value && value[0]) {
			Q_strncpyz( client->pers.botScriptName, value, sizeof( client->pers.botScriptName ) );
			ent->scriptName = client->pers.botScriptName;
		}
		ent->aiName = ent->scriptName;
		ent->s.number = clientNum;

		ent->r.svFlags |= SVF_BOT;
		ent->inuse = qtrue;
		// if this bot is reconnecting, and they aren't supposed to respawn, then dont let it in
		if (!firstTime) {
			value = Info_ValueForKey (userinfo, "respawn");
			if (value && value[0] && (!Q_stricmp(value, "NO") || !Q_stricmp(value, "DISCONNECT"))) {
				return "BotConnectFailed (no respawn)";
			}
		}

		/* TC ClientConnect2004c520 / Linux000a3b0c deliberately has no
		 * G_BotConnect/BotAISetupClient call. TC's entity-frame controller
		 * consumes the waypoint bot state rather than registering SDK AI. */
	}
	else if( g_gametype.integer == GT_COOP || g_gametype.integer == GT_SINGLE_PLAYER ) {
		// RF, in single player, enforce team = ALLIES
		// Arnout: disabled this for savegames as the double ClientBegin it causes wipes out all loaded data
		if( saveGamePending != 2 )
			client->sess.sessionTeam = TEAM_ALLIES;
			client->sess.spectatorState = SPECTATOR_NOT;
			client->sess.spectatorClient = 0;
	} else if( firstTime ) {
		// force into spectator
		client->sess.sessionTeam = TEAM_SPECTATOR;
		client->sess.spectatorState = SPECTATOR_FREE;
		client->sess.spectatorClient = 0;

		// unlink the entity - just in case they were already connected
		trap_UnlinkEntity( ent );
	}

	// get and distribute relevent paramters
	G_LogPrintf( "ClientConnect: %i\n", clientNum );
	G_UpdateCharacter( client );
	ClientUserinfoChanged( clientNum );

	if (g_gametype.integer == GT_SINGLE_PLAYER) {

		if (!isBot) {
			ent->scriptName = "player";

// START	Mad Doctor I changes, 8/14/2002
			// We must store this here, so that BotFindEntityForName can find the
			// player.
			ent->aiName = "player";
// END		Mad Doctor I changes, 8/12/2002

			G_Script_ScriptParse( ent );
			G_Script_ScriptEvent( ent, "spawn", "" );
		}

	}


	// don't do the "xxx connected" messages if they were caried over from previous level
	//		TAT 12/10/2002 - Don't display connected messages in single player
	if ( firstTime && !G_IsSinglePlayerGame())
	{
		trap_SendServerCommand( -1, va("cpm \"%s" S_COLOR_WHITE " connected\n\"", client->pers.netname) );
	}

	// count current clients and rank for scoreboard
	CalculateRanks();

#ifdef FEATURE_OMNIBOT
    Bot_Event_ClientConnected(clientNum, isBot);
#endif
	return NULL;
}

//
// Scaling for late-joiners of maxlives based on current game time
//
int G_ComputeMaxLives(gclient_t *cl, int maxRespawns)
{
#if defined(_MSC_VER) && defined(_M_IX86)
	/* TC2004ca00: no f32 scaled store and no zero-timelimit bypass. */
	int elapsed = (int)((unsigned int)level.time - (unsigned int)level.startTime);
	int remaining = (int)((unsigned int)maxRespawns - 1u);
	int val;
	float limit = g_timelimit.value, minute = 60000.0f, one = 1.0f, half = 0.5f;
	unsigned short savedControl, truncControl, comparison;
	__int64 converted;
	(void)cl;
	__asm {
		fild elapsed
		fld limit
		fmul minute
		fdivp st(1), st(0)
		fsubr one
		fimul remaining
		fld st(0)
		/* Original __ftol truncates to64 bits, caller consumes EAX. */
		fwait
		fnstcw savedControl
		fwait
		mov ax, savedControl
		or ax, 0x0c00
		mov truncControl, ax
		fldcw truncControl
		fistp converted
		fldcw savedControl
		mov eax, dword ptr converted
		mov val, eax
		fild val
		fsubr st(0), st(1)
		fcomp half
		fnstsw ax
		mov comparison, ax
		fstp st(0)
	}
	return (comparison & 0x100) ? val : (int)((unsigned int)val + 1u);
#else
	float scaled = (float)(maxRespawns - 1) * (1.0f - ((float)(level.time - level.startTime) / (g_timelimit.value * 60000.0f)));
	int val = (int)scaled;

	// rain - #102 - don't scale of the timelimit is 0
	if (g_timelimit.value == 0.0) {
		return maxRespawns - 1;
	}

	val += ((scaled - (float)val) < 0.5f) ? 0 : 1;
	return(val);
#endif
}

/*
===========
ClientBegin

called when a client has finished connecting, and is ready
to be placed into the level.  This will happen every level load,
and on transition between teams, but doesn't happen on respawns
============
*/
void ClientBegin( int clientNum )
{
	gentity_t	*ent;
	gclient_t	*client;
	int			flags;
	int			spawn_count, lives_left;		// DHM - Nerve

	ent = g_entities + clientNum;

	client = level.clients + clientNum;

	if ( ent->r.linked ) {
		trap_UnlinkEntity( ent );
	}

	G_InitGentity( ent );
	ent->touch = 0;
	ent->pain = 0;
	ent->client = client;

	client->pers.connected = CON_CONNECTED;
	client->pers.teamState.state = TEAM_BEGIN;

	// save eflags around this, because changing teams will
	// cause this to happen with a valid entity, and we
	// want to make sure the teleport bit is set right
	// so the viewpoint doesn't interpolate through the
	// world to the new position
	// DHM - Nerve :: Also save PERS_SPAWN_COUNT, so that CG_Respawn happens
	spawn_count = client->ps.persistant[PERS_SPAWN_COUNT];
	//bani - proper fix for #328
	lives_left = client->ps.persistant[PERS_RESPAWNS_LEFT] - 1;
	flags = client->ps.eFlags;
	memset( &client->ps, 0, sizeof( client->ps ) );
	client->ps.eFlags = flags;
	client->ps.persistant[PERS_SPAWN_COUNT] = spawn_count;
	client->ps.persistant[PERS_RESPAWNS_LEFT] = lives_left;
	

	client->pers.complaintClient = -1;
	client->pers.complaintEndTime = -1;

	// locate ent at a spawn point
	ClientSpawn( ent, qfalse, qfalse );
    G_ResetMarkers(ent);
    client->backupMarker.serverTime = 0;
    client->tceDefuseActive = qfalse;

	// Xian -- Changed below for team independant maxlives
	if( g_gametype.integer != GT_WOLF_LMS ) {
		if( ( client->sess.sessionTeam == TEAM_AXIS || client->sess.sessionTeam == TEAM_ALLIES ) ) {
		
			if( !client->maxlivescalced ) {
				if(g_maxlives.integer > 0) {
					client->ps.persistant[PERS_RESPAWNS_LEFT] = G_ComputeMaxLives(client, g_maxlives.integer);
				} else {
					client->ps.persistant[PERS_RESPAWNS_LEFT] = -1;
				}

				if( g_axismaxlives.integer > 0 || g_alliedmaxlives.integer > 0 ) {
					if(client->sess.sessionTeam == TEAM_AXIS) {
						client->ps.persistant[PERS_RESPAWNS_LEFT] = G_ComputeMaxLives(client, g_axismaxlives.integer);	
					} else if(client->sess.sessionTeam == TEAM_ALLIES) {
						client->ps.persistant[PERS_RESPAWNS_LEFT] = G_ComputeMaxLives(client, g_alliedmaxlives.integer);
					} else {
						client->ps.persistant[PERS_RESPAWNS_LEFT] = -1;
					}
 				}

				client->maxlivescalced = qtrue;
			} else {
				if( g_axismaxlives.integer > 0 || g_alliedmaxlives.integer > 0 ) {
					if( client->sess.sessionTeam == TEAM_AXIS ) {
						if( client->ps.persistant[ PERS_RESPAWNS_LEFT ] > g_axismaxlives.integer ) {
							client->ps.persistant[ PERS_RESPAWNS_LEFT ] = g_axismaxlives.integer;
						}
					} else if( client->sess.sessionTeam == TEAM_ALLIES ) {
						if( client->ps.persistant[ PERS_RESPAWNS_LEFT ] > g_alliedmaxlives.integer ) {
							client->ps.persistant[ PERS_RESPAWNS_LEFT ] = g_alliedmaxlives.integer;
						}
					}
 				}
			}
		}
	}	


	// DHM - Nerve :: Start players in limbo mode if they change teams during the match
	if(client->sess.sessionTeam != TEAM_SPECTATOR && (level.time - level.startTime > FRAMETIME * GAME_INIT_FRAMES) ) {
/*	  if( (client->sess.sessionTeam != TEAM_SPECTATOR && (level.time - client->pers.connectTime) > 60000) ||
		( g_gamestate.integer == GS_PLAYING && ( client->sess.sessionTeam == TEAM_AXIS || client->sess.sessionTeam == TEAM_ALLIES ) && 
		 g_gametype.integer == GT_WOLF_LMS && ( level.numTeamClients[0] > 0 || level.numTeamClients[1] > 0 ) ) ) {*/
		ent->health = 0;
		ent->r.contents = CONTENTS_CORPSE;

		client->ps.pm_type = PM_DEAD;
		client->ps.stats[STAT_HEALTH] = 0;

		if( g_gametype.integer != GT_WOLF_LMS ) {
			if( g_maxlives.integer > 0 ) {
				client->ps.persistant[PERS_RESPAWNS_LEFT]++;
			}
		}

		limbo(ent, qfalse);
	}

	if(client->sess.sessionTeam != TEAM_SPECTATOR) {
		trap_SendServerCommand( -1, va("print \"[lof]%s" S_COLOR_WHITE " [lon]entered the game\n\"", client->pers.netname) );
	}

	G_LogPrintf( "ClientBegin: %i\n", clientNum );

	// Xian - Check for maxlives enforcement
	if( g_gametype.integer != GT_WOLF_LMS ) {
		if ( g_enforcemaxlives.integer == 1 && (g_maxlives.integer > 0 || g_axismaxlives.integer > 0 || g_alliedmaxlives.integer > 0)) {
			char *value;
			char userinfo[MAX_INFO_STRING];
			trap_GetUserinfo( clientNum, userinfo, sizeof( userinfo ) );
			value = Info_ValueForKey ( userinfo, "cl_guid" );
			G_LogPrintf( "EnforceMaxLives-GUID: %s\n", value );
			AddMaxLivesGUID( value );

			value = Info_ValueForKey (userinfo, "ip");
			G_LogPrintf( "EnforceMaxLives-IP: %s\n", value );
			AddMaxLivesBan( value );
		}
	}
	// End Xian

	// count current clients and rank for scoreboard
	CalculateRanks();

	// No surface determined yet.
	ent->surfaceFlags = 0;

	// OSP
	G_smvUpdateClientCSList(ent);
	// OSP
	TCE_LuaClientEvent( "et_ClientBegin", clientNum );
}

gentity_t *SelectSpawnPointFromList( char *list, vec3_t spawn_origin, vec3_t spawn_angles )
{
	char *pStr, *token;
	gentity_t	*spawnPoint=NULL, *trav;
	#define	MAX_SPAWNPOINTFROMLIST_POINTS	16
	int	valid[MAX_SPAWNPOINTFROMLIST_POINTS];
	int numValid;

	memset( valid, 0, sizeof(valid) );
	numValid = 0;

	pStr = list;
	while((token = COM_Parse( &pStr )) != NULL && token[0]) {
		trav = g_entities + level.maxclients;
		while((trav = G_FindByTargetname(trav, token)) != NULL) {
			if (!spawnPoint) spawnPoint = trav;
			if (!SpotWouldTelefrag( trav )) {
				valid[numValid++] = trav->s.number;
				if (numValid >= MAX_SPAWNPOINTFROMLIST_POINTS) {
					break;
				}
			}
		}
	}

	if (numValid)
	{
		spawnPoint = &g_entities[valid[rand()%numValid]];

		// Set the origin of where the bot will spawn
		VectorCopy (spawnPoint->r.currentOrigin, spawn_origin);
		spawn_origin[2] += 9;

		// Set the angle we'll spawn in to
		VectorCopy (spawnPoint->s.angles, spawn_angles);
	}

	return spawnPoint;
}


// TAT 1/14/2003 - init the bot's movement autonomy pos to it's current position
void BotInitMovementAutonomyPos(gentity_t *bot);

#if 0 // rain - not used
static char *G_CheckVersion( gentity_t *ent )
{
	// Prevent nasty version mismatches (or people sticking in Q3Aimbot cgames)

	char userinfo[MAX_INFO_STRING];
	char *s;

	trap_GetUserinfo( ent->s.number, userinfo, sizeof( userinfo ) );
	s = Info_ValueForKey( userinfo, "cg_etVersion" );
	if( !s || strcmp( s, GAME_VERSION_DATED ) )
		return( s );
	return( NULL );
}
#endif

/*
===========
ClientSpawn

Called every time a client is placed fresh in the world:
after the first ClientBegin, and after each respawn
Initializes all non-persistant parts of playerState
============
*/
void ClientSpawn( gentity_t *ent, qboolean revived, qboolean hostage )
{
	int			index;
	vec3_t		spawn_origin, spawn_angles;
	gclient_t	*client;
	int			i;
	clientPersistant_t	saved;
	clientSession_t		savedSess;
	int			persistant[MAX_PERSISTANT];
	gentity_t	*spawnPoint;
	int			flags;
	int			savedPing;
	int			savedTeam;
	int			savedSlotNumber;
	gentity_t *savedTCSpawn;
	index = ent - g_entities;
	client = ent->client;
	G_UpdateSpawnCounts();
	savedTCSpawn = client->tceLastSpawnPoint;

	client->pers.lastSpawnTime = level.time;
	client->pers.lastBattleSenseBonusTime = level.timeCurrent;
	client->pers.lastHQMineReportTime = level.timeCurrent;

/*#ifndef _DEBUG
	if( !client->sess.versionOK ) {
		char *clientMismatchedVersion = G_CheckVersion( ent );	// returns NULL if version is identical

		if( clientMismatchedVersion ) {
			trap_DropClient( ent - g_entities, va( "Client/Server game mismatch: '%s/%s'", clientMismatchedVersion, GAME_VERSION_DATED ) );
		} else {
			client->sess.versionOK = qtrue;
		}
	}
#endif*/

	// find a spawn point
	// do it before setting health back up, so farthest
	// ranging doesn't count this client
	if( revived ) {
		spawnPoint = ent;
		VectorCopy( ent->r.currentOrigin, spawn_origin );
		spawn_origin[2] += 9;	// spawns seem to be sunk into ground?
		VectorCopy( ent->s.angles, spawn_angles );
	} else {
		// Arnout: let's just be sure it does the right thing at all times. (well maybe not the right thing, but at least not the bad thing!)
		//if( client->sess.sessionTeam == TEAM_SPECTATOR || client->sess.sessionTeam == TEAM_FREE ) {
		if( !hostage && client->sess.sessionTeam != TEAM_AXIS && client->sess.sessionTeam != TEAM_ALLIES ) {
			spawnPoint = SelectSpectatorSpawnPoint( spawn_origin, spawn_angles );
		} else {
			// RF, if we have requested a specific spawn point, use it (fixme: what if this will place us inside another character?)
/*			spawnPoint = NULL;
			trap_GetUserinfo( ent->s.number, userinfo, sizeof(userinfo) );
			if( (str = Info_ValueForKey( userinfo, "spawnPoint" )) != NULL && str[0] ) {
				spawnPoint = SelectSpawnPointFromList( str, spawn_origin, spawn_angles );
				if (!spawnPoint) {
					G_Printf( "WARNING: unable to find spawn point \"%s\" for bot \"%s\"\n", str, ent->aiName );
				}
			}
			//
			if( !spawnPoint ) {*/
                if (hostage && (g_gametype.integer != 5 ||
                    g_gamestate.integer == GS_WARMUP || g_gamestate.integer == GS_WARMUP_COUNTDOWN)) {
                    /* Original leaves origin undefined for this unsupported caller path.
                     * Retain current pose instead of consuming uninitialized stack data. */
                    spawnPoint = NULL;
                    VectorCopy(ent->r.currentOrigin, spawn_origin);
                    VectorCopy(ent->s.angles, spawn_angles);
                } else if (hostage) {
					spawnPoint = SelectCTFSpawnPoint(client->sess.sessionTeam, client->pers.teamState.state,
						spawn_origin, spawn_angles, client->sess.spawnObjectiveIndex,
						client->sess.tcePreferredSpawnEntity, qtrue);
				} else {
					spawnPoint = NULL;
					if (client->sess.tcePreferredSpawnEntity &&
						(g_gametype.integer == 2 || g_gametype.integer == 5 || g_gametype.integer == 7) &&
						g_gamestate.integer != GS_WARMUP && g_gamestate.integer != GS_WARMUP_COUNTDOWN)
						spawnPoint = SelectCTFSpawnPoint(client->sess.sessionTeam, client->pers.teamState.state,
							spawn_origin, spawn_angles, client->sess.spawnObjectiveIndex,
							client->sess.tcePreferredSpawnEntity, qfalse);
					if (!spawnPoint) spawnPoint = SelectCTFSpawnPoint(client->sess.sessionTeam,
						client->pers.teamState.state, spawn_origin, spawn_angles,
						client->sess.spawnObjectiveIndex, 0, qfalse);
					if (spawnPoint && (g_gametype.integer == 2 || g_gametype.integer == 5 || g_gametype.integer == 7)) {
						savedTCSpawn = spawnPoint;
						client->sess.tcePreferredSpawnEntity = spawnPoint->s.number;
					}
				}
//			}
		}
	}

	client->pers.teamState.state = TEAM_ACTIVE;

	// toggle the teleport bit so the client knows to not lerp
	flags = ent->client->ps.eFlags & EF_TELEPORT_BIT;
	flags ^= EF_TELEPORT_BIT;
	flags |= (client->ps.eFlags & EF_VOTED);
	// clear everything but the persistant data

	ent->s.eFlags &= ~EF_MOUNTEDTANK;

	saved			= client->pers;
	savedSess		= client->sess;
	savedPing		= client->ps.ping;
	savedTeam		= client->ps.teamNum;
	// START	xkan, 8/27/2002
	savedSlotNumber	= client->botSlotNumber;
	// END		xkan, 8/27/2002

	for( i = 0 ; i < MAX_PERSISTANT ; i++ ) {
		persistant[i] = client->ps.persistant[i];
	}

	{
		qboolean set = client->maxlivescalced;

		memset( client, 0, sizeof(*client) );

		client->maxlivescalced = set;
	}

	client->pers			= saved;
	client->sess			= savedSess;
	client->tceLastSpawnPoint = savedTCSpawn;
	if (hostage) client->ps.stats[STAT_TCE_FLAGS] |= 0x400;
	client->ps.ping			= savedPing;
	client->ps.teamNum		= savedTeam;
	// START	xkan, 8/27/2002
	client->botSlotNumber	= savedSlotNumber;
	// END		xkan, 8/27/2002

	for( i = 0 ; i < MAX_PERSISTANT ; i++ ) {
		client->ps.persistant[i] = persistant[i];
	}

	// increment the spawncount so the client will detect the respawn
	client->ps.persistant[PERS_SPAWN_COUNT]++;
	if( revived ) {
		client->ps.persistant[PERS_REVIVE_COUNT]++;
	}
	client->ps.persistant[PERS_TEAM] = client->sess.sessionTeam;
	client->ps.persistant[PERS_HWEAPON_USE] = 0;

	client->airOutTime = level.time + 12000;

	// clear entity values
	client->ps.stats[STAT_MAX_HEALTH] = client->pers.maxHealth;
	client->ps.eFlags = flags;
	// MrE: use capsules for AI and player
	//client->ps.eFlags |= EF_CAPSULE;

	ent->s.groundEntityNum = ENTITYNUM_NONE;
	ent->client = &level.clients[index];
	ent->takedamage = qtrue;
	ent->inuse = qtrue;
	if( ent->r.svFlags & SVF_BOT )
		{ ent->classname = "bot"; client->ps.stats[STAT_TCE_FLAGS] |= 0x800; }
	else
		ent->classname = "player";
	ent->r.contents = CONTENTS_BODY;

	ent->clipmask = MASK_PLAYERSOLID;

	// DHM - Nerve :: Init to -1 on first spawn;
	if ( !revived )
		ent->props_frame_state = -1;

	ent->die = player_die;
	ent->waterlevel = 0;
	ent->watertype = 0;
	ent->flags = 0;
	
	VectorCopy( playerMins, ent->r.mins );
	VectorCopy( playerMaxs, ent->r.maxs );
    {
        /* ClientSpawn2004cd90, ps+0x3dc..0x3f0. The stock SDK hull is wider. */
        float halfWidth = g_newbbox.integer ? 16.0f : 14.0f;
        VectorSet(ent->r.mins, -halfWidth, -halfWidth, g_newbbox.integer ? -30.0f : -24.0f);
        VectorSet(ent->r.maxs, halfWidth, halfWidth, g_newbbox.integer ? 57.0f : 46.0f);
    }


	// Ridah, setup the bounding boxes and viewheights for prediction
	VectorCopy( ent->r.mins, client->ps.mins );
	VectorCopy( ent->r.maxs, client->ps.maxs );
	
	client->ps.crouchViewHeight = CROUCH_VIEWHEIGHT;
	client->ps.standViewHeight = DEFAULT_VIEWHEIGHT;
	client->ps.deadViewHeight = DEAD_VIEWHEIGHT;
    {
        client->ps.crouchViewHeight = g_newbbox.integer ? 26.0f : 21.0f;
        client->ps.standViewHeight = g_newbbox.integer ? 52.0f : 42.0f;
        /* Original2004d26d/2004d2a2: -16 normal, -20 with new bounding box. */
        client->ps.deadViewHeight = g_newbbox.integer ? -20.0f : -16.0f;
        if (g_newbbox.integer) client->ps.stats[STAT_TCE_FLAGS] |= 0x200;
    }

	
	{
        char options[MAX_INFO_STRING];
        trap_GetUserinfo(ent-g_entities,options,sizeof(options));
        G_TCEUserinfoOptions(&client->ps,options);
    }
	client->ps.crouchMaxZ = client->ps.maxs[2] - (client->ps.standViewHeight - client->ps.crouchViewHeight);

	client->ps.runSpeedScale = 0.8;
	client->ps.sprintSpeedScale = 1.1;
	client->ps.crouchSpeedScale = 0.25;
    {
        client->ps.runSpeedScale = g_realism.integer == 1 ? .45f : .51f;
        client->ps.sprintSpeedScale = .744f;
        client->ps.crouchSpeedScale = .213f;
    }

	client->ps.weaponstate = WEAPON_READY;

	// Rafael
	client->pmext.sprintTime = SPRINTTIME;
	client->ps.sprintExertTime = 0;

	client->ps.friction = 1.0;
	// done.

	// TTimo
	// retrieve from the persistant storage (we use this in pmoveExt_t beause we need it in bg_*)
	client->pmext.bAutoReload = client->pers.bAutoReloadAux;
	// done

	client->ps.clientNum = index;

	trap_GetUsercmd( client - level.clients, &ent->client->pers.cmd );	// NERVE - SMF - moved this up here

	// DHM - Nerve :: Add appropriate weapons
	if ( !revived ) {
		qboolean update = qfalse;

		if( client->sess.playerType != client->sess.latchPlayerType )
			update = qtrue;

		//if ( update || client->sess.playerWeapon != client->sess.latchPlayerWeapon) {
		//	G_ExplodeMines(ent);
		//}

		client->sess.playerType = client->sess.latchPlayerType;

		if( G_IsWeaponDisabled( ent, client->sess.latchPlayerWeapon ) ) {
			bg_playerclass_t* classInfo = BG_PlayerClassForPlayerState( &ent->client->ps );
			client->sess.latchPlayerWeapon = classInfo->classWeapons[0];
			update = qtrue;
		}

		if( client->sess.playerWeapon != client->sess.latchPlayerWeapon ) {
			client->sess.playerWeapon = client->sess.latchPlayerWeapon;
			update = qtrue;
		}

		if( G_IsWeaponDisabled( ent, client->sess.playerWeapon ) ) {
			bg_playerclass_t* classInfo = BG_PlayerClassForPlayerState( &ent->client->ps );
			client->sess.playerWeapon = classInfo->classWeapons[0];
			update = qtrue;
		}

		client->sess.playerWeapon2 = client->sess.latchPlayerWeapon2;
		client->sess.playerWeapon3 = client->sess.latchPlayerWeapon3;

		if( update ) {
			ClientUserinfoChanged( index );
		}
	}

	// TTimo keep it isolated from spectator to be safe still
	if( client->sess.sessionTeam != TEAM_SPECTATOR ) {
		// Xian - Moved the invul. stuff out of SetWolfSpawnWeapons and put it here for clarity
		if ( g_fastres.integer == 1 && revived )
			client->ps.powerups[PW_INVULNERABLE] = level.time + 1000; 
		else
			client->ps.powerups[PW_INVULNERABLE] = level.time + 3000; 
	}
	// End Xian

	G_UpdateCharacter( client );

	if (g_currentRound.integer < 1 &&
        (g_gamestate.integer == GS_WARMUP || g_gamestate.integer == GS_WARMUP_COUNTDOWN)) CalculateRanks();
    SetWolfSpawnWeapons( client ); 
	
	// START	Mad Doctor I changes, 8/17/2002

	// JPW NERVE -- increases stats[STAT_MAX_HEALTH] based on # of medics in game
	client->pers.maxHealth = client->ps.stats[STAT_MAX_HEALTH] = 100;

	// END		Mad Doctor I changes, 8/17/2002

	if( !revived ) {
		client->pers.cmd.weapon = ent->client->ps.weapon;
	}
// dhm - end

	// JPW NERVE ***NOTE*** the following line is order-dependent and must *FOLLOW* SetWolfSpawnWeapons() in multiplayer
	// AddMedicTeamBonus() now adds medic team bonus and stores in ps.stats[STAT_MAX_HEALTH].

	if( client->sess.skill[SK_BATTLE_SENSE] >= 3 )
		// We get some extra max health, but don't spawn with that much
		ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH] - 15;
	else
		ent->health = client->ps.stats[STAT_HEALTH] = client->ps.stats[STAT_MAX_HEALTH];

    client->ps.holdable[11] = 0; /* TC ps+0x3bc. */
    client->ps.stats[STAT_TCE_SHOT_SEED] = rand() & 0xffff;
    client->tceObjectiveActivityUntil = level.time + 5000;
    client->ps.persistant[PERS_BLEH_2] = 0;
	G_SetOrigin( ent, spawn_origin );
	VectorCopy( spawn_origin, client->ps.origin );

	// the respawned flag will be cleared after the attack and jump keys come up
	client->ps.pm_flags |= PMF_RESPAWNED;

	if( !revived ) {
		SetClientViewAngle( ent, spawn_angles );
	} else {
		SetClientViewAnglePitch(ent, 0);
	}

	if( (ent->r.svFlags & SVF_BOT)
#ifdef FEATURE_OMNIBOT
        && !Bot_Interface_IsOmnibot(index)
#endif
    ) {
		// xkan, 10/11/2002 - the ideal view angle is defaulted to 0,0,0, but the 
		// spawn_angles is the desired angle for the bots to face.
		BotSetIdealViewAngles( index, spawn_angles );

		// TAT 1/14/2003 - now that we have our position in the world, init our autonomy positions
		BotInitMovementAutonomyPos(ent);
	}

	if( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		//G_KillBox( ent );
		trap_LinkEntity (ent);
	}

	/* Original ClientSpawn2004cd90: client[0x4cd] = level.time. */
	client->tceDamageSpawnTime = level.time;
    /* TC ClientSpawn2004d5e6: five seconds until objective activity refresh. */
    client->tceObjectiveActivityUntil=level.time+5000;
	client->respawnTime = level.timeCurrent;
    client->tceRespawnNotBefore = level.timeCurrent;
	client->inactivityTime = level.time + g_inactivity.integer * 1000;
	client->latched_buttons = 0;
	client->latched_wbuttons = 0;	//----(SA)	added

	// xkan, 1/13/2003 - reset death time
	client->deathTime = 0;

	if ( level.intermissiontime ) {
		MoveClientToIntermission( ent );
	} else {
		// fire the targets of the spawn point
		if ( !revived )
			G_UseTargets( spawnPoint, ent );
	}

	// run a client frame to drop exactly to the floor,
	// initialize animations and other things
	client->ps.commandTime = level.time - 100;
	ent->client->pers.cmd.serverTime = level.time;
	ClientThink( ent-g_entities );

	// positively link the client, even if the command times are weird
	if ( ent->client->sess.sessionTeam != TEAM_SPECTATOR ) {
		BG_PlayerStateToEntityState( &client->ps, &ent->s, qtrue );
		VectorCopy( ent->client->ps.origin, ent->r.currentOrigin );
		trap_LinkEntity( ent );
	}

	// run the presend to set anything else
	ClientEndFrame( ent );

	// set idle animation on weapon
	ent->client->ps.weapAnim = ( ( ent->client->ps.weapAnim & ANIM_TOGGLEBIT ) ^ ANIM_TOGGLEBIT ) | PM_IdleAnimForWeapon( ent->client->ps.weapon );

	// clear entity state values
	BG_PlayerStateToEntityState( &client->ps, &ent->s, qtrue );

	// https://zerowing.idsoftware.com/bugzilla/show_bug.cgi?id=569
	G_TCEResetFrameMarkers( ent ); /* TC2004d78c calls20048370, not80-byte reset. */

#ifdef FEATURE_OMNIBOT
    if (!Bot_Interface_IsOmnibot((int)(ent-g_entities))) {
#endif
	// Set up bot speed bonusses
	BotSpeedBonus( ent->s.number );

	// RF, start the scripting system
	if (!revived && client->sess.sessionTeam != TEAM_SPECTATOR) {
		Bot_ScriptInitBot( ent->s.number );
		//
		if (spawnPoint && spawnPoint->targetname) {
			Bot_ScriptEvent( ent->s.number, "spawn", spawnPoint->targetname );
		} else {
			Bot_ScriptEvent( ent->s.number, "spawn", "" );
		}
		// RF, call entity scripting event
		G_Script_ScriptEvent( ent, "playerstart", "" );
	} else if( revived && ent->r.svFlags & SVF_BOT) {
		Bot_ScriptEvent( ent->s.number, "revived", "" );
	}
#ifdef FEATURE_OMNIBOT
    } else if (!revived && client->sess.sessionTeam != TEAM_SPECTATOR) {
        /* Map scripts remain active even when SDK bot scripts do not own us. */
        G_Script_ScriptEvent(ent, "playerstart", "");
    }
#endif
	/* Encoded tactical offsets start centered, including after respawn. */
	client->ps.holdable[5]=client->ps.holdable[6]=2000;
#ifdef FEATURE_OMNIBOT
    Bot_Event_Respawn((int)(ent-g_entities));
#endif
	TCE_LuaSpawn((int)(ent-g_entities), revived, hostage);
}


/*
===========
ClientDisconnect

Called when a player drops from the server.
Will not be called between levels.

This should NOT be called directly by any game logic,
call trap_DropClient(), which will call this and do
server system housekeeping.
============
*/
void ClientDisconnect( int clientNum ) {
	gentity_t	*ent;
	gentity_t	*flag=NULL;
	gitem_t		*item=NULL;
	vec3_t		launchvel;
	int			i;

	ent = g_entities + clientNum;
	if ( !ent->client ) {
		return;
	}
	TCE_LuaClientEvent( "et_ClientDisconnect", clientNum );



	G_RemoveClientFromFireteams( clientNum, qtrue, qfalse );
	G_RemoveFromAllIgnoreLists( clientNum );
	G_LeaveTank( ent, qfalse );

	// stop any following clients
	for ( i = 0 ; i < level.numConnectedClients ; i++ ) {
		flag = g_entities + level.sortedClients[i];
		if ( flag->client->sess.sessionTeam == TEAM_SPECTATOR
			&& flag->client->sess.spectatorState == SPECTATOR_FOLLOW
			&& flag->client->sess.spectatorClient == clientNum ) {
			StopFollowing( flag );
		}
		if ( flag->client->ps.pm_flags & PMF_LIMBO
			&& flag->client->sess.spectatorClient == clientNum ) {
			Cmd_FollowCycle_f( flag, 1 );
		}
	}

	// NERVE - SMF - remove complaint client
	for ( i = 0 ; i < level.numConnectedClients ; i++ ) {
		if ( flag && flag->client->pers.complaintClient == clientNum ) {
			flag->client->pers.complaintClient = -1;
			flag->client->pers.complaintEndTime = 0;

			CPx( level.sortedClients[i], "complaint -2" );
			break;
		}
	}

	if( g_landminetimeout.integer ) {
		G_ExplodeMines(ent);
	}
	G_FadeItems(ent, (meansOfDeath_t)46);

	// remove ourself from teamlists
	{
		mapEntityData_t	*mEnt;
		mapEntityData_Team_t *teamList;

		for( i = 0; i < 2; i++ ) {
			teamList = &mapEntityData[i];

			if((mEnt = G_FindMapEntityData(&mapEntityData[0], ent-g_entities)) != NULL) {
				G_FreeMapEntityData( teamList, mEnt );
			}

			mEnt = G_FindMapEntityDataSingleClient( teamList, NULL, ent->s.number, -1 );
			
			while( mEnt ) {
				mapEntityData_t	*mEntFree = mEnt;

				mEnt = G_FindMapEntityDataSingleClient( teamList, mEnt, ent->s.number, -1 );

				G_FreeMapEntityData( teamList, mEntFree );
			}
		}
	}

	// send effect if they were completely connected
	if ( ent->client->pers.connected == CON_CONNECTED 
		&& ent->client->sess.sessionTeam != TEAM_SPECTATOR
		&& !(ent->client->ps.pm_flags & PMF_FOLLOW)
        && ent->client->sess.spectatorState != SPECTATOR_FOLLOW ) {

		// They don't get to take powerups with them!
		// Especially important for stuff like CTF flags
        /* TC2004d840: objective drop on disconnect precedes flag handling. */
        G_TCEReleaseObjectives(ent,qtrue,qtrue);
/* TC disconnect drops objective items only; no death weapon drop. */

		// New code for tossing flags
			if (ent->client->ps.powerups[PW_REDFLAG]) {
				item = BG_FindItem("Red Flag");
				if (!item)
					item = BG_FindItem("Objective");

				ent->client->ps.powerups[PW_REDFLAG] = 0;
			}
			if (ent->client->ps.powerups[PW_BLUEFLAG]) {
				item = BG_FindItem("Blue Flag");
				if (!item)
					item = BG_FindItem("Objective");

				ent->client->ps.powerups[PW_BLUEFLAG] = 0;
			}

			if( item ) {
                vec3_t angles, offset, origin, mins={-10,-10,0}, maxs={10,10,20};
                trace_t tr;
                VectorCopy(ent->client->ps.viewangles, angles);
                if (angles[PITCH]<-30) angles[PITCH]=-30;
                else if (angles[PITCH]>30) angles[PITCH]=30;
                angles[YAW]-=135;
                AngleVectors(angles, launchvel, NULL, NULL);
                VectorScale(launchvel,16,offset);
                offset[2]+=ent->client->ps.viewheight*.5f;
                VectorScale(launchvel,64,launchvel);
                /* 2004e1be..2004e1e2: x87 keeps random products and both
                 * additions extended, storing only the final velocity. */
                launchvel[2]=(float)((double)(rand()&0x7fff)*
                    (double)(1.f/32767.f)*35.0+(double)launchvel[2]+50.0);
                VectorAdd(ent->client->ps.origin,offset,origin);
                trap_Trace(&tr,ent->client->ps.origin,mins,maxs,origin,ent->s.number,CONTENTS_SOLID);
                flag=LaunchItem(item,tr.endpos,launchvel,ent-g_entities);
				/* TC2004e2a0: +0xa8 is modelindex2, not density (+0xf4). */
				flag->s.modelindex2 = ent->s.otherEntityNum2;
				flag->message = ent->message;	// DHM - Nerve :: also restore item name
				// Clear out player's temp copies
				ent->s.otherEntityNum2 = 0;
				ent->message = NULL;
			}

		// OSP - Log stats too
		G_LogPrintf("WeaponStats: %s\n", G_createStats(ent));
	}

	G_LogPrintf( "ClientDisconnect: %i\n", clientNum );

	trap_UnlinkEntity (ent);
	ent->s.modelindex = 0;
	ent->inuse = qfalse;
	ent->classname = "disconnected";
	ent->client->pers.connected = CON_DISCONNECTED;
	ent->client->ps.persistant[PERS_TEAM] = TEAM_FREE;
	i = ent->client->sess.sessionTeam;
	ent->client->sess.sessionTeam = TEAM_FREE;
	ent->active = 0;

	trap_SetConfigstring( CS_PLAYERS + clientNum, "");


	CalculateRanks();

	if ( (ent->r.svFlags & SVF_BOT)
#ifdef FEATURE_OMNIBOT
        && !Bot_Interface_IsOmnibot(clientNum)
#endif
    ) {
		BotAIShutdownClient( clientNum );
	}
#ifdef FEATURE_OMNIBOT
    Bot_Event_ClientDisConnected(clientNum);
#endif

	// OSP
	G_verifyMatchState(i);
	G_smvAllRemoveSingleClient(ent - g_entities);
	// OSP
}

// In just the GAME DLL, we want to store the groundtrace surface stuff,
// so we don't have to keep tracing.
void ClientStoreSurfaceFlags
( 
	int clientNum, 
	int surfaceFlags
)
{
	// Store the surface flags
	g_entities[clientNum].surfaceFlags = surfaceFlags;

}
