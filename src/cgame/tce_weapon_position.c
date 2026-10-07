#include "../game/q_shared.h"
#include "../game/tce_bg.h"
#include "tce_weapon_position.h"

/* Keep the original x87 intermediates until each observed float store. */
static void translate(float *v, const float *axis, double scale) {
    int i; for (i=0;i<3;++i) v[i]=(float)(v[i]+scale*axis[i]);
}
static double dot(const float *a,const float *b) {
    return (double)a[0]*b[0]+(double)a[1]*b[1]+(double)a[2]*b[2];
}
static float clampSway(float value,float limit) {
    if(value>limit)return limit;
    return value < -limit ? -limit : value;
}
static void posture(tce_weaponPosition_t *s,float *origin,float *angles,int active,int timer,int kind) {
    int delta=active?s->time-timer:s->time+timer;
    float f;
    if(active) {
        s->postureModified=1;
        if(delta<=0)return;
        f=delta>200?1.0f:(float)(1.0/(200.0/delta));
    } else {
        if(delta>=200)return;
        s->postureModified=1;
        f=delta==0?1.0f:(float)(1.0-1.0/(200.0/delta));
    }
    if(kind==0) {
        translate(origin,s->viewAxis[0],(float)(f*-25.0));
        translate(origin,s->viewAxis[1],(float)(f*5.0));
        translate(origin,s->viewAxis[2],(float)(f*-5.0));
        angles[1]=(float)(angles[1]+f*60.0);
    } else {
        if(kind==2)translate(origin,s->viewAxis[0],(float)(f*-5.0));
        translate(origin,s->viewAxis[2],(float)(f*(kind==1?-5.0:-20.0)));
        angles[1]=(float)(angles[1]+f*10.0);
        angles[0]=(float)(angles[0]+f*30.0);
    }
}
void TCE_CG_CalculateWeaponPosition(tce_weaponPosition_t *s,float *origin,float *angles) {
    const tce_weaponDef_t *w=&weaponDef[s->weapon];
    int i,delta,scoped=s->aiming && s->scopeEnabled && w->scoped>1;
    float f,dt,limit,strength,secondary,bobScale=1,idleScale=1,signedSpeed;
    float right[3],up[3],difference[3],target[2];
    double wave,recoilRate=0; int recoilPitch=0;
    VectorCopy(s->viewOrigin,origin);VectorCopy(s->viewAngles,angles);
    if(s->eFlags&0x8000)angles[0]=(float)(angles[0]*(5.0/6.0));
    if(!s->thirdPerson && (s->weapon==60||s->weapon==62) && s->weaponState!=1)angles[0]=s->mountedPitch;
    s->postureModified=0;
    posture(s,origin,angles,s->eFlags&0x100000,s->proneMovingTime,0);
    posture(s,origin,angles,s->flags_3407dfb0&0x4000,s->stanceTime,1);
    posture(s,origin,angles,s->pmFlags&4,s->duckTime,2);
    delta=s->time-s->leanTime;
    f=s->aiming?(delta<200?(float)(1.0-delta*(double).005f):0.0f):
        (delta<200?(float)(delta*(double).005f):1.0f);
    if(s->lean!=0 && f>0) {
        angles[2]=(float)(angles[2]-s->lean*.5*f);
        AngleVectors(angles,NULL,right,up);
        translate(origin,right,(double)f*angles[2]);
        angles[0]=(float)(angles[0]+abs((int)s->lean)*.5*f);
        AngleVectors(s->viewAngles,NULL,right,NULL);
        translate(origin,right,-s->lean*.25*f);
    }
    delta=s->time-s->shotTime;
    if(w->usesRecoilAnimMod && !w->pump && !scoped && delta>0) {
        if(delta<300){recoilRate=(double)(1.0f/300.0f);recoilPitch=1;}
    } else if(w->usesRecoilAnimMod && w->pump && w->semiauto && s->count_3407dff8>0 && delta>0) {
        if(delta<500){recoilRate=(double).002f;recoilPitch=1;}
    } else if(w->usesPistolAnimMod && delta>0 && delta<50)recoilRate=(double).02f;
    if(recoilRate) {
        wave=sin(sqrt(sqrt(delta*recoilRate))*3.1415927410125732);
        translate(origin,s->viewAxis[0],wave*-4.0);
        if(recoilPitch)angles[0]=(float)(angles[0]-wave*4.0);
    }
    delta=s->time-s->firemodeTime;
    if(BG_FiremodeWeapon(s->weapon) && delta<750 && !scoped) {
        wave=(1.0-cos(delta*(double)(1.0f/750.0f)*(double)6.2831854820251465f))*.5;
        translate(origin,s->viewAxis[2],wave*-2.0);
        angles[0]=(float)(angles[0]-wave*4);
        if(s->weapon==43||s->weapon==33||s->weapon==45)wave=-wave;
        angles[2]=(float)(angles[2]+wave*4);angles[1]=(float)(angles[1]-wave*4);
    }
    signedSpeed=(s->bobCycle&1)?-s->speed:s->speed;
    if(s->aiming && s->weapon!=30 && s->weapon!=9 && s->weapon!=4) {
        bobScale=scoped?0.0f:.15f;idleScale=0;
    }
    if(!scoped) {
        /* The original subtracts current time from previous time. */
        dt=(float)((s->swayTime-s->time)*(double).001f);if(dt<.005f)dt=.005f;
        for(i=0;i<3;++i){difference[i]=s->viewAxis[0][i]-s->priorForward[i];s->priorForward[i]=s->viewAxis[0][i];}
        target[0]=(float)(dot(s->velocity,s->viewAxis[1])*(double).1f-dot(difference,s->viewAxis[1])/dt);
        target[1]=(float)(dot(s->velocity,s->viewAxis[0])*(double).1f-dot(difference,s->viewAxis[2])/dt);
        f=(!(s->eFlags&0x80000) && s->proneTime+s->time>749)?250.0f:750.0f;
        if((float)(s->time-s->stepTime)<f)target[1]+=s->stepChange;
        limit=s->aiming?(float)(w->unknown_0f8[3]*(double)(1.0f/600.0f)):3.0f;
        secondary=s->aiming?.5f:1.0f;
        strength=(float)(s->scale_3407dfc0*(double)secondary*(double).001f+s->scale_3407dfbc*(double).001f);
        if(strength>1)strength=1;if(!s->aiming && strength<.5f)strength=.5f;
        for(i=0;i<2;++i)target[i]=fabs(target[i])<.2?0.0f:clampSway(target[i],limit);
        s->swayHorizontal=clampSway((float)((target[0]*(double).1f+s->swayHorizontal)*(double).9090908765792847f),limit);
        s->swayVertical=clampSway((float)((target[1]*(double).1f+s->swayVertical)*(double).9090908765792847f),limit);
        s->swayTime=s->time;
        translate(origin,s->viewAxis[1],s->swayHorizontal*(double)strength*.25);
        translate(origin,s->viewAxis[2],s->swayVertical*(double)strength*.25);
        angles[1]=(float)(angles[1]-s->swayHorizontal*(double)strength);
        angles[0]=(float)(angles[0]+s->swayVertical*(double)strength);
    }
    angles[2]=(float)(angles[2]+s->bobSin*(double)signedSpeed*bobScale*.005);
    angles[1]=(float)(angles[1]+s->bobSin*(double)signedSpeed*bobScale*.01);
    angles[0]=(float)(angles[0]+s->bobSin*(double)s->speed*bobScale*.005);
    delta=s->time-s->landTime;
    if(bobScale>0 && delta<450) {
        f=(float)(delta<150?s->landChange*(double)delta/600.0:(450-delta)*(double)s->landChange/1200.0);
        origin[2]+=f;
    }
    for(i=0;i<3;++i)angles[i]+=s->gunViewAngles[i];
    if(s->developer)for(i=0;i<3;++i)angles[i]+=s->developerAngles[i];
    if(!w->noTacMode && s->tacticalScale!=0) {
        angles[0]=(float)(angles[0]+(s->tacticalPitch-2000.0)*s->tacticalScale*(double).01f);
        angles[1]=(float)(angles[1]+(s->tacticalYaw-2000.0)*s->tacticalScale*(double).01f);
    }
    if(!(s->eFlags&0x8000) && s->weapon!=60 && s->weapon!=62) {
        wave=sin(s->time*.001)*80.0*idleScale*.01;
        for(i=0;i<3;++i)angles[i]=(float)(angles[i]+wave);
    }
    for(i=0;i<3;++i)angles[i]-=s->kickAngles[i];
}
