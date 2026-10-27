/* Semantic movement contracts. Production bodies are inserted unchanged.
 * Synthetic traces prove bounded invariants, not the contents of a real BSP. */
#include "src/game/q_shared.h"
#include "src/game/bg_public.h"
#include "src/game/bg_local.h"
#include "src/game/tce_bg.h"
#include "src/game/tce_numeric.h"
#include "src/game/tce_friction_precision.h"
#include "src/game/tce_movement.h"
#include <stdarg.h>
#define CS_MOVE_CVAR g_movespeed
#define CS_GAME_CVAR g_gametype
#define MAX_CLIP_PLANES 5
typedef char movementTestPointerAbi[(sizeof(void *) * 8 == MOVEMENT_TEST_POINTER_BITS) ? 1 : -1];

pmove_t *pm;
pml_t pml;
tce_weaponDef_t weaponDef[TCE_MAX_WEAPONS];
tce_gearDef_t gearDef;
vec3_t playerlegsProneMins = { -13.5f, -13.5f, -24.f };
vec3_t playerlegsProneMaxs = { 13.5f, 13.5f, -14.4f };
vmCvar_t g_gametype, g_movespeed;
float pm_stopspeed = 100, pm_friction = 6, pm_waterfriction = 1, pm_slagfriction = 1;
float pm_spectatorfriction = 5, pm_VC100friction = 2, pm_ladderfriction = 14;
float pm_accelerate = 10, pm_airaccelerate = 1, pm_proneSpeedScale = .12f;
float pm_waterSwimScale = .5f, pm_slagSwimScale = .3f;
int c_pmove;
static playerState_t ps;
static pmove_t move;
static pmoveExt_t ext;
static bg_character_t character;
static int checks, failures, events, lastEvent, airCalls, damagingFallEvents, lastAnimation;
static int scriptedSlide, slideCalls, stepKind, traces, climbTraces;
static int worldKind, useRealAir;
static float stepZ, firstX, firstY, secondX, secondY;
static qboolean PM_RealSlideMove(qboolean gravity);
static void PM_ActualAirMove(void);

void QDECL Com_Printf(const char *format, ...) { }
void QDECL Com_Error(int code, const char *format, ...) { abort(); }
int BG_AnimScriptEvent(playerState_t *state, animModelInfo_t *model,
                      scriptAnimEventTypes_t event, qboolean cont, qboolean force) { return 0; }
int BG_AnimScriptAnimation(playerState_t *state, animModelInfo_t *model, scriptAnimMoveTypes_t animation, qboolean cont)
{
    lastAnimation = animation; return 0;
}
void PM_AddEvent(int event)
{
    ++events; lastEvent = event;
    if (event == EV_FALL_NDIE || event == EV_TCE_FALL_DMG_75 || event == EV_FALL_DMG_50 ||
        event == EV_FALL_DMG_25 || event == EV_FALL_DMG_15 || event == EV_FALL_DMG_10) ++damagingFallEvents;
}
void PM_AddEventExt(int event, int parm) { PM_AddEvent(event); }
void PM_AddTouchEnt(int entity) { if (pm->numtouch < MAXTOUCH) pm->touchents[pm->numtouch++] = entity; }
static int PM_FootstepForSurface(void) { return 0; }
static void PM_AirMove(void) { ++airCalls; if (useRealAir) PM_ActualAirMove(); }
static void PM_WaterMove(void) { ++airCalls; }
static qboolean PM_CheckDodge(void) { return qfalse; }
static void PM_SetMovementDir(void) { }
static int PM_ProneTimeValue(unsigned bits) { int value; memcpy(&value, &bits, sizeof(value)); return value; }
static int PM_CorrectAllSolid(trace_t *trace) { ++failures; puts("Unexpected allsolid ground in analytic fixture"); return qfalse; }
void PM_StepSlideMoveProne(qboolean gravity) { PM_StepSlideMove(gravity); }

