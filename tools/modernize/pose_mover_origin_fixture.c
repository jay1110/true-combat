/* Actual legacy CGame mover/position functions, with deterministic geometry.
 * This is a design-stage defect reproducer; it does not modify production. */
#include "src/cgame/cg_local.h"
#include "src/game/tce_trajectory.h"
#include "src/game/tce_numeric.h"
#include "src/game/tce_rotate_precision.h"
#include "src/game/bg_tce_pose_origin.h"
#include "compat/compat_pose_origin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

cg_t cg;
cgs_t cgs;
centity_t cg_entities[MAX_GENTITIES];
vec3_t vec3_origin;
int numSplinePaths;
splinePath_t splinePaths[MAX_SPLINE_PATHS];
static volatile float tceSinRoll,tceCosRoll,tceSinPitch,tceCosYaw,tceCosPitch,tceSinYaw;
static unsigned checks,failures,demonstrated;
static int requireCanonical;
static int verifyFixed;
int FixturePushedPosition(const vec3_t target,const vec3_t pivot,const vec3_t move,const vec3_t angles,vec3_t out);
void QDECL Com_Error(int code,const char *format,...) {(void)code;(void)format;abort();}
static void CG_EntityEffects(centity_t *cent) {(void)cent;abort();}
static void CG_ProcessEntity(centity_t *cent) {(void)cent;abort();}
void BG_EvaluateTrajectory(const trajectory_t *tr,int time,vec3_t out,qboolean angle,int path)
{TCE_BG_EvaluateTrajectory(tr,time,out,angle,path,1);}

/* ACTUAL_FUNCTIONS */

