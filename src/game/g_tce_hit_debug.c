#include "g_local.h"
#include "g_tce_hit_debug.h"
#include "g_tce_pose.h"
#include "g_tce_pose_presentation.h"
#include "tce_damage_trace.h"

/* Fixed event budget, not an unbounded reliable-command queue. At 64 active
 * players each continuous sample refreshes within 800 ms. Its authoritative
 * timestamp travels with it; it is never extrapolated into a claimed hitbox. */
static int debugFrame, debugStart, debugNext, debugCursor, debugShotEvents[2], debugLogLines;
int G_TCEHitDebugTarget(int clientNum)
{
    return g_debugBulletsClient.integer < 0 || g_debugBulletsClient.integer == clientNum;
}
static int DebugPlayer(const gentity_t *ent)
{
    return ent->inuse && ent->client && ent->health > 0 &&
        ent->client->pers.connected == CON_CONNECTED &&
        (ent->client->sess.sessionTeam == TEAM_AXIS || ent->client->sess.sessionTeam == TEAM_ALLIES);
}
static void DebugBudget(void)
{
    if (debugStart != level.startTime || level.time < debugFrame) {
        debugStart = level.startTime; debugCursor = debugNext = debugLogLines = 0;
        memset(debugShotEvents, 0, sizeof(debugShotEvents));
    }
    if (level.time / TCE_HIT_DEBUG_INTERVAL != debugFrame / TCE_HIT_DEBUG_INTERVAL) {
        memset(debugShotEvents, 0, sizeof(debugShotEvents)); debugLogLines = 0;
    }
    debugFrame = level.time;
}
static void DebugSample(gentity_t *ent, int layer, uint32_t time, uint32_t ceiling)
{
    tceDamagePose_t pose;
    tceDamageVolume_t volumes[3];
    g_tcePresentationQuery_t query;
    vec3_t low[3], high[3];
    gentity_t *event;
    int policy, region, axis, kind = 0;
    policy = G_TCEPresentationQuery(ent->s.number, time, ceiling, &query);
    if (policy == G_TCE_PRESENTATION_LEGACY) {
        TCE_ClientDamagePose(ent, &pose); TCE_DamageVolumes(&pose, volumes);
        for (region = 0; region < 3; ++region) for (axis = 0; axis < 3; ++axis) {
            low[region][axis] = volumes[region].origin[axis] + volumes[region].mins[axis];
            high[region][axis] = volumes[region].origin[axis] + volumes[region].maxs[axis];
        }
    } else if (policy == G_TCE_PRESENTATION_AFFECTED) {
        if (!G_TCEPosePresentationBounds(&query, low, high)) return;
        kind = 1; /* Bounds of evaluated triangles, NOT solid damage boxes. */
    } else return; /* Never invent a legacy volume for UNKNOWN. */
    event = G_TempEntity(ent->r.currentOrigin, EV_RAILTRAIL);
    event->s.dmgFlags = TCE_HIT_DEBUG_BOX_EVENT;
    event->s.otherEntityNum = ent->s.number;
    event->s.time = level.time; event->s.time2 = (int)time;
    event->s.effect1Time = layer; event->s.effect2Time = kind;
    VectorCopy(low[0], event->s.pos.trBase); VectorCopy(high[0], event->s.origin2);
    VectorCopy(low[1], event->s.angles); VectorCopy(high[1], event->s.angles2);
    VectorCopy(low[2], event->s.apos.trBase); VectorCopy(high[2], event->s.apos.trDelta);
    event->r.svFlags |= SVF_BROADCAST;
}
void G_TCEHitDebugFrame(void)
{
    int checked, sent = 0;
    DebugBudget();
    if (!(g_debugBullets.integer & TCE_HIT_DEBUG_CONTINUOUS)) { debugNext = 0; return; }
    if (level.time < debugNext) return;
    debugNext = level.time + TCE_HIT_DEBUG_INTERVAL;
    if (g_debugBulletsClient.integer >= 0) {
        int client = g_debugBulletsClient.integer;
        if ((unsigned)client < MAX_CLIENTS && DebugPlayer(&g_entities[client]))
            DebugSample(&g_entities[client], TCE_HIT_DEBUG_CURRENT, (uint32_t)level.time, (uint32_t)level.time);
        return;
    }
    for (checked = 0; checked < MAX_CLIENTS && sent < TCE_HIT_DEBUG_BATCH; ++checked) {
        gentity_t *ent = &g_entities[debugCursor];
        debugCursor = (debugCursor + 1) % MAX_CLIENTS;
        if (!DebugPlayer(ent)) continue;
        DebugSample(ent, TCE_HIT_DEBUG_CURRENT, (uint32_t)level.time, (uint32_t)level.time);
        ++sent;
    }
}
void G_TCEHitDebugShot(gentity_t *shooter, int layer)
{
    int i;
    uint32_t time = (uint32_t)level.time, ceiling = time;
    if (layer != TCE_HIT_DEBUG_HISTORY && layer != TCE_HIT_DEBUG_SHOT) return;
    if (!(g_antilag.integer & (layer == TCE_HIT_DEBUG_HISTORY ? TCE_ANTILAG_REWOUND : TCE_ANTILAG_CURRENT))) return;
    DebugBudget();
    if (layer == TCE_HIT_DEBUG_HISTORY && shooter->client->tceRewind.shotQueryActive) {
        time = (uint32_t)shooter->client->tceRewind.requestedTime;
        ceiling = (uint32_t)shooter->client->tceRewind.receiptCeiling;
    }
    for (i = 0; i < MAX_CLIENTS && debugShotEvents[layer - 1] < 8; ++i) {
        gentity_t *ent = &g_entities[i];
        if (ent == shooter || !DebugPlayer(ent) || !G_TCEHitDebugTarget(i)) continue;
        DebugSample(ent, layer, time, ceiling); ++debugShotEvents[layer - 1];
    }
}
int G_TCEHitDebugLogAllowed(void)
{
    if (!(g_debugBullets.integer & TCE_HIT_DEBUG_LOG)) return 0;
    DebugBudget();
    if (++debugLogLines <= 96) return 1;
    if (debugLogLines == 97) G_Printf("TCEHIT log budget exhausted for this 100 ms interval\n");
    return 0;
}
void G_TCEHitDebugTrace(const char *stage, int shooter, int target, int region,
    const trace_t *trace, const vec3_t start, const vec3_t end)
{
    const gentity_t *blocker;
    if ((target >= 0 && !G_TCEHitDebugTarget(target)) || !G_TCEHitDebugLogAllowed()) return;
    blocker = (unsigned)trace->entityNum < MAX_GENTITIES ? &g_entities[trace->entityNum] : NULL;
    G_Printf("TCEHIT %s frame=%d shooter=%d target=%d region=%d blocker=%d class=%s contents=0x%x surface=0x%x fraction=%.9g startsolid=%d allsolid=%d start=(%.9g %.9g %.9g) end=(%.9g %.9g %.9g) hit=(%.9g %.9g %.9g)\n",
        stage, level.time, shooter, target, region, trace->entityNum,
        blocker && blocker->classname ? blocker->classname : "none", trace->contents, trace->surfaceFlags,
        trace->fraction, trace->startsolid, trace->allsolid,
        start[0], start[1], start[2], end[0], end[1], end[2], trace->endpos[0], trace->endpos[1], trace->endpos[2]);
}
