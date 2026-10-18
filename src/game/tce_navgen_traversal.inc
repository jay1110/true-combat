/* Included by tce_navgen.c after the ordinary walking/coverage helpers.
 * These are conservative collision-validated traversal candidates, NOT a
 * claim that a bot has executed them. No WPS action bits are reinterpreted.
 * Running-jump impulse comes from PM_CheckJump (270 * .707, newbbox *1.25). Full Pmove is
 * deliberately not invoked on fabricated clients: it also updates shared
 * animation state and requires a live character/client context. */
#include "tce_nav_ladder.h"
#include "tce_nav_jump.h"
#include "tce_nav_graph.h"
#define NG_NAV_CLIMB (1u << 9)
#define NG_NAV_JUMP (1u << 27)
static int ngJumpCandidates, ngJumpLinks, ngLadderCandidates, ngLadderLinks;
static int ngJumpPrefilterChecks,ngJumpPrefilterRejected;
static int ngBspTraversalTraces,ngGenericLadderTraces,ngJumpTraversalTraces;
/* Dense bitsets avoid scanning every existing edge for every nearby pair on
 * maps with thousands of generated nodes. Two bits per ordered node pair. */
static unsigned int ngTrPairCache[NG_PAIR_CACHE_SLOTS];
static qboolean ngTrCacheFailed;
static void NG_TrCacheReset(void) {
 memset(ngTrPairCache,0,sizeof(ngTrPairCache));ngTrCacheFailed=qfalse;
}
static qboolean NG_TrSeen(int a,int b) { return (NG_PairGet(ngTrPairCache,a,b)&2)!=0; }
static void NG_TrMarkSeen(int a,int b) {
 if(!NG_PairSet(ngTrPairCache,a,b,NG_PairGet(ngTrPairCache,a,b)|2))ngTrCacheFailed=qtrue;
}
static void NG_TrSetAccepted(int a,int b,qboolean accepted) {
 int state=NG_PairGet(ngTrPairCache,a,b);
 if(!NG_PairSet(ngTrPairCache,a,b,accepted?state|1:state&~1))ngTrCacheFailed=qtrue;
}
static int ngBspLadderPairs[128][2],ngBspLadderPairCount,ngBspLadderSurfaces;
static int ngLadderFailure[5];
static int ngLandingProbes,ngLandingGrounded,ngLandingSupported,ngLandingArcs,ngLandingWalks;
static int ngLowerTransferTests,ngLowerTransferSuccesses;
static int ngExitFailures[10];
static qboolean ngExitDebug;
static qboolean ngLadderDetailed;
static qboolean ngLadderUnsupportedExit;
typedef struct {int low,high,node[4],ascentRelease,ascentAlignment;qboolean reverse,ballistic,aligned,forwardExit,liftedExit;vec3_t launch,landing,alignment,touchdown;float duration,speed,wishspeed,groundContinuation,contactDuration;} ngLadderProof;
static ngLadderProof ngLadderProofs[32],ngLadderProofWorking;
static int ngLadderProofCount;
typedef struct {int approach,launch,landing;qboolean validatedReturn,standing;float duration,continuation,impact,crash;vec3_t touchdown;
 int failurePhase;float failureVz;vec3_t failureNormal;} ngJumpProof;
static ngJumpProof ngJumpProofs[64];
static int ngJumpProofCount;
typedef struct {int approach,launch,landing;vec3_t direction,touchdown;float duration,climbSeconds,stamina,impact,crash;} ngFreeClimbProof;
static ngFreeClimbProof ngFreeClimbProofs[32];
static int ngFreeClimbProofCount;
typedef struct {
 int approach,launch,landing,pairedJumpLaunch;
 vec3_t touchdown;float duration,contactDuration,groundContinuation,impact,crashDelta;
 int intermediateContacts;
} ngDropProof;
static ngDropProof ngDropProofs[32];
static int ngDropProofCount;
static const float ngDropSpeeds[3]={114.f,160.f,204.f};
static float NG_TrGravity(void);
/* PM_CrashLand's first damage event is delta>38.  This is a zero-impulse
 * descent bound, not a general allowance for a jump or an unchecked drop.
 * All command-speed samples still prove their actual contact delta below. */
static float NG_TrSafeDropHeight(void) {
 float gravity=NG_TrGravity(),scale=g_newbbox.integer?.64f:1.f;
 return gravity>0.f?38.f/(2.f*gravity*scale*.0002f):0.f;
}
static const char *ngDropFailure;
static vec3_t ngDropFailurePoint;
static int ngDropTests,ngDropAccepted;
#define NG_DROP_FAILURES 16
static const char *ngDropFailureNames[NG_DROP_FAILURES]={"accepted","endpoint_bounds","trace_budget","launch_capsule_clearance","launch_capsule_ground","landing_support","ground_contact_solid","invalid_ground_contact","hull_solid","blocked_hull_or_wrong_floor","crashland_damage","unstable_touchdown","ground_continuation","liquid_or_hurt","left_proved_corridor","time_limit"};
static int NG_TrDropTarget(int index) {
 int i;
 if(ngNodes[index].authored&&ngNodes[index].id==ngNodes[index].editSourceId&&(ngNodes[index].editRole&8)) {
  int target=NG_Find(ngNodes[index].editDropId);
  if(target>=0&&ngNodes[target].accepted)return target;
 }
for(i=0;i<ngDropProofCount;i++) {
  const ngDropProof *p=&ngDropProofs[i];
  if(p->launch==index&&ngNodes[p->approach].accepted&&ngNodes[p->launch].accepted&&
     ngNodes[p->landing].accepted&&ngNodes[p->pairedJumpLaunch].accepted)return p->landing;
 }
 return -1;
}
static int NG_TrStandTarget(int index) {
 int i;
 if(ngNodes[index].authored&&ngNodes[index].id==ngNodes[index].editSourceId&&(ngNodes[index].editRole&64)) {
  int target=NG_Find(ngNodes[index].editDropId);
  if(target>=0&&ngNodes[target].accepted)return target;
 }
 for(i=0;i<ngJumpProofCount;i++)if(ngJumpProofs[i].standing&&ngJumpProofs[i].launch==index&&
     ngNodes[ngJumpProofs[i].approach].accepted&&ngNodes[index].accepted&&ngNodes[ngJumpProofs[i].landing].accepted)
  return ngJumpProofs[i].landing;
 return -1;
}
#define NG_FRONT_JUMP_CASES 16
#define NG_FRONT_JUMP_TRACES 24000
#define NG_FRONT_JUMP_ARCS 256
#define NG_FRONT_JUMP_QUANTUM 8
#define NG_JUMP_FAILURES 14
static const char *ngJumpFailureNames[NG_JUMP_FAILURES]={"accepted","gravity_or_stance","horizontal_range","height_range","no_real_arc","return_crash_damage","horizontal_speed","trace_budget","start_or_allsolid","blocked_hull_arc","liquid_hazard","hurt_trigger","landing_continuation","unstable_landing"};
static int ngJumpArcFailure;
static vec3_t ngJumpArcFailurePoint;
typedef struct {
 int low,high,samples,walkSamples,arcChecks,traces;const char *reason;
 int lowSamples,highSamples,lowLaunches,highLaunches,upChecks,returnChecks,upAccepted;
 vec3_t lowPoints[21],highPoints[21];
 int upFailures[NG_JUMP_FAILURES],returnFailures[NG_JUMP_FAILURES],returnExampleFailure;
 vec3_t returnExampleFrom,returnExampleTo,returnExampleHit;
 int dropChecks,dropAccepted;const char *dropReason;
 int portalLane; qboolean portalClear,portalFirst;
 int dropFailures[NG_DROP_FAILURES],dropExampleFailure;vec3_t dropExampleFrom,dropExampleTo,dropExampleHit;
} ngFrontJumpCase;
typedef struct {int low,high;float cost;} ngFrontArcPair;
/* Bounded transient scheduling state: samples and failed returns survive a
 * front quantum; only one accepted upward receipt is pending per front. */
typedef struct {
 ngNode low[21],high[21];ngFrontArcPair arcs[21*21];
 qboolean highLaunch[21],done,found,runningOnly; signed char down[21][21];
 int lc,hc,arcCount,nextArc,pendingArc,nextDrop,upKind,ua,ub,da,db;
 ngJumpProof jump;ngDropProof drop;
} ngFrontState;
static ngFrontJumpCase ngFrontJumpCases[NG_FRONT_JUMP_CASES];
static int ngFrontJumpCaseCount,ngFrontJumpTraces,ngFrontJumpConnected,ngFrontJumpArcChecks;
static int ngFrontJumpNoTraceRejected;
/* A range/equation rejection has not sampled an arc. Charging it against
 * the collision budget exhausted whole maps on the first few fronts. Keep
 * those decisions/outcomes recorded, but reserve the cap for native queries. */
static void NG_TrFrontAccountArc(ngFrontJumpCase *entry,int before) {
 if(ngTraces>before){entry->arcChecks++;ngFrontJumpArcChecks++;ngJumpCandidates++;}
 else ngFrontJumpNoTraceRejected++;
}
static int ngFrontReturnDamageRejected,ngFrontReturnLandingRejected;
static const char *ngFrontJumpStop;
static int ngTrSeedEdgeBegin,ngTrSeedEdgeEnd;
static int ngTrSpokeHead[NG_NODES],ngTrSpokeNext[NG_EDGES],ngTrSpokeOrder[NG_EDGES];
static int ngTrSpokeHeap[NG_NODES],ngTrSpokeHeapPosition[NG_NODES];
static float ngTrSpokeLength[NG_EDGES],ngTrSpokeDistance[NG_NODES];
static qboolean NG_TrExplainClear(const vec3_t a,const vec3_t b,const char *segment,int low,int high,int lift) {
 trace_t tr;qboolean ok;
 if(!NG_Trace(&tr,a,b,ngStand))return qfalse;
 ok=!tr.startsolid&&!tr.allsolid&&tr.fraction==1.f;
 if(ngLadderDetailed&&!ok)G_Printf("navgen ladder %d->%d lift%d %s blocked: from %.3f %.3f %.3f to %.3f %.3f %.3f hit %.3f %.3f %.3f fraction %.5f solid %d/%d entity %d normal %.3f %.3f %.3f surf %x contents %x\n",low,high,lift,segment,a[0],a[1],a[2],b[0],b[1],b[2],tr.endpos[0],tr.endpos[1],tr.endpos[2],tr.fraction,tr.startsolid,tr.allsolid,tr.entityNum,tr.plane.normal[0],tr.plane.normal[1],tr.plane.normal[2],tr.surfaceFlags,tr.contents);
 return ok;
}

static qboolean NG_TrSpecialInput(int a,int b) {
 int i;
 for(i=0;i<ngNodes[a].count;i++)
  if(ngNodes[a].targets[i]==ngNodes[b].id && ngNodes[a].linkFlags[i])return qtrue;
 return qfalse;
}
static qboolean NG_TrAccepted(int a,int b) {
 return (NG_PairGet(ngTrPairCache,a,b)&1)!=0;
}
static void NG_TrLink(int a,int b,const char *reason) {
 ngEdge *e;int i;
 /* Upgrade only a geometrically rejected ordinary edge; preserve explicit
  * special-action exclusions. This avoids duplicate report/binary edges. */
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].from==a&&ngEdges[i].to==b){
  if(ngEdges[i].accepted)return;
  if(!strcmp(ngEdges[i].reason,"special_link_requires_review"))return;
  ngEdges[i].accepted=qtrue;ngEdges[i].crouch=qfalse;ngEdges[i].reason=reason;
  NG_TrSetAccepted(a,b,qtrue);return;
 }
 if(ngEdgeCount>=NG_EDGES)return;
 e=&ngEdges[ngEdgeCount++];memset(e,0,sizeof(*e));e->from=a;e->to=b;
 e->accepted=qtrue;e->added=qtrue;e->reason=reason;
 NG_TrSetAccepted(a,b,qtrue);NG_TrMarkSeen(a,b);
}
static qboolean NG_TrClear(const vec3_t a,const vec3_t b) {
 trace_t tr;
 if(!NG_Trace(&tr,a,b,ngStand))return qfalse;
 return !tr.startsolid&&!tr.allsolid&&tr.fraction==1.f;
}

/* Check a deliberately short running jump, with full standing-hull sweeps
 * every <=8 ms, a world-only walkable landing and no hurt/liquid volumes.
 * Speed range is a conservative candidate filter, not a movement guarantee.
 * Weapon weight/stamina and Omni's take-off timing still need bot trials. */
static float NG_TrGravity(void) {
 /* ClientThink uses the developer override only with g_developer, then the
  * same 1.25 enlarged-hull scale as the real player state. */
 int value=g_developer.integer?(int)g_gravity.value:800;
 return (float)(g_newbbox.integer?(int)((double)value*1.25):value);
}
static float NG_TrJumpImpulse(void) {
 /* ClientSpawn sets STAT_TCE_FLAGS0x200 for g_newbbox. PM_CheckJump applies
  * that model scale after the running and direction factors. */
 return 270.f*.707f*(g_newbbox.integer?1.25f:1.f);
}
static qboolean NG_TrStableLanding(const vec3_t feet);
/* An arc may only add PM_CheckJump's impulse at actual capsule-supported
 * ground. A point/foot-box over a platform lip is not that launch gate. */
static qboolean NG_TrGroundedLaunch(const vec3_t feet) {
 vec3_t down;trace_t tr;
 if(!NG_Trace(&tr,feet,feet,ngStand)||tr.startsolid||tr.allsolid)return qfalse;
 VectorCopy(feet,down);down[2]-=.25f;
 return NG_Trace(&tr,feet,down,ngStand)&&!tr.startsolid&&!tr.allsolid&&tr.fraction<1.f&&
  NG_StaticGround(tr.entityNum)&&NG_OrdinarySurface(tr.surfaceFlags,tr.plane.normal[2]);
}
static qboolean NG_TrJumpArc(const ngNode *a,const ngNode *b,qboolean validatedReturn) {
 const float impulse=NG_TrJumpImpulse();float gravity=NG_TrGravity();
 vec3_t d,previous,next;float horizontal,discriminant,duration,speed,t;int step,steps;
 trace_t tr;
 ngJumpArcFailure=1;VectorCopy(a->feet,ngJumpArcFailurePoint);
 if(gravity<100.f||gravity>2000.f||a->crouch||b->crouch)return qfalse;
 VectorSubtract(b->feet,a->feet,d);horizontal=sqrt(d[0]*d[0]+d[1]*d[1]);
 ngJumpArcFailure=2;if(horizontal<40.f||horizontal>160.f)return qfalse;
 ngJumpArcFailure=3;if((!validatedReturn&&d[2]<-24.f)||d[2]>impulse*impulse/(2.f*gravity)-2.f)return qfalse;
 ngJumpArcFailure=4;if(!NG_TrGroundedLaunch(a->feet))return qfalse;
 discriminant=impulse*impulse-2.f*gravity*d[2];if(discriminant<=0)return qfalse;
 if(validatedReturn) {
  /* Only used after a proved reciprocal ascent, never as a generic deep
   * drop permission. PM_CrashLand (both TC assembly and portable path)
   * scales impact velocity by .8 for the enlarged body, then uses
   * 2 * velocity^2 * .0001; damage and velocity clearing begin above38.
   * The old -24 proximity filter is not a gameplay fall-height limit. */
  float scaledImpactSquared=discriminant*(g_newbbox.integer?.64f:1.f);
  ngJumpArcFailure=5;if(d[2]>=0.f||scaledImpactSquared*.0002f>38.f){ngFrontReturnDamageRejected++;return qfalse;}
 }
 duration=(impulse+sqrt(discriminant))/gravity;speed=horizontal/duration;
 ngJumpArcFailure=6;if(speed<100.f||speed>220.f)return qfalse;
 steps=(int)ceil(duration/.008f);VectorCopy(a->feet,previous);
 for(step=1;step<=steps;step++){
  t=duration*(float)step/steps;
  VectorMA(a->feet,t/duration,d,next);
  next[2]=a->feet[2]+impulse*t-.5f*gravity*t*t;
  ngJumpArcFailure=7;if(!NG_Trace(&tr,previous,next,ngStand))return qfalse;
  VectorCopy(tr.endpos,ngJumpArcFailurePoint);
  ngJumpArcFailure=8;if(tr.startsolid||tr.allsolid)return qfalse;
  if(tr.fraction<1.f){
   vec3_t err;VectorSubtract(tr.endpos,b->feet,err);
   ngJumpArcFailure=9;if(step!=steps||!NG_StaticGround(tr.entityNum)||tr.plane.normal[2]<.7f||
      DotProduct(err,err)>4.f)return qfalse;
  }
  ngJumpArcFailure=10;if(trap_PointContents(next,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME))return qfalse;
  ngJumpArcFailure=11;if(NG_Hurt(next,ngStand))return qfalse;
  VectorCopy(next,previous);
 }
 /* Require a safe, level standing area after touchdown, not a thin ledge. */
 VectorMA(b->feet,16.f/horizontal,d,next);next[2]=b->feet[2];
 ngJumpArcFailure=12;VectorCopy(next,ngJumpArcFailurePoint);
 { vec3_t floor;const char *why=NG_Ground(next,floor,ngStand);
  if(why||fabs(floor[2]-b->feet[2])>4.f||!NG_TrClear(b->feet,floor))return qfalse;
 }
 ngJumpArcFailure=13;
 if(validatedReturn&&!NG_TrStableLanding(b->feet)){ngFrontReturnLandingRejected++;return qfalse;}
 ngJumpArcFailure=0;
 return qtrue;
}
static qboolean NG_TrJump(const ngNode *a,const ngNode *b) {
 return NG_TrJumpArc(a,b,qfalse);
}

/* Reject tall walls cheaply before spending the bounded arc budget. The
 * ray crosses the upper torso band at the maximum running-jump rise; a
 * positive result proves nothing and still requires the entire hull arc. */
static qboolean NG_TrJumpPrefilter(const ngNode *a,const ngNode *b) {
 vec3_t from,to;trace_t tr;float gravity=NG_TrGravity(),height;
 if(gravity<100.f||gravity>2000.f||ngTraces>=NG_TRACE_BUDGET)return qfalse;
 {float impulse=(b->feet[2]-a->feet[2]>NG_TrJumpImpulse()*NG_TrJumpImpulse()/(2.f*gravity)-2.f)?TCE_NavStandingImpulse(g_newbbox.integer):NG_TrJumpImpulse();
  height=ngStand*.5f+impulse*impulse/(2.f*gravity);}
 VectorCopy(a->feet,from);VectorCopy(b->feet,to);from[2]+=height;to[2]+=height;
 trap_Trace(&tr,from,NULL,NULL,to,ENTITYNUM_NONE,NG_MASK);ngTraces++;ngJumpPrefilterChecks++;
 if(tr.startsolid||tr.allsolid||tr.fraction<1.f){ngJumpPrefilterRejected++;return qfalse;}
 return qtrue;
}

