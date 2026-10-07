#include "cg_local.h"
#include "tce_reload_event.h"

void TCE_CG_ReloadEvent(int event, int entity, const vec3_t origin,
    int singleReload, int bolt, const sfxHandle_t sounds[4],
    const tce_fragmentSoundContext_t *context, int *ejectPending) {
    int volume;
    if (event < 130 || event > 133) return;
    volume = TCE_CG_SoundVolume(origin,127.0f,1200.0f,0,context);
    if (context->disabled) volume = 0;
    if (sounds[event-130] && volume)
        trap_S_StartSoundVControl(NULL,entity,CHAN_WEAPON,sounds[event-130],volume);
    /* Muting or missing audio must not suppress the mechanical event. */
    if ((event == 132 && singleReload) || (event == 133 && bolt))
        *ejectPending = 1;
}