qboolean PM_SlideMove(qboolean gravity)
{
    if (!scriptedSlide) return PM_RealSlideMove(gravity);
    ++slideCalls;
    pm->ps->origin[0] += slideCalls == 1 ? firstX : secondX;
    pm->ps->origin[1] += slideCalls == 1 ? firstY : secondY;
    pm->ps->velocity[2] = 0;
    if (slideCalls > 1) { pml.impactSpeed = 999; PM_AddTouchEnt(17); }
    return qtrue;
}

/* PRODUCTION_FUNCTIONS */

static void check(int condition, const char *label)
{
    ++checks;
    if (!condition) { ++failures; printf("FAIL %s\n", label); }
}
static int near(float first, float second) { return fabsf(first - second) < .001f; }

static void clearTrace(trace_t *trace, const vec3_t end)
{
    memset(trace, 0, sizeof(*trace)); trace->fraction = 1;
    trace->entityNum = ENTITYNUM_WORLD; trace->plane.normal[2] = 1;
    VectorCopy(end, trace->endpos);
}

/* Swept point against Minkowski-expanded axis aligned brush. Boundaries may
 * be touched; an upward move from a floor never starts inside the floor. */
static void box(trace_t *trace, const vec3_t start, const vec3_t end,
                const vec3_t mins, const vec3_t maxs, const vec3_t brushMin, const vec3_t brushMax)
{
    float enter = -1e30f, leave = 1e30f, normal = 0;
    int axis, hitAxis = -1, insideStart = 1, insideEnd = 1;
    for (axis = 0; axis < 3; ++axis) {
        float low = brushMin[axis] - maxs[axis], high = brushMax[axis] - mins[axis];
        float delta = end[axis] - start[axis], first, last, side;
        if (start[axis] <= low || start[axis] >= high) insideStart = 0;
        if (end[axis] <= low || end[axis] >= high) insideEnd = 0;
        if (fabsf(delta) < .000001f) {
            if (start[axis] <= low || start[axis] >= high) return;
            continue;
        }
        if (delta > 0) { first = (low - start[axis]) / delta; last = (high - start[axis]) / delta; side = -1; }
        else { first = (high - start[axis]) / delta; last = (low - start[axis]) / delta; side = 1; }
        if (first > enter) { enter = first; hitAxis = axis; normal = side; }
        if (last < leave) leave = last;
        if (enter > leave) return;
    }
    if (insideStart) { trace->startsolid = qtrue; trace->allsolid = insideEnd; }
    if (enter < 0 || enter > 1 || leave < 0 || enter >= trace->fraction) return;
    trace->fraction = enter;
    VectorClear(trace->plane.normal);
    if (hitAxis >= 0) trace->plane.normal[hitAxis] = normal;
    for (axis = 0; axis < 3; ++axis) trace->endpos[axis] = start[axis] + enter * (end[axis] - start[axis]);
}

static void collision(trace_t *trace, const vec3_t start, const vec3_t mins,
                      const vec3_t maxs, const vec3_t end, int pass, int mask)
{
    ++traces; clearTrace(trace, end);
    if (worldKind == 1) { /* vertical wall x=10 */
        vec3_t low = { 10, -1000, -1000 }, high = { 1000, 1000, 1000 };
        box(trace, start, end, mins, maxs, low, high);
    } else if (worldKind == 2 || worldKind == 3) { /* rail inside/outside standing hull */
        vec3_t low = { 1, -4, 20 }, high = { 3, 4, 22 };
        if (worldKind == 3) { low[0] = 20; high[0] = 22; }
        box(trace, start, end, mins, maxs, low, high);
    } else if (worldKind == 4 || worldKind == 7) { /* stair, optional low ceiling */
        vec3_t low = { -1000, -1000, -1000 }, high = { 1000, 1000, 0 };
        box(trace, start, end, mins, maxs, low, high);
        low[0] = 10; high[2] = stepZ;
        box(trace, start, end, mins, maxs, low, high);
        if (worldKind == 7) {
            VectorSet(low, -1000, -1000, 39.01f); VectorSet(high, 1000, 1000, 1000);
            box(trace, start, end, mins, maxs, low, high);
        }
    } else if (worldKind == 5 || worldKind == 6) { /* repeated9-unit stairs */
        int step;
        for (step = 0; step < 40; ++step) {
            vec3_t low = { -1000, -1000, -1000 }, high = { 10000, 1000, 0 };
            if (step) low[0] = 10 + (step - 1) * 30;
            if (step < 39) high[0] = 10 + step * 30;
            high[2] = worldKind == 5 ? (float)(-9 * step) : (float)(9 * step);
            box(trace, start, end, mins, maxs, low, high);
        }
    }
}

