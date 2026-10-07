#include "tce_bullet.h"

void TCE_BulletDeflect(const vec3_t start, vec3_t end, float retainedFraction,
                      unsigned seed, int wallsRemaining) {
    vec3_t direction, right, up;
    float radius, horizontal;
    double angle, vertical, loss=1.0-retainedFraction;
    int i;
    seed=(seed+11u*(unsigned)wallsRemaining)&65535u;
    VectorSubtract(end,start,direction);
    VectorNormalize(direction);
    PerpendicularVector(right,direction);
    CrossProduct(direction,right,up);
    seed=seed*69069u+1u;
    radius=(float)sqrt((seed&65535u)/65536.0);
    seed=seed*69069u+1u;
    angle=(2.0*((seed&65535u)/65536.0)-1.0)*3.141;
    horizontal=(float)(cos(angle)*radius*loss*2539.9999618530273);
    vertical=sin(angle)*radius*loss*2539.9999618530273;
    for(i=0;i<3;++i)
        end[i]=(float)((double)right[i]*horizontal+(double)up[i]*vertical+end[i]);
}

static void TCE_SnapWallPoint(vec3_t point, const vec3_t toward) {
    int i;
    for(i=0;i<3;++i) point[i]=(float)(point[i]<toward[i]?ceil(point[i]):floor(point[i]));
}

int TCE_BulletWallExit(const trace_t *entry, const vec3_t start, const vec3_t end,
                      int passEntity, tce_bulletTrace_t trace, trace_t *exit) {
    vec3_t direction, from;
    int i;
    VectorSubtract(end,start,direction);
    VectorNormalize(direction);
    for(i=0;i<3;++i)
        from[i]=(entry->endpos[i]-entry->plane.normal[i])+direction[i];
    trace(exit,from,NULL,NULL,end,passEntity,1);
    if(exit->fraction>=1) return 0;
    if(Distance(exit->endpos,entry->endpos)<4) {
        for(i=0;i<3;++i)
            from[i]=(float)(((double)entry->endpos[i]-entry->plane.normal[i])+direction[i]*4.0);
        trace(exit,from,NULL,NULL,end,passEntity,1);
        if(exit->fraction>=1) return 0;
    }
    TCE_SnapWallPoint(exit->endpos,from);
    VectorCopy(exit->endpos,from);
    trace(exit,from,NULL,NULL,start,passEntity,1);
    TCE_SnapWallPoint(exit->endpos,from);
    return Distance(entry->endpos,start)<Distance(exit->endpos,start);
}

int TCE_BulletMaterialLoss(const tce_weaponDef_t *def, unsigned entryFlags,
                          unsigned exitFlags, double thickness, int energy,
                          int damage, int *remainingEnergy, int *remainingDamage) {
    const tce_pierceMaterial_t *entry=&tcePierceTable[BG_SurfaceFlag2Type(entryFlags)];
    const tce_pierceMaterial_t *exit=&tcePierceTable[BG_SurfaceFlag2Type(exitFlags)];
    const tce_pierceMaterial_t *material=entry;
    double fraction, remaining;
    float storedFraction, storedEnergy, inputEnergy=(float)energy;
    if((entryFlags&0x80000) || entry->resistance>=9999) return 0;
    if(entry->resistance>=1000 && Q_stricmp(def->caliberClass,"338LAPUA") &&
       Q_stricmp(def->caliberClass,"50BMG")) return 0;
    if(exit->resistance>=entry->resistance) material=exit;
    if(thickness<0) thickness=0;
    fraction=(inputEnergy-(thickness/material->thicknessScale+1)*material->resistance)/inputEnergy;
    storedFraction=(float)fraction;
    remaining=(double)storedFraction*inputEnergy;
    storedEnergy=(float)remaining;
    if(!(fraction>0) || remaining<5) return 0;
    *remainingEnergy=(int)storedEnergy;
    *remainingDamage=(int)(damage*((double)storedEnergy/def->unknown_134));
    return 1;
}
