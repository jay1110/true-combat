/* TC free-climb is a short ordinary jump onto a vertical World ledge,
 * followed by PM_CheckLadderMove's non-SURF_LADDER contact. Reproduce that
 * readonly clearance/stamina gate, then integrate native full-hull contacts.
 * This remains three-speed collision evidence, not a full Pmove/live claim. */
static qboolean NG_TrFreeContact(const vec3_t feet,const vec3_t normal,float vz,qboolean wasClimbing,int limit) {
 vec3_t end,raised;trace_t tr;float below,rise=96.f*(g_newbbox.integer?1.25f:1.f),height;
 if(vz<-200.f||ngTraces>=limit-8)return qfalse;
 VectorMA(feet,48.f,normal,end); /* normal points into the climbable face. */
 if(!NG_Trace(&tr,feet,end,ngStand)||tr.startsolid||tr.allsolid||tr.fraction*48.f>=1.f||
    tr.entityNum!=ENTITYNUM_WORLD||fabs(tr.plane.normal[2])>=.001f||
    (tr.surfaceFlags&(SURF_LADDER|0x40000))||(tr.contents&0x6000000)||
    -(tr.plane.normal[0]*normal[0]+tr.plane.normal[1]*normal[1])<.98f)return qfalse;
 VectorCopy(feet,end);end[2]-=256.f;
 if(!NG_Trace(&tr,feet,end,ngStand)||tr.startsolid||tr.allsolid)return qfalse;
 below=tr.fraction*-256.f;
 VectorCopy(feet,end);end[2]+=rise;
 if(!NG_Trace(&tr,feet,end,ngStand)||tr.startsolid||tr.allsolid)return qfalse;
 if(tr.fraction<1.f)rise*=tr.fraction;if(rise<18.f)return qfalse;
 VectorMA(feet,4.f,normal,raised);raised[2]+=rise;
 if(!NG_Trace(&tr,raised,raised,ngStand))return qfalse;
 if(tr.allsolid){rise=(rise-18.f)*.5f+18.f;VectorMA(feet,4.f,normal,raised);raised[2]+=rise;
  if(!NG_Trace(&tr,raised,raised,ngStand)||tr.allsolid)return qfalse;}
 VectorCopy(raised,end);end[2]-=256.f;
 if(!NG_Trace(&tr,raised,end,ngStand)||tr.startsolid||tr.allsolid)return qfalse;
 height=(rise-below)-tr.fraction*256.f;
 (void)wasClimbing; /* Initial2500 stamina is budgeted by the integrator. */
 return height>=63.f&&height<=101.f*(g_newbbox.integer?1.25f:1.f)&&below<=-17.f;
}
/* A real capsule can touch the platform rim before its centre reaches the
 * top. Continue grounded forward motion using the same full body support;
 * do not snap to the ledge or require centre support at that first instant. */