static qboolean NG_TrRay(trace_t *tr,const vec3_t a,const vec3_t b) {
 if(ngTraces>=NG_TRACE_BUDGET)return qfalse;
 trap_Trace(tr,a,NULL,NULL,b,ENTITYNUM_NONE,NG_MASK);ngTraces++;
 return !tr->startsolid&&!tr->allsolid&&tr->fraction<1.f&&
        NG_StaticGround(tr->entityNum)&&(tr->surfaceFlags&SURF_LADDER)&&fabs(tr->plane.normal[2])<.1f;
}
static qboolean NG_TrBodyLadderRay(trace_t *tr,const vec3_t feet,const vec3_t offset,const vec3_t expected) {
 float heights[4];int h;vec3_t a,b;
 heights[0]=2.f;heights[1]=24.f;heights[2]=48.f;heights[3]=ngStand-4.f;
 /* PM_CheckLadder traces the entire body: a ladder starting above the feet
  * can legitimately be reached with the upper hull. Require surface contact
  * somewhere in that same standing body band, not at one arbitrary height. */
 for(h=0;h<4;h++){
  VectorCopy(feet,a);a[2]+=heights[h];VectorAdd(a,offset,b);
  if(NG_TrRay(tr,a,b)&&(!expected||DotProduct(tr->plane.normal,expected)>.95f))return qtrue;
 }
 return qfalse;
}
static qboolean NG_TrNativeLadderContact(const vec3_t feet,const vec3_t normal) {
 vec3_t end;trace_t tr;
 /* PM_CheckLadderMove uses just one forward unit while grounded. A long
  * discovery ray is not proof that the player can actually mount here. */
 VectorMA(feet,-1.f,normal,end);
 if(!NG_Trace(&tr,feet,end,ngStand))return qfalse;
 return !tr.startsolid&&!tr.allsolid&&tr.fraction<1.f&&NG_StaticGround(tr.entityNum)&&
        (tr.surfaceFlags&SURF_LADDER)&&DotProduct(tr.plane.normal,normal)>.95f;
}

/* Find a continuous real ladder surface, then validate bottom approach,
 * full-hull shaft and upper exit. No CLIMB tags without this surface proof. */
static qboolean NG_TrAirborneLadderContactState(const vec3_t feet,const vec3_t normal,qboolean *pull) {
 vec3_t end,mins,maxs;trace_t tr;
 if(pull)*pull=qfalse;
 /* PM_CheckLadderMove: airborne discovery uses 48 units, then repeats a
  * distant hit with mins.z=-1 relative to player origin. Feet-relative
  * generator coordinates require ngOrigin-1, not a one-unit ground probe.
  * This validates geometry only; facing and velocity still need live proof. */
 VectorMA(feet,-48.f,normal,end);
 if(!NG_Trace(&tr,feet,end,ngStand)||tr.startsolid||tr.allsolid||tr.fraction>=1.f||
    !NG_StaticGround(tr.entityNum)||!(tr.surfaceFlags&SURF_LADDER)||DotProduct(tr.plane.normal,normal)<.95f)return qfalse;
 if(tr.fraction*48.f<=1.f)return qtrue;
 if(ngTraces>=NG_TRACE_BUDGET)return qfalse;
 VectorSet(mins,-ngWidth,-ngWidth,ngOrigin-1.f);VectorSet(maxs,ngWidth,ngWidth,ngStand);
 VectorMA(feet,-48.f,tr.plane.normal,end);
 trap_TraceCapsule(&tr,feet,mins,maxs,end,ENTITYNUM_NONE,NG_MASK);ngTraces++;
 if(tr.startsolid||tr.allsolid||tr.fraction==1.f||!NG_StaticGround(tr.entityNum)||
    !(tr.surfaceFlags&SURF_LADDER)||DotProduct(tr.plane.normal,normal)<=.95f)return qfalse;
 if(pull)*pull=qtrue;return qtrue;
}
static qboolean NG_TrAirborneLadderContact(const vec3_t feet,const vec3_t normal) {
 return NG_TrAirborneLadderContactState(feet,normal,NULL);
}
static qboolean NG_TrExitSupported(const vec3_t from,const vec3_t to,const vec3_t normal) {
 vec3_t delta,point,down;trace_t tr;int i,steps;
 VectorSubtract(to,from,delta);steps=(int)ceil(VectorLength(delta)/2.f);if(steps<1)steps=1;
 /* An unobstructed horizontal sweep over an open shaft is not a walk-off.
  * Require support within a native step at every sample, or actual nearby
  * ladder contact. Do not silently turn this transition into a gap jump. */
 for(i=0;i<=steps;i++){
  VectorMA(from,(float)i/steps,delta,point);VectorCopy(point,down);down[2]-=18.25f;
  if(!NG_Trace(&tr,point,down,ngStand))return qfalse;
  if(!tr.startsolid&&!tr.allsolid&&tr.fraction<1.f&&NG_StaticGround(tr.entityNum)&&tr.plane.normal[2]>=.7f)continue;
  if(NG_TrAirborneLadderContact(point,normal))continue;
  return qfalse;
 }
 return qtrue;
}
/* A dismount initially faces the ladder. Bound its air input using the real
 * TC:E command scaler with a heavy loadout and no sprint/ADS. Turning toward
 * the exit can increase input; this is still a candidate requiring live proof. */
static float NG_TrExitWishspeed(const vec3_t direction,const vec3_t normal) {
 tce_moveScale_t state;vec3_t right;float magnitude;
 memset(&state,0,sizeof(state));VectorSet(right,-normal[1],normal[0],0);
 state.forward=(int)(-DotProduct(direction,normal)*127.f);
 state.right=(int)(DotProduct(direction,right)*127.f);
 state.speed=g_developer.integer?(int)g_speed.value:320;
 if(g_newbbox.integer)state.speed=(int)((double)state.speed*1.25);
 state.runScale=g_realism.integer==1?.45f:.51f;
 state.sprintScale=.744f;state.crouchScale=.213f;state.sprintTime=100;
 state.weaponWeight=state.carriedWeight=5;
 state.gametype=g_gametype.integer;state.movespeed=g_movespeed.integer;
 magnitude=sqrt((float)(state.forward*state.forward+state.right*state.right));
 return magnitude*TCE_PM_CmdScale(&state);
}
/* A short foot box can catch the rim while the player's centre still lies
 * over a ladder opening. Require support beneath the centre and around the
 * circular footprint before treating an exit as a place to stop. This is
 * deliberately stricter than the transient contact allowed during motion. */
static qboolean NG_TrStableLanding(const vec3_t feet) {
 static const float offsets[9][2]={{0,0},{1,0},{.707107f,.707107f},{0,1},
  {-.707107f,.707107f},{-1,0},{-.707107f,-.707107f},{0,-1},{.707107f,-.707107f}};
 int i;vec3_t start,end;trace_t tr;
 for(i=0;i<9;i++){
  if(ngTraces>=NG_TRACE_BUDGET)return qfalse;
  VectorSet(start,feet[0]+ngWidth*offsets[i][0],feet[1]+ngWidth*offsets[i][1],feet[2]+.5f);
  VectorCopy(start,end);end[2]=feet[2]-2.f;
  trap_Trace(&tr,start,NULL,NULL,end,ENTITYNUM_NONE,NG_MASK);ngTraces++;
  if(tr.startsolid||tr.allsolid||tr.fraction==1.f||!NG_StaticGround(tr.entityNum)||
     tr.plane.normal[2]<.7f||(tr.surfaceFlags&SURF_SLICK)||fabs(tr.endpos[2]-feet[2])>1.f)return qfalse;
 }
 return qtrue;
}
/* Standing jumps may stop on a continuous gentle slope even though the
 * exported square-foot anchor lies above the actual rounded body support.
 * Keep the existing flat footprint first. The additional contract requires
 * a real full-body floor within runtime's two-unit landing tolerance and
 * all nine points on one coherent, unslick static plane. This is deliberately
 * separate from ladder/drop stopping places and never permits a missing ray. */
static qboolean NG_TrGentlePlaneSupport(const vec3_t feet) {
 static const float offsets[9][2]={{0,0},{1,0},{.707107f,.707107f},{0,1},
  {-.707107f,.707107f},{-1,0},{-.707107f,-.707107f},{0,-1},{.707107f,-.707107f}};
 vec3_t start,end,support,normal,error;trace_t tr;
 float plane=0.f,expected;int i,entity;
 VectorCopy(feet,end);end[2]-=2.f;
 if(!NG_Trace(&tr,feet,end,ngStand)||tr.startsolid||tr.allsolid||tr.fraction==1.f||
    TCE_NavGroundKind(tr.entityNum)!=1||tr.plane.normal[2]<.99f||
    tr.plane.normal[2]>=.99999f||(tr.surfaceFlags&SURF_SLICK))return qfalse;
 VectorCopy(tr.endpos,support);VectorCopy(tr.plane.normal,normal);entity=tr.entityNum;
 if(feet[2]-support[2]<0.f||feet[2]-support[2]>2.f||
    (trap_PointContents(feet,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME))||
    (trap_PointContents(support,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME))||
    NG_Hurt(feet,ngStand)||NG_Hurt(support,ngStand))return qfalse;
 for(i=0;i<9;i++) {
  if(ngTraces>=NG_TRACE_BUDGET)return qfalse;
  VectorSet(start,feet[0]+ngWidth*offsets[i][0],feet[1]+ngWidth*offsets[i][1],feet[2]+.5f);
  VectorCopy(start,end);
  if(i) {
   expected=(plane-normal[0]*start[0]-normal[1]*start[1])/normal[2];
   start[2]=expected+.5f;end[2]=expected-.5f;
  }else end[2]=feet[2]-2.5f;
  trap_Trace(&tr,start,NULL,NULL,end,ENTITYNUM_NONE,NG_MASK);ngTraces++;
  if(tr.startsolid||tr.allsolid||tr.fraction==1.f||tr.entityNum!=entity||
     TCE_NavGroundKind(tr.entityNum)!=1||tr.plane.normal[2]<.99f||
     (tr.surfaceFlags&SURF_SLICK))return qfalse;
  VectorSubtract(tr.plane.normal,normal,error);
  if(DotProduct(error,error)>0.000001f)return qfalse;
  if(!i)plane=DotProduct(tr.endpos,normal);
  else if(fabs(DotProduct(tr.endpos,normal)-plane)>.0625f)return qfalse;
  if(trap_PointContents(tr.endpos,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME))return qfalse;
 }
 return qtrue;
}
/* A typed physical action pose can be below its square-foot export anchor.
 * Only a gentle, coherent, capsule-grounded pose may use that same-XY anchor
 * as the ordinary walk query endpoint; the stored action feet never move. */
static qboolean NG_TrPhysicalGroundAlias(const vec3_t feet,vec3_t alias) {
 ngNode physical,anchor;vec3_t down;trace_t tr;float difference;
 if(NG_Ground(feet,alias,ngStand))return qfalse;
 difference=alias[2]-feet[2];
 if(difference<=.25f||difference>2.f)return qfalse;
 VectorCopy(feet,down);down[2]-=.25f;
 if(!NG_Trace(&tr,feet,down,ngStand)||tr.startsolid||tr.allsolid||tr.fraction==1.f||
    TCE_NavGroundKind(tr.entityNum)!=1||tr.plane.normal[2]<.99f||
    tr.plane.normal[2]>=.99999f||(tr.surfaceFlags&SURF_SLICK)||
    !NG_TrGentlePlaneSupport(feet))return qfalse;
 memset(&physical,0,sizeof(physical));memset(&anchor,0,sizeof(anchor));
 VectorCopy(feet,physical.feet);VectorCopy(alias,anchor.feet);
 return !NG_Walk(&physical,&anchor,ngStand)&&!NG_Walk(&anchor,&anchor,ngStand);
}
static qboolean NG_TrStandingLanding(const vec3_t feet) {
 ngNode node;vec3_t alias;const char *why;
 if(NG_TrStableLanding(feet))return qtrue;
 if(!NG_TrGentlePlaneSupport(feet))return qfalse;
 memset(&node,0,sizeof(node));VectorCopy(feet,node.feet);
 why=NG_Walk(&node,&node,ngStand);
 if(!why)return qtrue;
 return !strcmp(why,"different_floor")&&NG_TrPhysicalGroundAlias(feet,alias);
}
static const char *NG_TrStandingPhysicalWalk(const ngNode *a,const ngNode *b,const char *why) {
 ngNode alias=*b;
 if(why&&!strcmp(why,"different_floor")&&NG_TrPhysicalGroundAlias(b->feet,alias.feet))
  return NG_Walk(a,&alias,ngStand);
 return why;
}
/* A transient first contact may precede the final stopping place, but its
 * centre must already have real floor within one native step. A capsule
 * touching a 4-unit lip while its centre is above the adjacent lower floor
 * can safely step down; a broad foot box bridging a deep hole cannot. */
static qboolean NG_TrCentreSupported(const vec3_t feet) {
 vec3_t start,end;trace_t tr;
 if(ngTraces>=NG_TRACE_BUDGET)return qfalse;
 VectorCopy(feet,start);start[2]+=.5f;VectorCopy(feet,end);end[2]-=18.25f;
 trap_Trace(&tr,start,NULL,NULL,end,ENTITYNUM_NONE,NG_MASK);ngTraces++;
 return !tr.startsolid&&!tr.allsolid&&tr.fraction<1.f&&NG_StaticGround(tr.entityNum)&&
  tr.plane.normal[2]>=.7f&&!(tr.surfaceFlags&SURF_SLICK)&&fabs(tr.endpos[2]-feet[2])<=18.25f;
}
static qboolean NG_TrExitContinuation(const vec3_t from,const vec3_t to) {
 ngNode a,b;vec3_t delta,point,floor;int step,steps;
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));VectorCopy(from,a.feet);VectorCopy(to,b.feet);
 VectorSubtract(to,from,delta);steps=(int)ceil(VectorLength(delta)/4.f);
 if(steps>24||!NG_TrCentreSupported(from)||!NG_TrStableLanding(to)||NG_Walk(&a,&b,ngStand))return qfalse;
 if(!steps)return qtrue;
 for(step=1;step<=steps;step++){
  VectorMA(from,(float)step/steps,delta,point);
  if(NG_Ground(point,floor,ngStand)||!NG_TrCentreSupported(floor))return qfalse;
 }
 return qtrue;
}
/* Standing-jump-only continuation; the ordinary movement and every centre
 * support sample are identical to the existing strict exit proof. */
static qboolean NG_TrStandingExitContinuation(const vec3_t from,const vec3_t to) {
 ngNode a,b;vec3_t delta,point,floor;int step,steps;
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));VectorCopy(from,a.feet);VectorCopy(to,b.feet);
 VectorSubtract(to,from,delta);steps=(int)ceil(VectorLength(delta)/4.f);
 if(steps>24||!NG_TrCentreSupported(from)||!NG_TrStandingLanding(to)||
    NG_TrStandingPhysicalWalk(&a,&b,NG_Walk(&a,&b,ngStand)))return qfalse;
 if(!steps)return qtrue;
 for(step=1;step<=steps;step++) {
  VectorMA(from,(float)step/steps,delta,point);
  if(NG_Ground(point,floor,ngStand)||!NG_TrCentreSupported(floor))return qfalse;
 }
 return qtrue;
}
/* At a forward ladder top the rounded player capsule can already rest on
 * the lip before its centre crosses the edge. PM_GroundTrace uses that full
 * capsule, not a point beneath its centre. Prove the short supported walk
 * with the same .25-unit body probe at every sample, while retaining the
 * ordinary hull/step/hazard checks and the strong final stopping footprint.
 * This is only a ladder-top continuation, never a general gap/drop waiver. */
static qboolean NG_TrForwardExitContinuation(const vec3_t from,const vec3_t to) {
 ngNode a,b;vec3_t delta,point,floor,down;trace_t tr;int step,steps;
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));VectorCopy(from,a.feet);VectorCopy(to,b.feet);
 VectorSubtract(to,from,delta);steps=(int)ceil(VectorLength(delta)/4.f);
 if(steps>24||!NG_TrStableLanding(to)||NG_Walk(&a,&b,ngStand))return qfalse;
 for(step=0;step<=steps;step++) {
  if(steps)VectorMA(from,(float)step/steps,delta,point);else VectorCopy(from,point);
  if(NG_Ground(point,floor,ngStand)||fabs(floor[2]-point[2])>18.25f)return qfalse;
  VectorCopy(floor,down);down[2]-=.25f;
  if(!NG_Trace(&tr,floor,down,ngStand)||tr.startsolid||tr.allsolid||tr.fraction==1.f||
     !NG_StaticGround(tr.entityNum)||tr.plane.normal[2]<.7f||(tr.surfaceFlags&SURF_SLICK))return qfalse;
 }
 return qtrue;
}
/* A descent is not a jump. Keep real capsule support while walking to the
 * edge, then integrate zero-impulse falling. Three explicit command-speed
 * samples use TC ground friction (stop control50), acceleration10, and the
 * full 3D PM_Accelerate push in air. This is bounded collision evidence for
 * those samples, not a claim of full Pmove or a live-tested transition. */
/* PM_ClipVelocity's overclip and PM_WalkMove's speed-preserving projection.
 * Used only while departing an independently proved gentle physical launch;
 * it neither resolves an obstacle nor projects an airborne contact. */
