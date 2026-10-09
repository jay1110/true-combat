/*
 * name:		g_combat.c
 *
 * desc:		
 *
*/

#include "g_local.h"
#include "tce_lua.h"
#ifdef FEATURE_OMNIBOT
#include "g_etbot_interface.h"
#define TCE_INTERNAL_BOT(ent) (!Bot_Interface_IsOmnibot((ent)->s.number))
#else
#define TCE_INTERNAL_BOT(ent) (qtrue)
#endif
#include "tce_bg.h"
extern void G_Voice(gentity_t *, gentity_t *, int, const char *, qboolean);
#include "../game/q_shared.h"
#include "../game/botlib.h"		//bot lib interface
#include "../game/be_aas.h"
#include "../game/be_ea.h"
#include "../game/be_ai_gen.h"
#include "../game/be_ai_goal.h"
#include "../game/be_ai_move.h"
#include "../botai/botai.h"			//bot ai interface
#include "../botai/ai_main.h"
#include "../botai/chars.h"
#include "../botai/ai_team.h"
#include "../botai/ai_dmq3.h"


extern void BotRecordKill( int client, int enemy );
extern void BotRecordPain( int client, int enemy, int mod );
extern void BotRecordDeath( int client, int enemy );

extern vec3_t muzzleTrace;

/*
============
AddScore

Adds score to both the client and his team
============
*/
void AddScore( gentity_t *ent, int score ) {
	if ( !ent || !ent->client ) {
		return;
	}
	// no scoring during pre-match warmup
	if ( g_gamestate.integer != GS_PLAYING ) {
		return;
	}

	/* TC qagame200571d0: gametypes 2/5/7 and shared flag 0x400
	 * suppress score changes before CalculateRanks. */
	if( g_gametype.integer == 5 || g_gametype.integer == 2 ||
	    g_gametype.integer == 7 ||
	    (ent->client->ps.stats[STAT_TCE_FLAGS] & 0x400) ) {
		return;
	}

	//ent->client->ps.persistant[PERS_SCORE] += score;
	ent->client->sess.game_points += score;

//	level.teamScores[ ent->client->ps.persistant[PERS_TEAM] ] += score;
	CalculateRanks();
}

/*
============
AddKillScore

Adds score to both the client and his team, only used for playerkills, for lms
============
*/
/* TC Windows20057230: whole controller; no score while warmup/winner/flag400. */
void AddKillScore(gentity_t *ent,int score) {
    if(!ent || !ent->client || level.warmupTime || level.lmsWinningTeam ||
        (ent->client->ps.stats[STAT_TCE_FLAGS]&0x400))return;
    if(g_gametype.integer==5 || g_gametype.integer==2 || g_gametype.integer==7) {
        ent->client->ps.persistant[PERS_SCORE]+=score;
        level.teamScores[ent->client->ps.persistant[PERS_TEAM]]+=score;
    }
    ent->client->sess.game_points+=score;
    CalculateRanks();
}

/*
=================
TossClientItems

Toss the weapon and powerups for the killed player
=================
*/
/* Whole original200572b0: primary, Bodycount health, Objective bomb. */
void TossClientItems(gentity_t *self) {
    weapon_t primary;
    if(g_gamestate.integer==GS_INTERMISSION)return;
    primary=G_GetPrimaryWeaponForClient(self->client);
    if(primary)G_DropWeapon(self,primary);
    if(g_gametype.integer==7)G_TCEDropHealth(self);
    G_TCEReleaseObjectives(self,qfalse,qfalse);
}

/*
==================
LookAtKiller
==================
*/
/* TC200a11b0 returns an unspilled x87 angle directly to __ftol in
 * LookAtKiller. Its degree factor is double(180 / float(pi)), not M_PI. */
static int G_TCEDeadYaw(const vec3_t dir) {
	static const double degrees = 57.29577791868204; /* 200ad5a0:25cf030ddca54c40 */
	if(dir[0] == 0.0f) return dir[1] == 0.0f ? 0 : dir[1] > 0.0f ? 90 : 270;
#if defined(_MSC_VER) && defined(_M_IX86)
	{
		int result;
		unsigned short control, truncated;
		static const float fullTurn = 360.0f;
		__asm {
			mov ecx, dir
			fld dword ptr [ecx+4]
			fld dword ptr [ecx]
			fpatan
			fmul degrees
			ftst
			fnstsw ax
			test ah, 1
			jz positiveYaw
			fadd fullTurn
		positiveYaw:
			fnstcw control
			mov ax, control
			or ax, 0c00h
			mov truncated, ax
			fldcw truncated
			fistp result
			fldcw control
		}
		return result;
	}
#else
	{
		long double yaw = atan2l((long double)dir[1], (long double)dir[0]) * degrees;
		if(yaw < 0) yaw += 360;
		return (int)yaw;
	}
#endif
}

void LookAtKiller( gentity_t *self, gentity_t *inflictor, gentity_t *attacker ) {
	vec3_t		dir;
	vec3_t		angles;

	if ( attacker && attacker != self ) {
		VectorSubtract (attacker->s.pos.trBase, self->s.pos.trBase, dir);
	} else if ( inflictor && inflictor != self ) {
		VectorSubtract (inflictor->s.pos.trBase, self->s.pos.trBase, dir);
	} else {
		self->client->ps.stats[STAT_DEAD_YAW] = self->s.angles[YAW];
		return;
	}

	self->client->ps.stats[STAT_DEAD_YAW] = G_TCEDeadYaw( dir );

	angles[YAW] = vectoyaw ( dir );
	angles[PITCH] = 0; 
	angles[ROLL] = 0;
}

/*
==================
GibEntity
==================
*/
/* Private TC adapters: preserve original x87 intermediates without changing
 * the SDK/shared math ABI used by other reconstructed callers. */
