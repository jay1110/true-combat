/* Focused production-state regression fixture. Engine/animation callbacks are
 * deterministic services; all decisions under test are extracted unmodified. */
#include "src/game/g_local.h"
#include "src/game/bg_local.h"
#include "src/game/tce_bg.h"
#include <stdio.h>
#include <string.h>

pmove_t *pm;
pml_t pml;
tce_weaponDef_t weaponDef[TCE_MAX_WEAPONS];
int weapAlts[WP_NUM_WEAPONS];
int weapBanksMultiPlayer[MAX_WEAP_BANKS_MP][MAX_WEAPS_IN_BANK_MP];
/* ACTUAL_WEAPON_BANKS */
#include "src/game/tce_weapon_ammo.c"
gentity_t g_entities[MAX_GENTITIES];
level_locals_t level;
vmCvar_t g_gametype, g_gamestate;
static int events, lastEvent, animation, launches, changes, failures, checks;
static int plants, defuses, completes, stops;
static playerState_t ps;
static pmove_t move;
static pmoveExt_t ext;
static bg_character_t character;
static int skills[SK_NUM_SKILLS];
static gclient_t client;
static gentity_t popup, dropped;

struct { playerState_t ps; } snapshot;
struct {
    int time, weaponSelect, tceAimRequested, tceAimComplete, tceAimActive, tceAimSyncTime;
    playerState_t predictedPlayerState;
    struct { playerState_t ps; } *snap;
} cg;

enum { tceSprintLatched = 0x4000, tceAttackButtonLatched = 0x400 };
static int PM_TCEAddWeaponCounter(int, int);
static int PM_TCEElapsedWeaponCounter(int, int);
static int PM_ProneTimeValue(unsigned bits) { int value; memcpy(&value, &bits, sizeof(value)); return value; }
static qboolean PM_WeaponChargePending(int elapsed, int charge, float fraction) { return elapsed < charge * fraction; }
static int TCE_FloatIsZeroOrNaN(float value) { return value == 0 || value != value; }
void PM_AddEvent(int event)
{
    ++events; lastEvent = event;
    if (event == EV_TCE_PLANT) ++plants;
    if (event == EV_TCE_DEFUSE) ++defuses;
    if (event == EV_TCE_OBJECTIVE_COMPLETE) ++completes;
    if (event == EV_TCE_OBJECTIVE_STOP) ++stops;
}
static void PM_StartWeaponAnim(int anim) { animation = anim; }
void PM_ContinueWeaponAnim(int anim) { animation = anim; }
int PM_IdleAnimForWeapon(int weapon) { return WEAP_IDLE1; }
static int TCE_PM_IdleAnimForWeapon(int weapon) { return WEAP_IDLE1; }
static int PM_DropAnimForWeapon(int weapon) { return WEAP_DROP; }
int PM_AltSwitchFromForWeapon(int weapon) { return WEAP_DROP; }
int PM_AltSwitchToForWeapon(int weapon) { return WEAP_RAISE; }
int PM_RaiseAnimForWeapon(int weapon) { return WEAP_RAISE; }
void BG_UpdateConditionValue(int clientNum,int condition,int value,qboolean checkConversion) { }
static void PM_WeaponAddTime(int interval) { pm->ps->weaponTime += interval; }
static void PM_ReloadClip(int weapon) { ++failures; puts("Unexpected ammo commit"); }
static void PM_BeginWeaponReload(int weapon) { pm->ps->weaponstate = WEAPON_RELOADING; }
static void PM_FinishWeaponReload(void) { pm->ps->weaponstate = WEAPON_READY; }
static void PM_CheckForReload(int weapon) { }
static qboolean PM_TCEWeaponIsFiring(void) { return pm->ps->weaponstate == WEAPON_FIRING || pm->ps->weaponstate == WEAPON_FIRINGALT; }
static qboolean PM_WeaponClipEmpty(int weapon) { return pm->ps->ammoclip[weapon] == 0; }
static void PM_TCEContinueBurst(void) { }
static qboolean PM_TCEHeldTriggerStops(playerState_t **timer) { *timer = pm->ps; return qtrue; }
static void PM_TCEStopTrigger(playerState_t *timer) { timer->weaponstate = WEAPON_READY; }
static qboolean PM_TCEUpdateAimInput(qboolean delayed) { return qfalse; }
int BG_AnimScriptEvent(playerState_t *state, animModelInfo_t *model, scriptAnimEventTypes_t event, qboolean cont, qboolean force) { return 0; }
void AxisToAngles(vec3_t axis[3], vec3_t angles) { }
void CrossProduct(const vec3_t first, const vec3_t second, vec3_t out)
{
    out[0] = first[1] * second[2] - first[2] * second[1];
    out[1] = first[2] * second[0] - first[0] * second[2];
    out[2] = first[0] * second[1] - first[1] * second[0];
}
qboolean COM_BitCheck(const int array[], int bit) { return (array[bit / 32] & (1u << (bit % 32))) != 0; }
void COM_BitClear(int array[], int bit) { array[bit / 32] &= ~(1u << (bit % 32)); }
weapon_t BG_FindAmmoForWeapon(weapon_t weapon) { return (weapon_t)TCE_BG_FindAmmoForWeapon(weapon); }
weapon_t BG_FindClipForWeapon(weapon_t weapon) { return (weapon_t)TCE_BG_FindClipForWeapon(weapon); }
int BG_FiremodeWeapon(int weapon) { return 0; }
static gentity_t *G_TCELaunchObjective(gentity_t *entity, qboolean vip) { ++launches; ++level.tceBombDropCount; return &dropped; }
gentity_t *G_PopupMessage(popupMessageType_t type) { return &popup; }
static int TCE_SubInt32(int first, int second) { return first - second; }
static void CG_Printf(const char *format, ...) { }
static void CG_FinishWeaponChange(int previous, int next) { cg.weaponSelect = next; ++changes; }
static int getAltWeapon(int weapon) { return weapon; }
static int getEquivWeapon(int weapon) { return weapon; }
static int getNextWeapInBank(int bank, int cycle) { return weapBanksMultiPlayer[bank][cycle]; }
static void CG_NextWeap(qboolean switchBanks) { }

