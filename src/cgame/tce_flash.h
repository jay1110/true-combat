#ifndef TCE_FLASH_H
#define TCE_FLASH_H
typedef struct {
    int blindUntil, blindActive, deafUntil;
    float deafness;
} tceFlashState_t;
extern tceFlashState_t tceFlash;
void TCE_CG_FlashBang(const vec3_t origin, int concussionOnly);
void TCE_CG_DrawFlashBang(void);
void TCE_CG_UpdateFlashRinging(void);
void TCE_CG_ResetFlash(void);
#endif