static qboolean NG_TrFreeFinish(const vec3_t from,const vec3_t to,const vec3_t direction,float wishspeed,int limit) {
 const float dt=.008f;vec3_t current,next,velocity,push,down,error,floor;trace_t tr;int step;
 VectorCopy(from,current);VectorClear(velocity);
 for(step=0;step<125;step++) {
  float speed=sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]),length,amount;
  if(ngTraces>=limit-16)return qfalse;
  VectorCopy(current,down);down[2]-=.25f;
  if(!NG_Trace(&tr,current,down,ngStand)||tr.startsolid||tr.allsolid||tr.fraction==1.f||
     !NG_StaticGround(tr.entityNum)||tr.plane.normal[2]<.7f||(tr.surfaceFlags&SURF_SLICK))return qfalse;
  if(speed<1.f)velocity[0]=velocity[1]=0.f;
  else {float friction=(speed>50.f?speed:50.f)*6.f*dt;VectorScale(velocity,friction<speed?1.f-friction/speed:0.f,velocity);}
  VectorScale(direction,wishspeed,push);VectorSubtract(push,velocity,push);
  length=VectorNormalize(push);amount=dt*wishspeed*10.f;if(amount>length)amount=length;
  VectorMA(velocity,amount,push,velocity);VectorMA(current,dt,velocity,next);
  if(!NG_Trace(&tr,current,next,ngStand)||tr.startsolid||tr.allsolid||tr.fraction<1.f)return qfalse;
  if(NG_Hurt(next,ngStand)||(trap_PointContents(next,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)))return qfalse;
  VectorSubtract(next,to,error);if(DotProduct(error,direction)>8.f)return qfalse;
  if(!NG_Ground(next,floor,ngStand)&&fabs(floor[2]-next[2])<=1.f&&NG_TrExitContinuation(next,to))return qtrue;
  VectorCopy(next,current);
 }
 return qfalse;
}
static qboolean NG_TrFreeSpeed(const ngNode *a,const ngNode *b,const vec3_t direction,float wishspeed,int limit,ngFreeClimbProof *proof) {
 const float dt=.008f;float gravity=NG_TrGravity(),stamina=2500.f,climbTime=0.f;
 vec3_t current,next,velocity,push,error,landed;trace_t tr;int step; qboolean climbed=qfalse,attached=qfalse;
 VectorCopy(a->feet,current);VectorSet(velocity,0,0,TCE_NavStandingImpulse(g_newbbox.integer));
 for(step=0;step<250;step++) {
  float amount,length;int slide;
  if(ngTraces>=limit-32)return qfalse;
  attached=NG_TrFreeContact(current,direction,velocity[2],climbed,limit);
  if(attached){
   /* PM_LadderMove: ladder friction14 affects XY, upwish=.9*CmdScale,
    * PM_Accelerate's full3D push uses acceleration10. */
   float horizontal=sqrt(velocity[0]*velocity[0]+velocity[1]*velocity[1]);
   if(horizontal>0.f){float f=horizontal*14.f*dt;velocity[0]*=f<horizontal?1.f-f/horizontal:0.f;velocity[1]*=f<horizontal?1.f-f/horizontal:0.f;}
   VectorSet(push,-velocity[0],-velocity[1],wishspeed*.9f-velocity[2]);
   length=VectorNormalize(push);amount=dt*wishspeed*.9f*10.f;if(amount>length)amount=length;
   VectorMA(velocity,amount,push,velocity);stamina-=1250.f*dt;climbTime+=dt;climbed=qtrue;
   if(stamina<10.f)return qfalse;
  } else {
   VectorScale(direction,step?wishspeed:wishspeed*.70710678f,push);VectorSubtract(push,velocity,push);
   length=VectorNormalize(push);amount=dt*(step?wishspeed:wishspeed*.70710678f);if(amount>length)amount=length;
   VectorMA(velocity,amount,push,velocity);
  }
  VectorMA(current,dt,velocity,next);if(!attached){next[2]-=.5f*gravity*dt*dt;velocity[2]-=gravity*dt;}
  /* Ordinary vertical-face slide only; no step/snap across an obstruction.
   * Keep the remainder of the same frame after a real face collision. */
  for(slide=0;slide<2;slide++) {
   if(!NG_Trace(&tr,current,next,ngStand)||tr.startsolid||tr.allsolid)return qfalse;
   if(tr.fraction==1.f)break;
   VectorSubtract(tr.endpos,b->feet,error);
   if(velocity[2]<0.f&&NG_StaticGround(tr.entityNum)&&tr.plane.normal[2]>=.7f&&!(tr.surfaceFlags&SURF_SLICK)) {
    float crash=velocity[2]*velocity[2]*(g_newbbox.integer?.64f:1.f)*.0002f;
    if(!climbed||fabs(error[2])>18.f||error[0]*error[0]+error[1]*error[1]>96.f*96.f||crash>38.f)return qfalse;
    VectorCopy(tr.endpos,landed);if(!NG_TrExitContinuation(landed,b->feet)&&!NG_TrFreeFinish(landed,b->feet,direction,wishspeed,limit))return qfalse;
    proof->duration=(step+1)*dt;proof->climbSeconds=climbTime;proof->stamina=2500.f-stamina;
    proof->impact=fabs(velocity[2]);proof->crash=crash;VectorCopy(landed,proof->touchdown);return qtrue;
   }
   if(tr.entityNum!=ENTITYNUM_WORLD||fabs(tr.plane.normal[2])>=.001f||
      -(tr.plane.normal[0]*direction[0]+tr.plane.normal[1]*direction[1])<.98f)return qfalse;
   VectorCopy(tr.endpos,current);velocity[0]=velocity[1]=0.f;
   VectorCopy(current,next);next[2]+=velocity[2]*dt*(1.f-tr.fraction);
  }
  if(NG_Hurt(next,ngStand)||(trap_PointContents(next,ENTITYNUM_NONE)&(CONTENTS_WATER|CONTENTS_LAVA|CONTENTS_SLIME)))return qfalse;
  VectorSubtract(next,a->feet,error);if(error[0]*error[0]+error[1]*error[1]>160.f*160.f||next[2]<a->feet[2]-2.f)return qfalse;
  VectorCopy(next,current);
 }
 return qfalse;
}
static qboolean NG_TrFreeClimb(const ngNode *a,const ngNode *b,int limit,ngFreeClimbProof *proof) {
 vec3_t direction,end,floor,down;trace_t tr;float horizontal;int speed;ngFreeClimbProof trial;
 VectorSubtract(b->feet,a->feet,direction);horizontal=sqrt(direction[0]*direction[0]+direction[1]*direction[1]);
 if(NG_TrGravity()<100.f||NG_TrGravity()>2000.f||a->crouch||b->crouch||horizontal<ngWidth+4.f||horizontal>160.f||
    direction[2]<=18.25f||direction[2]>101.f*(g_newbbox.integer?1.25f:1.f)||!NG_TrStableLanding(b->feet))return qfalse;
 direction[2]=0;VectorNormalize(direction);VectorMA(a->feet,1.f,direction,end);
 if(!NG_Trace(&tr,a->feet,end,ngStand)||tr.startsolid||tr.allsolid||tr.fraction>=1.f||
    tr.entityNum!=ENTITYNUM_WORLD||fabs(tr.plane.normal[2])>=.001f||
    (tr.surfaceFlags&(SURF_LADDER|0x40000))||(tr.contents&0x6000000))return qfalse;
 /* Face-normal input avoids diagonal friction/contact loss. The target
  * must lie in the same proved narrow forward lane. */
 VectorSet(direction,-tr.plane.normal[0],-tr.plane.normal[1],0);VectorNormalize(direction);
 VectorSubtract(b->feet,a->feet,end);if(fabs(direction[0]*end[1]-direction[1]*end[0])>4.f)return qfalse;
 VectorCopy(a->feet,down);down[2]-=.25f;
 if(!NG_Trace(&tr,a->feet,down,ngStand)||tr.startsolid||tr.allsolid||tr.fraction==1.f||!NG_StaticGround(tr.entityNum)||tr.plane.normal[2]<.7f)return qfalse;
 VectorCopy(a->feet,floor);floor[2]+=18.f;
 if(!NG_TrFreeContact(floor,direction,100.f,qfalse,limit))return qfalse;
 memset(proof,0,sizeof(*proof));VectorCopy(direction,proof->direction);
 for(speed=0;speed<3;speed++) {
  memset(&trial,0,sizeof(trial));if(!NG_TrFreeSpeed(a,b,direction,ngDropSpeeds[speed],limit,&trial))return qfalse;
  if(!speed){*proof=trial;VectorCopy(direction,proof->direction);}
  if(trial.crash>proof->crash){proof->crash=trial.crash;proof->impact=trial.impact;}
  if(trial.stamina>proof->stamina)proof->stamina=trial.stamina;
 }
 return qtrue;
}
/* Read-only current-world probe with the same launch projection and ordinary
 * approach checks as generation. Keep each print below the239-byte engine
 * formatter limit; no graph, proof array or diagnostic accounting changes. */
