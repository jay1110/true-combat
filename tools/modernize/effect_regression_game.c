#include "src/game/g_local.h"
#include "src/game/tce_damage_trace.h"
#include "src/game/tce_bullet.h"
#include "src/cgame/tce_shotgun_prediction.h"
#include <stdio.h>

gentity_t g_entities[MAX_GENTITIES];
level_locals_t level;
vmCvar_t g_newbbox, g_debugBullets;
tce_weaponDef_t weaponDef[TCE_MAX_WEAPONS];
vec3_t forward;
static gclient_t client;
static gentity_t events[32];
static int eventCount, damageCalls, damageAmount, bounces, predictedExits, traceCalls, checks, failures;
static int world; /* 0: empty, 1: slab x=10..20, 2: occluded, 3: asymmetric corners, 4: material ordering */
static int breakOnDeath;
static float solidFraction, materialFraction;
static vec3_t lastTrace;
static void check(int condition, const char *label) { ++checks; if (!condition) { ++failures; printf("FAIL %s\n", label); } }
void QDECL Com_Printf(const char *format, ...) { }
void QDECL Com_Error(int code, const char *format, ...) { abort(); }
int Q_stricmp(const char *first, const char *second) { return _stricmp(first, second); }
qboolean OnSameTeam(gentity_t *first, gentity_t *second) { return qfalse; }
void RubbleFlagCheck(gentity_t *entity, trace_t trace) { }
void EmitterCheck(gentity_t *entity, gentity_t *attacker, trace_t *trace) { }
void BecomeExplosion(gentity_t *entity) { }
int TCE_BulletClientDamage(const tce_weaponDef_t *definition, int damage, int distance, int enlarged) { return damage; }
void TCE_BulletDeflect(const vec3_t start, vec3_t end, float retained, unsigned seed, int walls) { }
gentity_t *G_TempEntity(vec3_t origin, int event)
{
    gentity_t *entity = &events[eventCount++ % 32]; memset(entity, 0, sizeof(*entity));
    entity->s.event = event; VectorCopy(origin, entity->s.origin); return entity;
}
tce_damageResult_t G_Damage(gentity_t *target, gentity_t *inflictor, gentity_t *attacker,
                          vec3_t direction, vec3_t point, int damage, int flags, int mod)
{
    ++damageCalls; damageAmount = damage; target->health -= damage;
    if (breakOnDeath && target->health <= 0) { target->takedamage = qfalse; target->think = BecomeExplosion; }
    return 0;
}
qboolean AccuracyHit(gentity_t *target, gentity_t *attacker) { return target->client != NULL; }
int trap_EntitiesInBox(const vec3_t mins, const vec3_t maxs, int *list, int count) { list[0] = 1; return 1; }
void trap_Trace(trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs,
                const vec3_t end, int pass, int mask)
{
    ++traceCalls; VectorCopy(end, lastTrace); memset(trace, 0, sizeof(*trace));
    trace->fraction = 1; trace->entityNum = ENTITYNUM_NONE; VectorCopy(end, trace->endpos);
    if (world == 2 || world == 3) { trace->fraction = .5f; trace->entityNum = ENTITYNUM_WORLD; return; }
    if (world == 4) {
        trace->fraction = mask == CONTENTS_PLAYERCLIP ? materialFraction : solidFraction;
        trace->surfaceFlags = mask == CONTENTS_PLAYERCLIP ? 0x14000000 | SURF_NOIMPACT : 0;
        trace->entityNum = mask == CONTENTS_PLAYERCLIP ? 5 : 6; return;
    }
    if (world == 1) {
        if (start[0] >= 10 && start[0] < 20) {
            trace->startsolid = qtrue;
            if (end[0] >= 10 && end[0] < 20) { trace->allsolid = qtrue; trace->fraction = 0; }
            return;
        }
        if (start[0] < 10 && end[0] >= 10) {
            trace->fraction = (10 - start[0]) / (end[0] - start[0]); trace->endpos[0] = 10; trace->plane.normal[0] = -1;
        } else if (start[0] >= 20 && end[0] < 20) {
            trace->fraction = (20 - start[0]) / (end[0] - start[0]); trace->endpos[0] = 20; trace->plane.normal[0] = 1;
        } else { return; }
        trace->entityNum = ENTITYNUM_WORLD; trace->surfaceFlags = 0x0c000000;
    }
}
static void TCE_MissileVelocity(gentity_t *entity, int time, vec3_t velocity) { VectorSet(velocity, 100, 0, 0); }
void G_BounceMissile(gentity_t *entity, trace_t *trace) { ++bounces; }
void G_AddEvent(gentity_t *entity, int event, int parm) { }
void G_FreeEntity(gentity_t *entity) { entity->inuse = qfalse; }
void trap_LinkEntity(gentity_t *entity) { entity->r.linked = qtrue; }
void trap_UnlinkEntity(gentity_t *entity) { entity->r.linked = qfalse; }
int BG_FootstepForSurface(int flags) { return 0; }
void BG_GetMarkDir(const vec3_t dir, const vec3_t normal, vec3_t out) { VectorCopy(normal, out); }
static void predictedBullet(float *end, int source, float *normal, int flesh, int victim, int other, float water, int seed, int damage, float *start, int predicted) { }
static void predictedWall(int weapon, int effect, float *origin, float *normal, float *direction, int flags, int force, int material) { ++predictedExits; }
#define MASK_CAN_DAMAGE (CONTENTS_SOLID | CONTENTS_BODY)