static void stepTrace(trace_t *trace, const vec3_t start, const vec3_t mins,
                      const vec3_t maxs, const vec3_t end, int pass, int mask)
{
    clearTrace(trace, end); ++traces;
    /* No ground below an ascending player is tested separately. */
    if (start[2] == 0 && end[2] < 0) { trace->fraction = 0; VectorCopy(start, trace->endpos); return; }
    /* Thin overhang intersects the upward sweep, but neither endpoint. */
    if (stepKind == 3 && end[2] > start[2]) {
        trace->fraction = .5f;
        VectorCopy(start, trace->endpos); trace->endpos[2] += (end[2] - start[2]) * .5f;
        return;
    }
    /* Force the full18 trial if the9-unit forward probe is blocked. */
    if (stepZ > 9 && near(start[2], 9) && near(start[0], 4) && near(end[2], 9)) {
        trace->allsolid = trace->startsolid = qtrue; trace->fraction = 0; return;
    }
    if (end[2] < start[2]) {
        if (stepKind == 2) { trace->allsolid = qtrue; trace->fraction = 0; return; }
        trace->fraction = (start[2] - stepZ) / (start[2] - end[2]);
        VectorCopy(end, trace->endpos); trace->endpos[2] = stepZ;
        if (stepKind == 4) trace->plane.normal[2] = .5f;
    }
}

static void climbTrace(trace_t *trace, const vec3_t start, const vec3_t mins,
                       const vec3_t maxs, const vec3_t end, int pass, int mask)
{
    int current = climbTraces++;
    clearTrace(trace, end);
    if (current == 0) trace->fraction = 0;
    if (current == 3) trace->fraction = (96 - stepZ) / 256;
}

static void reset(void)
{
    memset(&ps, 0, sizeof(ps)); memset(&move, 0, sizeof(move));
    memset(&ext, 0, sizeof(ext)); memset(&character, 0, sizeof(character));
    memset(&pml, 0, sizeof(pml)); memset(weaponDef, 0, sizeof(weaponDef));
    pm = &move; move.ps = &ps; move.pmext = &ext; move.character = &character;
    move.trace = collision; move.tracemask = MASK_PLAYERSOLID; ps.clientNum = 1;
    ps.groundEntityNum = ENTITYNUM_WORLD; ps.gravity = 800; ps.stats[STAT_HEALTH] = 100;
    ps.weapon = 8; ps.speed = 200; ps.sprintSpeedScale = 1.5f; ps.runSpeedScale = 1;
    ps.crouchSpeedScale = .5f; weaponDef[8].loadoutWeight = 4;
    VectorSet(ps.mins, -4, -4, 0); VectorSet(ps.maxs, 4, 4, 30);
    VectorCopy(ps.mins, move.mins); VectorCopy(ps.maxs, move.maxs);
    ps.crouchMaxZ = 15; ps.standViewHeight = 26; ps.crouchViewHeight = 12;
    move.cmd.serverTime = 10000; ext.sprintTime = 20000; pml.frametime = .05f;
    pml.walking = pml.groundPlane = qtrue; pml.groundTrace.plane.normal[2] = 1;
    pml.forward[0] = pml.right[1] = 1;
    g_gametype.integer = 2; g_movespeed.integer = 127;
    events = lastEvent = airCalls = traces = climbTraces = damagingFallEvents = 0;
    lastAnimation = -1;
    scriptedSlide = slideCalls = stepKind = worldKind = useRealAir = 0;
    stepZ = 0; firstX = 2; firstY = 0; secondX = 8; secondY = 0;
}

