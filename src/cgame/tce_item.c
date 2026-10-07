#include "cg_local.h"
qboolean CG_PlayerSeesItem(playerState_t *ps, entityState_t *item, int atTime, int itemType);

/* TC:E Windows 30030080 / Linux CG_Item0006128a. Pickup models are
 * optional: the original falls back to the third-person weapon model. */
void TCE_CG_Item( centity_t *cent ) {
    refEntity_t ent;
    entityState_t *es = &cent->currentState;
    gitem_t *item;
    int i;
    if(es->modelindex >= bg_numItems)
        CG_Error("Bad item index %i on entity",es->modelindex);
    if(!es->modelindex || (es->eFlags & EF_NODRAW)) return;
    item = &bg_itemlist[es->modelindex];
    memset(&ent,0,sizeof(ent));
    if(item->giType == IT_WEAPON) {
        weaponInfo_t *weapon = &cg_weapons[item->giTag];
        if(weapon->standModel) {
            refEntity_t stand;
            memset(&stand,0,sizeof(stand));
            stand.hModel=weapon->standModel;
            if(es->eFlags & EF_SPINNING) {
                if(es->groundEntityNum == -1 || !es->groundEntityNum) {
                    VectorCopy(cg.autoAnglesSlow,cent->lerpAngles);
                    VectorCopy(cg.autoAnglesSlow,cent->lastLerpAngles);
                } else VectorCopy(cent->lastLerpAngles,cent->lerpAngles);
            }
            AnglesToAxis(cent->lerpAngles,stand.axis);
            VectorCopy(cent->lerpOrigin,stand.origin);
            for(i=0;i<3;++i) VectorScale(stand.axis[i],1.5f,stand.axis[i]);
            CG_PositionEntityOnTag(&ent,&stand,es->frame ? va("tag_stand%d",es->frame) : "tag_stand",0,NULL);
            VectorCopy(ent.origin,ent.oldorigin);
            ent.nonNormalizedAxes=qtrue;
        } else {
            if(weapon->droppedAnglesHack || !weapon->weaponModel[W_PU_MODEL].model)
                cent->lerpAngles[2]+=90.0f;
            AnglesToAxis(cent->lerpAngles,ent.axis);
            VectorCopy(cent->lerpOrigin,ent.origin);
            VectorCopy(cent->lerpOrigin,ent.oldorigin);
            /* Original updates angles after constructing the current axes. */
            if(es->eFlags & EF_SPINNING) {
                if(es->groundEntityNum == -1 || !es->groundEntityNum) {
                    VectorCopy(cg.autoAnglesSlow,cent->lerpAngles);
                    VectorCopy(cg.autoAnglesSlow,cent->lastLerpAngles);
                } else VectorCopy(cent->lastLerpAngles,cent->lerpAngles);
            }
        }
    } else {
        if(item->giType == IT_AMMO && (es->otherEntityNum != 254 || es->effect3Time != 254))
            cent->lerpAngles[0]-=90.0f;
        AnglesToAxis(cent->lerpAngles,ent.axis);
        if(item->giType == IT_HEALTH) {
            for(i=0;i<3;++i) VectorScale(ent.axis[i],0.6f,ent.axis[i]);
            ent.nonNormalizedAxes=qtrue;
        }
        VectorCopy(cent->lerpOrigin,ent.origin);
        VectorCopy(cent->lerpOrigin,ent.oldorigin);
        if(es->eFlags & EF_SPINNING) {
            VectorCopy(cg.autoAnglesSlow,cent->lerpAngles);
            AxisCopy(cg.autoAxisSlow,ent.axis);
        }
    }
    if(es->modelindex2) ent.hModel=cgs.gameModels[es->modelindex2];
    else if(item->giType == IT_WEAPON) {
        weaponInfo_t *weapon=&cg_weapons[item->giTag];
        ent.hModel=weapon->weaponModel[W_PU_MODEL].model;
        if(!ent.hModel) ent.hModel=weapon->weaponModel[W_TP_MODEL].model;
        if(item->giTag == 12 && es->density == 2) ent.customShader=weapon->modModels[0];
    } else {
        if(item->giType == IT_AMMO && es->otherEntityNum2 != cg.snap->ps.clientNum) return;
        if(item->giType == IT_HEALTH && es->otherEntityNum2 == cg.snap->ps.clientNum) return;
        ent.hModel=cg_items[es->modelindex].models[0];
    }
    if(!cent->usehighlightOrigin) {
        vec3_t mins,maxs,offset;
        trap_R_ModelBounds(ent.hModel,mins,maxs);
        for(i=0;i<3;++i) offset[i]=mins[i]+0.5f*(maxs[i]-mins[i]);
        VectorCopy(cent->lerpOrigin,cent->highlightOrigin);
        for(i=0;i<3;++i)
            cent->highlightOrigin[i]+=offset[0]*ent.axis[0][i]+offset[1]*ent.axis[1][i]+offset[2]*ent.axis[2][i];
        cent->usehighlightOrigin=qtrue;
    }
    ent.renderfx |= RF_MINLIGHT;
    if(cg_drawCrosshairPickups.integer) {
        qboolean seen=CG_PlayerSeesItem(&cg.predictedPlayerState,es,cg.time,item->giType);
        if(item->giType == IT_TREASURE)
            trap_R_AddCoronaToScene(cent->highlightOrigin,1,0.85f,0.5f,2,es->number,seen);
    }
    trap_R_AddRefEntityToScene(&ent);
}

