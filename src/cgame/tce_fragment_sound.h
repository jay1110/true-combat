#ifndef TCE_FRAGMENT_SOUND_H
#define TCE_FRAGMENT_SOUND_H

/* Explicit TC state: do not substitute similarly named SDK fields without
 * verifying their producers. Numeric bounce types/materials are TC values. */
typedef struct {
    vec3_t listener;
    int distanceVariant; /* original 33fdf380; shared with brass size */
    float attenuation;  /* original 340a49cc */
    int disabled;       /* original 340a4a1c */
    int soundVariant;   /* original 340a49bc: 2 selects second bank */
    sfxHandle_t brass[4][3]; /* metal, soft, stone, wood */
    sfxHandle_t rubble[3], type6[3];
    sfxHandle_t casing[5][2][4]; /* bounce types 7..11, two banks */
    sfxHandle_t type12;
} tce_fragmentSoundContext_t;

int TCE_CG_SoundVolume(const vec3_t origin, float volume, float range,
    int linear, const tce_fragmentSoundContext_t *context);
void TCE_CG_FragmentBounceSound(localEntity_t *le, trace_t *trace,
    const tce_fragmentSoundContext_t *context);
#endif
