#include "../game/tce_bg.h"
#include "tce_weapon_offset.h"

/* CG_EliteFPWeaponOffset30072360. Double intermediates reproduce x87 math
 * until the explicit float stores identified in the original assembly. */
void TCE_CG_EliteFPWeaponOffset(int weapon, const tce_weaponOffset_t *s,
    const float axis[3][3], float offset[3], float secondaryOffset[3],
    float *tacticalScale, const float legacyStack[3]) {
    const tce_weaponDef_t *definition = &weaponDef[weapon];
    float aim[3] = {9,13,3}, hip[3], legacy[3];
    float side = 0, height = -5, secondaryBase = 0, secondaryAim = 0;
    double blend = 0, verticalBlend = 0, secondaryBlend = 0, amount;
    int adjustable = 0, delta, i;

    switch (weapon) {
    case 1: case 4: case 9: case 30:
        aim[0] = 5; aim[1] = 2; aim[2] = -7; height = -2;
        break;
    case 37: case 38: case 53: case 54:
        aim[0] = 12; aim[1] = 8; aim[2] = 6;
        side = 6; secondaryBase = secondaryAim = 12;
        break;
    case 2: case 3: case 5: case 6: case 7: case 8: case 10:
    case 13: case 14: case 23: case 24: case 25: case 32: case 33:
    case 39: case 40: case 41: case 42: case 43: case 44: case 45:
    case 46: case 47: case 48: case 49: case 50: case 51: case 52:
        adjustable = 1;
        break;
    default:
        break;
    }
    if (definition->scoped > 1) aim[0] -= 2;
    aim[0] -= 5; aim[1] -= 2; aim[2] += 7;
    if (definition->unknown_1ac == 75) aim[0] -= 3;
    else if (definition->unknown_1ac == 85) aim[0] -= 6;
    if (s->developer)
        for (i = 0; i < 3; ++i) aim[i] += s->developerOffset[i];

    /* The original reads these slots before writing them, even if their
     * later blend is zero. Never reproduce an uninitialized read in C. */
    for (i = 0; i < 3; ++i)
        legacy[i] = (legacyStack ? legacyStack[i] : 0.0f) - (float)(3-i);

    if (adjustable && (s->gunPosition == 1 || s->gunPosition == 2)) {
        float shift = s->gunPosition == 1 ? 2.0f : 4.0f;
        float spread = s->gunPosition == 1 ? 4.0f : 8.0f;
        aim[1] += shift;
        secondaryBase -= spread;
        secondaryAim += spread;
        side -= shift;
    }
    hip[0] = s->gunViewOffset[0] + 5;
    hip[1] = side + s->gunViewOffset[1];
    hip[2] = height + s->gunViewOffset[2];
    for (i = 0; i < 3; ++i) aim[i] += s->gunAimOffset[i];

    delta = s->time - s->aimTime;
    if (s->aiming) {
        blend = delta < 200 ? delta * (double).005f : 1.0;
        verticalBlend = blend * blend;
        delta = s->time - s->secondaryAimTime;
        if (s->secondaryAiming)
            secondaryBlend = delta < 100 ? delta * (double).01f : 1.0;
        else if (delta < 100)
            secondaryBlend = 1.0 - delta * (double).01f;
    } else if (delta < 200) {
        blend = 1.0 - delta * (double).005f;
        verticalBlend = blend * blend;
    }

    offset[0] = offset[1] = offset[2] = 0;
    amount = aim[0] * blend + legacy[0] * secondaryBlend + hip[0];
    for (i = 0; i < 3; ++i) offset[i] = (float)(amount * axis[0][i]);
    amount = aim[1] * blend + legacy[1] * secondaryBlend + hip[1];
    for (i = 0; i < 3; ++i) offset[i] = (float)(amount * axis[1][i] + offset[i]);
    amount = aim[2] * verticalBlend + legacy[2] * secondaryBlend + hip[2];
    for (i = 0; i < 3; ++i) offset[i] = (float)(amount * axis[2][i] + offset[i]);
    amount = -(secondaryAim * blend) - secondaryBase;
    for (i = 0; i < 3; ++i) secondaryOffset[i] = (float)(amount * axis[1][i]);
    *tacticalScale = (float)blend;
}