static void G_TCEGibNormalize(const vec3_t input, vec3_t output) {
	if(input[0]==0 && input[1]==0 && input[2]==0) { VectorClear(output); return; }
#if defined(_MSC_VER) && defined(_M_IX86)
	__asm {
		mov ecx, input
		mov edx, output
		fld dword ptr [ecx]
		fmul st(0), st(0)
		fld dword ptr [ecx+4]
		fmul st(0), st(0)
		faddp st(1), st(0)
		fld dword ptr [ecx+8]
		fmul st(0), st(0)
		faddp st(1), st(0)
		fsqrt
		fld1
		fdiv st(0), st(1)
		fstp st(1)
		fld st(0)
		fmul dword ptr [ecx]
		fstp dword ptr [edx]
		fld st(0)
		fmul dword ptr [ecx+4]
		fstp dword ptr [edx+4]
		fmul dword ptr [ecx+8]
		fstp dword ptr [edx+8]
	}
#else
	{
		long double inv=1.0L/sqrtl((long double)input[0]*input[0]+(long double)input[1]*input[1]+(long double)input[2]*input[2]);
		output[0]=(float)(inv*input[0]);output[1]=(float)(inv*input[1]);output[2]=(float)(inv*input[2]);
	}
#endif
}
static int G_TCEGibDirToByte(const vec3_t dir) {
	int i, best=0;
	float bestDot=0;
	for(i=0;i<NUMVERTEXNORMALS;++i) {
		const float *normal=bytedirs[i];
#if defined(_MSC_VER) && defined(_M_IX86)
		int better=0;
		__asm {
			mov ecx, normal
			mov edx, dir
			fld dword ptr [ecx]
			fmul dword ptr [edx]
			fld dword ptr [ecx+8]
			fmul dword ptr [edx+8]
			faddp st(1), st(0)
			fld dword ptr [edx+4]
			fmul dword ptr [ecx+4]
			faddp st(1), st(0)
			fcom bestDot
			fnstsw ax
			test ah, 41h
			jnz notBetterGib
			fstp bestDot
			mov better, 1
			jmp doneGibDot
		notBetterGib:
			fstp st(0)
		doneGibDot:
		}
		if(better) best=i;
#else
		long double dot=(long double)dir[0]*normal[0]+(long double)dir[2]*normal[2]+(long double)dir[1]*normal[1];
		if(dot>bestDot) {bestDot=(float)dot;best=i;}
#endif
	}
	return best;
}

void GibEntity( gentity_t *self, int killer ) 
{
	gentity_t *other=&g_entities[killer];
	vec3_t dir;

	VectorClear( dir );
	if (other->inuse) {
		if (other->client) {
			VectorSubtract( self->r.currentOrigin, other->r.currentOrigin, dir );
			G_TCEGibNormalize( dir, dir );
		} else if (!VectorCompare(other->s.pos.trDelta, vec3_origin)) {
			G_TCEGibNormalize( other->s.pos.trDelta, dir );
		}
	}

	G_AddEvent( self, EV_GIB_PLAYER, G_TCEGibDirToByte(dir) );
	self->takedamage = qfalse;
	self->s.eType = ET_INVISIBLE;
	self->r.contents = 0;
}

/*
==================
body_die
==================
*/
void body_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath )
{
	if(self->health <= GIB_HEALTH) {
		GibEntity(self, 0);
	}
}


// these are just for logging, the client prints its own messages
char *modNames[] =
{
	"MOD_UNKNOWN",
	"MOD_MACHINEGUN",
	"MOD_BROWNING",
	"MOD_MG42",
	"MOD_GRENADE",
	"MOD_ROCKET",

	// (SA) modified wolf weap mods
	"MOD_KNIFE",
	"MOD_LUGER",
	"MOD_COLT",
	"MOD_MP40",
	"MOD_THOMPSON",
	"MOD_STEN",
	"MOD_GARAND",
	"MOD_SNOOPERSCOPE",
	"MOD_SILENCER",	//----(SA)	
	"MOD_FG42",
	"MOD_FG42SCOPE",
	"MOD_PANZERFAUST",
	"MOD_GRENADE_LAUNCHER",
	"MOD_FLAMETHROWER",
	"MOD_GRENADE_PINEAPPLE",
	"MOD_CROSS",
	// end

	"MOD_MAPMORTAR",
	"MOD_MAPMORTAR_SPLASH",

	"MOD_KICKED",
	"MOD_GRABBER",

	"MOD_DYNAMITE",
	"MOD_AIRSTRIKE", // JPW NERVE
	"MOD_SYRINGE",	// JPW NERVE
	"MOD_AMMO",	// JPW NERVE
	"MOD_ARTY",	// JPW NERVE

	"MOD_WATER",
	"MOD_SLIME",
	"MOD_LAVA",
	"MOD_CRUSH",
	"MOD_TELEFRAG",
	"MOD_FALLING",
	"MOD_SUICIDE",
	"MOD_TARGET_LASER",
	"MOD_TRIGGER_HURT",
	"MOD_EXPLOSIVE",

	"MOD_CARBINE",
	"MOD_KAR98",
	"MOD_GPG40",
	"MOD_M7",
	"MOD_LANDMINE",
	"MOD_SATCHEL",
	"MOD_TRIPMINE",
	"MOD_SMOKEBOMB",
	"MOD_MOBILE_MG42",
	"MOD_SILENCED_COLT",
	"MOD_GARAND_SCOPE",

	"MOD_CRUSH_CONSTRUCTION",
	"MOD_CRUSH_CONSTRUCTIONDEATH",
	"MOD_CRUSH_CONSTRUCTIONDEATH_NOATTACKER",

	"MOD_K43",
	"MOD_K43_SCOPE",

	"MOD_MORTAR",

	"MOD_AKIMBO_COLT",
	"MOD_AKIMBO_LUGER",
	"MOD_AKIMBO_SILENCEDCOLT",
	"MOD_AKIMBO_SILENCEDLUGER",

	"MOD_SMOKEGRENADE",

	// RF
	"MOD_SWAP_PLACES",

	// OSP -- keep these 2 entries last
	"MOD_SWITCHTEAM",
	"MOD_SPAWNCAMP" /* TC MOD65, qagame200bfeb4 name table. */
};

/*
==================
player_die
==================
*/
void BotRecordTeamDeath( int client );

