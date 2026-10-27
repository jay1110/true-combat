/* Private server-frame presentation contract. No SDK/client/entity ABI fields. */
#ifndef G_TCE_POSE_PRESENTATION_H
#define G_TCE_POSE_PRESENTATION_H
#include "bg_tce_pose_presentation.h"
struct gentity_s;
typedef struct g_tcePresentationQuery_s {
    uint32_t generation, mapGeneration, sourceTime, serverTime, acceptedEpoch[2], profile;
    const bg_character_t *renderCharacter,*animationCharacter;
    int clientNum, animation[2], mode, eFlags;
    uint16_t code[2];
    uint32_t rateQ8[2];
    tcePosePresentationClip_t clip[2];
    tcePoseCompactSample_t channel[2];
    float origin[3];
    tcePosePresentationAxes_t axes;
} g_tcePresentationQuery_t;
enum { G_TCE_PRESENTATION_UNKNOWN=-1, G_TCE_PRESENTATION_LEGACY=0,
       G_TCE_PRESENTATION_AFFECTED=1 };
void G_TCEPresentationReset(void);
void G_TCEPresentationResetClient(struct gentity_s *ent);
void G_TCEPresentationClear(struct gentity_s *ent);
void G_TCEPresentationPublish(struct gentity_s *ent);
void G_TCEPresentationUserinfo(int clientNum,const char *userinfo);
void G_TCEPresentationConnect(int clientNum);
uint32_t G_TCEPresentationChallenge(int clientNum);
/* serverTime and receiptCeiling are engine server-frame timestamps, NEVER
 * victim commandTime. Only samples received at/before ceiling may be used.
 * AFFECTED returns a valid query. LEGACY requires a proven ordinary history
 * record or an explicit historical gate-off publication. Current negotiation
 * never overrides an older published affected record. UNKNOWN (gap/stale/life/profile)
 * clears out and MUST NOT permit legacy-hit fallback. Query is read-only.
 * The caller uses its immutable shot/rewind serverTime for every target. */
int G_TCEPresentationQuery(int clientNum, uint32_t serverTime,
                           uint32_t receiptCeiling, g_tcePresentationQuery_t *out);
#endif
