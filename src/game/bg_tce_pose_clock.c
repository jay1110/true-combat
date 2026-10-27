#include "bg_tce_pose_clock.h"
#include <limits.h>
#include <string.h>

int BG_TCEPoseClockFrameBounds(const tcePoseClockClip_t *clip, int *minimum, int *maximum)
{
    uint64_t frames, milliseconds;
    int64_t low, high;
    if (!clip || !minimum || !maximum || minimum == maximum ||
        clip->model < 0 || clip->firstFrame < 0 || clip->numFrames < 1 ||
        clip->loopFrames < 0 ||
        clip->frameLerp < 1 || clip->initialLerp < 0 ||
        (clip->reversed != 0 && clip->reversed != 1))
    {
        return 0;
    }
    low = clip->firstFrame;
    high = (int64_t)clip->firstFrame + clip->numFrames - 1;
    frames = (uint64_t)clip->numFrames;
    if (clip->loopFrames > clip->numFrames)
    {
        ++frames;
        if (clip->reversed)
        {
            high = (int64_t)clip->firstFrame + clip->loopFrames - 1;
        }
        else
        {
            low = (int64_t)clip->firstFrame + clip->numFrames - clip->loopFrames;
        }
    }
    milliseconds = frames * (uint64_t)clip->frameLerp +
                   (uint64_t)clip->initialLerp;
    if (low < 0 || high > INT_MAX || milliseconds > (UINT64_MAX >> 16))
    {
        return 0;
    }
    *minimum = (int)low;
    *maximum = (int)high;
    return 1;
}

int BG_TCEPoseClockValidClip(const tcePoseClockClip_t *clip)
{
    int minimum, maximum;
    return BG_TCEPoseClockFrameBounds(clip, &minimum, &maximum);
}

/* Shared checked-clip span: a zero period denotes the nonloop terminal cap. */
static void BG_TCEPoseClockSpan(const tcePoseClockClip_t *clip, uint64_t *prefix, uint64_t *period)
{
    if (clip->loopFrames > clip->numFrames)
    {
        *prefix = (uint64_t)clip->initialLerp << 16;
        *period = (((uint64_t)clip->numFrames + 1u) * clip->frameLerp) << 16;
    }
    else if (clip->loopFrames)
    {
        *prefix = ((uint64_t)clip->initialLerp +
                   (uint64_t)(clip->numFrames - clip->loopFrames) * clip->frameLerp) << 16;
        *period = ((uint64_t)clip->loopFrames * clip->frameLerp) << 16;
    }
    else
    {
        *prefix = ((uint64_t)clip->initialLerp +
                   (uint64_t)(clip->numFrames - 1) * clip->frameLerp) << 16;
        *period = 0;
    }
}

int BG_TCEPoseClockPhaseLimit(const tcePoseClockClip_t *clip, uint64_t *maximumQ16)
{
    uint64_t prefix, period;
    if (!maximumQ16 || !BG_TCEPoseClockValidClip(clip))
    {
        return 0;
    }
    BG_TCEPoseClockSpan(clip, &prefix, &period);
    *maximumQ16 = period ? prefix + period - 1u : prefix;
    return 1;
}

int BG_TCEPoseClockNormalizePhase(const tcePoseClockClip_t *clip, uint64_t elapsedQ16,
                                 uint64_t *canonicalQ16)
{
    uint64_t prefix, period;
    if (!canonicalQ16 || !BG_TCEPoseClockValidClip(clip))
    {
        return 0;
    }
    BG_TCEPoseClockSpan(clip, &prefix, &period);
    if (period)
    {
        *canonicalQ16 = elapsedQ16 < prefix ? elapsedQ16 : prefix + (elapsedQ16 - prefix) % period;
    }
    else
    {
        *canonicalQ16 = elapsedQ16 < prefix ? elapsedQ16 : prefix;
    }
    return 1;
}

void BG_TCEPoseClockReset(tcePoseClock_t *clock, uint32_t generation, uint32_t time)
{
    if (!clock)
    {
        return;
    }
    memset(clock, 0, sizeof(*clock));
    clock->generation = generation;
    clock->time = time;
    clock->rateQ16 = TCE_POSE_CLOCK_RATE_ONE;
}

int BG_TCEPoseClockAssign(tcePoseClock_t *clock, uint32_t generation, int animation,
                         const tcePoseClockClip_t *clip, uint32_t time)
{
    tcePoseClock_t next;
    uint32_t elapsed;
    if (!clock || generation != clock->generation || animation < 0 ||
        !BG_TCEPoseClockValidClip(clip))
    {
        return 0;
    }
    elapsed = time - clock->time;
    if (elapsed > INT32_MAX)
    {
        return 0;
    }
    memset(&next, 0, sizeof(next));
    next.clip = *clip;
    next.time = time;
    next.generation = generation;
    next.epoch = clock->epoch + 1u;
    if (!next.epoch)
    {
        next.epoch = 1u;
    }
    next.rateQ16 = TCE_POSE_CLOCK_RATE_ONE;
    next.animation = animation;
    next.valid = 1;
    *clock = next;
    return 1;
}

