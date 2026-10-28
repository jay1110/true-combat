#include "cg_local.h"
#include "tce_weapon_media.h"
#include "../game/tce_weapon_stats.h"
#include "cg_tce_weapon_stats.h"
#include <limits.h>

typedef struct {
	int sequence, pending, client, time, received, total, rounds, damage[3];
	tce_weaponStatsRow_t rows[TCE_WSTATS_ROWS];
	int ids[TCE_WSTATS_ROWS];
} tceStatsReply_t;
static tceStatsReply_t statsReply, statsCache;
static char statsCacheConfig[MAX_INFO_STRING];
static int statsCacheValid, queuedClient = -1;
static void CG_TCEWeaponStatsWindow(void);

void CG_TCEWeaponStatsCancel(void) { statsReply.pending = 0; statsReply.received = 0; queuedClient = -1; }
void CG_TCEWeaponStatsReset(void) { CG_TCEWeaponStatsCancel(); statsCacheValid = 0; }
static void CG_TCEWeaponStatsSend(int clientNum)
{
	CG_TCEWeaponStatsCancel();
	if (clientNum < 0 || clientNum >= MAX_CLIENTS) return;
	statsReply.sequence = statsReply.sequence == INT_MAX ? 1 : statsReply.sequence + 1;
	statsReply.pending = 1; statsReply.client = clientNum; statsReply.time = cg.time;
	statsReply.total = -1;
	trap_SendClientCommand(va("wstats %d tce1 %d", clientNum, statsReply.sequence));
}
void CG_TCEWeaponStatsUpdate(void)
{
	if (!cg.showStats) return;
	if (statsReply.pending && (unsigned)cg.time - (unsigned)statsReply.time > 2000u) {
		int client = statsReply.client;
		CG_TCEWeaponStatsCancel(); queuedClient = client;
	}
	if (queuedClient >= 0 && cg.statsRequestTime < cg.time) {
		int client = queuedClient;
		cg.statsRequestTime = cg.time + 500;
		CG_TCEWeaponStatsSend(client);
	}
}
void CG_TCEWeaponStatsRequest(int clientNum)
{
	if (clientNum < 0 || clientNum >= MAX_CLIENTS) return;
	if (statsReply.pending && statsReply.client != clientNum) CG_TCEWeaponStatsCancel();
	queuedClient = clientNum;
	if (!cg.statsWindow && statsCacheValid && statsCache.client == clientNum &&
	    (unsigned)cg.time - (unsigned)statsCache.time <= 2000u &&
	    !strcmp(statsCacheConfig, CG_ConfigString(CS_PLAYERS + clientNum))) CG_TCEWeaponStatsWindow();
	CG_TCEWeaponStatsUpdate();
	if (!cg.statsWindow && queuedClient >= 0) {
		CG_createStatsWindow(); cg.windowCurrent = cg.statsWindow;
		CG_printWindow("^7Loading weapon stats...\n");
	}
}
static int CG_StatsNumber(int arg, int *out)
{
	const char *text = CG_Argv(arg);
	unsigned value = 0;
	if (!*text) return 0;
	for (; *text; ++text) {
		unsigned digit = (unsigned char)*text - '0';
		if (digit > 9 || value > ((unsigned)INT_MAX - digit) / 10) return 0;
		value = value * 10 + digit;
	}
	*out = (int)value; return 1;
}
static void CG_TCEWeaponStatsWindow(void)
{
	const tceStatsReply_t *data = &statsCache;
	char name[32], player[MAX_NAME_LENGTH], accuracy[32];
	int i, shown = data->total < 40 ? data->total : 40;
	if (!cg.statsWindow || cg.statsWindow->id != WID_STATS || !cg.statsWindow->inuse) CG_createStatsWindow();
	if (!cg.statsWindow) { cg.showStats = qfalse; return; }
	cg.statsWindow->state = WSTATE_START; cg.statsWindow->time = trap_Milliseconds();
	cg.statsWindow->effects |= WFX_TEXTSIZING;
	CG_removeStrings(cg.statsWindow); cg.statsWindow->lineCount = 0;
	/* Text sizing must start from its bottom anchor again after every reply. */
	cg.statsWindow->y = cg.snap && cg.snap->ps.pm_type == PM_INTERMISSION ? -20 : -100;
	cg.windowCurrent = cg.statsWindow;
	BG_cleanName(cgs.clientinfo[data->client].name, player, sizeof(player), qfalse);
	CG_printWindow(va("Weapon stats: %s\n", player));
	CG_printWindow("Weapon\tAcc %\tHits/Att.\tKills\tDeaths\tHS\n");
	for (i = 0; i < shown; ++i) {
		int id = data->ids[i];
		const tce_weaponStatsRow_t *row = &data->rows[id];
		BG_cleanName(tce_cg_weapons[id].deployMenuShortName, name, sizeof(name), qfalse);
		/* Do not substitute an ET SDK name when this map has no media name. */
		if (!name[0]) Com_sprintf(name, sizeof(name), "Weapon %d", id);
		if (row->attacks && row->hits <= row->attacks) Com_sprintf(accuracy, sizeof(accuracy), "%.1f", 100.0 * row->hits / row->attacks);
		else Q_strncpyz(accuracy, "-", sizeof(accuracy));
		CG_printWindow(va("%s\t%s\t%d/%d\t%d\t%d\t%d\n", name, accuracy,
		    row->hits, row->attacks, row->kills, row->deaths, row->headshots));
	}
	if (!data->total) CG_printWindow("No weapon info available.\n");
	if (shown < data->total) CG_printWindow(va("Showing %d of %d weapons.\n", shown, data->total));
	CG_printWindow(va("Damage: Given %d | Received %d | Team %d\n",
	    data->damage[0], data->damage[1], data->damage[2]));
}
void CG_TCEWeaponStatsParse(void)
{
	int header[TCE_WSTATS_HEADER_ARGS - 1], packet[TCE_WSTATS_PACKET_ROWS][6];
	int argc = trap_Argc(), count, i, j, offset, total;
	if (!statsReply.pending || !cg.showStats ||
	    (unsigned)cg.time - (unsigned)statsReply.time > 2000u) return;
	if (argc < TCE_WSTATS_HEADER_ARGS ||
	    (argc - TCE_WSTATS_HEADER_ARGS) % 6 ||
	    (count = (argc - TCE_WSTATS_HEADER_ARGS) / 6) > TCE_WSTATS_PACKET_ROWS) goto invalid;
	for (i = 0; i < TCE_WSTATS_HEADER_ARGS - 1; ++i)
		if (!CG_StatsNumber(i + 1, &header[i])) goto invalid;
	/* Ignore unrelated/stale transactions without destroying the pending one. */
	if (header[0] != statsReply.sequence || header[1] != statsReply.client) return;
	offset = header[2]; total = header[3];
	if (total >= TCE_WSTATS_ROWS || offset != statsReply.received || offset > total ||
	    count != (total - offset < TCE_WSTATS_PACKET_ROWS ? total - offset : TCE_WSTATS_PACKET_ROWS) ||
	    (statsReply.total >= 0 && (statsReply.total != total || statsReply.rounds != header[4] ||
	     memcmp(statsReply.damage, &header[5], sizeof(statsReply.damage))))) goto invalid;
	for (i = 0; i < count; ++i) {
		for (j = 0; j < 6; ++j)
			if (!CG_StatsNumber(TCE_WSTATS_HEADER_ARGS + i * 6 + j, &packet[i][j])) goto invalid;
		if (packet[i][0] < 1 || packet[i][0] >= TCE_WSTATS_ROWS ||
		    (i ? packet[i][0] <= packet[i - 1][0] : offset && packet[i][0] <= statsReply.ids[offset - 1]) ||
		    packet[i][5] > packet[i][2]) goto invalid;
	}
	if (!offset) {
		memset(statsReply.rows, 0, sizeof(statsReply.rows)); statsReply.total = total;
		statsReply.rounds = header[4]; memcpy(statsReply.damage, &header[5], sizeof(statsReply.damage));
	}
	for (i = 0; i < count; ++i) {
		tce_weaponStatsRow_t *row = &statsReply.rows[packet[i][0]];
		statsReply.ids[offset + i] = packet[i][0];
		row->attacks = packet[i][1]; row->hits = packet[i][2]; row->kills = packet[i][3];
		row->deaths = packet[i][4]; row->headshots = packet[i][5];
	}
	statsReply.received += count;
	if (statsReply.received == total) {
		const char *config = CG_ConfigString(CS_PLAYERS + statsReply.client);
		statsReply.pending = 0; statsCache = statsReply; statsCache.time = cg.time;
		/* Cache only a complete player config; never reuse a truncated identity. */
		statsCacheValid = strlen(config) < sizeof(statsCacheConfig);
		if (statsCacheValid) Q_strncpyz(statsCacheConfig, config, sizeof(statsCacheConfig));
		CG_TCEWeaponStatsWindow();
	}
	return;
invalid:
	CG_TCEWeaponStatsCancel();
}
