#ifndef POSE_PRESENTATION_RUNTIME_CASES_H
#define POSE_PRESENTATION_RUNTIME_CASES_H
#include "bg_tce_pose_presentation.h"
#define PRESENTATION_RUNTIME_CASES 23
typedef struct {
    uint32_t signature;
    uint32_t nonce;
    entityState_t state[2];
    playerState_t ps[2];
    tcePoseCompactSample_t channel[2];
    tcePosePresentationAxes_t axes;
    float origin[3];
    int time[2],queryTime;
    int hasNext,step;
    int moverCount;
    entityState_t movers[4];
    char manifest[TCE_POSE_PRESENTATION_MANIFEST];
} presentationRuntimeCase_t;
static void PresentationRuntimeSpline(void) {
    int i;numSplinePaths=1;memset(splinePaths,0,sizeof(splinePaths));
    for(i=0;i<MAX_SPLINE_SEGMENTS;++i) {
        splineSegment_t *segment=&splinePaths[0].segments[i];vec3_t delta;
        double a=i*(M_PI*.5/MAX_SPLINE_SEGMENTS),b=(i+1)*(M_PI*.5/MAX_SPLINE_SEGMENTS);
        segment->start[0]=(float)(100*cos(a));segment->start[1]=(float)(100*sin(a));
        delta[0]=(float)(100*cos(b))-segment->start[0];delta[1]=(float)(100*sin(b))-segment->start[1];delta[2]=0;
        segment->length=VectorNormalize2(delta,segment->v_norm);splinePaths[0].length+=segment->length;
    }
}
static void PresentationRuntimeModel(bg_character_t *character,bg_characterDef_t *definition,
                                      animModelInfo_t *model,animation_t animations[236]) {
    int i;memset(character,0,sizeof(*character));memset(definition,0,sizeof(*definition));memset(model,0,sizeof(*model));
    memset(animations,0,236*sizeof(*animations));
    strcpy(character->characterFile,"characters/test.char");strcpy(definition->mesh,"models/body.mdm");
    strcpy(definition->hudhead,"models/head.md3");strcpy(model->animationGroup,"animations/test.anim");
    strcpy(model->animationScript,"animations/test.script");character->animModelInfo=model;model->numAnimations=236;
    for(i=0;i<236;++i) {
        model->animations[i]=&animations[i];strcpy(animations[i].name,"ordinary");
        animations[i].firstFrame=0;animations[i].numFrames=2;animations[i].loopFrames=2;
        animations[i].frameLerp=animations[i].initialLerp=25;
    }
    strcpy(animations[94].name,"climb_up");animations[94].flags=ANIMFL_LADDERANIM;
    animations[94].firstFrame=2981;animations[94].numFrames=animations[94].loopFrames=21;
    strcpy(animations[222].name,"torso_hold");animations[222].firstFrame=57;
    animations[222].numFrames=animations[222].loopFrames=20;
    animations[94].moveSpeed=animations[222].moveSpeed=-1;
#ifdef CGAMEDLL
    character->mesh=250;character->hudhead=228;
    animations[94].mdxFile=111;animations[222].mdxFile=222;
#endif
}
#endif
