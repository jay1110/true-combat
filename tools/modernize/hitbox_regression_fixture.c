/* Engine services for production rewind and active TC damage-volume tests. */
#include "src/game/g_local.h"
#include "src/game/tce_numeric.h"
#include "src/game/tce_pose_precision.h"
#include "src/game/tce_damage.h"
#include "src/game/tce_damage_trace.h"
#include "src/game/g_tce_hit_debug.h"
#include <stdio.h>
#include <math.h>
#include <float.h>

gentity_t g_entities[MAX_GENTITIES];
level_locals_t level;
vmCvar_t g_gamestate, g_debugDamage, g_debugBullets;
void G_TCEHitDebugShot(gentity_t *shooter,int layer) {(void)shooter;(void)layer;}
int G_TCEHitDebugLogAllowed(void) {return 0;}
int G_TCEHitDebugTarget(int client) {(void)client;return 1;}
void G_TCEHitDebugTrace(const char *stage,int shooter,int target,int region,const trace_t *tr,const vec3_t start,const vec3_t end)
{(void)stage;(void)shooter;(void)target;(void)region;(void)tr;(void)start;(void)end;}
static gclient_t testClients[MAX_CLIENTS];
static int failures, tests, traceCalls, spawnCalls, links, partLinks;
static vec3_t firstVolume;
static gentity_t debugEntity;
#define CHECK(c) do { ++tests; if (!(c)) { ++failures; printf("FAIL line%d: %s\n", __LINE__, #c); } } while (0)
static int closeFloat(float a, float b) { return fabs((double)a - b) < .0003; }

void QDECL G_Printf(const char *format, ...) { (void)format; }
void trap_LinkEntity(gentity_t *ent)
{
    ++links; ent->r.linked = qtrue;
    if (ent == &g_entities[70]) {
        if (!(partLinks++ % 3)) { VectorCopy(ent->r.currentOrigin, firstVolume); }
    }
}
gentity_t *G_Spawn(void) { gentity_t *ent = &g_entities[70]; ++spawnCalls; memset(ent, 0, sizeof(*ent)); ent->s.number = 70; ent->inuse = qtrue; return ent; }
void G_FreeEntity(gentity_t *ent) { ent->inuse = ent->r.linked = qfalse; }
void G_SetOrigin(gentity_t *ent, vec3_t point) { VectorCopy(point, ent->r.currentOrigin); VectorCopy(point, ent->s.origin); }
gentity_t *G_TempEntity(vec3_t point, int event) { (void)point; (void)event; return &debugEntity; }
int trap_Milliseconds(void) { return level.time; }
qboolean OnSameTeam(gentity_t *a, gentity_t *b) { return a->client && b->client && a->client->sess.sessionTeam == b->client->sess.sessionTeam; }
void G_LogRegionHit(gentity_t *ent, hitRegion_t region) { ++ent->client->pers.playerStats.hitRegions[region]; }
void AngleVectors(const vec3_t a, vec3_t forward, vec3_t right, vec3_t up)
{
    double p = a[0] * (M_PI / 180.0), y = a[1] * (M_PI / 180.0), r = a[2] * (M_PI / 180.0);
    double sp = sin(p), cp = cos(p), sy = sin(y), cy = cos(y), sr = sin(r), cr = cos(r);
    if (forward) { VectorSet(forward, (float)(cp * cy), (float)(cp * sy), (float)-sp); }
    if (right) { VectorSet(right, (float)(-sr * sp * cy + cr * sy), (float)(-sr * sp * sy - cr * cy), (float)(-sr * cp)); }
    if (up) { VectorSet(up, (float)(cr * sp * cy + sr * sy), (float)(cr * sp * sy - sr * cy), (float)(cr * cp)); }
}
vec_t VectorLength(const vec3_t v) { return (float)sqrt((double)v[0]*v[0] + (double)v[1]*v[1] + (double)v[2]*v[2]); }
void VectorNormalizeFast(vec3_t v) { float n = VectorLength(v); if (n) { VectorScale(v, 1.0f / n, v); } }

