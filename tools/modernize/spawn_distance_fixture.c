/* Actual custom GT7 selector with finite independent occupancy/world services. */
#include "src/game/g_local.h"
#include "src/game/tce_numeric.h"
#include "src/game/tce_team_spawn_precision.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

gentity_t g_entities[MAX_GENTITIES];
level_locals_t level;
vmCvar_t g_newbbox, g_gametype;
vec3_t playerMins = { -14, -14, -24 }, playerMaxs = { 14, 14, 46 };
static gclient_t clients[MAX_CLIENTS];
static int randomValue, randomCalls, checks, failures;
static vec3_t worldMin, worldMax;
static int worldPresent, worldCalls;
#define CHECK(x) do { ++checks; if (!(x)) { ++failures; printf("FAIL %d %s\n", __LINE__, #x); } } while (0)
static int fixtureRand(void) { ++randomCalls; return randomValue; }
#define rand fixtureRand

void QDECL G_Error(const char *format, ...) { (void)format; abort(); }
long double TCE_VectorLengthValue(const vec3_t x)
{
    return sqrtl((long double)x[0]*x[0] + (long double)x[1]*x[1] + (long double)x[2]*x[2]);
}
gentity_t *G_Find(gentity_t *from, int field, const char *name)
{
    int i = from ? (int)(from-g_entities)+1 : 0;
    CHECK(field == FOFS(classname));
    for (; i < level.num_entities; ++i)
        if (g_entities[i].classname && !strcmp(g_entities[i].classname, name)) return &g_entities[i];
    return NULL;
}
void trap_TraceCapsule(trace_t *trace, const vec3_t point, const vec3_t mins,
                       const vec3_t maxs, const vec3_t end, int pass, int mask)
{
    float radius = (maxs[0]-mins[0])*.5f;
    float x = point[0]+(mins[0]+maxs[0])*.5f, y = point[1]+(mins[1]+maxs[1])*.5f;
    float low = point[2]+mins[2]+radius, high = point[2]+maxs[2]-radius;
    float dx = x < worldMin[0] ? worldMin[0]-x : x > worldMax[0] ? x-worldMax[0] : 0;
    float dy = y < worldMin[1] ? worldMin[1]-y : y > worldMax[1] ? y-worldMax[1] : 0;
    float dz = high < worldMin[2] ? worldMin[2]-high : low > worldMax[2] ? low-worldMax[2] : 0;
    ++worldCalls; CHECK(pass == ENTITYNUM_NONE && !(mask & CONTENTS_BODY));
    CHECK(!memcmp(point, end, sizeof(vec3_t)));
    memset(trace, 0, sizeof(*trace)); trace->fraction = 1;
    if (worldPresent && dx*dx+dy*dy+dz*dz < radius*radius) {
        trace->fraction = 0; trace->startsolid = trace->allsolid = qtrue;
    }
}
int trap_EntitiesInBox(const vec3_t low, const vec3_t high, int *list, int capacity)
{
    int i, count = 0;
    for (i = 0; i < level.maxclients && count < capacity; ++i) {
        gentity_t *entity = &g_entities[i]; int axis, overlaps = 1;
        if (!entity->client || !entity->r.linked) continue;
        for (axis = 0; axis < 3; ++axis)
            if (entity->r.currentOrigin[axis]+entity->r.maxs[axis] < low[axis] ||
                entity->r.currentOrigin[axis]+entity->r.mins[axis] > high[axis]) overlaps = 0;
        if (overlaps) list[count++] = i;
    }
    return count;
}

/* ACTUAL_FUNCTIONS */

