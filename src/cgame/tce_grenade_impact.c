#include "tce_wall_renderer.h"
#include "tce_impact_particles.h"
#include "tce_fragment_sound.h"
#include "tce_smoke_grenade.h"
#include "tce_flash.h"
#include "tce_bullet_renderer.h"
#include "tce_shotgun_prediction.h"
#include "../game/tce_bg.h"
void CG_TCEAddLocalBulletSparks(vec3_t,vec3_t,int,int,int,float);
void CG_Explodef(vec3_t,vec3_t,int,int,int,int,qhandle_t);
void CG_AddDirtBulletParticles(vec3_t,vec3_t,int,int,int,float,float,float,float,qhandle_t);
void CG_WaterRipple(qhandle_t,vec3_t,vec3_t,int,int);
void CG_AddDebris(vec3_t,vec3_t,int,int,int);

/* Original eight-argument wall controller and all its material/media groups. */
static int volume(vec3_t origin,float amount,float range,int linear) {
    tce_fragmentSoundContext_t s;
    memset(&s,0,sizeof(s));VectorCopy(cg.refdef.vieworg,s.listener);
    s.distanceVariant=tceSmokeNewBBox;s.attenuation=tceFlash.deafness;
    s.disabled=cg.tcePortalScopeRendering;
    return TCE_CG_SoundVolume(origin,amount,range,linear,&s);
}
static void printMessage(const char *s) { CG_Printf("%s",s); }
static void particle(vec3_t p,vec3_t d,int count,int type,float alpha) {
    tce_impactParticleMedia_t m;
    m.blood=cgs.media.bloodTrailShader;
    m.smoke3=cgs.media.tceImpactSmokePuff3;
    m.smoke4=cgs.media.tceImpactSmokePuff4;
    TCE_CG_ParticleTest(p,d,count,type,alpha,&m);
}
static void flash(vec3_t p,int only) { TCE_CG_FlashBang(p,only); }
static void *puff(vec3_t p,vec3_t v,float radius,float r,float g,float b,float a,
    float duration,int start,int fade,int flags,int shader) {
    return CG_SmokePuff(p,v,radius,r,g,b,a,duration,start,fade,flags,shader);
}
static void mark(int shader,vec3_t p,vec3_t d,float angle,float r,float g,float b,
    float a,int fade,float radius,int temporary,int duration) {
    CG_EliteImpactMark(shader,p,d,angle,r,g,b,a,fade,radius,temporary,duration);
}
static void explosion(const char *name,vec3_t p,vec3_t v,int duration,int first,int last,int light) {
    CG_ParticleExplosion((char *)name,p,v,duration,first,last,light);
}
static void farSound(const vec3_t p,int entity,int channel,int sound,int flags,int amount) {
    trap_S_StartSoundExVControl((float *)p,entity,channel,sound,flags,amount);
}
static int surfaceType(int material) { return (int)BG_SurfaceType2Flag((unsigned)material); }
void CG_TCEMissileHitWall(int weapon,int effect,vec3_t origin,vec3_t normal,vec3_t direction,unsigned flags,int forceEffect,int materialIsType) {
    tce_wallRendererContext_t s;
    int i;

    memset(&s,0,sizeof(s));s.view=cg.refdef_current;s.time=cg.time;
    s.markTime=cg_markTime.integer;s.portal=cg.tcePortalScopeRendering;
    /* Original 340a2424 is set by CG_ParseSkyBox (3006aff0). */
    s.forceMarks=cg.skyboxEnabled;
    s.surfaceType=surfaceType;s.bulletParticles=CG_TCEAddLocalBulletSparks;
    s.flatSparks=CG_FlatSparks;s.explode=CG_Explodef;s.registerShader=trap_R_RegisterShader;
    /* Original weaponDef base34c1df20, field34c1e068 = pelletCount(+148). */
    for(i=0;i<64;++i)s.weaponMaterialMode[i]=weaponDef[i].pelletCount;
    s.soundVolume=volume;s.print=printMessage;s.particle=particle;
    s.concussion=flash;s.sparks=CG_GlowSparks;
    s.debrisParticles=CG_EliteAddBulletParticles;s.smokePuff=puff;
    s.cmPointContents=trap_CM_PointContents;s.boxTrace=trap_CM_BoxTrace;
    s.dirtParticles=CG_AddDirtBulletParticles;s.ripple=CG_WaterRipple;
    s.eliteMark=mark;s.impactMark=CG_ImpactMark;
    s.sound=trap_S_StartSound;s.soundEx=farSound;s.projectDecal=trap_R_ProjectDecal;
    s.pointContents=CG_PointContents;s.particleExplosion=explosion;s.debris=CG_AddDebris;
    s.media.burn=cgs.media.burnMarkShader;
    s.media.explosionFlash=cgs.media.tceImpactFlare;
    s.media.waterRipple=cgs.media.wakeMarkShaderAnim;
    /* Preserve original handles despite historical misleading field names. */
    s.media.dirt=cgs.media.dirtParticle1Shader;
    s.media.water=cgs.media.tceImpactSnow;
    s.media.snow=cgs.media.tceImpactSand;
    s.media.grass=cgs.media.tceImpactGravel;
    s.media.mud=cgs.media.tceImpactSoil;
    s.media.explosive4=trap_S_RegisterSound("sound/weapons/grenade/m84_boost.wav",qfalse);
    s.media.explosive30=trap_S_RegisterSound("sound/weapons/grenade/m83smoke_boost.wav",qfalse);
    {
        s.media.bulletDefault=cgs.media.bulletMarkShader;
        s.media.bulletStone=cgs.media.tceBulletStoneMark;
        s.media.exitStone=cgs.media.tceBulletStoneExit;
        s.media.bulletMetal=cgs.media.bulletMarkShaderMetal;
        s.media.exitMetal=cgs.media.tceBulletMetalExit;
        s.media.bulletWood=cgs.media.bulletMarkShaderWood;
        s.media.exitWood=cgs.media.tceBulletWoodExit;
        s.media.bulletGlass=cgs.media.bulletMarkShaderGlass;
        for(i=0;i<5;++i) {
            s.media.fabricHit[i]=trap_S_RegisterSound(va("sound/weapons/impact/bullethit_fence%i.wav",i+1),qfalse);
            s.media.metal2Hit[i]=trap_S_RegisterSound(va("sound/weapons/impact/bullethit_tin%i.wav",i+1),qfalse);
            s.media.strawHit[i]=trap_S_RegisterSound(va("sound/weapons/impact/bullethit_foliage%i.wav",i+1),qfalse);
            s.media.metalHit[i]=trap_S_RegisterSound(va("sound/weapons/impact/metal%i.wav",i+1),qfalse);
            s.media.woodHit[i]=trap_S_RegisterSound(va("sound/weapons/impact/wood%i.wav",i+1),qfalse);
            s.media.glassHit[i]=trap_S_RegisterSound(va("sound/weapons/impact/glass%i.wav",i+1),qfalse);
            s.media.stoneHit[i]=trap_S_RegisterSound(va("sound/weapons/impact/stone%i.wav",i+1),qfalse);
        }
    }
    s.media.knife=cgs.media.tceKnifeMark;
    for(i=0;i<4;++i)s.media.knifeFlesh[i]=trap_S_RegisterSound(va("sound/weapons/knife/knife_hit%i.wav",i+1),qfalse);
    s.media.knifeWall=trap_S_RegisterSound("sound/weapons/knife/knife_hitwall1.wav",qfalse);
    s.media.explosion=trap_S_RegisterSound("sound/weapons/rocket/rocket_expl.wav",qfalse);
    s.media.explosionFar=trap_S_RegisterSound("sound/weapons/rocket/rocket_expl_far.wav",qfalse);
    s.media.explosionWater=trap_S_RegisterSound("sound/weapons/grenade/gren_expl_water.wav",qfalse);
    s.media.grenade=trap_S_RegisterSound("sound/weapons/grenade/mk3a2_boost.wav",qfalse);
    s.media.grenadeFar=trap_S_RegisterSound("sound/weapons/grenade/gren_expl_far.wav",qfalse);
    s.media.artillery=trap_S_RegisterSound("sound/weapons/dynamite/dynamite_expl.wav",qfalse);
    s.media.artilleryFar=trap_S_RegisterSound("sound/weapons/dynamite/dynamite_expl_far.wav",qfalse);
    /* CG_RegisterSounds30048e80 explicitly clears rocket[3]/mortar[3], their
     * far handles, dynamite and satchel handles. Preserve zero, not SDK sounds. */
    TCE_CG_MissileHitWall(weapon,effect,origin,normal,direction,flags,forceEffect,materialIsType,&s);
}