static int rayBox(const vec3_t start, const vec3_t end, const vec3_t mins,
                  const vec3_t maxs, float *fraction)
{
    double nearPoint = 0, farPoint = 1; int axis;
    for (axis = 0; axis < 3; ++axis) {
        double d = end[axis] - start[axis], a, b, swap;
        if (!d) { if (start[axis] < mins[axis] || start[axis] > maxs[axis]) return 0; continue; }
        a = (mins[axis] - start[axis]) / d; b = (maxs[axis] - start[axis]) / d;
        if (a > b) { swap = a; a = b; b = swap; }
        if (a > nearPoint) nearPoint = a;
        if (b < farPoint) farPoint = b;
        if (nearPoint > farPoint) return 0;
    }
    *fraction = (float)nearPoint; return nearPoint <= 1 && farPoint >= 0;
}
void trap_Trace(trace_t *tr, const vec3_t start, const vec3_t mins,
                 const vec3_t maxs, const vec3_t end, int ignore, int mask)
{
    int id, axis; (void)mins; (void)maxs; ++traceCalls;
    memset(tr, 0, sizeof(*tr)); tr->fraction = 1; tr->entityNum = ENTITYNUM_WORLD;
    for (id = 0; id < MAX_GENTITIES; ++id) {
        gentity_t *ent = &g_entities[id]; float fraction; vec3_t low, high;
        if (id == ignore || !ent->inuse || !ent->r.linked || !(ent->r.contents & mask)) continue;
        for (axis = 0; axis < 3; ++axis) { low[axis] = ent->r.currentOrigin[axis] + ent->r.mins[axis]; high[axis] = ent->r.currentOrigin[axis] + ent->r.maxs[axis]; }
        if (rayBox(start, end, low, high, &fraction) && fraction < tr->fraction) { tr->fraction = fraction; tr->entityNum = id; }
    }
    for (axis = 0; axis < 3; ++axis) { tr->endpos[axis] = start[axis] + tr->fraction * (end[axis] - start[axis]); }
}

/* PRODUCTION_FUNCTIONS */

static void initClient(int id)
{
    gentity_t *ent = &g_entities[id]; int i;
    memset(ent, 0, sizeof(*ent)); memset(&testClients[id], 0, sizeof(testClients[id]));
    ent->s.number = id; ent->client = &testClients[id]; ent->inuse = qtrue;
    ent->health = 100; ent->r.linked = qtrue; ent->r.contents = CONTENTS_BODY;
    ent->client->sess.sessionTeam = id ? TEAM_AXIS : TEAM_ALLIES;
    ent->client->ps.pm_type = PM_NORMAL; ent->client->ps.crouchMaxZ = 24;
    ent->client->ps.weapon = 30; ent->client->ps.leanf = 25;
    VectorSet(ent->r.mins, -14, -14, -24); VectorSet(ent->r.maxs, 14, 14, 46);
    VectorSet(ent->r.currentOrigin, 200, 0, 0);
    ent->client->topMarker = 1;
    for (i = 0; i < MAX_CLIENT_MARKERS; ++i) {
        clientMarker_t *m = &ent->client->clientMarkers[i];
        VectorCopy(ent->r.mins, m->mins); VectorCopy(ent->r.maxs, m->maxs);
        VectorSet(m->origin, i == 1 ? 200 : 100, 0, 0);
        m->time = i == 1 ? 200 : i == 0 ? 100 : -1000;
        m->serverTime = m->time; m->lean = i == 1 ? 25 : -25; m->weapon = i == 1 ? 30 : 1;
    }
}
static void reset(void)
{
    memset(g_entities, 0, sizeof(g_entities)); memset(&level, 0, sizeof(level));
    level.time = 200; level.previousTime = 150; level.frameTime = 50;
    g_gamestate.integer = GS_PLAYING; traceCalls = spawnCalls = links = partLinks = 0;
    initClient(0); initClient(1);
    g_entities[0].r.linked = qfalse;
}
static int applyBodyFrom(gentity_t *target, gentity_t *attacker, const vec3_t point, const vec3_t direction)
{
#ifdef __linux__
    int status;
#ifdef HAS_BODY_REGION_POLICY
    return TCE_ApplyBodyDamage(target, attacker, MOD_MP40, direction, point, 10, qfalse, &status);
#elif defined(HAS_BODY_STATS)
    return TCE_ApplyBodyDamage(target, attacker, MOD_MP40, direction, point, 10, &status);
#else
    return TCE_ApplyBodyDamage(target, direction, point, 10, &status);
#endif
#else
#ifdef HAS_BODY_REGION_POLICY
    return TCE_ApplyBodyDamage(target, attacker, MOD_MP40, direction, point, 10, qfalse);
#elif defined(HAS_BODY_STATS)
    return TCE_ApplyBodyDamage(target, attacker, MOD_MP40, direction, point, 10);
#else
    return TCE_ApplyBodyDamage(target, direction, point, 10);
#endif
#endif
}
static int applyBody(gentity_t *target, const vec3_t point, const vec3_t direction)
{
    return applyBodyFrom(target, &g_entities[0], point, direction);
}