static void reset(void)
{
    memset(g_entities, 0, sizeof(g_entities)); memset(clients, 0, sizeof(clients));
    memset(&level, 0, sizeof(level)); g_gametype.integer = 7; g_newbbox.integer = 0;
    level.maxclients = MAX_CLIENTS; level.num_entities = MAX_CLIENTS;
    level.numspawntargets = 1; level.tceCustomDMSpawns = 1;
    randomValue = randomCalls = worldPresent = worldCalls = 0;
}
static gentity_t *spot(int index, const char *name, float x)
{
    gentity_t *entity = &g_entities[index];
    entity->inuse = qtrue; entity->s.number = index; entity->classname = (char *)name; entity->spawnflags = 2;
    VectorSet(entity->s.origin, x, 0, 0); VectorCopy(entity->s.origin, entity->r.currentOrigin);
    if (level.num_entities <= index) level.num_entities = index+1;
    return entity;
}
static gentity_t *player(int index, team_t team, float x)
{
    gentity_t *entity = &g_entities[index]; entity->client = &clients[index];
    entity->inuse = entity->r.linked = qtrue; entity->health = 100;
    entity->client->pers.connected = CON_CONNECTED; entity->client->sess.sessionTeam = team;
    entity->client->ps.pm_type = PM_NORMAL; entity->client->ps.stats[STAT_HEALTH] = 100;
    VectorSet(entity->r.mins, -14, -14, -24); VectorSet(entity->r.maxs, 14, 14, 46);
    VectorSet(entity->r.currentOrigin, x, 0, 0); return entity;
}
static gentity_t *choose(team_t team)
{
    return SelectRandomTeamSpawnPoint(1, team, 1, 0, qfalse);
}
static void strictDistanceAndPressure(void)
{
    gentity_t *best;
    reset(); spot(64, "info_player_deathmatch", 64); best = spot(65, "info_player_deathmatch", 1024);
    player(1, TEAM_ALLIES, 0); CHECK(choose(TEAM_AXIS) == best);
    reset(); spot(64, "info_player_deathmatch", 64); best = spot(65, "info_player_deathmatch", 600);
    spot(66, "info_player_deathmatch", 1136); player(1, TEAM_ALLIES, 0); player(2, TEAM_ALLIES, 1200);
    randomValue = 2; CHECK(choose(TEAM_AXIS) == best);
    /* Strict nearest-distance priority deliberately beats CQB pressure alone:
     * x0 has lower total pressure, but x1000 is farther from its nearest enemy. */
    reset(); spot(64, "info_player_deathmatch", 0); best = spot(65, "info_player_deathmatch", 1000);
    player(1, TEAM_ALLIES, 99); player(2, TEAM_ALLIES, 1100); player(3, TEAM_ALLIES, 1101);
    CHECK(choose(TEAM_AXIS) == best);
    /* Equal nearest enemy distance: the second enemy breaks the tie by crowd pressure. */
    reset(); spot(64, "info_player_deathmatch", 100); best = spot(65, "info_player_deathmatch", -100);
    player(1, TEAM_ALLIES, 0); player(2, TEAM_ALLIES, 1000); randomValue = 1;
    CHECK(choose(TEAM_AXIS) == best);
    /* The farthest point occurs after the old256 candidate cutoff. */
    reset(); { int i; for (i = 64; i < 364; ++i) spot(i, "info_player_deathmatch", (float)(i+20)); }
    best = spot(364, "info_player_deathmatch", 6000); player(1, TEAM_ALLIES, 0);
    CHECK(choose(TEAM_AXIS) == best);
    reset(); spot(64, "info_player_deathmatch", 100); best = spot(65, "info_player_deathmatch", 1000);
    player(1, TEAM_AXIS, 0); CHECK(choose(TEAM_ALLIES) == best);
}
static void spatialAxes(void)
{
    int axis, reverse;
    for (axis = 1; axis < 3; ++axis) for (reverse = 0; reverse < 2; ++reverse) {
        gentity_t *best, *enemy;
        team_t team = reverse ? TEAM_ALLIES : TEAM_AXIS;
        reset(); spot(64, "info_player_deathmatch", 120);
        /* Relative to enemy(20,-30,40), the first point is100units along X,
         * while the winning point is200units along Y or Z. */
        VectorSet(g_entities[64].s.origin, 120, -30, 40);
        VectorCopy(g_entities[64].s.origin, g_entities[64].r.currentOrigin);
        best = spot(65, "info_player_deathmatch", 20);
        VectorSet(best->s.origin, 20, -30, 40); best->s.origin[axis] += 200;
        VectorCopy(best->s.origin, best->r.currentOrigin);
        enemy = player(1, reverse ? TEAM_AXIS : TEAM_ALLIES, 20);
        VectorSet(enemy->r.currentOrigin, 20, -30, 40);
        CHECK(choose(team) == best);
    }
}
static void enemyEligibility(void)
{
    int kind;
    for (kind = 0; kind < 9; ++kind) {
        gentity_t *enemy, *first;
        reset(); first = spot(64, "info_player_deathmatch", 100); spot(65, "info_player_deathmatch", 1000);
        enemy = player(1, TEAM_ALLIES, 0); randomValue = 1;
        switch (kind) {
        case 0: enemy->inuse = qfalse; break;
        case 1: enemy->client = NULL; break;
        case 2: enemy->client->pers.connected = CON_DISCONNECTED; break;
        case 3: enemy->health = enemy->client->ps.stats[STAT_HEALTH] = 0; break;
        case 4: enemy->client->sess.sessionTeam = TEAM_SPECTATOR; break;
        case 5: enemy->client->sess.sessionTeam = TEAM_FREE; break;
        case 6: enemy->client->sess.sessionTeam = TEAM_AXIS; break;
        case 7: enemy->client->ps.pm_type = PM_DEAD; break;
        case 8: enemy->client->ps.pm_flags |= PMF_LIMBO; break;
        }
        CHECK(choose(TEAM_AXIS) == first); /* No threat: reservoir rand1 retains the first point. */
    }
    reset(); spot(64, "info_player_deathmatch", 100);
    { gentity_t *best = spot(65, "info_player_deathmatch", 1000);
      player(1, TEAM_ALLIES, 0); player(2, TEAM_AXIS, 900); CHECK(choose(TEAM_AXIS) == best); }
}
static void tiesAndNoEnemies(void)
{
    int enemies, random;
    for (enemies = 0; enemies < 2; ++enemies) for (random = 0; random < 2; ++random) {
        gentity_t *first, *second;
        reset(); first = spot(64, "info_player_deathmatch", -100); second = spot(65, "info_player_deathmatch", 100);
        if (enemies) player(1, TEAM_ALLIES, 0); randomValue = random;
        CHECK(choose(TEAM_AXIS) == (random ? first : second)); CHECK(randomCalls == 1);
    }
}
static void admissionAndFallbacks(void)
{
    int profile, kind;
    for (profile = 0; profile < 2; ++profile) for (kind = 0; kind < 3; ++kind) {
        gentity_t *near, *far;
        reset(); g_newbbox.integer = profile; near = spot(64, "info_player_deathmatch", 100);
        far = spot(65, "info_player_deathmatch", 1000); player(1, TEAM_ALLIES, 0);
        if (kind == 0) { worldPresent = 1; VectorSet(worldMin, 998, -2, -100); VectorSet(worldMax, 1002, 2, 100); }
        else if (kind == 1) player(2, TEAM_AXIS, 1000);
        else far->entstate = STATE_INVISIBLE;
        CHECK(choose(TEAM_AXIS) == near); CHECK(worldCalls > 0);
    }
    /* The occupant's center is outside the candidate box[986,1014], but its
     * linked bounds[1006,1034] overlap it. Candidate must remain excluded. */
    reset(); { gentity_t *near, *occupant;
        near = spot(64, "info_player_deathmatch", 100);
        spot(65, "info_player_deathmatch", 1000); player(1, TEAM_ALLIES, 0);
        occupant = player(2, TEAM_AXIS, 1020);
        CHECK(occupant->r.currentOrigin[0] > 1014);
        CHECK(SpotWouldTelefrag(&g_entities[65]));
        CHECK(choose(TEAM_AXIS) == near);
    }
    /* A wall15units to the right admits old14wide but blocks new16wide hulls. */
    for (profile = 0; profile < 2; ++profile) {
        gentity_t *near, *far;
        reset(); g_newbbox.integer = profile; near = spot(64, "info_player_deathmatch", 100);
        far = spot(65, "info_player_deathmatch", 1000); player(1, TEAM_ALLIES, 0);
        worldPresent = 1; VectorSet(worldMin, 1015, -2, -100); VectorSet(worldMax, 1016, 2, 100);
        CHECK(choose(TEAM_AXIS) == (profile ? near : far));
    }
    for (kind = 0; kind < 5; ++kind) {
        gentity_t *team;
        reset(); spot(64, "info_player_deathmatch", 1000); team = spot(65, "team_CTF_redspawn", 600);
        if (kind == 0) level.tceCustomDMSpawns = 0;
        else if (kind == 1) g_gametype.integer = 2;
        else if (kind == 2) g_gametype.integer = 5;
        else if (kind == 3) player(1, TEAM_ALLIES, 1000);
        else { worldPresent = 1; VectorSet(worldMin, 998, -2, -100); VectorSet(worldMax, 1002, 2, 100); }
        CHECK(choose(TEAM_AXIS) == team);
    }
    reset(); spot(64, "info_player_deathmatch", 1000);
    { gentity_t *hostage = spot(65, "team_hostage_spawn", 600);
      CHECK(SelectRandomTeamSpawnPoint(1, TEAM_AXIS, 1, 0, qtrue) == hostage); }
}
int main(void)
{
    strictDistanceAndPressure(); spatialAxes(); enemyEligibility(); tiesAndNoEnemies(); admissionAndFallbacks();
    printf("GT7 spawn distance: %d checks, %d failures; actual selector/world admission, controlled geometry\n", checks, failures);
    return failures ? 1 : 0;
}
