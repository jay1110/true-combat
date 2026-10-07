#include "cg_local.h"
#include "tce_smoke_grenade.h"
#include "tce_flash.h"

tceFlashState_t tceFlash;

void TCE_CG_ResetFlash(void) {
    memset(&tceFlash,0,sizeof(tceFlash));
}

/* CG_FlashBang300774f0. Camera-space exposure is client local; concussion
 * updates hearing only. Seven short offset traces allow exposure around edges. */
void TCE_CG_FlashBang(const vec3_t origin,int concussionOnly) {
    vec3_t target,delta,start;
    trace_t tr;
    float scale=tceSmokeNewBBox?1.25f:1.0f;
    float cap=concussionOnly?8000.0f:16000.0f;
    float exposure=0,blind=0,strength=0,hearing=0,distance,range;
    double facing,extra,length;
    int i,remaining;
    if(!cg.snap)return;
    /* Original snapshot.ps starts+0x2c: snapshot+0x180 is invulnerability. */
    if(cgs.gametype==5 && cg.snap->ps.powerups[PW_INVULNERABLE]>0)return;
    VectorCopy(origin,target);target[2]+=2;
    VectorSubtract(target,cg.refdef.vieworg,delta);
    length=sqrt((double)delta[0]*delta[0]+(double)delta[1]*delta[1]+(double)delta[2]*delta[2]);
    range=scale*500;
    if(length<range) {
        for(i=0;i<7;++i) {
            VectorCopy(cg.refdef.vieworg,start);
            if(i)start[(i-1)/2]+=(i&1)?-15.0f:15.0f;
            CG_Trace(&tr,start,vec3_origin,vec3_origin,target,cg.snap->ps.clientNum,CONTENTS_SOLID|CONTENTS_BODY);
            if(tr.fraction==1) {
                exposure=(float)(((double)range-length)/range*2.0);
                if(exposure>1)exposure=1;
                blind=exposure*5000;strength=exposure;break;
            }
        }
        range=scale*300;
        extra=((double)range-length)/range+exposure;
        if(extra>1)extra=1;
        hearing=(float)(extra*cap);
    }
    facing=(double)cg.refdef.viewaxis[0][0]*delta[0]+(double)cg.refdef.viewaxis[0][1]*delta[1]+(double)cg.refdef.viewaxis[0][2]*delta[2];
    if(!concussionOnly && facing>0) {
        CG_Trace(&tr,cg.refdef.vieworg,vec3_origin,vec3_origin,target,cg.snap->ps.clientNum,CONTENTS_SOLID|CONTENTS_BODY);
        if(tr.fraction>=1) {
            distance=(float)length;
            if(length!=0) {
                double inverse=1.0/length;
                for(i=0;i<3;++i)delta[i]=(float)(delta[i]*inverse);
            } else VectorClear(delta);
            facing=(double)cg.refdef.viewaxis[0][0]*delta[0]+(double)cg.refdef.viewaxis[0][1]*delta[1]+(double)cg.refdef.viewaxis[0][2]*delta[2];
            if(cg.tceAimActive && cg.refdef.fov_x<30) {
                double cosine;
                facing=(float)facing;
                distance=(float)((double)cg.refdef.fov_x*distance*(1.0f/90.0f));
                cosine=cos((double)cg.refdef.fov_x*0.01745329238474369f);
                facing=facing<=cosine?0:(facing-cosine)/(1.0-cosine)*2.0;
            }
            if(distance<1000) {
                extra=(1000.0-distance)*facing*.001f;
                blind=(float)(extra*5000.0+blind);strength=(float)(extra+strength);
            }
            CG_SmokePuff(target,vec3_origin,256,1,1,1,1,250,cg.time,0,0x101,cgs.media.tceImpactFlare);
        }
    }
    remaining=tceFlash.deafUntil-cg.time;if(remaining>0)hearing+=remaining;
    if(hearing>cap)hearing=cap;
    tceFlash.deafUntil=cg.time+(int)hearing;
    if(!concussionOnly) {
        extra=blind;
        remaining=tceFlash.blindUntil-cg.time;if(remaining>0)extra+=remaining;
        if(extra>10000)extra=10000;
        tceFlash.blindUntil=cg.time+(int)extra;
        if(strength>0)tceFlash.blindActive=(int)(strength+tceFlash.blindActive);
        if(tceFlash.blindActive>1)tceFlash.blindActive=1;
    }
}

static qboolean FlashCanRender(void) {
    return cg.snap && !cg.renderingThirdPerson && cgs.gamestate==GS_PLAYING &&
        cg.predictedPlayerState.pm_type!=PM_SPECTATOR &&
        !(cg.predictedPlayerState.pm_flags&PMF_FOLLOW) &&
        cg.predictedPlayerState.stats[STAT_HEALTH]>0;
}

/* CG_DrawFlashBang30027c40: full white during the first part, linear final3s. */
void TCE_CG_DrawFlashBang(void) {
    int remaining=tceFlash.blindUntil-cg.time;
    vec4_t color={1,1,1,0};
    if(!FlashCanRender()||remaining<=0)return;
    if(remaining>3000)remaining=3000;
    color[3]=remaining*(1.0f/3000.0f);
    if(color[3]<0)color[3]=0;if(color[3]>1)color[3]=1;
    CG_FillRect(-5,-85,862,650,color);
}

/* Original300275f0: quarter-power hearing loss, squared sound volume fade. */
void TCE_CG_UpdateFlashRinging(void) {
    int remaining=tceFlash.deafUntil-cg.time,volume;
    double fade;
    tceFlash.deafness=0;
    if(!FlashCanRender()||remaining<=0)return;
    if(remaining>6000){tceFlash.deafness=1;volume=255;}
    else {
        fade=(double)remaining*(1.0f/6000.0f);
        tceFlash.deafness=(float)sqrt(sqrt(fade));
        volume=(int)(fade*fade*255);
    }
    trap_S_AddLoopingSound(cg.refdef.vieworg,vec3_origin,cgs.media.tceDeafBeep,volume,0);
}
