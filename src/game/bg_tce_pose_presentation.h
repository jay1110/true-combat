/* Mod-owned shared affected-pose policy. No SDK structure changes.
 * Protocol2 requires explicit negotiated publication/consumption. Protocol1
 * primitives remain standalone. Neither recovers legacy observer swing,
 * animation blend, frame scheduling or command/presentation time mapping.
 */
#ifndef BG_TCE_POSE_PRESENTATION_H
#define BG_TCE_POSE_PRESENTATION_H

#include <stdint.h>
#include "q_shared.h"
#include "bg_public.h"
#include "bg_tce_pose_origin.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TCE_POSE_PRESENTATION_VERSION 1u
#define TCE_POSE_PHASE_VALID UINT32_C(0x80000000)
#define TCE_POSE_PHASE_VERSION_MASK UINT32_C(0x60000000)
#define TCE_POSE_PHASE_VERSION_1 UINT32_C(0x20000000)
#define TCE_POSE_PHASE_MASK UINT32_C(0x1fffffff)
#define TCE_POSE_PHASE_RATE_ONE 256u
#define TCE_POSE_PHASE_RATE_MAX 1024u
#define TCE_POSE_COMPACT_PROTOCOL 2
#define TCE_POSE_COMPACT_VALID 0x8000u
#define TCE_POSE_COMPACT_VERSION 0x4000u
#define TCE_POSE_COMPACT_MASK 0x3fffu
#define TCE_POSE_COMPACT_LEGS_SLOT 7
#define TCE_POSE_COMPACT_TORSO_SLOT 8
#define TCE_POSE_PRESENTATION_MAX_GAP 100u
/* Private mod lease in the existing unused40..63 range; SDK indices/layouts
 * are unchanged. Contains only registration identity, never recurring poses. */
#define TCE_POSE_PRESENTATION_CS 40
#define TCE_POSE_PRESENTATION_PROFILES 64
#define TCE_POSE_PRESENTATION_MANIFEST (3+9*TCE_POSE_PRESENTATION_PROFILES)

typedef struct {
    uint64_t elapsedQ16;
    int oldFrame, frame, held, finished;
    float backlerp;
} tcePoseCompactSample_t;

typedef struct {
    /* A canonical, validated character/animation asset profile identifier,
     * never a renderer handle, pointer or registry insertion-order index. */
    uint32_t profile;
    int animation[2];             /* Existing full legs/torso IDs incl toggle. */
} tcePosePresentationKey_t;

typedef struct {
    int firstFrame, numFrames, loopFrames, frameLerp, initialLerp, reversed;
} tcePosePresentationClip_t;

typedef struct {
    uint32_t elapsedQ8;
    int oldFrame, frame, held, finished;
    float backlerp;
} tcePosePresentationSample_t;

/* Zero is canonical unavailable. Valid words contain version1 and Q8
 * milliseconds. Lead-in, loop normalization (including legacy extended
 * cycles), and nonloop caps belong to the shared clock. Every playable frame
 * must fit [0,8191]. Pack requires an already canonical phase;
 * Step delegates normalization and sampling to the shared clock helper. There
 * is no transition/epoch in a word.
 * Caller must publish/clear BOTH words atomically with the same snapshot's
 * full animation IDs and profile, retaining old snapshot keys separately.
 * Comparing observed/expected keys cannot detect stale words if their caller
 * supplies an already-misassociated key. Same-ID restarts require the separate
 * accepted-event/transition contract. No timestamp domain conversion is implied.
 * All failures return0 and clear the output (when nonnull).
 */
int BG_TCEPosePhasePack(const tcePosePresentationKey_t *key, int channel,
                        const tcePosePresentationClip_t *clip,
                        uint64_t elapsedQ8, uint32_t *word);
int BG_TCEPosePhaseRead(uint32_t word, int channel,
                        const tcePosePresentationKey_t *observed,
                        const tcePosePresentationKey_t *expected,
                        const tcePosePresentationClip_t *clip,
                        tcePosePresentationSample_t *sample);
/* Advance a copy in ONE explicitly chosen timeline. Forward uint32 time wrap
 * is accepted for differences <=INT32_MAX; backward/ambiguous stamps fail.
 * This is a constant-rate segment only. Rate0 holds. It supplies no movement
 * rate selection, assignment timing or previous-clip transition policy.
 */
int BG_TCEPosePhaseStep(uint32_t word, int channel,
                        const tcePosePresentationKey_t *observed,
                        const tcePosePresentationKey_t *expected,
                        const tcePosePresentationClip_t *clip,
                        uint32_t fromTime, uint32_t toTime, uint32_t rateQ8,
                        uint32_t *advanced);

