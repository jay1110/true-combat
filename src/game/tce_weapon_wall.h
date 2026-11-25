/* Shared prediction/server wall clearance. No client setting or engine ABI change. */
#ifndef TCE_WEAPON_WALL_H
#define TCE_WEAPON_WALL_H
#include "tce_weapon_wall_state.h"
static float TCE_WeaponWallFraction(const playerState_t *ps, int leanMode,
    void (*trace)(trace_t *, const vec3_t, const vec3_t, const vec3_t,
                  const vec3_t, int, int))
{
    trace_t hit;
    vec3_t start, end, forward, right;
    vec3_t mins = { -2, -2, -2 }, maxs = { 2, 2, 2 };
    if (!TCE_WeaponWallFirearm(ps->weapon))
        return 1.f;
    {
        vec3_t angles; float lean=ps->leanf, fraction=1.f; int probe;
        VectorCopy(ps->viewangles,angles);
        if (leanMode) lean /= lean<0.f ? 3.3f : 1.8f;
        angles[ROLL] += leanMode ? lean : lean*.5f;
        AngleVectors(angles,forward,right,NULL);
        for (probe=0;probe<3;++probe) {
            VectorCopy(ps->origin,start);start[2]+=ps->viewheight;
            VectorMA(start,lean,right,start);
            if (probe) {
                if (!(ps->eFlags&EF_PRONE)) start[2]-=probe==1?10.f:18.f;
                VectorMA(start,probe==1?6.f:8.f,right,start);
            }
            VectorMA(start,48.f,forward,end);
            trace(&hit,start,mins,maxs,end,ps->clientNum,MASK_SOLID);
            /* Ground is not a wall; a prone view must not holster merely
             * because its low shoulder probe touches the floor. */
            if (!hit.startsolid && !hit.allsolid && hit.fraction<1.f && hit.plane.normal[2]>.65f) continue;
            if (hit.startsolid || hit.allsolid) fraction=0.f;
            else if (hit.fraction<fraction) fraction=hit.fraction;
        }
        return fraction;
    }
}
/* Search the shortest clear side direction. Reuse the previous side to avoid
 * left/right chatter at a symmetric corner. A crawling weapon is back-mounted. */
static float TCE_WeaponWallProneTarget(const playerState_t *ps,float previous,float *anchor,int *selectedSide,
    void (*trace)(trace_t *,const vec3_t,const vec3_t,const vec3_t,const vec3_t,int,int))
{
    vec3_t start,end,angles,forward,mins={-3,-3,-3},maxs={3,3,3};
    trace_t hit;float choices[2]={78.f,78.f},delta;int angle,side,preferred=*selectedSide<0?1:0;
    if(!(ps->eFlags&EF_PRONE)||!TCE_WeaponWallFirearm(ps->weapon)) { *selectedSide=0;return 0.f; }
    if(ps->eFlags&EF_PRONE_MOVING)return 78.f;
    VectorCopy(ps->origin,start);start[2]+=12.f;
    VectorCopy(ps->viewangles,angles);angles[PITCH]=0.f;
    AngleVectors(angles,forward,NULL,NULL);VectorMA(start,24.f,forward,start);
    for(angle=0;angle<=70;angle+=10) for(side=0;side<(angle?2:1);++side) {
        float yaw=angle*(side?-1.f:1.f);
        if(angle && choices[side]!=78.f)continue;
        VectorCopy(ps->viewangles,angles);angles[YAW]+=yaw;
        AngleVectors(angles,forward,NULL,NULL);VectorMA(start,45.f,forward,end);
        trace(&hit,start,mins,maxs,end,ps->clientNum,MASK_SOLID);
        if(!hit.startsolid&&!hit.allsolid && (hit.fraction>.99f || hit.plane.normal[2]>.65f)) {
            if(!angle) { *selectedSide=0;*anchor=ps->viewangles[YAW];return 0.f; }
            choices[side]=yaw;
        }
    }
    if(!*selectedSide) {
        preferred=(fabsf(choices[1])<fabsf(choices[0]) || (fabsf(choices[1])==fabsf(choices[0]) && previous<0.f))?1:0;
        if(choices[preferred]==78.f)return 78.f;
        *selectedSide=preferred?-1:1;*anchor=ps->viewangles[YAW];
        return choices[preferred];
    }
    delta=ps->viewangles[YAW]-*anchor;if(delta>180.f)delta-=360.f;if(delta< -180.f)delta+=360.f;
    /* Fixed 20-degree hysteresis plus a 15-degree clearance advantage. */
    if(fabsf(delta)>=20.f && choices[1-preferred]!=78.f &&
       (choices[preferred]==78.f || fabsf(choices[1-preferred])+15.f<fabsf(choices[preferred]))) {
        preferred=1-preferred;*selectedSide=preferred?-1:1;*anchor=ps->viewangles[YAW];
    }
    return choices[preferred];
}
static float TCE_WeaponWallProneApproach(float current,float target,int msec)
{
    float step;if(msec<0)msec=0;if(msec>50)msec=50;step=.2f*msec;
    if(current<target)return current+step<target?current+step:target;
    return current-step>target?current-step:target;
}
/* Keep the actual head envelope in free world space. This changes the
 * authoritative player origin, not just its rendered head or hit geometry. */
