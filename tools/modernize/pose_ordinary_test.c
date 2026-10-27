#include "src/game/g_local.h"
#include "src/game/g_tce_pose.h"
#include "src/game/g_tce_pose_presentation.h"
#include "src/game/g_tce_bullet_damage.h"
#include "src/game/tce_damage_trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#undef vsnprintf
gentity_t g_entities[MAX_GENTITIES];level_locals_t level;
vmCvar_t g_newbbox,g_leanmode;
bg_character_t axisClassCharacters[7],alliedClassCharacters[7];
static gclient_t clients[2];static animModelInfo_t fixtureModel,disguisedModel;static animation_t fixtureAnimations[2],disguisedAnimations[2];
static char fixtureManifest[TCE_POSE_PRESENTATION_MANIFEST],fixtureCounter[32]="100";
static float fixtureWorldFraction=1;static int skinCalls,failures,checks;
#define CHECK(x) do {++checks;if(!(x)){++failures;fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);}}while(0)
/* Actual preexisting diagnostic geometry services; fixture rays use zero angles. */
void AngleVectors(const vec3_t a,vec3_t f,vec3_t r,vec3_t u) {
 double p=a[0]*(M_PI/180.0),y=a[1]*(M_PI/180.0),z=a[2]*(M_PI/180.0);
 double sp=sin(p),cp=cos(p),sy=sin(y),cy=cos(y),sr=sin(z),cr=cos(z);
 if(f){VectorSet(f,(float)(cp*cy),(float)(cp*sy),(float)-sp);}
 if(r){VectorSet(r,(float)(-sr*sp*cy+cr*sy),(float)(-sr*sp*sy-cr*cy),(float)(-sr*cp));}
 if(u){VectorSet(u,(float)(cr*sp*cy+sr*sy),(float)(cr*sp*sy-sr*cy),(float)(cr*cp));}
}
vec_t VectorLength(const vec3_t v){return(float)sqrt((double)v[0]*v[0]+(double)v[1]*v[1]+(double)v[2]*v[2]);}
void VectorNormalizeFast(vec3_t v){float n=VectorLength(v);if(n){VectorScale(v,1/n,v);}}
void trap_LinkEntity(gentity_t *e){e->r.linked=qtrue;}
void trap_UnlinkEntity(gentity_t *e){e->r.linked=qfalse;}
void trap_Trace(trace_t *tr,const vec3_t a,const vec3_t mi,const vec3_t ma,const vec3_t b,int pass,int mask){
 vec3_t delta;
 (void)mi;(void)ma;(void)pass;CHECK(!(mask&CONTENTS_BODY));memset(tr,0,sizeof(*tr));tr->fraction=fixtureWorldFraction;
 tr->entityNum=fixtureWorldFraction<1?ENTITYNUM_WORLD:ENTITYNUM_NONE;VectorSubtract(b,a,delta);VectorMA(a,fixtureWorldFraction,delta,tr->endpos);
}
g_tcePoseTraceState_t G_TCEPosePresentationTrace(const g_tcePresentationQuery_t *q,const float a[3],const float b[3],double f,g_tcePoseTraceResult_t *r){
 (void)q;(void)a;(void)b;(void)f;(void)r;++skinCalls;abort();return 0;
}
/* These origin services must be unreachable for the ordinary/unavailable cases. */
void BG_TCEPoseMoverState(const entityState_t *s,tcePoseMoverState_t *o){(void)s;(void)o;abort();}
int BG_TCEPoseMoverEvaluate(int n,uint32_t t,tcePoseMoverRead_t r,void *u,float o[3],float a[3]){(void)n;(void)t;(void)r;(void)u;(void)o;(void)a;abort();return 0;}
int BG_TCEPosePresentationOrigin(const tcePoseOriginState_t *a,const tcePoseOriginState_t *b,uint32_t x,uint32_t y,uint32_t z,tcePoseMoverRead_t r,void *u,float o[3]){(void)a;(void)b;(void)x;(void)y;(void)z;(void)r;(void)u;(void)o;abort();return 0;}
void QDECL Com_sprintf(char *o,int n,const char *f,...){va_list a;va_start(a,f);vsnprintf(o,(size_t)n,f,a);va_end(a);}
char *Info_ValueForKey(const char *info,const char *key){static char o[128];const char *p=info;size_t n=strlen(key);o[0]=0;while(*p){const char *k,*v;if(*p=='\\')++p;k=p;while(*p&&*p!='\\')++p;if(!*p)break;++p;v=p;while(*p&&*p!='\\')++p;if((size_t)(v-k-1)==n&&!memcmp(k,key,n)){memcpy(o,v,(size_t)(p-v));o[p-v]=0;break;}}return o;}
void trap_Cvar_Register(vmCvar_t *v,const char *n,const char *t,int f){(void)v;(void)n;(void)t;(void)f;}
void trap_Cvar_Set(const char *n,const char *t){if(!strcmp(n,"tce_poseChallengeCounter"))strcpy(fixtureCounter,t);}
void trap_Cvar_VariableStringBuffer(const char *n,char *o,int z){CHECK(!strcmp(n,"tce_poseChallengeCounter"));Com_sprintf(o,z,"%s",fixtureCounter);}
int trap_Milliseconds(void){return 1000;}int trap_RealTime(qtime_t *o){memset(o,0,sizeof(*o));return 0;}
void trap_SetConfigstring(int n,const char *s){CHECK(n==TCE_POSE_PRESENTATION_CS);strcpy(fixtureManifest,s);}
@PRIVATE@
static g_tcePoseCharacter_t poseCharacters[G_TCE_POSE_CHARACTERS];
static g_tcePoseClient_t poseClients[MAX_CLIENTS];static g_tcePoseStats_t poseStats;static uint32_t poseGeneration;
@CHARACTER@
@POSE@
@PUBLISHER@
@CONTEXT@
@TRACE@
static void tick(int t){level.previousTime=level.time;level.time=t;G_TCEPresentationPublish(&g_entities[0]);}
static int shoot(void){trace_t tr;tceBulletTraceContext_t context;vec3_t a={-100,0,20},b={100,0,20};memset(&context,0,sizeof(context));context.shotTime=context.receiptCeiling=(uint32_t)level.time;context.hit[1]=qtrue;G_TCETraceBullet(&tr,&g_entities[1],a,b,&context);return tr.entityNum;}
static void result(const char *name,int expected){g_tcePresentationQuery_t q;int p=G_TCEPresentationQuery(0,(uint32_t)level.time,(uint32_t)level.time,&q),hit=shoot();printf("{\"case\":\"%s\",\"policy\":%d,\"hit_entity\":%d,\"epoch\":[%u,%u]}\n",name,p,hit,poseClients[0].acceptedEpoch[0],poseClients[0].acceptedEpoch[1]);CHECK(hit==expected);}
int main(void){gentity_t *target=&g_entities[0];g_tcePosePresentationMetadata_t metadata;char ack[64];int i;
 memset(&fixtureModel,0,sizeof(fixtureModel));fixtureModel.numAnimations=2;
 for(i=0;i<2;++i){fixtureModel.animations[i]=&fixtureAnimations[i];strcpy(fixtureAnimations[i].name,i?"idle_torso":"idle_2h");fixtureAnimations[i].numFrames=fixtureAnimations[i].loopFrames=2;fixtureAnimations[i].frameLerp=fixtureAnimations[i].initialLerp=100;}
 alliedClassCharacters[2].animModelInfo=&fixtureModel;alliedClassCharacters[3].animModelInfo=&fixtureModel;
 poseStats.initialized=1;poseStats.characters=1;poseStats.mapGeneration=1;
 poseCharacters[0].character=&alliedClassCharacters[2];poseCharacters[0].model=&fixtureModel;poseCharacters[0].ready=poseCharacters[0].surfaceReady=1;poseCharacters[0].animationCount=2;poseCharacters[0].assets.profile=1234;poseCharacters[0].assets.rawProfile=5678;poseCharacters[0].speedReady[0]=poseCharacters[0].speedReady[1]=1;
 for(i=0;i<2;++i){gentity_t *e=&g_entities[i];e->inuse=qtrue;e->client=&clients[i];e->health=100;e->s.number=i;e->s.eType=ET_PLAYER;e->s.groundEntityNum=ENTITYNUM_NONE;e->r.linked=qtrue;e->r.contents=CONTENTS_BODY;clients[i].pers.connected=CON_CONNECTED;clients[i].ps.clientNum=i;clients[i].sess.sessionTeam=i?TEAM_AXIS:TEAM_ALLIES;clients[i].ps.weapon=2;clients[i].ps.persistant[PERS_SPAWN_COUNT]=1;clients[i].pers.connectTime=10;}
 clients[0].pers.character=&alliedClassCharacters[2];clients[0].sess.playerType=2;clients[0].ps.torsoAnim=target->s.torsoAnim=1;g_entities[1].r.svFlags|=SVF_BOT;
 G_TCEPoseResetClient(target);G_TCEPresentationReset();G_TCEPresentationConnect(0);tick(100);
 Com_sprintf(ack,sizeof(ack),"\\tce_pose\\2:%08x:%08x",(unsigned)BG_TCEPosePresentationManifestDigest(fixtureManifest),(unsigned)G_TCEPresentationChallenge(0));G_TCEPresentationUserinfo(0,ack);
 poseClients[0].acceptedEpoch[0]=poseClients[0].acceptedEpoch[1]=1;tick(150);CHECK(G_TCEPosePresentationMetadata(target,&metadata));result("ready ordinary control",0);
 /* Actual callback used by G_UpdateCharacter; it invokes the real lifecycle reset. */
 G_TCEPoseCharacterChanged(&clients[0]);CHECK(!poseClients[0].acceptedEpoch[0]&&!poseClients[0].acceptedEpoch[1]);tick(200);CHECK(!G_TCEPosePresentationMetadata(target,&metadata));result("ordinary after actual character reset",0);
 strcpy(fixtureAnimations[0].name,"walk_2h");clients[0].ps.velocity[0]=100;poseClients[0].acceptedEpoch[0]=1;tick(250);result("ordinary walking with torso not yet assigned",0);
 poseClients[0].acceptedEpoch[1]=1;poseCharacters[0].animationModels[0]=-1;tick(300);result("ordinary clip not skin-ready",0);poseCharacters[0].animationModels[0]=0;
 clients[0].pers.character=&alliedClassCharacters[3];G_TCEPoseCharacterChanged(&clients[0]);tick(350);result("ordinary unregistered skin profile",0);
 clients[0].pers.character=&alliedClassCharacters[2];G_TCEPoseCharacterChanged(&clients[0]);strcpy(fixtureAnimations[0].name,"prone_idle");target->s.effect1Time=4;tick(400);result("affected missing epoch remains excluded",ENTITYNUM_NONE);
 strcpy(fixtureAnimations[0].name,"idle_2h");target->s.effect1Time=0;poseClients[0].acceptedEpoch[0]=poseClients[0].acceptedEpoch[1]=1;tick(450);fixtureWorldFraction=.2f;result("opaque world control",ENTITYNUM_WORLD);CHECK(!skinCalls);
 /* Ordinary history is independent of the private skin-generation clock. */
 fixtureWorldFraction=1;G_TCEPoseCharacterChanged(&clients[0]);result("ordinary history survives private clock reset",0);
 target->s.eFlags|=EF_MG42_ACTIVE;tick(500);result("mounted ordinary without accepted epochs",0);target->s.eFlags=0;
 /* Names preserve affected transitions even when the stance flag has cleared. */
 strcpy(fixtureAnimations[0].name,"prone_idle");tick(550);result("prone name without flag remains excluded",ENTITYNUM_NONE);
 strcpy(fixtureAnimations[0].name,"climb_up");tick(600);result("climb name without flag remains excluded",ENTITYNUM_NONE);
 strcpy(fixtureAnimations[0].name,"idle_2h");strcpy(fixtureAnimations[1].name,"prone_torso");tick(650);result("affected torso name remains excluded",ENTITYNUM_NONE);
 strcpy(fixtureAnimations[1].name,"idle_torso");target->s.effect1Time=0x400;tick(700);result("ladder flag with ordinary names remains excluded",ENTITYNUM_NONE);
 target->s.effect1Time=4;tick(750);result("prone flag with ordinary names remains excluded",ENTITYNUM_NONE);
 target->s.effect1Time=0;tick(800);result("ordinary after affected history",0);
 {g_tcePresentationQuery_t q;uint32_t oldMap=poseStats.mapGeneration;
  CHECK(G_TCEPresentationQuery(0,750,800,&q)==G_TCE_PRESENTATION_UNKNOWN); /* Never current-stance fallback. */
  CHECK(G_TCEPresentationQuery(0,800,799,&q)==G_TCE_PRESENTATION_UNKNOWN);
  CHECK(G_TCEPresentationQuery(0,852,900,&q)==G_TCE_PRESENTATION_UNKNOWN);
  ++poseStats.mapGeneration;CHECK(G_TCEPresentationQuery(0,800,800,&q)==G_TCE_PRESENTATION_UNKNOWN);poseStats.mapGeneration=oldMap;
  ++clients[0].pers.connectTime;CHECK(G_TCEPresentationQuery(0,800,800,&q)==G_TCE_PRESENTATION_UNKNOWN);--clients[0].pers.connectTime;
  ++clients[0].ps.persistant[PERS_SPAWN_COUNT];CHECK(G_TCEPresentationQuery(0,800,800,&q)==G_TCE_PRESENTATION_UNKNOWN);--clients[0].ps.persistant[PERS_SPAWN_COUNT];
  clients[0].pers.connected=CON_DISCONNECTED;CHECK(G_TCEPresentationQuery(0,800,800,&q)==G_TCE_PRESENTATION_UNKNOWN);clients[0].pers.connected=CON_CONNECTED;
  target->inuse=qfalse;CHECK(G_TCEPresentationQuery(0,800,800,&q)==G_TCE_PRESENTATION_UNKNOWN);target->inuse=qtrue;
  target->health=0;CHECK(G_TCEPresentationQuery(0,800,800,&q)==G_TCE_PRESENTATION_UNKNOWN);target->health=100;
  target->s.eType=ET_CORPSE;CHECK(G_TCEPresentationQuery(0,800,800,&q)==G_TCE_PRESENTATION_UNKNOWN);target->s.eType=ET_PLAYER;
  G_TCEPresentationResetClient(target);CHECK(G_TCEPresentationQuery(0,800,800,&q)==G_TCE_PRESENTATION_UNKNOWN);
 }
 /* Unclassifiable table inputs must not gain ordinary fallback. */
 target->s.legsAnim=MAX_MODEL_ANIMATIONS+2;tick(850);result("out-of-range published animation",ENTITYNUM_NONE);target->s.legsAnim=0;
 fixtureModel.animations[0]=NULL;tick(900);result("missing public animation entry",ENTITYNUM_NONE);fixtureModel.animations[0]=&fixtureAnimations[0];
 memset(fixtureAnimations[0].name,'a',sizeof(fixtureAnimations[0].name));tick(950);result("unterminated public animation name",ENTITYNUM_NONE);strcpy(fixtureAnimations[0].name,"idle_2h");
 clients[0].sess.sessionTeam=TEAM_SPECTATOR;tick(1000);result("nonplaying team is unclassified",ENTITYNUM_NONE);clients[0].sess.sessionTeam=TEAM_ALLIES;
 target->s.eFlags=EF_SPARE1;tick(1050);result("special unsupported render model",ENTITYNUM_NONE);target->s.eFlags=0;
 /* Disguise animation selection is public; carrier changes only the mesh. */
 axisClassCharacters[2].animModelInfo=&fixtureModel;
 target->s.powerups=(1<<PW_OPS_DISGUISED)|(2<<PW_OPS_CLASS_1);tick(1100);result("ordinary disguised character without skin registration",0);
 target->s.powerups=(1<<PW_OPS_DISGUISED)|(7<<PW_OPS_CLASS_1);tick(1150);result("invalid disguised class",ENTITYNUM_NONE);target->s.powerups=0;
 target->s.effect1Time=0x20;tick(1200);result("ordinary carrier without render skin readiness",0);target->s.effect1Time=0;
 disguisedModel=fixtureModel;disguisedAnimations[0]=fixtureAnimations[0];disguisedAnimations[1]=fixtureAnimations[1];
 disguisedModel.animations[0]=&disguisedAnimations[0];disguisedModel.animations[1]=&disguisedAnimations[1];
 axisClassCharacters[2].animModelInfo=&disguisedModel;target->s.powerups=(1<<PW_OPS_DISGUISED)|(2<<PW_OPS_CLASS_1);
 strcpy(disguisedAnimations[0].name,"prone_idle");tick(1250);result("disguise different table prone name remains excluded",ENTITYNUM_NONE);
 strcpy(disguisedAnimations[0].name,"climb_up");tick(1300);result("disguise different table climb name remains excluded",ENTITYNUM_NONE);
 target->s.powerups=0;clients[0].ps.legsAnim=clients[0].ps.torsoAnim=-1;tick(1350);result("published ordinary independent of private PS metadata",0);
 CHECK(!skinCalls);
 printf("{\"checks\":%d,\"failures\":%d,\"skin_calls\":%d}\n",checks,failures,skinCalls);return failures?1:0;}
