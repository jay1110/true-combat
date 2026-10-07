#include "cg_local.h"
#include "../game/tce_bg.h"
#include "tce_eject_brass.h"

static double ejectRandom(void) {
    return (rand() & 0x7fff) * (double)(1.0f / 32767.0f);
}
static double ejectCenteredRandom(void) { return 2.0 * (ejectRandom() - .5); }

/* Complete Windows CG_AddEjectBrass30071cb0. Returning the allocated entity
 * is an adapter extension so the caller can mark TC fragment provenance. */
localEntity_t *TCE_CG_AddEjectBrass(const refEntity_t *parent, const centity_t *cent,
    const tce_brassContext_t *context, qboolean localPlayer, qboolean cachedOrigin) {
    localEntity_t *le;
    vec3_t velocity,axis[3];
    const float *inheritedVelocity;
    float waterScale=1.0f,bounce=.4f;
    int category,i,base;
    double worldVelocity;
    if(cg_brassTime.integer<=0)return NULL;
    category=weaponDef[context->weapon].weaponCategory;
    if(category<=0)return NULL;
    le=CG_AllocLocalEntity();
    switch(category) {
    case 1:
        le->leBounceSoundType=(leBounceSoundType_t)8;
        le->refEntity.hModel=cgs.media.smallgunBrassModel;
        velocity[0]=(float)(ejectCenteredRandom()*15-30);
        velocity[1]=(float)(-40-ejectCenteredRandom()*15);
        velocity[2]=(float)(ejectCenteredRandom()*30+150);
        break;
    case 3:
        le->leBounceSoundType=(leBounceSoundType_t)9;
        le->refEntity.hModel=cgs.media.machinegunBrassModel;
        velocity[0]=(float)(ejectCenteredRandom()*20+60);
        velocity[1]=(float)(-160-ejectCenteredRandom()*40);
        velocity[2]=(float)(ejectCenteredRandom()*20+60);
        break;
    case 4:
        le->leBounceSoundType=(leBounceSoundType_t)7;
        le->refEntity.hModel=cgs.media.shotgunBrassModel;
        velocity[0]=(float)(ejectCenteredRandom()*20+50);
        velocity[1]=(float)(-90-ejectCenteredRandom()*30);
        velocity[2]=(float)(ejectCenteredRandom()*20+60);
        bounce=.3f;
        break;
    case 5:
        le->leBounceSoundType=(leBounceSoundType_t)10;
        le->refEntity.hModel=cgs.media.machinegunBrassModel;
        velocity[0]=(float)(ejectCenteredRandom()*20+50);
        velocity[1]=(float)(ejectCenteredRandom()*30-70);
        velocity[2]=(float)(ejectCenteredRandom()*20+50);
        break;
    default:
        le->leBounceSoundType=(leBounceSoundType_t)8;
        le->refEntity.hModel=cgs.media.smallgunBrassModel;
        velocity[0]=(float)(ejectCenteredRandom()*20+40);
        velocity[1]=(float)(-80-ejectCenteredRandom()*20);
        velocity[2]=(float)(ejectCenteredRandom()*20+50);
        break;
    }
    le->leType=LE_FRAGMENT;
    le->startTime=cg.time;
    base=(int)((unsigned)cg.time+(unsigned)cg_brassTime.integer);
    le->endTime=(int)(base+(cg_brassTime.integer/4)*ejectRandom());
    le->pos.trType=TR_GRAVITY;
    le->pos.trTime=(int)((unsigned)cg.time-(unsigned)(rand()&15));
    AnglesToAxis(cent->lerpAngles,axis);
    if(cachedOrigin)VectorCopy(ejectBrassCasingOrigin,le->refEntity.origin);
    else CG_PositionRotatedEntityOnTag(&le->refEntity,parent,"tag_brass");
    VectorCopy(le->refEntity.origin,le->pos.trBase);
    /* Unlike the older brass callbacks, only WATER scales this function. */
    if(CG_PointContents(le->refEntity.origin,-1)&CONTENTS_WATER)waterScale=.1f;
    inheritedVelocity=localPlayer?cg.snap->ps.velocity:context->entityVelocity;
    for(i=0;i<3;++i) {
        worldVelocity=(double)axis[2][i]*velocity[2]+(double)axis[1][i]*velocity[1]+
            (double)axis[0][i]*velocity[0]+inheritedVelocity[i];
        /* Y/Z are stored to temporaries before scaling in the original. */
        if(i)worldVelocity=(float)worldVelocity;
        le->pos.trDelta[i]=(float)(worldVelocity*waterScale);
    }
    AxisCopy(axisDefault,le->refEntity.axis);
    le->angles.trType=TR_LINEAR;
    le->bounceFactor=bounce*waterScale;
    le->angles.trTime=cg.time;
    le->angles.trBase[0]=90;
    le->angles.trBase[1]=(float)((ejectRandom()-.5)*60+cent->lerpAngles[1]);
    le->angles.trBase[2]=(float)((ejectRandom()-.5)*60+cent->lerpAngles[2]);
    VectorSet(le->angles.trDelta,2,1,0);
    le->leFlags=LEF_TUMBLE;
    le->leMarkType=LEMT_NONE;
    if(localPlayer) {
        le->leFlags=0x202;
        /* Original TC fragment renderer interprets this storage with0x200. */
        le->onFireEnd=(int)((unsigned)le->startTime+500u);
    } else le->sizeScale=context->sizeVariant?.75f:.6f;
    return le;
}
