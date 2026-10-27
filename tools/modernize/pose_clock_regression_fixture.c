#include "src/game/q_shared.h"
#include "src/game/bg_public.h"
#include "src/game/bg_tce_pose_clock.h"
#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned checks, failures, callbackCount, clockFailures;
static animScriptData_t scriptData;
static animScriptData_t *globalScriptData = &scriptData;
static animModelInfo_t modelInfo;
static animation_t animations[4];
static playerState_t fixturePlayer;
static tcePoseClock_t channels[2];
static uint32_t generation = 9;
typedef struct { int part, previous, animation, time, timer; const animModelInfo_t *model; } assignment_t;
static assignment_t assignments[16];

#define CHECK(c) do { ++checks; if (!(c)) { ++failures; if (failures < 40) fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #c); } } while (0)

/* ACTUAL_BG_PLAYANIM */

static tcePoseClockClip_t clip(int first, int frames, int loops, int lerp, int lead, int reversed)
{
    tcePoseClockClip_t result;
    memset(&result, 0, sizeof(result));
    result.model = 3; result.firstFrame = first; result.numFrames = frames;
    result.loopFrames = loops; result.frameLerp = lerp; result.initialLerp = lead;
    result.reversed = reversed;
    return result;
}
static void observe(const playerState_t *state, const animModelInfo_t *model,
                    animBodyPart_t part, int previous, int animation)
{
    assignment_t *event;
    tcePoseClockClip_t value;
    const animation_t *anim = model->animations[animation & ~ANIM_TOGGLEBIT];
    int channel = part == ANIM_BP_LEGS ? 0 : 1;
    if (callbackCount >= 16) { ++clockFailures; return; }
    event = &assignments[callbackCount++];
    event->part = part; event->previous = previous; event->animation = animation;
    event->time = state->commandTime; event->model = model;
    event->timer = part == ANIM_BP_LEGS ? state->legsTimer : state->torsoTimer;
    value = clip(anim->firstFrame, anim->numFrames, anim->loopFrames,
                 anim->frameLerp, anim->initialLerp, !!(anim->flags & ANIMFL_REVERSED));
    if (!BG_TCEPoseClockAssign(&channels[channel], generation, animation, &value,
                              (uint32_t)state->commandTime)) ++clockFailures;
}
static void resetAssignments(void)
{
    int i;
    memset(&fixturePlayer, 0, sizeof(fixturePlayer)); fixturePlayer.commandTime = 1000;
    memset(assignments, 0, sizeof(assignments)); callbackCount = clockFailures = 0;
    for (i = 0; i < 2; ++i) BG_TCEPoseClockReset(&channels[i], generation, 1000);
}
static void assignmentTests(void)
{
    int body, timer, continuation, force, setTimer, same, toggled, result;
    playerState_t withoutObserver;
    int withoutResult;
    unsigned expected, scenarios = 0;
    for (body = ANIM_BP_LEGS; body <= ANIM_BP_BOTH; ++body)
    for (timer = 0; timer < 4; ++timer)
    for (continuation = 0; continuation < 2; ++continuation)
    for (force = 0; force < 2; ++force)
    for (setTimer = 0; setTimer < 2; ++setTimer)
    for (same = 0; same < 2; ++same)
    for (toggled = 0; toggled < 2; ++toggled)
    {
        static const int timers[] = { 0, 49, 50, 900 };
        ++scenarios;
        resetAssignments();
        fixturePlayer.legsAnim = fixturePlayer.torsoAnim = (same ? 1 : 2) | (toggled ? ANIM_TOGGLEBIT : 0);
        fixturePlayer.legsTimer = fixturePlayer.torsoTimer = timers[timer];
        withoutObserver = fixturePlayer;
        scriptData.animationAssigned = NULL;
        withoutResult = BG_PlayAnim(&withoutObserver, &modelInfo, 1, (animBodyPart_t)body,
                                   300, setTimer, continuation, force);
        scriptData.animationAssigned = observe;
        result = BG_PlayAnim(&fixturePlayer, &modelInfo, 1, (animBodyPart_t)body,
                            300, setTimer, continuation, force);
        CHECK(result == withoutResult);
        CHECK(!memcmp(&fixturePlayer, &withoutObserver, sizeof(fixturePlayer)));
        expected = (force || timers[timer] < 50) && (!continuation || !same) ? 1u : 0u;
        if (body == ANIM_BP_BOTH) expected *= 2u;
        CHECK(callbackCount == expected); CHECK(clockFailures == 0);
        if (expected)
        {
            CHECK(assignments[0].previous == ((same ? 1 : 2) | (toggled ? ANIM_TOGGLEBIT : 0)));
            CHECK(assignments[0].animation == (1 | (toggled ? 0 : ANIM_TOGGLEBIT)));
            CHECK(assignments[0].time == 1000 && assignments[0].model == &modelInfo);
            CHECK(assignments[0].timer == (setTimer ? 300 : timers[timer]));
        }
    }
    CHECK(scenarios == 384);
    resetAssignments(); scriptData.animationAssigned = observe;
    fixturePlayer.legsAnim = 1;
    CHECK(BG_PlayAnim(&fixturePlayer, &modelInfo, 1, ANIM_BP_LEGS, 0, qfalse, qfalse, qtrue) == 550);
    CHECK(BG_PlayAnim(&fixturePlayer, &modelInfo, 1, ANIM_BP_LEGS, 0, qfalse, qfalse, qtrue) == 550);
    CHECK(fixturePlayer.legsAnim == 1 && callbackCount == 2 && channels[0].epoch == 2);
    CHECK(assignments[0].animation == assignments[1].previous);
    CHECK(assignments[1].time == assignments[0].time);
    /* A loop continuation refreshes its lock, without restarting phase. */
    CHECK(BG_PlayAnim(&fixturePlayer, &modelInfo, 1, ANIM_BP_LEGS, 700, qtrue, qtrue, qtrue) == -1);
    CHECK(fixturePlayer.legsTimer == 700 && callbackCount == 2 && channels[0].epoch == 2);
    resetAssignments(); fixturePlayer.legsAnim = 0; animations[0].loopFrames = 0;
    CHECK(BG_PlayAnim(&fixturePlayer, &modelInfo, 0, ANIM_BP_LEGS, 700, qtrue, qtrue, qtrue) == -1);
    CHECK(fixturePlayer.legsTimer == 0 && callbackCount == 0 && channels[0].epoch == 0);
    animations[0].loopFrames = 2;
    resetAssignments(); fixturePlayer.legsTimer = 100; fixturePlayer.torsoTimer = 0;
    CHECK(BG_PlayAnim(&fixturePlayer, &modelInfo, 1, ANIM_BP_BOTH, 300, qtrue, qfalse, qfalse) == -1);
    CHECK(callbackCount == 1 && assignments[0].part == ANIM_BP_TORSO);
    CHECK(channels[0].epoch == 0 && channels[1].epoch == 1);
    resetAssignments(); fixturePlayer.legsTimer = 0; fixturePlayer.torsoTimer = 100;
    CHECK(BG_PlayAnim(&fixturePlayer, &modelInfo, 1, ANIM_BP_BOTH, 300, qtrue, qfalse, qfalse) == 300);
    CHECK(callbackCount == 1 && assignments[0].part == ANIM_BP_LEGS);
    resetAssignments(); fixturePlayer.legsAnim = 2; fixturePlayer.torsoAnim = 3;
    CHECK(BG_PlayAnim(&fixturePlayer, &modelInfo, 1, ANIM_BP_BOTH, 250, qtrue, qfalse, qtrue) == 250);
    CHECK(callbackCount == 2 && assignments[0].part == ANIM_BP_LEGS && assignments[1].part == ANIM_BP_TORSO);
    CHECK(assignments[0].previous == 2 && assignments[1].previous == 3);
    CHECK(channels[0].epoch == 1 && channels[1].epoch == 1 && clockFailures == 0);
    resetAssignments();
    CHECK(BG_PlayAnim(&fixturePlayer, &modelInfo, 1, ANIM_BP_UNUSED, 300, qtrue, qfalse, qtrue) == -1);
    CHECK(callbackCount == 0);
    globalScriptData = NULL;
    CHECK(BG_PlayAnim(&fixturePlayer, &modelInfo, 1, ANIM_BP_TORSO, 300, qtrue, qfalse, qtrue) == -1);
    CHECK(fixturePlayer.torsoTimer == 300 && callbackCount == 0);
    globalScriptData = &scriptData;
}
static tcePoseClockSample_t sample(const tcePoseClock_t *clock, uint32_t time)
{
    tcePoseClockSample_t value;
    memset(&value, 0, sizeof(value));
    CHECK(BG_TCEPoseClockSample(clock, time, &value));
    return value;
}
static void phaseTests(void)
{
    tcePoseClock_t clock, copy;
    tcePoseClockClip_t value = clip(10, 5, 0, 100, 0, 0);
    tcePoseClockSample_t out;
    BG_TCEPoseClockReset(&clock, 1, 0);
    CHECK(BG_TCEPoseClockAssign(&clock, 1, 7, &value, 0));
    copy = clock; out = sample(&clock, 50);
    CHECK(!memcmp(&clock, &copy, sizeof(clock)));
    CHECK(out.oldFrame == 10 && out.frame == 11 && out.backlerp == .5f && !out.finished);
    out = sample(&clock, 100); CHECK(out.oldFrame == 11 && out.frame == 12 && out.backlerp == 1);
    out = sample(&clock, 400); CHECK(out.oldFrame == 14 && out.frame == 14 && out.finished && out.held);
    out = sample(&clock, INT32_MAX); CHECK(out.oldFrame == 14 && out.frame == 14 && out.backlerp == 0);
    value.loopFrames = 2;
    CHECK(BG_TCEPoseClockAssign(&clock, 1, 7, &value, 0));
    out = sample(&clock, 450); CHECK(out.oldFrame == 14 && out.frame == 13 && out.backlerp == .5f);
    out = sample(&clock, 500); CHECK(out.oldFrame == 13 && out.frame == 14 && !out.finished);
    value.reversed = 1;
    CHECK(BG_TCEPoseClockAssign(&clock, 1, 7, &value, 0));
    out = sample(&clock, 0); CHECK(out.oldFrame == 14 && out.frame == 13);
    out = sample(&clock, 450); CHECK(out.oldFrame == 10 && out.frame == 11 && out.backlerp == .5f);
    value.initialLerp = 150; value.reversed = 0;
    CHECK(BG_TCEPoseClockAssign(&clock, 1, 7, &value, 0));
    out = sample(&clock, 149); CHECK(out.oldFrame == 10 && out.frame == 10 && out.held && !out.finished);
    out = sample(&clock, 150); CHECK(out.oldFrame == 10 && out.frame == 11 && out.backlerp == 1);
    out = sample(&clock, 200); CHECK(out.backlerp == .5f);
    CHECK(BG_TCEPoseClockSetRate(&clock, 200, 0));
    copy = clock; out = sample(&clock, 900);
    CHECK(out.elapsedQ16 == copy.elapsedQ16 && out.held && out.backlerp == .5f);
    CHECK(BG_TCEPoseClockSetRate(&clock, 900, TCE_POSE_CLOCK_RATE_ONE / 2));
    out = sample(&clock, 1100); CHECK(out.oldFrame == 11 && out.frame == 12 && out.backlerp == .5f);
    /* A newly accepted clip explicitly starts at normal rate, in a new epoch. */
    value = clip(INT_MAX, 1, 0, 1, 0, 0);
    CHECK(BG_TCEPoseClockAssign(&clock, 1, 8, &value, 1100));
    out = sample(&clock, 1100); CHECK(out.oldFrame == INT_MAX && out.frame == INT_MAX && out.finished);
    CHECK(clock.rateQ16 == TCE_POSE_CLOCK_RATE_ONE && clock.epoch == 5);
    value.loopFrames = 1;
    CHECK(BG_TCEPoseClockAssign(&clock, 1, 8, &value, 1100));
    out = sample(&clock, 1500); CHECK(out.oldFrame == INT_MAX && out.frame == INT_MAX && out.held && !out.finished);
}
static void invalidAndWrapTests(void)
{
    tcePoseClock_t clock, before;
    tcePoseClockClip_t good = clip(10, 5, 2, 67, 100, 0), bad;
    tcePoseClockSample_t output, saved;
    int trial;
    BG_TCEPoseClockReset(NULL, 0, 0);
    CHECK(!BG_TCEPoseClockValidClip(NULL));
    CHECK(!BG_TCEPoseClockAssign(NULL, 0, 0, &good, 0));
    CHECK(!BG_TCEPoseClockAdvance(NULL, 0)); CHECK(!BG_TCEPoseClockSample(NULL, 0, &output));
    BG_TCEPoseClockReset(&clock, 4, 100);
    CHECK(!BG_TCEPoseClockAdvance(&clock, 101));
    for (trial = 0; trial < 12; ++trial)
    {
        bad = good;
        switch (trial)
        {
        case 0: bad.model = -1; break;
        case 1: bad.firstFrame = -1; break;
        case 2: bad.numFrames = 0; break;
        case 3: bad.loopFrames = -1; break;
        case 4: bad.loopFrames = 16; break; /* Reachable outlier would be -1. */
        case 5: bad.frameLerp = 0; break;
        case 6: bad.frameLerp = -1; break;
        case 7: bad.initialLerp = -1; break;
        case 8: bad.reversed = 2; break;
        case 9: bad.firstFrame = INT_MAX; break;
        case 10: bad.numFrames = INT_MAX; bad.firstFrame = 0; bad.frameLerp = INT_MAX; break;
        case 11: bad.reversed = -1; break;
        }
        before = clock;
        CHECK(!BG_TCEPoseClockValidClip(&bad));
        CHECK(!BG_TCEPoseClockAssign(&clock, 4, 1, &bad, 100));
        CHECK(!memcmp(&before, &clock, sizeof(clock)));
    }
    before = clock;
    CHECK(!BG_TCEPoseClockAssign(&clock, 5, 1, &good, 100));
    CHECK(!BG_TCEPoseClockAssign(&clock, 4, -1, &good, 100));
    CHECK(!BG_TCEPoseClockAssign(&clock, 4, 1, &good, 99));
    CHECK(!memcmp(&before, &clock, sizeof(clock)));
    CHECK(BG_TCEPoseClockAssign(&clock, 4, 1, &good, 100));
    before = clock;
    CHECK(!BG_TCEPoseClockAdvance(&clock, 99));
    CHECK(!BG_TCEPoseClockSetRate(&clock, 101, TCE_POSE_CLOCK_RATE_MAX + 1));
    CHECK(!memcmp(&before, &clock, sizeof(clock)));
    memset(&output, 0xa5, sizeof(output)); saved = output;
    CHECK(!BG_TCEPoseClockSample(&clock, 99, &output));
    CHECK(!memcmp(&output, &saved, sizeof(output)));
    CHECK(!BG_TCEPoseClockSample(&clock, 100, NULL));
    clock.epoch = UINT32_MAX;
    CHECK(BG_TCEPoseClockAssign(&clock, 4, 1, &good, 100)); CHECK(clock.epoch == 1);
    BG_TCEPoseClockReset(&clock, 5, UINT32_MAX - 15u);
    CHECK(!clock.valid && clock.epoch == 0 && clock.generation == 5);
    CHECK(!BG_TCEPoseClockAssign(&clock, 4, 1, &good, UINT32_MAX - 15u));
    CHECK(BG_TCEPoseClockAssign(&clock, 5, 1, &good, UINT32_MAX - 15u));
    CHECK(BG_TCEPoseClockAdvance(&clock, 32));
    CHECK(clock.time == 32 && clock.elapsedQ16 == (uint64_t)48 * TCE_POSE_CLOCK_RATE_ONE);
    before = clock; CHECK(!BG_TCEPoseClockAdvance(&clock, 32u + 0x80000000u));
    CHECK(!memcmp(&before, &clock, sizeof(clock)));
    CHECK(BG_TCEPoseClockSetRate(&clock, 32, TCE_POSE_CLOCK_RATE_MAX));
    CHECK(BG_TCEPoseClockAdvance(&clock, 32u + INT32_MAX));
    output = sample(&clock, clock.time);
    CHECK(output.oldFrame >= 10 && output.oldFrame <= 14 && output.frame >= 10 && output.frame <= 14);
    /* Corrupt private-state inputs fail atomically rather than indexing frames. */
    clock.elapsedQ16 = UINT64_MAX; before = clock;
    CHECK(!BG_TCEPoseClockAdvance(&clock, clock.time)); CHECK(!memcmp(&before, &clock, sizeof(clock)));
}
static void chunkingTests(void)
{
    uint32_t rates[] = { 0, 1, TCE_POSE_CLOCK_RATE_ONE / 4, TCE_POSE_CLOCK_RATE_ONE,
                         104858u, TCE_POSE_CLOCK_RATE_MAX };
    unsigned rate, loops, reverse, lead;
    for (rate = 0; rate < sizeof(rates) / sizeof(rates[0]); ++rate)
    for (loops = 0; loops < 4; ++loops)
    for (reverse = 0; reverse < 2; ++reverse)
    for (lead = 0; lead < 2; ++lead)
    {
        tcePoseClockClip_t value = clip(2981, 21, loops ? (loops == 1 ? 7 : loops == 2 ? 21 : 34) : 0,
                                      67, lead ? 120 : 0, (int)reverse);
        tcePoseClock_t once, pieces;
        tcePoseClockSample_t a, b;
        uint32_t at = 0, finish = 123456;
        int minimum, maximum;
        CHECK(BG_TCEPoseClockFrameBounds(&value, &minimum, &maximum));
        BG_TCEPoseClockReset(&once, 8, 0);
        CHECK(BG_TCEPoseClockAssign(&once, 8, 13, &value, 0));
        CHECK(BG_TCEPoseClockSetRate(&once, 0, rates[rate])); pieces = once;
        CHECK(BG_TCEPoseClockAdvance(&once, finish));
        while (at < finish)
        {
            uint32_t step = at % 97u + 1u;
            at = finish - at < step ? finish : at + step;
            CHECK(BG_TCEPoseClockAdvance(&pieces, at));
        }
        CHECK(!memcmp(&once, &pieces, sizeof(once)));
        a = sample(&once, finish); b = sample(&pieces, finish);
        CHECK(!memcmp(&a, &b, sizeof(a)));
        CHECK(a.oldFrame >= minimum && a.oldFrame <= maximum && a.frame >= minimum && a.frame <= maximum);
        CHECK(a.backlerp >= 0 && a.backlerp <= 1);
    }
}
static void extendedCycleTests(void)
{
    /* Independently authenticated original CG_RunLerpFrameRate vectors. They
     * exercise its negative-offset clamp before looping, not a contiguous
     * thirteen-frame tail. Canonical samples use oldFrame at frame boundaries. */
    static const int forward[] = {2088,2089,2090,2078,2088,2089,2090,2078,2088,2089,2090,2078,2088};
    static const int reverse[] = {2090,2089,2088,2100,2090,2089,2088,2100,2090,2089,2088,2100,2090};
    tcePoseClockClip_t value;
    tcePoseClock_t clock, before;
    tcePoseClockSample_t out;
    unsigned int reversed, index;
    int frames, loops, expected, candidate, nextExpected, minimum, maximum;
    for (reversed = 0; reversed < 2; ++reversed)
    {
        const int *original = reversed ? reverse : forward;
        value = clip(2088, 3, 13, 45, 40, (int)reversed);
        CHECK(BG_TCEPoseClockFrameBounds(&value, &minimum, &maximum));
        CHECK(minimum == (reversed ? 2088 : 2078) && maximum == (reversed ? 2100 : 2090));
        BG_TCEPoseClockReset(&clock, 2, 1000);
        CHECK(BG_TCEPoseClockAssign(&clock, 2, 50, &value, 1000));
        out = sample(&clock, 1039); CHECK(out.held && out.oldFrame == original[0] && out.frame == original[0]);
        for (index = 0; index + 1 < sizeof(forward) / sizeof(forward[0]); ++index)
        {
            out = sample(&clock, 1040u + index * 45u);
            CHECK(out.oldFrame == original[index] && out.frame == original[index + 1]);
            CHECK(out.backlerp == 1 && !out.finished && !out.held);
            out = sample(&clock, 1040u + index * 45u + 15u);
            CHECK(out.oldFrame == original[index] && out.frame == original[index + 1]);
            CHECK(out.backlerp > .6666f && out.backlerp < .6667f);
        }
        CHECK(BG_TCEPoseClockSetRate(&clock, 1175, 0));
        before = clock; out = sample(&clock, 2000);
        CHECK(out.held && out.oldFrame == original[3] && out.frame == original[4]);
        CHECK(out.elapsedQ16 == before.elapsedQ16 && !memcmp(&before, &clock, sizeof(clock)));
        CHECK(BG_TCEPoseClockAssign(&clock, 2, 50 | ANIM_TOGGLEBIT, &value, 2000));
        out = sample(&clock, 2040); CHECK(out.oldFrame == original[0] && clock.epoch == 2);
        CHECK(clock.rateQ16 == TCE_POSE_CLOCK_RATE_ONE);
        clock.elapsedQ16 = ((uint64_t)40 + 4u * 45u) << 16; before = clock;
        CHECK(!BG_TCEPoseClockAdvance(&clock, clock.time));
        CHECK(!memcmp(&before, &clock, sizeof(clock)));
    }
    /* Vary the declared loop length and compare with the original recurrence,
     * including the one-frame first pass. This oracle steps the prior absolute
     * frame and clamps before remapping, independently of canonical phase. */
    for (frames = 1; frames <= 12; ++frames)
    for (loops = frames + 1; loops <= frames + 15; ++loops)
    for (reversed = 0; reversed < 2; ++reversed)
    {
        value = clip(50, frames, loops, 17, 9, (int)reversed);
        BG_TCEPoseClockReset(&clock, 2, 0);
        CHECK(BG_TCEPoseClockAssign(&clock, 2, 50, &value, 0));
        expected = value.firstFrame + (reversed ? frames - 1 : 0);
        for (index = 0; index < (unsigned int)(frames + 1) * 4u; ++index)
        {
            candidate = (reversed ? value.firstFrame + frames - 1 - expected : expected - value.firstFrame) + 1;
            if (candidate < 0) candidate = 0;
            if (candidate >= frames) { candidate -= frames; candidate %= loops; candidate += frames - loops; }
            nextExpected = value.firstFrame + (reversed ? frames - 1 - candidate : candidate);
            out = sample(&clock, 9u + index * 17u);
            CHECK(out.oldFrame == expected && out.frame == nextExpected && out.backlerp == 1);
            expected = nextExpected;
        }
    }
    value = clip(10, 1, 2, 1, 0, 0);
    BG_TCEPoseClockReset(&clock, 2, UINT32_MAX - 1u);
    CHECK(BG_TCEPoseClockAssign(&clock, 2, 1, &value, UINT32_MAX - 1u));
    CHECK(BG_TCEPoseClockSetRate(&clock, UINT32_MAX - 1u, TCE_POSE_CLOCK_RATE_MAX));
    out = sample(&clock, 1u); CHECK(out.oldFrame == 10 && out.frame == 9);
}
static void frameBoundsTests(void)
{
    tcePoseClockClip_t value = clip(10, 3, 13, 45, 40, 0);
    int minimum = -7, maximum = -9;
    CHECK(BG_TCEPoseClockFrameBounds(&value, &minimum, &maximum));
    CHECK(minimum == 0 && maximum == 12);
    value.firstFrame = 9;
    CHECK(!BG_TCEPoseClockFrameBounds(&value, &minimum, &maximum));
    CHECK(minimum == 0 && maximum == 12);
    value = clip(INT_MAX - 12, 3, 13, 45, 40, 1);
    CHECK(BG_TCEPoseClockFrameBounds(&value, &minimum, &maximum));
    CHECK(minimum == INT_MAX - 12 && maximum == INT_MAX);
    ++value.firstFrame;
    CHECK(!BG_TCEPoseClockFrameBounds(&value, &minimum, &maximum));
    CHECK(minimum == INT_MAX - 12 && maximum == INT_MAX);
    CHECK(!BG_TCEPoseClockFrameBounds(NULL, &minimum, &maximum));
    CHECK(!BG_TCEPoseClockFrameBounds(&value, NULL, &maximum));
    CHECK(!BG_TCEPoseClockFrameBounds(&value, &minimum, NULL));
    CHECK(!BG_TCEPoseClockFrameBounds(&value, &minimum, &minimum));
    value = clip(1, 131070, 131071, INT_MAX, 0, 0);
    CHECK(BG_TCEPoseClockValidClip(&value));
    value.numFrames += 2; value.loopFrames += 2;
    CHECK(!BG_TCEPoseClockValidClip(&value)); /* N+1 Q16 period would overflow. */
}
static void canonicalPhaseTests(void)
{
    tcePoseClockClip_t value;
    tcePoseClock_t clock;
    tcePoseClockSample_t out;
    uint64_t limit=17, canonical=19, again, prefix, period;
    unsigned int n,kind,reverse,lead;
    CHECK(!BG_TCEPoseClockPhaseLimit(NULL,&limit) && limit==17);
    CHECK(!BG_TCEPoseClockNormalizePhase(NULL,UINT64_MAX,&canonical) && canonical==19);
    for(n=1;n<=12;++n)for(kind=0;kind<4;++kind)for(reverse=0;reverse<2;++reverse)for(lead=0;lead<2;++lead)
    {
        value=clip(50,(int)n,kind==0?0:kind==1?(int)(n/2+1):kind==2?(int)n:(int)n+13,
                   67,lead?120:0,(int)reverse);
        CHECK(BG_TCEPoseClockPhaseLimit(&value,&limit));
        CHECK(!BG_TCEPoseClockPhaseLimit(&value,NULL));
        CHECK(!BG_TCEPoseClockNormalizePhase(&value,0,NULL));
        CHECK(BG_TCEPoseClockNormalizePhase(&value,limit,&canonical) && canonical==limit);
        CHECK(BG_TCEPoseClockNormalizePhase(&value,UINT64_MAX,&canonical) && canonical<=limit);
        CHECK(BG_TCEPoseClockNormalizePhase(&value,canonical,&again) && again==canonical);
        BG_TCEPoseClockReset(&clock,1,0);CHECK(BG_TCEPoseClockAssign(&clock,1,1,&value,0));
        clock.elapsedQ16=canonical;out=sample(&clock,0);CHECK(out.elapsedQ16==canonical);
        CHECK(BG_TCEPoseClockNormalizePhase(&value,(uint64_t)123456*TCE_POSE_CLOCK_RATE_ONE,&canonical));
        BG_TCEPoseClockReset(&clock,1,0);CHECK(BG_TCEPoseClockAssign(&clock,1,1,&value,0));
        CHECK(BG_TCEPoseClockAdvance(&clock,123456) && clock.elapsedQ16==canonical);
        if(value.loopFrames) {
            prefix=(uint64_t)value.initialLerp*TCE_POSE_CLOCK_RATE_ONE;
            if(value.loopFrames<=value.numFrames)prefix+=(uint64_t)(value.numFrames-value.loopFrames)*value.frameLerp*TCE_POSE_CLOCK_RATE_ONE;
            period=(uint64_t)(value.loopFrames>value.numFrames?value.numFrames+1:value.loopFrames)*value.frameLerp*TCE_POSE_CLOCK_RATE_ONE;
            CHECK(limit==prefix+period-1);
            CHECK(BG_TCEPoseClockNormalizePhase(&value,limit+1,&canonical) && canonical==prefix);
        } else {
            CHECK(limit==((uint64_t)value.initialLerp+(uint64_t)(n-1)*value.frameLerp)*TCE_POSE_CLOCK_RATE_ONE);
            CHECK(BG_TCEPoseClockNormalizePhase(&value,limit+1,&canonical) && canonical==limit);
        }
    }
    value=clip(0,3,13,45,40,0);limit=17;canonical=19;
    CHECK(!BG_TCEPoseClockPhaseLimit(&value,&limit) && limit==17);
    CHECK(!BG_TCEPoseClockNormalizePhase(&value,100,&canonical) && canonical==19);
}
static void largePeriodTests(void)
{
    tcePoseClock_t clock;
    tcePoseClockClip_t value = clip(0, 131071, 131071, INT_MAX, 0, 0);
    uint64_t period, prefix, delta;
    CHECK(BG_TCEPoseClockValidClip(&value));
    BG_TCEPoseClockReset(&clock, 2, 0);
    CHECK(BG_TCEPoseClockAssign(&clock, 2, 1, &value, 0));
    period = ((uint64_t)value.numFrames * value.frameLerp) << 16;
    clock.elapsedQ16 = period - TCE_POSE_CLOCK_RATE_ONE;
    CHECK(BG_TCEPoseClockSetRate(&clock, 0, TCE_POSE_CLOCK_RATE_MAX));
    CHECK(BG_TCEPoseClockAdvance(&clock, 100));
    CHECK(clock.elapsedQ16 == (uint64_t)399 * TCE_POSE_CLOCK_RATE_ONE);
    /* A near-UINT64 prefix plus a tail cycle must also avoid overflowing. */
    value.loopFrames = 2;
    CHECK(BG_TCEPoseClockAssign(&clock, 2, 1, &value, 100));
    prefix = ((uint64_t)(value.numFrames - value.loopFrames) * value.frameLerp) << 16;
    period = ((uint64_t)value.loopFrames * value.frameLerp) << 16;
    clock.elapsedQ16 = prefix + period - 1;
    CHECK(BG_TCEPoseClockSetRate(&clock, 100, TCE_POSE_CLOCK_RATE_MAX));
    CHECK(BG_TCEPoseClockAdvance(&clock, 100u + INT32_MAX));
    delta = (uint64_t)INT32_MAX * TCE_POSE_CLOCK_RATE_MAX;
    CHECK(clock.elapsedQ16 == prefix + (delta - 1) % period);
}
int main(void)
{
    int i;
    for (i = 0; i < 4; ++i)
    {
        modelInfo.animations[i] = &animations[i];
        animations[i].duration = 500; animations[i].firstFrame = i * 10;
        animations[i].numFrames = 5; animations[i].loopFrames = 2;
        animations[i].frameLerp = 100; animations[i].initialLerp = 50;
    }
    modelInfo.numAnimations = 4;
    assignmentTests(); phaseTests(); invalidAndWrapTests(); chunkingTests(); largePeriodTests();
    extendedCycleTests(); frameBoundsTests();
    canonicalPhaseTests();
    printf("pose clock: %u checks, %u failures\n", checks, failures);
    return failures != 0;
}
