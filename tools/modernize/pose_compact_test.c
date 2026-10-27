#include "bg_tce_pose_presentation.h"
#include "bg_tce_pose_clock.h"
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <math.h>
#include <string.h>
static unsigned int checks,affected;
static double maximumError;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct {const char *name;tcePosePresentationClip_t clip;} actualClip_t;
/* ACTUAL_CLIPS */
static void compact(void) {
    unsigned int i,j;uint16_t code;uint64_t error,limit,phase; tcePoseCompactSample_t s,t;
    tcePoseClockClip_t raw;tcePosePresentationClip_t bad;
    for(i=0;i<sizeof(actualClips)/sizeof(actualClips[0]);++i) {
        const tcePosePresentationClip_t *c=&actualClips[i].clip;
        raw.firstFrame=c->firstFrame;raw.numFrames=c->numFrames;raw.loopFrames=c->loopFrames;
        raw.frameLerp=c->frameLerp;raw.initialLerp=c->initialLerp;raw.reversed=c->reversed;
        CHECK(BG_TCEPoseClockPhaseLimit(&raw,&limit));
        CHECK(BG_TCEPoseCompactPrecision(c,&error));
        CHECK(error<=((uint64_t)c->frameLerp<<16)/128u);
        if(BG_TCEPosePresentationMode(actualClips[i].name,"",0,0))++affected;
        for(j=0;j<2049;++j) {
            uint64_t distance;double fraction;
            phase=(limit/2048u)*j+(limit%2048u)*j/2048u;
            CHECK(BG_TCEPoseCompactPack(c,phase,&code));
            CHECK(code>=0xc000u);
            CHECK(BG_TCEPoseCompactSample(c,code,100,100,0,&s));
            distance=s.elapsedQ16>phase?s.elapsedQ16-phase:phase-s.elapsedQ16;
            CHECK(distance<=error);
            fraction=(double)distance/((uint64_t)c->frameLerp<<16);
            if(fraction>maximumError)maximumError=fraction;
            CHECK(BG_TCEPoseCompactSample(c,code,100,150,0,&t));
            CHECK(!memcmp(&s,&t,sizeof(s)));
            CHECK(BG_TCEPoseCompactSample(c,code,UINT32_MAX-20u,29u,256,&t));
            CHECK(BG_TCEPoseCompactPack(c,t.elapsedQ16,&code));
            CHECK(!BG_TCEPoseCompactSample(c,code,100,99,256,&t));
            CHECK(!BG_TCEPoseCompactSample(c,code,100,100,1025,&t));
        }
        CHECK(!BG_TCEPoseCompactPack(c,limit+1u,&code)&&!code);
        CHECK(!BG_TCEPoseCompactSample(c,0,0,0,0,&s));
        CHECK(!BG_TCEPoseCompactSample(c,0x8000u,0,0,0,&s));
        CHECK(!BG_TCEPoseCompactSample(c,0x4000u,0,0,0,&s));
        CHECK(BG_TCEPoseCompactPack(c,0,&code));
        CHECK((int)code-65536>=-32768&&(int)code-65536<0);
    }
    bad=actualClips[0].clip;bad.firstFrame=8192;CHECK(!BG_TCEPoseCompactPack(&bad,0,&code));
    bad.firstFrame=0;bad.numFrames=8192;bad.loopFrames=8192;
    CHECK(!BG_TCEPoseCompactPrecision(&bad,&error)&&!error);
    bad.firstFrame=0;bad.numFrames=1;bad.loopFrames=0;bad.frameLerp=INT_MAX;bad.initialLerp=INT_MAX;
    CHECK(BG_TCEPoseCompactPack(&bad,0,&code)); /* Large time, tiny frame span is valid. */
}
typedef struct {int calls,fail,first,ladder;float offset,delta;} footInput_t;
static int feet(void *user,int frame,float out[2][3]) {
    footInput_t *c=(footInput_t*)user;int f=frame-c->first;
    ++c->calls;if(c->fail==c->calls)return 0;
    memset(out,0,6*sizeof(float));
    out[0][0]=c->offset+f*c->delta;out[1][0]=out[0][0]-1;
    out[0][2]=c->ladder?c->offset+f*c->delta:-1;
    out[1][2]=c->ladder?out[0][2]+1:0;
    return 1;
}
static void speed_tests(void) {
    footInput_t c={0,0,10,0,400,1};int speed=123,speeds[3]={20,INT_MIN,0};uint32_t profile;
    float velocity[3]={0,0,0};
    CHECK(BG_TCEPosePresentationMoveSpeed(10,5,25,0,feet,&c,&speed)&&speed==40&&c.calls==5);
    c.calls=0;c.offset=-800;c.delta=-1;c.ladder=1;
    CHECK(BG_TCEPosePresentationMoveSpeed(10,5,25,1,feet,&c,&speed)&&speed==40);
    c.calls=0;c.offset=400;c.delta=.0078125f;c.ladder=0;
    CHECK(BG_TCEPosePresentationMoveSpeed(10,2,1,0,feet,&c,&speed)&&speed==16);
    c.calls=0;c.offset=-400;c.delta=-.0078125f;
    CHECK(BG_TCEPosePresentationMoveSpeed(10,2,1,0,feet,&c,&speed)&&speed==16);
    c.calls=0;c.fail=2;speed=123;
    CHECK(!BG_TCEPosePresentationMoveSpeed(10,5,25,0,feet,&c,&speed)&&speed==123);
    c.fail=0;c.calls=0;c.offset=(float)NAN;
    CHECK(!BG_TCEPosePresentationMoveSpeed(10,5,25,0,feet,&c,&speed)&&speed==123);
    CHECK(!BG_TCEPosePresentationMoveSpeed(10,1,25,0,feet,&c,&speed));
    CHECK(!BG_TCEPosePresentationMoveSpeed(8191,2,25,0,feet,&c,&speed));
    profile=BG_TCEPosePresentationSpeedProfile(123,speeds,3);CHECK(profile);
    speeds[0]++;CHECK(profile!=BG_TCEPosePresentationSpeedProfile(123,speeds,3));
    CHECK(!BG_TCEPosePresentationSpeedProfile(0,speeds,3));
    speeds[0]=-1;CHECK(!BG_TCEPosePresentationSpeedProfile(123,speeds,3));
    CHECK(BG_TCEPosePresentationRate(TCE_POSE_PRESENT_LADDER,velocity,69)==0);
    velocity[2]=69;CHECK(BG_TCEPosePresentationRate(TCE_POSE_PRESENT_LADDER,velocity,69)==256);
    velocity[2]=8192;CHECK(BG_TCEPosePresentationRate(TCE_POSE_PRESENT_LADDER,velocity,69)==1024);
    CHECK(BG_TCEPosePresentationRate(TCE_POSE_PRESENT_LADDER,velocity,-1)==0);
    CHECK(BG_TCEPosePresentationRate(TCE_POSE_PRESENT_LADDER,velocity,0)==256);
}
static void profile_tests(void) {
    static bg_character_t character;static bg_characterDef_t definition;static animModelInfo_t model;
    static animation_t animation;uint32_t profile;
    strcpy(character.characterFile,"characters/test.char");strcpy(definition.mesh,"models/body.mdm");
    strcpy(definition.hudhead,"models/head.md3");strcpy(model.animationGroup,"animations/test.anim");
    strcpy(model.animationScript,"animations/test.script");strcpy(animation.name,"climb_up");
    model.numAnimations=1;model.animations[0]=&animation;character.animModelInfo=&model;
    animation.firstFrame=20;animation.numFrames=21;animation.frameLerp=25;animation.initialLerp=25;
    profile=BG_TCEPosePresentationProfile(&character,&definition);CHECK(profile);
    strcpy(animation.mdxFileName,"ignored/renderer/path.mdx");animation.moveSpeed=69;
    CHECK(profile==BG_TCEPosePresentationProfile(&character,&definition));
    strcpy(character.characterFile,"CHARACTERS\\TEST.CHAR");
    CHECK(profile==BG_TCEPosePresentationProfile(&character,&definition));
    animation.firstFrame++;CHECK(profile!=BG_TCEPosePresentationProfile(&character,&definition));
    memset(character.characterFile,'x',sizeof(character.characterFile));
    CHECK(!BG_TCEPosePresentationProfile(&character,&definition));
    strcpy(character.characterFile,"characters/test.char");definition.mesh[0]=0;
    CHECK(!BG_TCEPosePresentationProfile(&character,&definition));
    CHECK(BG_TCEPosePresentationPairProfile(1,2)!=BG_TCEPosePresentationPairProfile(2,1));
    CHECK(!BG_TCEPosePresentationPairProfile(0,2));
    CHECK(sizeof(entityState_t)==288&&sizeof(playerState_t)==1452);
}
static void manifest_tests(void) {
    uint32_t profiles[65],digest;char text[TCE_POSE_PRESENTATION_MANIFEST],copy[TCE_POSE_PRESENTATION_MANIFEST];int i;
    for(i=0;i<65;++i)profiles[i]=(uint32_t)(65-i);
    CHECK(BG_TCEPosePresentationManifest(profiles,64,text,sizeof(text)));
    CHECK(strlen(text)==578);
    digest=BG_TCEPosePresentationManifestDigest(text);CHECK(digest);
    CHECK(BG_TCEPosePresentationManifestMatch(text,profiles,64)==digest);
    CHECK(!BG_TCEPosePresentationManifestMatch(text,profiles,63));
    strcpy(copy,text);copy[2]='A';CHECK(!BG_TCEPosePresentationManifestDigest(copy));
    strcpy(copy,text);copy[10]=';';CHECK(!BG_TCEPosePresentationManifestDigest(copy));
    strcpy(copy,text);copy[11]=0;CHECK(BG_TCEPosePresentationManifestDigest(copy));
    CHECK(!BG_TCEPosePresentationManifestDigest("2;00000000,"));
    CHECK(!BG_TCEPosePresentationManifestDigest("2;00000001,00000001,"));
    CHECK(!BG_TCEPosePresentationManifestDigest("2;00000002,00000001,"));
    CHECK(!BG_TCEPosePresentationManifestDigest("2;00000001"));
    CHECK(!BG_TCEPosePresentationManifestDigest("2;"));
    CHECK(!BG_TCEPosePresentationManifestDigest("1;00000001,"));
    CHECK(!BG_TCEPosePresentationManifest(profiles,65,text,sizeof(text))&&!text[0]);
    CHECK(!BG_TCEPosePresentationManifest(profiles,64,text,578)&&!text[0]);
    profiles[0]=0;CHECK(!BG_TCEPosePresentationManifest(profiles,64,text,sizeof(text))&&!text[0]);
}
int main(void) {
    compact();speed_tests();profile_tests();manifest_tests();
    printf("{\"checks\":%u,\"clip_alternatives\":%u,\"affected_alternatives\":%u,\"maximum_frame_error\":%.12g}\n",
        checks,(unsigned)(sizeof(actualClips)/sizeof(actualClips[0])),affected,maximumError);
    return 0;
}