static void NG_TrDropGroundClip(vec3_t velocity,const vec3_t normal,qboolean preserveSpeed) {
 float speed=VectorLength(velocity),backoff=DotProduct(velocity,normal);int i;
 backoff=backoff<0.f?backoff*1.001f:backoff/1.001f;
 for(i=0;i<3;i++)velocity[i]-=normal[i]*backoff;
 if(preserveSpeed){VectorNormalize(velocity);VectorScale(velocity,speed,velocity);}
}
static qboolean NG_TrWalkDropSpeed(const ngNode *a,const ngNode *b,float wishspeed,int limit,ngDropProof *proof) {
 const float dt=.008f;float gravity=NG_TrGravity(),time,contact=0.f,horizontalLimit,maxImpact=0.f,maxDelta=0.f;
 vec3_t current,next,velocity,direction,push,down,error,landed,launchNormal,alias;trace_t tr;int step,intermediateContacts=0;
 qboolean airborne=qfalse,gentleLaunch=qfalse;
#define NG_DROP_FAIL(why) do {ngDropFailure=why;VectorCopy(current,ngDropFailurePoint);return qfalse;}while(0)
 VectorCopy(a->feet,current);VectorClear(velocity);VectorSubtract(b->feet,a->feet,direction);direction[2]=0;horizontalLimit=VectorNormalize(direction)+48.f;
 for(step=0;step<250;step++) {
  float length,amount,horizontal=sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]);qboolean followsPlane=qfalse;
  if(ngTraces>=limit-256)NG_DROP_FAIL("trace_budget");
  if(!airborne) {
   VectorCopy(current,down);down[2]-=.25f;
   if(!NG_Trace(&tr,current,down,ngStand)||tr.startsolid||tr.allsolid)NG_DROP_FAIL("ground_contact_solid");
   if(tr.fraction==1.f){airborne=qtrue;gentleLaunch=qfalse;}
   else {
    if(!NG_StaticGround(tr.entityNum)||tr.plane.normal[2]<.7f||(tr.surfaceFlags&SURF_SLICK))NG_DROP_FAIL("invalid_ground_contact");
    if(!step&&tr.plane.normal[2]>=.99f&&tr.plane.normal[2]<.99999f&&
       NG_TrPhysicalGroundAlias(a->feet,alias)) {
     gentleLaunch=qtrue;
    }
    if(gentleLaunch) {
     /* The rounded body can roll onto an edge bevel before losing ground.
      * Use that fresh real support normal, as PM_WalkMove does; the ordinary
      * normal/slick gate above still applies. This first ground phase ends
      * permanently on contact loss and never waives an airborne impact. */
     if(TCE_NavGroundKind(tr.entityNum)!=1)NG_DROP_FAIL("invalid_ground_contact");
     followsPlane=qtrue;VectorCopy(tr.plane.normal,launchNormal);
    }else velocity[2]=0.f;
    contact+=dt;
    if(horizontal<1.f)velocity[0]=velocity[1]=0.f;
    else {float friction=(horizontal>50.f?horizontal:50.f)*6.f*dt;
     VectorScale(velocity,friction<horizontal?1.f-friction/horizontal:0.f,velocity);
    }
   }
  }
  VectorCopy(direction,push);
  if(followsPlane){NG_TrDropGroundClip(push,launchNormal,qfalse);VectorNormalize(push);}
  VectorScale(push,wishspeed,push);VectorSubtract(push,velocity,push);
  length=VectorNormalize(push);amount=dt*wishspeed*(airborne?1.f:10.f);if(amount>length)amount=length;
  VectorMA(velocity,amount,push,velocity);
  if(followsPlane)NG_TrDropGroundClip(velocity,launchNormal,qtrue);
  VectorMA(current,dt,velocity,next);
  if(airborne){next[2]-=.5f*gravity*dt*dt;velocity[2]-=gravity*dt;}
  time=(step+1)*dt;
  if(!NG_Trace(&tr,current,next,ngStand)||tr.startsolid||tr.allsolid)NG_DROP_FAIL("hull_solid");
  if(tr.fraction<1.f) {
   float delta=velocity[2]*velocity[2]*(g_newbbox.integer?.64f:1.f)*.0002f;
   VectorCopy(tr.endpos,current); /* Failure examples name the actual impact. */
   VectorSubtract(tr.endpos,b->feet,error);
   if(ngProbe)G_Printf("navgen drop contact: step=%d airborne=%d fraction=%.7f entity=%d normal=(%.7f %.7f %.7f) surface=%x contents=%x feet=(%.4f %.4f %.4f) next=(%.4f %.4f %.4f) velocity=(%.4f %.4f %.4f) target_error=(%.4f %.4f %.4f)\n",step,airborne,tr.fraction,tr.entityNum,tr.plane.normal[0],tr.plane.normal[1],tr.plane.normal[2],tr.surfaceFlags,tr.contents,tr.endpos[0],tr.endpos[1],tr.endpos[2],next[0],next[1],next[2],velocity[0],velocity[1],velocity[2],error[0],error[1],error[2]);
   if(!airborne||!NG_StaticGround(tr.entityNum)||tr.plane.normal[2]<.7f||
      (tr.surfaceFlags&SURF_SLICK)||DotProduct(error,direction)>48.f||a->feet[2]-tr.endpos[2]>NG_TrSafeDropHeight()+.25f)NG_DROP_FAIL("blocked_hull_or_wrong_floor");
   if(delta>38.f)NG_DROP_FAIL("crashland_damage");
   if(NG_Hurt(current,ngStand)||(trap_PointContents(current,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)))NG_DROP_FAIL("liquid_or_hurt");
   if(delta>maxDelta){maxDelta=delta;maxImpact=fabs(velocity[2]);}
   VectorCopy(tr.endpos,landed);
   /* A first capsule contact may be on a short step/rim. Exactly as for
    * ladder exits, require centre support plus a full-hull ordinary walk
    * to the final nine-probe stopping area, not a flat circle at this
    * transient contact. No height snap bypasses collision here. */
   if(fabs(error[2])<=18.f&&error[0]*error[0]+error[1]*error[1]<=96.f*96.f&&NG_TrExitContinuation(landed,b->feet)) {
    VectorCopy(landed,proof->touchdown);VectorSubtract(b->feet,landed,error);
    proof->duration=time;proof->contactDuration=contact;proof->groundContinuation=VectorLength(error);
    proof->impact=maxImpact;proof->crashDelta=maxDelta;proof->intermediateContacts=intermediateContacts;return qtrue;
   }
   /* Flat intermediate treads remain real ground. Resume walking only
    * after a collision-tested skin offset; a wall, slope, stuck contact
    * or ninth intermediate impact stays rejected. Subsequent contact
    * loss again starts a zero-Z fall, as in the actual ground/air phases. */
   if(tr.plane.normal[2]<.999f||intermediateContacts>=8)NG_DROP_FAIL("ground_continuation");
   VectorCopy(current,next);next[2]+=.125f;
   if(!NG_TrClear(current,next))NG_DROP_FAIL("unstable_touchdown");
   VectorCopy(next,current);velocity[2]=0.f;airborne=qfalse;intermediateContacts++;continue;
  }
  if(NG_Hurt(next,ngStand)||(trap_PointContents(next,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)))NG_DROP_FAIL("liquid_or_hurt");
  VectorSubtract(next,a->feet,error);
  if(DotProduct(error,direction)>horizontalLimit||DotProduct(error,direction)>208.f||next[2]<b->feet[2]-18.f)NG_DROP_FAIL("left_proved_corridor");
  VectorCopy(next,current);
 }
 NG_DROP_FAIL("time_limit");
#undef NG_DROP_FAIL
}
static qboolean NG_TrWalkDrop(const ngNode *a,const ngNode *b,int limit,ngDropProof *proof) {
 vec3_t d,down;float horizontal;int speed;ngDropProof trial;trace_t tr;
 ngDropTests++;ngDropFailure="endpoint_bounds";VectorCopy(a->feet,ngDropFailurePoint);
 VectorSubtract(b->feet,a->feet,d);horizontal=sqrt(d[0]*d[0]+d[1]*d[1]);
 if(NG_TrGravity()<100.f||NG_TrGravity()>2000.f||a->crouch||b->crouch||horizontal<8.f||horizontal>160.f||d[2]>=-18.25f||d[2]<-NG_TrSafeDropHeight())return qfalse;
 if(ngTraces>=limit-512){ngDropFailure="trace_budget";return qfalse;}
 /* At departure the actual stationary player capsule must be grounded,
  * as in PM_GroundTrace's .25-unit sweep. A nine-ray full footprint is a
  * stopping-area restriction, not the engine's condition for standing on
  * an edge before walking off it. Keep that stronger test at the landing. */
 ngDropFailure="launch_capsule_clearance";
 if(!NG_Trace(&tr,a->feet,a->feet,ngStand)||tr.startsolid||tr.allsolid)return qfalse;
 VectorCopy(a->feet,down);down[2]-=.25f;ngDropFailure="launch_capsule_ground";
 if(!NG_Trace(&tr,a->feet,down,ngStand)||tr.startsolid||tr.allsolid||tr.fraction==1.f||
    !NG_StaticGround(tr.entityNum)||tr.plane.normal[2]<.7f||(tr.surfaceFlags&SURF_SLICK))return qfalse;
 if(!NG_TrStableLanding(b->feet)){ngDropFailure="landing_support";VectorCopy(b->feet,ngDropFailurePoint);return qfalse;}
 memset(proof,0,sizeof(*proof));
 for(speed=0;speed<3;speed++) {
  memset(&trial,0,sizeof(trial));
  if(!NG_TrWalkDropSpeed(a,b,ngDropSpeeds[speed],limit,&trial))return qfalse;
  /* Report the slowest successful sample, plus worst observed impact. */
  if(!speed)*proof=trial;
  if(trial.crashDelta>proof->crashDelta){proof->crashDelta=trial.crashDelta;proof->impact=trial.impact;}
 }
 ngDropFailure="accepted";ngDropAccepted++;return qtrue;
}
/* Upgrade one checked authored downward edge without making the other
 * outgoing edges jump.  The target property is directed and survives WAY
 * export/reimport; a conflicting target remains a diagnostic, not a waiver. */
static qboolean NG_TrCommitManualDrop(ngEdge *edge,const ngDropProof *proof) {
 ngNode *launch=&ngNodes[edge->from],*landing=&ngNodes[edge->to];int i;
 if((launch->editRole&8)&&launch->editDropId!=landing->id)return qfalse;
 for(i=0;i<ngDropProofCount;i++)if(ngDropProofs[i].launch==edge->from) {
  if(ngDropProofs[i].landing!=edge->to)return qfalse;
  break;
 }
 if(i==ngDropProofCount){if(ngDropProofCount>=32)return qfalse;ngDropProofCount++;}
 ngDropProofs[i]=*proof;ngDropProofs[i].approach=ngDropProofs[i].launch=edge->from;
 ngDropProofs[i].landing=edge->to;ngDropProofs[i].pairedJumpLaunch=edge->from;
 launch->editRole|=8;launch->editSourceId=launch->id;launch->editDropId=landing->id;
 launch->topologyProtected=landing->topologyProtected=qtrue;
 launch->radius=2.f;if(landing->radius<=0.f||landing->radius>6.f)landing->radius=6.f;
 return qtrue;
}
/* PM_CheckJump's full impulse is conditional on a slow, forward-facing
 * launch. Runtime first brakes at launch-8, primes40units/sec, then issues
 * one forward+jump command only at the exact checked launch band. Native
 * proof sweeps all 3 initial speeds x3 command speeds x3 longitudinal
 * launch offsets, at8ms. It is sampled evidence, never a live-success claim.
 * The first jump command contains upmove, reducing its wishspeed by sqrt2;
 * subsequent air commands release jump. No fabricated Pmove clients. */
static qboolean NG_TrStandSpeed(const ngNode *a,const ngNode *b,float initial,float wishspeed,float offset,int limit,ngJumpProof *proof) {
 const float dt=.008f;float gravity=NG_TrGravity();
 vec3_t current,next,velocity,direction,push,error,down,floor;trace_t tr;int step;
 VectorSubtract(b->feet,a->feet,direction);direction[2]=0;VectorNormalize(direction);
 VectorMA(a->feet,offset,direction,current);
#define NG_STAND_FAIL(code) do {ngJumpArcFailure=(code);return qfalse;}while(0)
 ngJumpArcFailure=4;VectorCopy(current,ngJumpArcFailurePoint);
 if(NG_Ground(current,floor,ngStand)||fabs(floor[2]-a->feet[2])>1.f||!NG_TrClear(a->feet,floor))return qfalse;
 VectorCopy(floor,current);VectorCopy(current,down);down[2]-=.25f;
 if(!NG_Trace(&tr,current,down,ngStand)||tr.startsolid||tr.allsolid||tr.fraction==1.f||
    !NG_StaticGround(tr.entityNum)||!NG_OrdinarySurface(tr.surfaceFlags,tr.plane.normal[2])||(tr.surfaceFlags&SURF_SLICK))return qfalse;
 VectorScale(direction,initial,velocity);velocity[2]=TCE_NavStandingImpulse(g_newbbox.integer);
 for(step=0;step<180;step++) {
  float length,amount,commandWish=step?wishspeed:wishspeed*.70710678f;
  ngJumpArcFailure=7;if(ngTraces>=limit-512)return qfalse;
  VectorScale(direction,commandWish,push);VectorSubtract(push,velocity,push);
  length=VectorNormalize(push);amount=dt*commandWish;if(amount>length)amount=length;
  VectorMA(velocity,amount,push,velocity);
  VectorMA(current,dt,velocity,next);next[2]-=.5f*gravity*dt*dt;velocity[2]-=gravity*dt;
  if(!NG_Trace(&tr,current,next,ngStand))NG_STAND_FAIL(7);
  VectorCopy(tr.endpos,ngJumpArcFailurePoint);
  if(tr.startsolid||tr.allsolid){proof->failurePhase=1;proof->failureVz=velocity[2];VectorCopy(tr.plane.normal,proof->failureNormal);NG_STAND_FAIL(8);}
  if(tr.fraction<1.f) {
   float crash=velocity[2]*velocity[2]*(g_newbbox.integer?.64f:1.f)*.0002f;
   VectorSubtract(tr.endpos,b->feet,error);ngJumpArcFailure=9;
   if(velocity[2]>=0.f||!NG_StaticGround(tr.entityNum)||!NG_OrdinarySurface(tr.surfaceFlags,tr.plane.normal[2])||
      fabs(error[2])>18.f||DotProduct(error,direction)>8.f||error[0]*error[0]+error[1]*error[1]>96.f*96.f||crash>38.f) {
    proof->failurePhase=1;proof->failureVz=velocity[2];VectorCopy(tr.plane.normal,proof->failureNormal);
    return qfalse;
   }
   if(NG_Hurt(tr.endpos,ngStand))NG_STAND_FAIL(11);
   if(trap_PointContents(tr.endpos,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME))NG_STAND_FAIL(10);
   proof->duration=(step+1)*dt;proof->continuation=VectorLength(error);proof->impact=fabs(velocity[2]);proof->crash=crash;VectorCopy(tr.endpos,proof->touchdown);
   if(NG_TrStandingExitContinuation(tr.endpos,b->feet))return qtrue;
   /* A capsule can first touch a real platform rim before its centre
    * crosses the edge. PM_GroundTrace uses that same capsule, not a centre
    * ray. Prove bounded forward grounded steps until centre support is
    * available; never snap to a different floor or invent another impulse. */
   VectorCopy(tr.endpos,current);velocity[2]=0.f;
   {int walk;qboolean gentleGround=qfalse;vec3_t groundNormal;
    for(walk=0;walk<100;walk++) {
     float speed=sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]);
     float friction=(speed>50.f?speed:50.f)*6.f*dt;
     if(ngTraces>=limit-512)NG_STAND_FAIL(7);
     VectorCopy(current,down);down[2]-=.25f;
     if(!NG_Trace(&tr,current,down,ngStand))NG_STAND_FAIL(7);
     if(tr.startsolid||tr.allsolid||tr.fraction==1.f||
        !NG_StaticGround(tr.entityNum)||!NG_OrdinarySurface(tr.surfaceFlags,tr.plane.normal[2]))NG_STAND_FAIL(13);
     if(gentleGround) {
      if(TCE_NavGroundKind(tr.entityNum)!=1||tr.plane.normal[2]<.99f||(tr.surfaceFlags&SURF_SLICK))NG_STAND_FAIL(13);
      VectorCopy(tr.plane.normal,groundNormal);
     }
     if(speed<1.f)velocity[0]=velocity[1]=0.f;
     else VectorScale(velocity,friction<speed?1.f-friction/speed:0.f,velocity);
     VectorCopy(direction,push);
     if(gentleGround){NG_TrDropGroundClip(push,groundNormal,qfalse);VectorNormalize(push);}
     VectorScale(push,wishspeed,push);VectorSubtract(push,velocity,push);
     length=VectorNormalize(push);amount=dt*wishspeed*10.f;if(amount>length)amount=length;
     VectorMA(velocity,amount,push,velocity);
     if(gentleGround)NG_TrDropGroundClip(velocity,groundNormal,qtrue);
     VectorMA(current,dt,velocity,next);
     if(!NG_Trace(&tr,current,next,ngStand))NG_STAND_FAIL(7);
     VectorCopy(tr.endpos,ngJumpArcFailurePoint);
     if(tr.startsolid||tr.allsolid||tr.fraction<1.f){proof->failurePhase=2;proof->failureVz=velocity[2];VectorCopy(tr.plane.normal,proof->failureNormal);}
     if(tr.startsolid||tr.allsolid)NG_STAND_FAIL(8);
     if(tr.fraction<1.f) {
      float remaining=(1.f-tr.fraction)*dt;vec3_t contact;
      /* The flat bumper can meet a gentle hood after a valid touchdown.
       * Clip only this grounded collision, as PM_SlideMove does, then
       * sweep the real remainder of this frame. Never alter an air hit,
       * accept a wall, or substitute an unchecked height projection. */
      if(TCE_NavGroundKind(tr.entityNum)!=1||tr.plane.normal[2]<.99f||
         tr.plane.normal[2]>=.99999f||(tr.surfaceFlags&SURF_SLICK)||
         (!gentleGround&&!NG_TrGentlePlaneSupport(b->feet)))NG_STAND_FAIL(9);
      VectorCopy(tr.endpos,contact);VectorCopy(tr.plane.normal,groundNormal);
      if(NG_Hurt(contact,ngStand))NG_STAND_FAIL(11);
      if(trap_PointContents(contact,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME))NG_STAND_FAIL(10);
      NG_TrDropGroundClip(velocity,groundNormal,qfalse);VectorMA(contact,remaining,velocity,next);
      if(!NG_Trace(&tr,contact,next,ngStand))NG_STAND_FAIL(7);
      if(!tr.startsolid&&!tr.allsolid&&tr.fraction<1.f&&TCE_NavGroundKind(tr.entityNum)==1&&
         !(tr.surfaceFlags&SURF_SLICK)) {
       vec3_t difference;VectorSubtract(tr.plane.normal,groundNormal,difference);
       if(DotProduct(difference,difference)<=.000001f) {
        /* PM_SlideMove nudges velocity when a non-axial plane repeats.
         * One bounded retry uses only the remaining frame time and a
         * fresh full sweep; it never moves the contact point by fiat. */
        remaining*=1.f-tr.fraction;VectorCopy(tr.endpos,contact);
        VectorMA(velocity,1.f,tr.plane.normal,velocity);VectorMA(contact,remaining,velocity,next);
        if(!NG_Trace(&tr,contact,next,ngStand))NG_STAND_FAIL(7);
       }
      }
      VectorCopy(tr.endpos,ngJumpArcFailurePoint);
      if(tr.startsolid||tr.allsolid||tr.fraction<1.f){proof->failureVz=velocity[2];VectorCopy(tr.plane.normal,proof->failureNormal);NG_STAND_FAIL(9);}
      gentleGround=qtrue;proof->failurePhase=0;proof->failureVz=0.f;VectorClear(proof->failureNormal);
     }
     if(NG_Hurt(next,ngStand))NG_STAND_FAIL(11);
     if(trap_PointContents(next,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME))NG_STAND_FAIL(10);
     VectorSubtract(next,b->feet,error);if(DotProduct(error,direction)>8.f)NG_STAND_FAIL(12);
     if(gentleGround) {
      VectorCopy(next,down);down[2]-=.25f;
      if(!NG_Trace(&tr,next,down,ngStand))NG_STAND_FAIL(7);
      if(tr.startsolid||tr.allsolid||tr.fraction==1.f||TCE_NavGroundKind(tr.entityNum)!=1||
         tr.plane.normal[2]<.99f||(tr.surfaceFlags&SURF_SLICK))NG_STAND_FAIL(13);
      if(NG_TrPhysicalGroundAlias(next,floor)&&NG_TrStandingExitContinuation(next,b->feet))return qtrue;
     }
     if(!NG_Ground(next,floor,ngStand)&&fabs(floor[2]-next[2])<=1.f&&NG_TrStandingExitContinuation(next,b->feet))return qtrue;
     VectorCopy(next,current);
    }}
   NG_STAND_FAIL(12);
  }
  if(NG_Hurt(next,ngStand))NG_STAND_FAIL(11);
  if(trap_PointContents(next,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME))NG_STAND_FAIL(10);
  VectorSubtract(next,b->feet,error);
  if(DotProduct(error,direction)>8.f||(velocity[2]<=0.f&&next[2]<b->feet[2]-18.f))NG_STAND_FAIL(12);
  VectorCopy(next,current);
 }
 NG_STAND_FAIL(12);
#undef NG_STAND_FAIL
}
static qboolean NG_TrStandingJump(const ngNode *a,const ngNode *b,int limit,ngJumpProof *proof) {
 static const float speeds[3]={36.f,40.f,44.f},offsets[3]={0.f,-2.f,2.f};
 vec3_t d,stop,floor;ngNode braking=*a;ngJumpProof trial;float horizontal,gravity=NG_TrGravity(),impulse=TCE_NavStandingImpulse(g_newbbox.integer);int v,w,o;
 ngJumpArcFailure=1;if(gravity<100.f||gravity>2000.f||a->crouch||b->crouch)return qfalse;
 VectorSubtract(b->feet,a->feet,d);horizontal=sqrt(d[0]*d[0]+d[1]*d[1]);
 ngJumpArcFailure=2;if(horizontal<8.f||horizontal>128.f)return qfalse;
 ngJumpArcFailure=3;if(d[2]<=18.25f||d[2]>impulse*impulse/(2.f*gravity)-2.f)return qfalse;
 d[2]=0;VectorScale(d,1.f/horizontal,d);VectorMA(a->feet,-8.f,d,stop);
 ngJumpArcFailure=4;if(NG_Ground(stop,floor,ngStand)||fabs(floor[2]-a->feet[2])>1.f)return qfalse;
 VectorCopy(floor,braking.feet);
 if(NG_Walk(&braking,a,ngStand)||NG_Walk(a,&braking,ngStand))return qfalse;
 ngJumpArcFailure=13;if(!NG_TrStandingLanding(b->feet))return qfalse;
 memset(proof,0,sizeof(*proof));
 for(o=0;o<3;o++)for(v=0;v<3;v++)for(w=0;w<3;w++) {
  memset(&trial,0,sizeof(trial));
  if(!NG_TrStandSpeed(a,b,speeds[v],ngDropSpeeds[w],offsets[o],limit,&trial)){*proof=trial;return qfalse;}
  if(!o&&!v&&!w)*proof=trial;
  if(trial.crash>proof->crash){proof->crash=trial.crash;proof->impact=trial.impact;}
 }
 proof->standing=qtrue;ngJumpArcFailure=0;return qtrue;
}
/* Preserve the exact proof of an authored directed standing jump. Rechecking
 * an existing recipe must not lose its receipt or allocate a second one.
 * No geometry, flags, radii or adjacency is changed by this bookkeeping. */
