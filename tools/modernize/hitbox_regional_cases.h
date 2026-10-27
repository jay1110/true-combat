/* Actual full bullet/damage region contracts. Engine traces use the analytic
 * AABB service in the owning fixture; this is not live BSP/render evidence. */
#include <limits.h>
static void regionalSetup(int stance,int yaw)
{
    resetWorld();actor(1,137.375f,8,stance==2,yaw);
    VectorSet(g_entities[1].r.currentOrigin,137.375f,-89.625f,17.875f);
    VectorCopy(g_entities[1].r.currentOrigin,g_entities[1].client->ps.origin);
    if(stance==1)g_entities[1].client->ps.pm_flags|=PMF_DUCKED;
}
static void regionalShoot(const vec3_t start,const vec3_t end)
{
    hitCount=0;
    Bullet_Fire_Extended(&g_entities[0],&g_entities[0],(float*)start,(float*)end,
        0,20,1000,0,0,0,1,8,42,1);
}
static void regionalRay(int stance,int yaw,int part,float y,float z,vec3_t start,vec3_t end)
{
    tceDamagePose_t pose;tceDamageVolume_t volumes[3];vec3_t center;int axis;
    regionalSetup(stance,yaw);TCE_ClientDamagePose(&g_entities[1],&pose);TCE_DamageVolumes(&pose,volumes);
    for(axis=0;axis<3;++axis)center[axis]=volumes[part].origin[axis]+.5f*(volumes[part].mins[axis]+volumes[part].maxs[axis]);
    center[1]+=.5f*y*(volumes[part].maxs[1]-volumes[part].mins[1]);
    center[2]+=.5f*z*(volumes[part].maxs[2]-volumes[part].mins[2]);
    VectorCopy(center,start);VectorCopy(center,end);start[0]-=180;end[0]+=180;
}
static void regionalGrid(void)
{
    static const float offsets[]={-.98f,-.6f,0,.6f,.98f};
    int stance,yaw,part,y,z,rays=0;
    for(stance=0;stance<3;++stance)for(yaw=0;yaw<3;++yaw)for(part=0;part<3;++part)
    for(y=0;y<5;++y)for(z=0;z<5;++z)
    {
        tceDamagePose_t pose;tceDamageVolume_t volumes[3];vec3_t start,end;
        float fractions[3]={1,1,1};int i,region=-1,amount,shots;
        regionalRay(stance,yaw*67,part,offsets[y],offsets[z],start,end);
        TCE_ClientDamagePose(&g_entities[1],&pose);TCE_DamageVolumes(&pose,volumes);
        for(i=0;i<3;++i)if(TCE_DamageVolumeFraction(&volumes[i],start,end,&fractions[i])&&
            (region<0||fractions[i]<=fractions[region]))region=i;
        CHECK(region>=0);if(region<0)continue;
        amount=region==0?52:region==1?26:13;
        regionalShoot(start,end);++rays;
        CHECK(g_entities[1].health<100&&g_entities[1].client->tceLastAppliedDamage==amount);
        for(i=0;i<3;++i)CHECK(g_entities[1].client->ps.holdable[i+2]==(i==region?amount:0));
        for(shots=1;g_entities[1].health>0&&shots<8;++shots)regionalShoot(start,end);
        CHECK(g_entities[1].health<=0);CHECK(shots==(region==0?2:region==1?4:8));
    }
    printf("regional grid: %d rays, standing/crouched/prone, centers/feet/upper torso/edges; nominal contact and finite health\n",rays);
}
static void regionalCoverAndCompatibility(void)
{
    int stance,part,kind;vec3_t start,end,direction={1,0,0},point={100,0,0};
    for(stance=0;stance<3;++stance)for(part=0;part<3;++part)for(kind=0;kind<3;++kind)
    {
        gentity_t *wall;
        regionalRay(stance,0,part,0,0,start,end);
        if(kind==2){start[1]+=80;end[1]+=80;}
        else {
            wall=&g_entities[100];wall->inuse=wall->r.linked=qtrue;
            wall->r.contents=kind?CONTENTS_MISSILECLIP:CONTENTS_SOLID;
            VectorCopy(start,wall->r.currentOrigin);wall->r.currentOrigin[0]+=60;
            VectorSet(wall->r.mins,-1,-20,-20);VectorSet(wall->r.maxs,1,20,20);
        }
        regionalShoot(start,end);CHECK(g_entities[1].health==100&&hitCount==0);
        CHECK(!g_entities[1].client->ps.holdable[2]&&!g_entities[1].client->ps.holdable[3]&&!g_entities[1].client->ps.holdable[4]);
    }
    /* A historical zero-weight foot contact stays unchanged for radius input. */
    regionalRay(0,0,2,0,-.98f,start,end);VectorCopy(start,point);
    {
        tceDamagePose_t pose;tceDamageVolume_t volumes[3];vec3_t probeEnd;float fraction;
        TCE_ClientDamagePose(&g_entities[1],&pose);TCE_DamageVolumes(&pose,volumes);
        point[0]=volumes[2].origin[0]+volumes[2].mins[0]-2;
        VectorMA(point,64,direction,probeEnd);
        CHECK(TCE_DamageVolumeFraction(&volumes[2],point,probeEnd,&fraction)&&fraction>0&&fraction<1);
    }
    G_Damage(&g_entities[1],&g_entities[0],&g_entities[0],direction,point,26,DAMAGE_RADIUS,MOD_MP40);
    CHECK(g_entities[1].health==100&&g_entities[1].client->tceLastAppliedDamage==0);
    regionalSetup(0,0);G_Damage(&g_entities[1],&g_entities[0],&g_entities[0],direction,point,26,0,MOD_UNKNOWN);
    CHECK(g_entities[1].client->ps.holdable[2]==0&&g_entities[1].client->ps.holdable[3]==13&&g_entities[1].client->ps.holdable[4]==26);
    CHECK(g_entities[1].client->tceLastAppliedDamage==26&&g_entities[1].health<100);
}
static void regionalIntegerAndFractionBoundaries(void)
{
#ifdef HAS_BODY_REGION_POLICY
    static const int incoming[]={1,3,INT_MAX};
    int skin,region,n,i,expected;vec3_t start={0,0,42},end={300,0,42},direction={1,0,0},point={70,0,42};
    for(skin=0;skin<2;++skin)for(region=0;region<3;++region)for(n=0;n<3;++n)
    {
        tceBulletTraceContext_t context;trace_t trace;long long nominal;
        resetWorld();actor(1,100,8,0,0);nominal=region==0?(long long)incoming[n]*2:region==1?incoming[n]:incoming[n]/2;
        expected=nominal>INT_MAX?INT_MAX:nominal<1?1:(int)nominal;
        if(skin) {
            presentationPolicy[1]=G_TCE_PRESENTATION_AFFECTED;presentationRegion[1]=region;presentationFraction[1]=100;
            memset(&context,0,sizeof(context));context.shotTime=context.receiptCeiling=(uint32_t)level.time;
            G_TCETraceBullet(&trace,&g_entities[0],start,end,&context);CHECK(context.hasSelection);
            G_TCEPresentationDamage(&g_entities[1],&g_entities[0],&g_entities[0],direction,point,incoming[n],0,MOD_MP40,&context.selection);
        } else {
            regionFractionOverride=1;for(i=0;i<3;++i)regionProbeFractions[i]=i==region?.25f:1;
            G_Damage(&g_entities[1],&g_entities[0],&g_entities[0],direction,point,incoming[n],0,MOD_MP40);
        }
        CHECK(g_entities[1].client->tceLastAppliedDamage==expected&&g_entities[1].health<100);
        for(i=0;i<3;++i)CHECK(g_entities[1].client->ps.holdable[i+2]==(i==region?expected:0));
    }
    for(n=0;n<5;++n)
    {
        resetWorld();actor(1,100,8,0,0);regionFractionOverride=1;
        regionProbeFractions[0]=.25f;regionProbeFractions[1]=n==0?NAN:.25f;regionProbeFractions[2]=n<2?1:.25f;
        if(n==3)for(i=0;i<3;++i)regionProbeFractions[i]=NAN;
        if(n==4)for(i=0;i<3;++i)regionProbeFractions[i]=i==1?INFINITY:-.5f;
        G_Damage(&g_entities[1],&g_entities[0],&g_entities[0],direction,point,20,0,MOD_MP40);
        expected=n==0?40:n==1?20:n==2?10:0;
        CHECK(g_entities[1].client->tceLastAppliedDamage==expected);
        CHECK(g_entities[0].client->sess.aWeaponStats[G_weapStatIndex_MOD(MOD_MP40)].headshots==(unsigned)(n==0));
        if(n>=3)CHECK(g_entities[1].health==100);
    }
#endif
}
