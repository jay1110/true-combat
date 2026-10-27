#include "g_local.h"
#include "g_tce_pose.h"
#include "g_tce_pose_presentation.h"
#include <limits.h>
#include <stdlib.h>

enum { PRESENT_HISTORY=64 };
typedef struct {
    g_tcePresentationQuery_t query;
    tcePosePresentationAxesInput_t input;
    uint32_t step,slotGeneration;
    trajectory_t positionTrajectory;
    int splinePath,groundEntityNum,moverFrame,originValid;
    int legacy,gateOff,position;
} presentationRecord_t;
typedef struct {
    presentationRecord_t records[PRESENT_HISTORY];
    unsigned int head,count;
    uint32_t generation;
} presentationSlot_t;
static presentationSlot_t presentationSlots[MAX_CLIENTS];
/* At most one compact node per real entity per retained publication frame.
 * Shared by all 64 clients, rather than copying every ancestor into each
 * client's record. Queries never read current mover entities. */
typedef struct {
    uint32_t time,map;
    tcePoseMoverState_t state[ENTITYNUM_MAX_NORMAL];
    unsigned char present[ENTITYNUM_MAX_NORMAL];
} presentationMoverFrame_t;
static presentationMoverFrame_t presentationMovers[PRESENT_HISTORY];
static unsigned int presentationMoverHead;
static vmCvar_t presentationProtocol,presentationStep;
static uint32_t presentationMap,presentationFrame;
static int presentationRegistered,presentationFrameValid,presentationEnabled;
static uint32_t presentationAcknowledged[MAX_CLIENTS],presentationDigest;
static uint32_t presentationChallenges[MAX_CLIENTS];
static char presentationManifest[TCE_POSE_PRESENTATION_MANIFEST];

