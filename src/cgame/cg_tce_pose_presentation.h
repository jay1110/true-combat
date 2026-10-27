#ifndef CG_TCE_POSE_PRESENTATION_H
#define CG_TCE_POSE_PRESENTATION_H
#include "../game/bg_tce_pose_presentation.h"
void CG_TCEPresentationInit(void);
void CG_TCEPresentationRestart(void);
void CG_TCEPresentationManifestChanged(void);
void CG_TCEPresentationRememberModel(const animModelInfo_t *model);
void CG_TCEPresentationRegister(const bg_character_t *character,const bg_characterDef_t *definition);
int CG_TCEPresentationPrepare(centity_t *cent,const bg_character_t *character,
                              refEntity_t *body,refEntity_t *head);
typedef struct {
    vec3_t origin;
    int visible,visibilityUntil,active;
} tcePresentationOriginScope_t;
/* Only successful affected rendering enters this scope. Visibility uses its
 * existing real culling/glass/cover logic at the corrected location; every
 * player-render exit restores original entity interpolation/cache state. */
void CG_TCEPresentationBeginOrigin(centity_t *cent,const vec3_t origin,tcePresentationOriginScope_t *scope);
void CG_TCEPresentationEndOrigin(centity_t *cent,tcePresentationOriginScope_t *scope);
#endif
