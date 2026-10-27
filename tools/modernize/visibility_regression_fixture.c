#include "src/cgame/cg_local.h"
#include "src/cgame/cg_tce_pose_presentation.h"
#include <stdio.h>

cg_t cg;
cgs_t cgs;
centity_t cg_entities[MAX_GENTITIES];
vmCvar_t cg_shadows,developer;
int tceSmokeNewBBox;
static snapshot_t snapshot;
static int world, traceCalls, checks, failures;
static refEntity_t lastSprite;
static vec3_t lastSplash,lastShadow;
void trap_R_AddRefEntityToScene(const refEntity_t *entity) {lastSprite=*entity;}
int CG_PointContents(const vec3_t point,int pass) {(void)pass;VectorCopy(point,lastSplash);return 0;}
void trap_CM_BoxTrace(trace_t *trace,const vec3_t start,const vec3_t end,const vec3_t mins,
    const vec3_t maxs,clipHandle_t model,int mask) {
    (void)end;(void)mins;(void)maxs;(void)model;(void)mask;
    VectorCopy(start,lastShadow);memset(trace,0,sizeof(*trace));trace->fraction=1;
}
int CG_GetOriginForTag(centity_t *cent,refEntity_t *parent,char *name,int start,vec3_t origin,vec3_t axis[3])
{(void)cent;(void)parent;(void)name;(void)start;(void)origin;(void)axis;return -1;}
void CG_EliteImpactMark(qhandle_t shader,vec3_t origin,vec3_t dir,float orientation,
    float red,float green,float blue,float alpha,qboolean fade,float radius,qboolean temporary,int duration)
{(void)shader;(void)origin;(void)dir;(void)orientation;(void)red;(void)green;(void)blue;(void)alpha;(void)fade;(void)radius;(void)temporary;(void)duration;}
void trap_R_AddPolyToScene(qhandle_t shader,int count,const polyVert_t *vertices)
{(void)shader;(void)count;(void)vertices;}
static void check(int condition, const char *label)
{
    ++checks; if (!condition) { ++failures; printf("FAIL %s\n", label); }
}
void CG_Trace(trace_t *trace, const vec3_t start, const vec3_t mins, const vec3_t maxs,
              const vec3_t end, int skip, int mask)
{
    int candidates = 1, i, entity; float x, fraction;
    vec3_t delta;
    ++traceCalls; memset(trace, 0, sizeof(*trace)); trace->fraction = 1;
    trace->entityNum = ENTITYNUM_NONE; VectorCopy(end, trace->endpos);
    if (!world) return;
    if(world==14) {
        if(end[1]<0) {trace->fraction=.5f;trace->entityNum=ENTITYNUM_WORLD;trace->contents=CONTENTS_SOLID;}
        return;
    }
    if (world == 5 || world == 6 || world == 7 || world == 9 || world == 12 || world == 13) candidates = 2;
    for (i = 0; i < candidates; ++i) {
        x = i ? 60.f : 20.f;
        entity = world == 8 ? ENTITYNUM_WORLD : i ? ((world == 7 || world == 13) ? 100 : world == 9 ? 101 : ENTITYNUM_WORLD) : 100;
        if (skip == entity || start[0] == end[0]) continue;
        fraction = (x - start[0]) / (end[0] - start[0]);
        if (fraction < 0 || fraction >= trace->fraction) continue;
        trace->fraction = fraction; trace->entityNum = entity;
        VectorSubtract(end, start, delta); VectorMA(start, fraction, delta, trace->endpos);
        trace->surfaceFlags = ((world == 2 || world == 3 || world == 6) ? 0x0c000000 : 0x00200c02);
        trace->contents = CONTENTS_SOLID | CONTENTS_TRANSLUCENT;
        if (world == 3 || world == 4 || (i && world != 9)) {
            trace->surfaceFlags = world == 3 ? 0x0c000000 : 0x05000000;
            trace->contents = CONTENTS_SOLID;
        }
        if ((world == 10 || world == 12 || world == 13) && !i) trace->contents = CONTENTS_SOLID;
        if (world == 11) trace->surfaceFlags = 0;
    }
}

/* PRODUCTION_FUNCTIONS */

static centity_t *reset(int mode)
{
    centity_t *target;
    memset(&cg, 0, sizeof(cg)); memset(cg_entities, 0, sizeof(cg_entities));
    memset(&snapshot, 0, sizeof(snapshot)); cg.snap = &snapshot; cg.time = 100;
    snapshot.ps.clientNum = 0; VectorSet(cg.refdef.viewaxis[0], 1, 0, 0);
    cg_entities[100].currentState.solid = SOLID_BMODEL;
    cg_entities[101].currentState.solid = SOLID_BMODEL;
    target = &cg_entities[1]; target->currentState.eType = ET_PLAYER;
    target->currentState.clientNum = 1; target->lerpOrigin[0] = 100;
    world = mode; traceCalls = 0; return target;
}