/* PRODUCTION_FUNCTIONS */

static void reset(void)
{
    memset(g_entities, 0, sizeof(g_entities)); memset(&client, 0, sizeof(client)); memset(weaponDef, 0, sizeof(weaponDef));
    memset(&level, 0, sizeof(level)); level.num_entities = 8;
    eventCount = damageCalls = damageAmount = bounces = predictedExits = traceCalls = breakOnDeath = 0;
    g_newbbox.integer = 0; g_debugBullets.integer = 0;
    g_entities[0].client = &client; g_entities[0].s.number = 0; client.ps.weapon = 8;
    weaponDef[8].unknown_134 = 1000; weaponDef[8].unknown_130 = 1000;
    g_entities[1].s.number = 1; g_entities[1].takedamage = qtrue; g_entities[1].health = 100;
    VectorSet(g_entities[1].r.currentOrigin, 10, 0, 0);
    VectorSet(g_entities[1].r.absmin, 9, -1, -1); VectorSet(g_entities[1].r.absmax, 11, 1, 1);
}
int main(void)
{
    vec3_t origin = {0,0,0}, end = {100,0,0}; trace_t entry, exit, trace; tce_shotgunPrediction_t context;
    int i, exitEvents;
    for (i = 0; i < 2; ++i) {
        reset(); world = 2; g_entities[1].client = &client;
        if (i) etpro_RadiusDamage(origin, NULL, &g_entities[0], 100, 200, NULL, 4, qtrue);
        else G_RadiusDamage(origin, NULL, &g_entities[0], 100, 200, NULL, 4);
        check(damageCalls == 0, "solid cover blocks close-range client blast fallback");
        reset(); world = 0; g_entities[1].client = &client;
        if (i) etpro_RadiusDamage(origin, NULL, &g_entities[0], 100, 200, NULL, 4, qtrue);
        else G_RadiusDamage(origin, NULL, &g_entities[0], 100, 200, NULL, 4);
        check(damageCalls == 1 && damageAmount > 80, "unoccluded blast still damages nearby client");
    }
    reset(); world = 2; G_RadiusDamage(origin, NULL, &g_entities[0], 100, 200, NULL, 4);
    check(damageCalls == 1, "legacy map-prop blast fallback is retained");
    reset(); world = 2; g_entities[1].dmgparent = &g_entities[2]; g_entities[2].client = &client; g_entities[2].takedamage = qtrue;
    G_RadiusDamage(origin, NULL, &g_entities[0], 100, 200, NULL, 4);
    check(damageCalls == 0, "client damage parent cannot bypass cover");
    reset(); world = 3; g_entities[1].client = &client;
    VectorSet(client.ps.mins, -10, -20, -30); VectorSet(client.ps.maxs, 10, 20, 30);
    CanDamage(&g_entities[1], origin);
    check(traceCalls == 9 && lastTrace[1] == -20 && lastTrace[2] == -30, "blast corner trace uses Y bound for Y coordinate");
    reset(); world = 1; memset(&entry, 0, sizeof(entry)); entry.endpos[0] = 10; entry.plane.normal[0] = -1;
    check(TCE_BulletWallExit(&entry, origin, end, 0, trap_Trace, &exit) && exit.endpos[0] == 20,
          "isolated penetrable slab has reverse-traced exit into empty space");
    end[0] = 15;
    check(!TCE_BulletWallExit(&entry, origin, end, 0, trap_Trace, &exit), "allsolid probe cannot invent an exit");
    end[0] = 100;
    reset(); world = 1;
    Bullet_Fire_Extended(&g_entities[0], &g_entities[0], origin, end, 0, 100, 1000, 0, 0, 4, 4, 4, 42, 0);
    exitEvents = 0; for (i = 0; i < eventCount; ++i) if (events[i].s.event == EV_TCE_BULLET_PIERCED_WALL) ++exitEvents;
    check(exitEvents == 1, "server emits exit event for isolated thin wall");
    memset(&context, 0, sizeof(context)); context.maxClients = MAX_CLIENTS; context.weapons = weaponDef;
    context.trace = trap_Trace; context.bullet = predictedBullet; context.wall = predictedWall;
    predictedExits = 0; TCE_CG_BulletFireExtended(&context, 0, 8, origin, end, 1000, 4, 42);
    check(predictedExits == 1, "predicted shooter sees isolated wall exit mark");
    reset(); world = 4; solidFraction = .2f; materialFraction = .7f;
    G_TCEMissileTrace(&g_entities[0], end, &trace);
    check(trace.entityNum == 6 && trace.fraction == .2f, "material clip behind wall cannot replace nearer solid impact");
    solidFraction = .7f; materialFraction = .2f;
    G_TCEMissileTrace(&g_entities[0], end, &trace);
    check(trace.entityNum == 5 && !(trace.surfaceFlags & SURF_NOIMPACT), "nearer TC clip material remains collidable");
    reset(); world = 0; g_entities[1].classname = "func_explosive"; g_entities[1].health = 10;
    g_entities[0].classname = "grenade"; g_entities[0].r.ownerNum = 0; g_entities[0].s.eFlags = EF_BOUNCE_HALF;
    memset(&trace, 0, sizeof(trace)); trace.entityNum = 1;
    G_MissileImpact(&g_entities[0], &trace, 20);
    check(damageCalls == 1 && bounces == 0, "grenade that destroys breakable already passes without bounce");
    reset(); world = 0; g_entities[1].classname = "func_explosive"; g_entities[1].health = 100;
    g_entities[0].classname = "grenade"; g_entities[0].s.eFlags = EF_BOUNCE_HALF;
    G_MissileImpact(&g_entities[0], &trace, 20);
    check(bounces == 1 && damageCalls == 0, "surviving breakable retains ordinary grenade bounce");
    reset(); g_entities[1].classname = "func_explosive"; g_entities[1].health = 20;
    g_entities[0].classname = "grenade"; g_entities[0].s.eFlags = EF_BOUNCE_HALF; breakOnDeath = 1;
    G_MissileImpact(&g_entities[0], &trace, 20); G_MissileImpact(&g_entities[0], &trace, 20);
    check(g_entities[1].health == 0 && damageCalls == 1 && bounces == 0,
          "exact-health glass does not bounce during pending removal frame");
    reset(); g_entities[1].classname = "func_explosive"; g_entities[1].health = 0; g_entities[1].takedamage = qfalse;
    g_entities[0].classname = "grenade"; g_entities[0].s.eFlags = EF_BOUNCE_HALF;
    G_MissileImpact(&g_entities[0], &trace, 20);
    check(bounces == 1, "unbreakable zero-health brush is not treated as destroyed glass");
    printf("effect/cover game: %d checks, %d failures\n", checks, failures); return failures ? 1 : 0;
}
