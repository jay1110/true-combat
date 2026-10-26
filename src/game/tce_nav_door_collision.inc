/* Shared door collision/activation predicates. They never execute a mover,
 * callback, script or game frame. The including unit owns its trace counters. */
#define NG_DOOR_GROUP 32
#define NG_DOOR_SWEEP_STEPS 180
typedef struct {int entity;entityShared_t shared;} ngDoorSaved;
typedef struct {int count,master;ngDoorSaved part[NG_DOOR_GROUP];} ngDoorPose;
void target_relay_use(gentity_t *self,gentity_t *other,gentity_t *activator);
void use_invisible_user(gentity_t *ent,gentity_t *other,gentity_t *activator);
static qboolean NG_DoorClass(const gentity_t *ent) {
 return ent->classname&&(!Q_stricmp(ent->classname,"func_door")||!Q_stricmp(ent->classname,"func_door_rotating"));
}
static int NG_DoorMaster(int number) {
 gentity_t *ent;if(number<MAX_CLIENTS||number>=level.num_entities)return -1;
 ent=&g_entities[number];if(!ent->inuse||!NG_DoorClass(ent))return -1;
 if(ent->teammaster){int master=(int)(ent->teammaster-g_entities);if(master<MAX_CLIENTS||master>=level.num_entities)return -1;return master;}
 return number;
}
static int NG_DoorTeam(const gentity_t *ent) {
 /* Disguised access is conditional on a client, never a permanent team gate. */
 return ent->allowteams?(ent->allowteams&3):3;
}
static qboolean NG_DoorFixedRotation(const ngDoorPose *pose) {
 const gentity_t *master=&g_entities[pose->master];
 /* Use_BinaryMover asks IsBinaryMoverBlocked only on the master. FORCE
  * skips its angle reversal; a slave's flag cannot fix the master's sign. */
 return !Q_stricmp(master->classname,"func_door_rotating")&&(master->spawnflags&32)?qtrue:qfalse;
}
static void NG_DoorPoseSet(const ngDoorPose *pose,float fraction,int rotationSign) {
 int i;for(i=0;i<pose->count;i++){
  gentity_t *ent=&g_entities[pose->part[i].entity];vec3_t delta;
  trap_UnlinkEntity(ent);
  if(!Q_stricmp(ent->classname,"func_door_rotating")){
   /* SP_func_door_rotating restores the pivot from s.origin after InitMover;
    * its pos1 is not the rotation origin and can remain zero. */
   VectorCopy(pose->part[i].shared.currentOrigin,ent->r.currentOrigin);VectorScale(ent->rotate,ent->angle*fraction*rotationSign,ent->r.currentAngles);
  }else{
   VectorSubtract(ent->pos2,ent->pos1,delta);VectorMA(ent->pos1,fraction,delta,ent->r.currentOrigin);
   VectorCopy(pose->part[i].shared.currentAngles,ent->r.currentAngles);
  }
  trap_LinkEntity(ent);
 }
}
static void NG_DoorPoseRestore(const ngDoorPose *pose) {
 int i;for(i=0;i<pose->count;i++){
  gentity_t *ent=&g_entities[pose->part[i].entity];
  trap_UnlinkEntity(ent);ent->r=pose->part[i].shared;
  if(ent->r.linked)trap_LinkEntity(ent);
  /* Link recomputes engine bounds and increments linkcount. Restore all
   * shared bookkeeping too; engine's spatial link now has the original pose. */
  ent->r=pose->part[i].shared;
 }
}
static const char *NG_DoorRelay(int useNumber,int doorNumber,int *team) {
 int current=useNumber,depth,seen[9],count;gentity_t *ent;
 *team=3;
 if(current<MAX_CLIENTS||current>=level.num_entities)return "unknown_use_entity";
 ent=&g_entities[current];
 if(!ent->inuse||!ent->r.linked||!ent->classname||!ent->use||ent->takedamage)return "unusable_button";
 if(Q_stricmp(ent->classname,"func_button")&&Q_stricmp(ent->classname,"func_invisible_user"))return "unsupported_use_class";
 if(!Q_stricmp(ent->classname,"func_button")&&ent->use!=Use_BinaryMover)return "unknown_button_callback";
 if(!Q_stricmp(ent->classname,"func_invisible_user")&&ent->use!=use_invisible_user)return "unknown_button_callback";
 if(ent->scriptName&&*ent->scriptName)return "scripted_button_requires_review";
 if(!Q_stricmp(ent->classname,"func_invisible_user")&&(ent->spawnflags&1))return "button_disabled";
 *team&=NG_DoorTeam(ent);
 for(depth=0;depth<8;depth++){
  int i,next=-1;qboolean allDoors=qtrue;
  seen[depth]=current;ent=&g_entities[current];
  if(!ent->target||!*ent->target)return "button_has_no_target";
  count=0;for(i=MAX_CLIENTS;i<level.num_entities;i++){
   gentity_t *candidate=&g_entities[i];
   if(candidate->inuse&&candidate->targetname&&!Q_stricmp(candidate->targetname,ent->target)) {
    next=i;count++;if(NG_DoorMaster(i)!=NG_DoorMaster(doorNumber))allDoors=qfalse;
   }
  }
  /* A fan-out can execute arbitrary script effects. Do not infer its
   * result from one matching door among several activated entities. */
  if(count&&allDoors)return *team?NULL:"button_team_disallows_all";
  if(count!=1)return "ambiguous_or_missing_button_target";
  ent=&g_entities[next];
  if(!ent->classname||Q_stricmp(ent->classname,"target_relay")||ent->use!=target_relay_use||
     ent->key||ent->spawnflags&~3||ent->scriptName&&*ent->scriptName)return "unsupported_relay_action";
  if(ent->spawnflags&1)*team&=1;if(ent->spawnflags&2)*team&=2;
  if(!*team)return "relay_team_disallows_all";
  for(i=0;i<=depth;i++)if(seen[i]==next)return "relay_cycle";
  current=next;
 }
 return "relay_depth_budget";
}
static qboolean NG_DoorSweepFootClear(ngDoorPose *pose,const vec3_t feet,float width,float height,float tolerance) {
 int i,k,sign,sample,steps=1,rotation=0,begin=ngTraces;float motion=0.f,pad;vec3_t mins,maxs;
 qboolean clear=qfalse;
 for(i=0;i<pose->count;i++){
  gentity_t *part=&g_entities[pose->part[i].entity];vec3_t local,delta;float travel;int needed;
  if(!Q_stricmp(part->classname,"func_door_rotating")){
   for(k=0;k<3;k++)local[k]=fabs(part->r.mins[k])>fabs(part->r.maxs[k])?fabs(part->r.mins[k]):fabs(part->r.maxs[k]);
   travel=VectorLength(local)*fabs(part->angle)*(float)M_PI/180.f;needed=(int)ceil(fabs(part->angle)/2.f);rotation=1;
  }else{VectorSubtract(part->pos2,part->pos1,delta);travel=VectorLength(delta);needed=(int)ceil(travel/4.f);}
  if(needed>steps)steps=needed;if(travel>motion)motion=travel;
 }
 if(steps>NG_DOOR_SWEEP_STEPS)return qfalse;
 /* Every point of a rigid leaf moves at most R*dAngle (or dTranslation).
  * With both bracketing samples checked, the nearest sample is at most
  * half a step away. Inflate the entire real actor capsule by that bound
  * plus its arrival tolerance. This covers intermediate poses, not only
  * a visually sampled start/end or an overbroad circular exclusion. */
 pad=motion/(2.f*steps)+tolerance+.125f;
 VectorSet(mins,feet[0]-width-pad,feet[1]-width-pad,feet[2]-pad);
 VectorSet(maxs,feet[0]+width+pad,feet[1]+width+pad,feet[2]+height+pad);
 for(sign=1;sign>=(rotation&&!NG_DoorFixedRotation(pose)?-1:1);sign-=2)for(sample=0;sample<=steps;sample++){
  NG_DoorPoseSet(pose,(float)sample/steps,sign);
  for(i=0;i<pose->count;i++){
   if(ngTraces>=NG_TRACE_BUDGET)goto done;
   ngTraces++;if(trap_EntityContactCapsule(mins,maxs,&g_entities[pose->part[i].entity]))goto done;
  }
 }
 clear=qtrue;
done:NG_DoorPoseRestore(pose);ngDoorProofTraces+=ngTraces-begin;return clear;
}
