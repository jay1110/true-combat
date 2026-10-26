/* Gameplay collision validation shared by Omni-bot and custom map spawns.
 * No NavStudio generator, authoring commands, graph or editing state is linked. */
#include "g_local.h"
#include "tce_nav_runtime.h"
#include "tce_nav_water.h"
#define TCE_NAV_RUNTIME 1
#define NG_MASK (CONTENTS_SOLID | CONTENTS_PLAYERCLIP)
#define NG_TRACE_BASE_BUDGET 2000000
#define NG_TRACE_BUDGET ngTraceBudget
static int ngTraces,ngTraceBudget=NG_TRACE_BASE_BUDGET,ngDoorProofTraces;
static float ngWidth,ngStand,ngDuck,ngOrigin;
static qboolean NG_Finite(float f) { return f==f && f>-1000000.f && f<1000000.f; }

/* A stationary func_static is implemented as ET_MOVER in the SDK, although
 * its collision is a fixed map surface. Do not extend this exception to
 * doors, trains, lifts, or scripted moving entities. Destructible stationary
 * floors use the separate conditional-lifetime branch below. */
int TCE_NavGroundKind(int entityNum) {
 gentity_t *ent;
 if(entityNum==ENTITYNUM_WORLD)return 1;
 if(entityNum<MAX_CLIENTS||entityNum>=level.num_entities)return 0;
 ent=&g_entities[entityNum];
 if(!ent->inuse||!ent->r.linked||!ent->r.bmodel||!(ent->r.contents&NG_MASK)||
    ent->s.pos.trType!=TR_STATIONARY||ent->s.apos.trType!=TR_STATIONARY||!ent->classname)return 0;
 if(!ent->takedamage&&(!Q_stricmp(ent->classname,"func_static")||!Q_stricmp(ent->classname,"func_wall")))return 1;
 /* A destructible floor is physically walkable until destruction. Its
  * lifetime is conditional, not an immutable-world proof. Runtime support
  * traces call this same predicate again after every short movement step. */
 if(!Q_stricmp(ent->classname,"func_explosive")&&(!ent->takedamage||ent->health>0))return 2;
 return 0;
}
static qboolean NG_StaticGround(int entityNum) {
 return TCE_NavGroundKind(entityNum)!=0;
}
#include "tce_nav_collision.inc"
#include "tce_nav_door_collision.inc"

qboolean TCE_NavDoorUseMatches(int useNumber,int doorNumber,int model) {
 int master=NG_DoorMaster(doorNumber),team,count=0;gentity_t *part;
 if(master<0||g_entities[doorNumber].s.modelindex!=model||g_entities[doorNumber].key!=0)return qfalse;
 for(part=&g_entities[master];part;part=part->teamchain){
  if(++count>NG_DOOR_GROUP||part<g_entities+MAX_CLIENTS||part>=g_entities+level.num_entities||
     !part->inuse||!NG_DoorClass(part)||part->key!=0||part->use!=Use_BinaryMover)return qfalse;
 }
 /* FORCE changes the opening sign, not Cmd_Activate_f eligibility. The
  * generator and runtime separately prove the actual waiting body safe. */
 if(NG_DoorMaster(useNumber)==master)return !(g_entities[useNumber].spawnflags&(1|128));
 /* Target resolution has no use/think side effects and rejects scripts,
  * random fan-outs, locked/unknown relays and retargeted button chains. */
 return NG_DoorRelay(useNumber,doorNumber,&team)==NULL;
}
qboolean TCE_NavDoorSweepSafe(int number,const vec3_t feet,float width,float height,float tolerance) {
 ngDoorPose pose;gentity_t *part;int master=NG_DoorMaster(number),i,savedTraces=ngTraces,savedBudget=ngTraceBudget,savedProof=ngDoorProofTraces;qboolean clear;
 if(master<0||!NG_Finite(width)||width<1.f||width>32.f||!NG_Finite(height)||height<2.f*width||height>128.f||
    !NG_Finite(tolerance)||tolerance<0.f||tolerance>16.f)return qfalse;
 for(i=0;i<3;i++)if(!NG_Finite(feet[i]))return qfalse;
 memset(&pose,0,sizeof(pose));pose.master=master;
 for(part=&g_entities[master];part;part=part->teamchain){
  if(pose.count>=NG_DOOR_GROUP||part<g_entities+MAX_CLIENTS||part>=g_entities+level.num_entities||
     !part->inuse||!NG_DoorClass(part)||!part->r.bmodel||!part->r.linked||!(part->r.contents&NG_MASK))return qfalse;
  for(i=0;i<pose.count;i++)if(pose.part[i].entity==(int)(part-g_entities))return qfalse;
  if(!Q_stricmp(part->classname,"func_door_rotating")&&(!NG_Finite(part->angle)||fabs(part->angle)>180.f||part->s.pos.trType!=TR_STATIONARY))return qfalse;
  pose.part[pose.count].entity=(int)(part-g_entities);pose.part[pose.count++].shared=part->r;
 }
 /* Runtime queries preserve the generation accounting as well as every
  * collision link. No game time, mover trajectory or actor is advanced. */
 ngTraces=0;ngTraceBudget=8000;clear=NG_DoorSweepFootClear(&pose,feet,width,height,tolerance);
 ngTraces=savedTraces;ngTraceBudget=savedBudget;ngDoorProofTraces=savedProof;return clear;
}

const char *TCE_NavValidateSpawn(const vec3_t origin,vec3_t feet,int *traces) {
 int n,savedTraces=ngTraces,savedBudget=ngTraceBudget;vec3_t from;const char *why;
 float savedWidth=ngWidth,savedOrigin=ngOrigin,savedStand=ngStand;
 for(n=0;n<3;n++)if(!NG_Finite(origin[n])||fabsf(origin[n])>131072.f)return "invalid_coordinates";
 ngWidth=g_newbbox.integer?16.f:14.f;ngOrigin=g_newbbox.integer?30.f:24.f;ngStand=g_newbbox.integer?87.f:70.f;
 ngTraces=0;ngTraceBudget=NG_TRACE_BASE_BUDGET;
 VectorCopy(origin,from);from[2]-=ngOrigin;VectorCopy(from,feet);
 why=NG_Ground(from,feet,ngStand);
 /* Authoring may not silently project a spawn onto another step/storey. */
 if(!why&&fabsf(feet[2]-from[2])>1.f)why="spawn_not_on_selected_floor";
 if(traces)*traces=ngTraces;
 ngTraces=savedTraces;ngTraceBudget=savedBudget;ngWidth=savedWidth;ngOrigin=savedOrigin;ngStand=savedStand;
 return why;
}
