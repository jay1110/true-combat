/* Windows TC:E CG_DrawMineMarkerFlag @ 30031b70.
 * Accept just the two model handles: SDK and TC media structs differ. */
#include "cg_local.h"

void CG_DrawMineMarkerFlag(centity_t *cent, refEntity_t *ent, const int *models) {
    lerpFrame_t *frame = &cent->lerpFrame;
    ent->hModel = models[cent->currentState.otherEntityNum2 != 0];
    ent->origin[2] += 8.0f;
    ent->oldorigin[2] += 8.0f;
    if (cg.time >= frame->frameTime) {
        frame->oldFrameTime = frame->frameTime;
        frame->oldFrame = frame->frame;
        do {
            frame->frameTime += 50;
            ++frame->frame;
            if (frame->frame >= 20) frame->frame = 0;
        } while (cg.time >= frame->frameTime);
    }
    if (frame->frameTime == frame->oldFrameTime) frame->backlerp = 0;
    else frame->backlerp = (float)(1.0 - (double)(float)(cg.time-frame->oldFrameTime) /
                                   (float)(frame->frameTime-frame->oldFrameTime));
    /* Original subtracts 20 once, not a modulo or clamp. */
    ent->frame = frame->frame + cent->currentState.frame;
    if (ent->frame >= 20) ent->frame -= 20;
    ent->oldframe = frame->oldFrame + cent->currentState.frame;
    if (ent->oldframe >= 20) ent->oldframe -= 20;
    ent->backlerp = frame->backlerp;
}