/* PRODUCTION_FUNCTIONS */

static void check(int condition, const char *label)
{
    ++checks;
    if (!condition) { ++failures; printf("FAIL %s\n", label); }
}
static void reset(void)
{
    memset(&ps, 0, sizeof(ps)); memset(&move, 0, sizeof(move));
    memset(&ext, 0, sizeof(ext)); memset(&character, 0, sizeof(character));
    memset(&pml, 0, sizeof(pml)); memset(&cg, 0, sizeof(cg));
    memset(weaponDef, 0, sizeof(weaponDef));
    pm = &move; move.ps = &ps; move.pmext = &ext; move.character = &character; move.skill = skills;
    ps.weapon = 8; ps.stats[STAT_HEALTH] = 100; ps.ammoclip[8] = 20; move.cmd.weapon = 8;
    cg.snap = (void *)&snapshot;
    events = lastEvent = changes = 0; animation = -1;
    plants = defuses = completes = stops = 0;
}
/* The ordinary, unmounted PM_Weapon path calls these real production stages
 * in this order and returns immediately when Cycle does. No timer/action
 * decision is duplicated here; the fixture drives complete command sequences. */
static void objective_command(int msec)
{
    qboolean delayed = qfalse;
    pml.msec = msec;
    if (PM_TCEUpdateWeaponCycle(qfalse, &delayed)) {
#if TCE_CALLER_CANCELS_OBJECTIVE
        PM_TCECancelInterruptedObjective();
#endif
    } else {
        PM_TCEUpdateWeaponActions(delayed);
    }
}