static void jumpContracts(void)
{
    int stamina, ledge, timer;
    const int staminas[] = { 0, 749, 750, 1499, 1500, 20000 };
    const int timers[] = { 0, 250, 500, 501, 1000 };
    for (ledge = 0; ledge < 2; ++ledge) for (stamina = 0; stamina < 6; ++stamina)
        for (timer = 0; timer < 5; ++timer) {
            int accepted;
            reset(); ps.velocity[0] = 100; move.cmd.forwardmove = move.cmd.upmove = 127;
            ps.stats[STAT_TCE_FLAGS] = ledge ? 0x40 : 0; ext.sprintTime = staminas[stamina];
            ps.pm_time = timers[timer]; accepted = PM_CheckJump();
            check(accepted == (staminas[stamina] >= 750 && timers[timer] <= 500),
                  "ledge assistance respects moving jump stamina and landing cooldown");
        }
    reset(); ps.velocity[0] = 100; move.cmd.forwardmove = move.cmd.upmove = 127;
    ps.stats[STAT_TCE_FLAGS] = 0x40;
    check(PM_CheckJump() && near(ps.velocity[2], 270), "paid ledge assistance retains full jump height");
    reset(); move.cmd.forwardmove = move.cmd.upmove = 127; ext.sprintTime = 0;
    check(PM_CheckJump(), "ordinary standing forward jump remains possible when tired");
    for (stamina = 2; stamina < 6; ++stamina) {
        int expected = staminas[stamina] < 1500 ? 0 : (int)(staminas[stamina] * .625f);
        reset(); ps.velocity[0] = 100; move.cmd.forwardmove = move.cmd.upmove = 127;
        ps.stats[STAT_TCE_FLAGS] = 0x40; ext.sprintTime = staminas[stamina];
        ext.jumpTime = move.cmd.serverTime - 600;
        PM_WalkMove();
        check(airCalls == 1 && ext.sprintTime == expected && ext.jumpTime == move.cmd.serverTime,
              "every accepted jump charges stamina even in previous850ms window");
    }
    reset(); move.trace = climbTrace; stepZ = 50; ext.sprintTime = 0;
    { vec3_t normal = { -1, 0, 0 };
      check(!PM_ClimbSlideMove(1, normal, qfalse) && (ps.stats[STAT_TCE_FLAGS] & 0x40),
            "low ledge still detected when stamina is empty"); }
    for (stamina = 0; stamina < 2; ++stamina) {
        vec3_t normal = { -1, 0, 0 };
        reset(); move.trace = climbTrace; stepZ = 80; ext.sprintTime = stamina ? 900 : 899;
        (void)PM_ClimbSlideMove(1, normal, qfalse);
        check(!!(ps.stats[STAT_TCE_FLAGS] & 0x20) == stamina,
              "high free-climb retains900 stamina gate");
    }
}

