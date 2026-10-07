/* Windows 3000bc40; Linux qagame 0008d864. */
#include "tce_movement.h"
#include <math.h>
#include <stdlib.h>

float TCE_PM_CmdScale(const tce_moveScale_t *s) {
    int maximum = abs(s->forward);
    float total, forward;
    double scale, sidewaysDivisor, backwardsDivisor, right;
    if (abs(s->right) > maximum) maximum = abs(s->right);
    if (abs(s->up) > maximum) maximum = abs(s->up);
    if (!maximum) return 0;
    total = (float)sqrt((double)(s->forward*s->forward + s->right*s->right + s->up*s->up));
    scale = (double)s->speed * maximum / (127.0 * total);
    if ((s->moveButtons & 0x20) && !s->ladder && !s->ducked) {
        scale *= s->sprintTime < 51 ? (double)s->runScale * (double)1.4f : s->sprintScale;
        if (s->moveButtons & 0x10) scale += scale;
    } else if (s->ducked) {
        scale *= s->crouchScale;
        if (s->moveButtons & 0x10) scale += scale;
        if (s->tactical || s->lean != 0 || (s->moveButtons & 0x10)) scale *= (double).85f;
    } else {
        scale *= s->runScale;
        if (s->tactical || s->lean != 0) {
            scale *= (double).55f;
            if (s->moveButtons & 0x10) scale *= (double)1.818f;
        }
        if (s->moveButtons & 0x10) scale *= (double).833f;
    }
    if (s->noclip) scale *= 3.0;
    if (s->weaponWeight != 4) {
        if (s->weaponWeight == 5) scale *= (double).95f;
        else if (s->carriedWeight < 4) scale *= (double)1.05f;
        else if (s->carriedWeight < 5) scale *= (double)1.025f;
    }
    if (s->gametype == 0 || s->gametype == 1) scale *= (double)s->movespeed * (double)(1.0f/127.0f);
    if ((s->commandButtons & 0x30) == 0x10 || s->tactical || s->lean != 0 || s->ducked) {
        backwardsDivisor = 1.5625; sidewaysDivisor = 1.25;
    } else if (s->commandButtons & 0x20) {
        backwardsDivisor = 2.5; sidewaysDivisor = 2.0;
    } else {
        backwardsDivisor = 1.875; sidewaysDivisor = 1.5;
    }
    forward = (float)s->forward;
    if (forward < 0) forward = (float)(forward / backwardsDivisor);
    right = s->right / sidewaysDivisor;
    return (float)(sqrt((double)forward*forward + right*right + s->up*s->up) / total * scale);
}