static void chronological(void)
{
    static const int times[] = { 50, 100, 101, 110, 125, 150, 175, 190, 199, 200, 250 };
    unsigned i;
    for (i = 0; i < sizeof(times)/sizeof(times[0]); ++i) {
        int expected = times[i] < 100 ? 100 : times[i] > 200 ? 200 : times[i];
        reset(); G_TimeShiftClient(&g_entities[1], times[i]);
        CHECK(closeFloat(g_entities[1].r.currentOrigin[0], (float)expected));
        G_UnTimeShiftClient(&g_entities[1]); CHECK(closeFloat(g_entities[1].r.currentOrigin[0], 200));
    }
}
static void historicalBody(void)
{
    vec3_t point = { 80, 18.055555f, 38.42857f }, direction = { 1, 0, 0 };
    reset(); G_TimeShiftClient(&g_entities[1], 100);
    CHECK(applyBody(&g_entities[1], point, direction) > 0);
    CHECK(closeFloat(firstVolume[0], 110) && closeFloat(firstVolume[1], 18.055555f) && closeFloat(firstVolume[2], 38.42857f));
    G_UnTimeShiftClient(&g_entities[1]);
    CHECK(closeFloat(g_entities[1].client->ps.leanf, 25) && g_entities[1].client->ps.weapon == 30);
    CHECK(traceCalls == 3);
}
static void historicalInputs(void)
{
    gentity_t *target; clientMarker_t capture, old;
    reset(); target = &g_entities[1]; memset(&capture, 0, sizeof(capture));
    G_CaptureClientMarker(target, &capture, 175);
    CHECK(capture.lean == 25 && capture.weapon == 30 && capture.time == 175);
    target->client->clientMarkers[0].eFlags = EF_PRONE;
    target->client->clientMarkers[0].maxs[2] = 20;
    target->client->clientMarkers[0].angles[1] = 10;
    target->client->clientMarkers[1].angles[1] = 30;
    target->client->clientMarkers[0].viewangles[1] = 20;
    target->client->clientMarkers[1].viewangles[1] = 40;
    target->client->ps.pm_flags = PMF_DUCKED;
    target->client->ps.eFlags = EF_HEADSHOT;
    target->client->ps.stats[STAT_TCE_WEAPON_FLAGS] = 0x20 | 0x80;
    G_TimeShiftClient(target, 125);
    CHECK(closeFloat(target->client->tceRewind.lean, -12.5f) && target->client->tceRewind.weapon == 1);
    CHECK(closeFloat(target->r.currentAngles[1], 15) && closeFloat(target->client->ps.viewangles[1], 25));
    CHECK(closeFloat(target->r.maxs[2], 20) && (target->client->ps.eFlags & EF_PRONE));
    CHECK(!(target->client->ps.pm_flags & PMF_DUCKED) && (target->client->ps.eFlags & EF_HEADSHOT));
    CHECK(target->client->ps.stats[STAT_TCE_WEAPON_FLAGS] == 0x80);
    G_UnTimeShiftClient(target);
    CHECK((target->client->ps.pm_flags & PMF_DUCKED) && !(target->client->ps.eFlags & EF_PRONE));
    CHECK(target->client->ps.stats[STAT_TCE_WEAPON_FLAGS] == (0x20 | 0x80));
    reset(); target = &g_entities[1]; old = target->client->clientMarkers[0];
    target->client->clientMarkers[0] = target->client->clientMarkers[1];
    target->client->clientMarkers[1].time = -1000;
    target->client->clientMarkers[MAX_CLIENT_MARKERS - 1] = old; target->client->topMarker = 0;
    G_TimeShiftClient(target, 110); CHECK(closeFloat(target->r.currentOrigin[0], 110));
    G_UnTimeShiftClient(target); CHECK(closeFloat(target->r.currentOrigin[0], 200));
}
static int sameAngle(float actual, float expected)
{
    double difference = fmod((double)actual - expected, 360.0);
    if (difference > 180) difference -= 360;
    if (difference < -180) difference += 360;
    return fabs(difference) < .0003;
}
static void movingAngleWrap(void)
{
    static const float endpoints[][2] = { { 0, 90 }, { 90, 0 }, { 359, 1 }, { 1, 359 } };
    static const int latencies[] = { 50, 100, 150 };
    int pair, axis, latency;
    for (pair = 0; pair < 4; ++pair) for (axis = 0; axis < 3; ++axis) for (latency = 0; latency < 3; ++latency) {
        gentity_t *target;
        clientMarker_t *older, *newer;
        float delta = endpoints[pair][1] - endpoints[pair][0], expected;
        int query = 300 - latencies[latency];
        reset(); level.time = 300; target = &g_entities[1];
        older = &target->client->clientMarkers[0]; newer = &target->client->clientMarkers[1];
        older->time = 100; newer->time = 300;
        VectorSet(older->origin, 100.375f, -19.625f, 7.875f);
        VectorSet(newer->origin, 172.375f, 16.375f, 7.875f);
        VectorClear(older->angles); VectorClear(newer->angles);
        VectorClear(older->viewangles); VectorClear(newer->viewangles);
        older->angles[axis] = older->viewangles[axis] = endpoints[pair][0];
        newer->angles[axis] = newer->viewangles[axis] = endpoints[pair][1];
        older->pm_flags = 0; newer->pm_flags = PMF_DUCKED;
        if (delta > 180) delta -= 360; else if (delta < -180) delta += 360;
        expected = endpoints[pair][0] + (query - 100) * .005f * delta;
        G_TimeShiftClient(target, query);
        CHECK(sameAngle(target->r.currentAngles[axis], expected));
        CHECK(sameAngle(target->client->ps.viewangles[axis], expected));
        CHECK(closeFloat(target->r.currentOrigin[0], 100.375f + (query - 100) * .36f));
        CHECK(closeFloat(target->r.currentOrigin[1], -19.625f + (query - 100) * .18f));
        CHECK(!(target->client->ps.pm_flags & PMF_DUCKED));
        G_UnTimeShiftClient(target);
        CHECK(closeFloat(target->r.currentOrigin[0], 200) && !target->client->tceRewind.active);
    }
    /* A ray aimed at the historical visible head must not see a half-turn
     * when the two snapshots differ by only two degrees across zero. */
    for (pair = 2; pair < 4; ++pair) for (latency = -20; latency <= 20; latency += 20) {
        gentity_t *target; tceDamagePose_t visible; tceDamageVolume_t volumes[3];
        vec3_t point, direction = { 1, 0, 0 }; int i;
        reset(); target = &g_entities[1]; target->client->ps.weapon = 8;
        target->client->clientMarkers[0].lean = target->client->clientMarkers[1].lean = (float)latency;
        target->client->clientMarkers[0].weapon = target->client->clientMarkers[1].weapon = 8;
        target->client->clientMarkers[0].viewangles[YAW] = endpoints[pair][0];
        target->client->clientMarkers[1].viewangles[YAW] = endpoints[pair][1];
        memset(&visible, 0, sizeof(visible)); VectorSet(visible.origin, 150, 0, 0);
        visible.weapon = 8; visible.lean = (float)latency; /* Correct midpoint faces zero modulo360. */
        TCE_DamageVolumes(&visible, volumes);
        VectorCopy(volumes[0].origin, point); point[0] -= 40;
        G_TimeShiftClient(target, 150);
        CHECK(applyBody(target, point, direction) > 0);
        for (i = 0; i < 3; ++i) CHECK(closeFloat(firstVolume[i], volumes[0].origin[i]));
        G_UnTimeShiftClient(target);
        CHECK(closeFloat(target->client->ps.leanf, 25) && target->client->ps.weapon == 8);
    }
}
static void deathCleanup(void)
{
    gentity_t *target; int savedLinks;
    reset(); target = &g_entities[1]; g_entities[0].client->pers.cmd.serverTime = 110;
    G_TimeShiftAllClients(&g_entities[0], qtrue);
    CHECK(closeFloat(target->r.currentOrigin[0], 110));
    CHECK(closeFloat(target->r.maxs[0], 21));
    target->health = -1; target->client->ps.pm_type = PM_DEAD;
    target->client->ps.eFlags |= EF_DEAD; target->client->ps.pm_flags |= PMF_LIMBO;
    target->r.maxs[2] = 24; target->r.contents = CONTENTS_CORPSE;
    VectorSet(target->client->ps.viewangles, 0, 90, 0);
    G_UnTimeShiftAllClients(&g_entities[0]);
    CHECK(closeFloat(target->r.currentOrigin[0], 200));
    CHECK(closeFloat(target->r.mins[0], -14) && closeFloat(target->r.maxs[0], 14) && closeFloat(target->r.maxs[2], 24));
    CHECK(target->health == -1 && target->client->ps.pm_type == PM_DEAD && (target->client->ps.eFlags & EF_DEAD) && (target->client->ps.pm_flags & PMF_LIMBO));
    CHECK(target->r.contents == CONTENTS_CORPSE && closeFloat(target->client->ps.viewangles[1], 90) && !(target->r.svFlags & 0x60000));
    savedLinks = links; G_UnTimeShiftAllClients(&g_entities[0]); CHECK(links == savedLinks);
    reset(); target = &g_entities[1]; g_entities[0].client->pers.cmd.serverTime = 110;
    G_TimeShiftAllClients(&g_entities[0], qtrue);
    target->client->ps.eFlags |= EF_HEADSHOT; target->client->ps.pm_flags |= PMF_TIME_KNOCKBACK;
    G_UnTimeShiftAllClients(&g_entities[0]);
    CHECK((target->client->ps.eFlags & EF_HEADSHOT) && (target->client->ps.pm_flags & PMF_TIME_KNOCKBACK));
    reset(); target = &g_entities[1]; g_entities[0].client->pers.cmd.serverTime = 110;
    G_TimeShiftAllClients(&g_entities[0], qtrue);
    savedLinks = links; G_TimeShiftAllClients(&g_entities[0], qtrue);
    CHECK(links == savedLinks && closeFloat(target->r.maxs[0], 21));
    target->health = -1; target->client->ps.pm_type = PM_DEAD;
    G_UnTimeShiftAllClients(&g_entities[0]);
    CHECK(closeFloat(target->r.maxs[2], 46) && closeFloat(target->r.maxs[0], 14));
    reset(); target = &g_entities[1]; g_entities[0].client->pers.cmd.serverTime = 200;
    target->r.maxs[0] = 14.123456f; target->r.svFlags = 0x40000;
    G_TimeShiftAllClients(&g_entities[0], qtrue); G_UnTimeShiftAllClients(&g_entities[0]);
    CHECK(target->r.maxs[0] == 14.123456f && target->r.svFlags == 0x40000);
}
static void overlapAndStats(void)
{
    vec3_t point = { 70, 0, 42 }, direction = { 1, 0, 0 }; int kind;
    for (kind = 0; kind < 5; ++kind) {
        gentity_t *target, *blocker, *wall; reset(); target = &g_entities[1];
        VectorSet(target->r.currentOrigin, 100, 0, 0); target->client->ps.leanf = 0; target->client->ps.weapon = 1;
        blocker = &g_entities[2]; initClient(2); VectorSet(blocker->r.currentOrigin, 95, 25, 0);
        VectorSet(blocker->r.mins, -40, -40, -40); VectorSet(blocker->r.maxs, 40, 40, 60);
        blocker->r.contents = kind == 4 ? CONTENTS_CORPSE : kind ? CONTENTS_BODY : 0;
        wall = &g_entities[100]; wall->inuse = wall->r.linked = qtrue;
        wall->r.contents = kind == 2 ? CONTENTS_SOLID : kind == 3 ? CONTENTS_MISSILECLIP : 0;
        VectorSet(wall->r.currentOrigin, 85, 0, 0); VectorSet(wall->r.mins, -1, -40, -40); VectorSet(wall->r.maxs, 1, 40, 60);
        if (kind == 1) {
            /* The first broad candidate's real volumes miss this ray. The
             * second candidate must remain hittable inside that broad hull. */
            CHECK(applyBody(blocker, point, direction) == -2);
            CHECK(traceCalls == 3 && spawnCalls == 1);
            traceCalls = spawnCalls = partLinks = 0;
        }
        if (kind < 2 || kind == 4) {
            CHECK(applyBody(target, point, direction) > 0);
            CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots == 1);
            CHECK(g_entities[0].client->pers.playerStats.hitRegions[HR_HEAD] == 1);
        } else {
            CHECK(applyBody(target, point, direction) == -2);
            CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots == 0);
        }
        CHECK(traceCalls == 3 && spawnCalls == 1 && !g_entities[70].inuse);
    }
    reset(); g_entities[1].client->sess.sessionTeam = TEAM_SPECTATOR;
    CHECK(applyBody(&g_entities[1], point, direction) == -1 && spawnCalls == 0);
}
static void nonHeadAndUncreditedHits(void)
{
    vec3_t point = { 70, 0, 27.5f }, direction = { 1, 0, 0 }; int kind;
    for (kind = 0; kind < 6; ++kind) {
        gentity_t *target, *attacker; reset(); target = &g_entities[1]; attacker = &g_entities[0];
        VectorSet(target->r.currentOrigin, 100, 0, 0); target->client->ps.leanf = 0; target->client->ps.weapon = 1;
        point[1] = kind == 2 ? 100 : 0; point[2] = kind == 0 ? 27.5f : kind == 1 ? -10 : 42;
        if (kind == 3) attacker->client->sess.sessionTeam = TEAM_AXIS;
        if (kind == 4) attacker = target;
        if (kind == 5) attacker = NULL;
        CHECK(kind == 2 ? applyBodyFrom(target, attacker, point, direction) == -2 : applyBodyFrom(target, attacker, point, direction) > 0);
        CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots == 0);
        CHECK(g_entities[1].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots == 0);
        CHECK(!g_entities[0].client->pers.playerStats.hitRegions[HR_HEAD] && !target->client->pers.playerStats.hitRegions[HR_HEAD]);
        CHECK(traceCalls == 3);
    }
}
int main(void)
{
    chronological(); historicalBody(); historicalInputs(); movingAngleWrap(); deathCleanup(); overlapAndStats(); nonHeadAndUncreditedHits();
    printf("%s: %d checks, %d failures; long double %u bytes/%d mantissa bits; ray/AABB service fixture, no BSP/runtime claim\n",
#ifdef __linux__
           "Linux source path",
#else
           "Windows source path",
#endif
           tests, failures, (unsigned)sizeof(long double), LDBL_MANT_DIG);
    return failures ? 1 : 0;
}