void TCE_CG_GrenadeImpact(int weapon,int effect,vec3_t origin,vec3_t normal,int flags) {
    CG_TCEMissileHitWall(weapon,effect,origin,normal,normal,(unsigned)flags,0,0);
}
/* Preserve the original eleven-argument renderer contract, including supplied
 * muzzle origin, transmitted energy and predicted-shot suppression. */
void CG_TCEBullet(vec3_t end,int source,vec3_t normal,int flesh,int victim,int other,
    float water,int seed,int damage,vec3_t start,int predicted) {
    static int lastBloodSpat;
    tce_bulletRendererContext_t c;
    int i;
    memset(&c,0,sizeof(c));c.wall=CG_TCEMissileHitWall;c.particle=particle;
    c.mark=mark;c.soundVolume=volume;c.attenuation=tceFlash.deafness;
    c.portal=cg.tcePortalScopeRendering;c.lastBloodSpat=&lastBloodSpat;
    /* Original CG_RegisterSounds30048e80 uses TC flesh, not SDK fleshN.wav. */
    for(i=0;i<5;++i) {
        cgs.media.sfx_bullet_fleshhit[i]=trap_S_RegisterSound(va("sound/weapons/impact/bullethit_flesh%i.wav",i+1),qfalse);
        cgs.media.sfx_bullet_waterhit[i]=trap_S_RegisterSound(va("sound/weapons/impact/water%i.wav",i+1),qfalse);
    }
    TCE_CG_Bullet(end,source,normal,flesh,victim,other,water,seed,damage,start,predicted,&c);
}
static void predictedWall(int weapon,int effect,vec3_t origin,vec3_t normal,vec3_t direction,int flags,int force,int material) {
    CG_TCEMissileHitWall(weapon,effect,origin,normal,direction,(unsigned)flags,force,material);
}
void CG_TCEShotgunFire(entityState_t *event) {
    tce_shotgunPrediction_t c;
    memset(&c,0,sizeof(c));c.localClient=cg.snap->ps.clientNum;
    c.maxClients=cgs.maxclients;c.predictBullets=cg_predictBullets.integer;
    c.weapons=weaponDef;c.trace=CG_Trace;c.bullet=CG_TCEBullet;c.wall=predictedWall;
    TCE_CG_ShotgunFire(&c,event);
}

