/* Private mod animation clock. No engine player/entity/usercmd ABI changes. */
#ifndef BG_TCE_POSE_CLOCK_H
#define BG_TCE_POSE_CLOCK_H

#include <stdint.h>

#define TCE_POSE_CLOCK_RATE_ONE 65536u
#define TCE_POSE_CLOCK_RATE_MAX (4u * TCE_POSE_CLOCK_RATE_ONE)

/* Raw clip metadata, independent of the game's/cgame's animation_t model field.
 * model is a nonnegative private registry ID, not a renderer qhandle. */
typedef struct
{
    int model;
    int firstFrame, numFrames, loopFrames;
    int frameLerp, initialLerp;
    int reversed;
} tcePoseClockClip_t;

typedef struct
{
    tcePoseClockClip_t clip;
    /* Lead-in plus clip progress in Q16 milliseconds. Looping progress is
     * normalized into its cycle, not an unbounded age since assignment.
     * Sample by advancing a stored clock; do not lerp this scalar over a wrap. */
    uint64_t elapsedQ16;
    uint32_t time, generation, epoch, rateQ16;
    int animation, valid;
} tcePoseClock_t;

typedef struct
{
    uint64_t elapsedQ16;
    uint32_t time, generation, epoch;
    int animation, model, oldFrame, frame;
    float backlerp;
    int held, finished;
} tcePoseClockSample_t;

/* The caller supplies ONE timeline (command or presentation time, not a mix).
 * Forward differences through uint32 wrap are valid up to INT32_MAX ms.
 * Backward/ambiguous time, invalid clips/rates and wrong generations fail
 * atomically. All int-returning functions return 1 on success, 0 on failure. */
void BG_TCEPoseClockReset(tcePoseClock_t *clock, uint32_t generation, uint32_t time);
int BG_TCEPoseClockValidClip(const tcePoseClockClip_t *clip);

/* Checked inclusive absolute frame range for every reachable cycle frame.
 * Both distinct output pointers are required and remain unchanged on failure.
 * Legacy loopFrames>numFrames plays the declared frames plus one outlier,
 * then returns to the first played frame (the player controller clamps the
 * next negative offset before looping). Model/codec caps must check this
 * full range, rather than only firstFrame..firstFrame+numFrames-1. */
int BG_TCEPoseClockFrameBounds(const tcePoseClockClip_t *clip, int *minimum, int *maximum);

/* Inclusive greatest canonical Q16 phase and checked normalization of an
 * arbitrary nonnegative Q16 phase. Loops reduce into their shared cycle;
 * nonloops cap at the last frame. Invalid metadata/null outputs fail without
 * changing output. These helpers do not assign epochs or convert timelines. */
int BG_TCEPoseClockPhaseLimit(const tcePoseClockClip_t *clip, uint64_t *maximumQ16);
int BG_TCEPoseClockNormalizePhase(const tcePoseClockClip_t *clip, uint64_t elapsedQ16,
                                 uint64_t *canonicalQ16);

/* Call for an actual accepted assignment, even a restart of the same full ID.
 * Each call increments the nonzero epoch. Restart begins at normal rate;
 * movement policy can SetRate at the same time. Timers are never phase inputs.
 * initialLerp is a lead-in hold on the first played frame. No observer-local
 * old-clip blend/swing policy is implied; retain those epochs separately. */
int BG_TCEPoseClockAssign(tcePoseClock_t *clock, uint32_t generation, int animation,
                         const tcePoseClockClip_t *clip, uint32_t time);
int BG_TCEPoseClockAdvance(tcePoseClock_t *clock, uint32_t time);
int BG_TCEPoseClockSetRate(tcePoseClock_t *clock, uint32_t time, uint32_t rateQ16);

/* Read-only sampling advances a copy. Sample an older stored clock for rewind;
 * querying before this clock's time fails instead of undoing live state. */
int BG_TCEPoseClockSample(const tcePoseClock_t *clock, uint32_t time,
                         tcePoseClockSample_t *sample);

#endif
