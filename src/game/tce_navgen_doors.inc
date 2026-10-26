/* Conditional door transitions, proved with the loaded engine's collision.
 * Only collision poses are scoped here. Never call use/think, run scripts,
 * move clients, advance time, adjust area portals, or change mover state.
 * The real runtime must activate and recheck a door before traversing it. */
static int ngDoorProofTraces;
#include "tce_nav_door_collision.inc"
#define NG_DOOR_FLAG (1u<<7)
#define NG_DOOR_CASES 128
#define NG_DOOR_TRACES 120000
typedef struct {int entity,useEntity,from,to,team;const char *reason;} ngDoorCase;
static ngDoorCase ngDoorCases[NG_DOOR_CASES];
static int ngDoorCaseCount,ngDoorCandidates,ngDoorLinks,ngDoorTraceStart;
static qboolean ngDoorRepairExisting,ngDoorRepairRun;
static int ngDoorMinimumId;
static const char *ngDoorStop;
static void NG_Text(const char *text);
static void NG_DoorLink(int from,int to,const char *reason);
static qboolean NG_DoorAnchorWalk(ngDoorPose *pose,ngNode *point,ngNode *anchor);
static const char *NG_DoorGroup(int number,ngDoorPose *pose,int *team) {
 int master=NG_DoorMaster(number),n;gentity_t *ent;
 memset(pose,0,sizeof(*pose));*team=3;
 if(master<0)return "unknown_door";
 pose->master=master;ent=&g_entities[master];
 while(ent){
  n=(int)(ent-g_entities);
  if(n<MAX_CLIENTS||n>=level.num_entities||!ent->inuse||!NG_DoorClass(ent)||!ent->r.bmodel||
     !ent->r.linked||!(ent->r.contents&NG_MASK)||ent->use!=Use_BinaryMover)return "unsupported_door_group";
  if(pose->count>=NG_DOOR_GROUP)return "door_group_budget";
  for(n=0;n<pose->count;n++)if(pose->part[n].entity==(int)(ent-g_entities))return "door_group_cycle";
  if(ent->key!=0)return "door_locked_or_key_required";
  if(!Q_stricmp(ent->classname,"func_door")&&(ent->spawnflags&1))return "start_open_reverse_action_unsupported";
  if(ent->takedamage||ent->target&&*ent->target)return "door_shoot_or_target_effect_unsupported";
  if(ent->s.modelindex<=0||ent->s.modelindex>=MAX_MODELS)return "door_model_identity_missing";
  if(ent->moverState!=MOVER_POS1&&ent->moverState!=MOVER_POS2&&
     ent->moverState!=MOVER_POS1ROTATE&&ent->moverState!=MOVER_POS2ROTATE)return "door_currently_moving";
  if(!Q_stricmp(ent->classname,"func_door_rotating")){
   if(!NG_Finite(ent->angle)||fabs(ent->angle)<1.f||fabs(ent->angle)>180.f)return "invalid_rotation";
   if(!NG_Finite(ent->rotate[0])||!NG_Finite(ent->rotate[1])||!NG_Finite(ent->rotate[2])||
      fabs(VectorLengthSquared(ent->rotate)-1.f)>.001f)return "invalid_rotation_axis";
   /* SetMoverState actually opens from apos.trBase={0,0,0}, not from an
    * arbitrary authored angle. Reject noncanonical initial closed angles
    * instead of silently pretending a different collision pose is closed. */
   if(ent->moverState==MOVER_POS1ROTATE&&VectorLengthSquared(ent->r.currentAngles)>.015625f)return "noncanonical_rotating_closed_pose";
   if(ent->s.pos.trType!=TR_STATIONARY)return "rotating_origin_is_moving";
  }else{vec3_t d;VectorSubtract(ent->pos2,ent->pos1,d);if(VectorLength(d)<1.f)return "no_door_motion";}
  *team&=NG_DoorTeam(ent);if(!*team)return "door_team_disallows_all";
  pose->part[pose->count].entity=(int)(ent-g_entities);pose->part[pose->count++].shared=ent->r;
  ent=ent->teamchain;
 }
 return NULL;
}
static qboolean NG_DoorContains(const ngDoorPose *pose,int entity) {
 int i;for(i=0;i<pose->count;i++)if(pose->part[i].entity==entity)return qtrue;return qfalse;
}
static void NG_DoorCaseAdd(int entity,int useEntity,int from,int to,int team,const char *reason) {
 ngDoorCase *c;if(ngDoorCaseCount>=NG_DOOR_CASES)return;
 c=&ngDoorCases[ngDoorCaseCount++];c->entity=entity;c->useEntity=useEntity;c->from=from;c->to=to;c->team=team;c->reason=reason;
}
static float NG_DoorEye(const ngNode *node) {
 return ngOrigin+(g_newbbox.integer?(node->crouch?26.f:52.f):(node->crouch?21.f:42.f));
}
static qboolean NG_DoorAimRay(const vec3_t eye,int useNumber,float range,const vec3_t aim) {
 vec3_t end,d;trace_t tr;float length;
 VectorSubtract(aim,eye,d);length=VectorNormalize(d);if(length<.001f||length>range+.001f)return qfalse;
 VectorMA(eye,96.f,d,end);if(ngTraces>=NG_TRACE_BUDGET)return qfalse;
 trap_Trace(&tr,eye,NULL,NULL,end,ENTITYNUM_NONE,CONTENTS_SOLID|CONTENTS_MISSILECLIP|CONTENTS_BODY|CONTENTS_CORPSE);ngTraces++;
 if(tr.entityNum==ENTITYNUM_WORLD||tr.surfaceFlags&SURF_NOIMPACT){
  if(ngTraces>=NG_TRACE_BUDGET)return qfalse;
  trap_Trace(&tr,eye,NULL,NULL,end,ENTITYNUM_NONE,CONTENTS_SOLID|CONTENTS_MISSILECLIP|CONTENTS_BODY|CONTENTS_CORPSE|CONTENTS_TRIGGER);ngTraces++;
 }
 return !tr.startsolid&&!tr.allsolid&&tr.entityNum==useNumber&&tr.fraction*96.f<=range+.001f;
}
static qboolean NG_DoorAim(const ngNode *node,int useNumber,qboolean direct,vec3_t aim,qboolean supplied) {
 static const float insets[]={2.f,4.f,8.f};vec3_t eye,first;gentity_t *use;int k,axis,inset;
 float range=direct?38.4f:96.f;
 if(useNumber<MAX_CLIENTS||useNumber>=level.num_entities)return qfalse;
 use=&g_entities[useNumber];VectorCopy(node->feet,eye);eye[2]+=NG_DoorEye(node);
 if(!direct&&!Q_stricmp(use->classname,"func_button")&&
    (use->active||use->s.pos.trType!=TR_STATIONARY||use->s.apos.trType!=TR_STATIONARY))return qfalse;
 if(supplied)return NG_DoorAimRay(eye,useNumber,range,aim);
 for(k=0;k<3;k++){float lo=use->r.absmin[k]+.25f,hi=use->r.absmax[k]-.25f;
  aim[k]=lo>hi?(use->r.absmin[k]+use->r.absmax[k])*.5f:(eye[k]<lo?lo:(eye[k]>hi?hi:eye[k]));}
 if(NG_DoorAimRay(eye,useNumber,range,aim))return qtrue;
 if(!direct)return qfalse;VectorCopy(aim,first);
 /* A nearest AABB corner may be hidden behind a jamb although an actual
  * nearby leaf point is usable. At most six inset rays still require the
  * exact real closed leaf and unchanged direct 38.4-unit activation range.
  * Authored/saved aims above are rechecked exactly, never silently moved. */
 for(axis=0;axis<2;axis++)for(inset=0;inset<3;inset++){
  float lo=use->r.absmin[axis]+insets[inset],hi=use->r.absmax[axis]-insets[inset];
  VectorCopy(first,aim);aim[axis]=lo>hi?(use->r.absmin[axis]+use->r.absmax[axis])*.5f:(eye[axis]<lo?lo:(eye[axis]>hi?hi:eye[axis]));
  if(!VectorCompare(aim,first)&&NG_DoorAimRay(eye,useNumber,range,aim))return qtrue;
 }
 VectorCopy(first,aim);return qfalse;
}
static qboolean NG_DoorForceUseSafe(const ngDoorPose *pose,const ngNode *point) {
 int i,k;vec3_t middle;float height=point->crouch?ngDuck:ngStand,bodyRadius=sqrt(ngWidth*ngWidth+height*height*.25f),margin=point->radius;
 if(margin<10.f)margin=10.f;VectorCopy(point->feet,middle);middle[2]+=height*.5f;
 for(i=0;i<pose->count;i++){
  gentity_t *part=&g_entities[pose->part[i].entity];vec3_t local,delta;float radius;
  if(Q_stricmp(part->classname,"func_door_rotating")||!(part->spawnflags&32))continue;
  /* A remote FORCE activator is safe only outside an enclosing sphere of
   * every possible leaf pose plus the complete body and arrival tolerance.
   * This negative geometric envelope does not claim a simulated swing. */
  for(k=0;k<3;k++)local[k]=fabs(part->r.mins[k])>fabs(part->r.maxs[k])?fabs(part->r.mins[k]):fabs(part->r.maxs[k]);
  radius=VectorLength(local);VectorSubtract(middle,pose->part[i].shared.currentOrigin,delta);
  if(VectorLength(delta)<=radius+bodyRadius+margin)return qfalse;
 }
 return qtrue;
}
static qboolean NG_DoorConditionalEdge(int from,int to) {
 return ngNodes[from].door.mode&&ngNodes[from].door.role==2&&ngNodes[from].door.target==ngNodes[to].id;
}
static qboolean NG_DoorOrdinaryAllowed(int edge,int team) {
 int a=ngEdges[edge].from,b=ngEdges[edge].to;unsigned long long fa,fb;
 if(!ngEdges[edge].accepted||a<0||b<0||a>=ngCount||b>=ngCount||!ngNodes[a].accepted||!ngNodes[b].accepted||NG_DoorConditionalEdge(a,b))return qfalse;
 fa=ngNodes[a].authored?ngNodes[a].editFlags:ngNodes[a].navFlags;fb=ngNodes[b].authored?ngNodes[b].editFlags:ngNodes[b].navFlags;
 if((fa|fb)&((1ULL<<5)|(1ULL<<9)|(1ULL<<27)|(1ULL<<18)|(1ULL<<8)|(1ULL<<12)|(1ULL<<11)|(1ULL<<26)))return qfalse;
 if((fa&16u)&&!(fa&team))return qfalse;if((fb&16u)&&!(fb&team))return qfalse;return qtrue;
}
static void NG_DoorOrdinarySeen(int from,qboolean reverse,int team,unsigned char *seen) {
 static int queue[NG_NODES],head[NG_NODES],next[NG_EDGES];int read=0,tail=0,k;
 memset(seen,0,NG_NODES);if(from<0||from>=ngCount)return;
 for(k=0;k<ngCount;k++)head[k]=-1;
 for(k=0;k<ngEdgeCount;k++){
  int a=ngEdges[k].from,b=ngEdges[k].to;if(!NG_DoorOrdinaryAllowed(k,team))continue;
  if(reverse){int swap=a;a=b;b=swap;}next[k]=head[a];head[a]=k;
 }
 seen[from]=1;queue[tail++]=from;
 while(read<tail){int a=queue[read++];for(k=head[a];k>=0;k=next[k]){
  int b=reverse?ngEdges[k].from:ngEdges[k].to;if(seen[b])continue;seen[b]=1;queue[tail++]=b;
 }}
}
static qboolean NG_DoorOrdinaryReach(int from,int to,int teams) {
 static unsigned char seen[NG_NODES];if(to<0||to>=ngCount)return qfalse;
 {int team;for(team=1;team<=2;team*=2)if(teams&team){NG_DoorOrdinarySeen(from,qfalse,team,seen);if(!seen[to])return qfalse;}}
 return teams!=0;
}
static void NG_DoorDistances(int source,int team,qboolean reverse,float *distances) {
 static int head[NG_NODES],next[NG_EDGES],heap[NG_NODES],place[NG_NODES];int size=1,i,k;
 for(i=0;i<ngCount;i++){head[i]=-1;place[i]=-1;distances[i]=1e20f;}
 for(k=0;k<ngEdgeCount;k++)if(NG_DoorOrdinaryAllowed(k,team)){
  int a=reverse?ngEdges[k].to:ngEdges[k].from;next[k]=head[a];head[a]=k;
 }
 heap[0]=source;place[source]=0;distances[source]=0.f;
 while(size){int a=heap[0],last=heap[--size],at=0;
  place[a]=-2;
  if(size){while(at*2+1<size){int child=at*2+1;if(child+1<size&&distances[heap[child+1]]<distances[heap[child]])child++;
   if(distances[last]<=distances[heap[child]])break;heap[at]=heap[child];place[heap[at]]=at;at=child;}
   heap[at]=last;place[last]=at;
  }
  for(k=head[a];k>=0;k=next[k]){
   int b=reverse?ngEdges[k].from:ngEdges[k].to;vec3_t d;float value;
   if(place[b]==-2)continue;VectorSubtract(ngNodes[a].feet,ngNodes[b].feet,d);value=distances[a]+VectorLength(d);
   if(value>=distances[b])continue;distances[b]=value;
   at=place[b];if(at<0)at=size++;
   while(at>0){int parent=(at-1)/2;if(distances[heap[parent]]<=value)break;heap[at]=heap[parent];place[heap[at]]=at;at=parent;}
   heap[at]=b;place[b]=at;
  }
 }
}
static const char *NG_DoorPassage(ngDoorPose *pose,ngNode *a,ngNode *b) {
 const char *why;trace_t tr;vec3_t start,end;int i,sign,rotation=0,begin=ngTraces;
 NG_DoorPoseSet(pose,0.f,1);
 VectorCopy(a->feet,start);VectorCopy(b->feet,end);start[2]+=ngOrigin+16.f;end[2]+=ngOrigin+16.f;
 if(ngTraces>=NG_TRACE_BUDGET){why="trace_budget";goto done;}
 trap_Trace(&tr,start,NULL,NULL,end,ENTITYNUM_NONE,NG_MASK);ngTraces++;
 if(tr.startsolid||tr.allsolid||!NG_DoorContains(pose,tr.entityNum)){why="closed_passage_not_this_door";goto done;}
 for(i=0;i<pose->count;i++)if(!Q_stricmp(g_entities[pose->part[i].entity].classname,"func_door_rotating"))rotation=1;
 /* Ordinary rotors can reverse for an activator; FORCE on the master cannot.
  * Check all actually possible signs, without inventing a FORCE reversal. */
 for(sign=1;sign>= (rotation&&!NG_DoorFixedRotation(pose)?-1:1);sign-=2){
  NG_DoorPoseSet(pose,1.f,sign);
  why=NG_Walk(a,b,a->crouch||b->crouch?ngDuck:ngStand);
  if(why&&!a->crouch&&!b->crouch){why=NG_Walk(a,b,ngDuck);if(!why){
   /* Only an unbound generation proposal may acquire its required stance.
    * A saved USE aim is tied to its exact standing/crouched eye height. */
   if(a->authored||b->authored||a->door.mode||b->door.mode)why="door_saved_stance_requires_crouch";
   else a->crouch=b->crouch=qtrue;
  }}
  if(why)goto done;
 }
 why=NULL;
done:NG_DoorPoseRestore(pose);ngDoorProofTraces+=ngTraces-begin;return why;
}
static qboolean NG_DoorSafeExit(ngDoorPose *pose,ngNode *entry,ngNode *near,const ngNode *networkAnchor,ngNode *exit,int limit) {
 vec3_t direction,tangent;float best=1e20f;int forward,side;
 VectorSubtract(near->feet,entry->feet,direction);direction[2]=0;if(VectorNormalize(direction)<1.f)return qfalse;
 VectorSet(tangent,-direction[1],direction[0],0);
 for(forward=0;forward<=160&&ngTraces<limit-1500;forward+=16)for(side=-96;side<=96&&ngTraces<limit-1500;side+=16){
  ngNode candidate=*near,anchor=networkAnchor?*networkAnchor:*near;vec3_t p,d;float score;const char *why;qboolean valid;
  VectorMA(near->feet,(float)forward,direction,p);VectorMA(p,(float)side,tangent,p);
  VectorSubtract(p,entry->feet,d);score=DotProduct(d,d);if(score>=best)continue;
  NG_DoorPoseSet(pose,0.f,1);why=NG_Ground(p,candidate.feet,near->crouch?ngDuck:ngStand);if(why)continue;
  valid=NG_DoorAnchorWalk(pose,&candidate,&anchor);NG_DoorPoseSet(pose,0.f,1);if(!valid)continue;
  /* `near` is the opposite provisional USE entry, which may move sideways
   * when its FORCE direction is processed. It is not an ordinary exit
   * anchor and can be inside the opened leaf. Bind the exit to the exact
   * network seed that will actually receive the bidirectional ground edge. */
  valid=NG_DoorSweepFootClear(pose,candidate.feet,ngWidth,candidate.crouch?ngDuck:ngStand,2.f);
  NG_DoorPoseSet(pose,0.f,1);if(!valid)continue;
  why=NG_DoorPassage(pose,entry,&candidate);NG_DoorPoseSet(pose,0.f,1);if(why)continue;
  candidate.radius=candidate.requestedRadius=2.f;candidate.reason="generated_door_sweep_safe_exit_live_pending";*exit=candidate;best=score;
 }
 NG_DoorPoseRestore(pose);return best<1e19f;
}
static qboolean NG_DoorAnchorWalk(ngDoorPose *pose,ngNode *point,ngNode *anchor) {
 int sign,rotation=0,i,begin=ngTraces;qboolean valid=qfalse;float height=point->crouch||anchor->crouch?ngDuck:ngStand;
 /* Stance belongs to each endpoint, not to its neighbour. A ducked passage
  * can connect a standing USE to a crouched ENTRY without changing the
  * USE's saved eye height. Ordinary floor-guide attachment separately
  * proves the original authored anchor's standing body in FaceRoute. */
 NG_DoorPoseSet(pose,0.f,1);
 if(NG_Walk(point,anchor,height)||NG_Walk(anchor,point,height))goto done;
 for(i=0;i<pose->count;i++)if(!Q_stricmp(g_entities[pose->part[i].entity].classname,"func_door_rotating"))rotation=1;
 /* The antechamber route must remain usable after the leaf opens. A valid
  * closed-door approach alone can point through the opened rotating leaf. */
 for(sign=1;sign>=(rotation&&!NG_DoorFixedRotation(pose)?-1:1);sign-=2){NG_DoorPoseSet(pose,1.f,sign);if(NG_Walk(point,anchor,height)||NG_Walk(anchor,point,height))goto done;}
 valid=qtrue;
done:NG_DoorPoseRestore(pose);ngDoorProofTraces+=ngTraces-begin;return valid;
}
typedef struct {int index;float distance;} ngDoorNeighbor;
static int NG_DoorNeighborCompare(const void *aa,const void *bb){
 const ngDoorNeighbor *a=(const ngDoorNeighbor*)aa,*b=(const ngDoorNeighbor*)bb;
 if(a->distance<b->distance)return -1;if(a->distance>b->distance)return 1;return a->index-b->index;
}
static int NG_DoorNeighbors(const vec3_t point,int base,ngDoorNeighbor *nearest,int capacity) {
 int i,count=0;for(i=0;i<base;i++)if(ngNodes[i].accepted&&!ngNodes[i].navFlags){
  vec3_t d;float distance;int k;VectorSubtract(ngNodes[i].feet,point,d);distance=DotProduct(d,d);
  if(distance>384.f*384.f)continue;
  if(count<capacity){nearest[count].index=i;nearest[count++].distance=distance;}
  else{int worst=0;for(k=1;k<count;k++)if(nearest[k].distance>nearest[worst].distance)worst=k;
   if(distance<nearest[worst].distance){nearest[worst].index=i;nearest[worst].distance=distance;}}
 }
 qsort(nearest,count,sizeof(*nearest),NG_DoorNeighborCompare);return count;
}
static int NG_DoorConnectAnchors(ngDoorPose *pose,int index,int base,int limit) {
 ngDoorNeighbor nearest[24];int count=NG_DoorNeighbors(ngNodes[index].feet,base,nearest,24),i,connected=0;
 for(i=0;i<count&&connected<4&&ngTraces<limit-2500&&ngEdgeCount<NG_EDGES-2;i++){
  int target=nearest[i].index;ngNode point=ngNodes[index],anchor=ngNodes[target];qboolean valid;
  if(NG_HasEdge(index,target)&&NG_HasEdge(target,index)){connected++;continue;}
  valid=NG_DoorAnchorWalk(pose,&point,&anchor);NG_DoorPoseSet(pose,0.f,1);if(!valid)continue;
  ngNodes[index].crouch=point.crouch;
  NG_DoorLink(index,target,"door_anchor_closed_and_open_native_walk");NG_DoorLink(target,index,"door_anchor_closed_and_open_native_walk");connected++;
 }
 return connected;
}
static const char *NG_DoorRecipeIdentity(ngNode *node,ngDoorPose *pose) {
 ngDoorRecipe *r=&node->door;int team,buttonTeam=3;const char *why;
 if(!r->mode)return NULL;
 why=NG_DoorGroup(r->entity,pose,&team);if(why)return why;
 if(g_entities[r->entity].s.modelindex!=r->model)return "door_model_identity_changed";
 if(r->useEntity<MAX_CLIENTS||r->useEntity>=level.num_entities||
    !g_entities[r->useEntity].inuse||g_entities[r->useEntity].s.modelindex!=r->useModel)return "use_model_identity_changed";
 if(r->mode==1||r->mode==3){if(!NG_DoorContains(pose,r->useEntity)||r->mode==3&&r->useEntity!=r->entity)return "direct_use_is_not_door";
  if(g_entities[r->useEntity].spawnflags&1)return "direct_activation_disabled";
  if(g_entities[r->useEntity].spawnflags&128)return "direct_mount_priority_requires_review";
  if(r->role==2&&NG_DoorFixedRotation(pose)&&
     r->mode==1&&
     !NG_DoorSweepFootClear(pose,node->feet,ngWidth,node->crouch?ngDuck:ngStand,2.f))return "force_rotating_entry_inside_motion_sweep";
  if(r->mode==3&&r->role==1&&
     !NG_DoorSweepFootClear(pose,node->feet,ngWidth,node->crouch?ngDuck:ngStand,2.f))return "local_door_use_inside_motion_sweep";
 }
 else if(r->mode==2){why=NG_DoorRelay(r->useEntity,r->entity,&buttonTeam);if(why)return why;team&=buttonTeam;}
 else return "unknown_door_mode";
 if(!r->team||(r->team&~team))return "door_team_identity_changed";
 if(r->team!=3&&((node->authored?node->editFlags:node->navFlags)&(16u|3u))!=(unsigned int)(16|r->team))return "missing_door_team_only_flags";
 if(r->role<1||r->role>3)return "invalid_door_role";
 if((r->role!=3&&NG_Find(r->target)<0)||NG_Find(r->useSource)<0||NG_Find(r->entry)<0)return "door_recipe_missing_reference";
 if(r->role==1||r->mode==1&&r->role==2){qboolean aimed;
  if(r->mode==2&&!NG_DoorForceUseSafe(pose,node))return "force_rotating_switch_within_swept_envelope";
  NG_DoorPoseSet(pose,0.f,1);aimed=NG_DoorAim(node,r->useEntity,r->mode!=2,r->aim,qtrue);NG_DoorPoseRestore(pose);
  if(!aimed)return "use_aim_occluded_or_out_of_range";
 }
 return NULL;
}
static const char *NG_DoorEditorEdge(ngEdge *edge) {
 ngNode *a=&ngNodes[edge->from],*b=&ngNodes[edge->to];ngDoorPose pose;const char *why;
 if(!NG_DoorConditionalEdge(edge->from,edge->to))return NULL;
 why=NG_DoorRecipeIdentity(a,&pose);if(why)return why;
 if(!NG_DoorSweepFootClear(&pose,b->feet,ngWidth,b->crouch?ngDuck:ngStand,2.f))return "door_target_inside_motion_sweep";
 return NG_DoorPassage(&pose,a,b);
}
static qboolean NG_DoorManualFinalize(void) {
 int i,k;const char *why=NULL;qboolean valid=qtrue;
 for(i=0;i<ngCount;i++)if(ngNodes[i].door.mode&&ngNodes[i].accepted){
  ngNode *n=&ngNodes[i];ngDoorPose pose;int target=NG_Find(n->door.target),use=NG_Find(n->door.useSource),entry=NG_Find(n->door.entry);qboolean edge=qfalse;
  why=NG_DoorRecipeIdentity(n,&pose);
  if(!why&&n->door.role==3&&!NG_DoorSweepFootClear(&pose,n->feet,ngWidth,n->crouch?ngDuck:ngStand,2.f))why="door_exit_inside_motion_sweep";
  if(!why&&(use<0||entry<0||!ngNodes[use].accepted||!ngNodes[entry].accepted||
     (n->door.role!=3&&(target<0||!ngNodes[target].accepted))))why="door_recipe_rejected_reference";
  if(!why){const ngDoorRecipe *e=&ngNodes[entry].door,*u=&ngNodes[use].door;
   if(e->role!=2||e->entity!=n->door.entity||e->useEntity!=n->door.useEntity||
      e->mode!=n->door.mode||e->useSource!=n->door.useSource||e->entry!=n->door.entry||
      e->team!=n->door.team||!VectorCompare(e->aim,n->door.aim))why="inconsistent_door_entry_recipe";
   else if(n->door.mode==1&&(use!=entry||entry!=i&&n->door.role!=3))why="direct_use_must_be_entry";
   else if((n->door.mode==2||n->door.mode==3)&&(u->role!=1||u->entity!=n->door.entity||u->useEntity!=n->door.useEntity||
      u->mode!=n->door.mode||u->useSource!=ngNodes[use].id||u->entry!=n->door.entry||u->team!=n->door.team||
      !VectorCompare(u->aim,n->door.aim)))why="inconsistent_button_use_recipe";
   else if(n->door.mode==3&&u->target!=ngNodes[entry].id)why="local_door_use_target_is_not_entry";
   if(!why&&n->door.role==3&&e->target!=n->id)why="exit_is_not_entry_target";
  }
  if(!why&&n->door.role!=3){
   for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].from==i&&ngEdges[k].to==target&&ngEdges[k].accepted)edge=qtrue;
   if(!edge)why="door_recipe_directed_edge_missing";
  }
  if(!why&&(n->door.mode==2||n->door.mode==3)&&n->door.role==2&&
     (!NG_DoorOrdinaryReach(i,use,n->door.team)||!NG_DoorOrdinaryReach(use,i,n->door.team)))why="button_not_reachable_with_door_closed";
  if(!why&&n->door.mode==3&&n->door.role==2){ngDoorPose pose;ngNode a=ngNodes[entry],b=ngNodes[use];int team;
   why=NG_DoorGroup(n->door.entity,&pose,&team);
   if(!why&&!NG_DoorAnchorWalk(&pose,&a,&b))why="local_door_use_entry_ground_path_blocked";
   if(!why)for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted&&
      (ngEdges[k].to==entry&&ngEdges[k].from!=use||
       ngEdges[k].from==entry&&ngEdges[k].to!=use&&ngEdges[k].to!=target)){
    why="local_door_entry_requires_its_safe_use_approach";break;
   }
  }
  if(!why){n->door.validated=qtrue;continue;}
  n->accepted=qfalse;n->reason=why;valid=qfalse;NG_DoorCaseAdd(n->door.entity,n->door.useEntity,n->id,n->door.target,n->door.team,why);
 }
 if(!valid)for(k=0;k<ngEdgeCount;k++)if(!ngNodes[ngEdges[k].from].accepted||!ngNodes[ngEdges[k].to].accepted){ngEdges[k].accepted=qfalse;ngEdges[k].reason="door_recipe_rejected_endpoint";}
 return valid;
}
static int NG_DoorAppend(ngNode *n,int *maxid) {
 int index=ngCount++;n->id=++*maxid;n->index=-1;n->accepted=qtrue;n->topologyProtected=qtrue;
 n->reason="generated_door_approach_live_pending";
 n->radius=n->requestedRadius=8.f;VectorCopy(n->feet,n->raw);n->raw[2]+=ngOrigin;ngNodes[index]=*n;return index;
}
static void NG_DoorLink(int from,int to,const char *reason) {
 ngEdge *e;if(ngEdgeCount>=NG_EDGES||NG_HasEdge(from,to))return;
 e=&ngEdges[ngEdgeCount++];memset(e,0,sizeof(*e));e->from=from;e->to=to;e->accepted=e->added=qtrue;e->crouch=ngNodes[from].crouch||ngNodes[to].crouch;e->reason=reason;
 /* Linking never changes either endpoint. In particular, do not invalidate
  * an already bound USE aim by inheriting CROUCH from an adjacent floor. */
}
static qboolean NG_DoorApproach(const ngNode *seed,const trace_t *hit,ngNode *node) {
 vec3_t p;const char *why;memset(node,0,sizeof(*node));
 VectorMA(hit->endpos,ngWidth+6.f,hit->plane.normal,p);p[2]=seed->feet[2];
 why=NG_Ground(p,node->feet,ngStand);if(why){why=NG_Ground(p,node->feet,ngDuck);node->crouch=qtrue;}
 if(why)return qfalse;{ngNode from=*seed;return NG_BothWays(&from,node);}
}
static qboolean NG_DoorForceEntry(ngDoorPose *pose,const ngNode *seed,const trace_t *hit,ngNode *entry,int limit,const ngNode *stagedEntry) {
 vec3_t tangent;int step,away;VectorSet(tangent,-hit->plane.normal[1],hit->plane.normal[0],0);
 /* Close to the latch/hinge a real use ray can reach the closed leaf while
  * the complete actor stays outside its fixed swing. Search bounded nearby
  * floors, never turn an unsafe near-face point into a claimed wait proof. */
 for(step=0;step<=12&&ngTraces<limit-2500;step++)for(away=0;away<=16&&ngTraces<limit-2500;away+=8){
  float side=step?(float)((step+1)/2)*16.f*(step&1?1.f:-1.f):0.f;
  ngNode candidate=*entry,anchor=*seed;vec3_t p,aim;const char *why;qboolean clear;
  VectorMA(entry->feet,side,tangent,p);VectorMA(p,(float)away,hit->plane.normal,p);
  NG_DoorPoseSet(pose,0.f,1);why=NG_Ground(p,candidate.feet,ngStand);candidate.crouch=qfalse;
  if(why){why=NG_Ground(p,candidate.feet,ngDuck);candidate.crouch=qtrue;}if(why)continue;
  if(!NG_DoorAim(&candidate,hit->entityNum,qtrue,aim,qfalse))continue;
  clear=NG_DoorSweepFootClear(pose,candidate.feet,ngWidth,candidate.crouch?ngDuck:ngStand,2.f);
  NG_DoorPoseSet(pose,0.f,1);if(!clear)continue;
  clear=NG_DoorAnchorWalk(pose,&candidate,&anchor);NG_DoorPoseSet(pose,0.f,1);if(!clear)continue;
  if(stagedEntry){anchor=*stagedEntry;clear=NG_DoorAnchorWalk(pose,&candidate,&anchor);NG_DoorPoseSet(pose,0.f,1);if(!clear)continue;}
  *entry=candidate;return qtrue;
 }
 return qfalse;
}
typedef struct {ngNode point,bend;int anchor;qboolean bent;} ngDoorFaceRoute;
static qboolean NG_DoorFaceRoute(const vec3_t position,int base,ngDoorFaceRoute *route,int limit) {
 ngDoorNeighbor nearest[12];ngNode floor;int count,i,turn;const char *why;
 memset(&floor,0,sizeof(floor));why=NG_Ground(position,floor.feet,ngStand);
 if(why){why=NG_Ground(position,floor.feet,ngDuck);floor.crouch=qtrue;}if(why)return qfalse;
 count=NG_DoorNeighbors(floor.feet,base,nearest,12);if(count>6)count=6;
 for(turn=0;turn<3;turn++)for(i=0;i<count&&ngTraces<limit-4000;i++){
  ngNode point=floor,anchor=ngNodes[nearest[i].index],bend;float height=anchor.crouch?ngDuck:ngStand;vec3_t p;
  /* A new low approach cannot change an authored anchor's stance. Keep
   * its actual standing clearance, or use its already-authored crouch. */
  if(point.crouch&&!anchor.crouch)continue;point.crouch=anchor.crouch;
  if(!turn){if(NG_Walk(&anchor,&point,height)||NG_Walk(&point,&anchor,height))continue;}
  else{
   memset(&bend,0,sizeof(bend));VectorCopy(point.feet,p);p[turn==1?0:1]=anchor.feet[turn==1?0:1];
   if(NG_Ground(p,bend.feet,height))continue;bend.crouch=anchor.crouch;
   if(NG_Walk(&anchor,&bend,height)||NG_Walk(&bend,&anchor,height)||
      NG_Walk(&bend,&point,height)||NG_Walk(&point,&bend,height))continue;
  }
  route->point=point;route->anchor=nearest[i].index;route->bent=turn?qtrue:qfalse;
  if(turn)route->bend=bend;return qtrue;
 }
 return qfalse;
}
static qboolean NG_DoorFaceStage(ngDoorPose *pose,const vec3_t centre,int axis,int sample,int base,
 ngDoorNeighbor *seeds,int *maxid,int limit,qboolean *bootstrap) {
 static const float along[]={.5f,.25f,.75f,0.f,1.f};
 ngDoorNeighbor nearest[24];float heights[2];int count,heightCount=0,i,side,other=1-axis;
 ngDoorFaceRoute routes[2];vec3_t points[2],start,end;trace_t hit;
 *bootstrap=qfalse;
 if(ngCount>NG_NODES-12||ngEdgeCount>NG_EDGES-80||ngTraces>=limit-6000)return qfalse;
 NG_DoorPoseSet(pose,0.f,1);count=NG_DoorNeighbors(centre,base,nearest,24);
 for(i=0;i<count&&heightCount<2;i++){
  float z=ngNodes[nearest[i].index].feet[2];
  if(fabs(z-centre[2])>64.f||heightCount&&fabs(z-heights[0])<18.f)continue;heights[heightCount++]=z;
 }
 if(sample/5>=heightCount)return qfalse;
 for(side=0;side<2;side++){
  VectorCopy(centre,points[side]);points[side][2]=heights[sample/5];
  points[side][axis]=side?g_entities[pose->master].r.absmax[axis]+ngWidth+32.f:
   g_entities[pose->master].r.absmin[axis]-ngWidth-32.f;
  points[side][other]=g_entities[pose->master].r.absmin[other]+
   along[sample%5]*(g_entities[pose->master].r.absmax[other]-g_entities[pose->master].r.absmin[other]);
 }
 /* These are proposals from the actual closed bounds, not assumed portal
  * normals. Both native rays must identify this real door leaf first. */
 for(side=0;side<2;side++){
  VectorCopy(points[side],start);VectorCopy(points[1-side],end);start[2]+=ngOrigin+16.f;end[2]+=ngOrigin+16.f;
  if(ngTraces>=limit-6000)return qfalse;trap_Trace(&hit,start,NULL,NULL,end,ENTITYNUM_NONE,NG_MASK);ngTraces++;
  if(hit.startsolid||hit.allsolid||!NG_DoorContains(pose,hit.entityNum)||fabs(hit.plane.normal[2])>.25f)return qfalse;
 }
 for(side=0;side<2;side++){
  int away;qboolean found=qfalse,fallback=qfalse;ngDoorFaceRoute unanchored;
  /* Face seeds are ordinary waiting/egress floors, distinct from the close
   * USE entry. Keep their complete body outside every actual leaf pose. */
  for(away=0;away<=64&&ngTraces<limit-4000;away+=16){
   const char *why;ngDoorFaceRoute *route=&routes[side];vec3_t point;
   VectorCopy(points[side],point);point[axis]+=(side?1.f:-1.f)*(float)away;
   if(!NG_DoorFaceRoute(point,base,route,limit)){
    /* An empty side gains its first connection only from both complete
     * door recipes. No ordinary edge to a missing anchor is fabricated. */
    memset(route,0,sizeof(*route));route->anchor=-1;
    why=NG_Ground(point,route->point.feet,ngStand);
    if(why){why=NG_Ground(point,route->point.feet,ngDuck);route->point.crouch=qtrue;}
    if(why)continue;
   }
   found=NG_DoorSweepFootClear(pose,route->point.feet,ngWidth,route->point.crouch?ngDuck:ngStand,2.f);
   NG_DoorPoseSet(pose,0.f,1);if(!found)continue;
   if(route->anchor>=0)break;
   /* A tight unanchored duck floor must not conceal a slightly farther
    * stand-capable connection to the authored network. Keep this safe
    * empty-room proposal only as fallback within the same bounded search. */
   if(!fallback){unanchored=*route;fallback=qtrue;}found=qfalse;
  }
  if(!found&&fallback){routes[side]=unanchored;found=qtrue;}
  if(!found)return qfalse;
 }
 if(routes[0].anchor<0&&routes[1].anchor<0)return qfalse;
 *bootstrap=routes[0].anchor<0||routes[1].anchor<0?qtrue:qfalse;
 for(side=0;side<2;side++){
  int anchor=routes[side].anchor,index;
  if(routes[side].bent){index=NG_DoorAppend(&routes[side].bend,maxid);
   ngNodes[index].radius=ngNodes[index].requestedRadius=35.f;
   NG_DoorLink(index,anchor,"door_approach_native_walk");NG_DoorLink(anchor,index,"door_approach_native_walk");anchor=index;
  }
  index=NG_DoorAppend(&routes[side].point,maxid);ngNodes[index].radius=ngNodes[index].requestedRadius=35.f;
  if(anchor>=0){NG_DoorLink(index,anchor,"door_approach_native_walk");NG_DoorLink(anchor,index,"door_approach_native_walk");}
  seeds[side].index=index;seeds[side].distance=0.f;
 }
 return qtrue;
}
static qboolean NG_DoorSwitch(ngDoorPose *pose,int door,int seed,ngNode *entry,ngNode *usePoint,int *useAnchor,int *useEntity,int *team,vec3_t aim,int limit) {
 static float forward[2][NG_NODES],backward[2][NG_NODES];int entity,j,d,baseTeam=*team;float best=1e20f;
 NG_DoorDistances(seed,1,qfalse,forward[0]);NG_DoorDistances(seed,1,qtrue,backward[0]);
 NG_DoorDistances(seed,2,qfalse,forward[1]);NG_DoorDistances(seed,2,qtrue,backward[1]);
 *useEntity=-1;
 for(entity=MAX_CLIENTS;entity<level.num_entities&&ngTraces<limit-1000;entity++){
  int relayTeam;const char *why=NG_DoorRelay(entity,door,&relayTeam);gentity_t *use=&g_entities[entity];
  if(why||!(relayTeam&baseTeam))continue;
  for(j=0;j<ngCount&&ngTraces<limit-1000;j++)if(ngNodes[j].accepted&&!ngNodes[j].navFlags){
   vec3_t centre,delta;float dist;int reachable=(forward[0][j]<1e19f&&backward[0][j]<1e19f?1:0)|(forward[1][j]<1e19f&&backward[1][j]<1e19f?2:0);
   reachable&=relayTeam&baseTeam;if(!reachable)continue;
   VectorAdd(use->r.absmin,use->r.absmax,centre);VectorScale(centre,.5f,centre);VectorSubtract(ngNodes[j].feet,centre,delta);delta[2]=0;dist=DotProduct(delta,delta);
   if(dist>256.f*256.f)continue;
   for(d=0;d<8&&ngTraces<limit-1000;d++){
    ngNode candidate,anchor=ngNodes[j];vec3_t p,localAim;const char *bad;float angle=(float)d*(float)M_PI/4.f,score=0.f,localLength;int side;
    memset(&candidate,0,sizeof(candidate));VectorCopy(centre,p);p[0]+=cos(angle)*32.f;p[1]+=sin(angle)*32.f;p[2]=anchor.feet[2];
    bad=NG_Ground(p,candidate.feet,ngStand);if(bad){bad=NG_Ground(p,candidate.feet,ngDuck);candidate.crouch=qtrue;}
    if(bad||!NG_BothWays(&anchor,&candidate)||!NG_DoorAim(&candidate,entity,qfalse,localAim,qfalse)||!NG_DoorForceUseSafe(pose,&candidate))continue;
    VectorSubtract(candidate.feet,anchor.feet,delta);localLength=VectorLength(delta);
    /* The return after activation is timed by the real door. Choose the
     * shortest existing directed ground route, not the first circle angle
     * or the nearest waypoint to the button centre. No speed/wait override. */
    for(side=0;side<2;side++)if(reachable&(1<<side))if(backward[side][j]>score)score=backward[side][j];
    score+=localLength;VectorSubtract(entry->feet,ngNodes[seed].feet,delta);score+=VectorLength(delta);
    if(score>=best)continue;
    *usePoint=candidate;*useAnchor=j;*useEntity=entity;*team=reachable;VectorCopy(localAim,aim);best=score;
   }
  }
 }
 return *useEntity>=0;
}
static void NG_DoorFill(ngNode *node,int door,int use,int target,int mode,int team,int role,int useSource,int entry,const vec3_t aim) {
 ngDoorRecipe *r=&node->door;r->entity=door;r->useEntity=use;r->target=target;r->mode=mode;r->team=team;r->role=role;
 r->model=g_entities[door].s.modelindex;r->useModel=g_entities[use].s.modelindex;r->useSource=useSource;r->entry=entry;r->validated=qtrue;VectorCopy(aim,r->aim);
 node->navFlags|=NG_DOOR_FLAG;if(team!=3)node->navFlags|=(unsigned int)(team|16);
 node->reason=role==1?(mode==3?"generated_door_local_use_live_pending":"generated_door_button_use_live_pending"):(role==2?"generated_door_entry_live_pending":"generated_door_exit_live_pending");
}
static void NG_Doors(void) {
 int number,i,j,maxid=0,base=ngCount,limit=ngTraces+NG_DOOR_TRACES;
 ngDoorRepairRun=ngDoorRepairExisting;
 ngDoorCaseCount=ngDoorCandidates=ngDoorLinks=ngDoorProofTraces=0;ngDoorTraceStart=ngTraces;ngDoorStop="door_scan_complete";
 if(limit>NG_TRACE_BUDGET-20000)limit=NG_TRACE_BUDGET-20000;
 for(i=0;i<ngCount;i++)if(ngNodes[i].id>maxid)maxid=ngNodes[i].id;
 /* Reserve enough signed source IDs for the entire bounded node pool.
  * Authored drafts can have deleted their former highest ID; their saved
  * next_id floor, not merely this surviving maximum, must remain monotonic. */
 if(maxid>0x7fffffff-NG_NODES){ngDoorStop="door_source_id_budget";return;}
 if(ngDoorRepairExisting&&ngDoorMinimumId>maxid+1)maxid=ngDoorMinimumId-1;
 for(number=MAX_CLIENTS;number<level.num_entities;number++)if(NG_DoorMaster(number)==number){
  ngDoorPose pose;const char *why;int team,pairs=0,pairLimit=128,buttonEntity=-1,axis,ii,jj,seedCount;
  int faceSample=0,faceNodeBase=ngCount,faceEdgeBase=ngEdgeCount;ngDoorNeighbor seeds[32];vec3_t centre;qboolean generated=qfalse,force=qfalse,faceBootstrap=qfalse;
  if(ngDoorRepairExisting){
   qboolean present=qfalse;for(i=0;i<base;i++)if(ngNodes[i].accepted&&ngNodes[i].door.validated&&
    ngNodes[i].door.role==2&&NG_DoorMaster(ngNodes[i].door.entity)==number){present=qtrue;break;}
   if(present){NG_DoorCaseAdd(number,-1,-1,-1,0,"existing_authored_door_recipe_preserved");continue;}
  }
  why=NG_DoorGroup(number,&pose,&team);if(why){NG_DoorCaseAdd(number,-1,-1,-1,0,why);continue;}
  for(i=MAX_CLIENTS;i<level.num_entities;i++){int buttonTeam;if(!NG_DoorRelay(i,number,&buttonTeam)&&(buttonTeam&team)){buttonEntity=i;break;}}
  for(i=0;i<pose.count;i++)if(!Q_stricmp(g_entities[pose.part[i].entity].classname,"func_door_rotating")&&g_entities[pose.part[i].entity].spawnflags&32)force=qtrue;
  NG_DoorPoseSet(&pose,0.f,1);
  axis=(g_entities[number].r.absmax[0]-g_entities[number].r.absmin[0]<g_entities[number].r.absmax[1]-g_entities[number].r.absmin[1])?0:1;
  VectorAdd(g_entities[number].r.absmin,g_entities[number].r.absmax,centre);VectorScale(centre,.5f,centre);centre[2]=g_entities[number].r.absmin[2];
  seedCount=NG_DoorNeighbors(centre,base,seeds,32);
door_seed_search:
  for(ii=0;ii<seedCount&&!generated&&pairs<pairLimit;ii++){i=seeds[ii].index;
   for(jj=0;jj<seedCount&&!generated&&pairs<pairLimit;jj++){j=seeds[jj].index;if(i!=j){
    ngNode a,b,portal[2],usePoints[2],exitPoints[2];int anchors[2],uses[2],modes[2],teams[2]={team,team},indices[2],useIndices[2],side;vec3_t starts[2],ends[2],aims[2],d;trace_t hits[2];qboolean available[2];
    VectorSubtract(ngNodes[i].feet,ngNodes[j].feet,d);if(VectorLength(d)>512.f||fabs(d[2])>64.f)continue;
    if((ngNodes[i].feet[axis]-centre[axis])*(ngNodes[j].feet[axis]-centre[axis])>=0.f)continue;
    if(ngTraces>=limit||ngCount>NG_NODES-8||ngEdgeCount>NG_EDGES-64){ngDoorStop="door_candidate_or_trace_budget";break;}pairs++;
    ngDoorCandidates++;
    VectorCopy(ngNodes[i].feet,starts[0]);VectorCopy(ngNodes[j].feet,ends[0]);starts[0][2]+=ngOrigin+16.f;ends[0][2]+=ngOrigin+16.f;
    trap_Trace(&hits[0],starts[0],NULL,NULL,ends[0],ENTITYNUM_NONE,NG_MASK);ngTraces++;
    if(hits[0].startsolid||hits[0].allsolid||!NG_DoorContains(&pose,hits[0].entityNum)||fabs(hits[0].plane.normal[2])>.25f)continue;
    VectorCopy(ends[0],starts[1]);VectorCopy(starts[0],ends[1]);trap_Trace(&hits[1],starts[1],NULL,NULL,ends[1],ENTITYNUM_NONE,NG_MASK);ngTraces++;
    if(hits[1].startsolid||hits[1].allsolid||!NG_DoorContains(&pose,hits[1].entityNum)||fabs(hits[1].plane.normal[2])>.25f)continue;
    if(!NG_DoorApproach(&ngNodes[i],&hits[0],&a)||!NG_DoorApproach(&ngNodes[j],&hits[1],&b))continue;
    NG_DoorPoseRestore(&pose);why=NG_DoorPassage(&pose,&a,&b);NG_DoorPoseSet(&pose,0.f,1);if(why)continue;
    {ngNode anchor=ngNodes[i];qboolean safe=NG_DoorAnchorWalk(&pose,&a,&anchor);NG_DoorPoseSet(&pose,0.f,1);if(!safe)continue;
     anchor=ngNodes[j];safe=NG_DoorAnchorWalk(&pose,&b,&anchor);NG_DoorPoseSet(&pose,0.f,1);if(!safe)continue;}
    portal[0]=a;portal[1]=b;
    for(side=0;side<2;side++){
     ngNode *entry=side?&b:&a;int seed=side?j:i;
     modes[side]=1;
     if(force&&buttonEntity<0&&!NG_DoorForceEntry(&pose,&ngNodes[seed],&hits[side],entry,limit,NULL)){
      available[side]=qfalse;NG_DoorCaseAdd(number,hits[side].entityNum,-1,-1,teams[side],"no_force_rotating_sweep_safe_use_floor");continue;
     }
     available[side]=NG_DoorSafeExit(&pose,entry,&portal[1-side],&ngNodes[side?i:j],&exitPoints[side],limit);NG_DoorPoseSet(&pose,0.f,1);
     if(!available[side]&&force&&buttonEntity<0&&hits[side].entityNum==number){
      /* A swept-safe local USE can lie beside the jamb. Keep the original
       * aligned passage ENTRY and prove a separate closed/open ground leg
       * from that genuine direct USE, rather than a wall-crossing chord. */
      usePoints[side]=portal[side];
      if(NG_DoorForceEntry(&pose,&ngNodes[seed],&hits[side],&usePoints[side],limit,&portal[side])){
       *entry=portal[side];modes[side]=3;
       available[side]=NG_DoorSafeExit(&pose,entry,&portal[1-side],&ngNodes[side?i:j],&exitPoints[side],limit);
       NG_DoorPoseSet(&pose,0.f,1);
      }
     }
     if(!available[side]){NG_DoorCaseAdd(number,-1,-1,-1,teams[side],"no_complete_sweep_safe_exit");continue;}
     uses[side]=hits[side].entityNum;if(modes[side]==1)usePoints[side]=*entry;
     /* Preserve an explicit button binding. Its inaccessible opposite side
      * is not silently converted into a made-up remote-use return route. */
     if(buttonEntity>=0){modes[side]=2;available[side]=NG_DoorSwitch(&pose,number,seed,entry,&usePoints[side],&anchors[side],&uses[side],&teams[side],aims[side],limit);}
     else available[side]=TCE_NavDoorUseMatches(uses[side],number,g_entities[number].s.modelindex)&&NG_DoorAim(modes[side]==3?&usePoints[side]:entry,uses[side],qtrue,aims[side],qfalse);
    }
    /* A newly discovered empty side must be enterable and leaveable. A
     * lone outward action cannot attach it to the authored network, and a
     * lone inward action would create a room with no proved return route. */
    if(faceBootstrap&&(!available[0]||!available[1]))continue;
    if(!available[0]&&!available[1])continue;
    NG_DoorPoseRestore(&pose);
    indices[0]=NG_DoorAppend(&a,&maxid);indices[1]=NG_DoorAppend(&b,&maxid);
    if(modes[0]!=3){NG_DoorLink(i,indices[0],"door_approach_native_walk");NG_DoorLink(indices[0],i,"door_approach_native_walk");NG_DoorConnectAnchors(&pose,indices[0],base,limit);}
    if(modes[1]!=3){NG_DoorLink(j,indices[1],"door_approach_native_walk");NG_DoorLink(indices[1],j,"door_approach_native_walk");NG_DoorConnectAnchors(&pose,indices[1],base,limit);}
    NG_DoorPoseSet(&pose,0.f,1);
    for(side=0;side<2;side++)if(available[side]){
     int from=indices[side],to=NG_DoorAppend(&exitPoints[side],&maxid);
     ngNodes[to].radius=ngNodes[to].requestedRadius=2.f;useIndices[side]=from;
     if(modes[side]==2||modes[side]==3){int useIndex=NG_DoorAppend(&usePoints[side],&maxid),target=modes[side]==3?from:anchors[side];useIndices[side]=useIndex;
      NG_DoorFill(&ngNodes[useIndex],number,uses[side],ngNodes[target].id,modes[side],teams[side],1,ngNodes[useIndex].id,ngNodes[from].id,aims[side]);
      NG_DoorLink(useIndex,target,modes[side]==3?"local_door_use_entry_native_walk":"button_use_native_walk");NG_DoorLink(target,useIndex,modes[side]==3?"local_door_use_entry_native_walk":"button_use_native_walk");
      if(modes[side]==3){int seed=side?j:i;NG_DoorLink(useIndex,seed,"local_door_use_network_native_walk");NG_DoorLink(seed,useIndex,"local_door_use_network_native_walk");}
      NG_DoorConnectAnchors(&pose,useIndex,base,limit);NG_DoorPoseSet(&pose,0.f,1);
     }
     NG_DoorFill(&ngNodes[from],number,uses[side],ngNodes[to].id,modes[side],teams[side],2,ngNodes[useIndices[side]].id,ngNodes[from].id,aims[side]);
     NG_DoorFill(&ngNodes[to],number,uses[side],-1,modes[side],teams[side],3,ngNodes[useIndices[side]].id,ngNodes[from].id,aims[side]);
     /* SafeExit proved both ordinary directions to this precise network
      * anchor. Keep the staged face/bend chain attached after crossing. */
     NG_DoorLink(to,side?i:j,"door_exit_network_native_walk");NG_DoorLink(side?i:j,to,"door_exit_network_native_walk");
     NG_DoorConnectAnchors(&pose,to,base,limit);NG_DoorPoseSet(&pose,0.f,1);
     NG_DoorLink(from,to,"door_open_pose_native_walk_live_pending");ngDoorLinks++;
     NG_DoorCaseAdd(number,uses[side],ngNodes[from].id,ngNodes[to].id,teams[side],"closed_identity_open_pose_native_walk_live_pending");
    }
    for(side=0;side<2;side++)if(!available[side])NG_DoorCaseAdd(number,-1,ngNodes[indices[side]].id,-1,teams[side],"reverse_activation_or_sweep_safe_exit_not_proved");
    generated=qtrue;NG_DoorPoseSet(&pose,0.f,1);
   }
   }
  }
  NG_DoorPoseRestore(&pose);
  while(!generated&&faceSample<10&&pairs<256&&ngTraces<limit-6000){
   /* Unsuccessful proposals leave no guides. At least one side connects
    * to an authored floor; an empty other side needs both door recipes. */
   ngCount=faceNodeBase;ngEdgeCount=faceEdgeBase;
   if(NG_DoorFaceStage(&pose,centre,axis,faceSample++,base,seeds,&maxid,limit,&faceBootstrap)){
    seedCount=2;pairLimit=256;goto door_seed_search;
   }
  }
  if(!generated){ngCount=faceNodeBase;ngEdgeCount=faceEdgeBase;}
  NG_DoorPoseRestore(&pose);
  if(!generated)NG_DoorCaseAdd(number,-1,-1,-1,team,"no_proved_approach_activation_and_open_passage");
  if(ngTraces>=limit){ngDoorStop="door_trace_budget";break;}
 }
}
/* Explicit repair only: use the current authored floors/edges, preserve
 * existing validated door actions, and allocate new IDs above this draft.
 * Ordinary native checks must continue to call ManualFinalize alone. */
