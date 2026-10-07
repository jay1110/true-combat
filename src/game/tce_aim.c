#include "tce_aim.h"
#include <math.h>

/* The original retains intermediates in x87 registers. Double arithmetic
 * preserves that precision; explicit floats mark its FSTP rounding points. */
void TCE_PM_AdjustAimSpreadScale(tce_aimState_t *s) {
    float seconds, decrease, increase, phaseIncrease;
    double rate;
    int i, stanceChanged = 0;
    unsigned int seed;
    if (s->zooming) {
        s->aimSpread = 255;
        s->aimSpreadFloat = 255.0f;
        return;
    }
    seconds = (float)((double)(s->commandTime - s->oldCommandTime) * 0.001);
    if (s->movementRecovery) {
        decrease = (float)((double)s->movementRecovery * seconds);
        rate = 0.0;
        for (i = 0; i < 2; ++i)
            rate += fabs(s->angles[i] * (360.0 / 65536.0) -
                s->oldAngles[i] * (360.0 / 65536.0)) * 1.5;
        for (i = 0; i < 2; ++i) rate += fabs(s->velocity[i]);
        if (s->movementFlags & 0x200) rate *= (double)0.8f;
        rate /= seconds;
        increase = (float)(rate - 30.0);
        if (!(rate - 30.0 > 0.0)) increase = 0.0f;
        else if (increase > 120.0f) increase = 120.0f;
        if (!(rate > 0.0)) rate = 0.0;
        else if (rate > 120.0) rate = 120.0;
        increase = (float)(int)((double)increase * (double)(1.0f / 120.0f) * seconds * 3200.0f);
        phaseIncrease = (float)(int)(rate * (double)(1.0f / 120.0f) * seconds * 3200.0f);
    } else {
        decrease = 1000.0f;
        increase = phaseIncrease = 0.0f;
    }
    if (!!s->ducked != !!(s->weaponFlags & 0x10)) {
        if (s->ducked) s->weaponFlags |= 0x10;
        else s->weaponFlags &= ~0x10u;
        s->movementInstability = 10000;
        stanceChanged = 1;
    }
    if (s->weaponState != 7) {
        s->phase = (int)(s->phase + 2.0 * phaseIncrease);
        if (s->weaponState != 0 || stanceChanged) {
            seed = s->seed * 69069u + 1u;
            s->phase = (int)((seed & 65535u) / 65536.0 * 1000.0);
        }
        if (s->phase > 1000) s->phase -= 1000;
    }
    if ((s->weaponFlags & 4) && s->scoped == 0.0f) {
        increase = (float)((double)increase * (double)0.67f);
        if (s->movementInstability > 200) increase = 0.0f;
    }
    s->movementInstability = (int)((double)s->movementInstability + ((double)increase - decrease));
    if (s->movementInstability > 1000) s->movementInstability = 1000;
    else if (s->movementInstability < 0) s->movementInstability = 0;
    s->shotInstability = (int)((double)s->shotInstability - (double)s->shotRecovery * seconds);
    if (s->shotInstability > 1000) s->shotInstability = 1000;
    else if (s->shotInstability < 0) s->shotInstability = 0;
}
