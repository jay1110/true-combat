#include "../cgame/cg_local.h"

void testGrenadeTrail(unsigned char *base) {
    typedef void (__cdecl *trailFn)(centity_t *, const weaponInfo_t *);
    trailFn original = (trailFn)(base + 0x22a30);
    static centity_t expected, actual, before;
    static weaponInfo_t media, savedMedia;
    int pattern;

    /* Inspect the pinned binary too: RET excludes hidden global writes/calls
     * that a comparison of just the entity arguments would fail to detect.
     */
    if (base[0x22a30] != 0xc3) {
        fprintf(stderr, "GrenadeTrail original is no longer RET\n"); exit(1);
    }
    original(NULL, NULL);
    TCE_CG_GrenadeTrail(NULL, NULL);
    for (pattern = 0; pattern < 256; ++pattern) {
        memset(&before, pattern, sizeof(before));
        memset(&media, 255-pattern, sizeof(media));
        savedMedia = media;
        actual = expected = before;
        original(&expected, &media);
        TCE_CG_GrenadeTrail(&actual, &media);
        if (memcmp(&actual, &expected, sizeof(actual)) ||
            memcmp(&actual, &before, sizeof(actual)) ||
            memcmp(&media, &savedMedia, sizeof(media))) {
            fprintf(stderr, "GrenadeTrail state changed, pattern %d\n", pattern);
            exit(1);
        }
    }
    printf("257 grenade-trail original comparisons passed (including NULL).\n");
}
