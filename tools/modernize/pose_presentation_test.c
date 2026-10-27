#include "q_shared.h"
#include "bg_public.h"
#include "tce_numeric.h"
#include "bg_tce_pose_presentation.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int checks;
#define CHECK(x) do { ++checks; if(!(x)) { fprintf(stderr,"line%d: %s\n",__LINE__,#x); exit(1); } } while(0)
#define NEAR(a,b,t) CHECK(fabs((double)(a)-(double)(b))<=(t))
static int BG_WeaponOnBackToWeap(int weapon) { return weapon; }
/* ACTUAL_PLAYER_CONVERTERS */

typedef struct {
    tcePosePresentationAxesInput_t input;
    float expectedBody[3][3],expectedTorso[3][3],expectedHead[3][3];
    uint32_t rateQ8;
} presentFixture_t;
/* CAPTURED_INPUTS */

static void phase_tests(void) {
    tcePosePresentationKey_t key={0x1234u,{606,606}},other;
    tcePosePresentationClip_t clip={2981,21,21,25,25,0},bad;
    tcePosePresentationSample_t sample;
    uint32_t word,stepped,again;
    unsigned int i;
    CHECK(BG_TCEPosePhasePack(&key,0,&clip,0,&word));
    CHECK(word==(TCE_POSE_PHASE_VALID|TCE_POSE_PHASE_VERSION_1));
    CHECK(BG_TCEPosePhaseRead(word,0,&key,&key,&clip,&sample));
    CHECK(sample.oldFrame==2981 && sample.frame==2981 && sample.held && !sample.finished);
    for(i=0;i<12000;++i) {
        uint32_t msQ8=i*128u,phase,old,next; float blend;
        CHECK(BG_TCEPosePhasePack(&key,0,&clip,0,&word));
        CHECK(BG_TCEPosePhaseStep(word,0,&key,&key,&clip,0,i,128,&word));
        CHECK(BG_TCEPosePhaseRead(word,0,&key,&key,&clip,&sample));
        phase=msQ8<6400?msQ8:6400+(msQ8-6400)%134400;
        CHECK(sample.elapsedQ8==phase);
        if(phase<6400) { old=next=0; blend=0; }
        else { uint32_t t=phase-6400;old=t/6400;next=(old+1)%21;blend=(float)(1.0-(double)(t%6400)/6400); }
        CHECK(sample.oldFrame==2981+(int)old && sample.frame==2981+(int)next);
        NEAR(sample.backlerp,blend,1e-7);
        CHECK(BG_TCEPosePhasePack(&key,0,&clip,sample.elapsedQ8,&again) && word==again);
    }
    CHECK(BG_TCEPosePhasePack(&key,0,&clip,25*256,&word));
    CHECK(BG_TCEPosePhaseRead(word,0,&key,&key,&clip,&sample));
    CHECK(sample.oldFrame==2981 && sample.frame==2982 && sample.backlerp==1);
    CHECK(BG_TCEPosePhaseStep(word,0,&key,&key,&clip,UINT32_MAX-5u,10,256,&stepped));
    CHECK(BG_TCEPosePhaseRead(stepped,0,&key,&key,&clip,&sample) && sample.elapsedQ8==41*256);
    CHECK(BG_TCEPosePhaseStep(stepped,0,&key,&key,&clip,10,500,0,&again) && again==stepped);
    CHECK(!BG_TCEPosePhaseStep(word,0,&key,&key,&clip,10,9,256,&stepped) && !stepped);
    CHECK(!BG_TCEPosePhaseStep(word,0,&key,&key,&clip,0,INT32_MAX+1u,256,&stepped));
    CHECK(!BG_TCEPosePhaseStep(word,0,&key,&key,&clip,0,1,1025,&stepped));
    other=key; other.profile++;
    CHECK(!BG_TCEPosePhaseRead(word,0,&other,&key,&clip,&sample) && !sample.frame);
    other=key; other.animation[0]^=512;
    CHECK(!BG_TCEPosePhaseRead(word,0,&other,&key,&clip,&sample));
    other=key; other.animation[1]^=512;
    CHECK(BG_TCEPosePhaseRead(word,0,&other,&key,&clip,&sample));
    CHECK(!BG_TCEPosePhaseRead(0,0,&key,&key,&clip,&sample));
    CHECK(!BG_TCEPosePhaseRead(TCE_POSE_PHASE_VALID,0,&key,&key,&clip,&sample));
    CHECK(!BG_TCEPosePhaseRead(TCE_POSE_PHASE_VALID|0x40000000u,0,&key,&key,&clip,&sample));
    CHECK(!BG_TCEPosePhaseRead(TCE_POSE_PHASE_VALID|TCE_POSE_PHASE_VERSION_1|TCE_POSE_PHASE_MASK,0,&key,&key,&clip,&sample));
    CHECK(!BG_TCEPosePhasePack(&key,0,&clip,550*256,&again) && !again);
    CHECK(!BG_TCEPosePhaseRead(TCE_POSE_PHASE_VALID|TCE_POSE_PHASE_VERSION_1|(550*256),0,&key,&key,&clip,&sample) && !sample.frame);
    CHECK(BG_TCEPosePhasePack(&key,0,&clip,550*256-1,&again));
    CHECK(!BG_TCEPosePhaseRead(word,2,&key,&key,&clip,&sample));
    bad=clip; bad.loopFrames=INT32_MAX; CHECK(!BG_TCEPosePhasePack(&key,0,&bad,0,&again) && !again);
    bad=clip; bad.firstFrame=8191; CHECK(!BG_TCEPosePhasePack(&key,0,&bad,0,&again));
    bad=clip; bad.frameLerp=65535; bad.numFrames=8192;bad.firstFrame=0;bad.loopFrames=8192;
    CHECK(!BG_TCEPosePhasePack(&key,0,&bad,0,&again));
    bad=clip; bad.reversed=1; CHECK(BG_TCEPosePhasePack(&key,0,&bad,25*256,&word));
    CHECK(BG_TCEPosePhaseRead(word,0,&key,&key,&bad,&sample) && sample.oldFrame==3001 && sample.frame==3000);
    bad.firstFrame=4594;bad.numFrames=1;bad.loopFrames=0;bad.frameLerp=50;bad.initialLerp=50;bad.reversed=0;
    CHECK(!BG_TCEPosePhasePack(&key,0,&bad,50*256+1,&word) && !word);
    CHECK(!BG_TCEPosePhaseRead(TCE_POSE_PHASE_VALID|TCE_POSE_PHASE_VERSION_1|(50*256+1),0,&key,&key,&bad,&sample) && !sample.frame);
    CHECK(!BG_TCEPosePhasePack(&key,0,&bad,UINT64_MAX,&word));
    CHECK(BG_TCEPosePhasePack(&key,0,&bad,0,&word));
    CHECK(BG_TCEPosePhaseStep(word,0,&key,&key,&bad,0,100000,256,&word));
    CHECK(BG_TCEPosePhaseRead(word,0,&key,&key,&bad,&sample));
    CHECK(sample.elapsedQ8==50*256 && sample.frame==4594 && sample.oldFrame==4594 && sample.finished && sample.held);
    bad=clip;bad.numFrames=5;bad.loopFrames=2;
    CHECK(BG_TCEPosePhasePack(&key,0,&bad,0,&word));
    CHECK(BG_TCEPosePhaseStep(word,0,&key,&key,&bad,0,100000,256,&word));
    CHECK(BG_TCEPosePhaseRead(word,0,&key,&key,&bad,&sample) && sample.elapsedQ8>=100*256 && sample.elapsedQ8<150*256);
}
static void extended_cycle_tests(void) {
    /* Installed body.aninc row50: 3 declared frames, loop13, FPS22 ->45ms;
     * firing assignment sets initialLerp40. Original controller evidence is
     * 2088,2089,2090,2078,2088 (reverse2090,2089,2088,2100,2090),
     * not a thirteen-frame tail. The shared helper defines canonical timing. */
    const int sequence[2][4]={{2088,2089,2090,2078},{2090,2089,2088,2100}};
    tcePosePresentationKey_t key={0x1234u,{50,562}};
    tcePosePresentationClip_t clip={2088,3,13,45,40,0},bad;
    tcePosePresentationSample_t sample;
    uint32_t word,again,phase;
    unsigned int i;int reverse;
    for(reverse=0;reverse<2;++reverse) {
        clip.reversed=reverse;
        for(i=0;i<2000;++i) {
            uint32_t elapsed=i*128u,progress,ordinal,next;float weight;
            CHECK(BG_TCEPosePhasePack(&key,1,&clip,0,&word));
            CHECK(BG_TCEPosePhaseStep(word,1,&key,&key,&clip,0,i,128,&word));
            CHECK(BG_TCEPosePhaseRead(word,1,&key,&key,&clip,&sample));
            phase=elapsed<40*256?elapsed:40*256+(elapsed-40*256)%(4*45*256);
            CHECK(sample.elapsedQ8==phase);
            if(phase<40*256) { ordinal=next=0;weight=0; }
            else {
                progress=phase-40*256;ordinal=progress/(45*256);next=(ordinal+1)%4;
                weight=(float)(1.0-(double)(progress%(45*256))/(45*256));
            }
            CHECK(sample.oldFrame==sequence[reverse][ordinal] && sample.frame==sequence[reverse][next]);
            NEAR(sample.backlerp,weight,1e-7);
            CHECK(BG_TCEPosePhasePack(&key,1,&clip,sample.elapsedQ8,&again) && word==again);
        }
        CHECK(BG_TCEPosePhasePack(&key,1,&clip,(40+3*45)*256,&word));
        CHECK(BG_TCEPosePhaseRead(word,1,&key,&key,&clip,&sample));
        CHECK(sample.oldFrame==sequence[reverse][3] && sample.frame==sequence[reverse][0]);
        CHECK(BG_TCEPosePhaseStep(word,1,&key,&key,&clip,100,10000,0,&again) && again==word);
        CHECK(!BG_TCEPosePhasePack(&key,1,&clip,(40+4*45)*256,&word) && !word);
        CHECK(!BG_TCEPosePhaseRead(TCE_POSE_PHASE_VALID|TCE_POSE_PHASE_VERSION_1|((40+4*45)*256),
                                  1,&key,&key,&clip,&sample) && !sample.frame);
        CHECK(BG_TCEPosePhasePack(&key,1,&clip,(40+4*45)*256-1,&word));
    }
    /* Full-cycle bounds must reject a later outlier even if phase0 itself
     * would fit. Forward outlier hits0; reverse outlier hits8191 exactly. */
    clip.reversed=0;clip.firstFrame=10;
    CHECK(BG_TCEPosePhasePack(&key,0,&clip,0,&word));
    CHECK(BG_TCEPosePhasePack(&key,0,&clip,175*256,&word));
    CHECK(BG_TCEPosePhaseRead(word,0,&key,&key,&clip,&sample) && sample.oldFrame==0);
    bad=clip;bad.firstFrame=9;word=123;
    CHECK(!BG_TCEPosePhasePack(&key,0,&bad,0,&word) && !word);
    CHECK(!BG_TCEPosePhaseRead(TCE_POSE_PHASE_VALID|TCE_POSE_PHASE_VERSION_1,0,&key,&key,&bad,&sample));
    clip.firstFrame=8189;CHECK(BG_TCEPosePhasePack(&key,0,&clip,0,&word));
    bad=clip;bad.firstFrame=8190;CHECK(!BG_TCEPosePhasePack(&key,0,&bad,0,&word));
    clip.firstFrame=8179;clip.reversed=1;
    CHECK(BG_TCEPosePhasePack(&key,0,&clip,175*256,&word));
    CHECK(BG_TCEPosePhaseRead(word,0,&key,&key,&clip,&sample) && sample.oldFrame==8191);
    bad=clip;bad.firstFrame=8180;word=123;
    CHECK(!BG_TCEPosePhasePack(&key,0,&bad,0,&word) && !word);
    CHECK(!BG_TCEPosePhaseRead(TCE_POSE_PHASE_VALID|TCE_POSE_PHASE_VERSION_1,0,&key,&key,&bad,&sample));
    /* Normal loops retain their existing frame cap regardless of direction. */
    clip.firstFrame=8189;clip.loopFrames=3;
    CHECK(BG_TCEPosePhasePack(&key,0,&clip,0,&word));
    bad=clip;bad.firstFrame=8190;CHECK(!BG_TCEPosePhasePack(&key,0,&bad,0,&word));
}
static void axis_tests(void) {
    tcePosePresentationAxes_t a,b;
    tcePosePresentationAxesInput_t input={TCE_POSE_PRESENT_PRONE,{0,0,0},0,{100,0,0},0,{0,0},1.25f,1};
    unsigned int n; int i,j,k;
    for(n=0;n<sizeof(presentFixtures)/sizeof(presentFixtures[0]);++n) {
        const presentFixture_t *f=&presentFixtures[n];
        tcePosePresentationKey_t key={1,{606,606}};
        tcePosePresentationClip_t clip={2981,21,21,25,25,0};
        uint32_t word,stepped;
        CHECK(BG_TCEPosePresentationAxes(&f->input,&a));
        for(i=0;i<3;++i) for(j=0;j<3;++j) {
            NEAR(a.body[i][j],f->expectedBody[i][j],2e-5);
            NEAR(a.torso[i][j],f->expectedTorso[i][j],2e-5);
            NEAR(a.head[i][j],f->expectedHead[i][j],2e-5);
            for(k=0;k<3;++k) {
                double dot=(double)a.body[i][0]*a.body[k][0]+(double)a.body[i][1]*a.body[k][1]+(double)a.body[i][2]*a.body[k][2];
                NEAR(dot,i==k?f->input.scale*f->input.scale:0,2e-5);
            }
        }
        CHECK(BG_TCEPosePhasePack(&key,0,&clip,100*256,&word));
        CHECK(BG_TCEPosePhaseStep(word,0,&key,&key,&clip,100,150,f->rateQ8,&stepped));
        if(!f->rateQ8) CHECK(word==stepped);
    }
    CHECK(BG_TCEPosePresentationAxes(&input,&a));
    NEAR(a.bodyAngles[0],5,1e-6); NEAR(a.torsoRelativeAngles[0],-8,1e-6);
    input.velocity[0]=0;input.lean=-10;
    CHECK(BG_TCEPosePresentationAxes(&input,&a));NEAR(a.torsoRelativeAngles[2],-16.5,1e-6);
    input.lean=10;CHECK(BG_TCEPosePresentationAxes(&input,&b));NEAR(b.torsoRelativeAngles[2],10,1e-6);
    input.freelook[0]=20;input.freelook[1]=-30;
    CHECK(BG_TCEPosePresentationAxes(&input,&a));
    CHECK(!memcmp(a.body,b.body,sizeof(a.body)) && !memcmp(a.torso,b.torso,sizeof(a.torso)));
    NEAR(a.headRelativeAngles[0]-b.headRelativeAngles[0],14,1e-5);
    NEAR(a.headRelativeAngles[1]-b.headRelativeAngles[1],-21,1e-5);
    input.mode=TCE_POSE_PRESENT_FREECLIMB;CHECK(BG_TCEPosePresentationAxes(&input,&a));
    input.mode=TCE_POSE_PRESENT_LADDER;CHECK(BG_TCEPosePresentationAxes(&input,&b));
    CHECK(!memcmp(&a,&b,sizeof(a)));
    input.scale=0;CHECK(!BG_TCEPosePresentationAxes(&input,&a) && a.body[0][0]==0);
    input.scale=1;input.lean=(float)NAN;CHECK(!BG_TCEPosePresentationAxes(&input,&a));
    input.lean=0;input.velocity[2]=(float)INFINITY;CHECK(!BG_TCEPosePresentationAxes(&input,&a));
    input.velocity[2]=8193;CHECK(!BG_TCEPosePresentationAxes(&input,&a));
}
static void converter_tests(void) {
    playerState_t ps; entityState_t state;
    int mode;
    CHECK(sizeof(entityState_t)==288 && sizeof(playerState_t)==1452 && sizeof(qhandle_t)==4);
    CHECK(offsetof(entityState_t,time)==84 && offsetof(entityState_t,time2)==88);
    for(mode=0;mode<2;++mode) {
        memset(&ps,0,sizeof(ps)); memset(&state,0,sizeof(state));
        ps.stats[STAT_HEALTH]=100;ps.clientNum=3;ps.legsAnim=606;ps.torsoAnim=606;
        state.time=-1593835520;state.time2=-1593835519;
        if(mode) BG_PlayerStateToEntityStateExtraPolate(&ps,&state,100,qfalse);
        else BG_PlayerStateToEntityState(&ps,&state,qfalse);
        CHECK(state.eType==ET_PLAYER && state.time==-1593835520 && state.time2==-1593835519);
        ps.pm_type=PM_SPECTATOR;
        if(mode) BG_PlayerStateToEntityStateExtraPolate(&ps,&state,150,qfalse);
        else BG_PlayerStateToEntityState(&ps,&state,qfalse);
        CHECK(state.eType==ET_INVISIBLE && state.time==-1593835520 && state.time2==-1593835519);
    }
}
int main(void) {
    phase_tests();extended_cycle_tests();axis_tests();converter_tests();
    printf("{\"checks\":%u,\"captured_input_cases\":%u,\"entityState\":%u,\"playerState\":%u}\n",checks,
        (unsigned int)(sizeof(presentFixtures)/sizeof(presentFixtures[0])),(unsigned int)sizeof(entityState_t),(unsigned int)sizeof(playerState_t));
    return 0;
}
