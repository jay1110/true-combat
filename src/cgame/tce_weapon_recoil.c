#include <stdlib.h>
#include "tce_weapon_recoil.h"
void TCE_CG_WeaponFireRecoil(int weapon,float kick[3]) {
    float pitch=0,yawScale=0,negativeYaw;
    double yaw;
    switch(weapon) {
    case 2:case 7:case 14:case 37:case 38:case 52:case 53:case 54:case 65:
        break;
    case 3:case 8:case 10:case 31:case 33:case 41:case 59:case 62:
        pitch=(float)((rand()%3+1)*.3);yawScale=.6f;break;
    case 23:case 24:case 25:case 32:
        pitch=2;yawScale=1;break;
    case 57:case 58: pitch=.3f;break;
    default:return;
    }
    /* Windows keeps yaw in x87 until scaling, but stores negative yaw to a
     * float first. Preserve that asymmetry instead of negating scaled yaw. */
    yaw=((rand()&0x7fff)*(double)(1.0f/32767.0f)-.5)*2*yawScale;
    negativeYaw=(float)-yaw;
    kick[0]=-pitch*30.0f;kick[1]=(float)(yaw*30.0f);kick[2]=negativeYaw*30.0f;
}