static qboolean NG_TrCommitManualStandingJump(ngEdge *edge,const ngJumpProof *proof) {
 int i;
 if(edge->from<0||edge->from>=ngCount||edge->to<0||edge->to>=ngCount||!proof->standing)return qfalse;
 for(i=0;i<ngJumpProofCount;i++)if(ngJumpProofs[i].launch==edge->from) {
  if(ngJumpProofs[i].landing!=edge->to)return qfalse;
  break;
 }
 if(i==ngJumpProofCount){if(ngJumpProofCount>=64)return qfalse;ngJumpProofCount++;}
 ngJumpProofs[i]=*proof;ngJumpProofs[i].approach=ngJumpProofs[i].launch=edge->from;
 ngJumpProofs[i].landing=edge->to;
 return qtrue;
}
/* Read-only current-world diagnostic. It invokes the same complete arc and
 * 27-sample standing proof used by import, preserving all touched counters
 * and failure state. It neither adds a receipt nor changes a graph edge. */
static void NG_TrProbeJumps(const ngNode *from,const ngNode *to,int limit) {
 int savedTraces=ngTraces,savedDiagnostics=ngStepDiagnostics,savedFailure=ngJumpArcFailure;
 qboolean savedProbe=ngProbe;
 unsigned int savedConditional[MAX_GENTITIES];vec3_t savedPoint,runningHit,standingHit;
 qboolean running,standing;int runningReason,standingReason,runningQueries,standingQueries;ngJumpProof proof;
 VectorCopy(ngJumpArcFailurePoint,savedPoint);memcpy(savedConditional,ngConditionalGroundHits,sizeof(savedConditional));
 /* Keep one complete response below classic ET's RCON packet ceiling.
  * Quiet only diagnostic printing; all body queries and physics still run. */
 ngProbe=qfalse;ngStepDiagnostics=5;
 VectorCopy(from->feet,ngJumpArcFailurePoint);running=NG_TrJump(from,to);
 runningReason=ngJumpArcFailure;VectorCopy(ngJumpArcFailurePoint,runningHit);runningQueries=ngTraces-savedTraces;
 memset(&proof,0,sizeof(proof));VectorCopy(from->feet,ngJumpArcFailurePoint);
 standing=NG_TrStandingJump(from,to,limit,&proof);standingReason=ngJumpArcFailure;
 VectorCopy(ngJumpArcFailurePoint,standingHit);standingQueries=ngTraces-savedTraces-runningQueries;
 /* G_Printf itself has a smaller per-call limit than the UDP reply. Each
  * independent JSON record must fit, even at the allowed coordinate bound. */
 G_Printf("navgen probe jump {\"part\":\"points\",\"coordinates\":\"feet\",\"from\":[%.4f,%.4f,%.4f],\"to\":[%.4f,%.4f,%.4f],\"readonly\":true,\"live_verified\":false}\n",
  from->feet[0],from->feet[1],from->feet[2],to->feet[0],to->feet[1],to->feet[2]);
 G_Printf("navgen probe jump {\"part\":\"running\",\"accepted\":%s,\"reason\":\"%s\",\"queries\":%d,\"hit\":[%.4f,%.4f,%.4f]}\n",
  running?"true":"false",ngJumpFailureNames[runningReason],runningQueries,runningHit[0],runningHit[1],runningHit[2]);
 G_Printf("navgen probe jump {\"part\":\"standing\",\"accepted\":%s,\"reason\":\"%s\",\"queries\":%d,\"hit\":[%.4f,%.4f,%.4f],\"phase\":\"%s\",\"normal\":[%.7f,%.7f,%.7f],\"velocity_z\":%.5f}\n",
  standing?"true":"false",ngJumpFailureNames[standingReason],standingQueries,standingHit[0],standingHit[1],standingHit[2],
  proof.failurePhase==1?"air":proof.failurePhase==2?"ground":"none",proof.failureNormal[0],proof.failureNormal[1],proof.failureNormal[2],proof.failureVz);
 if(standing)G_Printf("navgen probe jump {\"part\":\"standing_proof\",\"samples\":27,\"duration\":%.4f,\"crash\":%.4f,\"touchdown\":[%.4f,%.4f,%.4f]}\n",
  proof.duration,proof.crash,proof.touchdown[0],proof.touchdown[1],proof.touchdown[2]);
 ngTraces=savedTraces;ngStepDiagnostics=savedDiagnostics;ngJumpArcFailure=savedFailure;ngProbe=savedProbe;
 VectorCopy(savedPoint,ngJumpArcFailurePoint);memcpy(ngConditionalGroundHits,savedConditional,sizeof(savedConditional));
}
/* The exit adapter keeps facing the ladder for horizontal strafing while
 * attached: forward=0, right=+/-127. PM_LadderMove applies half command-scale
 * sideways speed, friction14 and accel10; with no vertical wish its existing
 * Z velocity approaches zero. Only after actual contact loss does PM_AirMove
 * apply the full 3D acceleration push and falling gravity. A forward top exit
 * instead continues forwardmove=127 (vertical .9*scale wish) until contact
 * loss. This is the same input classification as the live Omni adapter. */
static qboolean NG_TrLadderDismount(const vec3_t from,const vec3_t to,const vec3_t normal,vec3_t landed,float *duration,float *speed) {
 const float dt=.008f;float gravity=NG_TrGravity(),time,wishspeed,amount,length,contactTime=0.f,side;
 vec3_t current,next,velocity,direction,push,error,down,floor,tangent;trace_t tr;int step;
 qboolean attached=qtrue,pull=qfalse,onLadder,forwardExit;
#define NG_EXIT_FAIL(code) do {ngExitFailures[code]++;if(ngExitDebug)G_Printf("navgen exit failure%d step%d attached%d at %.4f %.4f %.4f velocity %.4f %.4f %.4f target %.4f %.4f %.4f\n",code,step,attached,current[0],current[1],current[2]+ngOrigin,velocity[0],velocity[1],velocity[2],to[0],to[1],to[2]+ngOrigin);return qfalse;}while(0)
 VectorCopy(from,current);VectorSet(velocity,0,0,115.f);step=-1;
 if(gravity<100.f||gravity>2000.f)NG_EXIT_FAIL(0);
 if(NG_Ground(to,floor,ngStand)||fabs(floor[2]-to[2])>1.f||!NG_TrStableLanding(floor))NG_EXIT_FAIL(1);
 VectorCopy(from,current);VectorSet(velocity,0,0,115.f);
 VectorSet(tangent,-normal[1],normal[0],0);VectorSubtract(to,from,error);
 forwardExit=TCE_NavLadderForwardExit(error[0],error[1],normal[0],normal[1]);
 side=DotProduct(error,tangent);if(!forwardExit&&fabs(side)<2.f)NG_EXIT_FAIL(2);
 if(side<0.f)VectorInverse(tangent);
 for(step=0;step<256;step++){
  onLadder=attached&&NG_TrAirborneLadderContactState(current,normal,&pull);
  if(!onLadder)attached=qfalse;
  if(onLadder){
   if(forwardExit){
    VectorScale(normal,-1.f,direction);wishspeed=.9f*NG_TrExitWishspeed(direction,normal);
    VectorSet(direction,0.f,0.f,1.f);
   }else{VectorCopy(tangent,direction);wishspeed=.5f*NG_TrExitWishspeed(direction,normal);}
   if(pull){velocity[0]=-200.f*normal[0];velocity[1]=-200.f*normal[1];}
   if(VectorLength(velocity)<1.f)velocity[0]=velocity[1]=0.f;
   else VectorScale(velocity,1.f-14.f*dt,velocity);
   if(fabs(velocity[0])<1.f)velocity[0]=0.f;
   if(fabs(velocity[1])<1.f)velocity[1]=0.f;
   contactTime+=dt;
  }else{
   VectorSubtract(to,current,direction);direction[2]=0;
   if(VectorNormalize(direction)<.125f)VectorClear(direction);
   wishspeed=NG_TrExitWishspeed(direction,normal);
  }
  if(!step)ngLadderProofWorking.wishspeed=wishspeed;
  VectorScale(direction,wishspeed,push);VectorSubtract(push,velocity,push);
  length=VectorNormalize(push);amount=dt*wishspeed*(onLadder?10.f:1.f);if(amount>length)amount=length;
  VectorMA(velocity,amount,push,velocity);
  if(onLadder){
   /* PM_LadderMove applies the vertical gravity brake only for zero Z wish. */
   if(!forwardExit){
    if(velocity[2]>0.f){velocity[2]-=gravity*dt;if(velocity[2]<0.f)velocity[2]=0.f;}
    else{velocity[2]+=gravity*dt;if(velocity[2]>0.f)velocity[2]=0.f;}
   }
   VectorMA(current,dt,velocity,next);
  }else{
   VectorMA(current,dt,velocity,next);next[2]-=.5f*gravity*dt*dt;velocity[2]-=gravity*dt;
  }
  time=(step+1)*dt;
  if(ngExitDebug&&(step%16==0||!onLadder))G_Printf("navgen exit tick%d ladder%d pull%d pos %.4f %.4f %.4f vel %.4f %.4f %.4f wish%.4f\n",step,onLadder,onLadder?pull:0,current[0],current[1],current[2]+ngOrigin,velocity[0],velocity[1],velocity[2],wishspeed);
  if(!NG_Trace(&tr,current,next,ngStand)||tr.startsolid||tr.allsolid)NG_EXIT_FAIL(3);
  if(tr.fraction<1.f){
   if(ngExitDebug)G_Printf("navgen exit hit%d point %.4f %.4f %.4f normal %.4f %.4f %.4f entity%d surface%x fraction%.5f\n",step,tr.endpos[0],tr.endpos[1],tr.endpos[2]+ngOrigin,tr.plane.normal[0],tr.plane.normal[1],tr.plane.normal[2],tr.entityNum,tr.surfaceFlags,tr.fraction);
   VectorSubtract(tr.endpos,to,error);
   if(!NG_StaticGround(tr.entityNum)||tr.plane.normal[2]<.7f||fabs(error[2])>18.f||
      error[0]*error[0]+error[1]*error[1]>96.f*96.f)NG_EXIT_FAIL(4);
   if(NG_Ground(tr.endpos,landed,ngStand)||fabs(landed[2]-tr.endpos[2])>1.f)NG_EXIT_FAIL(5);
   if(forwardExit?!NG_TrForwardExitContinuation(landed,to):!NG_TrExitContinuation(landed,to))NG_EXIT_FAIL(6);
   VectorCopy(landed,down);down[2]-=2.f;
   if(!NG_Trace(&tr,landed,down,ngStand)||tr.startsolid||tr.allsolid||tr.fraction==1.f||
      !NG_StaticGround(tr.entityNum)||tr.plane.normal[2]<.7f)NG_EXIT_FAIL(7);
   VectorCopy(landed,ngLadderProofWorking.touchdown);VectorSubtract(to,landed,error);
   ngLadderProofWorking.groundContinuation=VectorLength(error);
   ngLadderProofWorking.contactDuration=contactTime;
   ngLadderProofWorking.forwardExit=forwardExit;
   VectorCopy(to,landed);
   *duration=time;*speed=sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]);return qtrue;
  }
  if(NG_Hurt(next,ngStand)||(trap_PointContents(next,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)))NG_EXIT_FAIL(8);
  if(next[2]<to[2]-18.f)NG_EXIT_FAIL(8);
  VectorCopy(next,current);
 }
 NG_EXIT_FAIL(9);
#undef NG_EXIT_FAIL
}
/* The upper route anchor can lie well past the first reachable landing.
 * Find a supported stopping place, then prove an ordinary walk from it to
 * the upper graph. A capsule merely touching a shaft rim is not a landing:
 * the independent footprint probes must also all find real floor. */
static qboolean NG_TrLadderLanding(const vec3_t from,const ngNode *target,const vec3_t normal,vec3_t landing,float *duration,float *speed) {
 static const float directions[16][2]={{1,0},{.92388f,.382683f},{.707107f,.707107f},{.382683f,.92388f},
  {0,1},{-.382683f,.92388f},{-.707107f,.707107f},{-.92388f,.382683f},
  {-1,0},{-.92388f,-.382683f},{-.707107f,-.707107f},{-.382683f,-.92388f},
  {0,-1},{.382683f,-.92388f},{.707107f,-.707107f},{.92388f,-.382683f}};
 int radius,direction;ngNode candidate;vec3_t point,down,delta,arcLanding;trace_t support;
 float discriminant=115.f*115.f-2.f*NG_TrGravity()*(target->feet[2]-from[2]);
 if(discriminant<=0.f)return qfalse;
 if(NG_TrLadderDismount(from,target->feet,normal,arcLanding,duration,speed)){
  candidate=*target;VectorCopy(arcLanding,candidate.feet);
  if(!NG_Walk(&candidate,target,ngStand)&&!NG_Walk(target,&candidate,ngStand)){VectorCopy(arcLanding,landing);return qtrue;}
 }
 memset(&candidate,0,sizeof(candidate));
 for(radius=2;radius<=64;radius+=2)for(direction=0;direction<16;direction++){
  VectorSet(point,from[0]+radius*directions[direction][0],from[1]+radius*directions[direction][1],target->feet[2]);
  VectorSubtract(point,target->feet,delta);if(DotProduct(delta,delta)>80.f*80.f)continue;
  ngLandingProbes++;
  if(NG_Ground(point,candidate.feet,ngStand)||fabs(candidate.feet[2]-target->feet[2])>1.f)continue;
  ngLandingGrounded++;
  if(!NG_TrStableLanding(candidate.feet))continue;
  VectorCopy(candidate.feet,down);down[2]-=2.f;
  if(!NG_Trace(&support,candidate.feet,down,ngStand)||support.startsolid||support.allsolid||support.fraction==1.f||
     !NG_StaticGround(support.entityNum)||support.plane.normal[2]<.7f)continue;
  ngLandingSupported++;
  if(!NG_TrLadderDismount(from,candidate.feet,normal,arcLanding,duration,speed))continue;
  VectorCopy(arcLanding,candidate.feet);
  ngLandingArcs++;
  if(NG_Walk(&candidate,target,ngStand)||NG_Walk(target,&candidate,ngStand))continue;
  ngLandingWalks++;VectorCopy(candidate.feet,landing);return qtrue;
 }
 return qfalse;
}
/* A small controlled tangential shift while still on the actual ladder
 * reduces the required air gap. Every shaft/shift/release segment must have
 * both standing-capsule clearance and continuous native ladder contact. */
static qboolean NG_TrContactSegment(const vec3_t from,const vec3_t to,const vec3_t normal) {
 vec3_t delta,point;int step,steps;
 if(!NG_TrClear(from,to))return qfalse;
 VectorSubtract(to,from,delta);steps=(int)ceil(VectorLength(delta)/4.f);if(steps<1)steps=1;
 for(step=0;step<=steps;step++){
  VectorMA(from,(float)step/steps,delta,point);
  if(!NG_TrAirborneLadderContact(point,normal))return qfalse;
 }
 return qtrue;
}
static qboolean NG_TrLadderAlignedExit(const ngNode *bottom,const ngNode *target,const vec3_t normal,vec3_t launch) {
 static const float heights[4]={8.f,4.f,2.f,0.f};
 vec3_t tangent,delta,alignment,release,point;float side,z;int shift,h,pass;
 VectorSet(tangent,-normal[1],normal[0],0);VectorSubtract(target->feet,launch,delta);
 side=DotProduct(delta,tangent)<0.f?-1.f:1.f;
 if(target->feet[2]>launch[2]||target->feet[2]<launch[2]-48.f)return qfalse;
 for(pass=0;pass<2;pass++)for(shift=4;shift<=ngWidth+8.f;shift+=2)for(h=0;h<4;h++){
  VectorMA(launch,(pass?-side:side)*shift,tangent,alignment);alignment[2]=target->feet[2]+heights[h];
  if(launch[2]-alignment[2]<8.f||!NG_TrContactSegment(bottom->feet,alignment,normal))continue;
  VectorCopy(alignment,release);
  for(z=alignment[2]+2.f;z<=launch[2];z+=2.f){
   VectorCopy(alignment,point);point[2]=z;
   if(!NG_TrContactSegment(alignment,point,normal))break;
   VectorCopy(point,release);
  }
  if(release[2]-alignment[2]<8.f)continue;
  if(!NG_TrLadderLanding(release,target,normal,ngLadderProofWorking.landing,&ngLadderProofWorking.duration,&ngLadderProofWorking.speed))continue;
  ngLadderProofWorking.aligned=qtrue;VectorCopy(alignment,ngLadderProofWorking.alignment);
  VectorCopy(release,launch);return qtrue;
 }
 return qfalse;
}
/* Starting the side transfer at the highest possible attachment point
 * loses contact immediately through the remaining upward velocity. Search
 * lower, directly supported exit targets first so PM_LadderMove can build
 * sideways speed while still attached. Every trial uses the same native
 * collision/contact simulation, never an assumed displacement. */
static qboolean NG_TrLadderLowerTransfer(const ngNode *bottom,const ngNode *target,const vec3_t normal,vec3_t launch) {
 vec3_t candidate,landing;float duration,speed,z,maximum=launch[2];
 /* Sixteen units also keep the release's six-unit arrival radius clear of
  * the central descent guide's eight-unit radius on the upper floor. */
 for(z=target->feet[2]+16.f;z<=target->feet[2]+18.f&&z<=maximum;z+=2.f){
  VectorCopy(launch,candidate);candidate[2]=z;
  if(!NG_TrContactSegment(bottom->feet,candidate,normal))continue;
  ngLowerTransferTests++;
  if(!NG_TrLadderDismount(candidate,target->feet,normal,landing,&duration,&speed))continue;
  ngLowerTransferSuccesses++;
  ngLadderProofWorking.duration=duration;ngLadderProofWorking.speed=speed;
  VectorCopy(landing,ngLadderProofWorking.landing);VectorCopy(candidate,launch);return qtrue;
 }
 return qfalse;
}
/* A forward exit starts while still attached and continues climbing until
 * contact is actually lost. Its input transition therefore need not occupy
 * the last two units below the descent guide. Find an earlier attached
 * release with disjoint 6/8-unit arrival spheres, then repeat the SAME full
 * contact/air/landing simulation to the unchanged upper floor. Never raise
 * the impulse, move the descent target, or accept an unproved overlap. */