static qboolean NG_DoorRepair(int minimumId) {
 int i;
 if(minimumId<0||minimumId>0x7fffffff-NG_NODES)return qfalse;
 for(i=0;i<ngCount;i++)if(ngNodes[i].id>0x7fffffff-NG_NODES)return qfalse;
 ngDoorMinimumId=minimumId;ngDoorRepairExisting=qtrue;NG_Doors();ngDoorRepairExisting=qfalse;
 return qtrue;
}
static qboolean NG_DoorApproachCandidate(int index) {
 const ngNode *n;
 if(index<0||index>=ngCount)return qfalse;n=&ngNodes[index];
 return n->accepted&&n->topologyProtected&&n->radius>=10.f&&!n->door.mode&&n->reason&&
  !strcmp(n->reason,"generated_door_approach_live_pending")?qtrue:qfalse;
}
static qboolean NG_DoorApproachBound(int index,qboolean reverse) {
 static int queue[NG_NODES];static unsigned char seen[NG_NODES];int head=0,tail=0,k;
 memset(seen,0,sizeof(seen));seen[index]=1;queue[tail++]=index;
 while(head<tail){int current=queue[head++];
  for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted){
   int from=reverse?ngEdges[k].to:ngEdges[k].from,to=reverse?ngEdges[k].from:ngEdges[k].to;
   if(from!=current||to<0||to>=ngCount||!ngNodes[to].accepted)continue;
   if(ngNodes[to].door.mode&&ngNodes[to].door.validated)return qtrue;
   if(!seen[to]&&NG_DoorApproachCandidate(to)){seen[to]=1;queue[tail++]=to;}
  }
 }
 return qfalse;
}
/* Only retained ordinary approaches with actual directed bindings to a
 * validated door recipe survive action-sample cleanup or enter the report. */