/* Add modulo a positive period without overflowing even near UINT64_MAX. */
static uint64_t BG_TCEPoseClockAddModulo(uint64_t value, uint64_t delta, uint64_t period)
{
    delta %= period;
    return value >= period - delta ? value - (period - delta) : value + delta;
}

int BG_TCEPoseClockAdvance(tcePoseClock_t *clock, uint32_t time)
{
    uint32_t elapsed;
    uint64_t delta, prefix, period, end;
    const tcePoseClockClip_t *clip;
    if (!clock || !clock->valid || !BG_TCEPoseClockValidClip(&clock->clip) ||
        clock->rateQ16 > TCE_POSE_CLOCK_RATE_MAX)
    {
        return 0;
    }
    elapsed = time - clock->time;
    if (elapsed > INT32_MAX)
    {
        return 0;
    }
    clip = &clock->clip;
    delta = (uint64_t)elapsed * clock->rateQ16;
    if (clip->loopFrames)
    {
        BG_TCEPoseClockSpan(clip, &prefix, &period);
        if (clock->elapsedQ16 < prefix)
        {
            if (delta < prefix - clock->elapsedQ16)
            {
                clock->elapsedQ16 += delta;
            }
            else
            {
                delta -= prefix - clock->elapsedQ16;
                clock->elapsedQ16 = prefix + delta % period;
            }
        }
        else
        {
            if (clock->elapsedQ16 - prefix >= period)
            {
                return 0;
            }
            clock->elapsedQ16 = prefix + BG_TCEPoseClockAddModulo(clock->elapsedQ16 - prefix,
                                                                delta, period);
        }
    }
    else
    {
        BG_TCEPoseClockSpan(clip, &end, &period);
        if (clock->elapsedQ16 > end)
        {
            return 0;
        }
        clock->elapsedQ16 = delta >= end - clock->elapsedQ16 ? end : clock->elapsedQ16 + delta;
    }
    clock->time = time;
    return 1;
}

int BG_TCEPoseClockSetRate(tcePoseClock_t *clock, uint32_t time, uint32_t rateQ16)
{
    if (rateQ16 > TCE_POSE_CLOCK_RATE_MAX || !BG_TCEPoseClockAdvance(clock, time))
    {
        return 0;
    }
    clock->rateQ16 = rateQ16;
    return 1;
}

int BG_TCEPoseClockSample(const tcePoseClock_t *clock, uint32_t time,
                         tcePoseClockSample_t *sample)
{
    tcePoseClock_t sampled;
    tcePoseClockSample_t result;
    uint64_t lead, frameDuration, phase, remainder;
    int first, next;
    if (!clock || !sample)
    {
        return 0;
    }
    sampled = *clock;
    if (!BG_TCEPoseClockAdvance(&sampled, time))
    {
        return 0;
    }
    memset(&result, 0, sizeof(result));
    result.time = time;
    result.generation = sampled.generation;
    result.epoch = sampled.epoch;
    result.animation = sampled.animation;
    result.model = sampled.clip.model;
    result.elapsedQ16 = sampled.elapsedQ16;
    result.held = sampled.rateQ16 == 0;
    lead = (uint64_t)sampled.clip.initialLerp << 16;
    frameDuration = (uint64_t)sampled.clip.frameLerp << 16;
    if (sampled.elapsedQ16 < lead)
    {
        first = next = 0;
        result.held = 1;
    }
    else
    {
        phase = sampled.elapsedQ16 - lead;
        first = (int)(phase / frameDuration);
        remainder = phase % frameDuration;
        next = first + 1;
        if (sampled.clip.loopFrames > sampled.clip.numFrames)
        {
            /* The legacy player controller derives its offset from the last
             * frame, then clamps negative offsets before loop reduction. The
             * exceptional loop therefore has N normal frames and one outlier,
             * not L consecutive tail frames. Keep canonical phase positive. */
            if (first == sampled.clip.numFrames)
            {
                first = sampled.clip.numFrames - sampled.clip.loopFrames;
            }
            if (next == sampled.clip.numFrames)
            {
                next = sampled.clip.numFrames - sampled.clip.loopFrames;
            }
            else if (next > sampled.clip.numFrames)
            {
                next = 0;
            }
        }
        else if (next >= sampled.clip.numFrames)
        {
            if (sampled.clip.loopFrames)
            {
                next = sampled.clip.numFrames - sampled.clip.loopFrames;
            }
            else
            {
                next = first;
                result.finished = result.held = 1;
            }
        }
        if (first != next)
        {
            result.backlerp = (float)(1.0 - (double)remainder / (double)frameDuration);
        }
        else
        {
            result.held = 1;
        }
    }
    if (sampled.clip.reversed)
    {
        first = sampled.clip.numFrames - 1 - first;
        next = sampled.clip.numFrames - 1 - next;
    }
    result.oldFrame = sampled.clip.firstFrame + first;
    result.frame = sampled.clip.firstFrame + next;
    *sample = result;
    return 1;
}