static void NG_TrProbeFreeClimb(const ngNode *from,const ngNode *to,int limit) {
 int savedTraces=ngTraces,savedDiagnostics=ngStepDiagnostics,queries; qboolean savedProbe=ngProbe;
 unsigned int savedConditional[MAX_GENTITIES];ngNode launch=*from;ngFreeClimbProof proof;
 vec3_t direction,end,contact;trace_t hit;qboolean face=qfalse,approach=qfalse,gate=qfalse,accepted=qfalse;const char *reason="world_face_not_found";
 memcpy(savedConditional,ngConditionalGroundHits,sizeof(savedConditional));memset(&proof,0,sizeof(proof));
 ngProbe=qfalse;ngStepDiagnostics=5;VectorSubtract(to->feet,from->feet,direction);direction[2]=0;VectorNormalize(direction);
 VectorCopy(to->feet,end);end[2]=from->feet[2];
 if(NG_Trace(&hit,from->feet,end,ngStand)&&!hit.startsolid&&!hit.allsolid&&hit.fraction<1.f&&
    hit.entityNum==ENTITYNUM_WORLD&&fabs(hit.plane.normal[2])<.001f) {
  face=qtrue;reason="launch_ground_or_walk_rejected";VectorMA(hit.endpos,.125f,hit.plane.normal,end);
  if(!NG_Ground(end,launch.feet,ngStand)&&fabs(launch.feet[2]-from->feet[2])<=1.f&&
     !NG_Walk(from,&launch,ngStand)&&!NG_Walk(&launch,from,ngStand)) {
   approach=qtrue;VectorSet(direction,-hit.plane.normal[0],-hit.plane.normal[1],0);VectorNormalize(direction);
   VectorCopy(launch.feet,contact);contact[2]+=18.f;gate=NG_TrFreeContact(contact,direction,100.f,qfalse,limit);
   accepted=NG_TrFreeClimb(&launch,to,limit,&proof);reason=accepted?"accepted":gate?"native_motion_or_exit_rejected":"native_lip_gate_rejected";
  }
 }
 queries=ngTraces-savedTraces;
 G_Printf("navgen probe climb {\"part\":\"points\",\"coordinates\":\"feet\",\"from\":[%.4f,%.4f,%.4f],\"to\":[%.4f,%.4f,%.4f],\"readonly\":true}\n",
  from->feet[0],from->feet[1],from->feet[2],to->feet[0],to->feet[1],to->feet[2]);
 G_Printf("navgen probe climb {\"part\":\"launch\",\"world_face\":%s,\"walk\":%s,\"lip_gate\":%s,\"position\":[%.4f,%.4f,%.4f]}\n",
  face?"true":"false",approach?"true":"false",gate?"true":"false",launch.feet[0],launch.feet[1],launch.feet[2]);
 G_Printf("navgen probe climb {\"part\":\"climb\",\"accepted\":%s,\"reason\":\"%s\",\"queries\":%d,\"live_verified\":false}\n",accepted?"true":"false",reason,queries);
 if(accepted)G_Printf("navgen probe climb {\"part\":\"proof\",\"samples\":3,\"duration\":%.4f,\"contact\":%.4f,\"stamina\":%.4f,\"crash\":%.4f,\"touchdown\":[%.4f,%.4f,%.4f]}\n",
  proof.duration,proof.climbSeconds,proof.stamina,proof.crash,proof.touchdown[0],proof.touchdown[1],proof.touchdown[2]);
 ngTraces=savedTraces;ngStepDiagnostics=savedDiagnostics;ngProbe=savedProbe;memcpy(ngConditionalGroundHits,savedConditional,sizeof(savedConditional));
}
static int NG_TrFreeTarget(int index) {
 int i;if(ngNodes[index].authored&&(ngNodes[index].editRole&128)) {
  int target=NG_Find(ngNodes[index].editDropId);return target>=0&&ngNodes[index].accepted&&ngNodes[target].accepted?target:-1;
 }
 for(i=0;i<ngFreeClimbProofCount;i++)if(ngFreeClimbProofs[i].launch==index&&ngNodes[index].accepted&&ngNodes[ngFreeClimbProofs[i].approach].accepted&&ngNodes[ngFreeClimbProofs[i].landing].accepted)return ngFreeClimbProofs[i].landing;
 return -1;
}
static qboolean NG_TrCommitManualFreeClimb(ngEdge *edge,const ngFreeClimbProof *proof) {
 ngNode *a=&ngNodes[edge->from];int i;
 for(i=0;i<ngFreeClimbProofCount;i++)if(ngFreeClimbProofs[i].launch==edge->from)break;
 if(i==ngFreeClimbProofCount){if(ngFreeClimbProofCount>=32)return qfalse;ngFreeClimbProofCount++;}
 ngFreeClimbProofs[i]=*proof;ngFreeClimbProofs[i].approach=ngFreeClimbProofs[i].launch=edge->from;ngFreeClimbProofs[i].landing=edge->to;
 a->editRole|=128;a->editSourceId=a->id;a->editDropId=ngNodes[edge->to].id;a->navFlags|=NG_NAV_CLIMB;a->radius=2.f;
 a->topologyProtected=ngNodes[edge->to].topologyProtected=qtrue;return qtrue;
}
/* Same launch and exact directed destination may be discovered from several
 * nearby ordinary approaches. Keep one automatically generated recipe;
 * distinct destinations and authored points remain independent. */
