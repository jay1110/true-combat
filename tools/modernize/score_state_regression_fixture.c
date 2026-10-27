/* Extracted production G_SendScore; controlled transport and class provider. */
#define GAMEDLL
#include "src/game/g_local.h"
#include "src/game/tce_numeric.h"
#include "src/game/tce_bg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#undef vsnprintf

gentity_t g_entities[MAX_GENTITIES];
level_locals_t level;
vmCvar_t g_gametype;
static gclient_t clients[MAX_CLIENTS];
static int checks, failures, cases, failed_cases, sends, destination;
static char packet[1024];
static const char *label;

static void expect(int condition, const char *message)
{
    ++checks;
    if (!condition) { ++failures; fprintf(stderr, "%s (GT%d): %s\n", label, g_gametype.integer, message); }
}
void QDECL Com_Printf(const char *format, ...) { (void)format; }
void QDECL Com_Error(int code, const char *format, ...) { (void)code; (void)format; abort(); }
int Q_vsnprintf(char *dest, int size, const char *format, va_list args)
{ return vsnprintf(dest, (size_t)size, format, args); }
qboolean G_smvLocateEntityInMVList(gentity_t *ent, int id, qboolean remove)
{ (void)ent; (void)id; (void)remove; return qfalse; }
int BG_WolfClassToTCE(int playerClass) { (void)playerClass; return 0; }
void trap_SendServerCommand(int id, const char *command)
{ ++sends; destination = id; Q_strncpyz(packet, command, sizeof(packet)); }

/* ACTUAL_SEND_SCORE */

static void score_case(const char *name, int gametype, int team, int raw,
                       int health, int pm_type, int pm_flags, int intermission,
                       int expected)
{
    int previous_failures = failures, n, values[18];
    char *cursor, *next;
    gclient_t saved;
    label = name; ++cases;
    memset(g_entities, 0, sizeof(g_entities));
    memset(clients, 0, sizeof(clients)); memset(&level, 0, sizeof(level));
    memset(packet, 0, sizeof(packet)); sends = 0; destination = -1;
    level.clients = clients; level.numConnectedClients = 1; level.sortedClients[0] = 1;
    level.time = 120000; level.intermissiontime = intermission;
    level.teamScores[TEAM_AXIS] = 12; level.teamScores[TEAM_ALLIES] = 8;
    g_gametype.integer = gametype;
    g_entities[0].client = &clients[0]; g_entities[1].client = &clients[1];
    g_entities[1].inuse = qtrue; g_entities[1].health = health;
    clients[0].ps.persistant[PERS_TEAM] = team;
    clients[1].sess.sessionTeam = team;
    clients[1].pers.connected = CON_CONNECTED;
    clients[1].ps.persistant[PERS_TEAM] = team;
    clients[1].ps.persistant[PERS_SCORE] = 17;
    clients[1].ps.persistant[PERS_RESPAWNS_LEFT] = raw;
    clients[1].ps.stats[STAT_HEALTH] = health;
    clients[1].ps.pm_type = pm_type; clients[1].ps.pm_flags = pm_flags;
    clients[1].ps.ping = 43; clients[1].sess.kills = 9;
    saved = clients[1];
    /* No client/PVS snapshot is available to this producer or fixture. */
    G_SendScore(&g_entities[0]);
    expect(sends == 1 && destination == 0, "one packet to score requester");
    expect(!strncmp(packet, "sc0 ", 4), "score packet header");
    cursor = packet + 4;
    for (n = 0; n < 18; ++n) {
        values[n] = (int)strtol(cursor, &next, 10);
        if (next == cursor) break;
        cursor = next;
    }
    expect(n == 17, "three header integers plus fourteen row integers");
    if (n == 17) {
        expect(values[0] == 12 && values[1] == 8 && values[2] == 1, "team scores and row count");
        expect(values[3] == 1 && values[4] == 17 && values[5] == 43 && values[6] == 2,
               "client identity, score, ping and elapsed minutes");
        expect(values[9] == expected, "derived respawn field matches current server state");
        expect(values[13] == 9, "extended kills field preserved");
    }
    expect(!memcmp(&saved, &clients[1], sizeof(saved)), "score generation leaves full player state unchanged");
    if (failures != previous_failures) ++failed_cases;
}

