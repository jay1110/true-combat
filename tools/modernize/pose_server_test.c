/* Actual complete server acquisition source, native stdio FS and allocation
 * accounting adapters. No game process or synthetic decision implementation. */
#include "g_tce_pose.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifdef G_TCE_POSE_ADAPTER_FIXTURES
#include "pose_adapter_cases.inc"
#endif

static unsigned int checks,failures,opens,successfulOpens,closes,allocations,frees;
static size_t liveBytes,peakBytes;
static const char *assetDirectory;
static FILE *files[32];
static int oversized;
static int failAllocation;
static int fullAssetMode;
static struct { void *pointer; size_t size; } memory[64];
#define CHECK(x) do { ++checks; if(!(x)) { ++failures; if(failures<30) fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); } } while(0)
static void *trackedMalloc(size_t size) {
    unsigned int i;
    void *result=failAllocation?NULL:malloc(size);
    if(result) {
        for(i=0;i<64 && memory[i].pointer;++i) { }
        CHECK(i<64); if(i==64) exit(2);
        memory[i].pointer=result; memory[i].size=size; liveBytes+=size; ++allocations;
        if(liveBytes>peakBytes) peakBytes=liveBytes;
        CHECK(liveBytes<=TCE_POSE_MAX_CACHE_BYTES);
    }
    return result;
}
static void trackedFree(void *pointer) {
    unsigned int i;
    if(!pointer) return;
    for(i=0;i<64 && memory[i].pointer!=pointer;++i) { }
    CHECK(i<64); if(i==64) exit(2);
    liveBytes-=memory[i].size; memory[i].pointer=NULL; ++frees; free(pointer);
}
int trap_FS_FOpenFile(const char *name,fileHandle_t *handle,fsMode_t mode) {
    char path[2048];
    int index; long size;
    CHECK(mode==FS_READ); *handle=0; ++opens;
    for(index=1;index<32 && files[index];++index) { }
    CHECK(index<32); if(index==32) return -1;
    snprintf(path,sizeof(path),"%s/%s",assetDirectory,name);
    files[index]=fopen(path,"rb"); if(!files[index]) return -1;
    ++successfulOpens;
    CHECK(!fseek(files[index],0,SEEK_END)); size=ftell(files[index]); CHECK(!fseek(files[index],0,SEEK_SET));
    *handle=index;
    return oversized?TCE_POSE_MAX_CACHE_BYTES:(int)size;
}
void trap_FS_Read(void *buffer,int size,fileHandle_t handle) {
    CHECK(handle>0 && handle<32 && files[handle]!=NULL);
    CHECK(fread(buffer,1,(size_t)size,files[handle])==(size_t)size);
}
void trap_FS_FCloseFile(fileHandle_t handle) {
    CHECK(handle>0 && handle<32 && files[handle]!=NULL);
    fclose(files[handle]); files[handle]=NULL; ++closes;
}
#define malloc trackedMalloc
#define free trackedFree
#include "g_tce_pose.c"
#undef malloc
#undef free
gentity_t g_entities[MAX_GENTITIES];
level_locals_t level;
static gclient_t clients[MAX_CLIENTS];

static animScriptData_t *globalScriptData;
/* Preserve the verbatim SDK BOTH -> TORSO dispatch. Classify its intentional
 * fallthrough locally so strict native GCC still diagnoses all new sources. */
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 7
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
/* ACTUAL_BG_PLAYANIM */
#if defined(__GNUC__) && !defined(__clang__) && __GNUC__ >= 7
#pragma GCC diagnostic pop
#endif
static animModelInfo_t modelInfo;
static animation_t animations[MAX_MODEL_ANIMATIONS];
static bg_character_t characters[G_TCE_POSE_CHARACTERS+1];
bg_character_t axisClassCharacters[7],alliedClassCharacters[7];
/* ACTUAL_BG_GETCHARACTER */
static bg_characterDef_t definition;
static const float identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};
static void connectClient(int index,int commandTime) {
    gentity_t *ent=&g_entities[index]; gclient_t *client=&clients[index];
    memset(ent,0,sizeof(*ent)); memset(client,0,sizeof(*client));
    ent->inuse=qtrue; ent->client=client; ent->s.number=index;
    client->pers.connected=CON_CONNECTED; client->pers.connectTime=10;
    client->pers.character=&characters[0]; client->ps.clientNum=index;
    client->ps.commandTime=commandTime; client->ps.persistant[PERS_SPAWN_COUNT]=1;
    G_TCEPoseResetClient(ent); G_TCEPoseCapture(ent);
}
static void assign(int index,int animation,animBodyPart_t part) {
    CHECK(BG_PlayAnim(&clients[index].ps,&modelInfo,animation,part,0,qfalse,qfalse,qtrue)>=0 || part==ANIM_BP_TORSO);
}
static void setup(void) {
    unsigned int i;
    memset(&definition,0,sizeof(definition));
    strcpy(definition.mesh,fullAssetMode?"models/players/temperate/allied/engineer/body.mdm":"mesh-0.mdm");
    memset(&modelInfo,0,sizeof(modelInfo)); memset(animations,0,sizeof(animations));
    strcpy(animations[0].mdxFileName,fullAssetMode?"ANIMATIONS\\HUMAN\\BASE\\BODY.MDX":"model-0.mdx");
    strcpy(animations[1].mdxFileName,fullAssetMode?"animations/human/base/prone_rifle.mdx":"model-1.mdx");
    for(i=0;i<2;++i) {
        animations[i].firstFrame=0; animations[i].numFrames=i?1:10;
        animations[i].loopFrames=animations[i].numFrames; animations[i].frameLerp=100;
        animations[i].initialLerp=40; animations[i].duration=1000; modelInfo.animations[i]=&animations[i];
    }
    if(fullAssetMode) { animations[0].firstFrame=2623; animations[1].firstFrame=57; }
    modelInfo.numAnimations=2;
    for(i=0;i<G_TCE_POSE_CHARACTERS+1;++i) characters[i].animModelInfo=&modelInfo;
    level.time=1900; G_TCEPoseInit();
    level.animScriptData.animationAssigned=G_TCEPoseAnimationAssigned; globalScriptData=&level.animScriptData;
    CHECK(G_TCEPoseRegisterCharacter(&characters[0],&definition));
    CHECK(G_TCEPoseRegisterCharacter(&characters[1],&definition));
    CHECK(opens==3 && closes==3 && allocations==3);
    CHECK(poseRegistry.skeletonCount==2 && poseRegistry.meshCount==1);
    connectClient(0,1000); connectClient(1,1000);
}
static void identityControls(void) {
    gentity_t *ent=&g_entities[0],foreign;
    g_tcePoseClient_t saved=poseClients[0];
    gclient_t savedClient=clients[0];
    g_tcePoseCharacter_t savedCharacter=poseCharacters[saved.character];
    uint32_t map=1,generation=1;
    unsigned int oldOpens=opens;
    CHECK(G_TCEPosePresentationIdentity(ent,&map,&generation));
    CHECK(map==poseStats.mapGeneration && generation==saved.generation);
    /* No current phase, geometry, render-selection or life-policy dependency. */
    clients[0].ps.legsAnim=clients[0].ps.torsoAnim=MAX_MODEL_ANIMATIONS+1;
    poseCharacters[saved.character].ready=poseCharacters[saved.character].surfaceReady=0;
    ent->s.effect1Time=0x60;ent->s.powerups=1<<PW_OPS_DISGUISED;ent->health=0;ent->s.eType=ET_CORPSE;
    CHECK(G_TCEPosePresentationIdentity(ent,&map,&generation) && generation==saved.generation);
    ent->s.effect1Time=ent->s.powerups=ent->health=ent->s.eType=0;
    clients[0]=savedClient;poseCharacters[saved.character]=savedCharacter;
#define IDENTITY_REJECT(change,restore) do { change;map=generation=1; \
    CHECK(!G_TCEPosePresentationIdentity(ent,&map,&generation) && !map && !generation);restore; } while(0)
    IDENTITY_REJECT(poseStats.initialized=0,poseStats.initialized=1);
    IDENTITY_REJECT(ent->inuse=qfalse,ent->inuse=qtrue);
    IDENTITY_REJECT(clients[0].pers.connected=CON_DISCONNECTED,clients[0].pers.connected=CON_CONNECTED);
    IDENTITY_REJECT(clients[0].pers.connectTime++,clients[0].pers.connectTime--);
    IDENTITY_REJECT(clients[0].ps.persistant[PERS_SPAWN_COUNT]++,clients[0].ps.persistant[PERS_SPAWN_COUNT]--);
    IDENTITY_REJECT(clients[0].pers.character=&characters[1],clients[0].pers.character=savedClient.pers.character);
    IDENTITY_REJECT(poseClients[0].occupied=0,poseClients[0].occupied=1);
    IDENTITY_REJECT(poseClients[0].generation=0,poseClients[0].generation=saved.generation);
    IDENTITY_REJECT(poseClients[0].character=-1,poseClients[0].character=saved.character);
    IDENTITY_REJECT(poseClients[0].character=(int)poseStats.characters,poseClients[0].character=saved.character);
    IDENTITY_REJECT(poseCharacters[saved.character].character=&characters[1],poseCharacters[saved.character].character=savedCharacter.character);
    IDENTITY_REJECT(poseCharacters[saved.character].model=NULL,poseCharacters[saved.character].model=savedCharacter.model);
    IDENTITY_REJECT(characters[0].animModelInfo=NULL,characters[0].animModelInfo=&modelInfo);