static void TCE_WeaponWallProtectHead(playerState_t *ps, const vec3_t bodyMins,
    const vec3_t bodyMaxs, int leanMode, float amount,
    void (*trace)(trace_t *,const vec3_t,const vec3_t,const vec3_t,const vec3_t,int,int))
{
    vec3_t angles={0,0,0},forward,right,start,end,offset,wanted;
    vec3_t mins,maxs;trace_t head,body;float scale,reach,lean,pitch,length,back;int i;
    if (ps->pm_type!=PM_NORMAL || (ps->eFlags&EF_DEAD) || (ps->pm_flags&PMF_LADDER)) return;
    scale=(ps->stats[STAT_TCE_FLAGS]&0x200)?1.25f:1.f;
    angles[YAW]=ps->viewangles[YAW];AngleVectors(angles,forward,right,NULL);
    lean=ps->leanf;if(leanMode)lean/=lean<0.f?3.3f:1.8f;
    pitch=ps->viewangles[PITCH];if(pitch>180.f)pitch-=360.f;
    if (amount>.5f) amount=.5f;if(amount<0.f)amount=0.f;
    pitch*=.75f*(1.f-amount*2.f);if(pitch<0.f)pitch=0.f;
    reach=(ps->eFlags&EF_PRONE)?24.f*scale:(8.f+20.f*sinf(pitch*.01745329252f))*scale;
    VectorCopy(ps->origin,start);
    start[2]+=(ps->eFlags&EF_PRONE)?12.f*scale:ps->viewheight-3.f;
    for(i=0;i<3;++i) {
        mins[i]=-8.f*scale;maxs[i]=8.f*scale;
        offset[i]=reach*forward[i]+lean*right[i];end[i]=start[i]+offset[i];
    }
    trace(&head,start,mins,maxs,end,ps->clientNum,MASK_SOLID);
    if (!head.startsolid && !head.allsolid && head.fraction>=.999f) return;
    length=sqrtf(offset[0]*offset[0]+offset[1]*offset[1]);if(length<1.f)return;
    back=1.f-head.fraction+.5f/length;
    for(i=0;i<3;++i)wanted[i]=ps->origin[i]-offset[i]*back;
    trace(&body,ps->origin,bodyMins,bodyMaxs,wanted,ps->clientNum,MASK_SOLID);
    if (body.startsolid || body.allsolid) return;
    for(i=0;i<3;++i)ps->origin[i]+=(wanted[i]-ps->origin[i])*body.fraction;
    if(!head.startsolid && !head.allsolid) {
        float into=ps->velocity[0]*head.plane.normal[0]+ps->velocity[1]*head.plane.normal[1]+ps->velocity[2]*head.plane.normal[2];
        if(into<0.f)for(i=0;i<3;++i)ps->velocity[i]-=into*head.plane.normal[i];
    }
}
static float TCE_WeaponWallPitch(int enabled, int weapon, float amount)
{
    if (!enabled || !TCE_WeaponWallFirearm(weapon)) return 0.f;
    amount *= 2.f;
    if (amount > 1.f) amount = 1.f;
    if (!(amount > 0.f)) amount = 0.f;
    return 78.f * amount;
}

/* Blend toward a downward low-ready direction, including when looking down;
 * adding a fixed angle could turn a floor-obstructed shot backwards. */
static float TCE_WeaponWallAimPitch(float basePitch, float wallPitch)
{
    if (wallPitch==0.f) return basePitch;
    if (basePitch > 180.f) basePitch -= 360.f;
    if (basePitch < -180.f) basePitch += 360.f;
    return basePitch + ((wallPitch<0.f?-78.f:78.f)-basePitch)*(fabsf(wallPitch)/78.f);
}

/* Bounded command-time approach: the low-ready threshold takes at least 250 ms. */
static float TCE_WeaponWallApproach(float current, float target, int msec)
{
    float step;
    if (msec < 0) msec = 0;
    if (msec > 50) msec = 50;
    step = msec * .002f;
    if (current < target) return current + step < target ? current + step : target;
    return current - step > target ? current - step : target;
}
#endif