/* Whole TC controller20057570; numeric gear and condition values are TC IDs. */
void player_die( gentity_t *self, gentity_t *inflictor, gentity_t *attacker, int damage, int meansOfDeath ) {
	int i, killer = ENTITYNUM_WORLD;
	char		*killerName = "<world>";
	gitem_t		*item = NULL;
	gentity_t	*ent;

	//float			timeLived;
	weapon_t	weap = BG_WeaponForMOD( meansOfDeath );

//	G_Printf( "player_die\n" );

	if(attacker == self) {
		if(self->client) {
			self->client->pers.playerStats.suicides++;
			trap_PbStat ( self - g_entities , "suicide" , 
				va ( "%d %d %d" , self->client->sess.sessionTeam , self->client->sess.playerType , weap ) ) ;
		}
	} else if(OnSameTeam( self, attacker )) {
		G_LogTeamKill(	attacker,	weap );
	} else {
		G_LogDeath( self,		weap );
		G_LogKill(	attacker,	weap );

		if( g_gamestate.integer == GS_PLAYING ) {
			if( attacker && attacker->client ) {
				attacker->client->combatState |= (1<<COMBATSTATE_KILLEDPLAYER);
			}
		}
	}

	// RF, record this death in AAS system so that bots avoid areas which have high death rates
	if( !OnSameTeam( self, attacker ) ) {
		BotRecordTeamDeath( self->s.number );

		self->isProp = qfalse;	// were we teamkilled or not?
	} else {
		self->isProp = qtrue;
	}	

	// if we got killed by a landmine, update our map
	if( self->client && meansOfDeath == MOD_LANDMINE ) {
		// if it's an enemy mine, update both teamlists
		/*int teamNum;
		mapEntityData_t	*mEnt;
		mapEntityData_Team_t *teamList;
	
		teamNum = inflictor->s.teamNum % 4;

		teamList = self->client->sess.sessionTeam == TEAM_AXIS ? &mapEntityData[0] : &mapEntityData[1];
		if((mEnt = G_FindMapEntityData(teamList, inflictor-g_entities)) != NULL) {
			G_FreeMapEntityData( teamList, mEnt );
		}

		if( teamNum != self->client->sess.sessionTeam ) {
			teamList = self->client->sess.sessionTeam == TEAM_AXIS ? &mapEntityData[1] : &mapEntityData[0];
			if((mEnt = G_FindMapEntityData(teamList, inflictor-g_entities)) != NULL) {
				G_FreeMapEntityData( teamList, mEnt );
			}
		}*/
		mapEntityData_t	*mEnt;

		if((mEnt = G_FindMapEntityData(&mapEntityData[0], inflictor-g_entities)) != NULL) {
			G_FreeMapEntityData( &mapEntityData[0], mEnt );
		}

		if((mEnt = G_FindMapEntityData(&mapEntityData[1], inflictor-g_entities)) != NULL) {
			G_FreeMapEntityData( &mapEntityData[1], mEnt );
		}
	}

	{
		mapEntityData_t	*mEnt;
		mapEntityData_Team_t *teamList = self->client->sess.sessionTeam == TEAM_AXIS ? &mapEntityData[1] : &mapEntityData[0];	// swapped, cause enemy team

		mEnt = G_FindMapEntityDataSingleClient( teamList, NULL, self->s.number, -1 );
		
		while( mEnt ) {
			if( mEnt->type == ME_PLAYER_DISGUISED ) {
				mapEntityData_t* mEntFree = mEnt;

				mEnt = G_FindMapEntityDataSingleClient( teamList, mEnt, self->s.number, -1 );

				G_FreeMapEntityData( teamList, mEntFree );
			} else {
				mEnt = G_FindMapEntityDataSingleClient( teamList, mEnt, self->s.number, -1 );
			}
		}
	}

	if( self->tankLink ) {
		G_LeaveTank( self, qfalse );

	}

	if( self->client->ps.pm_type == PM_DEAD || g_gamestate.integer == GS_INTERMISSION ) {
		return;
	}

	// OSP - death stats handled out-of-band of G_Damage for external calls
	G_addStats(self, attacker, damage, meansOfDeath);
	// OSP

	self->client->ps.pm_type = PM_DEAD;
	/* Notification after the duplicate-death guard and state transition. */
	TCE_LuaDeath(self->s.number, attacker ? attacker->s.number : ENTITYNUM_WORLD, meansOfDeath);

#ifdef FEATURE_OMNIBOT
    {
        const char *deathName = meansOfDeath >= 0 && meansOfDeath < sizeof(modNames)/sizeof(modNames[0])
            ? modNames[meansOfDeath] : "<unknown>";
        Bot_Event_Death(self->s.number, attacker, deathName);
        if(attacker && attacker->client)
            Bot_Event_KilledSomeone(attacker->s.number, self, deathName);
    }
#endif

	G_AddEvent( self, EV_STOPSTREAMINGSOUND, 0);

	if(attacker) {
		killer = attacker->s.number;
		killerName = (attacker->client) ? attacker->client->pers.netname : "<non_client>";
	}

	if(attacker == 0 || killer < 0 || killer >= MAX_CLIENTS) {
		killer = ENTITYNUM_WORLD;
		killerName = "<world>";
	}

	if(g_gamestate.integer == GS_PLAYING) {
		char *obit;

		if(meansOfDeath < 0 || meansOfDeath >= sizeof(modNames) / sizeof(modNames[0])) {
			obit = "<bad_obituary>";
		} else {
			obit = modNames[meansOfDeath];
		}

        if (attacker && attacker->client && (meansOfDeath == 9 || meansOfDeath == 6 || meansOfDeath == 8)) {
            int gear = self->client->tceLastKillingWeapon;
            G_LogPrintf("Kill: %i %i %i: %s killed %s by %s (%s)\n", killer, self->s.number,
                meansOfDeath, killerName, self->client->pers.netname, obit,
                gear >= 0 && gear < TCE_MAX_WEAPONS ? gearDef.weaponFile[gear] : "");
        } else {
            G_LogPrintf("Kill: %i %i %i: %s killed %s by %s\n", killer, self->s.number,
                meansOfDeath, killerName, self->client->pers.netname, obit);
        }
	}

	// RF, record bot kills
	if (attacker && (attacker->r.svFlags & SVF_BOT) && TCE_INTERNAL_BOT(attacker)) {
		BotRecordKill( attacker->s.number, self->s.number );
	}

	// broadcast the death event to everyone
	ent = G_TempEntity( self->r.currentOrigin, EV_OBITUARY );
	ent->s.eventParm = meansOfDeath;
	ent->s.otherEntityNum = self->s.number;
	ent->s.otherEntityNum2 = killer;
	if (self->client) ent->s.density = self->client->tceLastKillingWeapon;
	ent->r.svFlags = SVF_BROADCAST;	// send to everyone

    /* TC Objective teammates acknowledge only a visible death within1024. */
    if (g_gametype.integer == 5) {
        int nearest = -1;
        float nearestDistance = 1024.f;
        for (i = 0; i < level.numConnectedClients; ++i) {
            gclient_t *client = &level.clients[level.sortedClients[i]];
            vec3_t eye, direction, forward;
            trace_t tr;
            float distance;
            if (client->pers.connected != CON_CONNECTED || client->ps.stats[STAT_HEALTH] <= 0 ||
                client->sess.sessionTeam == TEAM_SPECTATOR ||
                client->sess.sessionTeam != self->client->sess.sessionTeam ||
                client->sess.spectatorClient == self->s.number) continue;
            VectorCopy(client->ps.origin, eye);
            eye[2] += client->ps.viewheight;
            AngleVectors(client->ps.viewangles, forward, NULL, NULL);
            VectorSubtract(self->r.currentOrigin, eye, direction);
            distance = VectorNormalize(direction);
            if (DotProduct(forward, direction) > .71f) {
                trap_Trace(&tr, eye, NULL, NULL, self->r.currentOrigin, client->ps.clientNum, CONTENTS_SOLID);
                if (tr.fraction >= 1.f && distance < nearestDistance) {
                    nearestDistance = distance;
                    nearest = i;
                }
            }
        }
        if (nearest >= 0) G_Voice(&g_entities[level.sortedClients[nearest]], NULL, SAY_TEAM, "ManDown", qfalse);
    }
    if (attacker && attacker->client && g_gametype.integer == 5 && attacker != self && !OnSameTeam(self, attacker)) {
        vec3_t eye, direction, forward;
        trace_t tr;
        VectorCopy(attacker->client->ps.origin, eye);
        eye[2] += attacker->client->ps.viewheight;
        AngleVectors(attacker->client->ps.viewangles, forward, NULL, NULL);
        VectorSubtract(self->r.currentOrigin, eye, direction);
        VectorNormalize(direction);
        if (DotProduct(forward, direction) >= .71f) {
            trap_Trace(&tr, eye, NULL, NULL, self->r.currentOrigin, attacker->client->ps.clientNum, CONTENTS_SOLID);
            if (tr.fraction >= 1.f) G_Voice(attacker, NULL, SAY_TEAM, "EnemyDown", qfalse);
        }
    }

	self->enemy = attacker;

	self->client->ps.persistant[PERS_KILLED]++;

	// JPW NERVE -- if player is holding ticking grenade, drop it
	if ((self->client->ps.grenadeTimeLeft) && (self->s.weapon != 15) && (self->s.weapon != 26) && (self->s.weapon != 27) && (self->s.weapon != 29)) {
		vec3_t launchvel, launchspot;

		launchvel[0] = (float)(rand() & 0x7fff) * 3.0518509447574615e-05f - .5f;
        launchvel[0] += launchvel[0];
        launchvel[1] = (float)(rand() & 0x7fff) * 3.0518509447574615e-05f - .5f;
        launchvel[1] += launchvel[1];
        launchvel[2] = (float)(rand() & 0x7fff) * 3.0518509447574615e-05f;
		VectorScale( launchvel, 160, launchvel );
		VectorCopy(self->r.currentOrigin, launchspot);
		launchspot[2] += 40;
		
		{
			// Gordon: fixes premature grenade explosion, ta bani ;)
			gentity_t *m = fire_grenade(self, launchspot, launchvel, self->s.weapon);
			m->damage = 0;
		}
	}

	if (attacker && attacker->client) {
		if ( attacker == self || OnSameTeam (self, attacker ) ) {

			// DHM - Nerve :: Complaint lodging
			if( attacker != self && level.warmupTime <= 0 && g_gamestate.integer == GS_PLAYING) {
				if( attacker->client->pers.localClient ) {
					trap_SendServerCommand( self-g_entities, "complaint -4" );
				} else {
					if( meansOfDeath != MOD_CRUSH_CONSTRUCTION && meansOfDeath != MOD_CRUSH_CONSTRUCTIONDEATH && meansOfDeath != MOD_CRUSH_CONSTRUCTIONDEATH_NOATTACKER ) {
						if( g_complaintlimit.integer ) {

							if( !(meansOfDeath == MOD_LANDMINE && g_disableComplaints.integer & TKFL_MINES ) &&
								!((meansOfDeath == MOD_ARTY || meansOfDeath == MOD_AIRSTRIKE) && g_disableComplaints.integer & TKFL_AIRSTRIKE ) &&
								!(meansOfDeath == MOD_MORTAR && g_disableComplaints.integer & TKFL_MORTAR ) ) {
								trap_SendServerCommand( self-g_entities, va( "complaint %i", attacker->s.number ) );
								self->client->pers.complaintClient = attacker->s.clientNum;
								self->client->pers.complaintEndTime = level.time + 20500;
							}
						}
					}
				}
			}

            if (g_gametype.integer == 2 || g_gametype.integer == 5 || g_gametype.integer == 7) {
                int penalty = attacker == self && meansOfDeath != 65 ? -1 : -3;
                AddScore(attacker, penalty);
                AddKillScore(attacker, penalty);
            }

		} else {

            if (g_gametype.integer == 2 || g_gametype.integer == 5 || g_gametype.integer == 7) {
                if (level.firstbloodTeam == -1) level.firstbloodTeam = attacker->client->sess.sessionTeam;
                AddScore(attacker, 1);
                AddKillScore(attacker, 1);
            }

			attacker->client->lastKillTime = level.time;
		}
        if ((g_gametype.integer == 2 || g_gametype.integer == 5 || g_gametype.integer == 7) &&
            attacker != self && !OnSameTeam(self, attacker)) {
            AddScore(self, -1);
            AddKillScore(self, -1);
        }

	} else {
		AddScore( self, -1 );

		if (g_gametype.integer == 2 || g_gametype.integer == 5 || g_gametype.integer == 7)
            AddKillScore(self, -1);
	}


	// drop flag regardless
	if (self->client->ps.powerups[PW_REDFLAG]) {
		item = BG_FindItem("Red Flag");
		if (!item)
			item = BG_FindItem("Objective");

		self->client->ps.powerups[PW_REDFLAG] = 0;
	}
	if (self->client->ps.powerups[PW_BLUEFLAG]) {
		item = BG_FindItem("Blue Flag");
		if (!item)
			item = BG_FindItem("Objective");

		self->client->ps.powerups[PW_BLUEFLAG] = 0;
	}

    if (g_gametype.integer == 5) {
        if ((self->client->ps.stats[STAT_TCE_FLAGS] & 0x100) && level.tceVipAssigned &&
            level.tceVipCarrier == self->client->ps.clientNum) {
            level.tceExitRulesNotBefore = level.time + 1000;
            G_Script_ScriptEvent(level.gameManager, "trigger", "vip_eliminated");
        }
        if (g_gametype.integer == 5 && (self->client->ps.stats[STAT_TCE_FLAGS] & 0x400) &&
            level.tceHostageSecured) {
            level.tceExitRulesNotBefore = level.time + 1000;
            G_Script_ScriptEvent(level.gameManager, "trigger", "hostage_eliminated");
        }
    }
    if (self->client->ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) {
        int target = self->client->tceObjectiveEntity;
        self->client->ps.pm_flags &= ~PMF_TCE_OBJECTIVE_ACTION;
        self->client->ps.weaponTime = 100;
        if (target >= 0 && target < MAX_GENTITIES && g_entities[target].s.eType == 65)
            G_Script_ScriptEvent(&g_entities[target], "stopped", "");
    }
    if (item) {
        vec3_t angles, forward, offset, velocity, origin;
        vec3_t mins = {-10.f,-10.f,0.f}, maxs = {10.f,10.f,20.f};
        trace_t tr;
        gentity_t *flag;
        VectorCopy(self->client->ps.viewangles, angles);
        if (angles[PITCH] < -30.f) angles[PITCH] = -30.f;
        else if (angles[PITCH] > 30.f) angles[PITCH] = 30.f;
        angles[YAW] -= 135.f;
        AngleVectors(angles, forward, NULL, NULL);
        VectorScale(forward, 16.f, offset);
        offset[2] += self->client->ps.viewheight * .5f;
        VectorScale(forward, 64.f, velocity);
        velocity[2] = (float)(rand() & 0x7fff) * 3.0518509447574615e-05f * 35.f + velocity[2] + 50.f;
        VectorAdd(self->client->ps.origin, offset, origin);
        trap_Trace(&tr, self->client->ps.origin, mins, maxs, origin, self->s.number, CONTENTS_SOLID);
        flag = LaunchItem(item, tr.endpos, velocity, self->s.number);
        flag->s.density = self->s.otherEntityNum2;
        flag->message = self->message;
        self->s.otherEntityNum2 = 0;
        self->message = NULL;
    }

	Cmd_Score_f( self );		// show scores

	// send updated scores to any clients that are following this one,
	// or they would get stale scoreboards
	for(i=0; i<level.numConnectedClients; i++) {
		gclient_t *client = &level.clients[level.sortedClients[i]];

		if(client->pers.connected != CON_CONNECTED) continue;
		if(client->sess.sessionTeam != TEAM_SPECTATOR) continue;

		if(client->sess.spectatorClient == self->s.number) {
			Cmd_Score_f(g_entities + level.sortedClients[i]);
		}
	}

	self->takedamage = qtrue;	// can still be gibbed
	self->r.contents = CONTENTS_CORPSE;

	//self->s.angles[2] = 0;
	self->s.powerups = 0;
	self->s.loopSound = 0;
	
	self->client->limboDropWeapon = self->s.weapon;
    TossClientItems(self);

	LookAtKiller( self, inflictor, attacker );
	self->client->ps.viewangles[0] = 0;
	self->client->ps.viewangles[2] = 0;
	//VectorCopy( self->s.angles, self->client->ps.viewangles );

//	trap_UnlinkEntity( self );
	self->r.maxs[2] = self->client->ps.crouchMaxZ;	//%	0;			// ydnar: so bodies don't clip into world
	self->client->ps.maxs[2] = self->client->ps.crouchMaxZ;	//%	0;	// ydnar: so bodies don't clip into world
	trap_LinkEntity( self );

	// don't allow respawn until the death anim is done
	// g_forcerespawn may force spawning at some later time
	self->client->respawnTime = level.timeCurrent + 800;
	/* Original client+1324: spawn-camp death delays reinforcement. */
	if (meansOfDeath == 65) self->client->tceRespawnNotBefore = level.timeCurrent + 1300;

	// remove powerups
	memset( self->client->ps.powerups, 0, sizeof(self->client->ps.powerups) );

    if (self->health < -174) {
        GibEntity(self, killer);
    } else {
        /* TC condition8 values are protocol-specific, not SDK impactpoint names. */
        if (self->client->ps.eFlags & EF_PRONE)
            BG_UpdateConditionValue(self->client->ps.clientNum, ANIM_COND_IMPACT_POINT, 2, qtrue);
        else if (self->client->ps.pm_flags & PMF_DUCKED)
            BG_UpdateConditionValue(self->client->ps.clientNum, ANIM_COND_IMPACT_POINT, 4, qtrue);
        else if (self->client->ps.holdable[2] > 80)
            BG_UpdateConditionValue(self->client->ps.clientNum, ANIM_COND_IMPACT_POINT, 1, qtrue);
        else if (self->client->ps.holdable[3] > 80)
            BG_UpdateConditionValue(self->client->ps.clientNum, ANIM_COND_IMPACT_POINT, 3, qtrue);
        self->client->ps.pm_time = BG_AnimScriptEvent(&self->client->ps,
            self->client->pers.character->animModelInfo, ANIM_ET_DEATH, qfalse, qtrue);
        self->client->torsoDeathAnim = self->client->ps.torsoAnim;
        self->client->legsDeathAnim = self->client->ps.legsAnim;
        G_AddEvent(self, EV_DEATH1 + 1, killer);
        self->die = body_die;
    }

	if( meansOfDeath == MOD_MACHINEGUN ) {
		switch( self->client->sess.sessionTeam ) {
			case TEAM_AXIS:
				level.axisMG42Counter = level.time;
				break;
			case TEAM_ALLIES:
				level.alliesMG42Counter = level.time;
				break;
			default:
				break;
		}
	}

	G_FadeItems( self, MOD_SATCHEL );

	CalculateRanks();

}

