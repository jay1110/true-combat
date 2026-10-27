#include "cg_local.h"
#include "tce_numeric.h"
#include "cg_tce_pose_presentation.h"
#include "tools/modernize/pose_presentation_runtime_cases.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#undef vsnprintf
#include "tools/modernize/pose_presentation_trajectory_services.h"
static unsigned int checks,tagCalls;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define NEAR(a,b) CHECK(fabs((double)(a)-(double)(b))<2e-5)
cg_t cg;cgs_t cgs;int tceSmokeNewBBox=1;
bg_character_t axisClassCharacters[NUM_PLAYER_CLASSES],alliedClassCharacters[NUM_PLAYER_CLASSES];
static bg_characterDef_t fixtureDefinition;
static animModelInfo_t fixtureModel;
static animation_t animations[236];
static char manifest[TCE_POSE_PRESENTATION_MANIFEST],fixtureAck[24],ownInfo[40],serverinfo[128]="\\tce_poseProtocol\\2\\tce_poseStep\\50";
static float calibration=1.725f;
static int tagFailure;
static int BG_WeaponOnBackToWeap(int weapon) {return weapon;}
/* Origin visibility/attachment scope is exercised with the complete actual
 * culling and sprite/shadow/splash bodies by visibility_regression.py. */
void CG_CalcEntityVisibility(centity_t *cent) {(void)cent;abort();}
/* ACTUAL_PLAYER_CONVERTERS */
/* ACTUAL_CHARACTER_SELECTORS */
/* ACTUAL_ENTITY_TRAJECTORY */
void CG_AdjustPositionForMover(const vec3_t in,int mover,int from,int to,vec3_t out,vec3_t angles) {
    (void)from;(void)to;(void)angles;CHECK(mover==ENTITYNUM_NONE);VectorCopy(in,out);
}
/* ACTUAL_POSITION_FUNCTIONS */
char *Info_ValueForKey(const char *info,const char *key) {
    static char value[MAX_INFO_STRING];const char *p=info;size_t size=strlen(key);
    value[0]=0;while(*p) {
        const char *begin,*end;if(*p=='\\')++p;begin=p;
        while(*p&&*p!='\\')++p;
        if(!*p)break;
        ++p;end=p;while(*p&&*p!='\\')++p;
        if((size_t)(end-begin-1)==size&&!memcmp(begin,key,size)) {
            size_t n=(size_t)(p-end);if(n>=sizeof(value))return value;
            memcpy(value,end,n);value[n]=0;return value;
        }
    }return value;
}
void QDECL Com_sprintf(char *out,int size,const char *format,...) {
    va_list args;va_start(args,format);vsnprintf(out,(size_t)size,format,args);va_end(args);
}
void AxisClear(vec3_t axis[3]) {int i;memset(axis,0,9*sizeof(float));for(i=0;i<3;++i)axis[i][i]=1;}
void AxisCopy(vec3_t in[3],vec3_t out[3]) {memcpy(out,in,9*sizeof(float));}
void trap_Cvar_Register(vmCvar_t *value,const char *name,const char *text,int flags) {
    (void)value;(void)name;(void)text;(void)flags;
}
void trap_Cvar_Set(const char *name,const char *text) {
    CHECK(!strcmp(name,"tce_pose"));CHECK(strlen(text)<sizeof(fixtureAck));strcpy(fixtureAck,text);
}
const char *CG_ConfigString(int index) {
    if(index==CS_SERVERINFO)return serverinfo;
    if(index==TCE_POSE_PRESENTATION_CS)return manifest;
    if(index==CS_PLAYERS+cg.clientNum)return ownInfo;
    return "";
}
int trap_R_LerpTag(orientation_t *tag,const refEntity_t *body,const char *name,int start) {
    int right=!strcmp(name,"tag_footright"),ladder=body->frameModel==111;
    int first=ladder?2981:57;float progress=(body->frame-first)*calibration;
    ++tagCalls;CHECK(!start);CHECK(!strcmp(name,"tag_footleft")||right);
    CHECK(body->frame==body->oldframe&&body->frame==body->torsoFrame&&body->frame==body->oldTorsoFrame);
    CHECK(body->frameModel==body->oldframeModel&&body->frameModel==body->torsoFrameModel&&body->frameModel==body->oldTorsoFrameModel);
    CHECK(body->axis[0][0]==1&&body->torsoAxis[2][2]==1&&!body->backlerp&&!body->torsoBacklerp);
    if(tagFailure)return -1;
    memset(tag,0,sizeof(*tag));
    tag->origin[0]=ladder?(right?1.0f:2.0f):progress;
    tag->origin[2]=ladder?progress:(right?0.0f:-1.0f);return 0;
}
/* ACTUAL_PRESENTATION */
static void compare(const refEntity_t *body,const refEntity_t *head,const presentationRuntimeCase_t *row) {
    int i,j;
    if(body->oldframe!=row->channel[0].oldFrame||body->frame!=row->channel[0].frame)
        fprintf(stderr,"frames actual%d,%d expected%d,%d code%d time%d/%d rate%g\n",body->oldframe,body->frame,row->channel[0].oldFrame,row->channel[0].frame,row->state[0].time,cg.snap->serverTime,cg.time,cg.predictedPlayerEntity.pe.legs.animSpeedScale);
    CHECK(body->oldframe==row->channel[0].oldFrame&&body->frame==row->channel[0].frame);
    CHECK(body->oldTorsoFrame==row->channel[1].oldFrame&&body->torsoFrame==row->channel[1].frame);
    CHECK(body->oldframeModel==111&&body->frameModel==111&&body->oldTorsoFrameModel==222&&body->torsoFrameModel==222);
    NEAR(body->backlerp,row->channel[0].backlerp);NEAR(body->torsoBacklerp,row->channel[1].backlerp);
    for(i=0;i<3;++i)for(j=0;j<3;++j) {
        NEAR(body->axis[i][j],row->axes.body[i][j]);NEAR(body->torsoAxis[i][j],row->axes.torso[i][j]);NEAR(head->axis[i][j],row->axes.head[i][j]);
    }
}
int main(int argc,char **argv) {
    presentationRuntimeCase_t row,variants[PRESENTATION_RUNTIME_CASES];FILE *file;refEntity_t body,head;centity_t remote;
    bg_character_t *character=&axisClassCharacters[0],*carrier=&axisClassCharacters[6];
    unsigned int tags;uint32_t localProfile;int i;
    CHECK(argc==2);file=fopen(argv[1],"rb");CHECK(file!=NULL);CHECK(fread(variants,sizeof(variants),1,file)==1);CHECK(!fclose(file));row=variants[0];CHECK(row.signature==0x504f5332u);
    PresentationRuntimeSpline();
    Com_sprintf(ownInfo,sizeof(ownInfo),"\\tcep\\%08x",(unsigned)row.nonce);
    strcpy(manifest,row.manifest);PresentationRuntimeModel(character,&fixtureDefinition,&fixtureModel,animations);
    CG_TCEPresentationInit();CG_TCEPresentationRememberModel(&fixtureModel);CG_TCEPresentationRegister(character,&fixtureDefinition);
    CHECK(presentationCharacters[0].moveSpeed[94]==69&&presentationCharacters[0].moveSpeed[222]==69);
    CHECK(presentationCharacters[0].profile&&presentationDigest&&fixtureAck[0]=='2');
    localProfile=presentationCharacters[0].profile;tags=tagCalls;
    cg.snap=&cg.activeSnapshots[0];cg.nextSnap=&cg.activeSnapshots[1];cg.time=row.queryTime;cgs.tceLeanMode=1;
    for(i=0;i<2;++i) {cg.activeSnapshots[i].serverTime=row.time[i];cg.activeSnapshots[i].ps=row.ps[i];}
    cgs.clientinfo[0].infoValid=qtrue;cgs.clientinfo[0].character=character;cgs.clientinfo[0].team=TEAM_AXIS;
    memset(&remote,0,sizeof(remote));remote.currentState=row.state[0];remote.nextState=row.state[1];remote.interpolate=qtrue;
    memset(&body,0,sizeof(body));memset(&head,0,sizeof(head));body.origin[0]=999;
    for(i=0;i<PRESENTATION_RUNTIME_CASES;++i) {
        int axis;remote.currentState=variants[i].state[0];remote.nextState=variants[i].state[1];
        remote.interpolate=(qboolean)variants[i].hasNext;cg.nextSnap=variants[i].hasNext?&cg.activeSnapshots[1]:NULL;
        cg.snap->serverTime=variants[i].time[0];cg.time=variants[i].queryTime;
        if(cg.nextSnap)cg.nextSnap->serverTime=variants[i].time[1];
        Com_sprintf(serverinfo,sizeof(serverinfo),"\\tce_poseProtocol\\2\\tce_poseStep\\%d",variants[i].step);
        cg.snap->numEntities=variants[i].moverCount;
        for(axis=0;axis<variants[i].moverCount;++axis)cg.snap->entities[axis]=variants[i].movers[axis];
        if(!variants[i].moverCount) {
            cg.frameInterpolation=.5f;CG_CalcEntityLerpPositions(&remote);
            for(axis=0;axis<3;++axis)NEAR(remote.lerpOrigin[axis],variants[i].origin[axis]);
        }
        {int prepared=CG_TCEPresentationPrepare(&remote,character,&body,&head);
         if(!prepared)fprintf(stderr,"origin tuple%d time%d/%d next%d step%d ground%d phase%d manifest%s\n",i,cg.time,cg.snap->serverTime,variants[i].hasNext,variants[i].step,remote.currentState.groundEntityNum,remote.currentState.time,variants[i].manifest);
         CHECK(prepared);}compare(&body,&head,&variants[i]);
        for(axis=0;axis<3;++axis)NEAR(body.origin[axis],variants[i].origin[axis]);
    }
    cg.snap->serverTime=row.time[0];cg.time=row.queryTime;cg.nextSnap=&cg.activeSnapshots[1];remote.interpolate=qtrue;
    cg.nextSnap->serverTime=row.time[1];
    strcpy(serverinfo,"\\tce_poseProtocol\\2\\tce_poseStep\\50");
    remote.currentState=row.state[0];remote.nextState=row.state[1];
    CHECK(CG_TCEPresentationPrepare(&remote,character,&body,&head));
    compare(&body,&head,&row);NEAR(body.origin[0],row.origin[0]);NEAR(body.origin[1],row.origin[1]);NEAR(body.origin[2],row.origin[2]);
    CHECK(tagCalls==tags); /* No asset evaluation or registration I/O in draw. */
    cg.predictedPlayerEntity.currentState=row.state[0];
    cg.predictedPlayerEntity.currentState.legsAnim=17;cg.predictedPlayerEntity.currentState.torsoAnim=19;
    cg.predictedPlayerState.legsAnim=17;cg.predictedPlayerState.torsoAnim=19;
    VectorSet(cg.predictedPlayerEntity.lerpOrigin,123,-45,67);
    CHECK(CG_TCEPresentationPrepare(&cg.predictedPlayerEntity,character,&body,&head));compare(&body,&head,&row);
    CHECK(body.origin[0]==123&&body.origin[1]==-45&&body.origin[2]==67);
    CHECK(cg.predictedPlayerEntity.pe.legs.animationNumber==606&&cg.predictedPlayerEntity.currentState.legsAnim==17);
    cg.snap->ps.holdable[7]=0;CHECK(!CG_TCEPresentationPrepare(&cg.predictedPlayerEntity,character,&body,&head));
    CHECK(!cg.predictedPlayerEntity.pe.legs.animation);cg.snap->ps.holdable[7]=row.ps[0].holdable[7];
    cg.nextSnap->serverTime=300;CHECK(!CG_TCEPresentationPrepare(&remote,character,&body,&head));cg.nextSnap->serverTime=250;
    cg.time=302;CHECK(!CG_TCEPresentationPrepare(&remote,character,&body,&head));cg.time=row.queryTime;
    remote.currentState.time=0;remote.pe.legs.frame=777;
    CHECK(!CG_TCEPresentationPrepare(&remote,character,&body,&head)&&remote.pe.legs.frame==777);remote.currentState.time=row.state[0].time;
    CHECK(CG_TCEPresentationPrepare(&remote,character,&body,&head));
    remote.currentState.eType=ET_CORPSE;CHECK(!CG_TCEPresentationPrepare(&remote,character,&body,&head));remote.currentState.eType=ET_PLAYER;
    remote.currentState.eFlags|=0x01000000;CHECK(!CG_TCEPresentationPrepare(&remote,character,&body,&head));remote.currentState.eFlags&=~0x01000000;
    /* Exact carrier render selection while retaining the source animation
     * character. Shared table is required; different uncalibrated tables fail. */
    *carrier=*character;strcpy(carrier->characterFile,"characters/carrier.char");carrier->mesh=251;
    CG_TCEPresentationRegister(carrier,&fixtureDefinition);remote.currentState.effect1Time|=0x20;remote.nextState.effect1Time|=0x20;
    CHECK(CG_TCEPresentationPrepare(&remote,carrier,&body,&head));compare(&body,&head,&row);
    CHECK(!CG_TCEPresentationPrepare(&remote,character,&body,&head));
    remote.currentState.effect1Time=row.state[0].effect1Time;remote.nextState.effect1Time=row.state[1].effect1Time;
    /* Disguise uses actual helper selection, not the real client's family. */
    alliedClassCharacters[0]=*character;strcpy(alliedClassCharacters[0].characterFile,"characters/disguise.char");
    CG_TCEPresentationRegister(&alliedClassCharacters[0],&fixtureDefinition);
    remote.currentState.powerups=remote.nextState.powerups=1<<PW_OPS_DISGUISED;
    CHECK(CG_TCEPresentationPrepare(&remote,&alliedClassCharacters[0],&body,&head));compare(&body,&head,&row);
    remote.currentState.powerups=remote.nextState.powerups=0;
    calibration=1.7f;CG_TCEPresentationRegister(character,&fixtureDefinition);
    CHECK(presentationCharacters[0].profile!=localProfile&&!presentationDigest&&!strcmp(fixtureAck,"0"));
    CHECK(!CG_TCEPresentationPrepare(&remote,character,&body,&head));
    calibration=1.725f;CG_TCEPresentationRegister(character,&fixtureDefinition);CHECK(presentationDigest);
    tagFailure=1;CG_TCEPresentationRegister(character,&fixtureDefinition);CHECK(!presentationDigest);
    CHECK(presentationCharacters[0].moveSpeed[94]==INT_MIN);tagFailure=0;
    CG_TCEPresentationRegister(character,&fixtureDefinition);CHECK(presentationDigest);
    manifest[2]=manifest[2]=='1'?'2':'1';CG_TCEPresentationManifestChanged();CHECK(!presentationDigest&&!strcmp(fixtureAck,"0"));
    strcpy(manifest,row.manifest);CG_TCEPresentationManifestChanged();CHECK(presentationDigest);
    {uint32_t oldNonce=presentationNonce;
     Com_sprintf(ownInfo,sizeof(ownInfo),"\\tcep\\%08x",(unsigned)(oldNonce+1u));CG_TCEPresentationManifestChanged();
     CHECK(presentationDigest&&presentationNonce==oldNonce+1u&&strlen(fixtureAck)==19);
     strcpy(ownInfo,"\\tcep\\00000000");CG_TCEPresentationManifestChanged();CHECK(!presentationDigest&&!strcmp(fixtureAck,"0"));
     Com_sprintf(ownInfo,sizeof(ownInfo),"\\tcep\\%08x",(unsigned)oldNonce);CG_TCEPresentationManifestChanged();CHECK(presentationDigest);}
    CG_TCEPresentationRestart();CHECK(!presentationActive[0]&&presentationDigest);
    printf("{\"checks\":%u,\"registration_tag_calls\":%u,\"entityState\":%u,\"playerState\":%u}\n",
        checks,tagCalls,(unsigned)sizeof(entityState_t),(unsigned)sizeof(playerState_t));return 0;
}