static void check(int condition,const char *label)
{
    ++checks;
    if(!condition) {++failures;printf("FAIL %s\n",label);}
}
static int nearv(const vec3_t a,const vec3_t b)
{return fabs(a[0]-b[0])<.0002&&fabs(a[1]-b[1])<.0002&&fabs(a[2]-b[2])<.0002;}
static void reset(void)
{
    memset(&cg,0,sizeof(cg));memset(cg_entities,0,sizeof(cg_entities));
    memset(splinePaths,0,sizeof(splinePaths));numSplinePaths=0;
    cg.snap=&cg.activeSnapshots[0];cg.nextSnap=NULL;cg.time=1000;
    cg.snap->serverTime=0;
}
static centity_t *target(int interpolate)
{
    centity_t *out=&cg_entities[1];
    out->currentState.number=1;out->currentState.eType=ET_PLAYER;
    out->currentState.groundEntityNum=200;
    out->currentState.pos.trType=TR_INTERPOLATE;
    out->currentState.pos.trBase[0]=10;
    out->interpolate=(qboolean)interpolate;
    return out;
}
static void observers(centity_t *player,const vec3_t a,const vec3_t b,vec3_t first,vec3_t second)
{
    VectorCopy(a,cg.snap->ps.origin);CG_CalcEntityLerpPositions(player);VectorCopy(player->lerpOrigin,first);
    VectorCopy(b,cg.snap->ps.origin);CG_CalcEntityLerpPositions(player);VectorCopy(player->lerpOrigin,second);
}
static void observed(const char *name,const vec3_t a,const vec3_t b,const vec3_t reference,int defect)
{
    printf("%s viewerA=(%.9g %.9g %.9g) viewerB=(%.9g %.9g %.9g) targetReference=(%.9g %.9g %.9g)\n",
        name,a[0],a[1],a[2],b[0],b[1],b[2],reference[0],reference[1],reference[2]);
    if(defect)++demonstrated;
}
static void make_path(void)
{
    int i;numSplinePaths=1;
    for(i=0;i<MAX_SPLINE_SEGMENTS;++i) {
        splineSegment_t *segment=&splinePaths[0].segments[i];
        double a=i*(M_PI*.5/MAX_SPLINE_SEGMENTS),b=(i+1)*(M_PI*.5/MAX_SPLINE_SEGMENTS);
        vec3_t delta;segment->start[0]=(float)(100*cos(a));segment->start[1]=(float)(100*sin(a));
        delta[0]=(float)(100*cos(b))-segment->start[0];delta[1]=(float)(100*sin(b))-segment->start[1];delta[2]=0;
        segment->length=VectorNormalize2(delta,segment->v_norm);splinePaths[0].length+=segment->length;
    }
}
static int read_mover(void *user,int number,tcePoseMoverState_t *out) {
    (void)user;
    if(number<0||number>=ENTITYNUM_MAX_NORMAL||!cg_entities[number].currentValid)return 0;
    BG_TCEPoseMoverState(&cg_entities[number].currentState,out);return 1;
}
static void fixed_controls(void) {
    tcePoseOriginState_t older,newer;vec3_t actual,expected,angles,move,pivot,held;
    entityState_t state;int i,j;uint32_t random=0x12345678u,digest=2166136261u,maximumUlp=0;
    reset();memset(&state,0,sizeof(state));state.number=1;state.pos.trType=TR_INTERPOLATE;
    state.groundEntityNum=200;VectorSet(state.pos.trBase,10,0,0);BG_TCEPoseOriginState(&state,&older);
    cg_entities[200].currentValid=qtrue;cg_entities[200].currentState.number=200;
    cg_entities[200].currentState.eType=ET_MOVER;
    for(i=0;i<1000;++i) {
        entityState_t *mover=&cg_entities[200].currentState;
        mover->pos.trType=mover->apos.trType=TR_LINEAR;
        for(j=0;j<3;++j) {
            random=random*1664525u+1013904223u;move[j]=(int)(random%20001u)*.01f-100;
            random=random*1664525u+1013904223u;angles[j]=(int)(random%36001u)*.01f-180;
            random=random*1664525u+1013904223u;older.position.trBase[j]=(int)(random%200001u)*.01f-1000;
            random=random*1664525u+1013904223u;mover->pos.trBase[j]=(int)(random%20001u)*.01f-100;
            mover->pos.trDelta[j]=move[j];mover->apos.trDelta[j]=angles[j];
        }
        BG_EvaluateTrajectory(&mover->pos,1000,pivot,qfalse,0);
        for(j=0;j<3;++j)move[j]=pivot[j]-mover->pos.trBase[j];
        check(FixturePushedPosition(older.position.trBase,pivot,move,angles,expected),"complete actual G_TryPushingEntity succeeds in empty world");
        check(BG_TCEPosePresentationOrigin(&older,NULL,0,0,1000,read_mover,NULL,actual),"affected origin evaluates combined translation/rotation");
        check(!memcmp(actual,expected,sizeof(actual)),"affected origin bit-matches actual G_TryPushingEntity new-pivot and store order");
        if(i<16) {
            VectorSet(cg.snap->ps.origin,99999,-55555,1111);
            check(BG_TCEPosePresentationOrigin(&older,NULL,0,0,1000,read_mover,NULL,held)&&!memcmp(actual,held,sizeof(actual)),
                "unrelated observer position cannot change canonical target origin");
        }
    }
    /* Interpolation must ignore mover deltas, including a missing newer mover.
     * Its old snapshot input remains immutable; no viewer global is consumed. */
    older.position.trBase[0]=10;older.position.trBase[1]=older.position.trBase[2]=0;
    newer=older;newer.position.trBase[0]=110;
    check(BG_TCEPosePresentationOrigin(&older,&newer,0,1000,500,NULL,NULL,actual)&&actual[0]==60,
        "eligible player interpolation applies no additional mover adjustment");
    VectorCopy(older.position.trBase,held);
    check(BG_TCEPosePresentationOrigin(&older,&newer,0,1000,500,NULL,NULL,older.position.trBase)&&older.position.trBase[0]==60,
        "origin output may alias older input");
    VectorCopy(held,older.position.trBase);
    /* Entity chains longer than a convenient arbitrary64 are valid. */
    for(i=200;i<350;++i) {
        entityState_t *mover=&cg_entities[i].currentState;memset(mover,0,sizeof(*mover));
        cg_entities[i].currentValid=qtrue;mover->number=i;mover->eType=ET_MOVER;
        if(i<349) {mover->eFlags=EF_PATH_LINK;mover->torsoAnim=i+1;}
    }
    VectorSet(cg_entities[200].currentState.pos.trBase,12,34,56);
    check(BG_TCEPoseMoverEvaluate(200,1000,read_mover,NULL,actual,angles)&&actual[0]==12&&actual[1]==34,
        "150 valid linked entities are resolved without arbitrary depth64 rejection");
    cg_entities[349].currentState.eFlags=EF_PATH_LINK;cg_entities[349].currentState.torsoAnim=200;
    check(!BG_TCEPoseMoverEvaluate(200,1000,read_mover,NULL,actual,angles),"cyclic entity ancestry fails finitely");
    cg_entities[349].currentState.torsoAnim=ENTITYNUM_MAX_NORMAL;
    check(!BG_TCEPoseMoverEvaluate(200,1000,read_mover,NULL,actual,angles),"world/sentinel parent index is rejected");
    reset();make_path();
    for(i=200;i<203;++i) {cg_entities[i].currentValid=qtrue;cg_entities[i].currentState.number=i;cg_entities[i].currentState.eType=ET_MOVER;}
    cg_entities[200].currentState.pos.trType=(trType_t)16;cg_entities[200].currentState.pos.trDuration=1000;
    cg_entities[201].currentState.eFlags=cg_entities[202].currentState.eFlags=EF_PATH_LINK;
    cg_entities[201].currentState.torsoAnim=200;cg_entities[202].currentState.torsoAnim=201;
    cg_entities[201].currentState.angles2[0]=-10;cg_entities[202].currentState.angles2[0]=12;
    for(i=-1;i<=1;i+=2)for(j=-200;j<=1200;j+=100) {
        cg_entities[200].currentState.effect2Time=i;cg_entities[200].currentState.apos.trBase[0]=5;
        check(CG_AddLinkedEntity(&cg_entities[202],qtrue,j),"actual linked resolver accepts signed spline and clamped endpoint");
        VectorCopy(cg_entities[202].lerpOrigin,expected);VectorCopy(cg_entities[202].lerpAngles,held);
        check(BG_TCEPoseMoverEvaluate(202,(uint32_t)j,read_mover,NULL,actual,angles)&&nearv(actual,expected)&&nearv(angles,held),
              "pure nested resolver matches complete actual CG_AddLinkedEntity positions/facing");
    }
    splinePaths[0].next=splinePaths[0].prev=&splinePaths[0];
    cg_entities[201].currentState.angles2[0]=1000000;
    check(!BG_TCEPoseMoverEvaluate(201,500,read_mover,NULL,actual,angles),"cyclic spline with no offset intersection fails finitely");
    check(TCE_PoseOriginInterpolate(1e20f,-1e20f,.5f)==0,
          "centralized interpolation preserves midpoint cancellation");
    for(i=0;i<20000;++i) {
        uint32_t a,b,bits,legacyBits,ordered,legacyOrdered,ulp;float first,last,fraction,result;
        centity_t legacy;vec3_t start,end;
        random=random*1664525u+1013904223u;a=random&0xff7fffffu;
        random=random*1664525u+1013904223u;b=random&0xff7fffffu;
        memcpy(&first,&a,sizeof(first));memcpy(&last,&b,sizeof(last));
        random=random*1664525u+1013904223u;fraction=(random&65535u)/65535.f;
        result=TCE_PoseOriginInterpolate(first,last,fraction);memcpy(&bits,&result,sizeof(bits));
        digest=(digest^bits)*16777619u;
        VectorSet(start,first,0,0);VectorSet(end,last,0,0);
        CG_InterpolateEntityOrigin(&legacy,start,end,fraction);memcpy(&legacyBits,&legacy.lerpOrigin[0],sizeof(legacyBits));
        ordered=(bits&0x80000000u)?~bits:bits|0x80000000u;
        legacyOrdered=(legacyBits&0x80000000u)?~legacyBits:legacyBits|0x80000000u;
        ulp=ordered>legacyOrdered?ordered-legacyOrdered:legacyOrdered-ordered;
        if(ulp>maximumUlp)maximumUlp=ulp;
        check(ulp<=1,"canonical binary64 interpolation remains within one ULP of actual legacy native interpolation");
    }
    printf("fixed_interpolation_digest=%08x maximum_legacy_ULP=%u\n",(unsigned)digest,(unsigned)maximumUlp);
}
int main(int argc,char **argv)
{
    vec3_t a={20,0,0},b={40,0,0},first,second,reference;
    centity_t *mover,*player;
    {int i;for(i=1;i<argc;++i) {if(!strcmp(argv[i],"--require-canonical"))requireCanonical=1;if(!strcmp(argv[i],"--verify-fixed"))verifyFixed=1;}}
    reset();mover=&cg_entities[200];mover->currentState.eType=ET_MOVER;
    mover->currentState.apos.trType=TR_LINEAR;mover->currentState.apos.trDelta[YAW]=90;
    player=target(0);observers(player,a,b,first,second);VectorSet(reference,0,10,0);
    check(!nearv(first,second),"actual no-next rotating mover depends on unrelated viewer origin");
    check(!nearv(first,reference)&&!nearv(second,reference),"neither observer yields target-relative 90-degree orbit");
    if(requireCanonical)check(nearv(first,second),"canonical no-next rotation must be observer independent");
    observed("rotating-no-next",first,second,reference,1);

    reset();mover=&cg_entities[200];mover->currentState.eType=ET_MOVER;
    mover->currentState.pos.trType=TR_LINEAR;mover->currentState.pos.trDelta[0]=100;
    mover->lerpOrigin[0]=100;
    player=target(0);observers(player,a,b,first,second);VectorSet(reference,110,0,0);
    check(nearv(first,second)&&nearv(first,reference),"ordinary translation already uses target plus mover delta");
    observed("linear-no-next",first,second,reference,0);

    reset();mover=&cg_entities[200];mover->currentState.eType=ET_MOVER;
    mover->currentState.pos.trType=TR_LINEAR;mover->currentState.pos.trDelta[0]=100;
    cg.time=500;cg.nextSnap=&cg.activeSnapshots[1];cg.nextSnap->serverTime=1000;cg.frameInterpolation=.5f;
    player=target(1);player->nextState=player->currentState;player->nextState.pos.trBase[0]=110;
    observers(player,a,b,first,second);VectorSet(reference,60,0,0);
    check(nearv(first,second)&&nearv(first,reference),"interpolated player already contains mover displacement; no extra adjustment");
    observed("linear-interpolated",first,second,reference,0);

    reset();make_path();mover=&cg_entities[200];mover->currentValid=qtrue;
    mover->currentState.eType=ET_MOVER;mover->currentState.pos.trType=(trType_t)16;
    mover->currentState.pos.trDuration=1000;mover->currentState.effect2Time=1;
    cg_entities[201].currentValid=qtrue;cg_entities[201].currentState.eType=ET_MOVER;
    cg_entities[201].currentState.eFlags=EF_PATH_LINK;cg_entities[201].currentState.torsoAnim=200;
    cg_entities[201].currentState.angles2[0]=-10;
    cg.snap->serverTime=250;cg.time=750;
    check(CG_AddLinkedEntity(&cg_entities[201],qtrue,250),"actual path-linked resolver accepts valid root and follower");
    player=target(0);player->currentState.groundEntityNum=201;
    VectorCopy(cg_entities[201].lerpOrigin,player->currentState.pos.trBase);player->currentState.pos.trBase[0]+=10;
    /* Target-relative reference isolates the incorrect viewer input, retaining
     * the SDK delta-angle arithmetic for comparison; it is not the proposal. */
    VectorCopy(player->currentState.pos.trBase,cg.snap->ps.origin);
    CG_CalcEntityLerpPositions(player);VectorCopy(player->lerpOrigin,reference);
    observers(player,a,b,first,second);
    check(!nearv(first,second),"actual curved path-linked mover depends on unrelated viewer origin");
    if(requireCanonical)check(nearv(first,second),"canonical path-linked rotation must be observer independent");
    observed("curved-path-linked-no-next",first,second,reference,1);
    check(CG_AddLinkedEntity(&cg_entities[201],qtrue,750),"path follower remains evaluable after legacy three-time sampling");
    if(verifyFixed)fixed_controls();
    printf("%u checks, %u failures; %u demonstrated observer-dependent cases; actual legacy CG functions, synthetic spline geometry\n",checks,failures,demonstrated);
    return failures?1:0;
}