static qboolean NG_TrLadderSeparateForwardRelease(const ngNode *bottom,const ngNode *top,const vec3_t normal) {
 ngLadderProof saved=ngLadderProofWorking;vec3_t delta,candidate,landed;float duration,speed;int step;
 if(!saved.ballistic||!saved.forwardExit||saved.aligned)return qtrue;
 VectorSubtract(top->feet,saved.launch,delta);
 if(VectorLength(delta)>=14.001f)return qtrue;
 for(step=1;step<=18;step++){
  VectorCopy(saved.launch,candidate);candidate[2]-=step*2.f;
  if(candidate[2]<saved.landing[2]-18.f||candidate[2]<bottom->feet[2]+8.f)break;
  VectorSubtract(top->feet,candidate,delta);if(VectorLength(delta)<14.001f)continue;
  if(!NG_TrContactSegment(bottom->feet,candidate,normal))continue;
  ngLadderProofWorking=saved;
  if(!NG_TrLadderDismount(candidate,saved.landing,normal,landed,&duration,&speed))continue;
  ngLadderProofWorking.duration=duration;ngLadderProofWorking.speed=speed;
  VectorCopy(candidate,ngLadderProofWorking.launch);return qtrue;
 }
 ngLadderProofWorking=saved;return qfalse;
}
static qboolean NG_TrLadder(int low,int high,ngNode *bottom,ngNode *top,qboolean *reverse) {
 static const float directions[8][2]={{1,0},{.7071f,.7071f},{0,1},{-.7071f,.7071f},
  {-1,0},{-.7071f,-.7071f},{0,-1},{.7071f,-.7071f}};
 int k,lift;vec3_t ray,normal,point,offset,exitRaised,launch;trace_t hit,tr;float z;
 const ngNode *a=&ngNodes[low],*b=&ngNodes[high];
 ngLadderUnsupportedExit=qfalse;
 for(k=0;k<8;k++){
  memset(&ngLadderProofWorking,0,sizeof(ngLadderProofWorking));
  ngLadderProofWorking.ascentRelease=ngLadderProofWorking.ascentAlignment=-1;
  VectorSet(offset,directions[k][0]*64.f,directions[k][1]*64.f,0);
  if(!NG_TrBodyLadderRay(&hit,a->feet,offset,NULL)){ngLadderFailure[0]++;continue;}VectorCopy(hit.plane.normal,normal);
  memset(bottom,0,sizeof(*bottom));memset(top,0,sizeof(*top));
  VectorMA(hit.endpos,ngWidth+.25f,normal,point);point[2]=a->feet[2];
  /* Keep the mounting lane through the lower shaft. A lateral floor exit
   * does not prove that the entire ladder shaft is wide enough for that lane. */
  if(NG_Ground(point,bottom->feet,ngStand)||fabs(bottom->feet[2]-a->feet[2])>18.f){ngLadderFailure[1]++;continue;}
  if(!NG_TrNativeLadderContact(bottom->feet,normal)){ngLadderFailure[1]++;continue;}
  if(NG_Walk(a,bottom,ngStand)||NG_Walk(bottom,a,ngStand)){ngLadderFailure[2]++;continue;}
  /* Ascending now targets the exit floor (node4) directly with CLIMB,
   * not a raised intermediate point consumed early by its radius. Node3
   * is the descent entry at the lip; prefer the closest collision-free
   * position to that edge, then try higher clearance only if necessary. */
  for(lift=2;lift<=42;lift+=8){
   VectorMA(bottom->feet,-.125f,normal,top->feet);top->feet[2]=b->feet[2]+lift;
   VectorCopy(b->feet,exitRaised);exitRaised[2]=top->feet[2];
   if(NG_TrExplainClear(bottom->feet,top->feet,"shaft",a->id,b->id,lift)&&
      NG_TrExplainClear(top->feet,exitRaised,"horizontal_exit",a->id,b->id,lift)&&
      NG_TrExplainClear(exitRaised,b->feet,"descent",a->id,b->id,lift)){
    if(NG_TrExitSupported(top->feet,exitRaised,normal)&&NG_TrStableLanding(b->feet))break;
    ngLadderUnsupportedExit=qtrue;
    if(ngLadderDetailed)G_Printf("navgen ladder %d->%d lift%d rejected: ladder_exit_without_floor_or_contact\n",a->id,b->id,lift);
   }
  }
  if(lift>42){
   qboolean contact=qfalse;
   VectorMA(bottom->feet,-.125f,normal,launch);
   /* Body contact, not the BSP bounding-box top, determines the release
    * height. Coarse point rays otherwise allow an impossible extra 30 units
    * of climbing or reject a valid short ballistic dismount. */
   for(z=bottom->feet[2]+2.f;z<=b->feet[2]+42.f;z+=2.f){
    VectorCopy(launch,point);point[2]=z;
    if(!NG_TrClear(bottom->feet,point)||!NG_TrAirborneLadderContact(point,normal))break;
    VectorCopy(point,launch);contact=qtrue;
   }
   if(!contact||(!NG_TrLadderLowerTransfer(bottom,b,normal,launch)&&
      !NG_TrLadderLanding(launch,b,normal,ngLadderProofWorking.landing,&ngLadderProofWorking.duration,&ngLadderProofWorking.speed)&&
      !NG_TrLadderAlignedExit(bottom,b,normal,launch))){
    ngLadderFailure[3]++;
    if(ngLadderDetailed)G_Printf("navgen ladder %d->%d no supported/ballistic exit: release %.3f %.3f %.3f floor %.3f %.3f %.3f\n",a->id,b->id,launch[0],launch[1],launch[2],b->feet[0],b->feet[1],b->feet[2]);
    continue;
   }
   /* The generated release carries tce_ascent_release=1. Its runtime
    * consumes the upward Z crossing, never the lower edge of a sphere. */
   ngLadderProofWorking.ballistic=qtrue;VectorCopy(launch,ngLadderProofWorking.launch);
   VectorMA(bottom->feet,-.125f,normal,top->feet);top->feet[2]=b->feet[2]+2.f;
   VectorCopy(b->feet,exitRaised);exitRaised[2]=top->feet[2];lift=2;
  }else {VectorCopy(top->feet,ngLadderProofWorking.launch);VectorCopy(b->feet,ngLadderProofWorking.landing);}
  /* The descent target is a lip entry, not the elevated ascent release.
   * A compulsory two-unit rise blocks valid low-ceiling walk-offs. Try the
   * actual floor height first; every separate segment still sweeps the hull. */
  *reverse=qfalse;
  for(lift=0;lift<=18;lift=lift?lift+8:2){
   top->feet[2]=ngLadderProofWorking.landing[2]+lift;VectorCopy(ngLadderProofWorking.landing,exitRaised);exitRaised[2]=top->feet[2];
   if(NG_TrExplainClear(ngLadderProofWorking.landing,exitRaised,"descent_lift",a->id,b->id,lift)&&
      NG_TrExplainClear(exitRaised,top->feet,"descent_entry",a->id,b->id,lift)&&
      NG_TrExplainClear(top->feet,bottom->feet,"descent_shaft",a->id,b->id,lift)){
    *reverse=qtrue;break;
   }
  }
  if(!NG_TrLadderSeparateForwardRelease(bottom,top,normal)){ngLadderFailure[3]++;continue;}
  for(z=bottom->feet[2];z<ngLadderProofWorking.launch[2]-8.f;z+=16.f){
   VectorCopy(bottom->feet,ray);ray[2]=z;VectorScale(normal,-(ngWidth+12.f),offset);
    if(!NG_TrBodyLadderRay(&tr,ray,offset,normal)&&!NG_TrAirborneLadderContact(ray,normal))break;
   if(NG_Hurt(ray,ngStand)||(trap_PointContents(ray,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)))break;
  }
  if(z<ngLadderProofWorking.launch[2]-8.f){ngLadderFailure[4]++;continue;}
  bottom->accepted=top->accepted=qtrue;bottom->navFlags=top->navFlags=NG_NAV_CLIMB;
  bottom->reason=top->reason="generated_surface_ladder_live_pending";
  VectorScale(normal,-1.f,bottom->facing);VectorCopy(bottom->facing,top->facing);
  return qtrue;
 }
 return qfalse;
}

/* Read bounded BSP lumps from the engine search path. Asset files remain
 * untouched; host endianness and untrusted lump offsets are checked. */
static unsigned char *NG_TrBspLump(int lump,int recordSize,int maxRecords,int *count) {
 unsigned char header[144],*data;fileHandle_t file;char path[MAX_QPATH];
 int size,offset,length,left,chunk;
 Com_sprintf(path,sizeof(path),"maps/%s.bsp",level.rawmapname);
 size=trap_FS_FOpenFile(path,&file,FS_READ);*count=0;if(!file)return NULL;
 if(size<144){trap_FS_FCloseFile(file);return NULL;}
 trap_FS_Read(header,144,file);offset=(int)NG_DecodeWord(header+8+lump*8);length=(int)NG_DecodeWord(header+12+lump*8);
 if(memcmp(header,"IBSP",4)||NG_DecodeWord(header+4)!=47||offset<144||length<=0||
    offset>size||length>size-offset||length%recordSize||length/recordSize>maxRecords){
  G_Printf("navgen BSP ladder scan rejected lump %d: offset=%d bytes=%d record=%d limit=%d file=%d\n",lump,offset,length,recordSize,maxRecords,size);
  trap_FS_FCloseFile(file);return NULL;}
 data=(unsigned char *)malloc(length);if(!data){trap_FS_FCloseFile(file);return NULL;}
 left=offset-144;while(left){chunk=left>NG_INPUT?NG_INPUT:left;trap_FS_Read(ngInput,chunk,file);left-=chunk;}
 trap_FS_Read(data,length,file);trap_FS_FCloseFile(file);*count=length/recordSize;return data;
}
static float NG_TrFloat(const unsigned char *p) { union {unsigned int u;float f;} v;v.u=NG_DecodeWord(p);return v.f; }
static int NG_TrSeed(const vec3_t point) {
 vec3_t a,b;trace_t tr;ngNode n;int i,j,maxid=0;
 if(ngCount>=NG_NODES-32||ngEdgeCount>=NG_EDGES-256)return -1;
 memset(&n,0,sizeof(n));VectorCopy(point,a);VectorCopy(point,b);a[2]+=16;b[2]-=128;
 if(ngTraces>=NG_TRACE_BUDGET-20000)return -1;
 trap_Trace(&tr,a,NULL,NULL,b,ENTITYNUM_NONE,NG_MASK);ngTraces++;
 if(tr.startsolid||tr.allsolid||tr.fraction==1.f||!NG_StaticGround(tr.entityNum)||tr.plane.normal[2]<.7f)return -1;
 VectorCopy(tr.endpos,a);a[2]+=.125f;if(NG_Ground(a,n.feet,ngStand))return -1;
 for(i=0;i<ngCount;i++){
  vec3_t d;VectorSubtract(n.feet,ngNodes[i].feet,d);
  if(ngNodes[i].id>maxid)maxid=ngNodes[i].id;
  if(ngNodes[i].accepted&&!ngNodes[i].crouch&&DotProduct(d,d)<16.f*16.f&&!NG_Walk(&n,&ngNodes[i],ngStand))return i;
 }
 /* Failed ladder candidates are ordinary grounded samples and may be
  * contracted later. Only endpoints of an accepted action are protected. */
 n.id=maxid+1;n.index=-1;n.accepted=qtrue;n.reason="traversal_candidate_floor";
 VectorCopy(n.feet,n.raw);n.raw[2]+=ngOrigin;i=ngCount++;ngNodes[i]=n;
 for(j=0;j<i&&ngEdgeCount<NG_EDGES-128&&ngTraces<NG_TRACE_BUDGET-20000;j++)if(ngNodes[j].accepted){
  vec3_t d;VectorSubtract(n.feet,ngNodes[j].feet,d);
  if(DotProduct(d,d)>192.f*192.f||fabs(d[2])>32.f)continue;
  NG_Edge(i,j,0,qtrue);NG_Edge(j,i,0,qtrue);
 }
 return i;
}
static qboolean NG_TrSpokeOrdinary(int edge) {
 const ngEdge *e=&ngEdges[edge];
 return e->accepted&&e->from>=0&&e->to>=0&&e->from<ngCount&&e->to<ngCount&&
  ngNodes[e->from].accepted&&ngNodes[e->to].accepted&&
  !ngNodes[e->from].navFlags&&!ngNodes[e->to].navFlags&&
  e->reason&&!strstr(e->reason,"ladder")&&!strstr(e->reason,"jump");
}
static qboolean NG_TrSpokeDuck(int edge) {
 const ngEdge *e=&ngEdges[edge];
 return e->crouch||ngNodes[e->from].crouch||ngNodes[e->to].crouch;
}
static int QDECL NG_TrSpokeCompare(const void *pa,const void *pb) {
 int a=*(const int *)pa,b=*(const int *)pb;
 if(ngTrSpokeLength[a]<ngTrSpokeLength[b])return -1;
 if(ngTrSpokeLength[a]>ngTrSpokeLength[b])return 1;
 return a-b;
}
/* A seed discovers every nearby collision-clear connection first, including
 * separate doorway branches. Remove a fan spoke only when a bounded directed
 * route already exists through ordinary native walk edges. Each witness leg
 * must be strictly shorter than the spoke; processing shortest spokes first
 * therefore makes the witness persistent. A bridge or a standing route with
 * only a crouching alternative remains intact. Action edges are never used. */
static qboolean NG_TrSpokeWitness(int edge,float maximumRatio,qboolean shorterOnly) {
 const ngEdge *spoke=&ngEdges[edge];
 const float limit=ngTrSpokeLength[edge]*maximumRatio;
 qboolean allowDuck=NG_TrSpokeDuck(edge);int i,used=1;
 for(i=0;i<ngCount;i++){ngTrSpokeDistance[i]=1e30f;ngTrSpokeHeapPosition[i]=-1;}
 ngTrSpokeDistance[spoke->from]=0.f;ngTrSpokeHeap[0]=spoke->from;ngTrSpokeHeapPosition[spoke->from]=0;
 while(used) {
  int node=ngTrSpokeHeap[0],tail=ngTrSpokeHeap[--used],at=0,k;
  if(node==spoke->to)return qtrue;
  ngTrSpokeHeapPosition[node]=-2;
  if(used) {
   ngTrSpokeHeap[0]=tail;ngTrSpokeHeapPosition[tail]=0;
   while(at*2+1<used) {
    int child=at*2+1;
    if(child+1<used&&ngTrSpokeDistance[ngTrSpokeHeap[child+1]]<ngTrSpokeDistance[ngTrSpokeHeap[child]])child++;
    if(ngTrSpokeDistance[tail]<=ngTrSpokeDistance[ngTrSpokeHeap[child]])break;
    ngTrSpokeHeap[at]=ngTrSpokeHeap[child];ngTrSpokeHeapPosition[ngTrSpokeHeap[at]]=at;at=child;
   }
   ngTrSpokeHeap[at]=tail;ngTrSpokeHeapPosition[tail]=at;
  }
  for(k=ngTrSpokeHead[node];k>=0;k=ngTrSpokeNext[k]) {
   int to=ngEdges[k].to;float next;
   if(!ngEdges[k].accepted||(shorterOnly&&ngTrSpokeLength[k]>=ngTrSpokeLength[edge])||
      (!allowDuck&&NG_TrSpokeDuck(k))||ngTrSpokeHeapPosition[to]==-2)continue;
   next=ngTrSpokeDistance[node]+ngTrSpokeLength[k];
   if(next>limit||next>=ngTrSpokeDistance[to])continue;
   ngTrSpokeDistance[to]=next;at=ngTrSpokeHeapPosition[to];
   if(at<0){at=used++;ngTrSpokeHeap[at]=to;ngTrSpokeHeapPosition[to]=at;}
   while(at>0) {
    int parent=(at-1)/2;
    if(ngTrSpokeDistance[ngTrSpokeHeap[parent]]<=next)break;
    ngTrSpokeHeap[at]=ngTrSpokeHeap[parent];ngTrSpokeHeapPosition[ngTrSpokeHeap[at]]=at;at=parent;
   }
   ngTrSpokeHeap[at]=to;ngTrSpokeHeapPosition[to]=at;
  }
 }
 return qfalse;
}
static void NG_TrSpokeGraph(void) {
 int i,k;vec3_t delta;
 for(i=0;i<ngCount;i++)ngTrSpokeHead[i]=-1;
 for(k=0;k<ngEdgeCount;k++) {
  if(ngEdges[k].from<0||ngEdges[k].to<0||ngEdges[k].from>=ngCount||ngEdges[k].to>=ngCount)continue;
  VectorSubtract(ngNodes[ngEdges[k].from].feet,ngNodes[ngEdges[k].to].feet,delta);
  ngTrSpokeLength[k]=VectorLength(delta);
  if(!NG_TrSpokeOrdinary(k))continue;
  ngTrSpokeNext[k]=ngTrSpokeHead[ngEdges[k].from];ngTrSpokeHead[ngEdges[k].from]=k;
 }
}
/* These are final-graph promises, not multiplicative phase guarantees.
 * A later 1.5x spoke replacement must not enlarge an earlier 1.25x chord
 * witness. Check both families on the exact current ordinary walk graph. */
