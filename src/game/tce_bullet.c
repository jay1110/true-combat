#include "tce_bullet.h"

void TCE_BulletEndpos(const tce_weaponDef_t *def,
                     const tce_bulletAim_t *aim, vec3_t end) {
    double shotScale, minimum, angle, vertical;
    float movementScale, spread, horizontal, radius, swayRight=0, swayUp=0;
    vec3_t direction;
    unsigned int seed=aim->seed;
    int i, distance=8192, scatter=1;
    shotScale=aim->aiming ? 0.0 : def->unknown_0f8[0]*(double)0.001f;
    movementScale=aim->aiming ? 0.0f : (float)(def->unknown_0f8[3]*(double)0.001f);
    spread=(float)((aim->movementSpread*(double)movementScale+
                    aim->shotSpread*shotScale)*(double)0.7f);
    minimum=def->unknown_0f8[2]*(double)0.08192f;
    if(minimum>0) {
        if(aim->prone) minimum*=(double)0.33333f;
        else if(aim->ducked) minimum*=(double)0.66667f;
        if(spread<minimum) spread=(float)minimum;
    }
    if(def->scoped>0) {
        if(aim->aiming) {distance=16384;scatter=0;}
        else if(def->unknown_1c0) spread=1200;
    }
    AngleVectors(aim->shotAngles,direction,NULL,NULL);
    for(i=0;i<3;++i) end[i]=(float)((double)direction[i]*distance+aim->muzzle[i]);
    if(!aim->aiming) {
        angle=aim->phase*(double)0.0062831854447722435f;
        swayRight=(float)(sin(angle)*600.0);
        swayUp=(float)(cos(angle)*600.0);
    }
    if(scatter) {
        seed=seed*69069u+1u;
        radius=(float)sqrt((seed&65535u)/65536.0);
        seed=seed*69069u+1u;
        angle=(2.0*((seed&65535u)/65536.0)-1.0)*3.141;
        horizontal=(float)(cos(angle)*radius*spread*(double)1.27f+swayRight);
        vertical=sin(angle)*radius*spread*(double)1.27f+swayUp;
        for(i=0;i<3;++i) end[i]=(float)((double)aim->right[i]*horizontal+end[i]);
        for(i=0;i<3;++i) end[i]=(float)((double)aim->up[i]*vertical+end[i]);
    }
}

int TCE_BulletClientDamage(const tce_weaponDef_t *def, int damage,
                          int distance, int newBoundingBox) {
    double range=def->unknown_130*(newBoundingBox?1.25:1.0)*(double)39.370079040527344f;
    double value;
    float input=(float)damage, travelled=(float)distance;
    if(range<=0) range=999999.0;
    value=input/(travelled/range+1.0);
    if(def->unknown_1c0 && def->unknown_12c*1.25<input) {
        if(distance<240) value*=0.5;
        else if(distance<480)
            value*=((double)travelled-240.0)*(double)0.0020833334419876337f+0.5;
    }
    value*=(double)1.35f;
    if(value<0) value=0;
    return (int)value;
}
