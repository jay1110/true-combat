/* Whole-structure differential testing of the Windows weapon initializer. */
#include "../game/q_shared.h"
#include "../game/tce_bg.h"

static int (__cdecl *originalInit)(tce_weaponDef_t *);
static unsigned int randomState = 0x49b;
static int initChecks;

static unsigned int NextRandom(void) {
    randomState = randomState * 1664525U + 1013904223U;
    return randomState;
}

void compareWeaponInit(const tce_weaponDef_t *input) {
    tce_weaponDef_t actual = *input, expected = *input;
    int a = BG_InitializeWeaponDef(&actual), b = originalInit(&expected);
    if (a != b || memcmp(&actual, &expected, sizeof(actual))) {
        size_t offset;
        fprintf(stderr, "Init mismatch: class=%s caliber=%s characteristic=%d clip=%d start=%d scope=%g\n",
            input->weapClass, input->caliberClass, input->characteristic,
            input->maxclip, input->startClips, input->scoped);
        for (offset = 0; offset < sizeof(actual); ++offset)
            if (((unsigned char *)&actual)[offset] != ((unsigned char *)&expected)[offset])
                fprintf(stderr, "offset 0x%03x: got %02x original %02x\n", (unsigned int)offset,
                    ((unsigned char *)&actual)[offset], ((unsigned char *)&expected)[offset]);
        exit(1);
    }
    ++initChecks;
}

void testWeaponInitializer(unsigned char *base) {
    const char *classes[] = {"K","DC","G","P","AP","MP","SMG","CAR","MBR","SG","SR","unknown","sr"};
    const char *calibers[] = {"9x19","40SW","45ACP","50AE","556x45","545x39","762x39","762x51","70","76","792x57","338LAPUA","50BMG","STUN","SMOKE","unknown","9X19"};
    const int clips[] = {-5,-1,0,1,3,6,7,15,18,20,30,33,35,40,999,1002,2000};
    const int starts[] = {-5,-1,0,1,3,10,100};
    const float scopes[] = {-1,0,0.5f,1,1.999f,2,2.001f,4,4.001f,8,8.001f,100};
    int ci, ca, sample, n;
    tce_weaponDef_t input;
    originalInit = (void *)(base + 0x6a50);
    for (ci=0; ci<sizeof(classes)/sizeof(classes[0]); ++ci)
        for (ca=0; ca<sizeof(calibers)/sizeof(calibers[0]); ++ca)
            for (sample=0; sample<256; ++sample) {
                /* Patterned bytes expose unwanted clearing of unknown fields. */
                memset(&input, sample, sizeof(input));
                strcpy(input.weapClass,classes[ci]); strcpy(input.caliberClass,calibers[ca]);
                input.characteristic = sample % 30;
                input.maxclip = clips[NextRandom() % (sizeof(clips)/sizeof(clips[0]))];
                input.startClips = starts[NextRandom() % (sizeof(starts)/sizeof(starts[0]))];
                input.scoped = scopes[NextRandom() % (sizeof(scopes)/sizeof(scopes[0]))];
                input.singleReload = (sample >> 1) & 1;
                input.suppressed = (sample >> 2) & 1;
                input.subsonic = (sample >> 3) & 1;
                input.pelletCount = sample & 1 ? 9 : 0;
                input.semiauto = (sample >> 4) & 1;
                input.fullauto = (sample >> 5) & 1;
                input.burst = sample % 5;
                input.pump = (sample >> 6) & 1;
                input.bolt = (sample >> 7) & 1;
                compareWeaponInit(&input);
            }
    /* Unknown class must preserve and initialize from each incoming table row. */
    for (n=0; n<30; ++n) {
        memset(&input,0,sizeof(input));
        strcpy(input.weapClass,"unknown"); input.characteristic=n; input.startClips=-1;
        compareWeaponInit(&input);
    }
    printf("%d initializer cases matched all 460 output bytes and return values.\n", initChecks);
}