static qboolean NG_TrSpokePromises(qboolean diagnostics) {
 int k;
 for(k=0;k<ngEdgeCount;k++) {
  ngEdge *e=&ngEdges[k];float ratio;
  if(e->accepted||!e->reason)continue;
  if(!strcmp(e->reason,"redundant_chord_shorter_directed_witness"))ratio=1.25f;
  else if(!strcmp(e->reason,"redundant_traversal_spoke_native_walk_witness"))ratio=1.5f;
  else continue;
  if(e->from<0||e->to<0||e->from>=ngCount||e->to>=ngCount||
     !ngNodes[e->from].accepted||!ngNodes[e->to].accepted||
     ngNodes[e->from].navFlags||ngNodes[e->to].navFlags||!NG_TrSpokeWitness(k,ratio,qfalse)) {
   if(diagnostics)G_Printf("navgen final walk witness failed: edge=%d indices=%d -> %d ratio=%.2f reason=%s\n",
    k,e->from,e->to,ratio,e->reason);
   return qfalse;
  }
 }
 return qtrue;
}
static qboolean NG_TrSeedSpokesValid(qboolean diagnostics) {
 NG_TrSpokeGraph();return NG_TrSpokePromises(diagnostics);
}
static void NG_TrPruneSeedSpokes(void) {
 int i,k,count=0,removed=0,retained=0;
 NG_TrSpokeGraph();
 for(k=0;k<ngEdgeCount;k++)if(NG_TrSpokeOrdinary(k)) {
  if(k>=ngTrSeedEdgeBegin&&k<ngTrSeedEdgeEnd&&ngEdges[k].reason&&
     !strcmp(ngEdges[k].reason,"accepted"))ngTrSpokeOrder[count++]=k;
 }
 qsort(ngTrSpokeOrder,count,sizeof(ngTrSpokeOrder[0]),NG_TrSpokeCompare);
 for(i=0;i<count;i++) {
  k=ngTrSpokeOrder[i];
  if(NG_TrSpokeWitness(k,1.5f,qtrue)) {
   ngEdge *e=&ngEdges[k];const char *previousReason=e->reason;
   e->accepted=qfalse;e->reason="redundant_traversal_spoke_native_walk_witness";
   if(!NG_TrSpokePromises(qfalse)){e->accepted=qtrue;e->reason=previousReason;retained++;continue;}
   NG_TrSetAccepted(e->from,e->to,qfalse);removed++;
  }
 }
 G_Printf("navgen traversal spokes: removed=%d checked=%d retained_for_prior_promises=%d directed_walk_detour_limit=1.5 action_edges_unchanged\n",removed,count,retained);
}
static void NG_TrLadderSeeds(void) {
 static const float lateralExitOffsets[13]={-12.f,12.f,-16.f,16.f,-20.f,20.f,-24.f,24.f,0.f,-32.f,32.f,-64.f,64.f};
 unsigned char *shaders,*planes,*sides,*brushes;int ns,np,nd,nb,i,j,k;
 ngBspLadderPairCount=ngBspLadderSurfaces=0;
 /* Detailed custom maps exceed classic SDK plane/side counts (Overkill:
  * 308122 planes, 396805 sides). Bounded temporary allocations are released
  * immediately; do not silently skip valid compiled custom map geometry. */
 shaders=NG_TrBspLump(1,72,8192,&ns);planes=NG_TrBspLump(2,16,1048576,&np);
 sides=NG_TrBspLump(9,8,1048576,&nd);brushes=NG_TrBspLump(8,12,131072,&nb);
 if(!shaders||!planes||!sides||!brushes)goto ladder_lumps_done;
 for(i=0;i<nb&&ngBspLadderPairCount<128&&ngTraces<NG_TRACE_BUDGET-25000;i++){
  int first=(int)NG_DecodeWord(brushes+i*12),count=(int)NG_DecodeWord(brushes+i*12+4);
  vec3_t lo,hi; qboolean ladder=qfalse,valid=qtrue;
  if(first<0||count<4||count>128||first>nd-count)continue;
  for(j=0;j<count;j++){int shader=(int)NG_DecodeWord(sides+(first+j)*8+4);
   if(shader>=0&&shader<ns&&(NG_DecodeWord(shaders+shader*72+64)&SURF_LADDER)){ladder=qtrue;break;}}
  if(!ladder)continue;VectorSet(lo,-1e6f,-1e6f,-1e6f);VectorSet(hi,1e6f,1e6f,1e6f);
  for(j=0;j<count;j++){int p=(int)NG_DecodeWord(sides+(first+j)*8);float dist;
   if(p<0||p>=np){valid=qfalse;break;}dist=NG_TrFloat(planes+p*16+12);
   for(k=0;k<3;k++){float normal=NG_TrFloat(planes+p*16+k*4);
    if(normal==1.f&&dist<hi[k])hi[k]=dist;else if(normal==-1.f&&-dist>lo[k])lo[k]=-dist;}
  }
  for(k=0;k<3;k++)if(!NG_Finite(lo[k])||!NG_Finite(hi[k])||lo[k]>hi[k])valid=qfalse;
  if(!valid||hi[2]-lo[2]<32.f||hi[2]-lo[2]>512.f)continue;
  for(j=0;j<count&&ngBspLadderPairCount<128;j++){
   int p=(int)NG_DecodeWord(sides+(first+j)*8),shader=(int)NG_DecodeWord(sides+(first+j)*8+4),low,high,attempt,tier,side,direction;
   vec3_t normal,face,point;float dist,error;
   if(shader<0||shader>=ns||!(NG_DecodeWord(shaders+shader*72+64)&SURF_LADDER))continue;
   for(k=0;k<3;k++)normal[k]=NG_TrFloat(planes+p*16+k*4);if(fabs(normal[2])>.1f)continue;
   ngBspLadderSurfaces++;VectorAdd(lo,hi,face);VectorScale(face,.5f,face);dist=NG_TrFloat(planes+p*16+12);
   error=DotProduct(face,normal)-dist;VectorMA(face,-error,normal,face);
   VectorMA(face,ngWidth+2.f,normal,point);point[2]=lo[2]+16.f;low=NG_TrSeed(point);if(low<0)continue;
   /* Search both above the lip and above standing head height. A ray that
    * starts below the actual upper floor otherwise selects the lower storey.
    * Also sample lateral exits: map ladders can end beside a doorway/window
    * rather than in front of a free passage directly behind the ladder. */
   /* A ladder can emerge from a floor opening on its FRONT side, rather
    * than over the wall behind it (Overkill's two basement shafts do this).
    * Test both half-spaces; surface existence alone cannot choose the exit. */
   for(direction=0;direction<2&&ngBspLadderPairCount<128;direction++)
   for(tier=0;tier<2&&ngBspLadderPairCount<128;tier++)for(side=0;side<13&&ngBspLadderPairCount<128;side++){
    high=-1;
    for(attempt=0;attempt<13&&high<0;attempt++){
     VectorMA(face,(direction?-1.f:1.f)*(ngWidth+8.f+attempt*4.f),normal,point);
     point[0]-=normal[1]*lateralExitOffsets[side];point[1]+=normal[0]*lateralExitOffsets[side];
     point[2]=hi[2]+(tier?24.f:ngStand+32.f);high=NG_TrSeed(point);
     if(high>=0&&ngNodes[high].feet[2]-ngNodes[low].feet[2]<32.f)high=-1;
    }
    if(high>=0){int pair; qboolean duplicate=qfalse;
     for(pair=0;pair<ngBspLadderPairCount;pair++)if(ngBspLadderPairs[pair][0]==low&&ngBspLadderPairs[pair][1]==high){duplicate=qtrue;break;}
     if(!duplicate){ngBspLadderPairs[ngBspLadderPairCount][0]=low;ngBspLadderPairs[ngBspLadderPairCount++][1]=high;}
    }
   }
  }
 }
ladder_lumps_done:
 free(shaders);free(planes);free(sides);free(brushes);
 G_Printf("navgen BSP ladders: %d vertical surfaces, %d collision-grounded endpoint pairs\n",ngBspLadderSurfaces,ngBspLadderPairCount);
}
/* A search may discover the very same grounded landing through two action
 * fronts. Reuse an automatic ordinary floor only at the measured position;
 * never merge a CLIMB/JUMP guide, authored SourceID, tactical flag or recipe.
 * The body and both directed ground queries still have to pass now. */
static int NG_TrReuseOrdinaryFloor(const ngNode *point) {
 int i;float height=point->crouch?ngDuck:ngStand;
 if(point->authored||point->flags||point->packed||point->navFlags||point->editFlags||
    point->editRole||point->door.mode||point->count||point->name[0]||
    point->ent[0]||point->ent[1]||point->ent[2])return -1;
 for(i=0;i<ngCount;i++) {
  const ngNode *n=&ngNodes[i];vec3_t gap;
  if(!n->accepted||n->crouch!=point->crouch||n->authored||n->flags||n->packed||n->navFlags||
     n->editFlags||n->editRole||n->door.mode||n->count||n->name[0]||n->ent[0]||n->ent[1]||n->ent[2])continue;
  VectorSubtract(point->feet,n->feet,gap);if(DotProduct(gap,gap)>.000001f)continue;
  if(!NG_Walk(point,n,height)&&!NG_Walk(n,point,height))return i;
 }
 return -1;
}
static void NG_TrAddLadder(int i,int j,int *maxid) {
 ngNode bottom,top,guide[4];int node[4],k;qboolean reverse;float releaseRadius=6.f;
 if(ngCount>NG_NODES-7||ngEdgeCount>NG_EDGES-16||ngLadderProofCount>=32)return;
 ngLadderCandidates++;
 /* An earlier route already owns this mounting anchor; do not repeat its
  * expensive native shaft/landing search for each nearby upper sample. */
 for(k=0;k<ngLadderProofCount;k++)if(ngLadderProofs[k].low==i)return;
 if(NG_TrSpecialInput(j,i))return;
 if(!NG_TrLadder(i,j,&bottom,&top,&reverse)){
  if(ngLadderUnsupportedExit){
   ngEdge *edge=NULL;
   for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].from==i&&ngEdges[k].to==j){edge=&ngEdges[k];break;}
   if(!edge&&ngEdgeCount<NG_EDGES){edge=&ngEdges[ngEdgeCount++];memset(edge,0,sizeof(*edge));edge->from=i;edge->to=j;edge->added=qtrue;}
   if(edge&&!edge->accepted&&(!edge->reason||strcmp(edge->reason,"special_link_requires_review")))edge->reason="ladder_exit_without_floor_or_contact";
  }
  return;
 }
 for(k=0;k<ngCount;k++)if(ngNodes[k].navFlags&NG_NAV_CLIMB){
  vec3_t gap;VectorSubtract(ngNodes[k].feet,bottom.feet,gap);if(DotProduct(gap,gap)<16.f*16.f)return;
 }
 /* Descent is a physical turning point, not an overlapping interior
  * guidance alias. Bound its sphere against the separately grounded exit. */
 {vec3_t gap;float radius;VectorSubtract(top.feet,ngLadderProofWorking.landing,gap);
  radius=(float)floor(VectorLength(gap)+.0001f)-8.f;
  if(radius<1.f){
   G_Printf("navgen ladder commit rejected low%d high%d reason=descent_landing_radius top=(%.3f %.3f %.3f) landing=(%.3f %.3f %.3f) radius=%.3f\n",ngNodes[i].id,ngNodes[j].id,top.feet[0],top.feet[1],top.feet[2]+ngOrigin,ngLadderProofWorking.landing[0],ngLadderProofWorking.landing[1],ngLadderProofWorking.landing[2]+ngOrigin,radius);return;
  }
  top.radius=radius<8.f?radius:8.f;
 }
 /* A lower transfer can be close to the independently raised descent
  * turn. Keep their actual arrival spheres disjoint, without any audit
  * exemption. Release uses directed Z crossing, so a small XY radius does
  * not risk consuming it on the wrong side of the height plane. */
 if(ngLadderProofWorking.ballistic){
  vec3_t gap;float distance,half;VectorSubtract(top.feet,ngLadderProofWorking.launch,gap);distance=VectorLength(gap);
  if(distance<top.radius+releaseRadius+.001f){
   half=(float)floor(distance*.5f+.0001f);if(half<top.radius)top.radius=half;
   releaseRadius=(float)floor(distance-top.radius+.0001f)-1.f;if(releaseRadius>6.f)releaseRadius=6.f;
   if(top.radius<1.f||releaseRadius<1.f){
    G_Printf("navgen ladder commit rejected low%d high%d reason=release_descent_radius top=(%.3f %.3f %.3f) release=(%.3f %.3f %.3f) landing=(%.3f %.3f %.3f) distance=%.4f radii=%.1f/%.1f reverse=%d\n",ngNodes[i].id,ngNodes[j].id,top.feet[0],top.feet[1],top.feet[2]+ngOrigin,ngLadderProofWorking.launch[0],ngLadderProofWorking.launch[1],ngLadderProofWorking.launch[2]+ngOrigin,ngLadderProofWorking.landing[0],ngLadderProofWorking.landing[1],ngLadderProofWorking.landing[2]+ngOrigin,distance,top.radius,releaseRadius,reverse);return;
   }
  }
 }
 /* Keep the first supported landing as the action endpoint. The existing
  * upper anchor remains connected through separately proven walking edges. */
 {vec3_t gap;VectorSubtract(ngLadderProofWorking.landing,ngNodes[j].feet,gap);
  if(DotProduct(gap,gap)>.0625f){
   ngNode landing=ngNodes[j];int previous=j,reuse;
   landing.index=-1;landing.count=0;landing.radius=6.f;
   landing.navFlags=0;landing.name[0]=0;landing.reason="ladder_exit_anchor";
   VectorCopy(ngLadderProofWorking.landing,landing.feet);VectorCopy(landing.feet,landing.raw);landing.raw[2]+=ngOrigin;
   reuse=NG_TrReuseOrdinaryFloor(&landing);
   if(reuse>=0&&!NG_Walk(&ngNodes[previous],&ngNodes[reuse],ngStand)&&
      !NG_Walk(&ngNodes[reuse],&ngNodes[previous],ngStand))j=reuse;
   else {landing.id=++*maxid;j=ngCount++;ngNodes[j]=landing;}
   NG_TrLink(previous,j,"ladder_floor_walk_validated_live_pending");
   NG_TrLink(j,previous,"ladder_floor_walk_validated_live_pending");
  }
 }
 if(strcmp(ngNodes[i].reason,"map_entity_spawn_seed"))ngNodes[i].reason="ladder_approach_anchor";
 if(strcmp(ngNodes[j].reason,"map_entity_spawn_seed"))ngNodes[j].reason="ladder_exit_anchor";
 /* Omni's long-ladder recipe uses four CLIMB guidance targets, not a
  * conventional chain of walking edges. The contact hull and raised exit
  * above were validated separately; node2 intentionally targets the ladder
  * plane, with a radius covering the physical contact position. It is not
  * a claim that a player capsule can stand inside that plane. */
 guide[0]=bottom;guide[1]=bottom;guide[2]=top;guide[3]=top;
 VectorMA(bottom.feet,ngWidth,bottom.facing,guide[1].feet);
 VectorCopy(ngNodes[j].feet,guide[3].feet);
 guide[0].radius=8.f;guide[1].radius=ngWidth+4.f;
 guide[2].radius=top.radius;guide[3].radius=8.f;
 guide[0].reason="ladder_base_contact_live_pending";
 guide[1].reason="ladder_inside_guidance_not_standable_live_pending";
 guide[2].reason="ladder_top_clearance_live_pending";
 guide[3].reason="ladder_exit_floor_live_pending";
 for(k=0;k<4;k++){
  node[k]=ngCount++;guide[k].id=++*maxid;guide[k].index=-1;
  VectorCopy(guide[k].feet,guide[k].raw);guide[k].raw[2]+=ngOrigin;
  ngNodes[node[k]]=guide[k];
 }
 NG_TrLink(i,node[0],"ladder_approach_validated_live_pending");
 NG_TrLink(node[0],i,"ladder_exit_validated_live_pending");
 NG_TrLink(node[0],node[1],"ladder_contact_guidance_live_pending");
 NG_TrLink(node[1],node[0],"ladder_contact_guidance_live_pending");
 if(ngLadderProofWorking.ballistic){
  ngNode release=top;int ascent=ngCount++;
  release.id=++*maxid;release.index=-1;release.radius=releaseRadius;
  VectorCopy(ngLadderProofWorking.launch,release.feet);VectorCopy(release.feet,release.raw);release.raw[2]+=ngOrigin;
  release.reason="ladder_ascent_release_contact_live_pending";ngNodes[ascent]=release;
  ngLadderProofWorking.ascentRelease=ascent;
  if(ngLadderProofWorking.aligned){
   ngNode alignment=release;int align=ngCount++;
   alignment.id=++*maxid;alignment.radius=1.f;
   VectorCopy(ngLadderProofWorking.alignment,alignment.feet);VectorCopy(alignment.feet,alignment.raw);alignment.raw[2]+=ngOrigin;
   alignment.reason="ladder_upper_alignment_contact_live_pending";ngNodes[align]=alignment;
   ngLadderProofWorking.ascentAlignment=align;
   NG_TrLink(node[0],align,"ladder_aligned_ascent_live_pending");
   NG_TrLink(align,ascent,"ladder_release_column_live_pending");ngLadderLinks++;
  }else NG_TrLink(node[0],ascent,"ladder_directed_ascent_live_pending");
  NG_TrLink(ascent,node[3],"ladder_ballistic_dismount_live_pending");ngLadderLinks++;
 }else NG_TrLink(node[0],node[3],"ladder_directed_ascent_live_pending");
 /* The raised descent guide may be airborne. Its optional return to the
  * exit floor is not proved by the separate ascent/dismount recipe. Keep
  * this extra direction only after a fresh complete capsule-supported walk;
  * the ascent route and Exit->Top->Interior descent remain independent. */
 ngLadderProofWorking.liftedExit=qfalse;
 if(!NG_Walk(&ngNodes[node[2]],&ngNodes[node[3]],ngStand)) {
  NG_TrLink(node[2],node[3],"ladder_lifted_exit_validated_live_pending");ngLadderLinks++;
  ngLadderProofWorking.liftedExit=qtrue;
 }
 NG_TrLink(node[3],j,"ladder_exit_floor_live_pending");
 NG_TrLink(j,node[3],"ladder_exit_floor_live_pending");ngLadderLinks+=7;
 if(reverse){
  NG_TrLink(node[3],node[2],"ladder_approach_validated_live_pending");
  NG_TrLink(node[2],node[1],"ladder_directed_descent_live_pending");ngLadderLinks+=2;
 }
 ngLadderProofWorking.low=i;ngLadderProofWorking.high=j;ngLadderProofWorking.reverse=reverse;
 for(k=0;k<4;k++)ngLadderProofWorking.node[k]=node[k];
 ngLadderProofs[ngLadderProofCount++]=ngLadderProofWorking;
 G_Printf("navgen ladder accepted low%d high%d guides%d/%d/%d/%d reverse%d dismount%s release %.3f %.3f %.3f speed%.3f time%.3f (live pending)\n",
  ngNodes[i].id,ngNodes[j].id,guide[0].id,guide[1].id,guide[2].id,guide[3].id,reverse,
  ngLadderProofWorking.ballistic?"ballistic":"supported",ngLadderProofWorking.launch[0],ngLadderProofWorking.launch[1],ngLadderProofWorking.launch[2]+ngOrigin,ngLadderProofWorking.speed,ngLadderProofWorking.duration);
}

/* A coarse front can be too far from a low step for its original endpoints
 * to pass the arc-speed filter. Sample local ground BEFORE testing arcs,
 * rather than requiring the unadjusted pair to jump first. This bounded
 * spatial-import pass commits only paired, fully proved return routes. */
static qboolean NG_TrLaunchSeparated(const ngNode *point) {
 int n;
 for(n=0;n<ngCount;n++)if(ngNodes[n].accepted) {
  vec3_t gap;float minimum=ngNodes[n].radius+8.f;
  if(minimum<20.f)minimum=20.f;
  VectorSubtract(point->feet,ngNodes[n].feet,gap);
  if(DotProduct(gap,gap)<minimum*minimum)return qfalse;
 }
 return qtrue;
}
static qboolean NG_TrFrontSample(int base,const vec3_t direction,const vec3_t tangent,float along,float side,int limit,ngFrontJumpCase *entry,ngNode *sample) {
 vec3_t trial;
 if(ngTraces>=limit-512)return qfalse;
 *sample=ngNodes[base];entry->samples++;
 VectorMA(sample->feet,along,direction,trial);VectorMA(trial,side,tangent,trial);
 /* A front belongs to this source floor. A wide foot box finding the next
  * platform, or a rim whose centre/footprint is unsupported, is not an
  * approach to that source. Both directions still use the real body walk. */
 if(NG_Ground(trial,sample->feet,ngStand)||fabs(sample->feet[2]-ngNodes[base].feet[2])>1.f||
    !NG_TrStableLanding(sample->feet))return qfalse;
 if(NG_Walk(&ngNodes[base],sample,ngStand)||NG_Walk(sample,&ngNodes[base],ngStand))return qfalse;
 sample->crouch=qfalse;entry->walkSamples++;return qtrue;
}
static int NG_TrFrontStrip(int base,const vec3_t direction,float side,float farBack,ngNode points[3],int limit,ngFrontJumpCase *entry) {
 static const float along[10]={96,80,64,48,32,16,0,-16,-32,-48};
 int a,back,count=0;vec3_t tangent;
 VectorSet(tangent,-direction[1],direction[0],0);
 /* Seven bounded approach strips, at most three output samples per strip.
  * Refine the first safe front against the preceding unsafe 16-unit span;
  * the two inward samples give the independently checked drop a way back.
  * No graph, arc, trace or proof capacity is increased by this search. */
 {
  ngNode sample;float front=0.f;int found=-1;
  for(a=0;a<10;a++) {
   if(ngTraces>=limit-512)return count;
   if(NG_TrFrontSample(base,direction,tangent,along[a],side,limit,entry,&sample)){front=along[a];found=a;break;}
  }
  if(found<0)return 0;
  if(found>0) {
   float upper=along[found-1];int refine;
   for(refine=0;refine<4;refine++) {
    ngNode trial;float middle=(front+upper)*.5f;
    if(ngTraces>=limit-512)return count;
    if(NG_TrFrontSample(base,direction,tangent,middle,side,limit,entry,&trial)){front=middle;sample=trial;}
    else upper=middle;
   }
  }
  points[count++]=sample;
  for(back=1;back<=2;back++) {
   float inward=front-(back==1?16.f:farBack);
   if(inward<-48.f)continue;
   if(ngTraces>=limit-512)return count;
   if(NG_TrFrontSample(base,direction,tangent,inward,side,limit,entry,&sample))points[count++]=sample;
  }
 }
 return count;
}
static int NG_TrFrontSamples(int base,const vec3_t direction,ngNode points[21],int limit,ngFrontJumpCase *entry) {
 static const float side[7]={0,16,-16,32,-32,48,-48};int s,count=0;
 for(s=0;s<7&&ngTraces<limit-512;s++)count+=NG_TrFrontStrip(base,direction,side[s],32.f,points+count,limit,entry);
 return count;
}
/* Cheap ordering evidence only. A full-width short capsule crosses the torso
 * band, above an ordinary step but below a door lintel. A wall here demotes a
 * source pair; it NEVER excludes it or certifies a traversable jump. */