static void stepContracts(void)
{
    int kind;
    for (kind = 1; kind <= 4; ++kind) {
        reset(); scriptedSlide = 1; move.trace = stepTrace; stepKind = kind;
        if (kind == 4) stepZ = 4;
        ps.velocity[0] = 100; firstX = 8; secondX = kind == 1 ? 3 : 12;
        PM_StepSlideMove(qfalse);
        check(near(ps.origin[0], 8) && near(ps.origin[2], 0),
              "worse/solid/overhang/steep elevated trial restores normal slide");
        check(pml.impactSpeed != 999 && move.numtouch == 0,
              "rejected trial restores impact/touch state");
    }
    for (kind = 9; kind <= 18; kind += 9) {
        reset(); scriptedSlide = 1; move.trace = stepTrace; stepZ = (float)kind;
        ps.velocity[0] = 100; PM_StepSlideMove(qfalse);
        check(near(ps.origin[0], 8) && near(ps.origin[2], (float)kind),
              "valid recorded9/18 step accepted with step event");
        check(events == 1, "accepted raised step emits one event");
        reset(); worldKind = 4; stepZ = (float)kind; ps.velocity[0] = 300;
        PM_StepSlideMove(qfalse);
        check(ps.origin[0] > 6 && near(ps.origin[2], (float)kind),
              "actual collision solver crosses analytic9/18-unit stair");
    }
    reset(); worldKind = 7; stepZ = 9; ps.velocity[0] = 300;
    PM_StepSlideMove(qfalse);
    check(ps.origin[0] > 6 && near(ps.origin[2], 9), "valid low9-unit step fits below ceiling that excludes18");
}

static void slideContracts(void)
{
    int timer, flags;
    const int times[] = { 0, 1, 500, 1000 };
    for (timer = 0; timer < 4; ++timer) for (flags = 0; flags < 3; ++flags) {
        reset(); worldKind = 1; ps.velocity[0] = 200; ps.velocity[1] = 30;
        ps.pm_time = times[timer]; ps.pm_flags = flags == 1 ? PMF_TIME_KNOCKBACK : flags == 2 ? PMF_TIME_WATERJUMP : 0;
        PM_RealSlideMove(qfalse);
        check(ps.origin[0] <= 6.001f && ps.origin[1] > 0, "wall collision clips position and preserves sideward progress");
        if (!flags) check(fabsf(ps.velocity[0]) < 1, "landing timer must not restore velocity into collided wall");
        else if (times[timer]) check(near(ps.velocity[0], 200), "knockback/waterjump retains timed velocity policy");
        check(VectorLength(ps.velocity) <= 203, "wall slide cannot create unbounded speed");
        reset(); worldKind = 1; ps.velocity[0] = 200; ps.velocity[1] = 30; ps.eFlags = EF_PRONE;
        ps.pm_time = times[timer]; ps.pm_flags = flags == 1 ? PMF_TIME_KNOCKBACK : flags == 2 ? PMF_TIME_WATERJUMP : 0;
        PM_SlideMoveProne(qfalse);
        check(ps.origin[0] <= 6.001f && ps.origin[1] > 0, "prone wall collision preserves safe sideways progress");
        if (!flags) check(fabsf(ps.velocity[0]) < 1, "prone landing timer retains clipped wall velocity");
        else if (times[timer]) check(near(ps.velocity[0], 200), "prone knockback/waterjump retains impulse policy");
    }
}

static void landingAndDuckContracts(void)
{
    int stamina, velocity, height;
    const int staminas[] = { 0, 749, 750, 1499, 1500 };
    for (stamina = 0; stamina < 5; ++stamina) for (velocity = 0; velocity <= 200; velocity += 50)
        for (height = -18; height <= 18; height += 9) {
            reset(); ext.sprintTime = staminas[stamina]; pml.previous_velocity[2] = (float)-velocity;
            ps.origin[2] = (float)height; PM_CrashLand(qfalse);
            check(lastEvent != EV_FALL_NDIE && lastEvent != EV_TCE_FALL_DMG_75 &&
                  lastEvent != EV_FALL_DMG_50 && lastEvent != EV_FALL_DMG_25 &&
                  lastEvent != EV_FALL_DMG_15 && lastEvent != EV_FALL_DMG_10,
                  "ordinary18-unit correction below200 vertical speed causes no damaging fall");
        }
    reset(); pml.previous_velocity[2] = -700; ps.origin[2] = -20; PM_CrashLand(qfalse);
    check(lastEvent == EV_FALL_NDIE, "genuine high-speed fall remains fatal");
    reset(); ps.pm_flags = PMF_DUCKED; worldKind = 2; PM_CheckDuck();
    check(ps.pm_flags & PMF_DUCKED, "rail intersecting standing hull prevents unsafe uncrouch");
    reset(); ps.pm_flags = PMF_DUCKED; worldKind = 3; PM_CheckDuck();
    check(!(ps.pm_flags & PMF_DUCKED), "clear rail outside standing hull allows uncrouch");
}