/* Protocol2 is a distinct normalized short, not truncated protocol1. Full
 * canonical range and normalization are provided by the clock. Quantization
 * error is bounded to1/128 frame; unsupported precision fails explicitly.
 * Pack requires canonical Q16 input. Sample advances only the decoded baseline
 * copy at a constant caller-supplied rate, without a second quantization. */
int BG_TCEPoseCompactPack(const tcePosePresentationClip_t *clip,
                          uint64_t elapsedQ16, uint16_t *code);
int BG_TCEPoseCompactSample(const tcePosePresentationClip_t *clip, uint16_t code,
                            uint32_t fromTime, uint32_t toTime, uint32_t rateQ8,
                            tcePoseCompactSample_t *out);
int BG_TCEPoseCompactPrecision(const tcePosePresentationClip_t *clip,
                               uint64_t *maximumErrorQ16);
/* Stable private profile identity; pointers/renderer handles and mutable
 * CG_CalcMoveSpeeds values are excluded. Pure hashing, no asset I/O. */
uint32_t BG_TCEPosePresentationProfile(const bg_character_t *character,
                                      const bg_characterDef_t *definition);
/* Registration-only calibration. Every coordinate is quantized to1/64 unit
 * before reducing. Frame0 initializes history; only the following N-1 samples
 * contribute, avoiding the original undefined first-frame delta. Callback
 * failure/nonfinite coordinates leave output unchanged. No engine I/O here. */
typedef int (*tcePoseFeet_t)(void *user,int absoluteFrame,float feet[2][3]);
int BG_TCEPosePresentationMoveSpeed(int firstFrame,int numFrames,int frameLerp,
                                   int ladder,tcePoseFeet_t feet,void *user,
                                   int *outSpeed);
/* INT_MIN marks an unresolved row. Includes the complete calibrated table in
 * the character identity, so unequal tag/evaluator results cannot negotiate. */
uint32_t BG_TCEPosePresentationSpeedProfile(uint32_t rawProfile,
                                           const int *speeds,int count);
uint32_t BG_TCEPosePresentationPairProfile(uint32_t renderProfile,
                                          uint32_t animationRawProfile);
int BG_TCEPosePresentationManifest(const uint32_t *profiles,int count,
                                   char *out,int size);
uint32_t BG_TCEPosePresentationManifestDigest(const char *manifest);
uint32_t BG_TCEPosePresentationManifestMatch(const char *manifest,
                                             const uint32_t *profiles,int count);

typedef enum {
    TCE_POSE_PRESENT_LADDER = 1,
    TCE_POSE_PRESENT_FREECLIMB = 2,
    TCE_POSE_PRESENT_PRONE = 3
} tcePosePresentationMode_t;

typedef struct {
    tcePosePresentationMode_t mode;
    float aim[3], movementYaw, velocity[3], lean, freelook[2], scale;
    int leanMode;                /* Server lean policy:0 disabled,1 enabled. */
} tcePosePresentationAxesInput_t;

typedef struct {
    float bodyAngles[3], torsoRelativeAngles[3], headRelativeAngles[3];
    /* refEntity basis rows. Scale affects body only, after relative axes. */
    float body[3][3], torso[3][3], head[3][3];
} tcePosePresentationAxes_t;

/* Canonical snapshot-only affected-pose policy. Torso aims immediately, legs
 * use aim+movement yaw; nonprone torso pitch is .75*aim, prone is -3 before
 * travel tilt. TC order: travel tilt, asymmetric lean, head-only .7 freelook,
 * hierarchical angle subtraction, basis conversion, body scale. No pain or
 * observer-local swing state, own-player ladder override, or geometry trace is
 * fabricated. Input pitch/yaw/roll and movement/freelook/lean must be bounded
 * finite degrees; velocity is bounded to8192 units/s/component and scale(0,4].
 * No automatic mode selection, slope fitting or damage activation occurs.
 */
int BG_TCEPosePresentationAxes(const tcePosePresentationAxesInput_t *input,
                              tcePosePresentationAxes_t *out);
int BG_TCEPosePresentationMode(const char *legsName, const char *torsoName,
                              int prone, int ladder);
uint32_t BG_TCEPosePresentationRate(int mode, const float velocity[3], int moveSpeed);
int BG_TCEPosePresentationInterpolate(const tcePosePresentationAxesInput_t *older,
                                     const tcePosePresentationAxesInput_t *newer,
                                     float fraction,tcePosePresentationAxesInput_t *out);

#ifdef __cplusplus
}
#endif
#endif
