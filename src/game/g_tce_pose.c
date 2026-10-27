#include "g_local.h"
#include "g_tce_pose.h"
#include "g_tce_pose_presentation.h"
#include <float.h>
#include <limits.h>

typedef struct {
    const bg_character_t *character;
    const animModelInfo_t *model;
    const tcePoseMesh_t *mesh;
    int ready;
    int animationCount, usableAnimations;
    /* Per-clip readiness: one unsupported declaration must not disable the
     * other clips or prevent later MDX paths from being registered. */
    signed char animationModels[MAX_MODEL_ANIMATIONS];
    g_tcePoseAssetInfo_t assets;
    const tcePoseSkinMesh_t *skin;
    const tcePoseHeadMesh_t *head;
    int surfaceReady;
    int moveSpeed[MAX_MODEL_ANIMATIONS];
    unsigned char speedReady[MAX_MODEL_ANIMATIONS];
} g_tcePoseCharacter_t;
typedef char poseModelIDsFitSignedChar[TCE_POSE_MAX_SKELETONS<=SCHAR_MAX?1:-1];
typedef struct {
    uint64_t sequence;
    uint32_t commandTime, receiptTime, acceptedEpoch;
    int channel, previous, animation, context;
    tcePoseClock_t clock;
} g_tcePoseEvent_t;
typedef struct {
    uint32_t generation;
    int occupied, connectTime, spawnCount, character, inCommand;
    const bg_character_t *characterPointer;
    tcePoseClock_t clocks[2];
    uint32_t acceptedEpoch[2];
    uint32_t acceptedCommandTime[2], acceptedReceiptTime[2];
    int context[2];
    uint64_t sequence;
    unsigned int eventHead, eventCount, historyHead, historyCount;
    g_tcePoseEvent_t events[G_TCE_POSE_EVENTS];
    g_tcePoseHistory_t history[G_TCE_POSE_HISTORY];
} g_tcePoseClient_t;

/* Deliberately module-private storage, independent of SDK entity/client sizes.
 * No diagnostic commands or mutation interface are registered in Game. */
static tcePoseRegistry_t poseRegistry;
static tcePoseSurfaceRegistry_t poseSurfaces;
static tcePoseMath_t poseMath;
static tcePoseSkin_t poseSurfaceScratch;
static g_tcePoseCharacter_t poseCharacters[G_TCE_POSE_CHARACTERS];
static g_tcePoseClient_t poseClients[MAX_CLIENTS];
typedef struct {
    char path[MAX_QPATH];
    unsigned char *bytes;
    size_t size;
    uint64_t fingerprint;
} g_tcePoseBuffer_t;
static g_tcePoseBuffer_t poseBuffers[TCE_POSE_MAX_SKELETONS+TCE_POSE_MAX_MESHES+TCE_POSE_SURFACE_MAX_HEADS];
static unsigned int poseBufferCount;
static g_tcePoseStats_t poseStats;
static uint32_t poseGeneration;
static unsigned char *pendingBuffer;
static char pendingPath[MAX_QPATH];
static size_t pendingSize;