#undef IDENTITY_REJECT
    memset(&foreign,0,sizeof(foreign));map=generation=1;
    CHECK(!G_TCEPosePresentationIdentity(&foreign,&map,&generation) && !map && !generation);
    map=generation=1;CHECK(!G_TCEPosePresentationIdentity(NULL,&map,&generation) && !map && !generation);
    map=generation=1;CHECK(!G_TCEPosePresentationIdentity(ent,NULL,&generation) && !generation);
    CHECK(!G_TCEPosePresentationIdentity(ent,&map,NULL) && !map);
    CHECK(opens==oldOpens && !memcmp(&saved,&poseClients[0],sizeof(saved)));
}
static void completeRegistry(void) {
    static const char *const names[]={"akimbo","binocs","body","mortar","no_juice",
        "prone_dual_pistols","prone_mortar","prone_pistol","prone_rifle","swim"};
    unsigned int i,before;
    char path[MAX_QPATH];
    if(!fullAssetMode) return;
    for(i=0;i<10;++i) {
        snprintf(path,sizeof(path),"animations/human/base/%s.mdx",names[i]);
        CHECK(PoseModel(path,1)!=NULL);
    }
    CHECK(poseRegistry.skeletonCount==10);
    CHECK(poseRegistry.borrowedBytes==3758560u+62468u);
    before=opens;
    CHECK(PoseModel("unloaded.mdx",1)==NULL);
    CHECK(opens==before); /* Registry capacity fails before opening/allocating. */
}
static void epochs(void) {
    gentity_t *ent=&g_entities[0];
    g_tcePoseQuery_t query,old;
    g_tcePoseClient_t before;
    tcePoseResult_t result;
    unsigned int oldHistory,oldOpens=opens;
    CHECK(G_TCEPoseQuery(0,1000,1900,&query) && query.commandPhaseMask==0);
    G_TCEPoseBeginCommand(ent); level.time=2000;
    clients[0].ps.commandTime=1010; assign(0,0,ANIM_BP_LEGS); assign(0,0,ANIM_BP_LEGS); assign(0,1,ANIM_BP_TORSO);
    clients[0].ps.commandTime=1018; assign(0,0,ANIM_BP_LEGS); G_TCEPoseEndCommand(ent);
    CHECK(poseClients[0].acceptedEpoch[0]==3 && poseClients[0].acceptedEpoch[1]==1);
    CHECK(poseClients[1].acceptedEpoch[0]==0);
    CHECK(poseClients[0].eventCount==4);
    CHECK(G_TCEPoseQuery(0,1018,2000,&query) && query.commandPhaseMask==3);
    CHECK(query.history.acceptedEpoch[0]==3 && query.history.acceptedEpoch[1]==1);
    CHECK(query.history.acceptedCommandTime[0]==1018 && query.history.acceptedReceiptTime[0]==2000);
    CHECK(query.channel[0].model!=query.channel[1].model);
    CHECK(G_TCEPoseEvaluate(&query,identity,identity,TCE_POSE_MATH_OBSERVED_WINDOWS,&result));
    CHECK(result.evaluatedBones==13);
    old=query; before=poseClients[0];
    CHECK(G_TCEPoseQuery(0,1012,2000,&query) && query.commandPhaseMask==3);
    CHECK(query.history.acceptedEpoch[0]==2 && query.channel[0].time==1012);
    CHECK(!memcmp(&before,&poseClients[0],sizeof(before)));
    CHECK(G_TCEPoseQuery(0,1012,1999,&query) && query.commandPhaseMask==0);
    CHECK(!G_TCEPoseQuery(0,999,2000,&query)); CHECK(!G_TCEPoseQuery(0,1019,2000,&query));
    oldHistory=poseClients[0].historyCount; G_TCEPoseCapture(ent); CHECK(poseClients[0].historyCount==oldHistory);
    level.time=2020; assign(0,0,ANIM_BP_LEGS);
    CHECK(G_TCEPoseQuery(0,1018,2020,&query) && query.commandPhaseMask==2);
    CHECK(query.history.context[0]==G_TCE_POSE_OUTSIDE_COMMAND);
    CHECK(query.history.acceptedCommandTime[0]==1018 && query.history.acceptedReceiptTime[0]==2020);
    CHECK(!G_TCEPoseEvaluate(&query,identity,identity,TCE_POSE_MATH_OBSERVED_WINDOWS,&result));
    G_TCEPoseBeginCommand(ent); clients[0].ps.commandTime=1024; assign(0,0,ANIM_BP_LEGS); G_TCEPoseEndCommand(ent);
    CHECK(G_TCEPoseQuery(0,1024,2020,&query) && query.commandPhaseMask==3);
    clients[0].ps.legsAnim^=ANIM_TOGGLEBIT; G_TCEPoseCapture(ent);
    CHECK(G_TCEPoseQuery(0,1024,2020,&query) && query.commandPhaseMask==2);
    CHECK(opens==oldOpens); /* All command, query and evaluate paths are I/O free. */
    ++clients[0].ps.persistant[PERS_SPAWN_COUNT];
    CHECK(!G_TCEPoseQuery(0,1024,2020,&query));
    G_TCEPoseBeginCommand(ent); assign(0,0,ANIM_BP_BOTH); G_TCEPoseEndCommand(ent);
    CHECK(poseClients[0].generation!=old.history.generation);
    CHECK(!G_TCEPoseEvaluate(&old,identity,identity,TCE_POSE_MATH_OBSERVED_WINDOWS,&result));
    CHECK(G_TCEPoseQuery(0,1024,2020,&query) && query.commandPhaseMask==3); old=query;
    /* Actual ClientBegin ordering: ClientSpawn accepted assignments precede
     * G_ResetMarkers. That marker reset must retain the fresh spawn epochs. */
    G_TCEPoseResetHistory(ent);
    CHECK(G_TCEPoseQuery(0,1024,2020,&query) && query.commandPhaseMask==3);
    CHECK(query.history.generation==old.history.generation &&
          query.history.acceptedEpoch[0]==old.history.acceptedEpoch[0]);
    /* ClientBegin's explicit boundary also handles same-tick slot reuse,
     * where connectTime, character and fresh spawn count all collide. */
    old=query; G_TCEPoseResetClient(ent);
    G_TCEPoseBeginCommand(ent); assign(0,0,ANIM_BP_BOTH); G_TCEPoseEndCommand(ent);
    G_TCEPoseResetHistory(ent);
    CHECK(G_TCEPoseQuery(0,1024,2020,&query) && query.commandPhaseMask==3);
    CHECK(query.history.generation!=old.history.generation);
    CHECK(!G_TCEPoseEvaluate(&old,identity,identity,TCE_POSE_MATH_OBSERVED_WINDOWS,&result));
    clients[0].pers.character=&characters[1]; G_TCEPoseCharacterChanged(&clients[0]);
    CHECK(!G_TCEPoseQuery(0,1024,2020,&query));
    G_TCEPoseBeginCommand(ent); assign(0,0,ANIM_BP_BOTH); G_TCEPoseEndCommand(ent);
    CHECK(G_TCEPoseQuery(0,1024,2020,&query) && query.commandPhaseMask==3);
    old=query;
    CHECK(G_TCEPoseRegisterCharacter(&characters[1],&definition));
    CHECK(!G_TCEPoseEvaluate(&old,identity,identity,TCE_POSE_MATH_OBSERVED_WINDOWS,&result));
    clients[0].pers.connected=CON_DISCONNECTED; CHECK(!G_TCEPoseQuery(0,1024,2020,&query));
    clients[0].pers.connected=CON_CONNECTED; ++clients[0].pers.connectTime;
    G_TCEPoseBeginCommand(ent); assign(0,0,ANIM_BP_BOTH); G_TCEPoseEndCommand(ent);
    CHECK(G_TCEPoseQuery(0,1024,2020,&query) && query.history.generation!=old.history.generation);
}
static void wrappedTimelines(void) {
    g_tcePoseQuery_t query;
    int savedTime=level.time;
    connectClient(3,-16); level.time=-6;
    G_TCEPoseBeginCommand(&g_entities[3]); clients[3].ps.commandTime=-4;
    assign(3,0,ANIM_BP_BOTH); G_TCEPoseEndCommand(&g_entities[3]);
    level.time=10; G_TCEPoseBeginCommand(&g_entities[3]); clients[3].ps.commandTime=8;
    assign(3,0,ANIM_BP_BOTH); G_TCEPoseEndCommand(&g_entities[3]);
    CHECK(G_TCEPoseQuery(3,4,10,&query) && query.commandPhaseMask==3);
    CHECK(query.channel[0].time==4 && query.history.acceptedCommandTime[0]==UINT32_MAX-3);
    level.time=savedTime;
}
static void rewindAndOverflow(void) {
    gentity_t *ent=&g_entities[0]; g_tcePoseQuery_t query;
    tcePoseClock_t before; playerState_t expected;
    unsigned int i,history;
    G_TCEPoseBeginCommand(ent); assign(0,0,ANIM_BP_BOTH); G_TCEPoseEndCommand(ent);
    before=poseClients[0].clocks[0]; history=poseClients[0].historyCount;
    clients[0].tceRewind.active=qtrue;
    clients[0].ps.commandTime+=10;
    G_TCEPoseCapture(ent); G_TCEPoseBeginCommand(ent); G_TCEPoseEndCommand(ent);
    CHECK(poseClients[0].historyCount==history);
    CHECK(!memcmp(&before,&poseClients[0].clocks[0],sizeof(before)));
    expected=clients[0].ps; level.animScriptData.animationAssigned=NULL;
    BG_PlayAnim(&expected,&modelInfo,0,ANIM_BP_LEGS,0,qfalse,qfalse,qtrue);
    level.animScriptData.animationAssigned=G_TCEPoseAnimationAssigned;
    assign(0,0,ANIM_BP_LEGS);
    CHECK(!memcmp(&expected,&clients[0].ps,sizeof(expected)));
    CHECK(poseClients[0].context[0]==G_TCE_POSE_DURING_REWIND);
    CHECK(!poseClients[0].clocks[0].valid && poseClients[0].clocks[0].time==before.time);
    clients[0].tceRewind.active=qfalse;
    G_TCEPoseResetClient(ent); G_TCEPoseCapture(ent);
    G_TCEPoseBeginCommand(ent);
    for(i=0;i<G_TCE_POSE_EVENTS+5;++i) assign(0,0,ANIM_BP_BOTH);
    CHECK(poseStats.eventsDropped>0);
    CHECK(!G_TCEPoseQuery(0,(uint32_t)clients[0].ps.commandTime,(uint32_t)level.time,&query));
    G_TCEPoseEndCommand(ent);
    CHECK(G_TCEPoseQuery(0,(uint32_t)clients[0].ps.commandTime,(uint32_t)level.time,&query) && query.commandPhaseMask==3);
    for(i=0;i<G_TCE_POSE_HISTORY+5;++i) { ++clients[0].ps.commandTime; ++level.time; G_TCEPoseCapture(ent); }
    CHECK(poseStats.samplesDropped>0);
    poseClients[0].sequence=UINT64_MAX; G_TCEPoseCapture(ent);
    CHECK(G_TCEPoseQuery(0,(uint32_t)clients[0].ps.commandTime,(uint32_t)level.time,&query) && query.commandPhaseMask==0);
    poseClients[0].acceptedEpoch[0]=UINT32_MAX;
    i=poseClients[0].generation; G_TCEPoseBeginCommand(ent); assign(0,0,ANIM_BP_LEGS); G_TCEPoseEndCommand(ent);
    CHECK(poseClients[0].generation!=i && poseClients[0].acceptedEpoch[0]==1);
}
static void lifecycle(void) {
    g_tcePoseStats_t stats;
    unsigned int i,oldAlloc,oldOpens;
    char badName[MAX_QPATH];
    G_TCEPoseShutdown(); CHECK(liveBytes==0 && allocations==frees);
    G_TCEPoseShutdown(); CHECK(liveBytes==0);
    CHECK(level.animScriptData.animationAssigned==NULL);
    G_TCEPoseInit(); level.animScriptData.animationAssigned=G_TCEPoseAnimationAssigned;
    CHECK(G_TCEPoseRegisterCharacter(&characters[0],&definition));
    {
        bg_characterDef_t missing=definition;
        oldAlloc=allocations; strcpy(missing.mesh,"missing.mdm");
        CHECK(!G_TCEPoseRegisterCharacter(&characters[1],&missing));
        CHECK(allocations==oldAlloc && pendingBuffer==NULL);
        oldOpens=opens; strcpy(missing.mesh,"../mesh-0.mdm");
        CHECK(!G_TCEPoseRegisterCharacter(&characters[1],&missing)); CHECK(opens==oldOpens);
    }
    oldAlloc=allocations; strcpy(badName,animations[0].mdxFileName); strcpy(animations[0].mdxFileName,"bad.mdx");
    CHECK(G_TCEPoseRegisterCharacter(&characters[1],&definition));
    CHECK(poseCharacters[1].usableAnimations==1 && poseCharacters[1].animationModels[0]==-1 &&
          poseCharacters[1].animationModels[1]>=0);
    CHECK(allocations==oldAlloc+1 && pendingBuffer==NULL && liveBytes==poseRegistry.borrowedBytes);
    oldOpens=opens; connectClient(2,1000); clients[2].pers.character=&characters[1]; G_TCEPoseCharacterChanged(&clients[2]);
    G_TCEPoseBeginCommand(&g_entities[2]); assign(2,0,ANIM_BP_BOTH); G_TCEPoseEndCommand(&g_entities[2]);
    CHECK(opens==oldOpens && !poseClients[2].clocks[0].valid);
    strcpy(animations[0].mdxFileName,badName);
    strcpy(animations[0].mdxFileName,"unloaded.mdx"); oldAlloc=allocations; oversized=1;
    CHECK(G_TCEPoseRegisterCharacter(&characters[1],&definition)); oversized=0;
    CHECK(poseCharacters[1].usableAnimations==1 && poseCharacters[1].animationModels[0]==-1);
    CHECK(allocations==oldAlloc && pendingBuffer==NULL && liveBytes==poseRegistry.borrowedBytes);
    failAllocation=1;
    CHECK(G_TCEPoseRegisterCharacter(&characters[1],&definition)); failAllocation=0;
    CHECK(poseCharacters[1].usableAnimations==1 && poseCharacters[1].animationModels[0]==-1);
    CHECK(allocations==oldAlloc && pendingBuffer==NULL && liveBytes==poseRegistry.borrowedBytes);
    strcpy(animations[0].mdxFileName,badName);
    for(i=1;i<G_TCE_POSE_CHARACTERS;++i) CHECK(G_TCEPoseRegisterCharacter(&characters[i],&definition));
    CHECK(!G_TCEPoseRegisterCharacter(&characters[G_TCE_POSE_CHARACTERS],&definition));
    G_TCEPoseStats(&stats); CHECK(stats.characters==G_TCE_POSE_CHARACTERS && stats.ownedBytes==liveBytes);
    G_TCEPoseResetCharacters(); CHECK(liveBytes==0 && allocations==frees);
    CHECK(!poseRegistry.skeletonCount && !poseRegistry.meshCount && !poseClients[0].occupied);
    G_TCEPoseShutdown(); CHECK(liveBytes==0);
    CHECK(successfulOpens==closes);
    for(i=1;i<32;++i) CHECK(files[i]==NULL);
}
static void partialRegistration(void) {
    /* Exact first declarations for all ten shipped MDX paths, with the real
     * body.aninc row50 inserted before the other nine model paths. The
     * original controller accepts its 3/13 declaration; readiness must check
     * its full cycle without changing metadata. The last row is deliberately
     * synthetic and invalid: its forward outlier would be negative. */
    static const struct { const char *file,*name; int first,frames,loops,fps; } rows[]={
        {"body","death_machinegun_1",0,32,0,20},
        {"body","firing_crch_2h",2088,3,13,22},
        {"mortar","e_mortar_stand",0,10,10,6},
        {"akimbo","firing_akimbo",0,4,0,15},
        {"prone_pistol","e_prone_pistol_switch",0,11,0,24},
        {"prone_rifle","e_prone_rifle_switch",0,10,0,24},
        {"prone_dual_pistols","e_prone_pistols_switch",0,6,0,24},
        {"prone_mortar","e_prone_mortar_stand",5,1,1,24},
        {"binocs","e_prone_binocs_rase",0,10,0,24},
        {"no_juice","e_no_juice",0,30,0,15},
        {"swim","e_swim_2h",0,30,30,20},
        {"body","fixture_negative_cycle",0,3,13,22}
    };
    unsigned int i,oldOpens;
    uint64_t rejected;
    g_tcePoseQuery_t query;
    tcePoseResult_t result;
    if(!fullAssetMode) return;
    G_TCEPoseInit(); level.animScriptData.animationAssigned=G_TCEPoseAnimationAssigned;
    memset(&modelInfo,0,sizeof(modelInfo)); memset(animations,0,sizeof(animations));
    for(i=0;i<sizeof(rows)/sizeof(rows[0]);++i) {
        animation_t *anim=&animations[i];
        snprintf(anim->mdxFileName,sizeof(anim->mdxFileName),"animations/human/base/%s.mdx",rows[i].file);
        strcpy(anim->name,rows[i].name); anim->firstFrame=rows[i].first; anim->numFrames=rows[i].frames;
        anim->loopFrames=rows[i].loops; anim->frameLerp=1000/rows[i].fps;
        anim->initialLerp=strstr(anim->name,"firing")?40:anim->frameLerp;
        anim->moveSpeed=i==1?-1:0; anim->flags=strstr(anim->name,"firing")?ANIMFL_FIRINGANIM:0;
        anim->duration=anim->initialLerp+anim->frameLerp*anim->numFrames;
        modelInfo.animations[i]=anim;
    }
    modelInfo.numAnimations=(int)(sizeof(rows)/sizeof(rows[0]));
    CHECK(G_TCEPoseRegisterCharacter(&characters[0],&definition));
    CHECK(poseRegistry.skeletonCount==10 && poseCharacters[0].ready);
    CHECK(poseCharacters[0].animationCount==12 && poseCharacters[0].usableAnimations==11);
    CHECK(poseCharacters[0].animationModels[1]>=0 && poseCharacters[0].animationModels[11]==-1);
    for(i=0;i<11;++i) {
        int model=poseCharacters[0].animationModels[i];
        CHECK(model>=0 && model<(int)poseRegistry.skeletonCount);
        if(model>=0) CHECK(!strcmp(poseRegistry.skeletons[model].name,animations[i].mdxFileName));
    }
    CHECK(animations[1].numFrames==3 && animations[1].loopFrames==13);
    connectClient(0,1000); level.time=2000; oldOpens=opens;
    G_TCEPoseBeginCommand(&g_entities[0]); clients[0].ps.commandTime=1010;
    assign(0,0,ANIM_BP_LEGS); assign(0,10,ANIM_BP_TORSO); G_TCEPoseEndCommand(&g_entities[0]);
    CHECK(G_TCEPoseQuery(0,1010,2000,&query) && query.commandPhaseMask==3);
    CHECK(query.channel[1].model==poseCharacters[0].animationModels[10]);
    CHECK(G_TCEPoseEvaluate(&query,identity,identity,TCE_POSE_MATH_OBSERVED_WINDOWS,&result));
    rejected=poseStats.rejected;
    G_TCEPoseBeginCommand(&g_entities[0]); clients[0].ps.commandTime=1020;
    assign(0,1,ANIM_BP_LEGS); G_TCEPoseEndCommand(&g_entities[0]);
    CHECK(G_TCEPoseQuery(0,1020,2000,&query) && query.commandPhaseMask==3);
    CHECK(poseStats.rejected==rejected && query.history.clocks[0].valid);
    G_TCEPoseBeginCommand(&g_entities[0]); clients[0].ps.commandTime=1195;
    G_TCEPoseEndCommand(&g_entities[0]);
    CHECK(G_TCEPoseQuery(0,1195,2000,&query) && query.commandPhaseMask==3);
    CHECK(query.channel[0].oldFrame==2078 && query.channel[0].frame==2088);
    CHECK(G_TCEPoseEvaluate(&query,identity,identity,TCE_POSE_MATH_OBSERVED_WINDOWS,&result));
    CHECK(animations[1].numFrames==3 && animations[1].loopFrames==13);
    G_TCEPoseBeginCommand(&g_entities[0]); clients[0].ps.commandTime=1205;
    assign(0,11,ANIM_BP_LEGS); G_TCEPoseEndCommand(&g_entities[0]);
    CHECK(G_TCEPoseQuery(0,1205,2000,&query) && query.commandPhaseMask==2);
    CHECK(poseStats.rejected==rejected+1 && !query.history.clocks[0].valid);
    G_TCEPoseBeginCommand(&g_entities[0]); clients[0].ps.commandTime=1215;
    assign(0,9,ANIM_BP_LEGS); G_TCEPoseEndCommand(&g_entities[0]);
    CHECK(G_TCEPoseQuery(0,1215,2000,&query) && query.commandPhaseMask==3);
    CHECK(query.channel[0].model==poseCharacters[0].animationModels[9]);
    CHECK(opens==oldOpens);
    /* The declared reversed first pass fits, but the extra cycle frame does
     * not. Both registration and a callback after metadata mutation must
     * reject it; a stale per-clip ready flag cannot bypass the model cap. */
    animations[1].firstFrame=(int)poseRegistry.skeletons[poseCharacters[0].animationModels[1]].model.frameCount-3;
    animations[1].flags|=ANIMFL_REVERSED;
    rejected=poseStats.rejected;
    G_TCEPoseBeginCommand(&g_entities[0]); clients[0].ps.commandTime=1225;
    assign(0,1,ANIM_BP_LEGS); G_TCEPoseEndCommand(&g_entities[0]);
    CHECK(G_TCEPoseQuery(0,1225,2000,&query) && query.commandPhaseMask==2);
    CHECK(poseStats.rejected==rejected+1 && !query.history.clocks[0].valid);
    CHECK(G_TCEPoseRegisterCharacter(&characters[0],&definition));
    CHECK(poseCharacters[0].animationModels[1]==-1 && poseCharacters[0].usableAnimations==10);
    animations[1].firstFrame=2088; animations[1].flags&=~ANIMFL_REVERSED;
    CHECK(G_TCEPoseRegisterCharacter(&characters[0],&definition));
    CHECK(poseCharacters[0].animationModels[1]>=0 && poseCharacters[0].usableAnimations==11);
    CHECK(opens==oldOpens);
    /* A group with no valid clips still fails, rather than guessing phase. */
    modelInfo.numAnimations=2; modelInfo.animations[0]=&animations[11]; modelInfo.animations[1]=NULL;
    CHECK(!G_TCEPoseRegisterCharacter(&characters[0],&definition));
    CHECK(!poseCharacters[0].ready && poseCharacters[0].usableAnimations==0);
    G_TCEPoseShutdown(); CHECK(liveBytes==0 && allocations==frees);
}
#ifdef G_TCE_POSE_ADAPTER_FIXTURES
static bg_character_t *adapterCharacter(unsigned int i) {
    const char *file=adapterCharacters[i].character;
    int cls=strstr(file,"/soldier.")?0:strstr(file,"/medic.")?1:strstr(file,"/engineer.")?2:
        strstr(file,"/fieldops.")?3:strstr(file,"/cvops.")?4:strstr(file,"/elite.")?5:6;
    return BG_GetCharacter(strstr(file,"/allied/")?TEAM_ALLIES:TEAM_AXIS,cls);
}
static void adapterDefinition(unsigned int i,bg_characterDef_t *out) {
    memset(out,0,sizeof(*out));strcpy(out->mesh,adapterCharacters[i].mesh);strcpy(out->hudhead,adapterCharacters[i].head);
    strcpy(out->animationGroup,adapterCharacters[i].group);strcpy(out->animationScript,adapterCharacters[i].script);
}
static void surfaceAdapter(void) {
    g_tcePoseAssetInfo_t info[14],unavailable;
    g_tcePosePresentationMetadata_t metadata;
    g_tcePoseClipMetadata_t clipMetadata;
    g_tcePoseTraceResult_t result,saved;
    g_tcePresentationQuery_t query,changed;
    unsigned int i,j,n,oldOpens,oldAlloc;
    unsigned int positive[3]={0,0,0},misses=0,gaps=0;
    uint32_t previousGeneration=0;
    uint32_t profiles[64],sentinel[1]={0x12345678u};
    float changedEnd[3];
    CHECK(fullAssetMode);
    G_TCEPoseInit();level.animScriptData.animationAssigned=G_TCEPoseAnimationAssigned;
    memset(&modelInfo,0,sizeof(modelInfo));memset(animations,0,sizeof(animations));
    strcpy(modelInfo.animationGroup,adapterCharacters[0].group);strcpy(modelInfo.animationScript,adapterCharacters[0].script);
    modelInfo.numAnimations=2;
    for(i=0;i<2;++i) {
        const AdapterAnimation *row=&adapterAnimations[i];animation_t *a=&animations[i];
        strcpy(a->name,row->name);strcpy(a->mdxFileName,row->model);a->firstFrame=row->first;a->numFrames=row->n;
        a->loopFrames=row->loops;a->frameLerp=row->f;a->initialLerp=row->i;a->moveSpeed=row->speed;
        a->flags=row->flags;a->movetype=row->movetype;a->duration=row->i+row->n*row->f;modelInfo.animations[i]=a;
    }
    for(i=0;i<14;++i) {
        bg_character_t *character=adapterCharacter(i);character->animModelInfo=&modelInfo;
        strcpy(character->characterFile,adapterCharacters[i].character);adapterDefinition(i,&definition);
        CHECK(G_TCEPoseRegisterCharacter(character,&definition));CHECK(G_TCEPoseAssetInfo(character,&info[i]));
        CHECK(info[i].profile!=0 && !strcmp(info[i].mesh,definition.mesh) && !strcmp(info[i].hudhead,definition.hudhead));
        CHECK(info[i].rawProfile==BG_TCEPosePresentationProfile(character,&definition));
        CHECK(info[i].profile==BG_TCEPosePresentationSpeedProfile(info[i].rawProfile,adapterExpectedSpeeds[i],2));
        for(j=0;j<i;++j)CHECK(info[j].profile!=info[i].profile);
        CHECK(G_TCEPoseClipMetadata(character,0,&clipMetadata));
        CHECK(clipMetadata.moveSpeed==adapterExpectedSpeeds[i][0] && animations[0].moveSpeed==-1);
    }
    CHECK(poseRegistry.skeletonCount==2 && poseRegistry.meshCount==ADAPTER_UNIQUE_MESHES);
    CHECK(poseSurfaces.meshCount==ADAPTER_UNIQUE_MESHES && poseSurfaces.headCount==1);
    CHECK(poseBufferCount==ADAPTER_UNIQUE_MESHES+3 && poseStats.ownedBytes==liveBytes);
    CHECK(info[2].bodyFingerprint!=info[5].bodyFingerprint && info[2].headFingerprint==info[5].headFingerprint);
    CHECK(G_TCEPosePresentationProfiles(profiles,64)==40);
    for(i=1;i<40;++i)CHECK(profiles[i-1]<profiles[i]);
    CHECK(G_TCEPosePresentationProfiles(sentinel,1)==-1 && sentinel[0]==0x12345678u);
    oldOpens=opens;
    for(n=0;n<sizeof(adapterCases)/sizeof(adapterCases[0]);++n) {
        const AdapterCase *f=&adapterCases[n];gentity_t *ent=&g_entities[0];
        connectClient(0,1000+(int)n*500);clients[0].pers.character=adapterCharacter(f->originalCharacter);
        clients[0].sess.sessionTeam=strstr(adapterCharacters[f->originalCharacter].character,"/allied/")?TEAM_ALLIES:TEAM_AXIS;
        G_TCEPoseCharacterChanged(&clients[0]);ent->s.eType=ET_PLAYER;ent->health=100;
        ent->s.effect1Time=f->effect1Time;ent->s.powerups=f->powerups;
        level.time=2000+(int)n*500;G_TCEPoseBeginCommand(ent);assign(0,(int)f->animation,ANIM_BP_BOTH);G_TCEPoseEndCommand(ent);
        CHECK(G_TCEPosePresentationMetadata(ent,&metadata));CHECK(metadata.generation!=previousGeneration);
        CHECK(metadata.renderCharacter==adapterCharacter(f->character) && metadata.animationCharacter==adapterCharacter(f->animationCharacter));
        CHECK(metadata.moveSpeed[0]==adapterExpectedSpeeds[f->character][f->animation]);
        previousGeneration=metadata.generation;
        memset(&query,0,sizeof(query));query.generation=metadata.generation;query.mapGeneration=metadata.mapGeneration;
        query.profile=metadata.profile;query.clientNum=0;query.mode=f->mode;query.sourceTime=query.serverTime=(uint32_t)level.time;
        query.renderCharacter=metadata.renderCharacter;query.animationCharacter=metadata.animationCharacter;
        memcpy(query.origin,f->origin,sizeof(query.origin));memcpy(query.axes.body,f->body,sizeof(query.axes.body));
        memcpy(query.axes.torso,f->torso,sizeof(query.axes.torso));memcpy(query.axes.head,f->head,sizeof(query.axes.head));
        for(i=0;i<2;++i) {
            const animation_t *a=metadata.animation[i];query.animation[i]=metadata.fullAnimation[i];query.acceptedEpoch[i]=metadata.acceptedEpoch[i];
            query.clip[i].firstFrame=a->firstFrame;query.clip[i].numFrames=a->numFrames;query.clip[i].loopFrames=a->loopFrames;
            query.clip[i].frameLerp=a->frameLerp;query.clip[i].initialLerp=a->initialLerp;query.clip[i].reversed=!!(a->flags&ANIMFL_REVERSED);
            query.code[i]=f->code;query.rateQ8[i]=0;
            CHECK(BG_TCEPoseCompactSample(&query.clip[i],query.code[i],query.sourceTime,query.serverTime,0,&query.channel[i]));
            CHECK(query.channel[i].elapsedQ16==f->elapsed);
        }
        for(j=0;j<sizeof(adapterRays)/sizeof(adapterRays[0]);++j) if(adapterRays[j].pose==n) {
            const AdapterRay *r=&adapterRays[j];
            CHECK((int)G_TCEPosePresentationTrace(&query,r->start,r->end,r->limit,&result)==r->state);
            CHECK((int)result.surface.region==r->region && fabs(result.surface.fraction-r->fraction)<.00002);
            CHECK(G_TCEPosePresentationTraceMatches(&result,&query,r->start,r->end,r->limit));
            if(result.state==G_TCE_POSE_TRACE_HIT) {
                ++positive[result.surface.region];
                for(i=0;i<3;++i)CHECK(result.nominal[i]==(i==(unsigned int)r->region?(i==0?2.f:i==1?1.f:.5f):0.f));
                if(result.surface.region==TCE_POSE_REGION_HEAD)for(i=0;i<3;++i)CHECK(fabs(result.anchor[i]-f->headAnchor[i])<.0004);
            } else {
                ++misses;CHECK(result.state==G_TCE_POSE_TRACE_MISS);
                CHECK(result.nominal[0]==0 && result.nominal[1]==0 && result.nominal[2]==0);
            }
            if(!strcmp(r->label,"leg-gap")) { CHECK(result.state==G_TCE_POSE_TRACE_MISS);++gaps; }
            saved=result;changed=query;changed.generation++;
            CHECK(!G_TCEPosePresentationTraceMatches(&result,&changed,r->start,r->end,r->limit));
            CHECK(G_TCEPosePresentationTrace(&changed,r->start,r->end,r->limit,&result)==G_TCE_POSE_TRACE_UNSUPPORTED);
            CHECK(result.state==G_TCE_POSE_TRACE_UNSUPPORTED);
            changed=query;changed.channel[0].frame++;
            CHECK(!G_TCEPosePresentationTraceMatches(&saved,&changed,r->start,r->end,r->limit));
            CHECK(G_TCEPosePresentationTrace(&changed,r->start,r->end,r->limit,&result)==G_TCE_POSE_TRACE_UNSUPPORTED);
            changed=query;changed.rateQ8[0]=1;
            CHECK(!G_TCEPosePresentationTraceMatches(&saved,&changed,r->start,r->end,r->limit));
            memcpy(changedEnd,r->end,sizeof(changedEnd));changedEnd[0]+=1;
            CHECK(!G_TCEPosePresentationTraceMatches(&saved,&query,r->start,changedEnd,r->limit));
            clients[0].tceRewind.active=qtrue;clients[0].ps.origin[0]+=100;
            CHECK(G_TCEPosePresentationTraceMatches(&saved,&query,r->start,r->end,r->limit));
            clients[0].tceRewind.active=qfalse;
        }
        CHECK(positive[0] && positive[1] && positive[2]);
        ent->s.eFlags|=0x01000000;CHECK(!G_TCEPosePresentationMetadata(ent,&metadata));ent->s.eFlags&=~0x01000000;
        ent->s.eType=ET_CORPSE;CHECK(!G_TCEPosePresentationMetadata(ent,&metadata));ent->s.eType=ET_PLAYER;
        changed=query;changed.profile++;CHECK(G_TCEPosePresentationTrace(&changed,adapterRays[0].start,adapterRays[0].end,1,&result)==G_TCE_POSE_TRACE_UNSUPPORTED);
    }
    CHECK(misses>=18 && gaps==1 && opens==oldOpens);
    adapterDefinition(0,&definition);strcpy(definition.hudhead,"adapter-head-copy.md3");oldAlloc=allocations;
    failAllocation=1;CHECK(G_TCEPoseRegisterCharacter(adapterCharacter(0),&definition));failAllocation=0;
    CHECK(!G_TCEPoseAssetInfo(adapterCharacter(0),&unavailable) && allocations==oldAlloc);
    CHECK(G_TCEPoseRegisterCharacter(adapterCharacter(0),&definition));CHECK(G_TCEPoseAssetInfo(adapterCharacter(0),&unavailable));
    CHECK(poseStats.ownedBytes==liveBytes && poseStats.ownedBytes<=TCE_POSE_MAX_CACHE_BYTES);
    oldAlloc=allocations;strcpy(definition.hudhead,"adapter-head-overcap.md3");
    CHECK(G_TCEPoseRegisterCharacter(adapterCharacter(0),&definition));
    CHECK(!G_TCEPoseAssetInfo(adapterCharacter(0),&unavailable) && allocations==oldAlloc);
    G_TCEPoseResetClient(&g_entities[0]);
    CHECK(!G_TCEPosePresentationTraceMatches(&saved,&query,adapterRays[0].start,adapterRays[0].end,1));
    G_TCEPoseShutdown();CHECK(liveBytes==0 && allocations==frees && successfulOpens==closes);
    /* All actual raw group declarations: script-derived movetype is outside
     * this fixture. Check registration/speed bounds without inventing it. */
    G_TCEPoseInit();level.animScriptData.animationAssigned=G_TCEPoseAnimationAssigned;
    memset(&modelInfo,0,sizeof(modelInfo));memset(animations,0,sizeof(animations));
    strcpy(modelInfo.animationGroup,adapterCharacters[0].group);strcpy(modelInfo.animationScript,adapterCharacters[0].script);
    modelInfo.numAnimations=(int)(sizeof(adapterFullAnimations)/sizeof(adapterFullAnimations[0]));
    CHECK(modelInfo.numAnimations==236);
    for(i=0;i<(unsigned int)modelInfo.numAnimations;++i) {
        const AdapterAnimation *row=&adapterFullAnimations[i];animation_t *a=&animations[i];
        strcpy(a->name,row->name);strcpy(a->mdxFileName,row->model);a->firstFrame=row->first;a->numFrames=row->n;
        a->loopFrames=row->loops;a->frameLerp=row->f;a->initialLerp=row->i;a->moveSpeed=row->speed;
        a->flags=row->flags;a->movetype=row->movetype;a->duration=row->i+row->n*row->f;modelInfo.animations[i]=a;
    }
    for(i=0;i<14;++i) {
        bg_character_t *character=adapterCharacter(i);character->animModelInfo=&modelInfo;adapterDefinition(i,&definition);
        CHECK(G_TCEPoseRegisterCharacter(character,&definition));CHECK(G_TCEPoseAssetInfo(character,&unavailable));
        CHECK(poseCharacters[i].animationCount==236 && poseCharacters[i].usableAnimations==236);
        for(j=0;j<236;++j)CHECK(G_TCEPoseClipMetadata(character,(int)j,&clipMetadata));
        CHECK(G_TCEPoseClipMetadata(character,50,&clipMetadata));
        CHECK(clipMetadata.firstFrame==2088 && clipMetadata.numFrames==3 && clipMetadata.loopFrames==13);
        CHECK(G_TCEPoseClipMetadata(character,94,&clipMetadata));CHECK(clipMetadata.moveSpeed==adapterExpectedSpeeds[i][0]);
    }
    CHECK(poseRegistry.skeletonCount==10 && poseSurfaces.meshCount==ADAPTER_UNIQUE_MESHES && poseSurfaces.headCount==1);
    CHECK(poseStats.ownedBytes==liveBytes && liveBytes<=TCE_POSE_MAX_CACHE_BYTES);
    CHECK(G_TCEPosePresentationProfiles(profiles,64)==40);
    G_TCEPoseShutdown();CHECK(liveBytes==0 && allocations==frees && successfulOpens==closes);
}
#endif
static void printLayout(void) {
#define FIELD(type,name) printf("\"%s\":%lu,",#name,(unsigned long)offsetof(type,name))
    printf("{\"pointer_bytes\":%lu,\"client\":{",(unsigned long)sizeof(void*));
    FIELD(g_tcePoseClient_t,generation); FIELD(g_tcePoseClient_t,occupied); FIELD(g_tcePoseClient_t,connectTime);
    FIELD(g_tcePoseClient_t,spawnCount); FIELD(g_tcePoseClient_t,character); FIELD(g_tcePoseClient_t,inCommand);
    FIELD(g_tcePoseClient_t,characterPointer);
    FIELD(g_tcePoseClient_t,clocks); FIELD(g_tcePoseClient_t,acceptedEpoch); FIELD(g_tcePoseClient_t,acceptedCommandTime);
    FIELD(g_tcePoseClient_t,acceptedReceiptTime); FIELD(g_tcePoseClient_t,context); FIELD(g_tcePoseClient_t,sequence);
    FIELD(g_tcePoseClient_t,eventHead); FIELD(g_tcePoseClient_t,eventCount); FIELD(g_tcePoseClient_t,historyHead);
    FIELD(g_tcePoseClient_t,historyCount); FIELD(g_tcePoseClient_t,events); FIELD(g_tcePoseClient_t,history);
    printf("\"size\":%lu},\"clock\":{",(unsigned long)sizeof(g_tcePoseClient_t));
    FIELD(tcePoseClock_t,clip); FIELD(tcePoseClock_t,elapsedQ16); FIELD(tcePoseClock_t,time);
    FIELD(tcePoseClock_t,generation); FIELD(tcePoseClock_t,epoch); FIELD(tcePoseClock_t,rateQ16);
    FIELD(tcePoseClock_t,animation); FIELD(tcePoseClock_t,valid);
    printf("\"size\":%lu},\"event\":{",(unsigned long)sizeof(tcePoseClock_t));
    FIELD(g_tcePoseEvent_t,sequence); FIELD(g_tcePoseEvent_t,commandTime); FIELD(g_tcePoseEvent_t,receiptTime);
    FIELD(g_tcePoseEvent_t,acceptedEpoch); FIELD(g_tcePoseEvent_t,channel); FIELD(g_tcePoseEvent_t,animation);
    FIELD(g_tcePoseEvent_t,previous);
    FIELD(g_tcePoseEvent_t,context); FIELD(g_tcePoseEvent_t,clock);
    printf("\"size\":%lu},\"history\":{",(unsigned long)sizeof(g_tcePoseEvent_t));
    FIELD(g_tcePoseHistory_t,generation); FIELD(g_tcePoseHistory_t,commandTime); FIELD(g_tcePoseHistory_t,receiptTime);
    FIELD(g_tcePoseHistory_t,acceptedEpoch); FIELD(g_tcePoseHistory_t,acceptedCommandTime); FIELD(g_tcePoseHistory_t,acceptedReceiptTime);
    FIELD(g_tcePoseHistory_t,sequence); FIELD(g_tcePoseHistory_t,character); FIELD(g_tcePoseHistory_t,animation);
    FIELD(g_tcePoseHistory_t,clientNum); FIELD(g_tcePoseHistory_t,eFlags); FIELD(g_tcePoseHistory_t,pmFlags);
    FIELD(g_tcePoseHistory_t,weaponFlags); FIELD(g_tcePoseHistory_t,weapon); FIELD(g_tcePoseHistory_t,velocity); FIELD(g_tcePoseHistory_t,lean);
    FIELD(g_tcePoseHistory_t,context); FIELD(g_tcePoseHistory_t,origin); FIELD(g_tcePoseHistory_t,viewangles);
    FIELD(g_tcePoseHistory_t,clocks);
    printf("\"size\":%lu},\"character\":{",(unsigned long)sizeof(g_tcePoseHistory_t));
    FIELD(g_tcePoseCharacter_t,character); FIELD(g_tcePoseCharacter_t,model); FIELD(g_tcePoseCharacter_t,mesh);
    FIELD(g_tcePoseCharacter_t,ready); FIELD(g_tcePoseCharacter_t,animationCount);
    FIELD(g_tcePoseCharacter_t,usableAnimations); FIELD(g_tcePoseCharacter_t,animationModels);
    printf("\"size\":%lu},\"registry\":{",(unsigned long)sizeof(g_tcePoseCharacter_t));
    FIELD(tcePoseRegistry_t,skeletonCount); FIELD(tcePoseRegistry_t,meshCount); FIELD(tcePoseRegistry_t,skeletons); FIELD(tcePoseRegistry_t,meshes);
    printf("\"skeleton_entry_size\":%lu,\"size\":%lu},\"clock_clip\":{",(unsigned long)sizeof(tcePoseSkeletonEntry_t),(unsigned long)sizeof(tcePoseRegistry_t));
    FIELD(tcePoseClockClip_t,model); FIELD(tcePoseClockClip_t,firstFrame); FIELD(tcePoseClockClip_t,numFrames);
    FIELD(tcePoseClockClip_t,loopFrames); FIELD(tcePoseClockClip_t,frameLerp); FIELD(tcePoseClockClip_t,initialLerp); FIELD(tcePoseClockClip_t,reversed);
    printf("\"size\":%lu}}\n",(unsigned long)sizeof(tcePoseClockClip_t));
#undef FIELD
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--layout")) { printLayout(); return 0; }
    if(argc<2 || argc>3) return 2;
    assetDirectory=argv[1]; fullAssetMode=argc==3; setup(); if(failures) return 1;
    identityControls(); epochs(); rewindAndOverflow(); wrappedTimelines(); completeRegistry(); lifecycle(); partialRegistration();
#ifdef G_TCE_POSE_ADAPTER_FIXTURES
    surfaceAdapter();
#endif
    printf("{\"checks\":%u,\"failures\":%u,\"allocations\":%u,\"frees\":%u,\"peak_bytes\":%lu,\"fs_attempts\":%u,\"fs_opened\":%u,\"fs_closes\":%u}\n",checks,failures,allocations,frees,(unsigned long)peakBytes,opens,successfulOpens,closes);
    return failures?1:0;
}