static void groundSpeedContracts(void)
{
    int restriction, tick;
    float run, sprint;
    for (restriction = 0; restriction <= 3; ++restriction) {
        reset(); move.cmd.forwardmove = 127; pml.tceContentRestriction = restriction;
        for (tick = 0; tick < 30; ++tick) PM_WalkMove();
        run = ps.origin[0];
        reset(); move.cmd.forwardmove = 127; move.cmd.buttons = BUTTON_SPRINT;
        pml.tceContentRestriction = restriction;
        for (tick = 0; tick < 30; ++tick) PM_WalkMove();
        sprint = ps.origin[0];
        if (!restriction) check(sprint > run * 1.3f, "unrestricted grounded sprint is faster than run");
        else check(near(sprint, run), "map content restriction deliberately caps both run and sprint");
    }
}

static float replayStairs(int world, int stamina, int sprint, int jump)
{
    int tick;
    reset(); worldKind = world; ext.sprintTime = stamina; useRealAir = 1;
    move.cmd.forwardmove = 127; if (sprint) move.cmd.buttons = BUTTON_SPRINT;
    for (tick = 0; tick < 100; ++tick) {
        int oldDamagingFalls = damagingFallEvents;
        move.cmd.serverTime += 50;
        if (jump && !(tick % 16)) move.cmd.upmove = 127;
        else { move.cmd.upmove = 0; ps.pm_flags &= ~PMF_JUMP_HELD; }
        if (ps.pm_time > 50) ps.pm_time -= 50;
        else { ps.pm_time = 0; ps.pm_flags &= ~PMF_ALL_TIMES; }
        VectorCopy(ps.origin, pml.previous_origin); VectorCopy(ps.velocity, pml.previous_velocity);
        PM_GroundTrace();
        if (pml.walking) PM_WalkMove(); else PM_ActualAirMove();
        PM_GroundTrace();
        check(!TCE_FloatIsNaN(ps.origin[0]) && !TCE_FloatIsNaN(ps.origin[2]), "stair replay remains finite");
        check(damagingFallEvents == oldDamagingFalls,
              "sprint/jump over ordinary stairs cannot cause damaging fall");
    }
    return ps.origin[0];
}

static void stairReplayContracts(void)
{
    int stamina, world;
    const int staminas[] = { 0, 749, 750, 1499, 1500 };
    float run, sprint;
    for (world = 5; world <= 6; ++world) for (stamina = 0; stamina < 5; ++stamina)
        check(replayStairs(world, staminas[stamina], 1, 1) > 100,
              "low stamina sprint/jump makes progress on ordinary stairs");
    run = replayStairs(5, 20000, 0, 0);
    sprint = replayStairs(5, 20000, 1, 0);
    printf("Descending stair distance run %.3f sprint %.3f\n", run, sprint);
    check(sprint > run * 1.2f, "unrestricted descending stairs retain faster sprint progress");
}

