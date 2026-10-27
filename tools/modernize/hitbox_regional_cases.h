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
static void movingLegDoorAndAntiLagBits(void)
{
    int mask, cover, latency, yaw, lean;
    /* The actual public Bullet_Fire wrapper decides rewind. Debug bits 2/4
     * alone must not rewind. A leg crosses a narrow doorway between markers;
     * solid and missileclip jambs remain authoritative on every path. */
    for(mask=0;mask<8;++mask)for(cover=0;cover<3;++cover)
    for(latency=50;latency<=150;latency+=50)for(yaw=0;yaw<=90;yaw+=90)for(lean=-20;lean<=20;lean+=40) {
        gentity_t *target,*wall;gclient_t *client;int i;
        resetWorld();actor(1,100,8,0,yaw);target=&g_entities[1];client=target->client;
        g_antilag.integer=mask;g_entities[0].client->pers.cmd.serverTime=level.time-latency;
        client->ps.leanf=(float)lean;
        VectorSet(target->r.currentOrigin,100,100,0);VectorCopy(target->r.currentOrigin,client->ps.origin);
        client->topMarker=1;G_CaptureClientMarker(target,&client->clientMarkers[1],level.time);
        G_CaptureClientMarker(target,&client->clientMarkers[0],level.time-latency*2);
        client->clientMarkers[0].origin[1]=0;
        for(i=2;i<MAX_CLIENT_MARKERS;++i)client->clientMarkers[i]=client->clientMarkers[0];
        VectorSet(muzzleTrace,0,50,-10);VectorSet(fireTestEnd,300,50,-10);
        weaponDef[8].unknown_12c=20;
        /* Jamb outside the open interval [40,60]. Then move that same
         * collider across the ray to prove SOLID/MISSILECLIP rejection. */
        wall=&g_entities[100];wall->inuse=wall->r.linked=qtrue;
        wall->r.contents=cover==2?CONTENTS_MISSILECLIP:CONTENTS_SOLID;
        VectorSet(wall->r.currentOrigin,50,cover?50:20,0);
        VectorSet(wall->r.mins,-2,-15,-24);VectorSet(wall->r.maxs,2,15,70);
        Bullet_Fire(&g_entities[0],0,999,qfalse);
        CHECK(hitCount==((mask&1)&&!cover?1:0));
        CHECK(((mask&1)&&!cover)?client->tceLastAppliedDamage==13:target->health==100);
        CHECK(seenShotTime==(uint32_t)((mask&1)?level.time-latency:level.time));
        CHECK(target->r.currentOrigin[1]==100&&!client->tceRewind.active&&!g_entities[0].client->tceRewind.shotQueryActive);
    }
    printf("moving legs: 288 actual Bullet_Fire cases, 50/100/150ms, yaw0/90, lean, open/solid/missileclip doorway and all anti-lag bitmasks\n");
}
static void regionalDebugMasks(void)
{
    static const int masks[]={-4,-3,-2,-1,0,2,3,4,8,12};
    vec3_t point={80,0,42},dir={1,0,0};unsigned int i;
    for(i=0;i<sizeof(masks)/sizeof(masks[0]);++i) {
        resetWorld();actor(1,100,8,0,0);g_debugBullets.integer=masks[i];
        G_Damage(&g_entities[1],&g_entities[0],&g_entities[0],dir,point,20,0,MOD_MP40);
        CHECK(g_entities[1].health<100);
        CHECK((masks[i]>0&&(masks[i]&2))?eventCount>0:eventCount==0);
    }
    g_debugBullets.integer=0;
}
static void railhouseLiveLegEmitterSnap(void)
{
    /* Real opt-in server logs, obj_railhouse Small bridge doorway, frames
     * 500900/500950/501050. These primary leg contacts preceded the far wall,
     * but EmitterCheck snapped the later damage probe inside the leg box. */
    static const float starts[3][3]={
        {-928.475159f,1811.02429f,225.656784f},
        {-928.473267f,1811.04419f,225.670181f},
        {-928.473572f,1811.07568f,225.691132f}};
    static const float ends[3][3]={
        {-2129.39624f,-6196.9668f,-1015.4931f},
        {-2124.63062f,-6200.81934f,-992.914551f},
        {-2132.71509f,-6205.04834f,-956.760925f}};
    int ray,cover;
    for(ray=0;ray<3;++ray)for(cover=0;cover<3;++cover) {
        gentity_t *target,*wall;vec3_t start,end;int expected=cover?0:13;
        resetWorld();actor(1,100,49,0,79);target=&g_entities[1];
        VectorSet(target->r.currentOrigin,-996.125f,1320.1270751953125f,174.12815856933594f);
        VectorCopy(target->r.currentOrigin,target->client->ps.origin);
        target->client->ps.stats[STAT_TCE_FLAGS]|=0x200;target->client->ps.stats[STAT_TCE_WEAPON_FLAGS]|=0x20;
        VectorSet(target->client->ps.viewangles,0.67565918f,79.4366455f,0);
        wall=&g_entities[100];wall->inuse=wall->r.linked=qtrue;wall->r.contents=cover==2?CONTENTS_MISSILECLIP:CONTENTS_SOLID;
        VectorClear(wall->r.currentOrigin);VectorSet(wall->r.mins,-1100,cover?1360:1300,100);VectorSet(wall->r.maxs,-900,cover?1364:1304.125f,260);
        VectorCopy(starts[ray],start);VectorCopy(ends[ray],end);
        if(!cover) {
            tceBulletTraceContext_t context;trace_t trace;vec3_t raw,direction;
            tceDamagePose_t pose;tceDamageVolume_t volumes[3];
            memset(&context,0,sizeof(context));context.shotTime=context.receiptCeiling=(uint32_t)level.time;
            G_TCETraceBullet(&trace,&g_entities[0],start,end,&context);CHECK(trace.entityNum==1);
            VectorCopy(trace.endpos,raw);EmitterCheck(target,&g_entities[0],&trace);
            TCE_ClientDamagePose(target,&pose);TCE_DamageVolumes(&pose,volumes);
            CHECK(raw[1]>volumes[2].origin[1]+volumes[2].maxs[1]);
            CHECK(trace.endpos[0]==-999&&trace.endpos[1]==1335&&trace.endpos[2]==151+ray*2);
            CHECK(trace.endpos[1]<volumes[2].origin[1]+volumes[2].maxs[1]);
            VectorSubtract(end,start,direction);VectorNormalize(direction);
            G_Damage(target,&g_entities[0],&g_entities[0],direction,raw,26,0,MOD_MP40);
            CHECK(target->client->tceLastAppliedDamage==13&&target->client->ps.holdable[4]==13&&target->health<100);
            target->health=100;target->client->ps.stats[STAT_HEALTH]=100;target->client->ps.holdable[4]=0;hitCount=0;
            CHECK(G_Damage(target,&g_entities[0],&g_entities[0],direction,trace.endpos,26,0,MOD_MP40)==1);
            CHECK(target->health==100&&target->client->tceLastAppliedDamage==0);
        }
        Bullet_Fire_Extended(&g_entities[0],&g_entities[0],start,end,0,20,1000,0,0,0,1,8,42,1);
        CHECK(target->client->tceLastAppliedDamage==expected);CHECK(cover?target->health==100:target->health<100);
        CHECK(target->client->ps.holdable[4]==expected&&target->client->ps.holdable[2]==0&&target->client->ps.holdable[3]==0);
        CHECK(hitCount==(cover?0:1));
    }
    printf("Railhouse live rays: 3 exact leg contacts with actual EmitterCheck, far-wall and front SOLID/MISSILECLIP controls\n");
}
