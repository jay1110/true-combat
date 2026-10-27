#include "cg_local.h"
#include "cg_tce_hit_debug.h"
#include "cg_hud_viewport.h"
#include "../game/tce_hit_debug.h"
#include <float.h>

typedef struct {
    int valid, received, sourceTime, kind;
    vec3_t low[3], high[3];
} hitDebugSample_t;
static hitDebugSample_t debugSamples[TCE_HIT_DEBUG_LAYERS][MAX_CLIENTS];
static int debugDrawTime, debugVisible, debugMeshes, debugAge;
void CG_TCEHitDebugReset(void)
{
    memset(debugSamples, 0, sizeof(debugSamples)); debugDrawTime = 0;
}
int CG_TCEHitDebugEvent(const entityState_t *event)
{
    hitDebugSample_t next, *slot;
    int layer = event->effect1Time, client = event->otherEntityNum, region, axis;
    if (event->dmgFlags != TCE_HIT_DEBUG_BOX_EVENT) return 0;
    if ((unsigned)client >= MAX_CLIENTS || (unsigned)layer >= TCE_HIT_DEBUG_LAYERS ||
        (unsigned)event->effect2Time > 1 || event->time > cg.time + 200 || cg.time - event->time > TCE_HIT_DEBUG_LIFETIME)
        return 1;
    memset(&next, 0, sizeof(next)); next.valid = 1; next.received = event->time;
    next.sourceTime = event->time2; next.kind = event->effect2Time;
    VectorCopy(event->pos.trBase, next.low[0]); VectorCopy(event->origin2, next.high[0]);
    VectorCopy(event->angles, next.low[1]); VectorCopy(event->angles2, next.high[1]);
    VectorCopy(event->apos.trBase, next.low[2]); VectorCopy(event->apos.trDelta, next.high[2]);
    for (region = 0; region < 3; ++region) for (axis = 0; axis < 3; ++axis)
        if (!(next.low[region][axis] >= -FLT_MAX && next.high[region][axis] <= FLT_MAX &&
            next.low[region][axis] <= next.high[region][axis] &&
            next.high[region][axis] - next.low[region][axis] < 1024)) return 1;
    slot = &debugSamples[layer][client];
    if (layer && !slot->valid) {
        int count = 0, oldest = -1, i;
        for (i = 0; i < MAX_CLIENTS; ++i) if (debugSamples[layer][i].valid) {
            ++count;
            if (oldest < 0 || debugSamples[layer][i].received < debugSamples[layer][oldest].received) oldest = i;
        }
        /* 64 continuous + 8 rewound + 8 current-shot samples: at most 2880
         * quads/11520 verts, independent of fire rate or entity count. */
        if (count >= 8) debugSamples[layer][oldest].valid = 0;
    }
    if (!slot->valid || event->time >= slot->received) *slot = next;
    return 1;
}
static void DebugLine(const vec3_t start, const vec3_t end, const byte color[4])
{
    vec3_t delta, side;
    polyVert_t verts[4];
    float x, y;
    int i;
    VectorSubtract(end, start, delta);
    x = DotProduct(delta, cg.refdef.viewaxis[1]); y = DotProduct(delta, cg.refdef.viewaxis[2]);
    VectorScale(cg.refdef.viewaxis[1], y, side); VectorMA(side, -x, cg.refdef.viewaxis[2], side);
    if (VectorNormalize(side) == 0) VectorCopy(cg.refdef.viewaxis[1], side);
    /* Same front-face winding as the existing CG_Tracer scene quads. */
    VectorMA(end, .16f, side, verts[0].xyz); VectorMA(end, -.16f, side, verts[1].xyz);
    VectorMA(start, -.16f, side, verts[2].xyz); VectorMA(start, .16f, side, verts[3].xyz);
    for (i = 0; i < 4; ++i) {
        verts[i].st[0] = (i >= 2); verts[i].st[1] = (i == 0 || i == 3);
        memcpy(verts[i].modulate, color, 4);
    }
    trap_R_AddPolyToScene(cgs.media.whiteShader, 4, verts);
}
void CG_TCEHitDebugDraw(void)
{
    static const byte colors[3][3][4] = {
        {{255,70,70,230},{255,210,50,230},{40,210,255,230}},
        {{255,90,220,220},{190,80,255,220},{100,100,255,220}},
        {{255,255,255,220},{80,255,100,220},{80,200,180,220}}
    };
    int layer, client, region, edge, axis;
    debugVisible = debugMeshes = debugAge = 0;
    if (cg.time < debugDrawTime) CG_TCEHitDebugReset();
    debugDrawTime = cg.time;
    for (layer = 0; layer < TCE_HIT_DEBUG_LAYERS; ++layer) for (client = 0; client < MAX_CLIENTS; ++client) {
        hitDebugSample_t *sample = &debugSamples[layer][client];
        int age = cg.time - sample->received;
        if (!sample->valid) continue;
        if (age > TCE_HIT_DEBUG_LIFETIME || age < -200) {sample->valid = 0; continue;}
        /* A first-person camera sits inside its own regional boxes. Their
         * near-plane edges can fill the screen; follow uses the viewed PS
         * client, not the local spectator slot. Third person keeps self. */
        if (!cg.renderingThirdPerson && cg.snap && client == cg.snap->ps.clientNum) continue;
        ++debugVisible; debugMeshes += sample->kind;
        if (age > debugAge) debugAge = age;
        for (region = 0; region < 3; ++region) for (edge = 0; edge < 12; ++edge) {
            vec3_t start, end;
            int direction = edge / 4, corner = edge % 4, bit = 0;
            for (axis = 0; axis < 3; ++axis) {
                if (axis == direction) {start[axis] = sample->low[region][axis]; end[axis] = sample->high[region][axis];}
                else {start[axis] = end[axis] = (corner & (1 << bit++)) ? sample->high[region][axis] : sample->low[region][axis];}
            }
            DebugLine(start, end, colors[layer][region]);
        }
    }
}
static void DebugText(float y, const char *text)
{
    float scale = .14f;
    int width = CG_Text_Width_Ext(text, scale, 0, &cgs.media.limboFont2);
    vec4_t color = {1, 1, 1, .95f};
    if (width > 360) scale *= 360.f / width;
    CG_Text_Paint_Ext(8, y, scale, scale, color, text, 0, 0, 3, &cgs.media.limboFont2);
}
void CG_TCEHitDebugOverlay(void)
{
    char line[160];
    if (!debugVisible) return;
    CG_HudViewportBegin(0, 0);
    Com_sprintf(line, sizeof(line), "Hitboxes: %d samples; max age %d ms | ^1head ^3torso ^5legs", debugVisible, debugAge);
    DebugText(80, line);
    DebugText(91, "Rewind: magenta/purple/blue | Shot: white/green/teal");
    if (debugMeshes) DebugText(102, "Affected pose: triangle BOUNDS, not solid damage boxes");
    CG_HudViewportEnd();
}