int main(void)
{
    int mode;
    score_case("GT7 living observed raw minus2", 7, TEAM_AXIS, -2, 100, PM_NORMAL, 0, 0, -1);
    score_case("GT7 living repeated negative", 7, TEAM_ALLIES, -3, 100, PM_NORMAL, 0, 0, -1);
    score_case("GT7 living unlimited", 7, TEAM_AXIS, -1, 100, PM_NORMAL, 0, 0, -1);
    score_case("GT7 living zero finite", 7, TEAM_AXIS, 0, 100, PM_NORMAL, 0, 0, 0);
    score_case("GT7 living finite counter", 7, TEAM_ALLIES, 3, 100, PM_NORMAL, 0, 0, 3);
    score_case("GT7 zero health unlimited", 7, TEAM_AXIS, -1, 0, PM_NORMAL, 0, 0, -2);
    score_case("GT7 negative health finite", 7, TEAM_ALLIES, 3, -40, PM_NORMAL, 0, 0, -2);
    score_case("GT7 PM_DEAD positive health", 7, TEAM_AXIS, -1, 100, PM_DEAD, 0, 0, -2);
    score_case("GT7 LIMBO followed positive health", 7, TEAM_AXIS, -1, 100, PM_NORMAL, PMF_LIMBO, 0, -2);
    score_case("GT7 zero health zero counter", 7, TEAM_AXIS, 0, 0, PM_NORMAL, 0, 0, -2);
    score_case("GT7 spectator preserves legacy field", 7, TEAM_SPECTATOR, -2, 100, PM_NORMAL, 0, 0, -2);
    score_case("GT7 free preserves legacy field", 7, TEAM_FREE, -2, 100, PM_NORMAL, 0, 0, -2);
    for (mode = 2; mode <= 6; ++mode) {
        if (mode != 2 && mode != 5 && mode != 6) continue;
        score_case("other TC mode observed alive minus2", mode, TEAM_AXIS, -2, 100, PM_NORMAL, 1024, 0, -1);
        score_case("other TC mode alive repeated negative", mode, TEAM_ALLIES, -3, 100, PM_NORMAL, 0, 0, -1);
        score_case("other TC mode alive zero finite", mode, TEAM_AXIS, 0, 100, PM_NORMAL, 0, 0, 0);
        score_case("other TC mode alive finite counter", mode, TEAM_ALLIES, 3, 100, PM_NORMAL, 0, 0, 3);
        score_case("other TC mode PM_DEAD preserves minus2", mode, TEAM_AXIS, -2, 100, PM_DEAD, 0, 0, -2);
        score_case("other TC mode LIMBO preserves minus2", mode, TEAM_AXIS, -2, 100, PM_NORMAL, PMF_LIMBO, 0, -2);
        score_case("other TC mode zero health preserves minus2", mode, TEAM_AXIS, -2, 0, PM_NORMAL, 0, 0, -2);
        score_case("other TC mode spectator preserves minus2", mode, TEAM_SPECTATOR, -2, 100, PM_NORMAL, 0, 0, -2);
        score_case("other TC mode finite dead", mode, TEAM_AXIS, 3, 0, PM_DEAD, 0, 0, mode == 5 ? -2 : 3);
        score_case("other TC mode zero limbo", mode, TEAM_AXIS, 0, 100, PM_NORMAL, PMF_LIMBO, 0, mode == 5 ? 0 : -2);
        score_case("other TC mode zero dead", mode, TEAM_AXIS, 0, 0, PM_DEAD, 0, 0, mode == 5 ? -2 : 0);
        score_case("other TC mode intermission dead", mode, TEAM_AXIS, 0, 0, PM_DEAD, 0, 1, -2);
    }
    printf("score state cases=%d checks=%d failures=%d failed_cases=%d\n", cases, checks, failures, failed_cases);
    return failures ? 1 : 0;
}