static void objective_sequences(void)
{
    int i, fast, hintIndex;
    const int hints[] = { HINT_BREAKABLE_DYNAMITE, HINT_DISARM, HINT_ACTIVATE };
    for (hintIndex = 0; hintIndex < 3; ++hintIndex) {
        for (fast = 0; fast <= 1; ++fast) {
            const int duration = hints[hintIndex] == HINT_DISARM ?
                (fast ? 5000 : 10000) : (fast ? 2000 : 4000);
            reset(); move.cmd.buttons = BUTTON_ACTIVATE | BUTTON_SPRINT;
            ps.serverCursorHint = hints[hintIndex]; ps.stats[STAT_TCE_FLAGS] = fast ? 2 : 0;
            objective_command(100);
            check((ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && ps.weaponTime == duration,
                  "stationary sprint+activate starts exact full objective duration");
            objective_command(500);
            check(ps.weaponTime == duration - 500, "held stationary objective counts elapsed command time");
            move.cmd.forwardmove = 127;
            objective_command(100);
            check(!(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && stops == 1 &&
                  !plants && !defuses && !completes,
                  "held sprint+activate departure cancels unfinished action through Cycle then Actions");
            for (i = 0; i < 50; ++i) objective_command(100);
            check(!(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && stops == 1 &&
                  !plants && !defuses && !completes,
                  "five seconds sprinting with activate never completes or repeats stop");
            move.cmd.forwardmove = 0;
            for (i = 0; i < 3 && !(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION); ++i)
                objective_command(100);
            check((ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && ps.weaponTime == duration,
                  "return with both keys still held restarts full objective timer");
        }
    }
    reset(); move.cmd.buttons = BUTTON_ACTIVATE | BUTTON_SPRINT;
    ps.serverCursorHint = HINT_BREAKABLE_DYNAMITE;
    objective_command(100); move.cmd.forwardmove = 127; objective_command(4000);
    check(stops == 1 && !plants && !(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION),
          "departure on exact expiry cancels before plant event");
    reset(); move.cmd.buttons = BUTTON_ACTIVATE; ps.serverCursorHint = HINT_BREAKABLE_DYNAMITE;
    objective_command(100); ps.serverCursorHint = HINT_NONE; objective_command(4000);
    check(stops == 1 && !plants && !(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION),
          "lost target on exact expiry cancels through full command stages");
    reset(); move.cmd.buttons = BUTTON_ACTIVATE; ps.serverCursorHint = HINT_BREAKABLE_DYNAMITE;
    objective_command(100); move.cmd.buttons = 0; objective_command(4000);
    check(stops == 1 && !plants && !(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION),
          "released activate on exact expiry cancels through full command stages");
    reset(); move.cmd.buttons = BUTTON_ACTIVATE; ps.serverCursorHint = HINT_BREAKABLE_DYNAMITE;
    objective_command(100);
    for (i = 0; i < 39; ++i) objective_command(100);
    check(!plants && (ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && ps.weaponTime == 100,
          "uninterrupted ordinary plant cannot complete before four seconds");
    objective_command(100);
    check(plants == 1 && !stops && !(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && ps.weaponTime == 500,
          "uninterrupted plant completes exactly once at four seconds");
    reset(); move.cmd.buttons = BUTTON_ACTIVATE; ps.serverCursorHint = HINT_BREAKABLE_DYNAMITE;
    objective_command(100); pml.ladder = qtrue; objective_command(100);
    check(stops == 1 && !plants && !(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION),
          "ladder departure cancels before ladder early return");
    reset(); move.cmd.buttons = BUTTON_ACTIVATE; ps.serverCursorHint = HINT_BREAKABLE_DYNAMITE;
    ps.eFlags = EF_PRONE | EF_PRONE_MOVING;
    objective_command(100);
    check(!(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && ps.weaponTime == 0 && !events,
          "prone Cycle early return cannot start a new valid-target action");
    ps.weaponTime = 321; ps.serverCursorHint = HINT_NONE;
    objective_command(100);
    check(ps.weaponTime == 321 && !events,
          "early cancellation with no action preserves unrelated paused weapon timer");
    reset(); move.cmd.buttons = BUTTON_ACTIVATE | BUTTON_SPRINT; move.cmd.forwardmove = 127;
    ps.pm_flags = PMF_DUCKED; ps.serverCursorHint = HINT_BREAKABLE_DYNAMITE;
    objective_command(100); objective_command(100);
    check((ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && ps.weaponTime == 3900 && !stops,
          "valid crouched movement retains ordinary plant eligibility and timer");
    reset(); move.cmd.buttons = BUTTON_ACTIVATE | BUTTON_SPRINT; move.cmd.forwardmove = 127;
    ps.eFlags = EF_PRONE; ps.serverCursorHint = HINT_BREAKABLE_DYNAMITE;
    objective_command(100);
    check((ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && ps.weaponTime == 4000 && !stops,
          "valid stationary-prone posture retains ordinary action eligibility");
    ps.eFlags |= EF_PRONE_MOVING;
    objective_command(100);
    check((ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && ps.weaponTime == 4000 && !stops,
          "valid-target prone Cycle preserves existing paused-timer policy");
    reset(); move.cmd.buttons = BUTTON_ACTIVATE; ps.serverCursorHint = HINT_BREAKABLE_DYNAMITE;
    objective_command(100); objective_command(500);
    ps.eFlags = EF_PRONE | EF_PRONE_MOVING; move.cmd.forwardmove = 127;
    ps.serverCursorHint = HINT_NONE;
    objective_command(100);
    check(stops == 1 && !(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION),
          "prone-moving lost target must cancel despite Cycle early return");
    for (i = 0; i < 50; ++i) objective_command(100);
    check(stops == 1 && !(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && !plants,
          "crawling away for five seconds cannot preserve unfinished plant");
    ps.eFlags = 0; move.cmd.forwardmove = 0; ps.serverCursorHint = HINT_BREAKABLE_DYNAMITE;
    for (i = 0; i < 3 && !(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION); ++i)
        objective_command(100);
    check((ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && ps.weaponTime == 4000,
          "return from prone movement restarts full duration rather than saved progress");
}

static void grenade_return_sequences(void)
{
    int request,frame,oldChecks=checks;
    const int requests[]={9,45,2};
    for(request=0;request<3;++request) {
        reset();memcpy(weapBanksMultiPlayer,actualWeaponBanks,sizeof(weapBanksMultiPlayer));
        /* Captured real assault loadout: primary M16/45 clip30 reserve90,
         * sidearm2 clip15 reserve45, one frag9 and knife1. */
        ps.weapon=9;ps.weaponstate=WEAPON_FIRING;ps.weaponTime=1576;
        ps.weapons[0]=(1u<<1)|(1u<<2)|(1u<<9);ps.weapons[1]=1u<<(45-32);
        ps.ammoclip[9]=0;ps.ammoclip[45]=30;ps.ammo[45]=90;
        ps.ammoclip[2]=15;ps.ammo[2]=45;ps.ammo[1]=1;
        PM_SwitchIfEmpty();
        check(lastEvent==EV_NOAMMO&&!COM_BitCheck(ps.weapons,9),"actual frag depletion emits NOAMMO and removes owned frag");
        cg.predictedPlayerState=ps;snapshot.ps=ps;cg.weaponSelect=requests[request];
        check(CG_WeaponSelectable(45)&&CG_WeaponHasAmmo(45),"captured primary45 is really owned and usable through actual helpers");
        CG_OutOfAmmoChange(qtrue);
        check(cg.weaponSelect==(request==2?2:45),"NOAMMO preserves pending firearm request while actual weapon is depleted frag9");
        move.cmd.weapon=cg.weaponSelect;move.cmd.buttons=0;
        for(frame=0;frame<50&&ps.weapon==9;++frame)objective_command(50);
        check(ps.weapon==(request==2?2:45)&&ps.weaponstate==WEAPON_RAISING,"real drop completion raises requested firearm instead of knife");
        for(frame=0;frame<10&&ps.weaponTime>0;++frame)objective_command(50);
        PM_TCEUpdateWeaponTrigger(qfalse);
        check(ps.weaponstate==WEAPON_READY&&animation==WEAP_IDLE1,"returned firearm finishes real raise path visibly ready");
        check(ps.ammoclip[45]==30&&ps.ammo[45]==90,"return selection does not consume or refill primary ammunition");
    }
    reset();memcpy(weapBanksMultiPlayer,actualWeaponBanks,sizeof(weapBanksMultiPlayer));
    cg.weaponSelect=45;cg.predictedPlayerState.weapon=45;snapshot.ps.weapon=9;
    cg.predictedPlayerState.weapons[0]=(1u<<1)|(1u<<2);cg.predictedPlayerState.weapons[1]=1u<<(45-32);
    cg.predictedPlayerState.ammoclip[45]=30;cg.predictedPlayerState.ammoclip[2]=15;
    CG_OutOfAmmoChange(qtrue);
    check(cg.weaponSelect==45,"older grenade snapshot event cannot replace already raising primary45");
    cg.predictedPlayerState.weapon=9;cg.predictedPlayerState.ammoclip[45]=0;
    CG_OutOfAmmoChange(qtrue);
    check(cg.weaponSelect==2,"unusable requested primary falls back to genuinely usable sidearm");
    cg.weaponSelect=9;cg.predictedPlayerState.weapons[1]=0;cg.predictedPlayerState.ammoclip[45]=30;
    CG_OutOfAmmoChange(qtrue);
    check(cg.weaponSelect==2,"unowned firearm with stale ammo never becomes return selection");
    cg.weaponSelect=9;cg.predictedPlayerState.weapons[0]=1u<<1;
    CG_OutOfAmmoChange(qtrue);
    check(cg.weaponSelect==1,"no usable firearm retains knife fallback");
    printf("grenade return request/actual-state controls: %d checks\n",checks-oldChecks);
}

int main(void)
{
    int i, before;
    qboolean delayed;
    const int reloadStates[] = { WEAPON_RELOADING, WEAPON_TCE_RELOAD_START, WEAPON_TCE_RELOAD_END };
    for (i = 0; i < 3; ++i) {
        reset(); ps.weapons[0] = 1u << 2; ps.weaponstate = reloadStates[i]; ps.weaponTime = 2500;
        PM_BeginWeaponChange(8, 2, qfalse);
        check(ps.weaponstate == WEAPON_DROPPING && ps.weaponTime == 250 && ps.nextWeapon == 2,
              "weapon change cancels reload immediately");
        check(ps.ammoclip[8] == 20, "cancelled reload does not refill magazine");
    }
    reset(); ps.weapons[0] = 1u << 2; ps.grenadeTimeLeft = 1000; ps.weaponTime = 1500;
    PM_BeginWeaponChange(8, 2, qfalse);
    check(ps.weaponTime == 1500 && ps.weaponstate == WEAPON_READY, "cannot cancel a live grenade");
    reset(); ps.weaponstate = WEAPON_RAISING; ps.pm_flags = tceAttackButtonLatched;
    move.cmd.buttons = BUTTON_ATTACK;
    PM_TCEUpdateWeaponTrigger(qfalse);
    check(ps.weaponstate == WEAPON_READY && animation == WEAP_IDLE1, "held attack completes weapon raise animation");
    for (i = -1; i <= 1; ++i) {
        reset(); ps.leanf = i * 10.0f; move.cmd.buttons = BUTTON_ATTACK;
        check(!PM_TCEWeaponEligibility837(qfalse), "hip fire remains eligible for both lean directions");
    }
    reset(); move.cmd.buttons = BUTTON_ACTIVATE; ps.serverCursorHint = HINT_BREAKABLE_DYNAMITE;
    PM_TCEUpdateObjectiveAction();
    check((ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && ps.weaponTime == 4000, "plant starts with full timer");
    move.cmd.buttons |= BUTTON_SPRINT; move.cmd.forwardmove = 127;
    PM_TCEUpdateWeaponActions(qfalse);
    check(!(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && lastEvent == EV_TCE_OBJECTIVE_STOP,
          "sprint cancels plant before the sprint early return");
    move.cmd.buttons &= ~BUTTON_SPRINT; move.cmd.forwardmove = 0; ps.weaponTime = 0;
    PM_TCEUpdateObjectiveAction();
    check((ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && ps.weaponTime == 4000, "returning to plant starts full timer");
    ps.serverCursorHint = 0; ps.weaponTime = 0; before = events;
    PM_TCEUpdateObjectiveAction();
    check(!(ps.pm_flags & PMF_TCE_OBJECTIVE_ACTION) && events == before + 1 && lastEvent == EV_TCE_OBJECTIVE_STOP,
          "lost plant target cannot emit completion even when timer expires");
    reset(); ps.weapon = 9; ps.grenadeTimeLeft = 300; ps.weaponDelay = 0; weaponDef[9].grenadeTimer = 3000;
    move.cmd.buttons = 0; delayed = qfalse;
    PM_TCEUpdateWeaponCycle(qfalse, &delayed);
    check(delayed, "released grenade completes delayed fire");
    reset(); cg.time = 3000; cg.tceAimRequested = cg.tceAimActive = 1;
    cg.predictedPlayerState.stats[STAT_TCE_WEAPON_FLAGS] = 4; snapshot.ps.stats[STAT_TCE_WEAPON_FLAGS] = 0;
    reconcileAim();
    check(cg.tceAimRequested && cg.tceAimActive, "old snapshot cannot cancel predicted aim");
    cg.predictedPlayerState.stats[STAT_TCE_WEAPON_FLAGS] = 0; snapshot.ps.stats[STAT_TCE_WEAPON_FLAGS] = 4;
    reconcileAim();
    check(!cg.tceAimRequested && !cg.tceAimActive, "reconcile aim with current prediction");
    reset(); memset(weapBanksMultiPlayer, 0, sizeof(weapBanksMultiPlayer));
    weapBanksMultiPlayer[1][0] = 8; weapBanksMultiPlayer[2][0] = 2; weapBanksMultiPlayer[3][0] = 1;
    cg.predictedPlayerState.weapons[0] = (1u << 1) | (1u << 2) | (1u << 8);
    cg.predictedPlayerState.ammoclip[8] = 10; cg.predictedPlayerState.ammoclip[2] = 10;
    for (i = 0; i < 3; ++i) {
        const int grenades[] = { 4, 9, 30 };
        cg.weaponSelect = grenades[i]; CG_OutOfAmmoChange(qtrue);
        check(cg.weaponSelect == 8, "exhausted grenade selects TC primary bank instead of knife");
    }
    cg.weaponSelect = 9; cg.predictedPlayerState.ammoclip[8] = 0; CG_OutOfAmmoChange(qtrue);
    check(cg.weaponSelect == 2, "empty primary falls back to usable pistol");
    memset(&level, 0, sizeof(level)); memset(&client, 0, sizeof(client));
    g_gametype.integer = 5; g_gamestate.integer = GS_PLAYING; g_entities[0].client = &client;
    level.tceBombAssigned = qtrue; level.tceBombCarrier = 0; client.ps.clientNum = 0;
    client.sess.sessionTeam = TEAM_AXIS; client.ps.stats[STAT_HEALTH] = 100;
    launches = 0;
    for (i = 0; i < 8; ++i) {
        level.tceBombCarrier = 0; level.tceBombCarrierCount = 1;
        client.ps.stats[STAT_TCE_WEAPON_FLAGS] |= 0x100;
        G_TCEReleaseObjectives(&g_entities[0], qfalse, qfalse);
    }
    check(launches == 8 && level.tceBombCarrier == -1 && level.tceBombCarrierCount == 0,
          "bomb remains droppable through eight deaths/pickups");
    level.tceBombCarrier = 0; level.tceBombCarrierCount = 1;
    client.ps.stats[STAT_TCE_WEAPON_FLAGS] |= 0x100;
    check(G_TCEManualDropBomb(&g_entities[0]) && level.tceBombCarrier == -1, "living carrier can manually drop bomb");
    before = launches;
    check(!G_TCEManualDropBomb(&g_entities[0]) && launches == before, "manual drop cannot duplicate bomb");
    level.tceBombCarrier = 0; client.ps.stats[STAT_TCE_WEAPON_FLAGS] |= 0x100; level.tceBombPlanted = qtrue;
    check(!G_TCEManualDropBomb(&g_entities[0]), "planted bomb cannot be manually dropped");
    level.tceBombPlanted = qfalse; client.ps.pm_flags = PMF_TCE_OBJECTIVE_ACTION;
    check(!G_TCEManualDropBomb(&g_entities[0]), "active planting blocks manual drop");
    objective_sequences();grenade_return_sequences();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
