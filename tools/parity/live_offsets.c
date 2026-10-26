/* Actual current cgame layout, used only by the external read-only observer. */
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "src/cgame/cg_local.h"
#include <stdio.h>
#define FIELD(type, field) printf("\"" #field "\":%u,", (unsigned)offsetof(type, field))
int main(void)
{
    printf("{\"cg\":{");
    FIELD(cg_t, snap); FIELD(cg_t, time); FIELD(cg_t, oldTime);
    FIELD(cg_t, frametime); FIELD(cg_t, clientFrame);
    FIELD(cg_t, predictedPlayerState); FIELD(cg_t, refdef_current);
    FIELD(cg_t, predictedPlayerEntity); FIELD(cg_t, renderingThirdPerson);
    printf("\"size\":%u},\"snapshot\":{", (unsigned)sizeof(cg_t));
    FIELD(snapshot_t, ps); FIELD(snapshot_t, serverTime);
    printf("\"size\":%u},\"ps\":{", (unsigned)sizeof(snapshot_t));
    FIELD(playerState_t, commandTime); FIELD(playerState_t, pm_type);
    FIELD(playerState_t, pm_flags); FIELD(playerState_t, origin);
    FIELD(playerState_t, velocity); FIELD(playerState_t, speed);
    FIELD(playerState_t, gravity); FIELD(playerState_t, groundEntityNum);
    FIELD(playerState_t, legsAnim); FIELD(playerState_t, torsoAnim);
    FIELD(playerState_t, weapon); FIELD(playerState_t, weaponstate);
    FIELD(playerState_t, weaponTime); FIELD(playerState_t, viewangles);
    FIELD(playerState_t, runSpeedScale); FIELD(playerState_t, sprintSpeedScale);
    FIELD(playerState_t, crouchSpeedScale); FIELD(playerState_t, sprintExertTime);
    FIELD(playerState_t, weapAnim); FIELD(playerState_t, legsTimer);
    FIELD(playerState_t, torsoTimer); FIELD(playerState_t, bobCycle);
    FIELD(playerState_t, stats); FIELD(playerState_t, persistant);
    printf("\"size\":%u},\"centity\":{", (unsigned)sizeof(playerState_t));
    FIELD(centity_t, pe.legs); FIELD(centity_t, pe.torso); FIELD(centity_t, pe.weap);
    printf("\"size\":%u},\"lerpFrame\":{", (unsigned)sizeof(centity_t));
    FIELD(lerpFrame_t, frame); FIELD(lerpFrame_t, oldFrame);
    FIELD(lerpFrame_t, frameTime); FIELD(lerpFrame_t, oldFrameTime);
    FIELD(lerpFrame_t, backlerp); FIELD(lerpFrame_t, animationNumber);
    printf("\"size\":%u}}\n", (unsigned)sizeof(lerpFrame_t));
    return 0;
}
