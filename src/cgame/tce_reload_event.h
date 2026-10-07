#ifndef TCE_RELOAD_EVENT_H
#define TCE_RELOAD_EVENT_H
#include "tce_fragment_sound.h"
/* Semantic IDs in the original TC:E protocol, independent of SDK event IDs. */
void TCE_CG_ReloadEvent(int event, int entity, const vec3_t origin,
    int singleReload, int bolt, const sfxHandle_t sounds[4],
    const tce_fragmentSoundContext_t *context, int *ejectPending);
#endif
