/* TC:E Windows cgame 0x30006a50. Field names not yet proven remain offsets.
 * Characteristic-table values are recovered game data, not executable code.
 */
#include "q_shared.h"
#include "tce_bg.h"
#include "tce_weapon_characteristics.h"

static int IsCaliber(const tce_weaponDef_t *w, const char *name) {
    return Q_stricmp(w->caliberClass, name) == 0;
}

int BG_InitializeWeaponDef(tce_weaponDef_t *w) {
    const int *table;
    int i, defaultClip;
    if (!Q_stricmp(w->weapClass, "K") || !Q_stricmp(w->weapClass, "DC") ||
        !Q_stricmp(w->weapClass, "G")) {
        if (!Q_stricmp(w->weapClass, "K")) w->characteristic = 1;
        else if (!Q_stricmp(w->weapClass, "DC")) w->characteristic = 29;
        else w->characteristic = IsCaliber(w,"STUN") ? 28 : IsCaliber(w,"SMOKE") ? 27 : 26;
        w->weaponCategory = 0;
        w->bolt = w->burst = w->fullauto = w->pump = 0;
        w->scoped = 0;
        w->noTacMode = 1;
    } else if (!Q_stricmp(w->weapClass, "P") || !Q_stricmp(w->weapClass, "AP")) {
        if (!Q_stricmp(w->weapClass, "P"))
            w->characteristic = IsCaliber(w,"9x19") ? 2 : IsCaliber(w,"40SW") ? 3 :
                IsCaliber(w,"45ACP") ? 4 : IsCaliber(w,"50AE") ? 5 : 2;
        else
            w->characteristic = IsCaliber(w,"40SW") ? 7 : IsCaliber(w,"45ACP") ? 8 : 6;
        w->weaponCategory = 1;
        w->bolt = w->burst = w->fullauto = w->pump = 0;
        w->scoped = 0;
        w->unknown_1b0 = 1;
        w->unknown_1ac = 85;
    } else if (!Q_stricmp(w->weapClass, "MP")) {
        w->characteristic = IsCaliber(w,"45ACP") ? 10 : 9;
        w->weaponCategory = 2;
        w->bolt = w->burst = w->pump = 0;
        w->scoped = 0;
        w->unknown_1b0 = 1;
        w->unknown_1ac = 75;
    } else if (!Q_stricmp(w->weapClass, "SMG")) {
        w->characteristic = IsCaliber(w,"40SW") ? 12 : IsCaliber(w,"45ACP") ? 13 : 11;
        w->weaponCategory = 2;
        w->bolt = w->pump = 0;
        w->unknown_1b0 = 1;
        if (w->scoped > 2) w->scoped = 2;
        w->unknown_1ac = 75;
    } else if (!Q_stricmp(w->weapClass, "CAR")) {
        w->characteristic = IsCaliber(w,"545x39") ? 15 : 14;
        w->weaponCategory = 3;
        w->pump = w->bolt = 0;
        w->unknown_1b0 = 1;
        if (w->scoped > 4) w->scoped = 4;
        w->unknown_1ac = 75;
    } else if (!Q_stricmp(w->weapClass, "MBR")) {
        w->characteristic = IsCaliber(w,"762x39") ? 17 : IsCaliber(w,"762x51") ? 18 : 16;
        w->weaponCategory = 3;
        w->pump = 0;
        w->unknown_1b0 = 1;
        if (w->scoped > 4) w->scoped = 4;
        w->unknown_1ac = 60;
    } else if (!Q_stricmp(w->weapClass, "SG")) {
        w->characteristic = IsCaliber(w,"76") ? 20 : 19;
        w->pelletCount = 9;
        w->weaponCategory = 4;
        w->bolt = w->burst = 0;
        w->scoped = 0;
        w->unknown_1b0 = 2;
        w->unknown_1ac = 75;
    } else if (!Q_stricmp(w->weapClass, "SR")) {
        w->unknown_1b0 = 1;
        if (IsCaliber(w,"556x45")) {
            w->characteristic = 21;
            w->weaponCategory = 3;
        } else if (IsCaliber(w,"338LAPUA") || IsCaliber(w,"50BMG")) {
            w->characteristic = IsCaliber(w,"338LAPUA") ? 24 : 25;
            w->weaponCategory = 5;
            w->burst = 0;
            w->subsonic = w->suppressed = 0;
            w->unknown_1a8 = 1;
            w->unknown_1b0 = 2;
        } else {
            w->characteristic = IsCaliber(w,"792x57") ? 23 : 22;
            w->weaponCategory = 3;
            w->burst = 0;
        }
        w->fullauto = w->pump = 0;
        if (w->scoped > 8) w->scoped = 8;
        w->unknown_1c0 = 1;
        w->unknown_1ac = 60;
    }

    /* Unknown classes preserve the incoming characteristic, as the original
     * does. Callers must supply a valid entry (0..29), even on that path. */
    table = tce_weaponCharacteristics[w->characteristic];
    w->maxammo = table[0]; w->uses = table[1]; defaultClip = table[2];
    w->startingAmmo = table[3]; w->startingClip = table[4];
    w->reloadTime = table[5]; w->fireDelayTime = table[6]; w->nextShotTime = table[7];
    w->maxHeat = table[8]; w->coolRate = table[9]; w->mod = table[10];
    for (i = 0; i < 9; ++i) w->unknown_0f8[i] = table[11+i];
    w->unknown_12c = table[20]; w->unknown_130 = table[21]; w->unknown_134 = table[22];
    w->unknown_154 = table[23]; w->unknown_138 = table[24]; w->unknown_158 = table[25];
    w->unknown_120 = table[26]; w->loadoutWeight = table[27];
    w->unknown_19c = 300;
    w->unknown_1a0 = 800;
    if (w->maxclip == 0) w->maxclip = defaultClip;
    else if (w->maxclip > defaultClip + 3) w->maxclip = defaultClip + 3;
    else if (w->maxclip < 1) w->maxclip = 1;
    w->startingClip = w->maxclip;
    if (w->startClips >= 0) {
        w->startingAmmo = w->singleReload ? w->startClips : w->startClips * w->maxclip;
        if (w->startingAmmo > w->maxammo) w->startingAmmo = w->maxammo;
    }
    if (w->pelletCount && w->singleReload && w->maxclip < 7) w->loadoutWeight = 3;
    if (w->suppressed || w->subsonic) {
        /* x87 multiplies integers by binary32 constants then truncates with
         * __ftol, without rounding the product to float first. */
        w->unknown_134 = (int)((double)w->unknown_134 * (double)0.6f);
        w->unknown_130 = (int)((double)w->unknown_130 * (double)0.8f);
        w->unknown_12c = (int)((double)w->unknown_12c * (double)0.9f);
        w->unknown_138 = 290;
        w->unknown_158 = 600;
    }
    if (w->scoped > 0 && w->scoped < 2) w->scoped = 2;
    return 1;
}