static qboolean NG_TrFrontPortal(int low,int high,int limit,int *lane) {
 static const float side[7]={0,16,-16,32,-32,48,-48};
 vec3_t direction,tangent,a,b;float height=2.f*ngWidth+1.f,lift=(ngStand-height)*.5f;int s;
 VectorSubtract(ngNodes[high].feet,ngNodes[low].feet,direction);direction[2]=0;VectorNormalize(direction);
 VectorSet(tangent,-direction[1],direction[0],0);*lane=0;
 for(s=0;s<7&&ngTraces<limit-512;s++) {
  trace_t tr;VectorMA(ngNodes[low].feet,side[s],tangent,a);VectorMA(ngNodes[high].feet,side[s],tangent,b);
  a[2]+=lift;b[2]+=lift;
  if(NG_Trace(&tr,a,b,height)&&!tr.startsolid&&!tr.allsolid&&tr.fraction==1.f){*lane=s;return qtrue;}
 }
 return qfalse;
}
static int NG_TrFrontPairs(const ngNode low[21],int lc,const ngNode high[21],int hc,const qboolean lowLaunch[21],const ngNode *lowBase,const ngNode *highBase,ngFrontArcPair pairs[21*21]) {
 int a,b,count=0;
 /* Both upper source-floor walks are already proved. Rank their two legs
  * together with the jump distance: a landing right on the rim can be a
  * shorter arc but an unnecessarily long continuation back into the room.
  * This orders candidates only; no arc or return proof is waived. */
 for(a=0;a<lc;a++)if(lowLaunch[a])for(b=0;b<hc;b++) {
  vec3_t gap;float cost;int at,move;
  VectorSubtract(high[b].feet,low[a].feet,gap);cost=VectorLength(gap);
  VectorSubtract(high[b].feet,highBase->feet,gap);cost+=2.f*VectorLength(gap);
  for(at=0;at<count&&pairs[at].cost<=cost;at++);
  for(move=count;move>at;move--)pairs[move]=pairs[move-1];
  pairs[at].low=a;pairs[at].high=b;pairs[at].cost=cost;count++;
 }
 /* Cost order within each transverse strip, round-robin across strips.
  * Seven nearby diagonals through the same obstacle are not seven distinct
  * approaches. No candidate is removed, and all retain their native proof. */
 {
  ngFrontArcPair ordered[21*21];unsigned char selected[21*21]={0};
  unsigned int strips=0;vec3_t direction,tangent;int out;
  VectorSubtract(highBase->feet,lowBase->feet,direction);direction[2]=0;VectorNormalize(direction);
  VectorSet(tangent,-direction[1],direction[0],0);
  for(out=0;out<count;out++) {
   int at=-1,strip=0,pass;
   for(pass=0;pass<2&&at<0;pass++) {
    for(a=0;a<count;a++)if(!selected[a]) {
     vec3_t gap;float lateral;
     VectorSubtract(low[pairs[a].low].feet,lowBase->feet,gap);lateral=DotProduct(gap,tangent)/16.f;
     strip=(int)floor(lateral+.5f)+3;if(strip<0)strip=0;if(strip>6)strip=6;
     if(!(strips&(1u<<strip))){at=a;break;}
    }
    if(at<0)strips=0;
   }
   if(at<0)break;
   selected[at]=1;strips|=1u<<strip;ordered[out]=pairs[at];
  }
  memcpy(pairs,ordered,(size_t)count*sizeof(*pairs));
 }
 return count;
}
static int NG_TrFrontNode(const ngNode *point,int base,qboolean jump,int *maxid) {
 ngNode node=*point;vec3_t gap;int index;
 VectorSubtract(point->feet,ngNodes[base].feet,gap);
 if(!jump&&DotProduct(gap,gap)<.01f)return base;
 if(!jump&&(index=NG_TrReuseOrdinaryFloor(point))>=0)return index;
 node.id=++*maxid;node.index=-1;node.count=0;node.name[0]=0;
 node.navFlags=jump?NG_NAV_JUMP:0;node.radius=8.f;node.requestedRadius=0;
 node.reason=jump?"generated_jump_takeoff_live_pending":"jump_landing_anchor";
 node.topologyProtected=qtrue;VectorCopy(node.feet,node.raw);node.raw[2]+=ngOrigin;
 index=ngCount++;ngNodes[index]=node;return index;
}
static int NG_TrStrongComponents(int *groups) {
 unsigned char active[NG_NODES];tce_navGraphEdge *edges;int i,count=0,result;
 for(i=0;i<ngCount;i++)active[i]=(unsigned char)ngNodes[i].accepted;
 edges=(tce_navGraphEdge *)malloc((size_t)ngEdgeCount*sizeof(*edges));
 if(ngEdgeCount&&!edges)return -1;
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted) {
  edges[count].from=ngEdges[i].from;edges[count++].to=ngEdges[i].to;
 }
 result=TCE_NavStrongComponents(ngCount,count,active,edges,groups);free(edges);return result;
}
static qboolean NG_TrFrontAdvance(ngFrontState *state,ngFrontJumpCase *entry,int limit,int quantum) {
 int begin=ngTraces,initialArcs=entry->arcChecks;
 while(!state->done&&ngTraces<limit-512&&ngFrontJumpArcChecks<NG_FRONT_JUMP_ARCS&&entry->arcChecks-initialArcs<quantum) {
  if(state->pendingArc<0) {
   ngFrontArcPair *pair;int before,kind;ngJumpProof proof;
   if(state->nextArc>=state->arcCount){state->done=qtrue;break;}
   pair=&state->arcs[state->nextArc++];before=ngTraces;entry->upChecks++;
   memset(&proof,0,sizeof(proof));kind=NG_TrJump(&state->low[pair->low],&state->high[pair->high])?1:-1;
   if(kind<0&&!state->runningOnly&&NG_TrStandingJump(&state->low[pair->low],&state->high[pair->high],limit,&proof))kind=2;
   entry->upFailures[ngJumpArcFailure]++;NG_TrFrontAccountArc(entry,before);
   if(kind<0)continue;
   entry->upAccepted++;entry->reason="outward_arc_only_no_proved_return";
   state->pendingArc=state->nextArc-1;state->nextDrop=0;state->upKind=kind;state->jump=proof;
   state->ua=pair->low;state->ub=pair->high;
   if(entry->arcChecks-initialArcs>=quantum)break;
  }
  while(state->nextDrop<state->hc*state->lc&&ngTraces<limit-512&&ngFrontJumpArcChecks<NG_FRONT_JUMP_ARCS&&entry->arcChecks-initialArcs<quantum) {
   int at=state->nextDrop++,c=at/state->lc,d=at%state->lc,before,outcome;vec3_t gap;ngDropProof proof;
   if(!state->highLaunch[c])continue;
   VectorSubtract(state->high[c].feet,state->high[state->ub].feet,gap);
   if(DotProduct(gap,gap)>.01f&&DotProduct(gap,gap)<10.f*10.f)continue;
   VectorSubtract(state->low[state->ua].feet,state->low[d].feet,gap);
   if(DotProduct(gap,gap)<20.f*20.f||state->down[c][d]<0)continue;
   before=ngTraces;entry->dropChecks++;
   if(NG_TrWalkDrop(&state->high[c],&state->low[d],limit,&proof)) {
    NG_TrFrontAccountArc(entry,before);entry->dropReason=ngDropFailure;entry->dropAccepted++;entry->dropFailures[0]++;
    state->drop=proof;state->da=c;state->db=d;state->found=state->done=qtrue;break;
   }
   state->down[c][d]=-1;NG_TrFrontAccountArc(entry,before);entry->dropReason=ngDropFailure;
   for(outcome=0;outcome<NG_DROP_FAILURES;outcome++)if(!strcmp(ngDropFailure,ngDropFailureNames[outcome]))break;
   if(outcome<NG_DROP_FAILURES) {
    entry->dropFailures[outcome]++;
    if(outcome>entry->dropExampleFailure) {
     entry->dropExampleFailure=outcome;VectorCopy(state->high[c].feet,entry->dropExampleFrom);
     VectorCopy(state->low[d].feet,entry->dropExampleTo);VectorCopy(ngDropFailurePoint,entry->dropExampleHit);
    }
   }
  }
  if(state->nextDrop>=state->hc*state->lc)state->pendingArc=-1;
 }
 entry->traces+=ngTraces-begin;
 if(!state->found&&ngTraces>=limit-512)entry->reason="trace_budget_before_complete_front_search";
 else if(!state->found&&ngFrontJumpArcChecks>=NG_FRONT_JUMP_ARCS)entry->reason="arc_budget_before_complete_front_search";
 return state->found;
}
static void NG_TrFrontExpand(ngFrontState *state,ngFrontJumpCase *entry,int limit) {
 int i=entry->low,j=entry->high,a,start=ngTraces;vec3_t direction;qboolean lowLaunch[21];
 memset(state,0,sizeof(*state));state->pendingArc=-1;
 VectorSubtract(ngNodes[j].feet,ngNodes[i].feet,direction);direction[2]=0;VectorNormalize(direction);
 state->lc=NG_TrFrontSamples(i,direction,state->low,limit,entry);VectorScale(direction,-1,direction);
 state->hc=NG_TrFrontSamples(j,direction,state->high,limit,entry);
 entry->lowSamples=state->lc;entry->highSamples=state->hc;entry->lowLaunches=entry->highLaunches=0;
 for(a=0;a<state->lc;a++){VectorCopy(state->low[a].feet,entry->lowPoints[a]);lowLaunch[a]=NG_TrLaunchSeparated(&state->low[a]);entry->lowLaunches+=lowLaunch[a]!=0;}
 for(a=0;a<state->hc;a++){VectorCopy(state->high[a].feet,entry->highPoints[a]);state->highLaunch[a]=NG_TrLaunchSeparated(&state->high[a]);entry->highLaunches+=state->highLaunch[a]!=0;}
 if(!state->lc||!state->hc){entry->reason="no_bidirectional_ground_approach";state->done=qtrue;}
 state->arcCount=NG_TrFrontPairs(state->low,state->lc,state->high,state->hc,lowLaunch,&ngNodes[i],&ngNodes[j],state->arcs);
 entry->traces+=ngTraces-start;
}
static void NG_TrFrontCommit(ngFrontState *state,ngFrontJumpCase *entry,int *maxid) {
 int i=entry->low,j=entry->high;
 int upLaunch=NG_TrFrontNode(&state->low[state->ua],i,qtrue,maxid),upLand=NG_TrFrontNode(&state->high[state->ub],j,qfalse,maxid);
 vec3_t shared;int downLaunch,downLand;ngDropProof *drop=&ngDropProofs[ngDropProofCount++];
 VectorSubtract(state->high[state->da].feet,state->high[state->ub].feet,shared);
 downLaunch=DotProduct(shared,shared)<.01f?upLand:NG_TrFrontNode(&state->high[state->da],j,qfalse,maxid);
 downLand=NG_TrFrontNode(&state->low[state->db],i,qfalse,maxid);
 ngNodes[downLaunch].reason="generated_walk_drop_takeoff_live_pending";ngNodes[downLaunch].radius=2.f;
 if(strcmp(ngNodes[downLand].reason,"map_entity_spawn_seed"))ngNodes[downLand].reason="walk_drop_landing_anchor";
 ngNodes[downLand].radius=6.f;
 *drop=state->drop;drop->approach=j;drop->launch=downLaunch;drop->landing=downLand;drop->pairedJumpLaunch=upLaunch;
 NG_TrLink(i,upLaunch,"jump_takeoff_validated_live_pending");NG_TrLink(upLaunch,upLand,"jump_hull_arc_validated_live_pending");
 if(j!=downLaunch)NG_TrLink(j,downLaunch,"walk_drop_approach_validated_live_pending");
 NG_TrLink(downLaunch,downLand,"walk_drop_hull_validated_live_pending");
 if(upLand!=j){NG_TrLink(upLand,j,"external_native_validated_walk");NG_TrLink(j,upLand,"external_native_validated_walk");}
 if(downLand!=i){NG_TrLink(downLand,i,"external_native_validated_walk");NG_TrLink(i,downLand,"external_native_validated_walk");}
 if(state->upKind==2) {
  ngJumpProofs[ngJumpProofCount]=state->jump;ngNodes[upLaunch].radius=2.f;
  ngNodes[upLaunch].reason="generated_standing_jump_takeoff_live_pending";
 }
 ngJumpProofs[ngJumpProofCount].approach=i;ngJumpProofs[ngJumpProofCount].launch=upLaunch;ngJumpProofs[ngJumpProofCount++].landing=upLand;
 ngJumpLinks++;ngFrontJumpConnected++;entry->reason="paired_native_jump_and_walk_drop_live_pending";NG_SpawnReachability();
}
static void NG_TrFrontJumps(int baseCount,int *maxid) {
 struct ngFrontPair {int low,high,priority,lane; qboolean clear;float distance;} pairs[64];
 int i,j,k,count=0,begin=ngTraces,limit=begin+NG_FRONT_JUMP_TRACES;
 static int directedGroups[NG_NODES];
 static unsigned char missionGroups[NG_NODES];
 float gravity=NG_TrGravity(),rise;
 if(!ngExternalSpatialImport){ngFrontJumpStop="spatial_import_only";return;}
 if(gravity<100.f||gravity>2000.f){ngFrontJumpStop="unsupported_gravity";return;}
 rise=TCE_NavStandingImpulse(g_newbbox.integer)*TCE_NavStandingImpulse(g_newbbox.integer)/(2.f*gravity)-2.f;
 if(limit>NG_TRACE_BUDGET-10000)limit=NG_TRACE_BUDGET-10000;
 ngFrontJumpStop="eligible_fronts_exhausted";
 if(NG_TrStrongComponents(directedGroups)<0){ngFrontJumpStop="directed_components_unavailable";return;}
 memset(missionGroups,0,sizeof(missionGroups));
 for(i=0;i<baseCount;i++)if(ngNodes[i].accepted&&ngNodes[i].topologyProtected&&directedGroups[i]>=0)
  missionGroups[directedGroups[i]]=1;
 for(i=0;i<baseCount;i++)if(ngNodes[i].accepted&&!ngNodes[i].crouch&&!ngNodes[i].navFlags)
  for(j=0;j<baseCount;j++)if(i!=j&&ngNodes[j].accepted&&!ngNodes[j].crouch&&!ngNodes[j].navFlags&&directedGroups[i]!=directedGroups[j]) {
   vec3_t d;float distance;int at,priority;
   VectorSubtract(ngNodes[j].feet,ngNodes[i].feet,d);
   if(d[2]<=18.25f||d[2]>rise||NG_TrAccepted(i,j)||NG_TrSpecialInput(i,j)||NG_TrSpecialInput(j,i))continue;
   distance=d[0]*d[0]+d[1]*d[1];if(distance<40.f*40.f||distance>256.f*256.f)continue;
   /* Bound selection by mission reachability before proximity: disconnected
    * protected contact floors need their real jump/return proof before short
    * decorative fronts consume the same finite capsule/arc budget. */
   priority=ngSpawnReachable[i]&&!ngSpawnReachable[j]?(missionGroups[directedGroups[j]]?0:1):
            !ngSpawnReachable[i]&&ngSpawnReachable[j]?1:
            ngSpawnReachable[i]&&ngSpawnReachable[j]?2:3;
   for(at=0;at<count&&(pairs[at].priority<priority||
       (pairs[at].priority==priority&&pairs[at].distance<=distance));at++);
   if(at>=64)continue;
   if(count<64)count++;
   for(k=count-1;k>at;k--)pairs[k]=pairs[k-1];
   pairs[at].low=i;pairs[at].high=j;pairs[at].priority=priority;pairs[at].distance=distance;pairs[at].clear=qfalse;pairs[at].lane=0;
  }
 /* A mission's entrance can be an ordinary point on the other side of its
  * room. Rank the already bounded source pairs by clear portal evidence,
  * retaining all blocked pairs and every exact native acceptance check. */
 for(k=0;k<count&&ngTraces<limit-512;k++)pairs[k].clear=NG_TrFrontPortal(pairs[k].low,pairs[k].high,limit,&pairs[k].lane);
 for(k=1;k<count;k++) {
  struct ngFrontPair pair=pairs[k];int at=k;
  while(at>0&&(pairs[at-1].priority>pair.priority||
    (pairs[at-1].priority==pair.priority&&pairs[at-1].clear<pair.clear))) {pairs[at]=pairs[at-1];at--;}
  pairs[at]=pair;
 }
 {
  ngFrontState *states=(ngFrontState *)calloc(NG_FRONT_JUMP_CASES,sizeof(*states));
  int firstCase=ngFrontJumpCaseCount,round,progress;
  if(!states){ngFrontJumpStop="front_state_memory";return;}
  /* Give each distinct front a finite first quantum before resuming any
   * blocked neighbour. Samples/failed returns stay cached between rounds. */
  for(k=0;k<count&&ngFrontJumpCaseCount<NG_FRONT_JUMP_CASES;k++) {
   ngFrontState *state;ngFrontJumpCase *entry;qboolean lowLaunch[21],separated=qtrue;
   int a,start=ngTraces;vec3_t direction;
   i=pairs[k].low;j=pairs[k].high;
   if(directedGroups[i]==directedGroups[j])continue;
   if(ngTraces>=limit-512){ngFrontJumpStop="trace_budget";break;}
   if(ngFrontJumpArcChecks>=NG_FRONT_JUMP_ARCS){ngFrontJumpStop="arc_budget";break;}
   if(ngCount>=NG_NODES-4||ngEdgeCount>=NG_EDGES-8||ngJumpProofCount>=64||ngDropProofCount>=32){ngFrontJumpStop="graph_budget";break;}
   for(a=0;a<ngFrontJumpCaseCount;a++) {
    vec3_t p,q;VectorSubtract(ngNodes[i].feet,ngNodes[ngFrontJumpCases[a].low].feet,p);
    VectorSubtract(ngNodes[j].feet,ngNodes[ngFrontJumpCases[a].high].feet,q);
    if(DotProduct(p,p)<64.f*64.f&&DotProduct(q,q)<64.f*64.f){separated=qfalse;break;}
   }
   if(!separated)continue;
   state=&states[ngFrontJumpCaseCount-firstCase];state->pendingArc=-1;
   entry=&ngFrontJumpCases[ngFrontJumpCaseCount++];entry->low=i;entry->high=j;
   entry->portalLane=pairs[k].lane;entry->portalClear=pairs[k].clear;
   entry->reason="no_paired_native_jump_and_drop";entry->dropReason="not_tested";
   if(!entry->portalClear)continue;
   /* Try the one body-clear approach lane before doing seven complete
    * floor/return-walk scans or any stopped 27-sample jump. A failure still
    * receives the complete original search in the second pass below. */
   entry->portalFirst=qtrue;state->runningOnly=qtrue;
   VectorSubtract(ngNodes[j].feet,ngNodes[i].feet,direction);direction[2]=0;VectorNormalize(direction);
   {static const float side[7]={0,16,-16,32,-32,48,-48};float lane=side[entry->portalLane];
    /* A middle launch needs a distinct return point, not two neighbours
     * only16 units away. Keep three fully checked points in this strip. */
    state->lc=NG_TrFrontStrip(i,direction,lane,48.f,state->low,limit,entry);VectorScale(direction,-1,direction);
    state->hc=NG_TrFrontStrip(j,direction,-lane,32.f,state->high,limit,entry);
   }
   entry->lowSamples=state->lc;entry->highSamples=state->hc;
   for(a=0;a<state->lc;a++){VectorCopy(state->low[a].feet,entry->lowPoints[a]);lowLaunch[a]=NG_TrLaunchSeparated(&state->low[a]);entry->lowLaunches+=lowLaunch[a]!=0;}
   for(a=0;a<state->hc;a++){VectorCopy(state->high[a].feet,entry->highPoints[a]);state->highLaunch[a]=NG_TrLaunchSeparated(&state->high[a]);entry->highLaunches+=state->highLaunch[a]!=0;}
   if(!state->lc||!state->hc){entry->reason="no_bidirectional_ground_approach";state->done=qtrue;}
   state->arcCount=NG_TrFrontPairs(state->low,state->lc,state->high,state->hc,lowLaunch,&ngNodes[i],&ngNodes[j],state->arcs);
   entry->traces=ngTraces-start;
   if(NG_TrFrontAdvance(state,entry,limit,NG_FRONT_JUMP_QUANTUM)) {
    NG_TrFrontCommit(state,entry,maxid);
    if(NG_TrStrongComponents(directedGroups)<0){ngFrontJumpStop="directed_components_unavailable";break;}
   }
  }
  /* No portal ordering result is a rejection. Restore all seven strips and
   * the running/standing search for every front not already connected. */
  for(k=firstCase;k<ngFrontJumpCaseCount&&ngTraces<limit-512&&ngFrontJumpArcChecks<NG_FRONT_JUMP_ARCS&&
      strcmp(ngFrontJumpStop,"graph_budget")&&strcmp(ngFrontJumpStop,"directed_components_unavailable");k++) {
   ngFrontJumpCase *entry=&ngFrontJumpCases[k];ngFrontState *state=&states[k-firstCase];
   if(state->found)continue;i=entry->low;j=entry->high;
   if(ngCount>=NG_NODES-4||ngEdgeCount>=NG_EDGES-8||ngJumpProofCount>=64||ngDropProofCount>=32){ngFrontJumpStop="graph_budget";break;}
   if(directedGroups[i]==directedGroups[j]){state->done=qtrue;entry->reason="already_connected_by_other_native_front";continue;}
   /* Retain an exact accepted upward receipt until its independent return
    * search finishes. Never throw it away merely at a scheduling quantum. */
   if(state->runningOnly&&!state->done) {
    if(NG_TrFrontAdvance(state,entry,limit,NG_FRONT_JUMP_QUANTUM)) {
     NG_TrFrontCommit(state,entry,maxid);
     if(NG_TrStrongComponents(directedGroups)<0){ngFrontJumpStop="directed_components_unavailable";break;}
     continue;
    }
    if(!state->done)continue;
   }
   NG_TrFrontExpand(state,entry,limit);
   if(NG_TrFrontAdvance(state,entry,limit,NG_FRONT_JUMP_QUANTUM)) {
    NG_TrFrontCommit(state,entry,maxid);
    if(NG_TrStrongComponents(directedGroups)<0){ngFrontJumpStop="directed_components_unavailable";break;}
   }
  }
  /* All candidates remain scheduled. Continue round-robin with the same
   * global budgets; a partial upward proof cannot mutate the graph. */
  for(round=0;round<21*21*2&&ngTraces<limit-512&&ngFrontJumpArcChecks<NG_FRONT_JUMP_ARCS&&
      strcmp(ngFrontJumpStop,"graph_budget")&&strcmp(ngFrontJumpStop,"directed_components_unavailable");round++) {
   progress=0;
   for(k=firstCase;k<ngFrontJumpCaseCount&&ngTraces<limit-512&&ngFrontJumpArcChecks<NG_FRONT_JUMP_ARCS;k++) {
    ngFrontState *state=&states[k-firstCase];ngFrontJumpCase *entry=&ngFrontJumpCases[k];
    if(directedGroups[entry->low]==directedGroups[entry->high]){state->done=qtrue;entry->reason="already_connected_by_other_native_front";continue;}
    if(ngCount>=NG_NODES-4||ngEdgeCount>=NG_EDGES-8||ngJumpProofCount>=64||ngDropProofCount>=32){ngFrontJumpStop="graph_budget";break;}
    if(state->done&&state->runningOnly&&!state->found)NG_TrFrontExpand(state,entry,limit);
    if(state->done)continue;
    progress=1;
    if(NG_TrFrontAdvance(state,entry,limit,NG_FRONT_JUMP_QUANTUM)) {
     NG_TrFrontCommit(state,entry,maxid);
     if(NG_TrStrongComponents(directedGroups)<0){ngFrontJumpStop="directed_components_unavailable";break;}
    }
   }
   if(!progress||!strcmp(ngFrontJumpStop,"graph_budget")||!strcmp(ngFrontJumpStop,"directed_components_unavailable"))break;
  }
  for(k=firstCase;k<ngFrontJumpCaseCount;k++)if(!states[k-firstCase].done) {
   if(ngTraces>=limit-512)ngFrontJumpCases[k].reason="trace_budget_before_complete_front_search";
   else if(ngFrontJumpArcChecks>=NG_FRONT_JUMP_ARCS)ngFrontJumpCases[k].reason="arc_budget_before_complete_front_search";
  }
  free(states);
 }
 ngFrontJumpTraces=ngTraces-begin;
 ngJumpTraversalTraces+=ngFrontJumpTraces;
 if(ngTraces>=limit-512)ngFrontJumpStop="trace_budget";
 else if(ngFrontJumpArcChecks>=NG_FRONT_JUMP_ARCS)ngFrontJumpStop="arc_budget";
 else if(ngFrontJumpCaseCount==NG_FRONT_JUMP_CASES)ngFrontJumpStop="front_budget";
 G_Printf("navgen low-step fronts: pairs=%d checked=%d paired_routes=%d traces=%d stop=%s (native geometry only)\n",count,ngFrontJumpCaseCount,ngFrontJumpConnected,ngFrontJumpTraces,ngFrontJumpStop);
}