qboolean IsHeadShotWeapon (int mod) {
	// players are allowed headshots from these weapons
	if (	mod == MOD_LUGER ||
			mod == MOD_COLT ||
			mod == MOD_AKIMBO_COLT ||
			mod == MOD_AKIMBO_LUGER ||
			mod == MOD_AKIMBO_SILENCEDCOLT ||
			mod == MOD_AKIMBO_SILENCEDLUGER ||
			mod == MOD_MP40 ||
			mod == MOD_THOMPSON ||
			mod == MOD_STEN ||
			mod == MOD_GARAND
			
			|| mod == MOD_KAR98
			|| mod == MOD_K43
			|| mod == MOD_K43_SCOPE		
			|| mod == MOD_CARBINE
			|| mod == MOD_GARAND
			|| mod == MOD_GARAND_SCOPE
			|| mod == MOD_SILENCER
			|| mod == MOD_SILENCED_COLT
			|| mod == MOD_FG42
			|| mod == MOD_FG42SCOPE
			)
		return qtrue;

	return qfalse;
}

gentity_t* G_BuildHead(gentity_t *ent) {
	gentity_t* head;
	orientation_t or;			// DHM - Nerve

	head = G_Spawn ();

	if (trap_GetTag( ent->s.number, 0, "tag_head", &or )) {
		G_SetOrigin( head, or.origin );
	} else {
		float height, dest;
		vec3_t v, angles, forward, up, right;

		G_SetOrigin (head, ent->r.currentOrigin); 

		if( ent->client->ps.eFlags & EF_PRONE ) {
			height = ent->client->ps.viewheight - 56;
		} else if( ent->client->ps.pm_flags & PMF_DUCKED ) {	// closer fake offset for 'head' box when crouching
			height = ent->client->ps.crouchViewHeight - 12;
		} else {
			height = ent->client->ps.viewheight;
		}

		// NERVE - SMF - this matches more closely with WolfMP models
		VectorCopy( ent->client->ps.viewangles, angles );
		if ( angles[PITCH] > 180 ) {
			dest = (-360 + angles[PITCH]) * 0.75;
		} else {
			dest = angles[PITCH] * 0.75;
		}
		angles[PITCH] = dest;

		AngleVectors( angles, forward, right, up );
		if( ent->client->ps.eFlags & EF_PRONE ) {
			VectorScale( forward, 24, v );
		} else {
			VectorScale( forward, 5, v );
		}
		VectorMA( v, 18, up, v );

		VectorAdd( v, head->r.currentOrigin, head->r.currentOrigin );
		head->r.currentOrigin[2] += height / 2;
		// -NERVE - SMF
	}

	VectorCopy (head->r.currentOrigin, head->s.origin);
	VectorCopy (ent->r.currentAngles, head->s.angles); 
	VectorCopy (head->s.angles, head->s.apos.trBase);
	VectorCopy (head->s.angles, head->s.apos.trDelta);
	VectorSet (head->r.mins , -6, -6, -2); // JPW NERVE changed this z from -12 to -6 for crouching, also removed standing offset
	VectorSet (head->r.maxs , 6, 6, 10); // changed this z from 0 to 6
	head->clipmask = CONTENTS_SOLID;
	head->r.contents = CONTENTS_SOLID;
	head->parent = ent;
	head->s.eType = ET_TEMPHEAD;

	trap_LinkEntity (head);
	
	return head;
}