#ifdef MOVEMENT_TEST_FOOTSTEPS
static void footstepContracts(void)
{
    const int weapons[] = { 4, 9, 30, 8 };
    const int postures[] = { PMF_DUCKED, EF_PRONE, EF_PRONE_MOVING };
    int weapon, lean;
    for (weapon = 0; weapon < 4; ++weapon) for (lean = -1; lean <= 1; ++lean) {
        usercmd_t command;
        reset(); ps.weapon = weapons[weapon]; weaponDef[ps.weapon].loadoutWeight = 4;
        ps.weaponstate = WEAPON_FIRING; ps.grenadeTimeLeft = weapon == 3 ? 0 : 3000;
        ps.leanf = (float)(lean * 20); move.cmd.forwardmove = 127; move.cmd.buttons = BUTTON_SPRINT;
        command = move.cmd; ps.velocity[0] = (float)(PM_CmdScale(&command) * move.cmd.forwardmove);
        ps.bobCycle = 32; pml.msec = 200; PM_Footsteps();
        printf("Footstep weapon %d lean %d actual speed %.3f event %d animation %d\n",ps.weapon,lean,pm->xyspeed,lastEvent,lastAnimation);
        check(pm->xyspeed > 220, "actual forward sprint remains fast while priming/leaning");
        check(lastEvent == EV_TCE_FOOTSTEP_SPRINT, "actual fast sprint uses audible sprint footsteps while leaning");
        check(lastAnimation == ANIM_MT_RUN, "actual fast sprint uses running gait while leaning");
        reset(); ps.leanf = (float)(lean * 20); move.cmd.forwardmove = 127;
        move.cmd.buttons = BUTTON_SPRINT; ps.velocity[0] = 100; ps.bobCycle = 32; pml.msec = 200;
        PM_Footsteps();
        check(lastEvent == (lean ? EV_TCE_FOOTSTEP_WALK : EV_FOOTSTEP), "slow lean remains quiet and ordinary slow motion remains run class");
        reset(); ps.leanf = (float)(lean * 20); move.cmd.forwardmove = 127;
        move.cmd.buttons = BUTTON_SPRINT; ps.velocity[0] = 0; ps.bobCycle = 32; pml.msec = 200;
        PM_Footsteps();
        check(lastEvent != EV_TCE_FOOTSTEP_SPRINT, "blocked sprint command cannot force sprint sound");
    }
    for (weapon = 0; weapon < 3; ++weapon) {
        reset(); ps.leanf = 20; move.cmd.forwardmove = 127; move.cmd.buttons = BUTTON_SPRINT;
        if (!weapon) ps.pm_flags = postures[weapon]; else ps.eFlags = postures[weapon];
        ps.velocity[0] = 300; ps.bobCycle = 32; pml.msec = 200; PM_Footsteps();
        check(lastEvent != EV_TCE_FOOTSTEP_SPRINT, "crouch/prone/transition does not acquire standing sprint sound");
    }
    reset(); ps.leanf = 20; move.cmd.forwardmove = 127; move.cmd.buttons = BUTTON_SPRINT;
    ps.velocity[0] = 220; ps.bobCycle = 32; pml.msec = 200; PM_Footsteps();
    check(lastEvent == EV_TCE_FOOTSTEP_WALK, "lean at exact220 threshold retains quiet gait");
    reset(); ps.leanf = 20; move.cmd.forwardmove = 127; move.cmd.buttons = BUTTON_SPRINT;
    ps.velocity[0] = 220.001f; ps.bobCycle = 32; pml.msec = 200; PM_Footsteps();
    check(lastEvent == EV_TCE_FOOTSTEP_SPRINT, "lean above220 threshold uses actual sprint gait");
    reset(); ps.leanf = 20; move.cmd.forwardmove = 127;
    ps.velocity[0] = 300; ps.bobCycle = 32; pml.msec = 200; PM_Footsteps();
    check(lastEvent == EV_TCE_FOOTSTEP_WALK, "fast momentum without sprint input does not force sprint gait");
}
#endif

int main(void)
{
    jumpContracts(); stepContracts(); slideContracts(); landingAndDuckContracts(); groundSpeedContracts(); stairReplayContracts();
#ifdef MOVEMENT_TEST_FOOTSTEPS
    footstepContracts();
#endif
    printf("%d movement contracts, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