/* A valid landing hull is not a route out of the landing. In particular an
 * isolated ordinary floor sample must not become a reachable sink solely
 * because an incoming jump was added. Keep that sample and its metadata for
 * the normal reachability/quarantine report; reject only this new action.
 * Paired jump/drop fronts commit their proved return separately above. */
static qboolean NG_TrCommitSingleJump(int approach,int landing,const ngNode *point,int *maxid,const ngJumpProof *standing) {
 int k,takeoff;ngNode launch;ngEdge *rejected=NULL;qboolean hasExit=qfalse;
 if(approach<0||approach>=ngCount||landing<0||landing>=ngCount||approach==landing||
    !ngNodes[approach].accepted||!ngNodes[landing].accepted)return qfalse;
 for(k=0;k<ngEdgeCount;k++) {
  const ngEdge *e=&ngEdges[k];
  if(e->from==landing&&e->accepted&&e->to>=0&&e->to<ngCount&&e->to!=landing&&
     ngNodes[e->to].accepted)hasExit=qtrue;
  if(e->from==approach&&e->to==landing)rejected=&ngEdges[k];
 }
 if(!hasExit) {
  if(!rejected&&ngEdgeCount<NG_EDGES) {
   rejected=&ngEdges[ngEdgeCount++];memset(rejected,0,sizeof(*rejected));
   rejected->from=approach;rejected->to=landing;rejected->added=qtrue;
  }
  if(rejected&&!rejected->accepted&&(!rejected->reason||strcmp(rejected->reason,"special_link_requires_review")))
   rejected->reason="jump_landing_without_validated_exit";
  return qfalse;
 }
 if(ngCount>=NG_NODES||ngEdgeCount>NG_EDGES-2||ngJumpProofCount>=64)return qfalse;
 takeoff=ngCount++;launch=*point;
 launch.id=++*maxid;launch.index=-1;launch.count=0;launch.navFlags=NG_NAV_JUMP;
 launch.radius=8.f;VectorCopy(launch.feet,launch.raw);launch.raw[2]+=ngOrigin;
 launch.name[0]=0;launch.reason="generated_jump_takeoff_live_pending";
 if(standing&&standing->standing){launch.radius=2.f;launch.reason="generated_standing_jump_takeoff_live_pending";}
 ngNodes[takeoff]=launch;
 if(strcmp(ngNodes[landing].reason,"map_entity_spawn_seed"))ngNodes[landing].reason="jump_landing_anchor";
 NG_TrLink(approach,takeoff,"jump_takeoff_validated_live_pending");
 NG_TrLink(takeoff,landing,"jump_hull_arc_validated_live_pending");ngJumpLinks++;
 if(standing&&standing->standing)ngJumpProofs[ngJumpProofCount]=*standing;
 ngJumpProofs[ngJumpProofCount].approach=approach;ngJumpProofs[ngJumpProofCount].launch=takeoff;
 ngJumpProofs[ngJumpProofCount++].landing=landing;
 return qtrue;
}

#include "tce_navgen_freeclimb.inc"
static void NG_Traversal(void) {
 int i,j,pass,baseCount,maxid=0,budgetStart=ngTraces;
 ngJumpCandidates=ngJumpLinks=ngLadderCandidates=ngLadderLinks=0;
 ngJumpPrefilterChecks=ngJumpPrefilterRejected=0;
 ngBspTraversalTraces=ngGenericLadderTraces=ngJumpTraversalTraces=0;
 ngLadderProofCount=0;memset(ngLadderProofs,0,sizeof(ngLadderProofs));
 ngJumpProofCount=0;memset(ngJumpProofs,0,sizeof(ngJumpProofs));
 ngDropProofCount=ngDropTests=ngDropAccepted=0;memset(ngDropProofs,0,sizeof(ngDropProofs));
 ngFreeClimbProofCount=0;memset(ngFreeClimbProofs,0,sizeof(ngFreeClimbProofs));
 ngFrontJumpCaseCount=ngFrontJumpTraces=ngFrontJumpConnected=ngFrontJumpArcChecks=0;
 ngFrontJumpNoTraceRejected=0;
 ngFrontReturnDamageRejected=ngFrontReturnLandingRejected=0;
 ngFrontJumpStop="not_started";memset(ngFrontJumpCases,0,sizeof(ngFrontJumpCases));
 memset(ngLadderFailure,0,sizeof(ngLadderFailure));
 ngLandingProbes=ngLandingGrounded=ngLandingSupported=ngLandingArcs=ngLandingWalks=0;
 ngLowerTransferTests=ngLowerTransferSuccesses=0;
 memset(ngExitFailures,0,sizeof(ngExitFailures));ngExitDebug=qfalse;
 ngTrSeedEdgeBegin=ngEdgeCount;NG_TrLadderSeeds();ngTrSeedEdgeEnd=ngEdgeCount;baseCount=ngCount;
 /* Rebuild graph state, retaining any earlier hard cache failure. */
 memset(ngTrPairCache,0,sizeof(ngTrPairCache));
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].from>=0&&ngEdges[i].to>=0){
  int a=ngEdges[i].from,b=ngEdges[i].to;
  NG_TrMarkSeen(a,b);
  if(ngEdges[i].accepted)NG_TrSetAccepted(a,b,qtrue);
 }
 for(i=0;i<ngCount;i++)if(ngNodes[i].id>maxid)maxid=ngNodes[i].id;
 ngLadderDetailed=qtrue;
 for(i=0;i<ngBspLadderPairCount;i++)NG_TrAddLadder(ngBspLadderPairs[i][0],ngBspLadderPairs[i][1],&maxid);
 ngLadderDetailed=qfalse;
 ngBspTraversalTraces=ngTraces-budgetStart;
 NG_SpawnReachability();
 NG_TrFrontJumps(baseCount,&maxid);
 NG_TrFreeClimbs(baseCount,&maxid);
 /* Test useful shortcuts inside the playable component first, bridges
  * second, isolated geometry last. File ordering must not spend the entire
  * arc budget on sealed roof details. */
 for(pass=0;pass<3;pass++)for(i=0;i<baseCount;i++)if(ngNodes[i].accepted&&!ngNodes[i].crouch){
  for(j=0;j<baseCount;j++)if(i!=j&&ngNodes[j].accepted&&!ngNodes[j].crouch){
   int group=ngSpawnReachable[i]?(ngSpawnReachable[j]?0:1):(ngSpawnReachable[j]?1:2);
   int phase=0,phaseStart=ngTraces;
   vec3_t d;float xy;VectorSubtract(ngNodes[j].feet,ngNodes[i].feet,d);xy=d[0]*d[0]+d[1]*d[1];
   if(group!=pass)continue;
   if(ngTraces>=NG_TRACE_BUDGET-4000||ngEdgeCount>=NG_EDGES-8||ngCount>=NG_NODES-2)goto traversal_done;
   if((ngJumpCandidates>=1024||ngJumpTraversalTraces>=140000)&&
      (ngLadderCandidates>=512||ngGenericLadderTraces>=40000))goto traversal_done;
   if(xy>160.f*160.f||d[2]<-24.f||d[2]>=384.f)continue;
   if(NG_TrAccepted(i,j)||NG_TrSpecialInput(i,j))continue;
   if(d[2]>60.f&&d[2]<384.f&&xy<96.f*96.f&&ngLadderCandidates<512&&ngGenericLadderTraces<40000){
    phase=2;NG_TrAddLadder(i,j,&maxid);
   }else if(d[2]>=-24.f&&d[2]<=60.f&&xy>=40.f*40.f&&xy<=160.f*160.f&&ngJumpCandidates<1024&&ngJumpTraversalTraces<140000){
    phase=1;
    if(!NG_TrJumpPrefilter(&ngNodes[i],&ngNodes[j]))goto traversal_pair_done;
    /* Never replace a perfectly walkable untested adjacency with a jump. */
    if(!NG_TrSeen(i,j)&&!NG_Walk(&ngNodes[i],&ngNodes[j],ngStand))goto traversal_pair_done;
    ngJumpCandidates++;
    if(ngJumpProofCount<64){
     /* A dedicated launch node has only this jump as its outgoing route.
      * Marking an existing junction would make bots jump on walking routes.
      * Place it on separately walkable ground rather than overlaying the
      * source vertex; revalidate the arc from the actual new launch point. */
     ngNode launch=ngNodes[i];ngJumpProof standing;int offset,n;float length=sqrt(xy);qboolean found=qfalse;memset(&standing,0,sizeof(standing));
     for(offset=20;offset<=68&&!found;offset+=8){
      vec3_t trial; qboolean clear=qtrue;
      VectorMA(ngNodes[i].feet,(float)offset/length,d,trial);trial[2]=ngNodes[i].feet[2];
      if(NG_Ground(trial,launch.feet,ngStand))continue;
      for(n=0;n<ngCount;n++)if(ngNodes[n].accepted){
       vec3_t gap;float minimum=ngNodes[n].radius>0.f?ngNodes[n].radius+8.f:20.f;
       if(minimum<20.f)minimum=20.f;
       VectorSubtract(launch.feet,ngNodes[n].feet,gap);
       if(DotProduct(gap,gap)<minimum*minimum){clear=qfalse;break;}
      }
      if(!clear||NG_Walk(&ngNodes[i],&launch,ngStand)||NG_Walk(&launch,&ngNodes[i],ngStand))continue;
      if(NG_TrJump(&launch,&ngNodes[j]))found=qtrue;
      else if(NG_TrStandingJump(&launch,&ngNodes[j],phaseStart+16000,&standing))found=qtrue;
     }
     if(!found){
      ngEdge *edge=NULL;
      for(n=0;n<ngEdgeCount;n++)if(ngEdges[n].from==i&&ngEdges[n].to==j){edge=&ngEdges[n];break;}
      if(!edge&&ngEdgeCount<NG_EDGES){edge=&ngEdges[ngEdgeCount++];memset(edge,0,sizeof(*edge));edge->from=i;edge->to=j;edge->added=qtrue;}
      if(edge&&!edge->accepted&&(!edge->reason||strcmp(edge->reason,"special_link_requires_review")))edge->reason="jump_takeoff_no_separated_validated_ground";
      goto traversal_pair_done;
     }
     NG_TrCommitSingleJump(i,j,&launch,&maxid,&standing);
    }
   }
traversal_pair_done:
   if(phase==1)ngJumpTraversalTraces+=ngTraces-phaseStart;
   else if(phase==2)ngGenericLadderTraces+=ngTraces-phaseStart;
  }
 }
traversal_done:
 G_Printf("navgen traversal: jump %d/%d directed links; ladder %d links/%d candidates; geometry only, live traversal pending\n",ngJumpLinks,ngJumpCandidates,ngLadderLinks,ngLadderCandidates);
 G_Printf("navgen jump negative prefilter: %d/%d tall-wall candidates rejected before full arcs; spawn-connected pairs prioritized\n",ngJumpPrefilterRejected,ngJumpPrefilterChecks);
 G_Printf("navgen ladder rejected rays/ground/approach/shaft-exit/continuity: %d %d %d %d %d\n",ngLadderFailure[0],ngLadderFailure[1],ngLadderFailure[2],ngLadderFailure[3],ngLadderFailure[4]);
}

/* Quarantine may exclude an inaccessible mounting floor. Never retain its
 * redundant exit alias (or interior guidance target) as if a complete
 * ladder survived. Keep the ordinary sampled floors for separate review. */
static void NG_TrPruneIncomplete(void) {
 int i,k;unsigned char removed[NG_NODES];memset(removed,0,sizeof(removed));
 for(i=0;i<ngLadderProofCount;i++){
  const ngLadderProof *p=&ngLadderProofs[i];qboolean complete=ngNodes[p->low].accepted&&ngNodes[p->high].accepted;
  for(k=0;k<4;k++)if((k!=2||p->reverse)&&!ngNodes[p->node[k]].accepted)complete=qfalse;
  if(p->ascentRelease>=0&&!ngNodes[p->ascentRelease].accepted)complete=qfalse;
  if(p->ascentAlignment>=0&&!ngNodes[p->ascentAlignment].accepted)complete=qfalse;
  if(complete)continue;
  for(k=0;k<4;k++){
   ngNode *n=&ngNodes[p->node[k]];n->accepted=qfalse;n->index=-1;
   n->reason="incomplete_ladder_route_not_exported";removed[p->node[k]]=1;
  }
  if(p->ascentAlignment>=0){
   ngNode *n=&ngNodes[p->ascentAlignment];n->accepted=qfalse;n->index=-1;
   n->reason="incomplete_ladder_route_not_exported";removed[p->ascentAlignment]=1;
  }
  if(p->ascentRelease>=0){
   ngNode *n=&ngNodes[p->ascentRelease];n->accepted=qfalse;n->index=-1;
   n->reason="incomplete_ladder_route_not_exported";removed[p->ascentRelease]=1;
  }
 }
 for(i=0;i<ngFreeClimbProofCount;i++) {
  const ngFreeClimbProof *p=&ngFreeClimbProofs[i];
  if(ngNodes[p->approach].accepted&&ngNodes[p->launch].accepted&&ngNodes[p->landing].accepted)continue;
  ngNodes[p->launch].accepted=qfalse;ngNodes[p->launch].index=-1;
  ngNodes[p->launch].reason="incomplete_free_climb_route_not_exported";removed[p->launch]=1;
 }
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted&&
    (removed[ngEdges[i].from]||removed[ngEdges[i].to])){
  ngEdges[i].accepted=qfalse;ngEdges[i].reason="incomplete_ladder_route_not_exported";
 }
}