gentity_t* G_BuildLeg(gentity_t *ent) {
	gentity_t* leg;
	vec3_t flatforward, org;
	//orientation_t or;			// DHM - Nerve

	if( !(ent->client->ps.eFlags & EF_PRONE) )
		return NULL;

	leg = G_Spawn ();

	AngleVectors( ent->client->ps.viewangles, flatforward, NULL, NULL );
	flatforward[2] = 0;
	VectorNormalizeFast( flatforward );

	org[0] = ent->r.currentOrigin[0] + flatforward[0] * -32;
	org[1] = ent->r.currentOrigin[1] + flatforward[1] * -32;
	org[2] = ent->r.currentOrigin[2] + ent->client->pmext.proneLegsOffset;

	G_SetOrigin( leg, org );

	VectorCopy( leg->r.currentOrigin, leg->s.origin );
	VectorCopy( ent->r.currentAngles, leg->s.angles ); 
	VectorCopy( leg->s.angles, leg->s.apos.trBase );
	VectorCopy( leg->s.angles, leg->s.apos.trDelta );
	VectorCopy( playerlegsProneMins, leg->r.mins );
	VectorCopy( playerlegsProneMaxs, leg->r.maxs );
	leg->clipmask = CONTENTS_SOLID;
	leg->r.contents = CONTENTS_SOLID;
	leg->parent = ent;
	leg->s.eType = ET_TEMPLEGS;

	trap_LinkEntity( leg );
	
	return leg;
}

