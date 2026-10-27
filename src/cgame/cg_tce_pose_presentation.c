#include "cg_local.h"
#include "cg_tce_pose_presentation.h"
#include "tce_smoke_grenade.h"
#include "../game/bg_tce_pose_presentation.h"
#include <limits.h>

enum { PRESENT_CHARACTERS=MAX_CHARACTERS+2*NUM_PLAYER_CLASSES };
typedef struct {const bg_character_t *character;uint32_t rawProfile,profile;int moveSpeed[MAX_MODEL_ANIMATIONS];} presentCharacter_t;
typedef struct {const animModelInfo_t *model;int moveSpeed[MAX_MODEL_ANIMATIONS];} presentModel_t;
static presentCharacter_t presentationCharacters[PRESENT_CHARACTERS];
static presentModel_t presentationModels[MAX_ANIMSCRIPT_MODELS];
static int presentationCharacterCount,presentationModelCount;
static unsigned char presentationActive[MAX_GENTITIES+1];
static vmCvar_t presentationCapability;
static uint32_t presentationDigest;
static uint32_t presentationNonce;

void CG_TCEPresentationRestart(void) {memset(presentationActive,0,sizeof(presentationActive));}
void CG_TCEPresentationInit(void) {
    memset(presentationCharacters,0,sizeof(presentationCharacters));memset(presentationModels,0,sizeof(presentationModels));
    presentationCharacterCount=presentationModelCount=0;CG_TCEPresentationRestart();
    presentationDigest=presentationNonce=0;
    trap_Cvar_Register(&presentationCapability,"tce_pose","0",CVAR_USERINFO|CVAR_ROM);
    trap_Cvar_Set("tce_pose","0");
}
void CG_TCEPresentationRememberModel(const animModelInfo_t *model) {
    int i,j;if(!model||model->numAnimations<1||model->numAnimations>MAX_MODEL_ANIMATIONS)return;
    for(i=0;i<presentationModelCount;++i)if(presentationModels[i].model==model)return;
    if(presentationModelCount==MAX_ANIMSCRIPT_MODELS)return;
    i=presentationModelCount++;presentationModels[i].model=model;
    for(j=0;j<model->numAnimations;++j)if(model->animations[j])presentationModels[i].moveSpeed[j]=model->animations[j]->moveSpeed;
}
typedef struct {const bg_character_t *character;const animation_t *animation;} presentationFeet_t;
static int PresentationFeet(void *user,int absoluteFrame,float feet[2][3]) {
    const presentationFeet_t *context=(const presentationFeet_t*)user;
    const char *names[2]={"tag_footleft","tag_footright"};refEntity_t body;orientation_t tag;int i;
    memset(&body,0,sizeof(body));body.hModel=context->character->mesh;
    body.frame=body.oldframe=body.torsoFrame=body.oldTorsoFrame=absoluteFrame;
    body.frameModel=body.oldframeModel=body.torsoFrameModel=body.oldTorsoFrameModel=context->animation->mdxFile;
    AxisClear(body.axis);AxisClear(body.torsoAxis);
    for(i=0;i<2;++i) {
        if(trap_R_LerpTag(&tag,&body,names[i],0)<0)return 0;
        VectorCopy(tag.origin,feet[i]);
    }return 1;
}
void CG_TCEPresentationRegister(const bg_character_t *character,const bg_characterDef_t *definition) {
    const animModelInfo_t *model;const presentModel_t *raw=NULL;presentCharacter_t *entry;
    int i,j,budget=32768;uint32_t profile=BG_TCEPosePresentationProfile(character,definition);
    if(!character||!character->animModelInfo)return;
    model=character->animModelInfo;
    for(i=0;i<presentationCharacterCount;++i)if(presentationCharacters[i].character==character)break;
    if(i==presentationCharacterCount) {if(i==PRESENT_CHARACTERS)return;++presentationCharacterCount;}
    entry=&presentationCharacters[i];memset(entry,0,sizeof(*entry));entry->character=character;
    for(i=0;i<presentationModelCount;++i)if(presentationModels[i].model==model) {raw=&presentationModels[i];break;}
    if(!profile||!raw)return;
    entry->rawProfile=profile;
    for(j=0;j<model->numAnimations;++j) {
        const animation_t *animation=model->animations[j];int speed=raw->moveSpeed[j];
        if(!animation) {entry->moveSpeed[j]=INT_MIN;continue;}
        if(speed<0) {
            presentationFeet_t context;context.character=character;context.animation=animation;
            speed=INT_MIN;
            if(animation->numFrames>=2&&animation->numFrames<=budget) {
                budget-=animation->numFrames;
                BG_TCEPosePresentationMoveSpeed(animation->firstFrame,animation->numFrames,animation->frameLerp,
                    !!(animation->flags&ANIMFL_LADDERANIM),PresentationFeet,&context,&speed);
            }
        }
        entry->moveSpeed[j]=speed>=0&&speed<=8192?speed:INT_MIN;
    }
    entry->profile=BG_TCEPosePresentationSpeedProfile(profile,entry->moveSpeed,model->numAnimations);
    CG_TCEPresentationManifestChanged();
}
void CG_TCEPresentationManifestChanged(void) {
    uint32_t profiles[TCE_POSE_PRESENTATION_PROFILES],digest,nonce=0;int i,j,count=0;char ack[24];
    const char *nonceText;
    const bg_character_t *carriers[2]={BG_GetCharacter(TEAM_AXIS,6),BG_GetCharacter(TEAM_ALLIES,6)};
    for(i=0;i<presentationCharacterCount;++i) {
        const presentCharacter_t *entry=&presentationCharacters[i];int k;
        if(entry->profile) {
            for(k=0;k<count&&profiles[k]!=entry->profile;++k) {}
            if(k<count)continue;
            if(count==TCE_POSE_PRESENTATION_PROFILES)goto unavailable;
            profiles[count++]=entry->profile;
        }
    }
    for(i=0;i<presentationCharacterCount;++i) {
        const presentCharacter_t *render=&presentationCharacters[i];
        if(render->character!=carriers[0]&&render->character!=carriers[1])continue;
        for(j=0;j<presentationCharacterCount;++j) {
            const presentCharacter_t *animation=&presentationCharacters[j];uint32_t profile;int k;
            if(!render->profile||!animation->rawProfile||animation==render||
               animation->character->animModelInfo!=render->character->animModelInfo)continue;
            profile=BG_TCEPosePresentationPairProfile(render->profile,animation->rawProfile);
            for(k=0;k<count&&profiles[k]!=profile;++k) {}
            if(k<count)continue;
            if(count==TCE_POSE_PRESENTATION_PROFILES)goto unavailable;
            profiles[count++]=profile;
        }
    }
    digest=BG_TCEPosePresentationManifestMatch(CG_ConfigString(TCE_POSE_PRESENTATION_CS),profiles,count);
    nonceText=Info_ValueForKey(CG_ConfigString(CS_PLAYERS+cg.clientNum),"tcep");
    for(i=0;i<8;++i) {
        unsigned char c=(unsigned char)nonceText[i];unsigned digit;
        if(c>='0'&&c<='9')digit=c-'0';else if(c>='a'&&c<='f')digit=c-'a'+10u;else goto unavailable;
        nonce=(nonce<<4)|digit;
    }
    if(nonceText[8])goto unavailable;
    if(digest&&nonce) {
        if(digest!=presentationDigest||nonce!=presentationNonce) {
            Com_sprintf(ack,sizeof(ack),"2:%08x:%08x",(unsigned)digest,(unsigned)nonce);trap_Cvar_Set("tce_pose",ack);
        }
        presentationDigest=digest;presentationNonce=nonce;return;
    }
unavailable:
    if(presentationDigest)trap_Cvar_Set("tce_pose","0");
    presentationDigest=presentationNonce=0;
}
static uint32_t PresentationProfile(const bg_character_t *character) {
    int i;for(i=0;i<presentationCharacterCount;++i)if(presentationCharacters[i].character==character)return presentationCharacters[i].profile;return 0;
}
static int PresentationSpeed(const bg_character_t *character,int animation,int *speed) {
    int i;
    for(i=0;i<presentationCharacterCount;++i)if(presentationCharacters[i].character==character) {
        *speed=presentationCharacters[i].moveSpeed[animation];return *speed>=0;
    }
    return 0;
}
static void PresentationClip(const animation_t *animation,tcePosePresentationClip_t *clip) {
    clip->firstFrame=animation->firstFrame;clip->numFrames=animation->numFrames;
    clip->loopFrames=animation->loopFrames;clip->frameLerp=animation->frameLerp;
    clip->initialLerp=animation->initialLerp;clip->reversed=!!(animation->flags&ANIMFL_REVERSED);
}
static int PresentationInput(const entityState_t *state,const bg_character_t *character,const bg_character_t *animationCharacter,
                              tcePosePresentationAxesInput_t *input,const animation_t *animation[2],int speed[2]) {
    int i,number[2]={state->legsAnim,state->torsoAnim},mode;const animModelInfo_t *model;
    if(!character||!animationCharacter)return 0;
    model=animationCharacter->animModelInfo;
    if(!model||model!=character->animModelInfo||state->eType!=ET_PLAYER||
       (state->eFlags&(EF_DEAD|EF_NODRAW|EF_MOUNTEDTANK|EF_MG42_ACTIVE|EF_AAGUN_ACTIVE|0x01000000)))return 0;
    for(i=0;i<2;++i) {
        int index=number[i]&~ANIM_TOGGLEBIT;
        if(number[i]<0||number[i]>=1024||index>=model->numAnimations||index>=MAX_MODEL_ANIMATIONS)return 0;
        animation[i]=model->animations[index];
        if(!animation[i]||!PresentationSpeed(character,index,&speed[i]))return 0;
    }
    mode=BG_TCEPosePresentationMode(animation[0]->name,animation[1]->name,!!(state->effect1Time&4),!!(state->effect1Time&0x400));
    if(!mode)return 0;
    memset(input,0,sizeof(*input));input->mode=(tcePosePresentationMode_t)mode;
    VectorCopy(state->apos.trBase,input->aim);VectorCopy(state->pos.trDelta,input->velocity);
    input->movementYaw=state->angles2[YAW];input->lean=state->angles2[ROLL];
    if(input->movementYaw>128&&input->movementYaw<=255)input->movementYaw-=256;
    input->freelook[0]=state->angles[PITCH];input->freelook[1]=state->angles[YAW];
    input->scale=tceSmokeNewBBox?1.25f:1.0f;input->leanMode=cgs.tceLeanMode>0;return 1;
}
static int PresentationCode(int value,uint16_t *code) {
    if(value>=0||value<-32768)return 0;
    *code=(uint16_t)(value+65536);
    return (*code&~TCE_POSE_COMPACT_MASK)==(TCE_POSE_COMPACT_VALID|TCE_POSE_COMPACT_VERSION);
}
static void PresentationLerp(lerpFrame_t *lf,const animation_t *animation,int number,
                              const tcePoseCompactSample_t *sample,uint32_t rate,int now) {
    lf->animation=(animation_t*)animation;lf->animationNumber=number;
    lf->oldFrame=sample->oldFrame;lf->frame=sample->frame;lf->backlerp=sample->backlerp;
    lf->oldFrameModel=lf->frameModel=animation->mdxFile;lf->animSpeedScale=rate/256.0f;
    lf->frameTime=lf->oldFrameTime=now;lf->animationTime=now;
    lf->yawing=lf->pitching=qfalse;
}
static int PresentationMoverSnapshot(void *user,int number,tcePoseMoverState_t *out) {
    const snapshot_t *snapshot=(const snapshot_t *)user;int i;
    if(!snapshot||number<0||number>=ENTITYNUM_MAX_NORMAL)return 0;
    for(i=0;i<snapshot->numEntities;++i)if(snapshot->entities[i].number==number) {
        BG_TCEPoseMoverState(&snapshot->entities[i],out);return 1;
    }
    return 0;
}
int CG_TCEPresentationPrepare(centity_t *cent,const bg_character_t *character,refEntity_t *body,refEntity_t *head) {
    entityState_t own[2],*state,*next=NULL;playerState_t copy;centity_t selector;
    const animation_t *animation[2],*nextAnimation[2];const bg_character_t *animationCharacter,*renderCharacter;
    tcePosePresentationAxesInput_t input,newInput;tcePosePresentationAxes_t axes;
    tcePosePresentationClip_t clip[2];tcePoseCompactSample_t sample[2];uint32_t rate[2],from,to,gap,step;
    uint16_t code[2];int speed[2],newSpeed[2],i,index,success=0;const char *info;
    tcePoseOriginState_t olderOrigin,newerOrigin;vec3_t origin;
    index=cent==&cg.predictedPlayerEntity?MAX_GENTITIES:cent->currentState.number;
    if(index<0||index>MAX_GENTITIES||!cg.snap) return 0;
    info=CG_ConfigString(CS_SERVERINFO);
    if(strcmp(Info_ValueForKey(info,"tce_poseProtocol"),"2")||!PresentationProfile(character)||!presentationDigest||
       presentationDigest!=BG_TCEPosePresentationManifestDigest(CG_ConfigString(TCE_POSE_PRESENTATION_CS)))goto finish;
    step=(uint32_t)atoi(Info_ValueForKey(info,"tce_poseStep"));
    if(!step||step>TCE_POSE_PRESENTATION_MAX_GAP)goto finish;
    state=&cent->currentState;
    if(cent==&cg.predictedPlayerEntity) {
        copy=cg.snap->ps;memset(&own[0],0,sizeof(own[0]));BG_PlayerStateToEntityStateExtraPolate(&copy,&own[0],cg.snap->serverTime,qfalse);
        own[0].time=cg.snap->ps.holdable[TCE_POSE_COMPACT_LEGS_SLOT];own[0].time2=cg.snap->ps.holdable[TCE_POSE_COMPACT_TORSO_SLOT];state=&own[0];
        if(cg.nextSnap) {
            copy=cg.nextSnap->ps;memset(&own[1],0,sizeof(own[1]));BG_PlayerStateToEntityStateExtraPolate(&copy,&own[1],cg.nextSnap->serverTime,qfalse);
            own[1].time=cg.nextSnap->ps.holdable[TCE_POSE_COMPACT_LEGS_SLOT];own[1].time2=cg.nextSnap->ps.holdable[TCE_POSE_COMPACT_TORSO_SLOT];next=&own[1];
        }
    }else if(cent->interpolate&&cg.nextSnap)next=&cent->nextState;
    if(state->clientNum<0||state->clientNum>=MAX_CLIENTS||!cgs.clientinfo[state->clientNum].infoValid)goto finish;
    memset(&selector,0,sizeof(selector));selector.currentState=*state;
    animationCharacter=CG_CharacterForClientinfo(&cgs.clientinfo[state->clientNum],&selector);
    renderCharacter=(state->effect1Time&0x60)?BG_GetCharacter(cgs.clientinfo[state->clientNum].team,6):animationCharacter;
    if(renderCharacter!=character||!PresentationInput(state,character,animationCharacter,&input,animation,speed)||
       !PresentationCode(state->time,&code[0])||!PresentationCode(state->time2,&code[1]))goto finish;
    from=(uint32_t)cg.snap->serverTime;to=(uint32_t)cg.time;gap=to-from;
    if(gap>step+1u||gap>TCE_POSE_PRESENTATION_MAX_GAP)goto finish;
    if(next) {
        uint32_t interval=(uint32_t)cg.nextSnap->serverTime-from;
        /* A hidden publication/rate segment is not reconstructible. Rebase at
         * the fresh snapshot instead of inventing packet-loss parity. */
        if(!interval||interval>step+1u||interval>TCE_POSE_PRESENTATION_MAX_GAP)goto finish;
        if(next->clientNum==state->clientNum&&next->legsAnim==state->legsAnim&&next->torsoAnim==state->torsoAnim&&
           !((next->eFlags^state->eFlags)&EF_TELEPORT_BIT)) {
            const bg_character_t *nextAnimationCharacter,*nextRenderCharacter;
            tcePosePresentationAxesInput_t mixed;
            selector.currentState=*next;
            nextAnimationCharacter=CG_CharacterForClientinfo(&cgs.clientinfo[next->clientNum],&selector);
            nextRenderCharacter=(next->effect1Time&0x60)?BG_GetCharacter(cgs.clientinfo[next->clientNum].team,6):nextAnimationCharacter;
            if(nextRenderCharacter==renderCharacter&&nextAnimationCharacter==animationCharacter&&
               PresentationInput(next,character,nextAnimationCharacter,&newInput,nextAnimation,newSpeed)&&
               BG_TCEPosePresentationInterpolate(&input,&newInput,(float)((double)gap/interval),&mixed))input=mixed;
        }
    }
    for(i=0;i<2;++i) {
        /* Rate is held from the older published state, not interpolated. */
        rate[i]=BG_TCEPosePresentationRate(input.mode,state->pos.trDelta,speed[i]);PresentationClip(animation[i],&clip[i]);
        if(!BG_TCEPoseCompactSample(&clip[i],code[i],from,to,rate[i],&sample[i]))goto finish;
    }
    if(!BG_TCEPosePresentationAxes(&input,&axes))goto finish;
    if(cent==&cg.predictedPlayerEntity) {
        /* Accepted own-body policy: shared authoritative phases and axes,
         * ordinary predicted translation/camera/weapon remain unchanged. */
        VectorCopy(cent->lerpOrigin,origin);
    } else {
        BG_TCEPoseOriginState(state,&olderOrigin);
        if(next)BG_TCEPoseOriginState(next,&newerOrigin);
        if(!BG_TCEPosePresentationOrigin(&olderOrigin,next?&newerOrigin:NULL,from,
            next?(uint32_t)cg.nextSnap->serverTime:0,to,PresentationMoverSnapshot,cg.snap,origin))goto finish;
    }
    VectorCopy(origin,body->origin);
    AxisCopy(axes.body,body->axis);AxisCopy(axes.torso,body->torsoAxis);AxisCopy(axes.head,head->axis);
    body->oldframe=sample[0].oldFrame;body->frame=sample[0].frame;body->backlerp=sample[0].backlerp;
    body->oldframeModel=body->frameModel=animation[0]->mdxFile;
    body->oldTorsoFrame=sample[1].oldFrame;body->torsoFrame=sample[1].frame;body->torsoBacklerp=sample[1].backlerp;
    body->oldTorsoFrameModel=body->torsoFrameModel=animation[1]->mdxFile;
    PresentationLerp(&cent->pe.legs,animation[0],state->legsAnim,&sample[0],rate[0],cg.time);
    PresentationLerp(&cent->pe.torso,animation[1],state->torsoAnim,&sample[1],rate[1],cg.time);
    success=1;
finish:
    if(!success&&presentationActive[index]) {
        /* Explicit affected exit cut; ordinary-to-ordinary stays untouched. */
        memset(&cent->pe.legs,0,sizeof(cent->pe.legs));memset(&cent->pe.torso,0,sizeof(cent->pe.torso));
    }
    presentationActive[index]=(unsigned char)success;return success;
}
void CG_TCEPresentationBeginOrigin(centity_t *cent,const vec3_t origin,tcePresentationOriginScope_t *scope) {
    scope->active=0;
    if(cent->lerpOrigin[0]==origin[0]&&cent->lerpOrigin[1]==origin[1]&&cent->lerpOrigin[2]==origin[2])return;
    VectorCopy(cent->lerpOrigin,scope->origin);scope->visible=cent->tceVisible;
    scope->visibilityUntil=cent->tceVisibilityUntil;scope->active=1;
    VectorCopy(origin,cent->lerpOrigin);
    /* An old positive cache belongs to a different world location. Re-run
     * actual occlusion rather than treating every corrected body as visible. */
    cent->tceVisible=0;cent->tceVisibilityUntil=cg.time;
    CG_CalcEntityVisibility(cent);
}
void CG_TCEPresentationEndOrigin(centity_t *cent,tcePresentationOriginScope_t *scope) {
    if(!scope->active)return;
    VectorCopy(scope->origin,cent->lerpOrigin);cent->tceVisible=scope->visible;
    cent->tceVisibilityUntil=scope->visibilityUntil;scope->active=0;
}
