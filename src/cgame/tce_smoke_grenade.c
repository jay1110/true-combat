#include "cg_local.h"
#include "tce_smoke_grenade.h"

static void SmokeLight(const tce_lightGrid_t *grid, const vec3_t point,
    const vec3_t cached, vec3_t color, vec3_t sum, int *count) {
    int k;
    TCE_CG_LightForParticleSimple(grid,point,color);
    if (!(trap_CM_PointContents(point,0)&CONTENTS_SOLID)) {
        ++*count;
        for(k=0;k<3;++k) {
            sum[k]+=color[k];
            color[k]=(float)(((double)color[k]+cached[k])*0.5);
        }
    } else VectorCopy(cached,color);
}
static void SmokeRGBA(refEntity_t *ent,const vec3_t color,float alpha) {
    int k;
    for(k=0;k<3;++k)ent->shaderRGBA[k]=(byte)(int)((double)color[k]*204.0);
    ent->shaderRGBA[3]=(byte)(int)alpha;
}
static float SmokeRotation(int *seed,float growth,float speed) {
    float spin=(float)((double)Q_crandom(seed)*growth*speed);
    return (float)((double)Q_crandom(seed)*180.0+spin);
}
static float SmokeAlpha(float age) {
    double a=(7.0-age)*0.5;
    if(a>1.0)a=1.0;
    return (float)(a*255.0);
}

/* Windows 300310a0. The explicit state/context replaces original cg_t and
 * centity_t offsets; random seeds are per sprite, independent of frame rate. */
void TCE_CG_SmokeGrenadeExtended(const tce_lightGrid_t *grid,int now,int start,
    qboolean large,const vec3_t origin,const vec3_t forward,vec3_t cachedColor,
    const qhandle_t shaders[3]) {
    static const vec3_t lobes[4]={{0,0,.5f},{-.866f,.5f,.2f},{.866f,.5f,.2f},{0,-1,.2f}};
    static const vec3_t outer[10]={{1,0,.2f},{.5f,.866f,.2f},{-.5f,.866f,.2f},
        {-1,0,.2f},{-.5f,-.866f,.2f},{.5f,-.866f,.2f},{.866f,.5f,1.4f},
        {-.866f,.5f,1.4f},{0,-1,1.4f},{0,0,1}};
    float scale=large?137.5f:110.0f;
    int elapsed=now-start,i,j,k,axis,seed,delay,count=0;
    vec3_t direction,center,color,sum={0,0,0};
    float spread=(float)(sqrt((double)elapsed*0.0002f)*scale*0.5);
    float age,growth,alpha,fraction,distance,radius;
    refEntity_t ent;
    for(i=0;i<4;++i) {
        VectorCopy(lobes[i],direction);
        if(i)VectorNormalize(direction);
        for(k=0;k<3;++k)center[k]=(float)((double)spread*direction[k]+origin[k]);
        SmokeLight(grid,center,cachedColor,color,sum,&count);
        for(j=0;j<2;++j) {
            seed=(int)((unsigned)start+i*327u+j*754u);
            delay=(int)((double)Q_random(&seed)*500.0);
            if(elapsed<=delay || elapsed>=delay+35000)continue;
            age=(float)((double)(elapsed-delay)*0.0002f);
            alpha=SmokeAlpha(age);growth=(float)sqrt(age);
            memset(&ent,0,sizeof(ent));ent.reType=RT_SPRITE;ent.customShader=shaders[0];
            if(!j) {
                ent.radius=(float)((double)growth*scale*0.5);
                ent.rotation=SmokeRotation(&seed,growth,150);
                VectorCopy(center,ent.origin);SmokeRGBA(&ent,color,alpha);
                trap_R_AddRefEntityToScene(&ent);
            } else {
                fraction=j*.5f-i*.125f;
                distance=(float)((double)fraction*growth*scale*.85f);
                radius=(float)((double)growth*scale*sqrt(1.0-(double)fraction*fraction)*.5);
                for(k=0;k<2;++k) {
                    memset(&ent,0,sizeof(ent));ent.reType=RT_SPRITE;ent.customShader=shaders[0];ent.radius=radius;
                    ent.rotation=SmokeRotation(&seed,growth,150);
                    for(axis=0;axis<3;++axis)ent.origin[axis]=(float)((double)(k?-distance:distance)*forward[axis]+center[axis]);
                    SmokeRGBA(&ent,color,alpha);trap_R_AddRefEntityToScene(&ent);
                }
            }
        }
    }
    for(i=0;i<10;++i) {
        VectorCopy(outer[i],direction);VectorNormalize(direction);
        seed=(int)((unsigned)start+i*127u);
        delay=(int)((double)Q_random(&seed)*500.0);
        if(elapsed<=delay || elapsed>=delay+35000)continue;
        age=(float)((double)(elapsed-delay)*0.0002f);
        alpha=SmokeAlpha(age);growth=(float)sqrt((double)(elapsed-delay)*0.0002f);
        distance=(float)((double)growth*scale*.8f);
        memset(&ent,0,sizeof(ent));ent.reType=RT_SPRITE;
        ent.customShader=shaders[i<6?0:1];
        ent.radius=(float)((double)growth*scale*.4f);
        if(i<6)ent.rotation=SmokeRotation(&seed,growth,300);
        else {
            if(Q_random(&seed)>.5f)ent.customShader=shaders[2];
            distance*=.7f;
        }
        ent.radius=(float)(((double)Q_random(&seed)+1.0)*ent.radius*(2.0f/3.0f));
        for(k=0;k<3;++k)ent.origin[k]=(float)((double)distance*direction[k]+origin[k]);
        SmokeLight(grid,ent.origin,cachedColor,color,sum,&count);
        SmokeRGBA(&ent,color,alpha);trap_R_AddRefEntityToScene(&ent);
    }
    if(count)for(k=0;k<3;++k) {
        double weighted=(double).1f/count*sum[k];
        if(k<2)weighted=(float)weighted;
        cachedColor[k]=(float)((double)cachedColor[k]*.9f+weighted);
    }
}
