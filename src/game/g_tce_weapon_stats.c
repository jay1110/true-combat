#include "g_local.h"
#include "tce_bg.h"
#include "g_tce_weapon_stats.h"
#include <limits.h>

typedef struct {
	int initialized, connectTime, requestTime, requested;
	unsigned generation, serial, hitToken[TCE_WSTATS_ROWS], headToken[TCE_WSTATS_ROWS];
	g_tceStatsShot_t active;
	tce_weaponStatsRow_t row[TCE_WSTATS_ROWS];
} tce_clientWeaponStats_t;
typedef struct {
	int valid, owner, connectTime, weapon, trajectoryTime, hit, head;
} tce_projectileWeaponStats_t;
static tce_clientWeaponStats_t weaponStats[MAX_CLIENTS];
static tce_projectileWeaponStats_t projectileStats[MAX_GENTITIES];
static unsigned statsGeneration;

static void TCE_StatsIncrement(int *value) { if (*value < INT_MAX) ++*value; }
static tce_clientWeaponStats_t *TCE_StatsClient(gentity_t *ent)
{
	tce_clientWeaponStats_t *stats;
	if (!ent || !ent->client || ent->s.number < 0 || ent->s.number >= MAX_CLIENTS ||
	    ent != &g_entities[ent->s.number] || ent->client->pers.connected != CON_CONNECTED)
		return NULL;
	stats = &weaponStats[ent->s.number];
	if (!stats->initialized || stats->connectTime != ent->client->pers.connectTime) {
		memset(stats, 0, sizeof(*stats)); stats->initialized = 1;
		if (++statsGeneration == 0) ++statsGeneration;
		stats->generation = statsGeneration;
		stats->connectTime = ent->client->pers.connectTime;
	}
	return stats;
}
static int TCE_StatsWeapon(int weapon)
{
	return weapon > 0 && weapon < TCE_WSTATS_ROWS && gearDef.weaponFile[weapon][0] && weaponDef[weapon].parsed;
}
void G_TCEWeaponStatsResetAll(void)
{
	memset(weaponStats, 0, sizeof(weaponStats)); memset(projectileStats, 0, sizeof(projectileStats));
}
void G_TCEWeaponStatsReset(int clientNum)
{
	int i;
	if (clientNum < 0 || clientNum >= MAX_CLIENTS) return;
	memset(&weaponStats[clientNum], 0, sizeof(weaponStats[clientNum]));
	for (i = 0; i < MAX_GENTITIES; ++i)
		if (projectileStats[i].valid && projectileStats[i].owner == clientNum)
			projectileStats[i].valid = 0;
}
g_tceStatsShot_t G_TCEWeaponStatsBegin(gentity_t *ent, int weapon)
{
	g_tceStatsShot_t previous = { 0, 0, 0 };
	tce_clientWeaponStats_t *stats = TCE_StatsClient(ent);
	if (!stats) return previous;
	previous = stats->active; stats->active.token = 0;
	if (g_gamestate.integer != GS_PLAYING || !TCE_StatsWeapon(weapon) ||
	    (ent->client->sess.sessionTeam != TEAM_AXIS && ent->client->sess.sessionTeam != TEAM_ALLIES)) return previous;
	if (++stats->serial == 0) {
		++stats->serial; memset(stats->hitToken, 0, sizeof(stats->hitToken));
		memset(stats->headToken, 0, sizeof(stats->headToken));
	}
	stats->active.token = stats->serial; stats->active.weapon = weapon;
	stats->active.generation = stats->generation;
	TCE_StatsIncrement(&stats->row[weapon].attacks);
	return previous;
}
void G_TCEWeaponStatsEnd(gentity_t *ent, g_tceStatsShot_t previous)
{
	tce_clientWeaponStats_t *stats = TCE_StatsClient(ent);
	if (stats && (!previous.token || previous.generation == stats->generation)) stats->active = previous;
}
void G_TCEWeaponStatsDamage(gentity_t *target, gentity_t *inflictor,
    gentity_t *attacker, int mod, int take, int wasAlive, int head)
{
	tce_clientWeaponStats_t *stats, *victim;
	tce_projectileWeaponStats_t *projectile = NULL;
	int weapon, *hitSeen = NULL, *headSeen = NULL;
	unsigned token;
	if (!wasAlive || take <= 0 || g_gamestate.integer != GS_PLAYING || mod == 65 ||
	    target == attacker || !(stats = TCE_StatsClient(attacker)) ||
	    !(victim = TCE_StatsClient(target)) ||
	    target->client->ps.pm_type == PM_DEAD ||
	    (target->client->sess.sessionTeam != TEAM_AXIS && target->client->sess.sessionTeam != TEAM_ALLIES) ||
	    (attacker->client->sess.sessionTeam != TEAM_AXIS && attacker->client->sess.sessionTeam != TEAM_ALLIES) ||
	    OnSameTeam(target, attacker)) return;
	/* Bullets/knife are synchronous inside an accepted FireWeapon call.
	 * Never attribute an unrelated later G_Damage to the last held weapon. */
	weapon = stats->active.weapon; token = stats->active.token;
	if (inflictor != attacker) {
		/* fire_grenade stores the launch ID, owner and parent. Explosion may
		 * already have changed eType to ET_GENERAL; those identities remain. */
		if (!inflictor || inflictor->s.number < MAX_CLIENTS || inflictor->s.number >= MAX_GENTITIES ||
		    inflictor != &g_entities[inflictor->s.number] || inflictor->parent != attacker ||
		    inflictor->r.ownerNum != attacker->s.number ||
		    (mod != inflictor->methodOfDeath && mod != inflictor->splashMethodOfDeath)) return;
		weapon = inflictor->s.weapon;
		if (!TCE_StatsWeapon(weapon)) return;
		projectile = &projectileStats[inflictor->s.number];
		if (!projectile->valid || projectile->owner != attacker->s.number ||
		    projectile->connectTime != stats->connectTime || projectile->weapon != weapon ||
		    projectile->trajectoryTime != inflictor->s.pos.trTime) {
			memset(projectile, 0, sizeof(*projectile)); projectile->valid = 1;
			projectile->owner = attacker->s.number; projectile->connectTime = stats->connectTime;
			projectile->weapon = weapon; projectile->trajectoryTime = inflictor->s.pos.trTime;
		}
		hitSeen = &projectile->hit; headSeen = &projectile->head;
	} else if (!token || !TCE_StatsWeapon(weapon) || weaponDef[weapon].mod != mod) return;
	if (projectile ? !*hitSeen : stats->hitToken[weapon] != token) {
		TCE_StatsIncrement(&stats->row[weapon].hits);
		if (projectile) *hitSeen = 1; else stats->hitToken[weapon] = token;
	}
	if (head && (projectile ? !*headSeen : stats->headToken[weapon] != token)) {
		TCE_StatsIncrement(&stats->row[weapon].headshots);
		if (projectile) *headSeen = 1; else stats->headToken[weapon] = token;
	}
	if (target->health <= 0) {
		TCE_StatsIncrement(&stats->row[weapon].kills); TCE_StatsIncrement(&victim->row[weapon].deaths);
	}
}
static int TCE_StatsNumber(const char *text, int *out)
{
	unsigned value = 0;
	if (!text || !*text) return 0;
	for (; *text; ++text) {
		unsigned digit = (unsigned char)*text - '0';
		if (digit > 9 || value > ((unsigned)INT_MAX - digit) / 10) return 0;
		value = value * 10 + digit;
	}
	*out = (int)value; return 1;
}
qboolean G_TCEWeaponStatsRequest(gentity_t *ent)
{
	char text[64], packet[MAX_STRING_CHARS];
	int target, sequence, total = 0, ids[TCE_WSTATS_ROWS], offset, i;
	tce_clientWeaponStats_t *requester, *stats;
	trap_Argv(2, text, sizeof(text));
	if (strcmp(text, "tce1")) return qfalse;
	/* A recognized malformed request must not fall through to the old path. */
	if (trap_Argc() != 4 || !(requester = TCE_StatsClient(ent)) || (ent->r.svFlags & SVF_BOT)) return qtrue;
	trap_Argv(1, text, sizeof(text)); if (!TCE_StatsNumber(text, &target) || target >= MAX_CLIENTS) return qtrue;
	trap_Argv(3, text, sizeof(text)); if (!TCE_StatsNumber(text, &sequence) || sequence < 1) return qtrue;
	if (!(stats = TCE_StatsClient(&g_entities[target]))) return qtrue;
	/* Same own/follow restriction as the popup; no arbitrary client scrape. */
	if (target != ent->s.number && !(ent->client->sess.sessionTeam == TEAM_SPECTATOR &&
	    ent->client->sess.spectatorState == SPECTATOR_FOLLOW && ent->client->sess.spectatorClient == target) &&
	    !(ent->client->sess.sessionTeam == TEAM_SPECTATOR && target < 32 &&
	      (ent->client->pers.mvReferenceList & (1u << target)))) return qtrue;
	if (requester->requested && (unsigned)level.time - (unsigned)requester->requestTime < 500u) return qtrue;
	requester->requested = 1; requester->requestTime = level.time;
	for (i = 1; i < TCE_WSTATS_ROWS; ++i) {
		tce_weaponStatsRow_t *r = &stats->row[i];
		if (r->attacks || r->hits || r->kills || r->deaths || r->headshots) ids[total++] = i;
	}
	for (offset = 0; offset < total || offset == 0; offset += TCE_WSTATS_PACKET_ROWS) {
		gclient_t *client = g_entities[target].client;
		Com_sprintf(packet, sizeof(packet), "tws1 %d %d %d %d %d %d %d %d", sequence, target, offset, total,
		    client->sess.rounds < 0 ? 0 : client->sess.rounds,
		    client->sess.damage_given < 0 ? 0 : client->sess.damage_given,
		    client->sess.damage_received < 0 ? 0 : client->sess.damage_received,
		    client->sess.team_damage < 0 ? 0 : client->sess.team_damage);
		for (i = offset; i < total && i < offset + TCE_WSTATS_PACKET_ROWS; ++i) {
			tce_weaponStatsRow_t *r = &stats->row[ids[i]];
			Q_strcat(packet, sizeof(packet), va(" %d %d %d %d %d %d", ids[i], r->attacks, r->hits, r->kills, r->deaths, r->headshots));
		}
		trap_SendServerCommand(ent->s.number, packet);
		if (!total) break;
	}
	return qtrue;
}