/* CG_PredictedFire30076160 / Linux000c1d66. This is the real local producer,
 * distinct from server Bullet_Endpos: scoped cutoff and snapshot sources differ. */
void CG_PredictedFire(centity_t *cent) {
    playerState_t *ps=&cg.predictedPlayerState;
    int weapon=cent->currentState.weapon,seed=ps->stats[STAT_TCE_SHOT_SEED];
    int i,distance=8192,scatter=1;
    float movement,shot,spread,minimum,radius,horizontal,swayRight=0,swayUp=0,lean;
    double angle,vertical;
    vec3_t start,end,forward,right,up,angles;
    tce_shotgunPrediction_t c;
    const tce_weaponDef_t *def=&weaponDef[weapon];
    if(!(BG_CheckUTWeapon(weapon)||weapon==37||weapon==38)||weapon==4||weapon==9||weapon==30||weapon==15||weapon==1)return;
    VectorCopy(ps->origin,start);start[2]+=ps->viewheight;
    if(ps->leanf!=0) {
        lean=ps->leanf;
        if(cgs.tceLeanMode>0)lean/=lean<0?3.3f:1.8f;
        AngleVectors(cg.refdefViewAngles,NULL,right,NULL);
        VectorMA(start,lean,right,start);
    }
    start[2]-=8;
    AngleVectors(cg.refdefViewAngles,forward,NULL,up);
    for(i=0;i<3;++i)start[i]=up[i]*8+forward[i]*4+start[i];
    shot=movement=0;
    if(!(ps->stats[STAT_TCE_WEAPON_FLAGS]&4)) {
        shot=(float)def->unknown_0f8[0]*.001f;
        movement=(float)def->unknown_0f8[3]*.001f;
    }
    spread=((float)ps->holdable[1]*shot+(float)ps->holdable[0]*movement)*.7f;
    minimum=(float)def->unknown_0f8[2]*.08192f;
    if(minimum>0) {
        if(ps->eFlags&EF_PRONE)minimum*=.33333f;
        else if(ps->pm_flags&PMF_DUCKED)minimum*=.66667f;
        if(spread<minimum)spread=minimum;
    }
    if(def->scoped>1) {
        if(!(cg.snap->ps.stats[STAT_TCE_WEAPON_FLAGS]&4))spread=1200;
        else {distance=16384;scatter=0;}
    }
    VectorCopy(ps->viewangles,angles);
    if(!weaponDef[ps->weapon].noTacMode&&(ps->stats[STAT_TCE_WEAPON_FLAGS]&4)) {
        angles[0]+=((float)ps->holdable[5]-2000)*.01f;
        angles[1]+=((float)ps->holdable[6]-2000)*.01f;
    }
    AngleVectors(angles,forward,right,up);
    for(i=0;i<3;++i)end[i]=(float)distance*forward[i]+start[i];
    if(!(ps->stats[STAT_TCE_WEAPON_FLAGS]&4)) {
        angle=ps->stats[STAT_TCE_AIM_PHASE]*(double).0062831854447722435f;
        swayRight=(float)(sin(angle)*600.0);swayUp=(float)(cos(angle)*600.0);
    }
    if(scatter) {
        radius=(float)sqrt(Q_random(&seed));angle=Q_crandom(&seed)*3.141;
        horizontal=(float)(cos(angle)*radius*spread*(double)1.27f+swayRight);
        vertical=sin(angle)*radius*spread*(double)1.27f+swayUp;
        end[0]=(float)(vertical*up[0]+(double)horizontal*right[0]+end[0]);
        end[1]=(float)(vertical*up[1]+(float)(horizontal*right[1]+end[1]));
        end[2]=(float)(vertical*up[2]+(float)(horizontal*right[2]+end[2]));
    }
    memset(&c,0,sizeof(c));c.localClient=cg.snap->ps.clientNum;
    c.maxClients=cgs.maxclients;c.predictBullets=cg_predictBullets.integer;
    c.weapons=weaponDef;c.trace=CG_Trace;c.bullet=CG_TCEBullet;c.wall=predictedWall;
    if(def->pelletCount) {
        VectorSubtract(end,start,end);
        TCE_CG_ShotgunPattern(&c,start,end,seed&255,cent->currentState.clientNum,1);
    } else TCE_CG_BulletFireExtended(&c,cent->currentState.clientNum,weapon,start,end,def->unknown_134,4,seed);
}