qboolean IsHeadShot( gentity_t *targ, vec3_t dir, vec3_t point, int mod ) {
	gentity_t	*head;
	trace_t		tr;
	vec3_t		start, end;
	gentity_t	*traceEnt;

	// not a player or critter so bail
	if( !(targ->client) )
		return qfalse;

	if( targ->health <= 0 )
		return qfalse;

	if (!IsHeadShotWeapon (mod) ) {
		return qfalse;
	}

	head = G_BuildHead( targ );
	
	// trace another shot see if we hit the head
	VectorCopy( point, start );
	VectorMA( start, 64, dir, end );
	trap_Trace( &tr, start, NULL, NULL, end, targ->s.number, MASK_SHOT );
		
	traceEnt = &g_entities[ tr.entityNum ];

	if( g_debugBullets.integer >= 3 ) {	// show hit player head bb
		gentity_t *tent;
		vec3_t b1, b2;
		VectorCopy(head->r.currentOrigin, b1);
		VectorCopy(head->r.currentOrigin, b2);
		VectorAdd(b1, head->r.mins, b1);
		VectorAdd(b2, head->r.maxs, b2);
		tent = G_TempEntity( b1, EV_RAILTRAIL );
		VectorCopy(b2, tent->s.origin2);
		tent->s.dmgFlags = 1;

		// show headshot trace
		// end the headshot trace at the head box if it hits
		if( tr.fraction != 1 ) {
			VectorMA(start, (tr.fraction * 64), dir, end);
		}
		tent = G_TempEntity( start, EV_RAILTRAIL );
		VectorCopy(end, tent->s.origin2);
		tent->s.dmgFlags = 0;
	}

	G_FreeEntity( head );

	if( traceEnt == head ) {
		level.totalHeadshots++;		// NERVE - SMF
		return qtrue;
	} else
		level.missedHeadshots++;	// NERVE - SMF

	return qfalse;
}

qboolean IsLegShot( gentity_t *targ, vec3_t dir, vec3_t point, int mod ) {
	float height;
	float theight;
	gentity_t *leg;

	if (!(targ->client))
		return qfalse;

	if (targ->health <= 0)
		return qfalse;

	if(!point) {
		return qfalse;
	}

	if(!IsHeadShotWeapon(mod)) {
		return qfalse;
	}

	leg = G_BuildLeg( targ );

	if( leg ) {
		gentity_t	*traceEnt;
		vec3_t		start, end;
		trace_t		tr;

		// trace another shot see if we hit the legs
		VectorCopy( point, start );
		VectorMA( start, 64, dir, end );
		trap_Trace( &tr, start, NULL, NULL, end, targ->s.number, MASK_SHOT );
			
		traceEnt = &g_entities[ tr.entityNum ];

		if( g_debugBullets.integer >= 3 ) {	// show hit player head bb
			gentity_t *tent;
			vec3_t b1, b2;
			VectorCopy( leg->r.currentOrigin, b1 );
			VectorCopy( leg->r.currentOrigin, b2 );
			VectorAdd( b1, leg->r.mins, b1 );
			VectorAdd( b2, leg->r.maxs, b2 );
			tent = G_TempEntity( b1, EV_RAILTRAIL );
			VectorCopy( b2, tent->s.origin2 );
			tent->s.dmgFlags = 1;

			// show headshot trace
			// end the headshot trace at the head box if it hits
			if( tr.fraction != 1 ) {
				VectorMA( start, (tr.fraction * 64), dir, end );
			}
			tent = G_TempEntity( start, EV_RAILTRAIL );
			VectorCopy( end, tent->s.origin2 );
			tent->s.dmgFlags = 0;
		}

		G_FreeEntity( leg );

		if( traceEnt == leg ) {
			return qtrue;
		}
	} else {
		height = point[2] - targ->r.absmin[2];
		theight = targ->r.absmax[2] - targ->r.absmin[2];

		if(height < (theight * 0.4f)) {
			return qtrue;
		}
	}

	return qfalse;
}

qboolean IsArmShot( gentity_t *targ, gentity_t* ent, vec3_t point, int mod ) {
	vec3_t path, view;
	vec_t dot;

	if (!(targ->client))
		return qfalse;

	if (targ->health <= 0)
		return qfalse;

	if(!IsHeadShotWeapon (mod)) {
		return qfalse;
	}

	VectorSubtract(targ->client->ps.origin, point, path);
	path[2] = 0;

	AngleVectors(targ->client->ps.viewangles, view, NULL, NULL);
	view[2] = 0;

	VectorNormalize(path);

	dot = DotProduct(path, view);

	if(dot > 0.4f || dot < -0.75f ) {
		return qfalse;
	}

	return qtrue;
}

