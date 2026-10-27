#include "g_local.h"
#include "g_tce_pose.h"
#include "g_tce_pose_presentation.h"
#include "tools/modernize/pose_presentation_runtime_cases.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#undef vsnprintf
#include "tools/modernize/pose_presentation_trajectory_services.h"
static unsigned int checks,configWrites;
#define CHECK(x) do {++checks;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
gentity_t g_entities[MAX_GENTITIES];
level_locals_t level;
vmCvar_t g_newbbox,g_leanmode;
static gclient_t client;
static gclient_t joiningClient,botClient;
static g_tcePosePresentationMetadata_t fixtureMetadata;
static bg_character_t character;
static bg_character_t carrierCharacter;
static int assetReplacement;
static bg_characterDef_t definition;
static animModelInfo_t model;
static animation_t animationTable[236];
static animation_t *animations[2];
static uint32_t fixtureProfiles[2]={1234,5678},fixtureMapGeneration=1;
static int ready=1,profileCount=1;
static char config[TCE_POSE_PRESENTATION_MANIFEST];
static char nonceCounter[32]="100";
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
void trap_Cvar_Register(vmCvar_t *value,const char *name,const char *text,int flags) {
    (void)value;(void)name;(void)text;(void)flags;
}
void trap_Cvar_Set(const char *name,const char *text) {if(!strcmp(name,"tce_poseChallengeCounter"))strcpy(nonceCounter,text);}
void trap_Cvar_VariableStringBuffer(const char *name,char *out,int size) {
    CHECK(!strcmp(name,"tce_poseChallengeCounter"));Com_sprintf(out,size,"%s",nonceCounter);
}
int trap_Milliseconds(void) {return 123456;}
int trap_RealTime(qtime_t *out) {memset(out,0,sizeof(*out));out->tm_year=126;return 0;}
void trap_SetConfigstring(int number,const char *text) {
    CHECK(number==TCE_POSE_PRESENTATION_CS);CHECK(strlen(text)<sizeof(config));strcpy(config,text);++configWrites;
}
void G_TCEPoseStats(g_tcePoseStats_t *out) {memset(out,0,sizeof(*out));out->initialized=1;out->mapGeneration=fixtureMapGeneration;}
int G_TCEPosePresentationMetadata(const gentity_t *ent,g_tcePosePresentationMetadata_t *out) {
    (void)ent;if(!ready)return 0;*out=fixtureMetadata;return 1;
}
int G_TCEPosePresentationIdentity(const gentity_t *ent,uint32_t *map,uint32_t *generation) {
    (void)ent;*map=fixtureMetadata.mapGeneration;*generation=fixtureMetadata.generation;return 1;
}
int G_TCEPoseAssetInfo(const bg_character_t *which,g_tcePoseAssetInfo_t *out) {
    memset(out,0,sizeof(*out));
    if(which!=&character&&which!=&carrierCharacter)return 0;
    out->profile=which==&character?fixtureProfiles[0]+(unsigned)assetReplacement:9876;
    out->rawProfile=BG_TCEPosePresentationProfile(&character,&definition);return 1;
}
int G_TCEPosePresentationProfiles(uint32_t *out,int cap) {
    if(profileCount<0||profileCount>cap)return -1;
    memcpy(out,fixtureProfiles,(size_t)profileCount*sizeof(*out));return profileCount;
}
/* ACTUAL_PRESENTATION */
static void tick(int time) {
    level.previousTime=level.time;level.time=time;g_entities[0].s.pos.trTime=time;G_TCEPresentationPublish(&g_entities[0]);
}
static void acknowledge(int slot) {
    char info[64];Com_sprintf(info,sizeof(info),"\\tce_pose\\2:%08x:%08x",(unsigned)BG_TCEPosePresentationManifestDigest(config),(unsigned)G_TCEPresentationChallenge(slot));
    G_TCEPresentationUserinfo(slot,info);
}
static void setup(void) {
    gentity_t *ent=&g_entities[0];int i,speeds[236];
    memset(g_entities,0,sizeof(g_entities));memset(&client,0,sizeof(client));memset(&fixtureMetadata,0,sizeof(fixtureMetadata));
    memset(&level,0,sizeof(level));PresentationRuntimeModel(&character,&definition,&model,animationTable);
    animations[0]=&animationTable[94];animations[1]=&animationTable[222];
    memset(speeds,0,sizeof(speeds));speeds[94]=speeds[222]=69;
    fixtureProfiles[0]=BG_TCEPosePresentationSpeedProfile(BG_TCEPosePresentationProfile(&character,&definition),speeds,236);
    ent->inuse=qtrue;ent->client=&client;ent->health=100;ent->s.eType=ET_PLAYER;ent->s.number=ent->s.clientNum=0;
    ent->s.pos.trType=TR_LINEAR_STOP;ent->s.pos.trDuration=50;ent->s.groundEntityNum=ENTITYNUM_NONE;
    client.pers.connected=CON_CONNECTED;client.ps.clientNum=0;client.ps.stats[STAT_HEALTH]=100;
    ent->s.legsAnim=client.ps.legsAnim=606;ent->s.torsoAnim=client.ps.torsoAnim=734;
    for(i=0;i<2;++i) {
        fixtureMetadata.animation[i]=animations[i];fixtureMetadata.acceptedEpoch[i]=1;fixtureMetadata.moveSpeed[i]=69;
    }
    fixtureMetadata.fullAnimation[0]=606;fixtureMetadata.fullAnimation[1]=734;fixtureMetadata.profile=fixtureProfiles[0];
    fixtureMetadata.generation=fixtureMetadata.mapGeneration=1;fixtureMetadata.renderCharacter=fixtureMetadata.animationCharacter=&character;
    ent->s.pos.trDelta[2]=client.ps.velocity[2]=69;ent->s.apos.trBase[0]=client.ps.viewangles[0]=20;
    ent->s.apos.trBase[1]=client.ps.viewangles[1]=350;ent->s.angles2[YAW]=20;
    ent->s.angles2[ROLL]=-10;ent->s.angles[PITCH]=5;ent->s.angles[YAW]=-10;
    client.ps.movementDir=20;client.ps.leanf=-10;client.ps.pm_flags=4;
    client.ps.holdable[TCE_FREELOOK_PITCH]=TCE_FREELOOK_CENTER+500;
    client.ps.holdable[TCE_FREELOOK_YAW]=TCE_FREELOOK_CENTER-1000;
    ent->s.effect1Time=0x400;g_newbbox.integer=g_leanmode.integer=1;
    G_TCEPresentationReset();G_TCEPresentationConnect(0);tick(100);CHECK(!ent->s.time);acknowledge(0);tick(150);
}
int main(int argc,char **argv) {
    g_tcePresentationQuery_t query,copy;gentity_t *ent=&g_entities[0];presentationRuntimeCase_t output,variant;FILE *file;
    uint16_t old;unsigned int writes;
    CHECK(argc==2);setup();CHECK(ent->s.time<0&&ent->s.time2<0);
    CHECK(G_TCEPresentationQuery(0,150,150,&query)==G_TCE_PRESENTATION_AFFECTED);
    CHECK(query.channel[0].elapsedQ16==0&&query.profile==fixtureProfiles[0]&&query.rateQ8[0]==256);
    old=query.code[0];tick(200);CHECK(G_TCEPresentationQuery(0,200,200,&query)==1&&query.code[0]!=old);
    copy=query;memset(&output,0,sizeof(output));output.signature=0x504f5332u;output.hasNext=1;output.step=50;output.nonce=G_TCEPresentationChallenge(0);output.state[0]=ent->s;output.ps[0]=client.ps;output.time[0]=200;
    ent->s.pos.trBase[2]=client.ps.origin[2]=10;ent->s.apos.trBase[0]=client.ps.viewangles[0]=30;
    ent->s.apos.trBase[1]=client.ps.viewangles[1]=10;tick(250);
    output.state[1]=ent->s;output.ps[1]=client.ps;output.time[1]=250;output.queryTime=225;strcpy(output.manifest,config);
    CHECK(G_TCEPresentationQuery(0,225,250,&query)==1);memcpy(output.channel,query.channel,sizeof(output.channel));output.axes=query.axes;VectorCopy(query.origin,output.origin);
    CHECK(query.origin[2]==5);CHECK(query.sourceTime==200&&query.serverTime==225);
    CHECK(G_TCEPresentationQuery(0,225,225,&query)==1&&fabs(query.origin[2]-1.725)<1e-5);
    CHECK(G_TCEPresentationQuery(0,250,249,&query)==-1&&!query.profile);
    CHECK(G_TCEPresentationQuery(0,251,200,&query)==-1&&!query.profile);
    CHECK(G_TCEPresentationQuery(0,302,350,&query)==-1);
    CHECK(G_TCEPresentationQuery(0,149,300,&query)==0);
    CHECK(G_TCEPresentationQuery(0,200,250,&query)==1);CHECK(!memcmp(&copy,&query,sizeof(copy)));
    file=fopen(argv[1],"wb");CHECK(file!=NULL);CHECK(fwrite(&output,sizeof(output),1,file)==1);
    ent->s.effect1Time|=8;ent->s.powerups|=1;G_TCEPresentationPublish(ent);
    variant=output;variant.state[1]=ent->s;variant.ps[1]=client.ps;
    CHECK(G_TCEPresentationQuery(0,225,250,&query)==1);
    memcpy(variant.channel,query.channel,sizeof(variant.channel));variant.axes=query.axes;VectorCopy(query.origin,variant.origin);
    CHECK(fwrite(&variant,sizeof(variant),1,file)==1);
    ent->s=output.state[1];
    fixtureMetadata.renderCharacter=&carrierCharacter;
    fixtureMetadata.profile=BG_TCEPosePresentationPairProfile(9876,BG_TCEPosePresentationProfile(&character,&definition));
    ent->s.effect1Time|=0x20;G_TCEPresentationPublish(ent);
    variant.state[1]=ent->s;CHECK(G_TCEPresentationQuery(0,225,250,&query)==1);
    memcpy(variant.channel,query.channel,sizeof(variant.channel));variant.axes=query.axes;VectorCopy(query.origin,variant.origin);
    CHECK(fwrite(&variant,sizeof(variant),1,file)==1);
    fixtureMetadata.animationCharacter=&carrierCharacter;fixtureMetadata.profile=9876;
    ent->s=output.state[1];ent->s.powerups=1<<PW_OPS_DISGUISED;G_TCEPresentationPublish(ent);
    variant.state[1]=ent->s;CHECK(G_TCEPresentationQuery(0,225,250,&query)==1);
    memcpy(variant.channel,query.channel,sizeof(variant.channel));variant.axes=query.axes;VectorCopy(query.origin,variant.origin);
    CHECK(fwrite(&variant,sizeof(variant),1,file)==1);
    fixtureMetadata.renderCharacter=fixtureMetadata.animationCharacter=&character;fixtureMetadata.profile=fixtureProfiles[0];
    ent->s=output.state[1];
    fixtureMetadata.fullAnimation[0]=ent->s.legsAnim=client.ps.legsAnim=607;G_TCEPresentationPublish(ent);
    variant=output;variant.state[1]=ent->s;variant.ps[1]=client.ps;
    CHECK(G_TCEPresentationQuery(0,225,250,&query)==1&&query.origin[2]==5);
    memcpy(variant.channel,query.channel,sizeof(variant.channel));variant.axes=query.axes;VectorCopy(query.origin,variant.origin);
    CHECK(fwrite(&variant,sizeof(variant),1,file)==1);
    fixtureMetadata.fullAnimation[0]=ent->s.legsAnim=client.ps.legsAnim=17;fixtureMetadata.animation[0]=&animationTable[17];
    fixtureMetadata.fullAnimation[1]=ent->s.torsoAnim=client.ps.torsoAnim=19;fixtureMetadata.animation[1]=&animationTable[19];ent->s.effect1Time=0;
    G_TCEPresentationPublish(ent);variant.state[1]=ent->s;variant.ps[1]=client.ps;
    CHECK(G_TCEPresentationQuery(0,225,250,&query)==1&&query.origin[2]==5);
    memcpy(variant.channel,query.channel,sizeof(variant.channel));variant.axes=query.axes;VectorCopy(query.origin,variant.origin);
    CHECK(fwrite(&variant,sizeof(variant),1,file)==1);
    ent->s=output.state[1];client.ps=output.ps[1];fixtureMetadata.animation[0]=animations[0];fixtureMetadata.animation[1]=animations[1];
    fixtureMetadata.fullAnimation[0]=606;fixtureMetadata.fullAnimation[1]=734;G_TCEPresentationPublish(ent);
    fixtureMetadata.acceptedEpoch[0]++;tick(300);CHECK(G_TCEPresentationQuery(0,300,300,&query)==1&&query.channel[0].elapsedQ16==0&&query.channel[1].elapsedQ16!=0);
    ent->s.pos.trDelta[2]=client.ps.velocity[2]=0;tick(350);CHECK(G_TCEPresentationQuery(0,350,350,&query)==1&&!query.rateQ8[0]);old=query.code[0];
    tick(400);CHECK(G_TCEPresentationQuery(0,400,400,&query)==1&&query.code[0]==old);
    tick(600);CHECK(G_TCEPresentationQuery(0,600,600,&query)==1&&query.channel[0].elapsedQ16==0);
    strcpy(animations[0]->name,"ordinary");ent->s.effect1Time=0;tick(650);
    CHECK(!ent->s.time&&!ent->s.time2);CHECK(G_TCEPresentationQuery(0,650,650,&query)==0);
    ready=0;tick(700);CHECK(G_TCEPresentationQuery(0,700,700,&query)==-1);ready=1;
    strcpy(animations[0]->name,"climb_up");fixtureMetadata.moveSpeed[0]=INT_MIN;tick(750);
    CHECK(!ent->s.time&&G_TCEPresentationQuery(0,750,750,&query)==-1);fixtureMetadata.moveSpeed[0]=69;
    fixtureMetadata.generation++;CHECK(G_TCEPresentationQuery(0,600,750,&query)==-1);
    tick(800);CHECK(G_TCEPresentationQuery(0,800,800,&query)==1&&query.channel[0].elapsedQ16==0);
    ent->client->tceRewind.active=1;copy=query;tick(850);ent->client->tceRewind.active=0;
    CHECK(G_TCEPresentationQuery(0,800,800,&query)==1&&!memcmp(&copy,&query,sizeof(copy)));
    G_TCEPresentationResetClient(ent);CHECK(!ent->s.time&&!ent->client->ps.holdable[7]);
    CHECK(G_TCEPresentationQuery(0,800,850,&query)==-1);tick(900);
    g_entities[1].inuse=qtrue;g_entities[1].client=&joiningClient;joiningClient.pers.connected=CON_CONNECTED;
    G_TCEPresentationConnect(1);
    g_entities[2].inuse=qtrue;g_entities[2].client=&botClient;botClient.pers.connected=CON_CONNECTED;g_entities[2].r.svFlags=SVF_BOT;
    tick(950);
    CHECK(!ent->s.time&&G_TCEPresentationQuery(0,950,950,&query)==0);
    CHECK(G_TCEPresentationQuery(0,900,950,&query)==1); /* Current gate-off cannot rewrite old affected policy. */
    acknowledge(1);tick(1000);CHECK(ent->s.time<0);
    CHECK(G_TCEPresentationQuery(0,950,1000,&query)==0);
    CHECK(G_TCEPresentationQuery(0,1000,1000,&query)==1&&query.channel[0].elapsedQ16==0);
    writes=configWrites;fixtureProfiles[0]++;fixtureMetadata.profile=fixtureProfiles[0];tick(1050);CHECK(configWrites==writes+1&&!ent->s.time);
    acknowledge(0);acknowledge(1);tick(1100);CHECK(ent->s.time<0);
    writes=configWrites;fixtureMapGeneration++;fixtureMetadata.mapGeneration=fixtureMapGeneration;tick(1150);
    CHECK(configWrites==writes+1&&G_TCEPresentationQuery(0,1100,1150,&query)==-1);
    profileCount=-1;tick(1200);CHECK(!config[0]&&!ent->s.time);
    profileCount=1;tick(1250);acknowledge(0);acknowledge(1);tick(1300);CHECK(ent->s.time<0);
    {char remembered[64];uint32_t oldNonce=G_TCEPresentationChallenge(0);
     Com_sprintf(remembered,sizeof(remembered),"\\tce_pose\\2:%08x:%08x",(unsigned)BG_TCEPosePresentationManifestDigest(config),(unsigned)oldNonce);
     G_TCEPresentationConnect(0);CHECK(G_TCEPresentationChallenge(0)!=oldNonce);
     G_TCEPresentationUserinfo(0,remembered);tick(1350);CHECK(!ent->s.time);
     acknowledge(0);tick(1400);CHECK(ent->s.time<0);
     G_TCEPresentationConnect(0);G_TCEPresentationUserinfo(0,remembered);tick(1450);CHECK(!ent->s.time);
     acknowledge(0);tick(1500);CHECK(ent->s.time<0);}
    fixtureMetadata.renderCharacter=&carrierCharacter;
    fixtureMetadata.profile=BG_TCEPosePresentationPairProfile(9876,BG_TCEPosePresentationProfile(&character,&definition));
    tick(1550);CHECK(G_TCEPresentationQuery(0,1550,1550,&query)==1);
    fixtureMetadata.renderCharacter=&character;fixtureMetadata.profile=fixtureProfiles[0];tick(1600);
    CHECK(G_TCEPresentationQuery(0,1550,1600,&query)==1&&query.renderCharacter==&carrierCharacter);
    ready=0;tick(1650);CHECK(G_TCEPresentationQuery(0,1550,1650,&query)==1);ready=1;
    assetReplacement=1;CHECK(G_TCEPresentationQuery(0,1600,1650,&query)==-1);assetReplacement=0;
    {int type,j;const int times[4]={0,25,50,75};
     for(type=0;type<2;++type) {
        int base=1700+type*200;
        client.ps.velocity[0]=123;client.ps.velocity[1]=-22;client.ps.velocity[2]=35;
        ent->s.pos.trBase[0]=100;ent->s.pos.trBase[1]=-20;ent->s.pos.trBase[2]=50;
        ent->s.pos.trType=type?TR_LINEAR_STOP:TR_INTERPOLATE;ent->s.pos.trDuration=50;ent->s.pos.trTime=base;
        level.previousTime=base-100;level.time=base;G_TCEPresentationPublish(ent);
        for(j=0;j<4;++j) {
            variant=output;variant.hasNext=0;variant.step=100;variant.state[0]=ent->s;variant.ps[0]=client.ps;
            variant.time[0]=base;variant.queryTime=base+times[j];
            CHECK(G_TCEPresentationQuery(0,(uint32_t)variant.queryTime,(uint32_t)(base+100),&query)==1);
            {int axis;float seconds=(type?(times[j]>50?50:times[j]):0)*.001f;
             for(axis=0;axis<3;++axis)CHECK(fabs(query.origin[axis]-
                (ent->s.pos.trBase[axis]+seconds*client.ps.velocity[axis]))<1e-5);}
            memcpy(variant.channel,query.channel,sizeof(variant.channel));variant.axes=query.axes;VectorCopy(query.origin,variant.origin);
            CHECK(fwrite(&variant,sizeof(variant),1,file)==1);
        }
     }}
    /* Remote origin tuples from complete publication/history, not a direct
     * shared-helper comparison. Later mutable server movers must be ignored. */
    {int scene,j;for(scene=0;scene<9;++scene) {
        int base=2500+scene*200;gentity_t *mover;
        fixtureMapGeneration=1;profileCount=1;ready=1;assetReplacement=0;setup();
        ent=&g_entities[0];mover=&g_entities[200];mover->inuse=qtrue;
        mover->s.number=200;mover->s.eType=ET_MOVER;
        mover->s.pos.trTime=mover->s.apos.trTime=base;
        ent->s.pos.trType=scene==8?TR_LINEAR_STOP:TR_INTERPOLATE;
        ent->s.pos.trDuration=50;VectorSet(ent->s.pos.trBase,10,0,0);
        VectorClear(client.ps.velocity);ent->s.groundEntityNum=200;
        if(scene==0||scene==4||scene==8) {mover->s.pos.trType=TR_LINEAR;mover->s.pos.trDelta[0]=100;}
        if(scene==1||scene==2) {mover->s.apos.trType=TR_LINEAR;mover->s.apos.trDelta[YAW]=scene==1?3600.f:-3600.f;}
        if(scene==3||scene==8) {
            mover->s.pos.trType=mover->s.apos.trType=TR_LINEAR;
            VectorSet(mover->s.pos.trDelta,120,-40,20);VectorSet(mover->s.apos.trDelta,320,1500,-400);
            if(scene==8)VectorSet(client.ps.velocity,31,-12,9);
        }
        if(scene==5||scene==6||scene==7) {
            gentity_t *follower=&g_entities[201];follower->inuse=qtrue;
            follower->s.number=201;follower->s.eType=ET_MOVER;
            follower->s.eFlags=EF_PATH_LINK;follower->s.torsoAnim=200;follower->s.angles2[0]=-10;
            ent->s.groundEntityNum=201;
            mover->s.pos.trType=(trType_t)16;mover->s.pos.trDuration=100;
            mover->s.pos.trTime=base-25;mover->s.effect2Time=scene==6?-1:1;
            PresentationRuntimeSpline();
            if(scene==6) {
                gentity_t *nested=&g_entities[202];nested->inuse=qtrue;
                nested->s.number=202;nested->s.eType=ET_MOVER;nested->s.eFlags=EF_PATH_LINK;
                nested->s.torsoAnim=201;nested->s.angles2[0]=12;mover->s.apos.trBase[0]=5;
                ent->s.groundEntityNum=202;
            }
            if(scene==7) {
                mover->s.pos.trType=TR_STATIONARY;VectorSet(follower->s.pos.trBase,42,24,11);
                follower->s.apos.trBase[YAW]=90;
            }
        }
        tick(base);memset(&variant,0,sizeof(variant));variant.signature=output.signature;
        variant.nonce=G_TCEPresentationChallenge(0);variant.step=50;
        variant.time[0]=base;variant.queryTime=base+25;variant.state[0]=ent->s;variant.ps[0]=client.ps;
        strcpy(variant.manifest,config);
        for(j=200;j<=202;++j)if(g_entities[j].inuse)variant.movers[variant.moverCount++]=g_entities[j].s;
        if(scene==4) {
            ent->s.pos.trBase[0]=110;tick(base+50);variant.hasNext=1;
            variant.time[1]=base+50;variant.state[1]=ent->s;variant.ps[1]=client.ps;
        }
        CHECK(G_TCEPresentationQuery(0,(uint32_t)(base+25),(uint32_t)(base+50),&query)==1);
        copy=query;
        for(j=200;j<=202;++j) {
            g_entities[j].s.pos.trDelta[0]=9999;g_entities[j].s.apos.trDelta[YAW]=-12345;
            g_entities[j].s.torsoAnim=100000;g_entities[j].s.effect2Time=0;
        }
        CHECK(G_TCEPresentationQuery(0,(uint32_t)(base+25),(uint32_t)(base+50),&query)==1&&!memcmp(&copy,&query,sizeof(copy)));
        if(scene==4)CHECK(query.origin[0]==60); /* No second mover delta. */
        memcpy(variant.channel,query.channel,sizeof(variant.channel));variant.axes=query.axes;VectorCopy(query.origin,variant.origin);
        CHECK(fwrite(&variant,sizeof(variant),1,file)==1);
    }}
    CHECK(!fclose(file));
    G_TCEPresentationReset();level.previousTime=-90;level.time=-40;ent->s.pos.trTime=-40;G_TCEPresentationPublish(ent);
    tick(10);CHECK(G_TCEPresentationQuery(0,5u,20u,&query)==1);
    printf("{\"checks\":%u,\"config_writes\":%u,\"history_slots\":%u,\"mover_history_bytes\":%u}\n",checks,configWrites,(unsigned)PRESENT_HISTORY,(unsigned)sizeof(presentationMovers));
    return 0;
}
