/* Private server-side pose acquisition. No player/entity/network ABI fields.
 * This records accepted command epochs; it does not enable a damage provider
 * or claim that the command clock matches an observer's animation clock. */
#ifndef G_TCE_POSE_H
#define G_TCE_POSE_H
#include "q_shared.h"
#include "bg_public.h"
#include "bg_tce_pose_clock.h"
#include "tce_pose_math.h"
struct gentity_s;
struct gclient_s;
struct g_tcePresentationQuery_s;

enum { G_TCE_POSE_HISTORY = 64, G_TCE_POSE_EVENTS = 128,
       G_TCE_POSE_CHARACTERS = 64 };
typedef enum {
    G_TCE_POSE_UNKNOWN = 0, G_TCE_POSE_COMMAND = 1,
    G_TCE_POSE_OUTSIDE_COMMAND = 2, G_TCE_POSE_DURING_REWIND = 3
} g_tcePoseContext_t;
typedef struct {
    uint32_t generation, commandTime, receiptTime, acceptedEpoch[2];
    uint32_t acceptedCommandTime[2], acceptedReceiptTime[2];
    uint64_t sequence;
    int clientNum, character, animation[2], context[2];
    int eFlags, pmFlags, weaponFlags, weapon;
    float origin[3], viewangles[3], velocity[3], lean;
    tcePoseClock_t clocks[2];
} g_tcePoseHistory_t;
typedef struct {
    g_tcePoseHistory_t history;
    tcePoseClockSample_t channel[2];
    unsigned int commandPhaseMask;
} g_tcePoseQuery_t;
typedef struct {
    uint32_t mapGeneration;
    unsigned int initialized, characters, models, meshes;
    size_t ownedBytes;
    uint64_t accepted, rejected, eventsDropped, samplesDropped, rewindSkipped;
} g_tcePoseStats_t;

typedef struct {
    uint32_t mapGeneration, generation, acceptedEpoch[2], profile;
    const bg_character_t *renderCharacter, *animationCharacter;
    const animation_t *animation[2];
    int fullAnimation[2];
    int moveSpeed[2];
} g_tcePosePresentationMetadata_t;

typedef struct {
    uint32_t mapGeneration, profile, rawProfile;
    char characterFile[MAX_QPATH], mesh[MAX_QPATH], hudhead[MAX_QPATH];
    char animationGroup[MAX_QPATH], animationScript[MAX_QPATH];
    uint64_t bodyFingerprint, headFingerprint; /* Local FNV-1a, not wire IDs. */
} g_tcePoseAssetInfo_t;

typedef struct {
    int fullAnimation, firstFrame, numFrames, loopFrames, frameLerp, initialLerp, reversed;
    int moveSpeed, flags, movetype;
    char name[MAX_QPATH], mdxFile[MAX_QPATH];
} g_tcePoseClipMetadata_t;

typedef enum {
    G_TCE_POSE_TRACE_UNSUPPORTED = 0, G_TCE_POSE_TRACE_MISS = 1,
    G_TCE_POSE_TRACE_HIT = 2
} g_tcePoseTraceState_t;

/* Complete values used for this geometry/ray query, not a lossy hash or a
 * scratch pointer. Zeroed padding and explicit field copies permit exact
 * reuse checks without repeating evaluation or intersection work. */
typedef struct {
    uint32_t mapGeneration, generation, profile, sourceTime, serverTime, acceptedEpoch[2];
    const bg_character_t *renderCharacter, *animationCharacter;
    int clientNum, animation[2], mode, eFlags, frame[2][2];
    uint16_t code[2];
    uint32_t rateQ8[2];
    int clip[2][6], held[2], finished[2];
    uint64_t elapsedQ16[2];
    float backlerp[2], origin[3], bodyAxis[3][3], torsoAxis[3][3], headAxis[3][3];
    float bodyAngles[3], torsoAngles[3], headAngles[3];
    float start[3], end[3];
    double maxFraction;
} g_tcePoseTraceKey_t;

typedef struct {
    g_tcePoseTraceState_t state;
    g_tcePoseTraceKey_t key;
    tcePoseSurfaceHit_t surface;
    float anchor[3], nominal[3];
} g_tcePoseTraceResult_t;