int main(void)
{
    centity_t *target; int before;
    target = reset(0); CG_CalcEntityVisibility(target);
    check(target->tceVisible, "clear player remains visible");
    target = reset(1); CG_CalcEntityVisibility(target);
    check(target->tceVisible, "transparent legacy SURF_GLASS does not hide player");
    target = reset(2); CG_CalcEntityVisibility(target);
    check(target->tceVisible, "transparent TC glass remains visible");
    target = reset(3); CG_CalcEntityVisibility(target);
    check(!target->tceVisible, "opaque glass material cannot bypass visibility");
    target = reset(4); CG_CalcEntityVisibility(target);
    check(!target->tceVisible, "opaque breakable wood still occludes");
    target = reset(5); CG_CalcEntityVisibility(target);
    check(!target->tceVisible, "world wall behind legacy glass still occludes");
    target = reset(6); CG_CalcEntityVisibility(target);
    check(!target->tceVisible, "world wall behind TC glass still occludes");
    target = reset(7); CG_CalcEntityVisibility(target);
    check(!target->tceVisible, "opaque rear face in same inline model still occludes");
    target = reset(8); CG_CalcEntityVisibility(target);
    check(!target->tceVisible, "world glass fails closed when one brush cannot be skipped");
    target = reset(9); CG_CalcEntityVisibility(target);
    check(!target->tceVisible, "multiple pane ambiguity stays conservative");
    target = reset(10); CG_CalcEntityVisibility(target);
    check(target->tceVisible, "Delta legacy alpha-glass side remains transparent with SOLID-only caulk brush");
    target = reset(12); CG_CalcEntityVisibility(target);
    check(!target->tceVisible, "opaque world wall behind Delta SOLID-only glass still occludes");
    target = reset(13); CG_CalcEntityVisibility(target);
    check(!target->tceVisible, "opaque rear face behind Delta SOLID-only glass still occludes");
    target = reset(11); CG_CalcEntityVisibility(target);
    check(!target->tceVisible, "translucent non-glass remains an occluder");
    target = reset(0); target->lerpOrigin[0] = -100; CG_CalcEntityVisibility(target);
    check(!target->tceVisible, "out-of-forward-view policy retained");
    target = reset(4); target->currentState.clientNum = 0; CG_CalcEntityVisibility(target);
    check(target->tceVisible && !traceCalls, "local player exception retained");
    target = reset(4); target->currentState.eFlags = EF_DEAD; CG_CalcEntityVisibility(target);
    check(target->tceVisible && !traceCalls, "dead player exception retained");
    target = reset(0); CG_CalcEntityVisibility(target); before = traceCalls;
    world = 4; CG_CalcEntityVisibility(target);
    check(target->tceVisible && traceCalls == before, "existing positive cache retained");
    cg.time = 1100; CG_CalcEntityVisibility(target);
    check(!target->tceVisible, "expired positive cache rechecks occlusion");
    before = traceCalls; CG_CalcEntityVisibility(target);
    check(!target->tceVisible && traceCalls == before + 10, "no unsafe negative cache introduced");
    {
        tcePresentationOriginScope_t scope;vec3_t canonical={100,100,0};float plane;
        target=reset(14);target->lerpOrigin[1]=-100;CG_CalcEntityVisibility(target);
        check(!target->tceVisible,"actual legacy call rejects wrong-position body behind opaque cover");
        memset(&scope,0,sizeof(scope));
#if TCE_TEST_ORIGIN_SCOPE
        CG_TCEPresentationBeginOrigin(target,canonical,&scope);
#endif
        check(target->tceVisible&&target->lerpOrigin[1]==100,"corrected affected body receives actual visible-origin culling");
        CG_PlayerFloatSprite(target,1,48);
        cg_shadows.integer=1;CG_PlayerSplash(target);CG_PlayerShadow(target,&plane);
        check(lastSprite.origin[1]==100&&lastSplash[1]==100&&lastShadow[1]==100,
              "actual sprite splash and shadow bodies all consume canonical affected origin");
        CG_TCEPresentationEndOrigin(target,&scope);
        check(target->lerpOrigin[1]==-100&&!target->tceVisible&&target->tceVisibilityUntil==1100,
              "scope exit restores original interpolation and negative visibility cache");
        target=reset(14);target->lerpOrigin[1]=100;CG_CalcEntityVisibility(target);before=traceCalls;
        check(target->tceVisible&&target->tceVisibilityUntil==1100,"actual legacy positive cache precondition");
        canonical[1]=-100;memset(&scope,0,sizeof(scope));
#if TCE_TEST_ORIGIN_SCOPE
        CG_TCEPresentationBeginOrigin(target,canonical,&scope);
#endif
        check(!target->tceVisible&&traceCalls==before+10,
              "new canonical point behind cover invalidates stale positive cache and remains occluded");
        CG_TCEPresentationEndOrigin(target,&scope);
        check(target->lerpOrigin[1]==100&&target->tceVisible&&target->tceVisibilityUntil==1100,
              "scope exit restores original positive cache without persisting canonical override");
        before=traceCalls;VectorCopy(target->lerpOrigin,canonical);
        CG_TCEPresentationBeginOrigin(target,canonical,&scope);
        check(!scope.active&&traceCalls==before,"identical ordinary/own predicted origin does not rerun visibility or alter cache");
        CG_TCEPresentationEndOrigin(target,&scope);
    }
    printf("player visibility: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
