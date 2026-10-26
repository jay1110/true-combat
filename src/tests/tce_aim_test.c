#include "../game/tce_aim.h"
#include "../game/tce_bg.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

static unsigned int randomState = 0x5a17d902u;
static unsigned int next(void) { randomState = randomState * 1664525u + 1013904223u; return randomState; }
#define I(p,o) (*(int *)((p)+(o)))
#define F(p,o) (*(float *)((p)+(o)))

int main(int argc, char **argv) {
    HMODULE dll;
    unsigned char *base, raw[0x700], expected[0x700], move[0x200];
    void (__cdecl *original)(void);
    tce_weaponDef_t *defs;
    tce_aimState_t s;
    int i, k, w;
    const int times[] = {0,1,2,8,16,20,33,50,66,100,200};
    const int values[] = {-1,0,1,199,200,201,999,1000,1001,10000};
    if (argc != 2 || !(dll = LoadLibraryA(argv[1]))) return 2;
    base = (unsigned char *)dll;
    original = (void *)(base + 0x9c30);
    defs = (void *)(base + 0x4c1df20);
    *(void **)(base + 0x4ba9edc) = move;
    for (i = 0; i < 300000; ++i) {
        memset(raw, 0xa5, sizeof(raw));
        memset(move, 0, sizeof(move));
        memset(&s, 0, sizeof(s));
        w = i % 64;
        s.zooming = (i % 17 == 0);
        s.ducked = next() >> 31;
        s.weaponState = i % 13;
        s.commandTime = 20000;
        s.oldCommandTime = s.commandTime - times[i % 11];
        for (k = 0; k < 2; ++k) {
            s.oldAngles[k] = (int)(next() & 65535) - 32768;
            s.angles[k] = s.oldAngles[k] + (int)(next() % 41) - 20;
            s.velocity[k] = ((int)(next() % 201) - 100) / (i % 3 ? 128.0f : 1.0f);
        }
        s.movementRecovery = i % 5 ? (int)(next() % 1501) : 0;
        s.shotRecovery = next() % 2001;
        s.scoped = i % 4 ? 0.0f : 4.0f;
        s.seed = next();
        s.weaponFlags = (next() >> 16) & ~0x10u;
        if (i & 1) s.weaponFlags |= 0x10;
        if (i & 2) s.weaponFlags |= 4; else s.weaponFlags &= ~4u;
        s.movementFlags = (i & 4) ? 0x200 : 0;
        s.movementInstability = values[(i / 7) % 10];
        s.shotInstability = values[(i / 13) % 10];
        s.phase = values[(i / 19) % 10];
        s.aimSpreadFloat = 17.25f; s.aimSpread = 29;
        *(void **)move = raw;
        I(move,0xc) = s.commandTime; I(move,0x28) = s.oldCommandTime;
        I(raw,0xc) = s.ducked; I(raw,0x68) = s.zooming ? 0x40000 : 0;
        I(raw,0xa4) = w; I(raw,0xa8) = s.weaponState;
        for (k = 0; k < 2; ++k) {
            I(move,0x14+4*k) = s.angles[k]; I(move,0x30+4*k) = s.oldAngles[k];
            F(raw,0x20+4*k) = s.velocity[k];
        }
        I(raw,0xec) = s.seed; I(raw,0xf0) = s.weaponFlags;
        I(raw,0xf4) = s.movementFlags; I(raw,0xfc) = s.movementInstability;
        I(raw,0x100) = s.shotInstability; I(raw,0x104) = s.phase;
        F(raw,0x488) = s.aimSpreadFloat; I(raw,0x48c) = s.aimSpread;
        defs[w].unknown_0f8[4] = s.movementRecovery;
        defs[w].unknown_0f8[5] = s.shotRecovery;
        defs[w].scoped = s.scoped;
        memcpy(expected, raw, sizeof(raw));
        original();
        TCE_PM_AdjustAimSpreadScale(&s);
        I(expected,0xf0) = s.weaponFlags; I(expected,0xfc) = s.movementInstability;
        I(expected,0x100) = s.shotInstability; I(expected,0x104) = s.phase;
        F(expected,0x488) = s.aimSpreadFloat; I(expected,0x48c) = s.aimSpread;
        if (memcmp(raw, expected, sizeof(raw))) {
            for(k=0;k<(int)sizeof(raw);++k) if(raw[k]!=expected[k]) break;
            fprintf(stderr,"Aim mismatch case%d offset%x: move%d/%d shot%d/%d phase%d/%d flags%x/%x dt%d decay%d\n",
                i,k,I(raw,0xfc),s.movementInstability,I(raw,0x100),s.shotInstability,
                I(raw,0x104),s.phase,I(raw,0xf0),s.weaponFlags,s.commandTime-s.oldCommandTime,s.movementRecovery);
            return 1;
        }
    }
    FreeLibrary(dll);
    puts("300000 complete original aim-state comparisons matched, including untouched bytes");
    return 0;
}