static uint32_t PoseGeneration(void) {
    if (++poseGeneration == 0) ++poseGeneration;
    return poseGeneration;
}
static int PoseAtOrBefore(uint32_t a, uint32_t b) { return b-a <= INT32_MAX; }
static int PosePath(const char *path, char out[MAX_QPATH]) {
    unsigned int i;
    if (!path) return 0;
    for (i=0; i<MAX_QPATH; ++i) {
        unsigned char ch=(unsigned char)path[i];
        if (!ch) {
            out[i]=0;
            return i && out[0]!='/' && !strstr(out,"..") && !strchr(out,':');
        }
        if (ch<32 || ch>126) return 0;
        out[i]=ch=='\\' ? '/' : (char)(ch>='A' && ch<='Z' ? ch+'a'-'A' : ch);
    }
    return 0;
}
static int PoseLoad(void *unused, const char *name, const unsigned char **bytes, size_t *size) {
    fileHandle_t handle=0;
    int length;
    unsigned int i;
    char canonical[MAX_QPATH]={0};
    (void)unused;
    *bytes=NULL; *size=0;
    if(!PosePath(name,canonical)) return 0;
    for(i=0;i<poseBufferCount;++i) if(!strcmp(poseBuffers[i].path,canonical)) {
        *bytes=poseBuffers[i].bytes; *size=poseBuffers[i].size; return 1;
    }
    /* A single synchronous registration owns this pending buffer. Include it
     * in the cap before allocating, even when parsing later rejects the file. */
    if (pendingBuffer || poseBufferCount >= sizeof(poseBuffers)/sizeof(poseBuffers[0])) return 0;
    length=trap_FS_FOpenFile(name,&handle,FS_READ);
    if (!handle || length<=0 || length>TCE_POSE_MAX_ASSET_BYTES ||
        poseStats.ownedBytes>TCE_POSE_MAX_CACHE_BYTES ||
        (size_t)length>TCE_POSE_MAX_CACHE_BYTES-poseStats.ownedBytes) {
        if (handle) trap_FS_FCloseFile(handle);
        return 0;
    }
    pendingBuffer=(unsigned char*)malloc((size_t)length);
    if (pendingBuffer) {
        memcpy(pendingPath,canonical,sizeof(pendingPath)); pendingSize=(size_t)length;
        trap_FS_Read(pendingBuffer,length,handle);
        *bytes=pendingBuffer; *size=(size_t)length;
    }
    trap_FS_FCloseFile(handle);
    return pendingBuffer!=NULL;
}
static void PoseFinishLoad(tcePoseStatus_t status) {
    if (!pendingBuffer) return;
    if (status==TCE_POSE_OK) {
        g_tcePoseBuffer_t *buffer=&poseBuffers[poseBufferCount++];
        size_t i;
        buffer->bytes=pendingBuffer; buffer->size=pendingSize;
        memcpy(buffer->path,pendingPath,sizeof(buffer->path));
        buffer->fingerprint=UINT64_C(14695981039346656037);
        for(i=0;i<pendingSize;++i) { buffer->fingerprint^=pendingBuffer[i]; buffer->fingerprint*=UINT64_C(1099511628211); }
        poseStats.ownedBytes+=pendingSize;
    } else free(pendingBuffer);
    pendingBuffer=NULL; pendingSize=0; memset(pendingPath,0,sizeof(pendingPath));
}
static const tcePoseSkeleton_t *PoseModel(const char *path, int load) {
    char canonical[MAX_QPATH];
    const tcePoseSkeleton_t *model=NULL;
    tcePoseStatus_t status;
    if (!PosePath(path,canonical)) return NULL;
    status=TCE_PoseRegistrySkeleton(&poseRegistry,canonical,load?PoseLoad:NULL,NULL,&model);
    PoseFinishLoad(status);
    return status==TCE_POSE_OK?model:NULL;
}
static int PoseModelID(const tcePoseSkeleton_t *model) {
    unsigned int i;
    for(i=0;i<poseRegistry.skeletonCount;++i) if(model==&poseRegistry.skeletons[i].model) return (int)i;
    return -1;
}
static int PoseCharacterIndex(const bg_character_t *character) {
    unsigned int i;
    for(i=0;i<poseStats.characters;++i) if(poseCharacters[i].character==character) return (int)i;
    return -1;
}
static uint64_t PoseFingerprint(const char *path) {
    unsigned int i;
    for(i=0;i<poseBufferCount;++i) if(!strcmp(poseBuffers[i].path,path)) return poseBuffers[i].fingerprint;
    return 0;
}
typedef struct {
    tcePoseAttachment_t attachment;
    tcePoseInput_t input;
} g_tcePoseFeet_t;
static int PoseFeet(void *user,int absoluteFrame,float feet[2][3]) {
    g_tcePoseFeet_t *context=(g_tcePoseFeet_t *)user;
    tcePoseResult_t result;
    if(absoluteFrame<0) return 0;
    context->input.legs.currentFrame=context->input.legs.oldFrame=(unsigned int)absoluteFrame;
    context->input.torso.currentFrame=context->input.torso.oldFrame=(unsigned int)absoluteFrame;
    if(TCE_PoseEvaluate(&poseMath,&context->attachment,&context->input,&result)!=TCE_POSE_OK) return 0;
    memcpy(feet[0],result.model[TCE_POSE_FOOT_LEFT].origin,sizeof(feet[0]));
    memcpy(feet[1],result.model[TCE_POSE_FOOT_RIGHT].origin,sizeof(feet[1])); return 1;
}
static void PoseResolveSpeeds(g_tcePoseCharacter_t *entry) {
    unsigned int i;
    int animation,budget=32768;
    for(animation=0;animation<MAX_MODEL_ANIMATIONS;++animation) entry->moveSpeed[animation]=INT_MIN;
    /* Equal raw meshes plus the same immutable animation table imply equal
     * foot queries. Share the result without assuming a named mesh family. */
    for(i=0;i<poseStats.characters;++i) {
        const g_tcePoseCharacter_t *other=&poseCharacters[i];
        if(other!=entry && other->surfaceReady && other->model==entry->model && other->skin &&
           other->skin->byteCount==entry->skin->byteCount &&
           !memcmp(other->skin->bytes,entry->skin->bytes,entry->skin->byteCount)) {
            memcpy(entry->moveSpeed,other->moveSpeed,sizeof(entry->moveSpeed));
            memcpy(entry->speedReady,other->speedReady,sizeof(entry->speedReady)); return;
        }
    }
    for(animation=0;animation<entry->animationCount;++animation) {
        const animation_t *clip=entry->model->animations[animation];
        int modelID=entry->animationModels[animation],speed;
        g_tcePoseFeet_t context;
        if(!clip || modelID<0) continue;
        if(clip->moveSpeed>=0) {
            if(clip->moveSpeed<=8192) { entry->moveSpeed[animation]=clip->moveSpeed; entry->speedReady[animation]=1; }
            continue;
        }
        if(clip->numFrames>budget) continue;
        budget-=clip->numFrames;
        memset(&context,0,sizeof(context));
        context.input.legs.currentModel=context.input.legs.oldModel=&poseRegistry.skeletons[modelID].model;
        context.input.torso.currentModel=context.input.torso.oldModel=context.input.legs.currentModel;
        context.input.mode=TCE_POSE_MATH_OBSERVED_WINDOWS;
        for(i=0;i<3;++i) context.input.axis[i][i]=context.input.torsoAxis[i][i]=1;
        if(TCE_PosePrepareAttachment(&context.attachment,entry->mesh,context.input.legs.currentModel)==TCE_POSE_OK &&
           BG_TCEPosePresentationMoveSpeed(clip->firstFrame,clip->numFrames,clip->frameLerp,
               !!(clip->flags&ANIMFL_LADDERANIM),PoseFeet,&context,&speed)) {
            entry->moveSpeed[animation]=speed; entry->speedReady[animation]=1;
        }
    }
}
static int PoseRegisterSurfaces(g_tcePoseCharacter_t *entry,const bg_characterDef_t *definition) {
    g_tcePoseAssetInfo_t info;
    const tcePoseSkeleton_t *model=NULL;
    const tcePoseSkinMesh_t *skin=NULL;
    const tcePoseHeadMesh_t *head=NULL;
    tcePoseStatus_t status;
    char group[MAX_QPATH]={0},script[MAX_QPATH]={0};
    unsigned int i;
    int animation;
    memset(&info,0,sizeof(info));
    if(!entry->ready || !PosePath(entry->character->characterFile,info.characterFile) ||
       !PosePath(definition->mesh,info.mesh) || !PosePath(definition->hudhead,info.hudhead) ||
       !PosePath(entry->model->animationGroup,info.animationGroup) ||
       !PosePath(entry->model->animationScript,info.animationScript) ||
       !PosePath(definition->animationGroup,group) || !PosePath(definition->animationScript,script) ||
       strcmp(group,info.animationGroup) || strcmp(script,info.animationScript)) return 0;
    info.rawProfile=BG_TCEPosePresentationProfile(entry->character,definition);
    if(!info.rawProfile) return 0;
    for(animation=0;animation<entry->animationCount;++animation) if(entry->animationModels[animation]>=0) {
        model=&poseRegistry.skeletons[(unsigned int)entry->animationModels[animation]].model; break;
    }
    if(!model) return 0;
    status=TCE_PoseSurfaceRegistryBody(&poseSurfaces,info.mesh,model,PoseLoad,NULL,&skin); PoseFinishLoad(status);
    if(status!=TCE_POSE_OK) return 0;
    status=TCE_PoseSurfaceRegistryHead(&poseSurfaces,info.hudhead,PoseLoad,NULL,&head); PoseFinishLoad(status);
    if(status!=TCE_POSE_OK || !head->frameCount) return 0;
    info.mapGeneration=poseStats.mapGeneration;
    info.bodyFingerprint=PoseFingerprint(info.mesh); info.headFingerprint=PoseFingerprint(info.hudhead);
    entry->skin=skin; entry->head=head; PoseResolveSpeeds(entry);
    info.profile=BG_TCEPosePresentationSpeedProfile(info.rawProfile,entry->moveSpeed,entry->animationCount);
    if(!info.profile) return 0;
    /* Reject a hash collision between distinct canonical asset identities.
     * A profile is never a registry index or permission to substitute a mesh. */
    for(i=0;i<poseStats.characters;++i) {
        const g_tcePoseCharacter_t *other=&poseCharacters[i];
        if(other!=entry && other->surfaceReady && other->assets.profile==info.profile &&
           (strcmp(other->assets.characterFile,info.characterFile) || strcmp(other->assets.mesh,info.mesh) ||
            strcmp(other->assets.hudhead,info.hudhead) || strcmp(other->assets.animationGroup,info.animationGroup) ||
            strcmp(other->assets.animationScript,info.animationScript))) return 0;
    }
    entry->assets=info; entry->surfaceReady=1;
    return 1;
}
static int PoseClientNumber(const gentity_t *ent) {
    int i;
    if(!ent) return -1;
    /* Equality avoids pointer subtraction on unrelated entity pointers. */
    for(i=0;i<MAX_CLIENTS;++i) if(ent==&g_entities[i]) return i;
    return -1;
}
static int PoseIdentity(const g_tcePoseClient_t *slot,const gentity_t *ent) {
    return slot->occupied && ent->client && ent->inuse && ent->client->pers.connected==CON_CONNECTED &&
        slot->connectTime==ent->client->pers.connectTime &&
        slot->spawnCount==ent->client->ps.persistant[PERS_SPAWN_COUNT] &&
        slot->characterPointer==ent->client->pers.character;
}
void G_TCEPoseResetCharacters(void) {
    unsigned int i;
    for(i=0;i<poseBufferCount;++i) { free(poseBuffers[i].bytes); memset(&poseBuffers[i],0,sizeof(poseBuffers[i])); }
    free(pendingBuffer); pendingBuffer=NULL; poseBufferCount=0;
    pendingSize=0; memset(pendingPath,0,sizeof(pendingPath));
    TCE_PoseRegistryInit(&poseRegistry);
    TCE_PoseSurfaceRegistryInit(&poseSurfaces); memset(&poseSurfaceScratch,0,sizeof(poseSurfaceScratch));
    memset(poseCharacters,0,sizeof(poseCharacters));
    memset(poseClients,0,sizeof(poseClients));
    poseStats.characters=poseStats.models=poseStats.meshes=0; poseStats.ownedBytes=0;
    poseStats.mapGeneration=PoseGeneration();
}
void G_TCEPoseShutdown(void) {
    G_TCEPoseResetCharacters();
    poseStats.initialized=0;
    if(level.animScriptData.animationAssigned==G_TCEPoseAnimationAssigned)
        level.animScriptData.animationAssigned=NULL;
}
void G_TCEPoseInit(void) {
    G_TCEPoseShutdown();
    memset(&poseStats,0,sizeof(poseStats));
    poseStats.mapGeneration=PoseGeneration();
    poseStats.initialized=TCE_PoseMathInit(&poseMath)==TCE_POSE_OK;
}
int G_TCEPoseRegisterCharacter(const bg_character_t *character,const bg_characterDef_t *definition) {
    const tcePoseMesh_t *mesh=NULL;
    g_tcePoseCharacter_t *entry;
    tcePoseStatus_t status;
    char path[MAX_QPATH];
    int index,i;
    if(!poseStats.initialized || !character || !character->animModelInfo || !definition) return 0;
    index=PoseCharacterIndex(character);
    if(index<0) {
        if(poseStats.characters==G_TCE_POSE_CHARACTERS) return 0;
        index=(int)poseStats.characters++;
    }
    entry=&poseCharacters[index]; memset(entry,0,sizeof(*entry));
    entry->character=character; entry->model=character->animModelInfo;
    for(i=0;i<MAX_MODEL_ANIMATIONS;++i) entry->animationModels[i]=-1;
    /* Re-registration can replace assets at the same character address. */
    for(i=0;i<MAX_CLIENTS;++i) if(poseClients[i].occupied &&
        poseClients[i].characterPointer==character) G_TCEPoseResetClient(&g_entities[i]);
    if(!PosePath(definition->mesh,path)) return 0;
    status=TCE_PoseRegistryMesh(&poseRegistry,path,PoseLoad,NULL,&mesh); PoseFinishLoad(status);
    poseStats.models=poseRegistry.skeletonCount; poseStats.meshes=poseRegistry.meshCount;
    if(status || entry->model->numAnimations<1 || entry->model->numAnimations>MAX_MODEL_ANIMATIONS) return 0;
    entry->mesh=mesh; entry->animationCount=entry->model->numAnimations;
    for(i=0;i<entry->animationCount;++i) {
        const animation_t *anim=entry->model->animations[i];
        const tcePoseSkeleton_t *model;
        tcePoseClockClip_t clip;
        tcePoseAttachment_t attachment;
        int minimum,maximum;
        if(!anim) continue;
        model=PoseModel(anim->mdxFileName,1);
        if(!model) continue;
        memset(&clip,0,sizeof(clip)); clip.model=0; clip.firstFrame=anim->firstFrame;
        clip.numFrames=anim->numFrames; clip.loopFrames=anim->loopFrames;
        clip.frameLerp=anim->frameLerp; clip.initialLerp=anim->initialLerp;
        clip.reversed=!!(anim->flags&ANIMFL_REVERSED);
        if(!BG_TCEPoseClockFrameBounds(&clip,&minimum,&maximum) ||
            (unsigned int)maximum>=model->frameCount ||
            TCE_PosePrepareAttachment(&attachment,mesh,model)!=TCE_POSE_OK) continue;
        entry->animationModels[i]=(signed char)PoseModelID(model);
        ++entry->usableAnimations;
    }
    poseStats.models=poseRegistry.skeletonCount;
    entry->ready=entry->usableAnimations!=0;
    if(entry->ready) PoseRegisterSurfaces(entry,definition);
    return entry->ready;
}
void G_TCEPoseResetClient(gentity_t *ent) {
    int index=PoseClientNumber(ent),i;
    g_tcePoseClient_t *slot;
    if(index<0 || !poseStats.initialized || !ent->client || ent->client->tceRewind.active) return;
    slot=&poseClients[index]; memset(slot,0,sizeof(*slot));
    slot->generation=PoseGeneration(); slot->occupied=1;
    slot->characterPointer=ent->client->pers.character;
    slot->character=PoseCharacterIndex(slot->characterPointer);
    slot->connectTime=ent->client->pers.connectTime;
    slot->spawnCount=ent->client->ps.persistant[PERS_SPAWN_COUNT];
    for(i=0;i<2;++i) BG_TCEPoseClockReset(&slot->clocks[i],slot->generation,(uint32_t)ent->client->ps.commandTime);
}
void G_TCEPoseCharacterChanged(gclient_t *client) {
    int i;
    for(i=0;i<MAX_CLIENTS;++i) if(g_entities[i].client==client) { G_TCEPoseResetClient(&g_entities[i]); return; }
}
static g_tcePoseClient_t *PoseClient(gentity_t *ent) {
    int index=PoseClientNumber(ent);
    if(index<0 || !poseStats.initialized || !ent->inuse || !ent->client ||
        ent->client->pers.connected!=CON_CONNECTED) return NULL;
    if(!PoseIdentity(&poseClients[index],ent)) G_TCEPoseResetClient(ent);
    return PoseIdentity(&poseClients[index],ent)?&poseClients[index]:NULL;
}
void G_TCEPoseResetHistory(gentity_t *ent) {
    g_tcePoseClient_t *slot=PoseClient(ent);
    if(!slot || ent->client->tceRewind.active) return;
    memset(slot->history,0,sizeof(slot->history)); slot->historyHead=slot->historyCount=0;
    G_TCEPoseCapture(ent);
}
static uint64_t PoseSequence(g_tcePoseClient_t *slot) {
    /* At practical rates wrap is unreachable. Never alias an old ring key. */
    if(slot->sequence==UINT64_MAX) {
        unsigned int i;
        slot->generation=PoseGeneration(); slot->sequence=0;
        slot->eventHead=slot->eventCount=slot->historyHead=slot->historyCount=0;
        memset(slot->events,0,sizeof(slot->events)); memset(slot->history,0,sizeof(slot->history));
        for(i=0;i<2;++i) {
            BG_TCEPoseClockReset(&slot->clocks[i],slot->generation,slot->clocks[i].time);
            slot->acceptedEpoch[i]=0; slot->context[i]=G_TCE_POSE_UNKNOWN;
        }
    }
    return ++slot->sequence;
}
static void PoseEvent(g_tcePoseClient_t *slot,int channel,int previous,int animation,
                      uint32_t commandTime,int context) {
    g_tcePoseEvent_t *event;
    uint64_t sequence=PoseSequence(slot);
    unsigned int i;
    if(!sequence) return;
    event=&slot->events[slot->eventHead];
    if(slot->eventCount==G_TCE_POSE_EVENTS) {
        /* A sample before an evicted assignment can no longer prove there
         * were no intervening epochs. Retire it explicitly, not silently. */
        for(i=0;i<G_TCE_POSE_HISTORY;++i)
            if(slot->history[i].sequence<event->sequence) slot->history[i].sequence=0;
        ++poseStats.eventsDropped;
    } else ++slot->eventCount;
    memset(event,0,sizeof(*event));
    event->sequence=sequence; event->commandTime=commandTime; event->receiptTime=(uint32_t)level.time;
    event->channel=channel; event->previous=previous; event->animation=animation;
    event->acceptedEpoch=slot->acceptedEpoch[channel]; event->context=context; event->clock=slot->clocks[channel];
    slot->eventHead=(slot->eventHead+1)%G_TCE_POSE_EVENTS;
}
void G_TCEPoseAnimationAssigned(const playerState_t *ps,const animModelInfo_t *model,
                              animBodyPart_t part,int previous,int animation) {
    gentity_t *ent;
    g_tcePoseClient_t *slot;
    tcePoseClockClip_t clip;
    const tcePoseSkeleton_t *asset=NULL;
    const animation_t *anim=NULL;
    int channel,index,context,minimum,maximum,modelID=-1,valid=0;
    if(!ps || ps->clientNum<0 || ps->clientNum>=MAX_CLIENTS ||
       (part!=ANIM_BP_LEGS && part!=ANIM_BP_TORSO)) return;
    ent=&g_entities[ps->clientNum];
    if(!ent->client || ps!=&ent->client->ps) return;
    slot=PoseClient(ent);
    if(!slot) return;
    channel=part==ANIM_BP_LEGS?0:1;
    context=ent->client->tceRewind.active?G_TCE_POSE_DURING_REWIND:
        slot->inCommand?G_TCE_POSE_COMMAND:G_TCE_POSE_OUTSIDE_COMMAND;
    ++poseStats.accepted;
    if(++slot->acceptedEpoch[channel]==0) {
        /* Avoid epoch reuse. Restart this client's entire private generation. */
        if(ent->client->tceRewind.active) { slot->clocks[channel].valid=0; slot->occupied=0; return; }
        G_TCEPoseResetClient(ent); slot=&poseClients[ps->clientNum]; slot->acceptedEpoch[channel]=1;
    }
    index=animation&~ANIM_TOGGLEBIT;
    memset(&clip,0,sizeof(clip)); clip.model=-1;
    if(slot->character>=0 && poseCharacters[slot->character].ready &&
        model==poseCharacters[slot->character].model && index>=0 && index<model->numAnimations &&
        index<poseCharacters[slot->character].animationCount) {
        modelID=poseCharacters[slot->character].animationModels[index];
        if(modelID>=0) anim=model->animations[index];
    }
    if(anim) asset=PoseModel(anim->mdxFileName,0); /* Callback never loads. */
    if(asset && modelID>=0 && modelID<(int)poseRegistry.skeletonCount && asset==&poseRegistry.skeletons[modelID].model) {
        clip.model=modelID;
        clip.firstFrame=anim->firstFrame; clip.numFrames=anim->numFrames; clip.loopFrames=anim->loopFrames;
        clip.frameLerp=anim->frameLerp; clip.initialLerp=anim->initialLerp; clip.reversed=!!(anim->flags&ANIMFL_REVERSED);
        if(context!=G_TCE_POSE_DURING_REWIND && BG_TCEPoseClockFrameBounds(&clip,&minimum,&maximum) &&
            (unsigned int)maximum<asset->frameCount)
            valid=BG_TCEPoseClockAssign(&slot->clocks[channel],slot->generation,animation,&clip,(uint32_t)ps->commandTime);
    }
    if(valid) slot->clocks[channel].epoch=slot->acceptedEpoch[channel];
    else { slot->clocks[channel].valid=0; ++poseStats.rejected; }
    slot->context[channel]=context;
    slot->acceptedCommandTime[channel]=(uint32_t)ps->commandTime;
    slot->acceptedReceiptTime[channel]=(uint32_t)level.time;
    PoseEvent(slot,channel,previous,animation,(uint32_t)ps->commandTime,context);
}
void G_TCEPoseCapture(gentity_t *ent) {
    g_tcePoseClient_t *slot=PoseClient(ent);
    g_tcePoseHistory_t *sample;
    playerState_t *ps;
    int i,replace;
    if(!slot) return;
    if(ent->client->tceRewind.active) { ++poseStats.rewindSkipped; return; }
    ps=&ent->client->ps;
    /* Direct assignments outside the accepted callback cannot establish phase. */
    for(i=0;i<2;++i) if(slot->clocks[i].valid &&
       slot->clocks[i].animation!=(i?ps->torsoAnim:ps->legsAnim)) {
        slot->clocks[i].valid=0; slot->context[i]=G_TCE_POSE_UNKNOWN;
    }
    sample=&slot->history[(slot->historyHead+G_TCE_POSE_HISTORY-1)%G_TCE_POSE_HISTORY];
    replace=sample->sequence && sample->commandTime==(uint32_t)ps->commandTime &&
        sample->receiptTime==(uint32_t)level.time;
    if(!replace) {
        sample=&slot->history[slot->historyHead];
        if(slot->historyCount==G_TCE_POSE_HISTORY) ++poseStats.samplesDropped;
        else ++slot->historyCount;
    }
    memset(sample,0,sizeof(*sample)); sample->sequence=PoseSequence(slot);
    sample->generation=slot->generation; sample->clientNum=PoseClientNumber(ent); sample->character=slot->character;
    sample->commandTime=(uint32_t)ps->commandTime; sample->receiptTime=(uint32_t)level.time;
    sample->eFlags=ps->eFlags; sample->pmFlags=ps->pm_flags; sample->weaponFlags=ps->stats[STAT_TCE_WEAPON_FLAGS];
    sample->weapon=ps->weapon; sample->lean=ps->leanf;
    memcpy(sample->origin,ps->origin,sizeof(sample->origin)); memcpy(sample->viewangles,ps->viewangles,sizeof(sample->viewangles));
    memcpy(sample->velocity,ps->velocity,sizeof(sample->velocity));
    for(i=0;i<2;++i) {
        sample->clocks[i]=slot->clocks[i]; sample->context[i]=slot->context[i];
        sample->acceptedEpoch[i]=slot->acceptedEpoch[i]; sample->animation[i]=i?ps->torsoAnim:ps->legsAnim;
        sample->acceptedCommandTime[i]=slot->acceptedCommandTime[i]; sample->acceptedReceiptTime[i]=slot->acceptedReceiptTime[i];
    }
    if(!replace) slot->historyHead=(slot->historyHead+1)%G_TCE_POSE_HISTORY;
}
void G_TCEPoseBeginCommand(gentity_t *ent) {
    g_tcePoseClient_t *slot=PoseClient(ent);
    if(slot && !ent->client->tceRewind.active) slot->inCommand=1;
}
void G_TCEPoseEndCommand(gentity_t *ent) {
    g_tcePoseClient_t *slot=PoseClient(ent);
    int i;
    if(!slot) return;
    slot->inCommand=0;
    if(ent->client->tceRewind.active) return;
    for(i=0;i<2;++i) if(slot->clocks[i].valid &&
        !BG_TCEPoseClockAdvance(&slot->clocks[i],(uint32_t)ent->client->ps.commandTime)) {
        slot->clocks[i].valid=0; slot->context[i]=G_TCE_POSE_UNKNOWN;
    }
    G_TCEPoseCapture(ent);
}
int G_TCEPoseQuery(int clientNum,uint32_t commandTime,uint32_t receiptCeiling,g_tcePoseQuery_t *out) {
    const g_tcePoseClient_t *slot;
    const g_tcePoseHistory_t *base=NULL;
    unsigned int i;
    int channel;
    uint64_t last[2];
    if(!out) return 0;
    memset(out,0,sizeof(*out));
    if(!poseStats.initialized || clientNum<0 || clientNum>=MAX_CLIENTS) return 0;
    slot=&poseClients[clientNum];
    if(!PoseIdentity(slot,&g_entities[clientNum])) return 0;
    for(i=0;i<G_TCE_POSE_HISTORY;++i) {
        const g_tcePoseHistory_t *sample=&slot->history[i];
        if(sample->sequence && sample->generation==slot->generation &&
           PoseAtOrBefore(sample->receiptTime,receiptCeiling) && PoseAtOrBefore(sample->commandTime,commandTime) &&
           (!base || sample->sequence>base->sequence)) base=sample;
    }
    if(!base || !PoseAtOrBefore(commandTime,(uint32_t)g_entities[clientNum].client->ps.commandTime)) return 0;
    out->history=*base; last[0]=last[1]=base->sequence;
    for(i=0;i<G_TCE_POSE_EVENTS;++i) {
        const g_tcePoseEvent_t *event=&slot->events[i];
        channel=event->channel;
        if(event->sequence>last[channel] && PoseAtOrBefore(event->receiptTime,receiptCeiling) &&
           PoseAtOrBefore(event->commandTime,commandTime)) {
            out->history.clocks[channel]=event->clock; out->history.context[channel]=event->context;
            out->history.acceptedEpoch[channel]=event->acceptedEpoch; out->history.animation[channel]=event->animation;
            out->history.acceptedCommandTime[channel]=event->commandTime;
            out->history.acceptedReceiptTime[channel]=event->receiptTime;
            last[channel]=event->sequence;
        }
    }
    for(channel=0;channel<2;++channel) {
        if(BG_TCEPoseClockSample(&out->history.clocks[channel],commandTime,&out->channel[channel]) &&
           out->history.context[channel]==G_TCE_POSE_COMMAND) out->commandPhaseMask|=1u<<channel;
    }
    return 1;
}
int G_TCEPoseEvaluate(const g_tcePoseQuery_t *query,const float bodyAxis[3][3],
                     const float torsoAxis[3][3],tcePoseMathMode_t mode,tcePoseResult_t *out) {
    tcePoseInput_t input;
    tcePoseAttachment_t attachment;
    const g_tcePoseCharacter_t *character;
    const g_tcePoseClient_t *slot;
    int client,legs,torso;
    if(!out) return 0;
    memset(out,0,sizeof(*out));
    if(!query || !bodyAxis || !torsoAxis || query->commandPhaseMask!=3 || !poseStats.initialized) return 0;
    client=query->history.clientNum;
    if(client<0 || client>=MAX_CLIENTS || query->history.character<0 ||
        query->history.character>=(int)poseStats.characters) return 0;
    slot=&poseClients[client];
    if(!PoseIdentity(slot,&g_entities[client]) || slot->generation!=query->history.generation) return 0;
    character=&poseCharacters[query->history.character];
    legs=query->channel[0].model; torso=query->channel[1].model;
    if(!character->ready || legs<0 || torso<0 || legs>=(int)poseRegistry.skeletonCount || torso>=(int)poseRegistry.skeletonCount) return 0;
    memset(&input,0,sizeof(input));
    input.legs.currentModel=input.legs.oldModel=&poseRegistry.skeletons[legs].model;
    input.torso.currentModel=input.torso.oldModel=&poseRegistry.skeletons[torso].model;
    input.legs.currentFrame=query->channel[0].frame; input.legs.oldFrame=query->channel[0].oldFrame;
    input.legs.backlerp=query->channel[0].backlerp;
    input.torso.currentFrame=query->channel[1].frame; input.torso.oldFrame=query->channel[1].oldFrame;
    input.torso.backlerp=query->channel[1].backlerp;
    memcpy(input.origin,query->history.origin,sizeof(input.origin));
    memcpy(input.axis,bodyAxis,sizeof(input.axis)); memcpy(input.torsoAxis,torsoAxis,sizeof(input.torsoAxis)); input.mode=mode;
    return TCE_PosePrepareAttachment(&attachment,character->mesh,input.legs.currentModel)==TCE_POSE_OK &&
        TCE_PoseEvaluate(&poseMath,&attachment,&input,out)==TCE_POSE_OK;
}
void G_TCEPoseStats(g_tcePoseStats_t *out) { if(out) *out=poseStats; }