static int PresentationIndex(const gentity_t *ent) {
    int i;for(i=0;i<MAX_CLIENTS;++i)if(ent==&g_entities[i])return i;return -1;
}
static int PresentationBefore(uint32_t a,uint32_t b) {return b-a<=INT32_MAX;}
void G_TCEPresentationClear(gentity_t *ent) {
    if(!ent)return;
    ent->s.time=ent->s.time2=0;
    if(ent->client)ent->client->ps.holdable[TCE_POSE_COMPACT_LEGS_SLOT]=
        ent->client->ps.holdable[TCE_POSE_COMPACT_TORSO_SLOT]=0;
}
void G_TCEPresentationResetClient(gentity_t *ent) {
    int index=PresentationIndex(ent);G_TCEPresentationClear(ent);
    if(index>=0) {
        uint32_t generation=presentationSlots[index].generation+1u;if(!generation)generation=1;
        memset(&presentationSlots[index],0,sizeof(presentationSlots[index]));presentationSlots[index].generation=generation;
    }
}
void G_TCEPresentationUserinfo(int clientNum,const char *userinfo) {
    const char *ack;uint32_t value=0,nonce=0;int i;
    if(clientNum<0||clientNum>=MAX_CLIENTS)return;
    presentationAcknowledged[clientNum]=0;
    ack=Info_ValueForKey(userinfo,"tce_pose");
    if(ack[0]!='2'||ack[1]!=':')return;
    for(i=2;i<10;++i) {
        unsigned char c=(unsigned char)ack[i];unsigned int digit;
        if(c>='0'&&c<='9')digit=c-'0';else if(c>='a'&&c<='f')digit=c-'a'+10u;else return;
        value=(value<<4)|digit;
    }
    if(ack[10]!=':')return;
    for(i=11;i<19;++i) {
        unsigned char c=(unsigned char)ack[i];unsigned int digit;
        if(c>='0'&&c<='9')digit=c-'0';else if(c>='a'&&c<='f')digit=c-'a'+10u;else return;
        nonce=(nonce<<4)|digit;
    }
    if(!ack[19]&&nonce&&nonce==presentationChallenges[clientNum])presentationAcknowledged[clientNum]=value;
}
uint32_t G_TCEPresentationChallenge(int clientNum) {
    return clientNum>=0&&clientNum<MAX_CLIENTS?presentationChallenges[clientNum]:0;
}
void G_TCEPresentationConnect(int clientNum) {
    char previous[32],next[32],*end;uint32_t value;unsigned long parsed;qtime_t now;vmCvar_t counter;
    if(clientNum<0||clientNum>=MAX_CLIENTS)return;
    trap_Cvar_Register(&counter,"tce_poseChallengeCounter","0",CVAR_ROM);
    trap_Cvar_VariableStringBuffer("tce_poseChallengeCounter",previous,sizeof(previous));
    parsed=strtoul(previous,&end,10);
    value=*previous&&!*end&&parsed<=UINT32_MAX?(uint32_t)parsed:0;
    if(!value) {
        trap_RealTime(&now);
        value=(uint32_t)trap_Milliseconds()^((uint32_t)now.tm_year*31536000u+
            (uint32_t)now.tm_yday*86400u+(uint32_t)now.tm_hour*3600u+
            (uint32_t)now.tm_min*60u+(uint32_t)now.tm_sec);
    }
    ++value;if(!value)++value;
    Com_sprintf(next,sizeof(next),"%u",(unsigned)value);trap_Cvar_Set("tce_poseChallengeCounter",next);
    presentationChallenges[clientNum]=value;presentationAcknowledged[clientNum]=0;
    G_TCEPresentationResetClient(&g_entities[clientNum]);
}
void G_TCEPresentationReset(void) {
    memset(presentationSlots,0,sizeof(presentationSlots));presentationFrameValid=0;
    memset(presentationMovers,0,sizeof(presentationMovers));presentationMoverHead=0;
    presentationManifest[0]=0;presentationDigest=0;presentationEnabled=0;
}
static void PresentationFrame(void) {
    g_tcePoseStats_t stats;int i,step,count;char number[16],manifest[TCE_POSE_PRESENTATION_MANIFEST];
    uint32_t profiles[TCE_POSE_PRESENTATION_PROFILES],frameStep;
    G_TCEPoseStats(&stats);
    if(presentationMap!=stats.mapGeneration) {G_TCEPresentationReset();presentationMap=stats.mapGeneration;}
    if(presentationFrameValid&&presentationFrame==(uint32_t)level.time)return;
    presentationFrame=(uint32_t)level.time;presentationFrameValid=1;
    presentationMoverHead=(presentationMoverHead+1u)%PRESENT_HISTORY;
    memset(&presentationMovers[presentationMoverHead],0,sizeof(presentationMovers[0]));
    presentationMovers[presentationMoverHead].time=presentationFrame;
    presentationMovers[presentationMoverHead].map=presentationMap;
    if(!presentationRegistered) {
        trap_Cvar_Register(&presentationProtocol,"tce_poseProtocol","2",CVAR_SERVERINFO|CVAR_ROM);
        trap_Cvar_Register(&presentationStep,"tce_poseStep","50",CVAR_SERVERINFO|CVAR_ROM);
        presentationRegistered=1;
    }
    frameStep=(uint32_t)level.time-(uint32_t)level.previousTime;
    step=frameStep&&frameStep<=TCE_POSE_PRESENTATION_MAX_GAP?(int)frameStep:50;
    Com_sprintf(number,sizeof(number),"%d",step);trap_Cvar_Set("tce_poseStep",number);
    count=G_TCEPosePresentationProfiles(profiles,TCE_POSE_PRESENTATION_PROFILES);
    if(!BG_TCEPosePresentationManifest(profiles,count,manifest,sizeof(manifest)))manifest[0]=0;
    if(strcmp(presentationManifest,manifest)) {
        memcpy(presentationManifest,manifest,strlen(manifest)+1);
        presentationDigest=BG_TCEPosePresentationManifestDigest(manifest);
        trap_SetConfigstring(TCE_POSE_PRESENTATION_CS,manifest);
    }
    presentationEnabled=stats.initialized&&presentationDigest;
    for(i=0;i<MAX_CLIENTS&&presentationEnabled;++i) {
        const gentity_t *ent=&g_entities[i];
        if(!ent->inuse||!ent->client||ent->client->pers.connected!=CON_CONNECTED||
           (ent->r.svFlags&SVF_BOT))continue;
        if(presentationAcknowledged[i]!=presentationDigest)presentationEnabled=0;
    }
}
static void PresentationClip(const animation_t *animation,tcePosePresentationClip_t *clip) {
    clip->firstFrame=animation->firstFrame;clip->numFrames=animation->numFrames;
    clip->loopFrames=animation->loopFrames;clip->frameLerp=animation->frameLerp;
    clip->initialLerp=animation->initialLerp;clip->reversed=!!(animation->flags&ANIMFL_REVERSED);
}
static int PresentationSame(const g_tcePresentationQuery_t *a,const g_tcePresentationQuery_t *b) {
    return a->mode&&a->mode==b->mode&&a->generation==b->generation&&a->mapGeneration==b->mapGeneration&&
        a->profile==b->profile&&!((a->eFlags^b->eFlags)&EF_TELEPORT_BIT)&&
        a->renderCharacter==b->renderCharacter&&a->animationCharacter==b->animationCharacter&&
        a->animation[0]==b->animation[0]&&a->animation[1]==b->animation[1];
}
static int PresentationMoverCapture(void *user,int number,tcePoseMoverState_t *out) {
    presentationMoverFrame_t *frame=(presentationMoverFrame_t *)user;
    if(number<0||number>=ENTITYNUM_MAX_NORMAL)return 0;
    if(!frame->present[number]) {
        const gentity_t *ent=&g_entities[number];
        frame->present[number]=2;
        if(ent->inuse&&ent->s.number==number) {
            BG_TCEPoseMoverState(&ent->s,&frame->state[number]);frame->present[number]=1;
        }
    }
    if(frame->present[number]!=1)return 0;
    *out=frame->state[number];return 1;
}
static int PresentationMoverRead(void *user,int number,tcePoseMoverState_t *out) {
    const presentationMoverFrame_t *frame=(const presentationMoverFrame_t *)user;
    if(number<0||number>=ENTITYNUM_MAX_NORMAL||frame->present[number]!=1)return 0;
    *out=frame->state[number];return 1;
}
void G_TCEPresentationPublish(gentity_t *ent) {
    g_tcePosePresentationMetadata_t metadata;presentationSlot_t *slot;presentationRecord_t next,*previous=NULL;
    uint32_t time=(uint32_t)level.time,elapsed;int index=PresentationIndex(ent),i,mode;
    if(index<0||!ent->client||ent->client->tceRewind.active)return;
    G_TCEPresentationClear(ent);PresentationFrame();slot=&presentationSlots[index];memset(&next,0,sizeof(next));
    next.query.clientNum=index;next.query.sourceTime=next.query.serverTime=time;
    next.slotGeneration=slot->generation;next.query.mapGeneration=presentationMap;
    if(ent->s.eType==ET_PLAYER&&ent->health>0) {
        next.position=1;VectorCopy(ent->s.pos.trBase,next.query.origin);next.query.eFlags=ent->s.eFlags;
    }
    elapsed=time-(uint32_t)level.previousTime;next.step=elapsed&&elapsed<=TCE_POSE_PRESENTATION_MAX_GAP?elapsed:50u;
    if(slot->count)previous=&slot->records[(slot->head+PRESENT_HISTORY-1)%PRESENT_HISTORY];
    if(!presentationEnabled) {next.legacy=next.gateOff=1;goto store;}
    if(ent->s.eType!=ET_PLAYER||ent->health<=0||
       (ent->s.eFlags&(EF_MOUNTEDTANK|EF_MG42_ACTIVE|EF_AAGUN_ACTIVE))||
       !G_TCEPosePresentationMetadata(ent,&metadata))goto store;
    if(metadata.fullAnimation[0]!=ent->s.legsAnim||metadata.fullAnimation[1]!=ent->s.torsoAnim)goto store;
    mode=BG_TCEPosePresentationMode(metadata.animation[0]->name,metadata.animation[1]->name,
                                   !!(ent->s.effect1Time&4),!!(ent->s.effect1Time&0x400));
    next.query.mapGeneration=metadata.mapGeneration;next.query.generation=metadata.generation;
    next.query.profile=metadata.profile;next.query.mode=mode;next.query.eFlags=ent->s.eFlags;
    next.query.renderCharacter=metadata.renderCharacter;next.query.animationCharacter=metadata.animationCharacter;
    if(!mode) {next.legacy=1;goto store;}
    /* The ordinary interpolating converter intentionally leaves trDelta
     * untouched. Affected policy explicitly publishes authoritative velocity
     * for both g_smoothClients modes, matching the own-PS conversion. */
    VectorCopy(ent->client->ps.velocity,ent->s.pos.trDelta);
    VectorCopy(ent->s.pos.trBase,next.query.origin);next.input.mode=(tcePosePresentationMode_t)mode;
    VectorCopy(ent->s.apos.trBase,next.input.aim);VectorCopy(ent->s.pos.trDelta,next.input.velocity);
    next.input.movementYaw=ent->s.angles2[YAW];next.input.lean=ent->s.angles2[ROLL];
    if(next.input.movementYaw>128&&next.input.movementYaw<=255)next.input.movementYaw-=256;
    next.input.freelook[0]=ent->s.angles[PITCH];next.input.freelook[1]=ent->s.angles[YAW];
    next.input.scale=g_newbbox.integer?1.25f:1.0f;next.input.leanMode=g_leanmode.integer>0;
    for(i=0;i<2;++i) {
        uint64_t phase=0;
        next.query.acceptedEpoch[i]=metadata.acceptedEpoch[i];next.query.animation[i]=metadata.fullAnimation[i];
        PresentationClip(metadata.animation[i],&next.query.clip[i]);
        if(metadata.moveSpeed[i]<0)goto unavailable;
        next.query.rateQ8[i]=BG_TCEPosePresentationRate(mode,next.input.velocity,metadata.moveSpeed[i]);
        elapsed=previous?time-previous->query.sourceTime:UINT32_MAX;
        if(previous&&previous->query.mode==mode&&previous->query.generation==metadata.generation&&
           previous->query.mapGeneration==metadata.mapGeneration&&previous->query.profile==metadata.profile&&
           previous->query.acceptedEpoch[i]==metadata.acceptedEpoch[i]&&
           previous->query.animation[i]==metadata.fullAnimation[i]&&elapsed<=previous->step+1u&&
           elapsed<=TCE_POSE_PRESENTATION_MAX_GAP) {
            tcePoseCompactSample_t sample;
            if(!BG_TCEPoseCompactSample(&previous->query.clip[i],previous->query.code[i],previous->query.sourceTime,
                                        time,previous->query.rateQ8[i],&sample))goto unavailable;
            phase=sample.elapsedQ16;
        }
        if(!BG_TCEPoseCompactPack(&next.query.clip[i],phase,&next.query.code[i])||
           !BG_TCEPoseCompactSample(&next.query.clip[i],next.query.code[i],time,time,next.query.rateQ8[i],&next.query.channel[i]))goto unavailable;
    }
    if(!BG_TCEPosePresentationAxes(&next.input,&next.query.axes))goto unavailable;
    ent->s.time=(int)next.query.code[0]-65536;ent->s.time2=(int)next.query.code[1]-65536;
    ent->client->ps.holdable[TCE_POSE_COMPACT_LEGS_SLOT]=ent->s.time;
    ent->client->ps.holdable[TCE_POSE_COMPACT_TORSO_SLOT]=ent->s.time2;
    goto store;
unavailable:
    next.query.mode=0;next.query.code[0]=next.query.code[1]=0;
store:
    next.positionTrajectory=ent->s.pos;next.splinePath=ent->s.effect2Time;
    next.groundEntityNum=ent->s.groundEntityNum;next.moverFrame=(int)presentationMoverHead;next.originValid=1;
    if(next.query.mode&&next.position&&next.groundEntityNum>0&&next.groundEntityNum<ENTITYNUM_MAX_NORMAL) {
        tcePoseMoverState_t mover;vec3_t origin,angles;
        presentationMoverFrame_t *frame=&presentationMovers[presentationMoverHead];
        if(!PresentationMoverCapture(frame,next.groundEntityNum,&mover)||
           (mover.type==ET_MOVER&&!BG_TCEPoseMoverEvaluate(next.groundEntityNum,time,
               PresentationMoverCapture,frame,origin,angles)))next.originValid=0;
    }
    if(previous&&previous->query.sourceTime==time)*previous=next;
    else {slot->records[slot->head]=next;slot->head=(slot->head+1)%PRESENT_HISTORY;if(slot->count<PRESENT_HISTORY)++slot->count;}
}
int G_TCEPresentationQuery(int clientNum,uint32_t serverTime,uint32_t receiptCeiling,g_tcePresentationQuery_t *out) {
    const presentationSlot_t *slot;const presentationRecord_t *base=NULL,*next=NULL;
    tcePosePresentationAxesInput_t input;g_tcePoseAssetInfo_t render,animation;
    g_tcePoseStats_t stats;
    unsigned int i;uint32_t elapsed,mapGeneration,generation,profile;int channel;
    tcePoseOriginState_t olderOrigin,newerOrigin,*nextOrigin=NULL;
    const presentationMoverFrame_t *moverFrame;
    if(out)memset(out,0,sizeof(*out));
    if(!out||clientNum<0||clientNum>=MAX_CLIENTS)return G_TCE_PRESENTATION_UNKNOWN;
    if(!presentationFrameValid)return G_TCE_PRESENTATION_LEGACY;
    if(!PresentationBefore(serverTime,receiptCeiling))return G_TCE_PRESENTATION_UNKNOWN;
    G_TCEPoseStats(&stats);
    slot=&presentationSlots[clientNum];
    for(i=0;i<slot->count;++i) {
        const presentationRecord_t *record=&slot->records[(slot->head+PRESENT_HISTORY-1-i)%PRESENT_HISTORY];
        if(!PresentationBefore(record->query.sourceTime,receiptCeiling))continue;
        if(PresentationBefore(record->query.sourceTime,serverTime)) {base=record;break;}
        next=record;
    }
    if(!base||base->query.mapGeneration!=stats.mapGeneration||base->slotGeneration!=slot->generation)return G_TCE_PRESENTATION_UNKNOWN;
    elapsed=serverTime-base->query.sourceTime;
    if(elapsed>base->step+1u||elapsed>TCE_POSE_PRESENTATION_MAX_GAP)return G_TCE_PRESENTATION_UNKNOWN;
    if(base->gateOff)return G_TCE_PRESENTATION_LEGACY;
    if(!G_TCEPosePresentationIdentity(&g_entities[clientNum],&mapGeneration,&generation)||
       base->query.mapGeneration!=mapGeneration||base->query.generation!=generation)return G_TCE_PRESENTATION_UNKNOWN;
    if(base->legacy)return G_TCE_PRESENTATION_LEGACY;
    if(!base->query.mode)return G_TCE_PRESENTATION_UNKNOWN;
    if(!G_TCEPoseAssetInfo(base->query.renderCharacter,&render)||
       !G_TCEPoseAssetInfo(base->query.animationCharacter,&animation))return G_TCE_PRESENTATION_UNKNOWN;
    profile=base->query.renderCharacter==base->query.animationCharacter?render.profile:
        BG_TCEPosePresentationPairProfile(render.profile,animation.rawProfile);
    if(base->query.profile!=profile)return G_TCE_PRESENTATION_UNKNOWN;
    *out=base->query;out->serverTime=serverTime;input=base->input;
    if(next&&PresentationSame(&base->query,&next->query)&&next->query.sourceTime-base->query.sourceTime<=base->step+1u) {
        float fraction=(float)((double)elapsed/(next->query.sourceTime-base->query.sourceTime));
        BG_TCEPosePresentationInterpolate(&base->input,&next->input,fraction,&input);
    }
    /* Translation belongs to the snapshot interpolation contract, not to an
     * animation key. Clip/stance transitions do not stop a visible player. */
    memset(&olderOrigin,0,sizeof(olderOrigin));olderOrigin.position=base->positionTrajectory;
    olderOrigin.number=clientNum;olderOrigin.ground=base->groundEntityNum;olderOrigin.spline=base->splinePath;
    if(next&&base->position&&next->position&&base->slotGeneration==next->slotGeneration&&
       base->query.mapGeneration==next->query.mapGeneration&&
       !((base->query.eFlags^next->query.eFlags)&EF_TELEPORT_BIT)&&
       next->query.sourceTime-base->query.sourceTime<=base->step+1u) {
        memset(&newerOrigin,0,sizeof(newerOrigin));newerOrigin.position=next->positionTrajectory;
        newerOrigin.number=clientNum;newerOrigin.ground=next->groundEntityNum;newerOrigin.spline=next->splinePath;
        nextOrigin=&newerOrigin;
    }
    if(!base->position||!base->originValid||base->moverFrame<0||base->moverFrame>=PRESENT_HISTORY)goto failure;
    moverFrame=&presentationMovers[base->moverFrame];
    if(moverFrame->time!=base->query.sourceTime||moverFrame->map!=base->query.mapGeneration||
       !BG_TCEPosePresentationOrigin(&olderOrigin,nextOrigin,base->query.sourceTime,
           next?next->query.sourceTime:0,serverTime,PresentationMoverRead,(void *)moverFrame,out->origin))goto failure;
    for(channel=0;channel<2;++channel)if(!BG_TCEPoseCompactSample(&out->clip[channel],out->code[channel],
        out->sourceTime,serverTime,out->rateQ8[channel],&out->channel[channel]))goto failure;
    if(BG_TCEPosePresentationAxes(&input,&out->axes))return G_TCE_PRESENTATION_AFFECTED;
failure:
    memset(out,0,sizeof(*out));return G_TCE_PRESENTATION_UNKNOWN;
}