static qboolean NG_DoorApproachGuide(int index) {
 return NG_DoorApproachCandidate(index)&&NG_DoorApproachBound(index,qfalse)&&NG_DoorApproachBound(index,qtrue)?qtrue:qfalse;
}
static void NG_DoorReport(void) {
 int i;NG_Text(va("\"door_generation\":{\"model\":\"scoped_binary_mover_collision_poses_v1\",\"explicit_repair\":%s,\"minimum_new_source_id\":%d,\"existing_recipe_policy\":\"preserve_validated_door_masters\",\"candidates\":%d,\"conditional_links\":%d,\"passage_proof_traces\":%d,\"trace_budget\":%d,\"stop_reason\":\"%s\",\"scripts_executed\":false,\"live_verified\":false,\"cases\":[",ngDoorRepairRun?"true":"false",ngDoorRepairRun?ngDoorMinimumId:0,ngDoorCandidates,ngDoorLinks,ngDoorProofTraces,NG_DOOR_TRACES,ngDoorStop?ngDoorStop:"manual_recipe_validation"));
 for(i=0;i<ngDoorCaseCount;i++){const ngDoorCase *c=&ngDoorCases[i];NG_Text(va("%s{\"entity\":%d,\"use_entity\":%d,\"from\":%d,\"to\":%d,\"team\":%d,\"reason\":\"%s\"",i?",":"",c->entity,c->useEntity,c->from,c->to,c->team,c->reason));
  if(c->entity>=MAX_CLIENTS&&c->entity<level.num_entities){const gentity_t *ent=&g_entities[c->entity];NG_Text(va(",\"position\":[%.4f,%.4f,%.4f],\"coordinate_space\":\"world\"",(ent->r.absmin[0]+ent->r.absmax[0])*.5f,(ent->r.absmin[1]+ent->r.absmax[1])*.5f,(ent->r.absmin[2]+ent->r.absmax[2])*.5f));}NG_Text("}");
 }
 NG_Text("],\"approach_source_ids\":[");
 {int count=0;for(i=0;i<ngCount;i++)if(NG_DoorApproachGuide(i))NG_Text(va("%s%d",count++?",":"",ngNodes[i].id));}
 NG_Text("]},");
}
static void NG_DoorNodeReport(int index) {
 const ngNode *n=&ngNodes[index];const ngDoorRecipe *r=&n->door;
 if(!r->mode)return;
 NG_Text(va(",\"door_recipe\":{\"entity\":%d,\"use_entity\":%d,\"target_source_id\":%d,\"mode\":%d,\"team\":%d,\"role\":%d,\"model\":%d,\"use_model\":%d,\"use_source_id\":%d,\"entry_source_id\":%d,\"aim\":[%.4f,%.4f,%.4f],\"validated\":%s,\"exported\":%s}",r->entity,r->useEntity,r->target,r->mode,r->team,r->role,r->model,r->useModel,r->useSource,r->entry,r->aim[0],r->aim[1],r->aim[2],r->validated?"true":"false",n->accepted&&r->validated?"true":"false"));
}