/*
============
G_Damage

targ		entity that is being damaged
inflictor	entity that is causing the damage
attacker	entity that caused the inflictor to damage targ
	example: targ=monster, inflictor=rocket, attacker=player

dir			direction of the attack for knockback
point		point at which the damage is being inflicted, used for headshots
damage		amount of damage being inflicted
knockback	force to be applied against targ as a result of the damage

inflictor, attacker, dir, and point can be NULL for environmental effects

dflags		these flags are used to control how T_Damage works
	DAMAGE_RADIUS			damage was indirect (from a nearby explosion)
	DAMAGE_NO_ARMOR			armor does not protect from this damage
	DAMAGE_NO_KNOCKBACK		do not affect velocity, just view angles
	DAMAGE_NO_PROTECTION	kills godmode, armor, everything
============
*/

#include "tce_damage_controller.inc"


/*
============
CanDamage

Returns qtrue if the inflictor can directly damage the target.  Used for
explosions and melee attacks.
============
*/

void G_RailTrail( vec_t* start, vec_t* end ) {
	gentity_t* temp = G_TempEntity( start, EV_RAILTRAIL );
	VectorCopy( end, temp->s.origin2 );
	temp->s.dmgFlags = 0;
}

#define MASK_CAN_DAMAGE		(CONTENTS_SOLID | CONTENTS_BODY)

qboolean CanDamage (gentity_t *targ, vec3_t origin) {
	vec3_t	dest;
	trace_t	tr;
	vec3_t	midpoint;
	vec3_t offsetmins = { -12.f, -12.f, -12.f };
	vec3_t offsetmaxs = { 12.f, 12.f, 12.f };

	if ( g_newbbox.integer ) {
		VectorSet( offsetmins, -15.f, -15.f, -15.f );
		VectorSet( offsetmaxs, 15.f, 15.f, 15.f );
	}

	// use the midpoint of the bounds instead of the origin, because
	// bmodels may have their origin is 0,0,0
	// Gordon: well, um, just check then...
	if(targ->r.currentOrigin[0] || targ->r.currentOrigin[1] || targ->r.currentOrigin[2]) {
		VectorCopy( targ->r.currentOrigin, midpoint );

		if( targ->s.eType == ET_MOVER ) {
			midpoint[2] += 32;
		}
	} else {
		VectorAdd (targ->r.absmin, targ->r.absmax, midpoint);
		VectorScale (midpoint, 0.5, midpoint);
	}

//	G_RailTrail( origin, dest );

	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, midpoint, ENTITYNUM_NONE, MASK_CAN_DAMAGE );
	if (tr.fraction == 1.0)
		return qtrue;

	if(&g_entities[tr.entityNum] == targ)
		return qtrue;

	if( targ->client ) {
		VectorCopy( targ->client->ps.mins, offsetmins );
		VectorCopy( targ->client->ps.maxs, offsetmaxs );
	}

	// this should probably check in the plane of projection, 
	// rather than in world coordinate
	VectorCopy (midpoint, dest);
	dest[0] += offsetmaxs[0];
	dest[1] += offsetmaxs[1];
	dest[2] += offsetmaxs[2];
	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_CAN_DAMAGE );
	if( tr.fraction == 1 || &g_entities[tr.entityNum] == targ ) {
		return qtrue;
	}

	VectorCopy (midpoint, dest);
	dest[0] += offsetmaxs[0];
	dest[1] += offsetmins[1];
	dest[2] += offsetmaxs[2];
	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_CAN_DAMAGE );
	if( tr.fraction == 1 || &g_entities[tr.entityNum] == targ ) {
		return qtrue;
	}

	VectorCopy (midpoint, dest);
	dest[0] += offsetmins[0];
	dest[1] += offsetmaxs[1];
	dest[2] += offsetmaxs[2];
	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_CAN_DAMAGE );
	if( tr.fraction == 1 || &g_entities[tr.entityNum] == targ ) {
		return qtrue;
	}

	VectorCopy (midpoint, dest);
	dest[0] += offsetmins[0];
	dest[1] += offsetmins[1];
	dest[2] += offsetmaxs[2];
	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_CAN_DAMAGE );
	if( tr.fraction == 1 || &g_entities[tr.entityNum] == targ ) {
		return qtrue;
	}

	// =========================

	VectorCopy (midpoint, dest);
	dest[0] += offsetmaxs[0];
	dest[1] += offsetmaxs[1];
	dest[2] += offsetmins[2];
	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_CAN_DAMAGE );
	if( tr.fraction == 1 || &g_entities[tr.entityNum] == targ ) {
		return qtrue;
	}

	VectorCopy (midpoint, dest);
	dest[0] += offsetmaxs[0];
	dest[1] += offsetmins[1];
	dest[2] += offsetmins[2];
	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_CAN_DAMAGE );
	if( tr.fraction == 1 || &g_entities[tr.entityNum] == targ ) {
		return qtrue;
	}

	VectorCopy (midpoint, dest);
	dest[0] += offsetmins[0];
	dest[1] += offsetmaxs[1];
	dest[2] += offsetmins[2];
	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_CAN_DAMAGE );
	if( tr.fraction == 1 || &g_entities[tr.entityNum] == targ ) {
		return qtrue;
	}

	VectorCopy (midpoint, dest);
	dest[0] += offsetmins[0];
	dest[1] += offsetmins[2];
	dest[2] += offsetmins[2];
	trap_Trace ( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_CAN_DAMAGE );
	if( tr.fraction == 1 || &g_entities[tr.entityNum] == targ ) {
		return qtrue;
	}

	return qfalse;
}

void G_AdjustedDamageVec( gentity_t *ent, vec3_t origin, vec3_t v )
{
	int i;

	if (!ent->r.bmodel)
		VectorSubtract(ent->r.currentOrigin,origin,v); // JPW NERVE simpler centroid check that doesn't have box alignment weirdness
	else {
		for ( i = 0 ; i < 3 ; i++ ) {
			if ( origin[i] < ent->r.absmin[i] ) {
				v[i] = ent->r.absmin[i] - origin[i];
			} else if ( origin[i] > ent->r.absmax[i] ) {
				v[i] = origin[i] - ent->r.absmax[i];
			} else {
				v[i] = 0;
			}
		}
	}
}