static int NG_TrReuseFreeClimb(const ngNode *point,int landing) {
 int i;
 if(point->authored||point->crouch||point->flags||point->packed||point->editFlags||point->editRole||
    point->door.mode||point->count||point->name[0]||point->ent[0]||point->ent[1]||point->ent[2]||
    (point->navFlags&&point->navFlags!=NG_NAV_CLIMB))return -1;
 for(i=0;i<ngFreeClimbProofCount;i++) {
  const ngFreeClimbProof *p=&ngFreeClimbProofs[i];const ngNode *n;vec3_t gap;
  if(p->landing!=landing||p->launch<0||p->launch>=ngCount||p->approach<0||p->approach>=ngCount)continue;
  n=&ngNodes[p->launch];
  if(!n->accepted||!ngNodes[p->approach].accepted||!ngNodes[landing].accepted||n->authored||
     n->navFlags!=NG_NAV_CLIMB||n->crouch||n->flags||n->packed||n->editFlags||n->editRole||n->door.mode||n->count||n->name[0]||
     n->ent[0]||n->ent[1]||n->ent[2])continue;
  VectorSubtract(point->feet,n->feet,gap);if(DotProduct(gap,gap)<=.000001f)return p->launch;
 }
 return -1;
}
static void NG_TrFreeClimbs(int baseCount,int *maxid) {
 int a,b,attempts=0,start=ngTraces;ngNode launch;ngFreeClimbProof proof;
 for(a=0;a<baseCount;a++)if(ngNodes[a].accepted&&!ngNodes[a].crouch&&ngSpawnReachable[a])
  for(b=0;b<baseCount;b++)if(a!=b&&ngNodes[b].accepted&&!ngNodes[b].crouch) {
   vec3_t d,end;float horizontal;trace_t tr;int takeoff;
   VectorSubtract(ngNodes[b].feet,ngNodes[a].feet,d);horizontal=sqrt(d[0]*d[0]+d[1]*d[1]);
   /* PM_ClimbSlideMove's63 minimum is the measured front lip, not the
    * destination floor behind that lip. The unchanged contact gate and
    * complete three-speed motion still prove the actual ledge. */
   if(d[2]<=18.25f||d[2]>101.f*(g_newbbox.integer?1.25f:1.f)||horizontal<ngWidth+4.f||horizontal>160.f||NG_TrAccepted(a,b))continue;
   if(attempts>=96||ngTraces-start>=40000||ngCount>=NG_NODES||ngFreeClimbProofCount>=32)return;
   d[2]=0;VectorNormalize(d);VectorMA(ngNodes[a].feet,horizontal,d,end);attempts++;
   if(!NG_Trace(&tr,ngNodes[a].feet,end,ngStand)||tr.startsolid||tr.allsolid||tr.fraction==1.f||tr.entityNum!=ENTITYNUM_WORLD||fabs(tr.plane.normal[2])>=.001f)continue;
   launch=ngNodes[a];VectorMA(tr.endpos,.125f,tr.plane.normal,end);
   if(NG_Ground(end,launch.feet,ngStand)||fabs(launch.feet[2]-ngNodes[a].feet[2])>1.f||
      NG_Walk(&ngNodes[a],&launch,ngStand)||NG_Walk(&launch,&ngNodes[a],ngStand))continue;
   if(!NG_TrFreeClimb(&launch,&ngNodes[b],ngTraces+8000,&proof))continue;
   takeoff=NG_TrReuseFreeClimb(&launch,b);
   if(takeoff>=0) {
    NG_TrLink(a,takeoff,"free_climb_approach_validated_live_pending");
    NG_TrLink(takeoff,a,"free_climb_retreat_validated_live_pending");
    ngNodes[a].topologyProtected=qtrue;continue;
   }
   takeoff=ngCount++;launch.id=++*maxid;launch.index=-1;launch.count=0;launch.name[0]=0;launch.navFlags=NG_NAV_CLIMB;launch.radius=2.f;
   launch.editRole=0;launch.authored=qfalse;launch.topologyProtected=qtrue;launch.reason="generated_free_climb_launch_live_pending";
   VectorCopy(launch.feet,launch.raw);launch.raw[2]+=ngOrigin;ngNodes[takeoff]=launch;
   proof.approach=a;proof.launch=takeoff;proof.landing=b;ngFreeClimbProofs[ngFreeClimbProofCount++]=proof;ngNodes[a].topologyProtected=ngNodes[b].topologyProtected=qtrue;
   NG_TrLink(a,takeoff,"free_climb_approach_validated_live_pending");NG_TrLink(takeoff,a,"free_climb_retreat_validated_live_pending");
   NG_TrLink(takeoff,b,"free_climb_hull_samples_validated_live_pending");
  }
 G_Printf("navgen freeclimb: %d directed ledges/%d candidate fronts, %d traces; PM_ClimbSlideMove gate, live pending\n",ngFreeClimbProofCount,attempts,ngTraces-start);
}