void G_TCEPoseInit(void);
void G_TCEPoseShutdown(void);
void G_TCEPoseResetCharacters(void);
/* Success means at least one usable clip. Unsupported clips remain unknown;
 * they do not disable unrelated valid clips or stop later model registration. */
int G_TCEPoseRegisterCharacter(const bg_character_t *character, const bg_characterDef_t *definition);
void G_TCEPoseCharacterChanged(struct gclient_s *client);
void G_TCEPoseResetClient(struct gentity_s *ent);
/* Marker reset follows ClientSpawn. Preserve assignments already observed in
 * that new spawn generation; discard only coarse history from before it. */
void G_TCEPoseResetHistory(struct gentity_s *ent);
void G_TCEPoseBeginCommand(struct gentity_s *ent);
void G_TCEPoseEndCommand(struct gentity_s *ent);
void G_TCEPoseCapture(struct gentity_s *ent);
void G_TCEPoseAnimationAssigned(const playerState_t *ps, const animModelInfo_t *model,
                              animBodyPart_t part, int previous, int animation);
/* Separate explicit timelines: no receipt->command conversion or live-state
 * advancement. Unknown channels return a query with commandPhaseMask clear. */
int G_TCEPoseQuery(int clientNum, uint32_t commandTime, uint32_t receiptCeiling,
                  g_tcePoseQuery_t *out);
/* Optional explicit consumer only. The caller must establish shot-time axes
 * and profile; no gameplay path calls this function. */
int G_TCEPoseEvaluate(const g_tcePoseQuery_t *query, const float bodyAxis[3][3],
                     const float torsoAxis[3][3], tcePoseMathMode_t mode,
                     tcePoseResult_t *out);
void G_TCEPoseStats(g_tcePoseStats_t *out);

/* Registration-owned immutable assets/metadata only; never loads on a command,
 * historical query or trace. Registered ordinary/disguise/carrier selections
 * retain distinct render and animation identities. Corpse/special models and
 * incompatible uncalibrated animation tables return unavailable. */
int G_TCEPoseAssetInfo(const bg_character_t *character, g_tcePoseAssetInfo_t *out);
int G_TCEPoseClipMetadata(const bg_character_t *character, int fullAnimation,
                          g_tcePoseClipMetadata_t *out);
/* Current slot/base-character identity only. Historical presentation must not
 * depend on current clip, render selection or surface readiness. Both outputs
 * are cleared on failure; life/map eligibility remains the caller's policy. */
int G_TCEPosePresentationIdentity(const struct gentity_s *ent,
                                 uint32_t *mapGeneration, uint32_t *generation);
int G_TCEPosePresentationMetadata(const struct gentity_s *ent,
                                 g_tcePosePresentationMetadata_t *out);
/* Stable sorted unique final hashes. Returns count, or -1 on insufficient
 * capacity/invalid output without writing a partial negotiated profile set. */
int G_TCEPosePresentationProfiles(uint32_t *out, int capacity);

/* Pure supplied-presentation consumer. Fixed head MD3 frame0 is an explicit
 * damage-geometry policy, attached to evaluated tag_head with supplied relative
 * head axes; facial animation is not inferred. MISS means validated skin was
 * available and must not fall through to a coarse bbox. No damage is applied.
 * Nominal HIT coefficients are one-hot HEAD2/BODY1/LEGS.5; radial weights are
 * outside this API. The returned key/result can be reused by selection/damage.
 * Module-private evaluation scratch is serialized with normal game callbacks. */
g_tcePoseTraceState_t G_TCEPosePresentationTrace(const struct g_tcePresentationQuery_s *query,
    const float start[3], const float end[3], double maxFraction, g_tcePoseTraceResult_t *out);
int G_TCEPosePresentationTraceMatches(const g_tcePoseTraceResult_t *result,
    const struct g_tcePresentationQuery_s *query, const float start[3], const float end[3], double maxFraction);
/* Diagnostics only: AABBs enclosing the evaluated triangle regions. These
 * are NOT solid hit volumes and are never consumed by damage selection. */
int G_TCEPosePresentationBounds(const struct g_tcePresentationQuery_s *query,
    float mins[3][3], float maxs[3][3]);
#endif