/*
============
G_RadiusDamage
============
*/
qboolean G_RadiusDamage( vec3_t origin, gentity_t *inflictor, gentity_t *attacker, float damage, float radius, gentity_t *ignore, int mod ) {
	float		points;
	double		dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	int			i, e;
	qboolean	hitClient = qfalse;
	double		boxradius;
	vec3_t		dest; 
	trace_t		tr;
	vec3_t		midpoint;
	int			flags = DAMAGE_RADIUS;

	if( mod == 45 || mod == 46 ) {
		flags |= DAMAGE_HALF_KNOCKBACK;
	}

	if( g_newbbox.integer ) radius *= 1.25f;

	if( radius < 1 ) {
		radius = 1;
	}

	boxradius = 1.41421356 * (double)radius; // radius * sqrt(2) for bounding box enlargement -- 
	// bounding box was checking against radius / sqrt(2) if collision is along box plane
	for( i = 0 ; i < 3 ; i++ ) {
		mins[i] = origin[i] - boxradius;
		maxs[i] = origin[i] + boxradius;
	}

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for( e = 0 ; e < level.num_entities ; e++ ) {
		g_entities[e].dmginloop = qfalse;
	}

	for( e = 0 ; e < numListedEntities ; e++ ) {
		ent = &g_entities[entityList[ e ]];

		if( ent == ignore ) {
			continue;
		}
		if( !ent->takedamage && ( !ent->dmgparent || !ent->dmgparent->takedamage )) {
			continue;
		}

		G_AdjustedDamageVec( ent, origin, v );

		dist = sqrt( (double)v[0]*v[0] + (double)v[1]*v[1] + (double)v[2]*v[2] );
		if ( dist >= radius ) {
			continue;
		}

		points = damage * ( 1.0 - dist / radius );

		if( CanDamage( ent, origin ) ) {
			if( ent->dmgparent ) {
				ent = ent->dmgparent;
			}

			if( ent->dmginloop ) {
				continue;
			}

			if( AccuracyHit( ent, attacker ) ) {
				hitClient = qtrue;
			}
			VectorSubtract (ent->r.currentOrigin, origin, dir);
			// push the center of mass higher than the origin so players
			// get knocked into the air more
			dir[2] += 24;


			G_Damage( ent, inflictor, mod == 26 ? ent : attacker, dir, origin, (int)points, flags, mod );
		} else {
			VectorAdd( ent->r.absmin, ent->r.absmax, midpoint );
			VectorScale( midpoint, 0.5, midpoint );
			VectorCopy( midpoint, dest );
		
			trap_Trace( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID );
			if( tr.fraction < 1.0 ) {
				VectorSubtract( dest, origin, dest );
				dist = sqrt( (double)dest[0]*dest[0] + (double)dest[1]*dest[1] + (double)dest[2]*dest[2] );
				if( dist < (double)radius * (double)0.2f ) { // closer than 1/4 dist
					if( ent->dmgparent ) {
						ent = ent->dmgparent;
					}

					if( ent->dmginloop ) {
						continue;
					}

					if( AccuracyHit( ent, attacker ) ) {
						hitClient = qtrue;
					}
					VectorSubtract (ent->r.currentOrigin, origin, dir);
					dir[2] += 24;
					G_Damage( ent, inflictor, mod == 26 ? ent : attacker, dir, origin, (int)((double)points*(double)0.1f), flags, mod );
				}
			}
		}
	}
	return hitClient;
}

/*
============
etpro_RadiusDamage
mutation of G_RadiusDamage which lets us selectively damage only clients or only non clients
============
*/
qboolean etpro_RadiusDamage( vec3_t origin, gentity_t *inflictor, gentity_t *attacker, float damage, float radius, gentity_t *ignore, int mod, qboolean clientsonly ) {
	float		points;
	double		dist;
	gentity_t	*ent;
	int			entityList[MAX_GENTITIES];
	int			numListedEntities;
	vec3_t		mins, maxs;
	vec3_t		v;
	vec3_t		dir;
	int			i, e;
	qboolean	hitClient = qfalse;
	double		boxradius;
	vec3_t		dest; 
	trace_t		tr;
	vec3_t		midpoint;
	int			flags = DAMAGE_RADIUS;

	if( mod == 45 || mod == 46 ) { /* TC satchel/landmine MOD values */
		flags |= DAMAGE_HALF_KNOCKBACK;
	}

	if( radius < 1 ) {
		radius = 1;
	}

	boxradius = 1.41421356 * (double)radius; // Windows2005bbf6 keeps the double multiplier through bounds
	// bounding box was checking against radius / sqrt(2) if collision is along box plane
	for( i = 0 ; i < 3 ; i++ ) {
		mins[i] = origin[i] - boxradius;
		maxs[i] = origin[i] + boxradius;
	}

	numListedEntities = trap_EntitiesInBox( mins, maxs, entityList, MAX_GENTITIES );

	for( e = 0 ; e < level.num_entities ; e++ ) {
		g_entities[e].dmginloop = qfalse;
	}

	for( e = 0 ; e < numListedEntities ; e++ ) {
		ent = &g_entities[entityList[ e ]];

		if( ent == ignore ) {
			continue;
		}
		if( !ent->takedamage && ( !ent->dmgparent || !ent->dmgparent->takedamage )) {
			continue;
		}

		if( clientsonly && !ent->client ) {
			continue;
		}
		if( !clientsonly && ent->client ) {
			continue;
		}

		G_AdjustedDamageVec( ent, origin, v );

		dist = sqrt( (double)v[0]*v[0] + (double)v[1]*v[1] + (double)v[2]*v[2] );
		if ( dist >= radius ) {
			continue;
		}

		points = damage * ( 1.0 - dist / radius );

		if( CanDamage( ent, origin ) ) {
			if( ent->dmgparent ) {
				ent = ent->dmgparent;
			}

			if( ent->dmginloop ) {
				continue;
			}

			if( AccuracyHit( ent, attacker ) ) {
				hitClient = qtrue;
			}
			VectorSubtract (ent->r.currentOrigin, origin, dir);
			// push the center of mass higher than the origin so players
			// get knocked into the air more
			dir[2] += 24;

			G_Damage( ent, inflictor, attacker, dir, origin, (int)points, flags, mod );
		} else {
			VectorAdd( ent->r.absmin, ent->r.absmax, midpoint );
			VectorScale( midpoint, 0.5, midpoint );
			VectorCopy( midpoint, dest );
		
			trap_Trace( &tr, origin, vec3_origin, vec3_origin, dest, ENTITYNUM_NONE, MASK_SOLID );
			if( tr.fraction < 1.0 ) {
				VectorSubtract( dest, origin, dest );
				dist = sqrt( (double)dest[0]*dest[0] + (double)dest[1]*dest[1] + (double)dest[2]*dest[2] );
				if( dist < (double)radius * (double)0.2f ) { // closer than 1/4 dist
					if( ent->dmgparent ) {
						ent = ent->dmgparent;
					}

					if( ent->dmginloop ) {
						continue;
					}

					if( AccuracyHit( ent, attacker ) ) {
						hitClient = qtrue;
					}
					VectorSubtract (ent->r.currentOrigin, origin, dir);
					dir[2] += 24;
					G_Damage( ent, inflictor, attacker, dir, origin, (int)((double)points*(double)0.1f), flags, mod );
				}
			}
		}
	}

	return hitClient;
}