int G_TCEPoseAssetInfo(const bg_character_t *character,g_tcePoseAssetInfo_t *out) {
    int index;
    if(!out) return 0;
    memset(out,0,sizeof(*out)); index=PoseCharacterIndex(character);
    if(!poseStats.initialized || index<0 || !poseCharacters[index].surfaceReady) return 0;
    *out=poseCharacters[index].assets; return 1;
}
int G_TCEPoseClipMetadata(const bg_character_t *character,int fullAnimation,g_tcePoseClipMetadata_t *out) {
    const g_tcePoseCharacter_t *entry;
    const animation_t *animation;
    int index,clip;
    if(!out) return 0;
    memset(out,0,sizeof(*out)); index=PoseCharacterIndex(character); clip=fullAnimation&~ANIM_TOGGLEBIT;
    if(!poseStats.initialized || index<0 || fullAnimation<0) return 0;
    entry=&poseCharacters[index];
    if(!entry->surfaceReady || clip<0 || clip>=entry->animationCount || entry->animationModels[clip]<0 || !entry->speedReady[clip]) return 0;
    animation=entry->model->animations[clip];
    if(!animation || !PosePath(animation->mdxFileName,out->mdxFile)) return 0;
    out->fullAnimation=fullAnimation; out->firstFrame=animation->firstFrame; out->numFrames=animation->numFrames;
    out->loopFrames=animation->loopFrames; out->frameLerp=animation->frameLerp; out->initialLerp=animation->initialLerp;
    out->reversed=!!(animation->flags&ANIMFL_REVERSED); out->moveSpeed=entry->moveSpeed[clip];
    out->flags=animation->flags; out->movetype=animation->movetype;
    memcpy(out->name,animation->name,sizeof(out->name)); return 1;
}
int G_TCEPosePresentationIdentity(const gentity_t *ent,uint32_t *mapGeneration,uint32_t *generation) {
    const g_tcePoseClient_t *slot;
    const g_tcePoseCharacter_t *character;
    int client;
    if(mapGeneration)*mapGeneration=0;
    if(generation)*generation=0;
    if(!mapGeneration || !generation || !poseStats.initialized) return 0;
    client=PoseClientNumber(ent); if(client<0) return 0;
    slot=&poseClients[client];
    if(!PoseIdentity(slot,ent) || !slot->generation ||
       slot->character<0 || slot->character>=(int)poseStats.characters) return 0;
    character=&poseCharacters[slot->character];
    if(character->character!=slot->characterPointer || !character->model ||
       character->model!=slot->characterPointer->animModelInfo) return 0;
    *mapGeneration=poseStats.mapGeneration; *generation=slot->generation; return 1;
}
int G_TCEPosePresentationMetadata(const gentity_t *ent,g_tcePosePresentationMetadata_t *out) {
    const g_tcePoseClient_t *slot;
    const g_tcePoseCharacter_t *render,*animation;
    const bg_character_t *renderCharacter,*animationCharacter;
    int client,channel,index,team,cls,renderIndex,animationIndex;
    if(!out) return 0;
    memset(out,0,sizeof(*out)); client=PoseClientNumber(ent);
    if(!poseStats.initialized || client<0 || !ent->client || ent->s.eType!=ET_PLAYER || ent->health<=0 ||
       ent->s.eFlags&(0x01000000|EF_NODRAW)) return 0;
    slot=&poseClients[client];
    if(!PoseIdentity(slot,ent) || slot->character<0 || slot->character>=(int)poseStats.characters) return 0;
    team=ent->client->sess.sessionTeam;
    if(team!=TEAM_AXIS && team!=TEAM_ALLIES) return 0;
    animationCharacter=ent->client->pers.character;
    if(ent->s.powerups&(1<<PW_OPS_DISGUISED)) {
        cls=(ent->s.powerups>>PW_OPS_CLASS_1)&7;
        if(cls>=7) return 0;
        animationCharacter=BG_GetCharacter(team==TEAM_AXIS?TEAM_ALLIES:TEAM_AXIS,cls);
    }
    renderCharacter=ent->s.effect1Time&0x60?BG_GetCharacter(team,6):animationCharacter;
    renderIndex=PoseCharacterIndex(renderCharacter); animationIndex=PoseCharacterIndex(animationCharacter);
    if(renderIndex<0 || animationIndex<0) return 0;
    render=&poseCharacters[renderIndex]; animation=&poseCharacters[animationIndex];
    /* Shipped classes share one immutable table. A different custom animation
     * table needs its own registration-time render-mesh calibration. */
    if(!render->surfaceReady || !animation->surfaceReady || render->model!=animation->model ||
       animation->model!=poseCharacters[slot->character].model) return 0;
    out->fullAnimation[0]=ent->client->ps.legsAnim; out->fullAnimation[1]=ent->client->ps.torsoAnim;
    for(channel=0;channel<2;++channel) {
        index=out->fullAnimation[channel]&~ANIM_TOGGLEBIT;
        if(out->fullAnimation[channel]<0 || index<0 || index>=animation->animationCount ||
           animation->animationModels[index]<0 || !render->speedReady[index] || !slot->acceptedEpoch[channel]) { memset(out,0,sizeof(*out)); return 0; }
        out->animation[channel]=animation->model->animations[index];
        if(!out->animation[channel]) { memset(out,0,sizeof(*out)); return 0; }
        out->acceptedEpoch[channel]=slot->acceptedEpoch[channel];
        out->moveSpeed[channel]=render->moveSpeed[index];
    }
    out->mapGeneration=poseStats.mapGeneration; out->generation=slot->generation;
    out->profile=render==animation?render->assets.profile:
        BG_TCEPosePresentationPairProfile(render->assets.profile,animation->assets.rawProfile);
    out->renderCharacter=renderCharacter; out->animationCharacter=animationCharacter; return out->profile!=0;
}
int G_TCEPosePresentationProfiles(uint32_t *out,int capacity) {
    uint32_t profiles[G_TCE_POSE_CHARACTERS*3],value;
    unsigned int count=0,i,j,at;
    int team,index;
    if(capacity<0) return -1;
    if(!poseStats.initialized) return 0;
    for(i=0;i<poseStats.characters;++i) {
        const g_tcePoseCharacter_t *animation=&poseCharacters[i];
        if(!animation->surfaceReady) continue;
        profiles[count++]=animation->assets.profile;
        for(team=TEAM_AXIS;team<=TEAM_ALLIES;++team) {
            index=PoseCharacterIndex(BG_GetCharacter(team,6));
            if(index>=0 && poseCharacters[index].surfaceReady && poseCharacters[index].model==animation->model &&
               &poseCharacters[index]!=animation)
                profiles[count++]=BG_TCEPosePresentationPairProfile(poseCharacters[index].assets.profile,animation->assets.rawProfile);
        }
    }
    for(i=1;i<count;++i) { value=profiles[i];at=i;while(at && profiles[at-1]>value) {profiles[at]=profiles[at-1];--at;}profiles[at]=value; }
    for(i=0,j=0;i<count;++i) if(!i || profiles[i]!=profiles[i-1]) profiles[j++]=profiles[i];
    if(j>(unsigned int)capacity || (j && !out)) return -1;
    if(j)memcpy(out,profiles,j*sizeof(profiles[0]));
    return (int)j;
}
static const g_tcePoseCharacter_t *PoseTraceCharacter(const g_tcePresentationQuery_t *query,
    const g_tcePoseCharacter_t **animationOut) {
    const g_tcePoseClient_t *slot;
    const g_tcePoseCharacter_t *render,*animation;
    uint32_t profile;
    int renderIndex,animationIndex;
    if(!query || !poseStats.initialized || query->clientNum<0 || query->clientNum>=MAX_CLIENTS ||
       query->mapGeneration!=poseStats.mapGeneration || !query->profile ||
       query->mode<TCE_POSE_PRESENT_LADDER || query->mode>TCE_POSE_PRESENT_PRONE) return NULL;
    slot=&poseClients[query->clientNum];
    if(!PoseIdentity(slot,&g_entities[query->clientNum]) || slot->generation!=query->generation ||
       slot->character<0 || slot->character>=(int)poseStats.characters) return NULL;
    renderIndex=PoseCharacterIndex(query->renderCharacter); animationIndex=PoseCharacterIndex(query->animationCharacter);
    if(renderIndex<0 || animationIndex<0) return NULL;
    render=&poseCharacters[renderIndex]; animation=&poseCharacters[animationIndex];
    if(!render->surfaceReady || !animation->surfaceReady || render->model!=animation->model ||
       animation->model!=poseCharacters[slot->character].model ||
       render->assets.mapGeneration!=query->mapGeneration || animation->assets.mapGeneration!=query->mapGeneration) return NULL;
    profile=render==animation?render->assets.profile:
        BG_TCEPosePresentationPairProfile(render->assets.profile,animation->assets.rawProfile);
    if(profile!=query->profile) return NULL;
    if(animationOut)*animationOut=animation;
    return render;
}
static int PoseTraceKey(const g_tcePresentationQuery_t *query,const float start[3],const float end[3],
                        double maxFraction,g_tcePoseTraceKey_t *key) {
    int i;
    if(!query || !start || !end || !key || !(maxFraction>=0 && maxFraction<=1)) return 0;
    for(i=0;i<3;++i) if(!(start[i]>=-FLT_MAX && start[i]<=FLT_MAX && end[i]>=-FLT_MAX && end[i]<=FLT_MAX)) return 0;
    memset(key,0,sizeof(*key));
    key->mapGeneration=query->mapGeneration; key->generation=query->generation; key->profile=query->profile;
    key->sourceTime=query->sourceTime; key->serverTime=query->serverTime; key->clientNum=query->clientNum;
    key->mode=query->mode; key->eFlags=query->eFlags;
    key->renderCharacter=query->renderCharacter; key->animationCharacter=query->animationCharacter;
    for(i=0;i<2;++i) {
        key->acceptedEpoch[i]=query->acceptedEpoch[i]; key->animation[i]=query->animation[i]; key->code[i]=query->code[i];
        key->elapsedQ16[i]=query->channel[i].elapsedQ16; key->frame[i][0]=query->channel[i].oldFrame;
        key->frame[i][1]=query->channel[i].frame; key->backlerp[i]=query->channel[i].backlerp;
        key->rateQ8[i]=query->rateQ8[i]; key->held[i]=query->channel[i].held; key->finished[i]=query->channel[i].finished;
        key->clip[i][0]=query->clip[i].firstFrame; key->clip[i][1]=query->clip[i].numFrames;
        key->clip[i][2]=query->clip[i].loopFrames; key->clip[i][3]=query->clip[i].frameLerp;
        key->clip[i][4]=query->clip[i].initialLerp; key->clip[i][5]=query->clip[i].reversed;
    }
    memcpy(key->origin,query->origin,sizeof(key->origin)); memcpy(key->bodyAxis,query->axes.body,sizeof(key->bodyAxis));
    memcpy(key->torsoAxis,query->axes.torso,sizeof(key->torsoAxis)); memcpy(key->headAxis,query->axes.head,sizeof(key->headAxis));
    memcpy(key->bodyAngles,query->axes.bodyAngles,sizeof(key->bodyAngles));
    memcpy(key->torsoAngles,query->axes.torsoRelativeAngles,sizeof(key->torsoAngles));
    memcpy(key->headAngles,query->axes.headRelativeAngles,sizeof(key->headAngles));
    memcpy(key->start,start,sizeof(key->start)); memcpy(key->end,end,sizeof(key->end)); key->maxFraction=maxFraction;
    return 1;
}
static int PoseTraceInput(const g_tcePoseCharacter_t *entry,const g_tcePoseCharacter_t *animationEntry,
    const g_tcePresentationQuery_t *query,tcePoseInput_t *input) {
    int channel,index,modelID,minimum,maximum;
    memset(input,0,sizeof(*input));
    for(channel=0;channel<2;++channel) {
        tcePoseCompactSample_t sample;
        tcePoseClockClip_t raw;
        tcePosePresentationClip_t clip;
        tcePoseFramePair_t *pair=channel?&input->torso:&input->legs;
        const animation_t *animation;
        index=query->animation[channel]&~ANIM_TOGGLEBIT;
        if(query->animation[channel]<0 || index<0 || index>=animationEntry->animationCount || !query->acceptedEpoch[channel]) return 0;
        modelID=animationEntry->animationModels[index]; animation=animationEntry->model->animations[index];
        if(modelID<0 || !animation || !entry->speedReady[index]) return 0;
        memset(&clip,0,sizeof(clip)); clip.firstFrame=animation->firstFrame; clip.numFrames=animation->numFrames;
        clip.loopFrames=animation->loopFrames; clip.frameLerp=animation->frameLerp; clip.initialLerp=animation->initialLerp;
        clip.reversed=!!(animation->flags&ANIMFL_REVERSED);
        if(memcmp(&clip,&query->clip[channel],sizeof(clip)) ||
           !BG_TCEPoseCompactSample(&clip,query->code[channel],query->sourceTime,query->serverTime,
                                   query->rateQ8[channel],&sample) ||
           sample.elapsedQ16!=query->channel[channel].elapsedQ16 || sample.frame!=query->channel[channel].frame ||
           sample.oldFrame!=query->channel[channel].oldFrame || sample.backlerp!=query->channel[channel].backlerp) return 0;
        memset(&raw,0,sizeof(raw)); raw.model=modelID; raw.firstFrame=clip.firstFrame; raw.numFrames=clip.numFrames;
        raw.loopFrames=clip.loopFrames; raw.frameLerp=clip.frameLerp; raw.initialLerp=clip.initialLerp; raw.reversed=clip.reversed;
        pair->currentModel=pair->oldModel=&poseRegistry.skeletons[modelID].model;
        if(!BG_TCEPoseClockFrameBounds(&raw,&minimum,&maximum) || (unsigned int)maximum>=pair->currentModel->frameCount) return 0;
        pair->currentFrame=(unsigned int)sample.frame; pair->oldFrame=(unsigned int)sample.oldFrame; pair->backlerp=sample.backlerp;
    }
    memcpy(input->origin,query->origin,sizeof(input->origin)); memcpy(input->axis,query->axes.body,sizeof(input->axis));
    memcpy(input->torsoAxis,query->axes.torso,sizeof(input->torsoAxis)); input->mode=TCE_POSE_MATH_OBSERVED_WINDOWS;
    return 1;
}
g_tcePoseTraceState_t G_TCEPosePresentationTrace(const g_tcePresentationQuery_t *query,const float start[3],
    const float end[3],double maxFraction,g_tcePoseTraceResult_t *out) {
    const g_tcePoseCharacter_t *entry,*animationEntry=NULL;
    tcePoseInput_t input;
    tcePoseHeadInput_t head;
    tcePoseAttachment_t attachment;
    tcePoseResult_t anchors;
    int row,column,i,anchor;
    if(!out) return G_TCE_POSE_TRACE_UNSUPPORTED;
    memset(out,0,sizeof(*out));
    entry=PoseTraceCharacter(query,&animationEntry);
    if(!entry || !animationEntry || !PoseTraceKey(query,start,end,maxFraction,&out->key) ||
       !PoseTraceInput(entry,animationEntry,query,&input) ||
       TCE_PosePrepareAttachment(&attachment,entry->mesh,input.legs.currentModel)!=TCE_POSE_OK ||
       TCE_PoseEvaluate(&poseMath,&attachment,&input,&anchors)!=TCE_POSE_OK) goto unsupported;
    memset(&head,0,sizeof(head)); /* Explicit fixed facial frame0. */
    memcpy(head.origin,anchors.world[TCE_POSE_HEAD].origin,sizeof(head.origin));
    for(row=0;row<3;++row) for(column=0;column<3;++column) {
        double value=0;
        for(i=0;i<3;++i) value+=(double)query->axes.head[row][i]*anchors.world[TCE_POSE_HEAD].axis[i][column];
        head.axis[row][column]=(float)value;
    }
    if(TCE_PoseSkinEvaluate(&poseMath,entry->skin,entry->head,&input,&head,&poseSurfaceScratch)!=TCE_POSE_OK ||
       TCE_PoseSkinRaycast(&poseSurfaceScratch,start,end,maxFraction,&out->surface)!=TCE_POSE_OK) goto unsupported;
    out->state=out->surface.hitMask?G_TCE_POSE_TRACE_HIT:G_TCE_POSE_TRACE_MISS;
    if(out->state==G_TCE_POSE_TRACE_HIT) {
        if(out->surface.region<TCE_POSE_REGION_HEAD || out->surface.region>TCE_POSE_REGION_LEGS) goto unsupported;
        out->nominal[out->surface.region]=out->surface.region==TCE_POSE_REGION_HEAD?2.f:
            out->surface.region==TCE_POSE_REGION_BODY?1.f:.5f;
        anchor=out->surface.region==TCE_POSE_REGION_HEAD?TCE_POSE_HEAD:TCE_POSE_CHEST;
        for(i=0;i<3;++i) out->anchor[i]=out->surface.region==TCE_POSE_REGION_LEGS?
            .5f*(anchors.world[TCE_POSE_THIGH_LEFT].origin[i]+anchors.world[TCE_POSE_THIGH_RIGHT].origin[i]):
            anchors.world[anchor].origin[i];
    }
    return out->state;
unsupported:
    memset(out,0,sizeof(*out)); return G_TCE_POSE_TRACE_UNSUPPORTED;
}
int G_TCEPosePresentationTraceMatches(const g_tcePoseTraceResult_t *result,const g_tcePresentationQuery_t *query,
    const float start[3],const float end[3],double maxFraction) {
    g_tcePoseTraceKey_t key;
    return result && (result->state==G_TCE_POSE_TRACE_MISS || result->state==G_TCE_POSE_TRACE_HIT) &&
        PoseTraceCharacter(query,NULL) && PoseTraceKey(query,start,end,maxFraction,&key) && !memcmp(&key,&result->key,sizeof(key));
}
int G_TCEPosePresentationBounds(const g_tcePresentationQuery_t *query,float mins[3][3],float maxs[3][3]) {
    const g_tcePoseCharacter_t *entry,*animationEntry=NULL;
    tcePoseInput_t input;tcePoseHeadInput_t head;tcePoseAttachment_t attachment;tcePoseResult_t anchors;
    unsigned int triangle,vertex,index;int row,column,i,axis,region,seen[3]={0,0,0};
    if(!mins||!maxs)return 0;
    memset(mins,0,sizeof(float)*9);memset(maxs,0,sizeof(float)*9);
    entry=PoseTraceCharacter(query,&animationEntry);
    if(!entry||!animationEntry||!PoseTraceInput(entry,animationEntry,query,&input)||
       TCE_PosePrepareAttachment(&attachment,entry->mesh,input.legs.currentModel)!=TCE_POSE_OK||
       TCE_PoseEvaluate(&poseMath,&attachment,&input,&anchors)!=TCE_POSE_OK)return 0;
    memset(&head,0,sizeof(head));memcpy(head.origin,anchors.world[TCE_POSE_HEAD].origin,sizeof(head.origin));
    for(row=0;row<3;++row)for(column=0;column<3;++column) {
        double value=0;for(i=0;i<3;++i)value+=(double)query->axes.head[row][i]*anchors.world[TCE_POSE_HEAD].axis[i][column];
        head.axis[row][column]=(float)value;
    }
    if(TCE_PoseSkinEvaluate(&poseMath,entry->skin,entry->head,&input,&head,&poseSurfaceScratch)!=TCE_POSE_OK)return 0;
    for(triangle=0;triangle<entry->skin->triangleCount+entry->head->triangleCount;++triangle) {
        int isHead=triangle>=entry->skin->triangleCount;
        unsigned int at=isHead?triangle-entry->skin->triangleCount:triangle;
        region=isHead?TCE_POSE_REGION_HEAD:entry->skin->region[at];
        if(region<0||region>2)return 0;
        for(vertex=0;vertex<3;++vertex) {
            index=isHead?entry->skin->vertexCount+entry->head->triangle[at][vertex]:entry->skin->triangle[at][vertex];
            if(index>=poseSurfaceScratch.vertexCount)return 0;
            for(axis=0;axis<3;++axis) {
                float value=poseSurfaceScratch.vertices[index][axis];
                if(!(value>=-FLT_MAX&&value<=FLT_MAX))return 0;
                if(!seen[region]||value<mins[region][axis])mins[region][axis]=value;
                if(!seen[region]||value>maxs[region][axis])maxs[region][axis]=value;
            }
            seen[region]=1;
        }
    }
    return seen[0]&&seen[1]&&seen[2];
}
