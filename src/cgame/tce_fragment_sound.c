#include "cg_local.h"
#include "tce_fragment_sound.h"

int TCE_CG_SoundVolume(const vec3_t origin, float volume, float range,
    int linear, const tce_fragmentSoundContext_t *context) {
    vec3_t delta;
    double distance, result;
    float nearDistance;
    int i;
    /* The original stores the differences to floats, but leaves the length
     * in the x87 register through the attenuation calculation. */
    for (i = 0; i < 3; ++i) delta[i] = context->listener[i] - origin[i];
    distance = sqrt((double)delta[0]*delta[0] + (double)delta[1]*delta[1]
        + (double)delta[2]*delta[2]);
    if (context->distanceVariant) distance *= .8f;
    nearDistance = range * (linear ? .01f : .1f);
    if (distance < nearDistance) distance = nearDistance;
    result = linear ? nearDistance / distance :
        ((double)nearDistance * nearDistance) / (distance * distance);
    result *= volume;
    if (context->attenuation != 0) {
        result *= 1.0 - context->attenuation;
        if (result < 0) result = 0;
    }
    return (int)result;
}

static qboolean quietMaterial(unsigned material) {
    switch (material) {
    case 30: case 29: case 9: case 32: case 18: case 23:
    case 8: case 21: case 7: case 13: case 16: case 10:
        return qtrue;
    default: return qfalse;
    }
}

void TCE_CG_FragmentBounceSound(localEntity_t *le, trace_t *trace,
    const tce_fragmentSoundContext_t *context) {
    int volume, type, rnd, bank;
    unsigned material;
    sfxHandle_t sound;
    if (context->disabled) return;
    volume = TCE_CG_SoundVolume(trace->endpos, 127, 800, 0, context);
    if (!volume) { le->leBounceSoundType = LEBS_NONE; return; }
    type = le->leBounceSoundType;
    material = (unsigned)trace->surfaceFlags >> 24;
    if (type != 0 && type != 2 && type != 12 && quietMaterial(material)) return;
    switch (type) {
    case 2: sound = context->rubble[rand() % 3]; break;
    case 4:
        rnd = rand() % 3;
        bank = material == 3 ? 0 : material == 5 ? 3 :
            (material == 7 || material == 13 || material == 16 || material == 10) ? 1 : 2;
        sound = context->brass[bank][rnd];
        break;
    case 6: sound = context->type6[rand() % 3]; break;
    case 7: case 8: case 9: case 10: case 11:
        rnd = rand() % 4;
        sound = context->casing[type-7][context->soundVariant == 2][rnd];
        break;
    case 12: sound = context->type12; break;
    default: return;
    }
    trap_S_StartSoundVControl(trace->endpos, -1, CHAN_AUTO, sound, volume);
    le->leBounceSoundType = LEBS_NONE;
}
