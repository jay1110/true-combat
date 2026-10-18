/* Map-derived, bounded navigation generation against the loaded collision map.
 * WPS is optional input. Sampled coverage and jump/ladder geometry candidates
 * are not full-map or runtime proof; scripted traversal remains manual review. */
#include "g_local.h"
#include "tce_navgen.h"
#include "tce_nav_water.h"
#include "tce_movement.h"
#define NG_NODES 16384
#define NG_EDGES (NG_NODES * 16)
#define NG_ORDINARY_RADIUS_MAX 300
#define NG_ACTION_RADIUS_MAX 100
#define NG_PAIR_CACHE_SLOTS (NG_EDGES * 2)
#if NG_NODES > 16384 || (NG_PAIR_CACHE_SLOTS & (NG_PAIR_CACHE_SLOTS - 1))
#error Navigation pair keys require at most 16384 nodes and a power-of-two cache
#endif
#define NG_INPUT (128 * 1024)
#define NG_TRACE_BASE_BUDGET 2000000
/* A full16k external input can exceed8M in its mandatory walk checks alone.
 * Keep the existing count-derived allowance and a finite cap; post-input
 * searches retain their independent limits rather than scaling with it. */
#define NG_TRACE_MAX_BUDGET 32000000
#define NG_TRACE_NODE_ALLOWANCE 16
#define NG_TRACE_EDGE_ALLOWANCE 320
/* All native collision gates and phase reserves use this one per-run limit.
 * Native/WPS generation retains its original budget. An external graph has
 * already chosen its samples, so validating thousands of supplied walks must
 * not consume the whole allowance intended for component/action repair. */
static int ngTraceBudget=NG_TRACE_BASE_BUDGET;
#define NG_TRACE_BUDGET ngTraceBudget
#define NG_MASK (CONTENTS_SOLID | CONTENTS_PLAYERCLIP)

typedef struct {int entity,useEntity,target,mode,team,role,model,useModel,useSource,entry;vec3_t aim;qboolean validated;} ngDoorRecipe;
typedef struct { int id, flags, packed, ent[3], count, targets[12], linkFlags[12], index; vec3_t raw, feet, facing; unsigned int navFlags,importUID,outputUID; unsigned long long editFlags; int editSourceId,editRole,editDropId,propertyHead,propertyCount; qboolean authored,metadataSupplied; float radius, requestedRadius; char name[256]; qboolean accepted, crouch, topologyProtected; const char *reason; ngDoorRecipe door; } ngNode;
#define NG_PROPERTY_PAIRS 4096
#define NG_PROPERTY_PER_NODE 32
#define NG_PROPERTY_BYTES (1024 * 1024)
typedef struct {int owner,next,key,value;} ngCustomProperty;
static ngCustomProperty ngCustomProperties[NG_PROPERTY_PAIRS];
static char ngCustomPropertyText[NG_PROPERTY_BYTES];
static int ngCustomPropertyCount,ngCustomPropertyBytes;
typedef struct { int from, to; qboolean accepted, crouch, added; const char *reason; } ngEdge;
static ngNode ngNodes[NG_NODES];
static ngEdge ngEdges[NG_EDGES];
static unsigned char ngInput[NG_INPUT];
static int ngCount, ngEdgeCount, ngTraces, ngOffset, ngLength;
static float ngWidth, ngStand, ngDuck, ngOrigin;
static fileHandle_t ngFile;
static qboolean ngIoFailed;
static int ngStepDiagnostics;
static int ngMetadataInputCount;
static void NG_ExternalJsonString(const char *value);
static qboolean ngProbe;
static qboolean ngAuthoring;
static unsigned int ngAuthoringStarted;

qboolean TCE_NavgenAuthoringActive(void) {
 if(!ngAuthoring)return qfalse;
 if((unsigned int)trap_Milliseconds()-ngAuthoringStarted>=600000u ||
    !trap_Cvar_VariableIntegerValue("sv_cheats") || trap_Cvar_VariableIntegerValue("dedicated")){
  ngAuthoring=qfalse;return qfalse;
 }
 return qtrue;
}
static qboolean ngExternalImport,ngExternalManualDraft,ngExternalSpatialImport;
static int ngDetourRaw,ngDetourKept,ngCoverageSamples,ngCoverageRejected,ngCoverageAdded,ngCoverageExpanded,ngCoverageRemaining;
static const char *ngCoverageStop;
static int ngCampCount,ngQuarantinedCount;
static int ngGeometrySamples,ngGeometrySeeds,ngSpawnSeeds;
static qboolean ngGeometryBoundsValid,ngGeometryScanComplete;
static vec3_t ngWorldMins,ngWorldMaxs;
static unsigned char ngSpawnReachable[NG_NODES];
static int ngReachableCount,ngIsolatedCount,ngCoverageSpawnPending,ngCoveragePasses,ngCoverageTraceCeiling;
static unsigned char ngCoverageVisited[NG_NODES];
static unsigned int ngConditionalGroundHits[MAX_GENTITIES];
static void NG_SpawnReachability(void);

/* Exact directed-pair memoization without quadratic per-map storage. The
 * packed key is nonzero even for (0,0); the low two bits hold caller state.
 * Linear probing never evicts a proof or aliases colliding keys. Saturation
 * is explicit so callers can stop optional work or withhold the export. */
static unsigned int NG_PairKey(int a,int b) {
 if((unsigned int)a>=NG_NODES||(unsigned int)b>=NG_NODES)return 0;
 return (unsigned int)a*NG_NODES+(unsigned int)b+1u;
}
static unsigned int NG_PairHash(unsigned int key) {
 key^=key>>16;key*=0x7feb352du;key^=key>>15;key*=0x846ca68bu;key^=key>>16;
 return key&(NG_PAIR_CACHE_SLOTS-1);
}
static int NG_PairGet(const unsigned int *table,int a,int b) {
 unsigned int key=NG_PairKey(a,b),slot,probe;
 if(!key)return 0;
 slot=NG_PairHash(key);
 for(probe=0;probe<NG_PAIR_CACHE_SLOTS;probe++,slot=(slot+1)&(NG_PAIR_CACHE_SLOTS-1)) {
  unsigned int entry=table[slot];
  if(!entry)return 0;
  if((entry>>2)==key)return (int)(entry&3u);
 }
 return 0;
}
static qboolean NG_PairSet(unsigned int *table,int a,int b,int state) {
 unsigned int key=NG_PairKey(a,b),slot,probe;
 if(!key||(unsigned int)state>3u)return qfalse;
 slot=NG_PairHash(key);
 for(probe=0;probe<NG_PAIR_CACHE_SLOTS;probe++,slot=(slot+1)&(NG_PAIR_CACHE_SLOTS-1)) {
  unsigned int entry=table[slot];
  if(!entry||(entry>>2)==key){table[slot]=(key<<2)|(unsigned int)state;return qtrue;}
 }
 return qfalse;
}

/* Deterministic scheduling estimate, not collision or coverage evidence.
 * A 128-unit straight link needs about 150 traces with both flat export
 * anchors and independently carried capsule support. 320 allows a standing
 * attempt plus a crouch retry without consuming the shared repair reserve. The extra 2M
 * is shared by the existing, individually bounded post-input phases. Long
 * or difficult edges can cost more: the cap still fails closed, never marks
 * unchecked input accepted, and never grants a larger per-phase search. */
static int NG_ExternalTraceBudget(int nodes,int edges) {
 int estimate;
 if(nodes<0)nodes=0;else if(nodes>NG_NODES)nodes=NG_NODES;
 if(edges<0)edges=0;else if(edges>NG_EDGES)edges=NG_EDGES;
 /* Clamped storage counts keep both products and their sum within int. */
 estimate=nodes*NG_TRACE_NODE_ALLOWANCE+edges*NG_TRACE_EDGE_ALLOWANCE;
 if(estimate>=NG_TRACE_MAX_BUDGET-NG_TRACE_BASE_BUDGET)return NG_TRACE_MAX_BUDGET;
 return NG_TRACE_BASE_BUDGET+estimate;
}



static unsigned int NG_U16(void) { unsigned int v=ngInput[ngOffset] | (ngInput[ngOffset+1]<<8); ngOffset+=2; return v; }
static unsigned int NG_U32(void) { unsigned int v=ngInput[ngOffset] | ((unsigned int)ngInput[ngOffset+1]<<8) | ((unsigned int)ngInput[ngOffset+2]<<16) | ((unsigned int)ngInput[ngOffset+3]<<24); ngOffset+=4; return v; }
static float NG_Float(void) { union { unsigned int u; float f; } v; v.u=NG_U32(); return v.f; }
static qboolean NG_Finite(float f) { return f==f && f>-1000000.f && f<1000000.f; }
static qboolean NG_SafePath(const char *s) { int i; if(!*s || strstr(s,"..") || *s=='/')return qfalse; for(i=0;s[i];i++)if(!((s[i]>='a'&&s[i]<='z')||(s[i]>='A'&&s[i]<='Z')||(s[i]>='0'&&s[i]<='9')||s[i]=='_'||s[i]=='-'||s[i]=='/'||s[i]=='.'))return qfalse; return qtrue; }
static qboolean NG_Read(const char *path) {
 fileHandle_t f; int i,j; float version;
 ngLength=trap_FS_FOpenFile(path,&f,FS_READ);
 if(!f)return qfalse;
 if(ngLength<17||ngLength>NG_INPUT){trap_FS_FCloseFile(f);return qfalse;}
 trap_FS_Read(ngInput,ngLength,f);trap_FS_FCloseFile(f);
 if(memcmp(ngInput,"truecombat",11))return qfalse;
 ngOffset=11;version=NG_Float();ngCount=(short)NG_U16();
 if(version!=2.f||ngCount<1||ngCount>NG_NODES)return qfalse;
 memset(ngNodes,0,sizeof(ngNodes));
 for(i=0;i<ngCount;i++) {
  ngNode *n=&ngNodes[i]; if(ngOffset+28>ngLength)return qfalse;
  n->id=(short)NG_U16();for(j=0;j<3;j++){n->raw[j]=NG_Float();if(!NG_Finite(n->raw[j]))return qfalse;}
  n->flags=(short)NG_U16();for(j=0;j<3;j++)n->ent[j]=(short)NG_U16();n->packed=(int)NG_U32();n->count=(short)NG_U16();n->index=-1;
  if(n->count<0||n->count>12||ngOffset+n->count*4>ngLength)return qfalse;
  for(j=0;j<n->count;j++){n->targets[j]=(short)NG_U16();n->linkFlags[j]=(int)NG_U16();}
  if(n->id>=0&&n->flags!=-1)for(j=0;j<i;j++)if(ngNodes[j].id==n->id&&ngNodes[j].flags!=-1)return qfalse;
 }
 return ngOffset==ngLength;
}
static qboolean NG_Trace(trace_t *tr,const vec3_t a,const vec3_t b,float height) {
 vec3_t mins,maxs;
 if(ngTraces>=NG_TRACE_BUDGET)return qfalse;
 VectorSet(mins,-ngWidth,-ngWidth,0);VectorSet(maxs,ngWidth,ngWidth,height);
 /* Match ClientThink/Pmove's capsule for actual body movement; the thin
  * supporting-foot probe remains a box and must not become a tiny capsule. */
 if(height>2.f*ngWidth&&VectorCompare(a,b)) {
  vec3_t up,down;trace_t reverse;
  /* The classic engine's stationary capsule path can report ALLSOLID with
   * contents0 on an otherwise clear stair. Both real sweeps must be clear:
   * retain the entire original capsule and include its start in each sweep.
   * This adds .01u vertically rather than shrinking the player or accepting
   * a single direction. Reserve/count both calls before touching the engine. */
  if(ngTraces>NG_TRACE_BUDGET-2)return qfalse;
  VectorCopy(a,up);VectorCopy(a,down);up[2]+=.01f;down[2]-=.01f;
  trap_TraceCapsule(tr,a,mins,maxs,up,ENTITYNUM_NONE,NG_MASK);
  trap_TraceCapsule(&reverse,a,mins,maxs,down,ENTITYNUM_NONE,NG_MASK);ngTraces+=2;
  if(!tr->startsolid&&!tr->allsolid&&tr->fraction==1.f) {
   if(!reverse.startsolid&&!reverse.allsolid&&reverse.fraction==1.f){VectorCopy(a,tr->endpos);return qtrue;}
   *tr=reverse;
  }
  /* Callers use stationary occupancy flags, not sweep fractions. Preserve
   * the blocking entity/contents, but make any hit an occupied query. */
  tr->startsolid=tr->allsolid=qtrue;tr->fraction=0.f;VectorCopy(a,tr->endpos);return qtrue;
 }
 if(height>2.f*ngWidth)trap_TraceCapsule(tr,a,mins,maxs,b,ENTITYNUM_NONE,NG_MASK);
 else trap_Trace(tr,a,mins,maxs,b,ENTITYNUM_NONE,NG_MASK);
 ngTraces++;return qtrue;
}
static qboolean NG_Hurt(const vec3_t feet,float height) {
 int list[MAX_GENTITIES],count,i;vec3_t mins,maxs;
 VectorSet(mins,feet[0]-ngWidth,feet[1]-ngWidth,feet[2]);
 VectorSet(maxs,feet[0]+ngWidth,feet[1]+ngWidth,feet[2]+height);
 count=trap_EntitiesInBox(mins,maxs,list,MAX_GENTITIES);
 for(i=0;i<count;i++){gentity_t *ent=&g_entities[list[i]];
  if(ent->inuse&&ent->classname&&!Q_stricmp(ent->classname,"trigger_hurt"))return qtrue;
 }
 return qfalse;
}
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
 int kind=TCE_NavGroundKind(entityNum);
 if(kind==2)ngConditionalGroundHits[entityNum]++;
 return kind!=0;
}
/* Diagnostic only: zero-length capsule tests use a different engine path
 * than sweeps. Compare mathematically identical feet/origin conventions
 * and tiny vertical motions, without changing the rejected walk result.
 * tce_navgen_probe restores its trace accounting after the copied-node test. */
static void NG_ProbeBodyRepresentations(const vec3_t feet,float height) {
 static const float offsets[3]={0.f,.125f,.25f};
 static const float sweeps[3]={0.f,.01f,-.01f};
 int representation,lift,motion;vec3_t start,end,mins,maxs;trace_t tr;
 if(!ngProbe)return;
 for(representation=0;representation<2;representation++)for(lift=0;lift<3;lift++)for(motion=0;motion<3;motion++) {
  float origin=representation?ngOrigin:0.f;
  if(ngTraces>=NG_TRACE_BUDGET){G_Printf("navgen body compare: trace budget\n");return;}
  VectorCopy(feet,start);start[2]+=offsets[lift]+origin;
  VectorCopy(start,end);end[2]+=sweeps[motion];
  VectorSet(mins,-ngWidth,-ngWidth,-origin);VectorSet(maxs,ngWidth,ngWidth,height-origin);
  memset(&tr,0,sizeof(tr));
  trap_TraceCapsule(&tr,start,mins,maxs,end,ENTITYNUM_NONE,NG_MASK);ngTraces++;
  G_Printf("navgen body compare repr=%s height=%.2f lift=%.3f dz=%.3f solid=%d/%d entity=%d fraction=%.7f contents=%x feet=(%.4f %.4f %.4f) endfeet=(%.4f %.4f %.4f) normal=(%.4f %.4f %.4f)\n",
   representation?"player_origin":"feet",height,offsets[lift],sweeps[motion],tr.startsolid,tr.allsolid,tr.entityNum,tr.fraction,tr.contents,
   start[0],start[1],start[2]-origin,tr.endpos[0],tr.endpos[1],tr.endpos[2]-origin,tr.plane.normal[0],tr.plane.normal[1],tr.plane.normal[2]);
 }
}
static qboolean NG_OrdinarySurface(int surfaceFlags,float normalZ) {
 return normalZ>=.7f&&(!(surfaceFlags&SURF_SLICK)||normalZ>=.9999f);
}
static const char *NG_Ground(const vec3_t from,vec3_t feet,float height) {
 trace_t tr;vec3_t a,b; int contents;
 /* Find the highest supporting step with a short foot hull. Raising a
  * full player hull here invents headroom requirements under low ceilings;
  * probing only at current height can instead find a floor below a step. */
 VectorCopy(from,a);VectorCopy(from,b);a[2]+=18.f;b[2]-=18.5f;
 if(!NG_Trace(&tr,a,b,.25f))return "trace_budget";
 if(tr.startsolid||tr.allsolid){
  if(ngProbe)G_Printf("navgen ground stage=foot solid=%d/%d entity=%d start=(%.4f %.4f %.4f) end=(%.4f %.4f %.4f)\n",tr.startsolid,tr.allsolid,tr.entityNum,a[0],a[1],a[2],b[0],b[1],b[2]);
  return "blocked_clearance";
 }
 if(tr.fraction==1.f)return "gap_or_drop";
 if(!NG_StaticGround(tr.entityNum))return "dynamic_ground";
 if(tr.plane.normal[2]<.7f)return "steep_ground";
 /* PM_WalkMove retains grounded movement on horizontal ice. Its reduced
  * acceleration changes live steering, not the existence of this floor.
  * Sloped slick still requires a dedicated sliding recipe. */
 if(!NG_OrdinarySurface(tr.surfaceFlags,tr.plane.normal[2]))return "slippery_ground";
 VectorCopy(tr.endpos,feet);feet[2]+=.125f;
 /* Check actual standing/crouching clearance at the discovered floor,
  * independently of the elevated support probe. */
 if(!NG_Trace(&tr,feet,feet,height))return "trace_budget";
 if(tr.startsolid||tr.allsolid){
  if(ngProbe)G_Printf("navgen ground stage=body solid=%d/%d entity=%d height=%.2f start=end=(%.4f %.4f %.4f)\n",tr.startsolid,tr.allsolid,tr.entityNum,height,feet[0],feet[1],feet[2]);
  NG_ProbeBodyRepresentations(feet,height);
  return "blocked_clearance";
 }
 contents=trap_PointContents(feet,ENTITYNUM_NONE);
 if(contents&(CONTENTS_LAVA|CONTENTS_SLIME))return "water_or_hazard";
 if(contents&CONTENTS_WATER) {
  /* PM_SetWaterLevel stays at level1 while the posture-specific middle
   * sample is dry. The previous hardcoded18 rejected legitimate shallow
   * rivers, even though the actual standing player remains grounded. */
  VectorCopy(feet,a);a[2]+=TCE_NavWaterMiddleSample(ngOrigin==30.f,height<=ngDuck);
  if(trap_PointContents(a,ENTITYNUM_NONE)&(CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER))return "water_or_hazard";
 }
 if(NG_Hurt(feet,height))return "hurt_trigger";
 return NULL;
}
static void NG_Node(ngNode *n) {
 vec3_t a,b,feet;trace_t tr;const char *why;int j;
 n->reason="deleted";if(n->id<0||n->flags==-1)return;
 n->reason="special_node_requires_review";
 /* WPS flag/action bit meanings cannot be treated as Omni flags. Preserve
  * metadata in report and exclude unknown actions rather than guessing. */
 if(n->flags || n->packed)return;
 for(j=0;j<3;j++)if(n->ent[j]!=0&&n->ent[j]!=ENTITYNUM_NONE&&n->ent[j]!=-1)return;
 VectorCopy(n->raw,a);VectorCopy(a,b);a[2]+=18;b[2]-=96;
 if(ngTraces>=NG_TRACE_BUDGET){n->reason="trace_budget";return;}
 trap_Trace(&tr,a,NULL,NULL,b,ENTITYNUM_NONE,NG_MASK);ngTraces++;
 if(tr.startsolid||tr.allsolid||tr.fraction==1.f){n->reason="no_floor_near_seed";return;}
 if(!NG_StaticGround(tr.entityNum)){n->reason="dynamic_ground";return;}
 VectorCopy(tr.endpos,feet);feet[2]+=.125f;
 why=NG_Ground(feet,n->feet,ngStand);
 if(why){why=NG_Ground(feet,n->feet,ngDuck);n->crouch=qtrue;}
 if(why){n->reason=why;return;}
 n->accepted=qtrue;n->reason="accepted";
}
/* Read-only ordinary import, using exactly the normal point-floor then
 * short-foot/body pipeline. A requested crouched stance remains explicit;
 * a standing point that only imports crouched is not a standing proof. */
static qboolean NG_ImportOrdinaryCopy(const vec3_t feet,qboolean crouch,ngNode *copy,qboolean *exact) {
 vec3_t delta;
 memset(copy,0,sizeof(*copy));copy->id=0;copy->crouch=crouch;
 VectorCopy(feet,copy->feet);VectorCopy(feet,copy->raw);copy->raw[2]+=ngOrigin;
 *exact=qfalse;NG_Node(copy);
 if(!copy->accepted)return qfalse;
 if(copy->crouch!=crouch){copy->accepted=qfalse;copy->reason="import_posture_mismatch";return qfalse;}
 VectorSubtract(copy->feet,feet,delta);*exact=DotProduct(delta,delta)<=.0001f;
 return qtrue;
}
static void NG_ProbeImportOrdinary(const ngNode *from,const ngNode *to) {
 int endpoint,stance;qboolean savedProbe=ngProbe;ngProbe=qfalse;
 for(endpoint=0;endpoint<2;endpoint++)for(stance=0;stance<2;stance++) {
  ngNode copy;qboolean exact,accepted;
  accepted=NG_ImportOrdinaryCopy(endpoint?to->feet:from->feet,stance!=0,&copy,&exact);
  G_Printf("navgen probe import endpoint=%d crouch=%d accepted=%d exact=%d reason=%s feet=(%.5f %.5f %.5f)\n",
   endpoint,stance,accepted,exact,copy.reason?copy.reason:"unknown",copy.feet[0],copy.feet[1],copy.feet[2]);
 }
 ngProbe=savedProbe;
}
static const char *NG_Walk(const ngNode *a,const ngNode *b,float height) {
 vec3_t delta,previous,next,target,raised,raisedNext,actual,supportStart,supported,down;
 float length;int steps,i,contents;trace_t tr;const char *why;
 VectorSubtract(b->feet,a->feet,delta);length=VectorLength(delta);
 if(length>2048.f)return "edge_too_long";
 steps=(int)ceil(length/8.f);if(steps<1)steps=1;VectorCopy(a->feet,previous);
 VectorCopy(a->feet,actual);
 for(i=0;i<=steps;i++) {
  if(i) {
  VectorMA(a->feet,(float)i/steps,delta,target);target[2]=previous[2];
  why=NG_Ground(target,next,height);if(why){
   if(ngProbe)G_Printf("navgen probe sample %d/%d height=%.2f target=(%.2f %.2f %.2f) reason=%s\n",i,steps,height,target[0],target[1],target[2],why);
   return why;
  }
  if(fabs(next[2]-previous[2])>18.25f)return "step_or_drop";
  if(!NG_Trace(&tr,previous,next,height))return "trace_budget";
  if(tr.startsolid||tr.allsolid||tr.fraction<1.f) {
   /* PM step-slide analogue: raise, move, descend, each using full hull. */
   VectorCopy(previous,raised);raised[2]+=18;VectorCopy(next,raisedNext);raisedNext[2]=raised[2];
   if(!NG_Trace(&tr,previous,raised,height))return "trace_budget";
   if(tr.startsolid||tr.allsolid||tr.fraction<1.f)return "blocked_step_up";
   if(!NG_Trace(&tr,raised,raisedNext,height))return "trace_budget";
   if(tr.startsolid||tr.allsolid||tr.fraction<1.f)return "blocked_sweep";
   if(!NG_Trace(&tr,raisedNext,next,height))return "trace_budget";
   if(tr.startsolid||tr.allsolid||tr.fraction<1.f) {
    vec3_t error;VectorSubtract(tr.endpos,next,error);
    /* Collision traces stop on the walkable floor skin just above a target
     * point. Accept only that tiny world-floor contact, never a wall/mover. */
    if(!tr.startsolid&&!tr.allsolid&&NG_StaticGround(tr.entityNum)&&
       tr.plane.normal[2]>=.7f&&DotProduct(error,error)<=.25f) {
     VectorCopy(tr.endpos,next);next[2]+=.125f;
    } else {
     if(ngStepDiagnostics++<5)G_Printf("navgen step diagnostic %d->%d prev=(%.3f %.3f %.3f) next=(%.3f %.3f %.3f) raised=(%.3f %.3f %.3f) hit=(%.3f %.3f %.3f) frac=%.5f normal=(%.3f %.3f %.3f) ent=%d solid=%d/%d\n",a->id,b->id,previous[0],previous[1],previous[2],next[0],next[1],next[2],raisedNext[0],raisedNext[1],raisedNext[2],tr.endpos[0],tr.endpos[1],tr.endpos[2],tr.fraction,tr.plane.normal[0],tr.plane.normal[1],tr.plane.normal[2],tr.entityNum,tr.startsolid,tr.allsolid);
     return "blocked_step_down";
    }
   }
  }
  VectorCopy(next,previous);
  } else VectorCopy(a->feet,next);

  /* Export anchors retain NG_Ground's flat-foot convention. Independently
   * track where the real body rests: a box can still overlap a ledge after
   * the capsule has lost support. Never promote that overlap to a walk.
   * Starting at the already checked anchor avoids raising the whole body
   * into a low ceiling just to discover the floor. Slopes may put this
   * anchor several units above the actual capsule's supporting surface. */
  VectorCopy(next,supportStart);
  /* Resolve the actual capsule floor before choosing a movement target.
   * The export anchor is only a collision-query start, never an extra
   * climb allowance: a 5u slope offset must not turn an 18u step into 23u. */
  VectorCopy(supportStart,down);down[2]-=18.5f;
  if(!NG_Trace(&tr,supportStart,down,height))return "trace_budget";
  if(tr.startsolid||tr.allsolid)return "capsule_support_solid";
  if(tr.fraction==1.f)return "capsule_support_gap_or_drop";
  if(!NG_StaticGround(tr.entityNum))return "capsule_support_dynamic_ground";
  if(!NG_OrdinarySurface(tr.surfaceFlags,tr.plane.normal[2]))return "capsule_support_surface";
  VectorCopy(tr.endpos,supported);supported[2]+=.125f;
  if(i&&(supported[2]-actual[2]>18.25f||actual[2]-supported[2]>18.5f))return "capsule_support_step_or_drop";
  VectorCopy(tr.endpos,down);
  if(i) {
   if(!NG_Trace(&tr,actual,supported,height))return "trace_budget";
   if(tr.startsolid||tr.allsolid)return "capsule_walk_startsolid";
   if(tr.fraction<1.f) {
    qboolean descended=qfalse;
    /* A diagonal into a lower supported pose may cut through the lip of
     * the floor we are leaving. Try its horizontal-then-down movement
     * before requesting extra headroom for an upward step. Both sweeps
     * use the full body and the already bounded, independently supported
     * target; this cannot bridge a gap or grant a larger step/drop. */
    if(supported[2]<actual[2]&&actual[2]-supported[2]<=18.5f) {
     VectorCopy(supported,raisedNext);raisedNext[2]=actual[2];
     if(!NG_Trace(&tr,actual,raisedNext,height))return "trace_budget";
     if(!tr.startsolid&&!tr.allsolid&&tr.fraction==1.f) {
      if(!NG_Trace(&tr,raisedNext,supported,height))return "trace_budget";
      descended=!tr.startsolid&&!tr.allsolid&&tr.fraction==1.f;
     }
    }
    if(!descended) {
    VectorCopy(actual,raised);raised[2]+=18.f;
    VectorCopy(supported,raisedNext);raisedNext[2]=raised[2];
    if(!NG_Trace(&tr,actual,raised,height))return "trace_budget";
    if(tr.startsolid||tr.allsolid||tr.fraction<1.f)return "capsule_walk_step_up";
    if(!NG_Trace(&tr,raised,raisedNext,height))return "trace_budget";
    if(tr.startsolid||tr.allsolid||tr.fraction<1.f)return "capsule_walk_sweep";
    if(!NG_Trace(&tr,raisedNext,supported,height))return "trace_budget";
    if(tr.startsolid||tr.allsolid)return "capsule_walk_step_down";
    if(tr.fraction<1.f) {
     vec3_t error;VectorSubtract(tr.endpos,supported,error);
     if(!NG_StaticGround(tr.entityNum)||!NG_OrdinarySurface(tr.surfaceFlags,tr.plane.normal[2])||
        DotProduct(error,error)>.25f)return "capsule_walk_step_down";
     /* The supported target stays unchanged; its final skin/occupancy
      * queries below must still prove the exact same bounded pose. */
    }
    }
   }
  }
  /* The support hit and its skin offset are real full-body queries, not
   * a height snap. Recheck occupancy at the exact carried-forward pose. */
  if(!NG_Trace(&tr,down,supported,height))return "trace_budget";
  if(tr.startsolid||tr.allsolid||tr.fraction<1.f)return "capsule_support_skin_blocked";
  if(!NG_Trace(&tr,supported,supported,height))return "trace_budget";
  if(tr.startsolid||tr.allsolid)return "capsule_support_clearance";
  contents=trap_PointContents(supported,ENTITYNUM_NONE);
  if(contents&(CONTENTS_LAVA|CONTENTS_SLIME))return "capsule_support_hazard";
  if(contents&CONTENTS_WATER) {
   VectorCopy(supported,down);down[2]+=TCE_NavWaterMiddleSample(ngOrigin==30.f,height<=ngDuck);
   if(trap_PointContents(down,ENTITYNUM_NONE)&(CONTENTS_LAVA|CONTENTS_SLIME|CONTENTS_WATER))return "capsule_support_water";
  }
  if(NG_Hurt(supported,height))return "capsule_support_hurt";
  VectorCopy(supported,actual);
 }
 if(fabs(previous[2]-b->feet[2])>1.f)return "different_floor";
 return NULL;
}
static int NG_Find(int id) { int i;for(i=0;i<ngCount;i++)if(ngNodes[i].id==id&&ngNodes[i].flags!=-1)return i;return -1; }
static void NG_Edge(int from,int to,int flags,qboolean added) {
 ngEdge *e;const char *why;int previous;
 if(ngEdgeCount>=NG_EDGES)return;
 e=&ngEdges[ngEdgeCount++];memset(e,0,sizeof(*e));e->from=from;e->to=to;e->added=added;
 e->reason="invalid_target";if(to<0||to==from)return;
 for(previous=0;previous<ngEdgeCount-1;previous++)if(ngEdges[previous].from==from&&ngEdges[previous].to==to){e->reason="duplicate_edge";return;}
 e->reason="special_link_requires_review";if(flags)return;
 e->reason="rejected_endpoint";if(!ngNodes[from].accepted||!ngNodes[to].accepted)return;
 why=NG_Walk(&ngNodes[from],&ngNodes[to],ngStand);
 if(why){why=NG_Walk(&ngNodes[from],&ngNodes[to],ngDuck);e->crouch=qtrue;}
 if(why){e->reason=why;return;}
 e->accepted=qtrue;e->reason="accepted";
 /* Omni's crouch is node-based: mark both ends of crouch-only edges. */
 if(e->crouch)ngNodes[from].crouch=ngNodes[to].crouch=qtrue;
}
/* Never reinterpret an existing WPS action, including a rejected edge. */
static qboolean NG_HasEdge(int from,int to) {
 int i;for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].from==from&&ngEdges[i].to==to)return qtrue;
 return qfalse;
}
static void NG_Reach(int seed,qboolean reverse,unsigned char *seen) {
 int queue[NG_NODES],head=0,tail=0,i;memset(seen,0,NG_NODES);seen[seed]=1;queue[tail++]=seed;
 while(head<tail){int node=queue[head++];for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted){
  int from=reverse?ngEdges[i].to:ngEdges[i].from,to=reverse?ngEdges[i].from:ngEdges[i].to;
  if(from==node&&!seen[to]){seen[to]=1;queue[tail++]=to;}
 }}
}
static int NG_MainComponent(unsigned char *main) {
 unsigned char done[NG_NODES]={0},forward[NG_NODES],backward[NG_NODES];int i,j,best=0;
 memset(main,0,NG_NODES);
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted&&!done[i]){int count=0;
  NG_Reach(i,qfalse,forward);NG_Reach(i,qtrue,backward);
  for(j=0;j<ngCount;j++)if(forward[j]&&backward[j]){done[j]=1;count++;}
  if(count>best){best=count;for(j=0;j<ngCount;j++)main[j]=(unsigned char)(forward[j]&&backward[j]);}
 }
 return best;
}
#define NG_SEARCH 4096
typedef struct { ngNode node; int parent; float cost,score; qboolean closed; } ngSearch;
static ngSearch ngSearchNodes[NG_SEARCH];
static float NG_NearestMain(const vec3_t point,const unsigned char *main) {
 int i;float best=1e20f;for(i=0;i<ngCount;i++)if(main[i]){vec3_t d;float ds;VectorSubtract(point,ngNodes[i].feet,d);ds=DotProduct(d,d);if(ds<best)best=ds;}return sqrt(best);
}
static qboolean NG_BothWays(ngNode *a,ngNode *b) {
 const char *ab=NG_Walk(a,b,ngStand),*ba=NG_Walk(b,a,ngStand);
 if(!ab&&!ba)return qtrue;
 if(!NG_Walk(a,b,ngDuck)&&!NG_Walk(b,a,ngDuck)){a->crouch=b->crouch=qtrue;return qtrue;}
 return qfalse;
}
static void NG_Detour(int seed,const unsigned char *main) {
 int used=1,found=-1,goal=-1,begin=ngTraces,i,j,dx,dy,maxid=0;
 memset(ngSearchNodes,0,sizeof(ngSearchNodes));ngSearchNodes[0].node=ngNodes[seed];ngSearchNodes[0].parent=-1;
 ngSearchNodes[0].score=NG_NearestMain(ngNodes[seed].feet,main);
 while(used<NG_SEARCH&&ngTraces-begin<65000&&ngTraces<NG_TRACE_BUDGET-4000){
  int current=-1;float score=1e20f;ngSearch *cur;
  for(i=0;i<used;i++)if(!ngSearchNodes[i].closed&&ngSearchNodes[i].score<score){score=ngSearchNodes[i].score;current=i;}
  if(current<0)break;cur=&ngSearchNodes[current];cur->closed=qtrue;
  for(i=0;i<ngCount;i++)if(main[i]){vec3_t d;ngNode target=ngNodes[i];VectorSubtract(cur->node.feet,target.feet,d);
   if(DotProduct(d,d)<=64.f*64.f&&NG_BothWays(&cur->node,&target)){found=current;goal=i;break;}
  }
  if(found>=0)break;
  for(dx=-1;dx<=1;dx++)for(dy=-1;dy<=1;dy++)if(dx||dy){
   ngNode candidate;vec3_t trial;const char *why;float cost;int previous=-1;
   if(used>=NG_SEARCH||ngTraces-begin>=65000||ngTraces>=NG_TRACE_BUDGET-4000)break;
   memset(&candidate,0,sizeof(candidate));VectorCopy(cur->node.feet,trial);trial[0]+=dx*16.f;trial[1]+=dy*16.f;
   if(fabs(trial[0]-ngNodes[seed].feet[0])>1024.f||fabs(trial[1]-ngNodes[seed].feet[1])>1024.f)continue;
   why=NG_Ground(trial,candidate.feet,ngStand);if(why){why=NG_Ground(trial,candidate.feet,ngDuck);candidate.crouch=qtrue;}if(why)continue;
   for(j=0;j<used;j++)if(fabs(ngSearchNodes[j].node.feet[0]-candidate.feet[0])<1.f&&fabs(ngSearchNodes[j].node.feet[1]-candidate.feet[1])<1.f&&fabs(ngSearchNodes[j].node.feet[2]-candidate.feet[2])<1.f){previous=j;break;}
   cost=cur->cost+(dx&&dy?22.628f:16.f);
   if(previous>=0&&(ngSearchNodes[previous].closed||ngSearchNodes[previous].cost<=cost))continue;
   if(!NG_BothWays(&cur->node,&candidate))continue;
   if(previous<0)previous=used++;
   ngSearchNodes[previous].node=candidate;ngSearchNodes[previous].parent=current;ngSearchNodes[previous].cost=cost;
   ngSearchNodes[previous].score=cost+1.25f*NG_NearestMain(candidate.feet,main);
  }
 }
 if(found>=0){int route[NG_SEARCH],length=0,last=seed,kept=0;
  for(i=found;i>0;i=ngSearchNodes[i].parent)route[length++]=i;
  if(ngCount+length>NG_NODES||ngEdgeCount+(length+1)*2>NG_EDGES)return;
  for(i=0;i<ngCount;i++)if(ngNodes[i].id>maxid)maxid=ngNodes[i].id;
  ngDetourRaw+=length;
  /* Search uses a fine grid; export only necessary turns. Every shortcut is
   * validated with the same player hull, floor checks and reverse traversal. */
  for(i=length-1;i>=0;){int chosen=i,index;ngNode start=ngNodes[last],end=ngNodes[goal],*n;
   if(!NG_HasEdge(last,goal)&&!NG_HasEdge(goal,last)&&NG_BothWays(&start,&end))break;
   for(j=0;j<i&&ngTraces<NG_TRACE_BUDGET-4000;j++){
    ngNode target=ngSearchNodes[route[j]].node;
    if(NG_BothWays(&start,&target)){chosen=j;break;}
   }
   index=ngCount++;n=&ngNodes[index];*n=ngSearchNodes[route[chosen]].node;
   n->id=++maxid;n->accepted=qtrue;n->reason="generated_validated_detour";n->index=-1;VectorCopy(n->feet,n->raw);n->raw[2]+=ngOrigin;
   NG_Edge(last,index,0,qtrue);NG_Edge(index,last,0,qtrue);last=index;kept++;i=chosen-1;
  }
  NG_Edge(last,goal,0,qtrue);NG_Edge(goal,last,0,qtrue);ngDetourKept+=kept;
  G_Printf("navgen detour: seed=%d main=%d intermediate=%d search_path=%d traces=%d\n",ngNodes[seed].id,ngNodes[goal].id,kept,length,ngTraces-begin);
 }else G_Printf("navgen detour: unresolved seed=%d explored=%d traces=%d (no unsafe connection added)\n",ngNodes[seed].id,used,ngTraces-begin);
}
static void NG_Repair(void) {
 unsigned char main[NG_NODES];int i,j,k,original=ngEdgeCount,pass;
 /* A WPS graph can omit the uphill reverse of an entirely walkable route.
  * Test it independently; a successful downhill trace proves no reverse. */
 for(i=0;i<original&&ngTraces<NG_TRACE_BUDGET;i++)if(ngEdges[i].accepted){
  int from=ngEdges[i].to,to=ngEdges[i].from;
  if(!NG_HasEdge(from,to))NG_Edge(from,to,0,qtrue);
 }
 for(pass=0;pass<3&&ngTraces<NG_TRACE_BUDGET;pass++){
  int before=NG_MainComponent(main);
  for(i=0;i<ngCount&&ngTraces<NG_TRACE_BUDGET;i++)if(ngNodes[i].accepted&&!main[i]){
   int best[16];float dist[16];for(k=0;k<16;k++){best[k]=-1;dist[k]=1024.f*1024.f;}
   for(j=0;j<ngCount;j++)if(main[j]){vec3_t d;float ds;
    if(NG_HasEdge(i,j)&&NG_HasEdge(j,i))continue;
    VectorSubtract(ngNodes[i].feet,ngNodes[j].feet,d);ds=DotProduct(d,d);
    for(k=0;k<16;k++)if(ds<dist[k]){int t;for(t=15;t>k;t--){dist[t]=dist[t-1];best[t]=best[t-1];}dist[k]=ds;best[k]=j;break;}
   }
   for(k=0;k<16&&ngTraces<NG_TRACE_BUDGET;k++)if(best[k]>=0){
    j=best[k];if(!NG_HasEdge(i,j))NG_Edge(i,j,0,qtrue);
    if(!NG_HasEdge(j,i))NG_Edge(j,i,0,qtrue);
   }
  }
  if(NG_MainComponent(main)<=before)break;
 }
 NG_MainComponent(main);original=ngCount;
 for(i=0;i<original&&ngTraces<NG_TRACE_BUDGET-4000;i++)if(ngNodes[i].accepted&&!main[i]){NG_Detour(i,main);NG_MainComponent(main);}
}
/* Bounded seeded exploration samples floors, not a claim that every map
 * polygon was covered. Unseeded islands and scripted traversal remain unknown. */
static qboolean NG_FloorRay(const ngNode *from,float x,float y,ngNode *candidate) {
 vec3_t previous,target,next;int step,steps;const char *why;float dx=x-from->feet[0],dy=y-from->feet[1];
 memset(candidate,0,sizeof(*candidate));VectorCopy(from->feet,previous);
 VectorCopy(previous,target);target[0]=x;target[1]=y;
 /* Flat floors need only an endpoint projection; NG_BothWays still checks
  * the entire swept path. Incremental height discovery is for stairs. */
 why=NG_Ground(target,candidate->feet,ngStand);
 if(!why)return qtrue;
 steps=(int)ceil(sqrt(dx*dx+dy*dy)/8.f);if(steps<1)return qfalse;
 for(step=1;step<=steps;step++){
  VectorCopy(previous,target);target[0]=from->feet[0]+dx*step/steps;target[1]=from->feet[1]+dy*step/steps;
  why=NG_Ground(target,next,ngStand);if(why){why=NG_Ground(target,next,ngDuck);candidate->crouch=qtrue;}
  if(why||fabs(next[2]-previous[2])>18.25f)return qfalse;
  VectorCopy(next,previous);
 }
 VectorCopy(previous,candidate->feet);return qtrue;
}
static void NG_VerifiedWalkEdge(int from,int to,qboolean crouch) {
 ngEdge *e;if(ngEdgeCount>=NG_EDGES||NG_HasEdge(from,to))return;
 e=&ngEdges[ngEdgeCount++];memset(e,0,sizeof(*e));e->from=from;e->to=to;e->added=e->accepted=qtrue;e->crouch=crouch;e->reason="accepted";
 if(crouch)ngNodes[from].crouch=ngNodes[to].crouch=qtrue;
}
static void NG_Coverage(qboolean resume) {
 int i,j,dx,dy,maxid=0;ngCoverageStop="frontier_exhausted";
 if(!resume)memset(ngCoverageVisited,0,sizeof(ngCoverageVisited));
 /* Keep sampling bounded independently; leave a separate budget for
  * doorway recovery and collision-proven region simplification. */
 ngCoverageTraceCeiling=resume?ngTraces+100000:850000;
 if(ngCoverageTraceCeiling>NG_TRACE_BUDGET-10000)ngCoverageTraceCeiling=NG_TRACE_BUDGET-10000;
 ngCoveragePasses++;NG_SpawnReachability();
 for(i=0;i<ngCount;i++)if(ngNodes[i].id>maxid)maxid=ngNodes[i].id;
 for(;;){
  /* Playable, spawn-connected fronts take precedence over roof/island seeds.
   * New children inherit priority immediately rather than waiting behind the
   * entire coarse geometry seed list. Isolated surfaces remain in the report. */
  i=-1;for(j=0;j<ngCount;j++)if(ngNodes[j].accepted&&!ngCoverageVisited[j]&&ngSpawnReachable[j]){i=j;break;}
  if(i<0&&ngSpawnSeeds){
   NG_SpawnReachability();for(j=0;j<ngCount;j++)if(ngNodes[j].accepted&&!ngCoverageVisited[j]&&ngSpawnReachable[j]){i=j;break;}
   if(i<0){ngCoverageStop="spawn_frontier_exhausted_islands_quarantined";break;}
  }
  if(i<0)for(j=0;j<ngCount;j++)if(ngNodes[j].accepted&&!ngCoverageVisited[j]){i=j;break;}
  if(i<0)break;
  if(ngTraces>=ngCoverageTraceCeiling){ngCoverageStop="trace_budget";break;}
  if(ngCount>=NG_NODES-128){ngCoverageStop="node_budget";break;}
  if(ngEdgeCount>=NG_EDGES-512){ngCoverageStop="edge_budget";break;}
  ngCoverageExpanded++;
  for(dx=-1;dx<=1;dx++)for(dy=-1;dy<=1;dy++){
   ngNode candidate,from=ngNodes[i];qboolean represented=qfalse;float x=floor(from.feet[0]/64.f+.5f)*64.f+dx*64.f,y=floor(from.feet[1]/64.f+.5f)*64.f+dy*64.f;
   if(fabs(x-from.feet[0])<.5f&&fabs(y-from.feet[1])<.5f)continue;
   /* All expansion fronts share one absolute lattice. An already tested
    * bidirectional adjacency requires no further collision work. */
   for(j=0;j<ngCount;j++)if(ngNodes[j].accepted&&fabs(ngNodes[j].feet[0]-x)<.5f&&fabs(ngNodes[j].feet[1]-y)<.5f&&fabs(ngNodes[j].feet[2]-from.feet[2])<18.f&&NG_HasEdge(i,j)&&NG_HasEdge(j,i)){represented=qtrue;break;}
   if(represented)continue;
   if(ngTraces>=ngCoverageTraceCeiling){ngCoverageStop="trace_budget";goto coverage_done;}
   if(ngCount>=NG_NODES-128){ngCoverageStop="node_budget";goto coverage_done;}
   if(ngEdgeCount>=NG_EDGES-512){ngCoverageStop="edge_budget";goto coverage_done;}
   ngCoverageSamples++;
   if(!NG_FloorRay(&from,x,y,&candidate)||!NG_BothWays(&from,&candidate)){ngCoverageRejected++;continue;}
   /* Proximity across a wall is not coverage. Require a real walk before
    * treating an existing point as representing the newly sampled floor. */
   for(j=0;j<ngCount;j++)if(ngNodes[j].accepted){vec3_t d;ngNode near=ngNodes[j];VectorSubtract(near.feet,candidate.feet,d);
    if(DotProduct(d,d)<.25f||(DotProduct(d,d)<48.f*48.f&&NG_BothWays(&candidate,&near))){
     represented=qtrue;
     if(j!=i){
      if(DotProduct(d,d)<.25f){NG_VerifiedWalkEdge(i,j,from.crouch||candidate.crouch);NG_VerifiedWalkEdge(j,i,from.crouch||candidate.crouch);}
      else{if(!NG_HasEdge(i,j))NG_Edge(i,j,0,qtrue);if(!NG_HasEdge(j,i))NG_Edge(j,i,0,qtrue);}
     }
     break;
    }
   }
   if(!represented){int index=ngCount++;ngNode *n=&ngNodes[index];*n=candidate;
    n->id=++maxid;n->accepted=qtrue;n->reason="generated_floor_coverage";n->index=-1;VectorCopy(n->feet,n->raw);n->raw[2]+=ngOrigin;
    NG_VerifiedWalkEdge(i,index,from.crouch||candidate.crouch);NG_VerifiedWalkEdge(index,i,from.crouch||candidate.crouch);ngSpawnReachable[index]=ngSpawnReachable[i];ngCoverageAdded++;
   }
  }
  ngCoverageVisited[i]=1;
  if(!(ngCoverageExpanded%64))NG_SpawnReachability();
 }
coverage_done:
 NG_SpawnReachability();ngCoverageRemaining=ngCoverageSpawnPending=0;for(j=0;j<ngCount;j++)if(ngNodes[j].accepted&&!ngCoverageVisited[j]){ngCoverageRemaining++;if(ngSpawnReachable[j])ngCoverageSpawnPending++;}
 G_Printf("navgen coverage: samples=%d rejected=%d added=%d expanded=%d pending=%d stop=%s (sampled seeded floors, not full-map proof)\n",ngCoverageSamples,ngCoverageRejected,ngCoverageAdded,ngCoverageExpanded,ngCoverageRemaining,ngCoverageStop);
}
static unsigned int NG_DecodeWord(const unsigned char *b) { return b[0]|((unsigned int)b[1]<<8)|((unsigned int)b[2]<<16)|((unsigned int)b[3]<<24); }
static qboolean NG_WorldBounds(void) {
 unsigned char header[144],model[40];fileHandle_t file;char path[MAX_QPATH];int size,offset,left,chunk,i;
 Com_sprintf(path,sizeof(path),"maps/%s.bsp",level.rawmapname);size=trap_FS_FOpenFile(path,&file,FS_READ);
 if(!file)return qfalse;
 if(size<184){trap_FS_FCloseFile(file);return qfalse;}
 trap_FS_Read(header,sizeof(header),file);
 /* ET IBSP v47: 17 lumps; model lump7 begins with world model bounds. */
 offset=(int)NG_DecodeWord(header+8+7*8);
 if(memcmp(header,"IBSP",4)||NG_DecodeWord(header+4)!=47||offset<144||offset>size-40||NG_DecodeWord(header+12+7*8)<40){trap_FS_FCloseFile(file);return qfalse;}
 left=offset-144;while(left>0){chunk=left>NG_INPUT?NG_INPUT:left;trap_FS_Read(ngInput,chunk,file);left-=chunk;}
 trap_FS_Read(model,sizeof(model),file);trap_FS_FCloseFile(file);
 for(i=0;i<3;i++){union {unsigned int u;float f;} a,b;a.u=NG_DecodeWord(model+i*4);b.u=NG_DecodeWord(model+12+i*4);
  if(!NG_Finite(a.f)||!NG_Finite(b.f)||a.f>=b.f||b.f-a.f>65536.f)return qfalse;
  ngWorldMins[i]=a.f;ngWorldMaxs[i]=b.f;
 }
 return qtrue;
}
static qboolean NG_MapSeed(const vec3_t position,const char *reason) {
 ngNode candidate;vec3_t a,b,floor;trace_t tr;const char *why;int j;
 if(ngCount>=1024||ngTraces>=120000)return qfalse;
 memset(&candidate,0,sizeof(candidate));VectorCopy(position,a);VectorCopy(a,b);a[2]+=18.f;b[2]-=128.f;
 trap_Trace(&tr,a,NULL,NULL,b,ENTITYNUM_NONE,NG_MASK);ngTraces++;
 if(tr.startsolid||tr.allsolid||tr.fraction==1.f||!NG_StaticGround(tr.entityNum)||tr.plane.normal[2]<.7f||(tr.surfaceFlags&SURF_SKY))return qfalse;
 VectorCopy(tr.endpos,floor);floor[2]+=.125f;
 why=NG_Ground(floor,candidate.feet,ngStand);if(why){why=NG_Ground(floor,candidate.feet,ngDuck);candidate.crouch=qtrue;}if(why)return qfalse;
 for(j=0;j<ngCount;j++){vec3_t delta;VectorSubtract(candidate.feet,ngNodes[j].feet,delta);if(DotProduct(delta,delta)<64.f*64.f){ngNode old=ngNodes[j];if(NG_BothWays(&candidate,&old))return qfalse;}}
 candidate.id=ngCount;candidate.index=-1;candidate.accepted=qtrue;candidate.reason=reason;VectorCopy(position,candidate.raw);ngNodes[ngCount++]=candidate;return qtrue;
}
static void NG_MapSeeds(void) {
 int i;float x,y,z;ngCount=0;memset(ngNodes,0,sizeof(ngNodes));
 ngGeometrySamples=ngGeometrySeeds=ngSpawnSeeds=0;ngGeometryScanComplete=qfalse;ngGeometryBoundsValid=NG_WorldBounds();
 for(i=MAX_CLIENTS;i<level.num_entities;i++){gentity_t *ent=&g_entities[i];const char *name=ent->classname;
  if(!ent->inuse||!name)continue;
  if(!Q_stricmpn(name,"info_player_",12)||!Q_stricmpn(name,"team_CTF_",9)||!Q_stricmp(name,"trigger_objective_info")){
   vec3_t origin;VectorCopy(ent->r.currentOrigin,origin);
   if(ent->r.bmodel){VectorAdd(ent->r.absmin,ent->r.absmax,origin);VectorScale(origin,.5f,origin);}
   if(!Q_stricmp(name,"trigger_objective_info"))NG_MapSeed(origin,"map_objective_seed");
   else if(NG_MapSeed(origin,"map_entity_spawn_seed"))ngSpawnSeeds++;
  }
 }
 if(ngGeometryBoundsValid){
  /* Vertical columns also sample interior storeys, not only the top roof.
   * Coarse cells are deliberately reported; small isolated rooms may remain
   * undiscovered and must not be represented as full-map coverage. */
  for(x=ngWorldMins[0]+64.f;x<ngWorldMaxs[0];x+=192.f)
   for(y=ngWorldMins[1]+64.f;y<ngWorldMaxs[1];y+=192.f)
    for(z=ngWorldMins[2]+32.f;z<=ngWorldMaxs[2]+128.f;z+=128.f){vec3_t point;
     if(ngCount>=1024||ngTraces>=120000||ngGeometrySamples>=50000)goto map_seed_done;
     VectorSet(point,x,y,z);ngGeometrySamples++;if(NG_MapSeed(point,"map_geometry_floor_seed"))ngGeometrySeeds++;
    }
  ngGeometryScanComplete=qtrue;
 }
map_seed_done:
 G_Printf("navgen map seeds: spawns=%d geometry=%d samples=%d bounds=%d scan_finished=%d (no WPS required)\n",ngSpawnSeeds,ngGeometrySeeds,ngGeometrySamples,ngGeometryBoundsValid,ngGeometryScanComplete);
}
#include "tce_navgen_portals.inc"
#include "tce_navgen_traversal.inc"
#include "tce_navgen_doors.inc"
#include "tce_navgen_simplify.inc"
static void NG_Camps(void) {
 unsigned char main[NG_NODES];int i,j,d;ngCampCount=0;
 /* A complete manual graph owns even its implicit empty/default metadata.
  * Automatic camp derivation must never rename any supplied editor point. */
 if(ngExternalManualDraft)return;
 NG_MainComponent(main);if(ngSpawnSeeds)memcpy(main,ngSpawnReachable,sizeof(main));
 for(i=0;i<ngCount&&ngCampCount<32&&ngTraces<NG_TRACE_BUDGET-2000;i++)if(main[i]&&!ngNodes[i].crouch&&!ngNodes[i].navFlags&&NG_TrDropTarget(i)<0){
  ngNode *n=&ngNodes[i];vec3_t target,direction;trace_t tr;qboolean spaced=qtrue,cover=qfalse,view=qfalse;
  /* An empty name/facing in a manual draft is authored metadata too, even
   * for a newly appended ordinary point with no imported WAY identity. */
  if(n->metadataSupplied&&(ngExternalManualDraft||(n->authored&&n->id==n->editSourceId)))continue;
  for(j=0;j<i;j++)if(ngNodes[j].name[0]){vec3_t delta;VectorSubtract(n->feet,ngNodes[j].feet,delta);if(DotProduct(delta,delta)<256.f*256.f){spaced=qfalse;break;}}
  if(!spaced||!NG_Trace(&tr,n->feet,n->feet,ngStand)||tr.startsolid||tr.allsolid)continue;
  /* Nearby solid cover is only a positional heuristic, not tactical proof.
   * Static world walls only; do not advertise a moving door as cover. */
  for(d=0;d<4;d++){VectorCopy(n->feet,target);target[d/2]+=(d&1)?64.f:-64.f;
   if(NG_Trace(&tr,n->feet,target,ngStand)&&!tr.startsolid&&tr.fraction<1.f&&tr.entityNum==ENTITYNUM_WORLD&&fabs(tr.plane.normal[2])<.3f)cover=qtrue;
  }
  if(!cover)continue;
  for(j=0;j<ngEdgeCount;j++)if(ngEdges[j].accepted&&ngEdges[j].from==i&&main[ngEdges[j].to]){
   VectorSubtract(ngNodes[ngEdges[j].to].feet,n->feet,direction);direction[2]=0;
   if(VectorNormalize(direction)<32.f)continue;
   VectorMA(n->feet,128.f,direction,target);
   if(NG_Trace(&tr,n->feet,target,ngStand)&&!tr.startsolid&&!tr.allsolid&&tr.fraction==1.f){view=qtrue;break;}
  }
  if(!view)continue;
  VectorCopy(direction,n->facing);Com_sprintf(n->name,sizeof(n->name),"tce2_camp_%d",n->id);ngCampCount++;
 }
 G_Printf("navgen camps: %d sparse standing cover candidates (tactical/live review required)\n",ngCampCount);
}
static void NG_SpawnReachability(void) {
 int queue[NG_NODES],head=0,tail=0,i;memset(ngSpawnReachable,0,sizeof(ngSpawnReachable));ngReachableCount=ngIsolatedCount=0;
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted&&!strcmp(ngNodes[i].reason,"map_entity_spawn_seed")){ngSpawnReachable[i]=1;queue[tail++]=i;}
 while(head<tail){int node=queue[head++];for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted&&ngEdges[i].from==node&&!ngSpawnReachable[ngEdges[i].to]){ngSpawnReachable[ngEdges[i].to]=1;queue[tail++]=ngEdges[i].to;}}
 ngCoverageRemaining=ngCoverageSpawnPending=0;
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted){if(ngSpawnReachable[i])ngReachableCount++;else ngIsolatedCount++;
  if(!ngCoverageVisited[i]){ngCoverageRemaining++;if(ngSpawnReachable[i])ngCoverageSpawnPending++;}
 }
}
#define NG_ARRIVAL_TRACE_BUDGET 350000
#define NG_ARRIVAL_NEIGHBORS 64
#define NG_ARRIVAL_ATTEMPTS 8
typedef struct {float radius;int traces;const char *reason;} ngArrivalAttempt;
typedef struct {const char *policy,*role,*limitReason;float requested,regionRadius;int hullChecks,floorSamples,transitionChecks,attemptCount;qboolean verified,precision;ngArrivalAttempt attempts[NG_ARRIVAL_ATTEMPTS];} ngArrivalProof;
typedef struct {int node;qboolean incoming,outgoing,crouch;} ngArrivalNeighbor;
static ngArrivalProof ngArrivalProofs[NG_NODES];
static int ngArrivalTraceStart,ngArrivalTraceLimit,ngArrivalTraces,ngArrivalBudget;
static qboolean ngArrivalBudgetExhausted;
static int NG_ArrivalNeighbors(int index,ngArrivalNeighbor *neighbors) {
 int k,count=0;
 for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted) {
  const ngEdge *e=&ngEdges[k];int other=-1,j;
  if(e->from==index)other=e->to;else if(e->to==index)other=e->from;
  if(other<0||other==index||!ngNodes[other].accepted||ngNodes[other].navFlags)continue;
  for(j=0;j<count&&neighbors[j].node!=other;j++);
  if(j==count){if(count>=NG_ARRIVAL_NEIGHBORS)return -1;memset(&neighbors[count],0,sizeof(neighbors[0]));neighbors[count++].node=other;}
  neighbors[j].incoming|=e->to==index;neighbors[j].outgoing|=e->from==index;
  neighbors[j].crouch|=e->crouch||ngNodes[index].crouch||ngNodes[other].crouch;
 }
 return count;
}
static qboolean NG_ArrivalBudget(int traces) {
 if(ngTraces+traces<=ngArrivalTraceLimit)return qtrue;
 ngArrivalBudgetExhausted=qtrue;return qfalse;
}
static const char *NG_ArrivalGround(const vec3_t from,vec3_t feet,float height) {
 /* One support trace plus the two-sweep stationary capsule occupancy test. */
 if(!NG_ArrivalBudget(3))return "arrival_trace_budget";
 return NG_Ground(from,feet,height);
}
static const char *NG_ArrivalWalk(const ngNode *a,const ngNode *b,float height) {
 vec3_t d;int reserve;VectorSubtract(a->feet,b->feet,d);
 /* Ground uses a foot trace and two stationary capsule sweeps. Include
  * these and the worst step-slide sequence, including a stationary sweep,
  * before entering NG_Walk; its global budget is not this phase's limit. */
 reserve=9*(int)ceil(VectorLength(d)/8.f);if(reserve<9)reserve=9;
 if(!NG_ArrivalBudget(reserve))return "arrival_trace_budget";
 return NG_Walk(a,b,height);
}
static void NG_ArrivalAttempt(ngArrivalProof *proof,float radius,const char *reason,int start) {
 if(proof->attemptCount<NG_ARRIVAL_ATTEMPTS){ngArrivalAttempt *a=&proof->attempts[proof->attemptCount++];a->radius=radius;a->reason=reason;a->traces=ngTraces-start;}
}
/* Radius is an arrival tolerance intersecting reachable space, not a disc
 * reserved around the marker. A wall beside a straight corridor therefore
 * does not justify shrinking 35. Precision is required only when an actual
 * native-reachable approach point cannot continue to an outgoing walk leg. */
static const char *NG_ArrivalTurns(int index,float radius,const ngArrivalNeighbor *neighbors,int count,ngArrivalProof *proof) {
 int a,b;
 for(a=0;a<count;a++)if(neighbors[a].incoming) {
  ngNode entry;vec3_t d;float length;const char *why; qboolean relevant=qfalse;
  VectorSubtract(ngNodes[neighbors[a].node].feet,ngNodes[index].feet,d);length=VectorLength(d);
  if(length<.01f)continue;
  for(b=0;b<count;b++)if(a!=b&&neighbors[b].outgoing) {
   vec3_t out;VectorSubtract(ngNodes[neighbors[b].node].feet,ngNodes[index].feet,out);
   if(-DotProduct(d,out)<.95f*length*VectorLength(out)){relevant=qtrue;break;}
  }
  if(!relevant)continue;
  memset(&entry,0,sizeof(entry));entry.id=-1;
  VectorMA(ngNodes[index].feet,radius<length?radius/length:1.f,d,entry.feet);
  proof->floorSamples++;
  why=NG_ArrivalGround(entry.feet,entry.feet,neighbors[a].crouch?ngDuck:ngStand);
  if(why&&!strcmp(why,"arrival_trace_budget"))return why;
  if(why)continue; /* This wall/void sample cannot be an actual arrival. */
  why=NG_ArrivalWalk(&ngNodes[neighbors[a].node],&entry,neighbors[a].crouch?ngDuck:ngStand);
  if(why){if(!strcmp(why,"arrival_trace_budget"))return why;continue;}
  for(b=0;b<count;b++)if(a!=b&&neighbors[b].outgoing) {
   vec3_t out;VectorSubtract(ngNodes[neighbors[b].node].feet,ngNodes[index].feet,out);
   if(-DotProduct(d,out)>=.95f*length*VectorLength(out))continue;
   proof->transitionChecks++;
   why=NG_ArrivalWalk(&entry,&ngNodes[neighbors[b].node],neighbors[b].crouch?ngDuck:ngStand);
   if(why)return !strcmp(why,"arrival_trace_budget")?why:"arrival_turn_clearance_rejected";
  }
 }
 return NULL;
}
/* Enlarged tolerances require an unobstructed expanded body box, a bounded
 * native floor grid, and eight boundary entries/exits through every final
 * ordinary directed neighbor. Runtime still checks the actual next leg. */
static const char *NG_ArrivalRegion(int index,float radius,const ngArrivalNeighbor *neighbors,int count,ngArrivalProof *proof) {
 static const float directions[8][2]={{1,0},{.70710678f,.70710678f},{0,1},{-.70710678f,.70710678f},{-1,0},{-.70710678f,-.70710678f},{0,-1},{.70710678f,-.70710678f}};
 ngNode sample;trace_t tr;vec3_t mins,maxs,point;int x,y,n,s,steps=(int)ceil(radius/16.f);
 float height=ngNodes[index].crouch?ngDuck:ngStand;const char *why;
 if(!count)return "arrival_no_ordinary_transitions";
 if(!NG_ArrivalBudget(1))return "arrival_trace_budget";
 VectorSet(mins,-ngWidth-radius,-ngWidth-radius,0);VectorSet(maxs,ngWidth+radius,ngWidth+radius,height);
 trap_Trace(&tr,ngNodes[index].feet,mins,maxs,ngNodes[index].feet,ENTITYNUM_NONE,NG_MASK);ngTraces++;proof->hullChecks++;
 if(tr.startsolid||tr.allsolid)return "arrival_expanded_hull_blocked";
 memset(&sample,0,sizeof(sample));sample.id=-1;
 for(x=-steps;x<=steps;x++)for(y=-steps;y<=steps;y++) {
  if((float)(x*x+y*y)*256.f>radius*radius)continue;
  VectorCopy(ngNodes[index].feet,point);point[0]+=x*16.f;point[1]+=y*16.f;
  proof->floorSamples++;why=NG_ArrivalGround(point,sample.feet,height);
  if(why&&!strcmp(why,"arrival_trace_budget"))return why;
  if(why)return "arrival_region_ground_rejected";
  if(fabs(sample.feet[2]-ngNodes[index].feet[2])>1.f)return "arrival_region_floor_not_level";
 }
 for(s=0;s<8;s++) {
  VectorCopy(ngNodes[index].feet,point);point[0]+=directions[s][0]*radius;point[1]+=directions[s][1]*radius;
  proof->floorSamples++;why=NG_ArrivalGround(point,sample.feet,height);
  if(why&&!strcmp(why,"arrival_trace_budget"))return why;
  if(why||fabs(sample.feet[2]-ngNodes[index].feet[2])>1.f)return "arrival_boundary_ground_rejected";
  for(n=0;n<count;n++) {
   float stance=neighbors[n].crouch?ngDuck:ngStand;
   if(neighbors[n].incoming) {
    proof->transitionChecks++;why=NG_ArrivalWalk(&ngNodes[neighbors[n].node],&sample,stance);
    if(why)return !strcmp(why,"arrival_trace_budget")?why:"arrival_boundary_incoming_rejected";
   }
   if(neighbors[n].outgoing) {
    proof->transitionChecks++;why=NG_ArrivalWalk(&sample,&ngNodes[neighbors[n].node],stance);
    if(why)return !strcmp(why,"arrival_trace_budget")?why:"arrival_boundary_outgoing_rejected";
   }
  }
 }
 return NULL;
}
static void NG_ArrivalAction(int index,const char *role) {
 if(index<0||index>=ngCount||!ngNodes[index].accepted)return;
 ngArrivalProofs[index].role=role;
}
static void NG_FinalizeArrivalRadii(void) {
 static const float larger[3]={100.f,80.f,60.f};
 int i,k;ngArrivalNeighbor neighbors[NG_ARRIVAL_NEIGHBORS];
 memset(ngArrivalProofs,0,sizeof(ngArrivalProofs));ngArrivalBudgetExhausted=qfalse;
 ngArrivalTraceStart=ngTraces;ngArrivalTraceLimit=ngTraces+NG_ARRIVAL_TRACE_BUDGET;
 if(ngArrivalTraceLimit>NG_TRACE_BUDGET-4000)ngArrivalTraceLimit=NG_TRACE_BUDGET-4000;
 ngArrivalBudget=ngArrivalTraceLimit-ngTraces;if(ngArrivalBudget<0)ngArrivalBudget=0;
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted&&(ngNodes[i].navFlags||ngNodes[i].editRole))NG_ArrivalAction(i,"special_action");
 for(i=0;i<ngLadderProofCount;i++) {
  const ngLadderProof *p=&ngLadderProofs[i];qboolean complete=ngNodes[p->low].accepted&&ngNodes[p->high].accepted;
  for(k=0;k<4;k++)if((k!=2||p->reverse)&&!ngNodes[p->node[k]].accepted)complete=qfalse;
  if(p->ascentRelease>=0&&!ngNodes[p->ascentRelease].accepted)complete=qfalse;
  if(p->ascentAlignment>=0&&!ngNodes[p->ascentAlignment].accepted)complete=qfalse;
  if(!complete)continue;
  NG_ArrivalAction(p->low,"ladder_approach");NG_ArrivalAction(p->high,"ladder_exit_anchor");
  for(k=0;k<4;k++)NG_ArrivalAction(p->node[k],"ladder_guide");
  NG_ArrivalAction(p->ascentRelease,"ladder_release");NG_ArrivalAction(p->ascentAlignment,"ladder_alignment");
 }
 for(i=0;i<ngJumpProofCount;i++) {
  const ngJumpProof *p=&ngJumpProofs[i];if(!ngNodes[p->approach].accepted||!ngNodes[p->launch].accepted||!ngNodes[p->landing].accepted)continue;
  NG_ArrivalAction(p->approach,"jump_approach");NG_ArrivalAction(p->launch,"jump_launch");NG_ArrivalAction(p->landing,"jump_landing");
 }
 for(i=0;i<ngDropProofCount;i++) {
  const ngDropProof *p=&ngDropProofs[i];if(NG_TrDropTarget(p->launch)<0)continue;
  NG_ArrivalAction(p->approach,"walk_drop_approach");NG_ArrivalAction(p->launch,"walk_drop_launch");NG_ArrivalAction(p->landing,"walk_drop_landing");
 }
 for(i=0;i<ngFreeClimbProofCount;i++) {
  const ngFreeClimbProof *p=&ngFreeClimbProofs[i];if(NG_TrFreeTarget(p->launch)<0)continue;
  NG_ArrivalAction(p->approach,"free_climb_approach");NG_ArrivalAction(p->launch,"free_climb_launch");NG_ArrivalAction(p->landing,"free_climb_landing");
 }
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted) {
  ngNode *node=&ngNodes[i];ngArrivalProof *proof=&ngArrivalProofs[i];int count;
  proof->requested=node->requestedRadius>0.f?node->requestedRadius:35.f;
  if(proof->role) {
   float radius=node->radius>0.f?node->radius:8.f;
   /* Preserve the former recipe/anchor output exactly; ordinary navigation
    * no longer inherits this legacy spacing rule. */
   if(!node->navFlags&&strcmp(proof->role,"walk_drop_launch")&&strcmp(proof->role,"walk_drop_landing"))for(k=0;k<ngCount;k++)if(i!=k&&ngNodes[k].accepted) {
    vec3_t d;float limit;VectorSubtract(node->feet,ngNodes[k].feet,d);limit=(float)floor(VectorLength(d)*.5f);
    if(limit>=1.f&&limit<radius)radius=limit;
   }
   node->radius=radius;proof->policy=node->navFlags?"preserved_action_recipe":"preserved_action_anchor";
   proof->limitReason="action_geometry_radius_unchanged";continue;
  }
  node->radius=proof->requested<35.f?proof->requested:35.f;
  proof->policy="restricted_runtime_guard";proof->limitReason="standard_arrival_tolerance";
  count=NG_ArrivalNeighbors(i,neighbors);
  if(count<0){proof->limitReason="arrival_neighbor_limit";continue;}
  for(;;) {
   ngArrivalProof trial;const char *why;int start=ngTraces;memset(&trial,0,sizeof(trial));
   why=NG_ArrivalTurns(i,node->radius,neighbors,count,&trial);
   NG_ArrivalAttempt(proof,node->radius,why?why:"native_turn_clearance_verified",start);
   proof->floorSamples=trial.floorSamples;proof->transitionChecks=trial.transitionChecks;
   if(!why)break;
   proof->limitReason=why;
   if(strcmp(why,"arrival_turn_clearance_rejected"))break;
   proof->precision=qtrue;if(node->radius<=10.f)break;
   node->radius=node->radius>20.f?20.f:10.f;
  }
 }
 /* Finish precision checks for the entire graph before spending optional
  * work on enlarged open-area tolerances. A blocked annulus keeps 35. */
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted&&!ngArrivalProofs[i].role&&!ngArrivalProofs[i].precision) {
  ngNode *node=&ngNodes[i];ngArrivalProof *proof=&ngArrivalProofs[i];float candidates[4];int n=0,count;
  if(node->requestedRadius>0.f&&node->requestedRadius<=35.f)continue;
  count=NG_ArrivalNeighbors(i,neighbors);if(count<0)continue;
  if(node->requestedRadius>35.f)candidates[n++]=node->requestedRadius;
  for(k=0;k<3;k++)if(node->requestedRadius<=0.f||larger[k]<node->requestedRadius)candidates[n++]=larger[k];
  for(k=0;k<n;k++) {
   ngArrivalProof trial;const char *why;int start=ngTraces;memset(&trial,0,sizeof(trial));
   why=NG_ArrivalRegion(i,candidates[k],neighbors,count,&trial);
   NG_ArrivalAttempt(proof,candidates[k],why?why:"native_local_region_verified",start);
   if(!why) {
    node->radius=candidates[k];proof->policy="validated_local_region";proof->verified=qtrue;proof->regionRadius=node->radius;
    proof->hullChecks=trial.hullChecks;proof->floorSamples=trial.floorSamples;proof->transitionChecks=trial.transitionChecks;
    proof->limitReason="native_local_region_verified";break;
   }
   proof->limitReason=why;if(!strcmp(why,"arrival_trace_budget"))break;
  }
 }
 ngArrivalTraces=ngTraces-ngArrivalTraceStart;
 G_Printf("navgen arrival regions: traces=%d/%d budget_exhausted=%d default=35 minimum=10 ordinary_overlap_allowed=1\n",ngArrivalTraces,ngArrivalBudget,ngArrivalBudgetExhausted);
}
/* Geometry alone also finds roofs, sealed rooms and container tops. Keep
 * their coordinates/reasons in the report, but never install islands that
 * no spawn can reach. They remain unresolved coverage, not completed rooms. */
static void NG_QuarantineUnreachable(void) {
 int i;ngQuarantinedCount=0;
 if(!ngSpawnSeeds)return;
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted&&!ngSpawnReachable[i]) {
  ngNodes[i].accepted=qfalse;ngNodes[i].index=-1;
  ngNodes[i].reason="quarantined_no_validated_route_from_spawn";
  ngQuarantinedCount++;
 }
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted&&
    (!ngNodes[ngEdges[i].from].accepted||!ngNodes[ngEdges[i].to].accepted)) {
  ngEdges[i].accepted=qfalse;ngEdges[i].reason="quarantined_unreachable_endpoint";
 }
 G_Printf("navgen: %d unreachable geometry samples quarantined in report, not exported\n",ngQuarantinedCount);
}
/* Authored flags are independent of geometry classification and never narrowed
 * to 32 bits. Copies used for newly generated guides have a different ID. */
static unsigned long long NG_OutputFlags(const ngNode *n) {
 if(n->authored && n->id==n->editSourceId)return n->editFlags;
 return (unsigned long long)(n->navFlags|(n->crouch?64u:0u));
}
static int NG_OutputPropertyCount(const ngNode *n) {
 return n->authored&&n->id==n->editSourceId?n->propertyCount:0;
}
#define NG_UID_SLOTS (NG_NODES * 2)
static unsigned int ngUIDSet[NG_UID_SLOTS];
static unsigned int NG_UIDHash(unsigned int uid) {
 uid^=uid>>16;uid*=0x7feb352du;uid^=uid>>15;uid*=0x846ca68bu;uid^=uid>>16;
 return uid&(NG_UID_SLOTS-1);
}
static qboolean NG_UIDUsed(unsigned int uid) {
 unsigned int slot=NG_UIDHash(uid),i;
 if(!uid)return qfalse;
 for(i=0;i<NG_UID_SLOTS;i++,slot=(slot+1)&(NG_UID_SLOTS-1)) {
  if(!ngUIDSet[slot])return qfalse;
  if(ngUIDSet[slot]==uid)return qtrue;
 }
 return qtrue;
}
static qboolean NG_UIDInsert(unsigned int uid) {
 unsigned int slot=NG_UIDHash(uid),i;
 if(!uid)return qfalse;
 for(i=0;i<NG_UID_SLOTS;i++,slot=(slot+1)&(NG_UID_SLOTS-1)) {
  if(ngUIDSet[slot]==uid)return qfalse;
  if(!ngUIDSet[slot]){ngUIDSet[slot]=uid;return qtrue;}
 }
 return qfalse;
}
static unsigned int NG_ImportUID(const ngNode *n) {
 return n->authored&&n->id==n->editSourceId?n->importUID:0;
}
/* Reserve all source identities, including rejected floors. New native guides
 * may copy geometry from an authored point, but never inherit its identity.
 * Legacy graphs without imports retain the previous index+1 sequence exactly. */
static qboolean NG_AssignOutputUIDs(void) {
 unsigned int next=1;int i;
 memset(ngUIDSet,0,sizeof(ngUIDSet));
 for(i=0;i<ngCount;i++) {
  unsigned int uid=NG_ImportUID(&ngNodes[i]);ngNodes[i].outputUID=0;
  if(uid&&!NG_UIDInsert(uid))return qfalse;
 }
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted) {
  unsigned int uid=NG_ImportUID(&ngNodes[i]);
  if(!uid) {
   uid=(unsigned int)ngNodes[i].index+1u;
   if(!uid||NG_UIDUsed(uid)) {
    while(next&&NG_UIDUsed(next))next++;
    if(!next)return qfalse;
    uid=next++;
   }
   if(!NG_UIDInsert(uid))return qfalse;
  }
  ngNodes[i].outputUID=uid;
 }
 return qtrue;
}
static void NG_Pack64(unsigned long long n);
static void NG_Write(const void *p,int n) { if(trap_FS_Write(p,n,ngFile)!=n)ngIoFailed=qtrue; }
static void NG_Byte(unsigned int v) { unsigned char b=(unsigned char)v;NG_Write(&b,1); }
static void NG_Word(unsigned int v) { int i;for(i=0;i<4;i++)NG_Byte(v>>(8*i)); }
static void NG_Real(float f) { union {float f;unsigned int u;} v;v.f=f;NG_Word(v.u); }
static void NG_Pack(unsigned int n) { do {unsigned int b=n&127;n>>=7;NG_Byte(b|(n?0:128));}while(n); }
static void NG_Pack64(unsigned long long n) { do {unsigned int b=(unsigned int)(n&127);n>>=7;NG_Byte(b|(n?0:128));}while(n); }
static void NG_String(const char *s) { NG_Pack((unsigned int)strlen(s));NG_Write(s,(int)strlen(s)); }
static qboolean NG_Save(const char *path,int count) {
 int i,j,k,n,delta;unsigned int previousUID=0;vec3_t lo,hi,pos;char comment[232]={0},author[32]={0};
 if(!NG_AssignOutputUIDs())return qfalse;
 trap_FS_FOpenFile(path,&ngFile,FS_WRITE);if(!ngFile)return qfalse;ngIoFailed=qfalse;
 VectorSet(lo,999999,999999,999999);VectorSet(hi,-999999,-999999,-999999);
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted){VectorCopy(ngNodes[i].feet,pos);pos[2]+=ngOrigin;for(j=0;j<3;j++){if(pos[j]<lo[j])lo[j]=pos[j];if(pos[j]>hi[j])hi[j]=pos[j];}}
 Q_strncpyz(comment,"TCE2 map navigation candidate; special traversal/objectives require review",sizeof(comment));
 NG_Byte(9);NG_Word(count);NG_Write(author,32);NG_Write(comment,232);for(j=0;j<3;j++)NG_Real(lo[j]);for(j=0;j<3;j++)NG_Real(hi[j]);
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted) {
  ngNode *p=&ngNodes[i];unsigned long long flags=NG_OutputFlags(p);
  float radius=p->radius>0.f?p->radius:(p->navFlags?8.f:35.f);
  qboolean byteRadius=radius<=255.f&&floor(radius)==radius;
  qboolean explicitUID=previousUID==0xffffffffu||p->outputUID!=previousUID+1u;
  NG_Byte(4|(byteRadius?8:16)|(flags?1:0)|(p->name[0]?2:0)|(VectorLength(p->facing)>0.f?64:0)|(explicitUID?32:0));
  if(explicitUID)NG_Pack(p->outputUID);previousUID=p->outputUID;
  for(j=0;j<3;j++)NG_Real(p->feet[j]+(j==2?ngOrigin:0));
  if(VectorLength(p->facing)>0.f)for(j=0;j<3;j++)NG_Real(p->facing[j]);
  if(flags)NG_Pack64(flags);if(byteRadius)NG_Byte((unsigned int)radius);else NG_Real(radius);
  if(p->name[0])NG_String(p->name); /* Final arrival tolerance; special action recipes retain their own radius. */
  {qboolean release=!strcmp(p->reason,"ladder_ascent_release_contact_live_pending")||(p->authored&&p->id==p->editSourceId&&(p->editRole&2));
   qboolean alignment=!strcmp(p->reason,"ladder_upper_alignment_contact_live_pending")||(p->authored&&p->id==p->editSourceId&&(p->editRole&4));
   int dropTarget=NG_TrDropTarget(i),standTarget=NG_TrStandTarget(i),freeTarget=NG_TrFreeTarget(i);
   NG_Pack(1+(release?1:0)+(alignment?1:0)+(dropTarget>=0?2:0)+(standTarget>=0?2:0)+(freeTarget>=0?2:0)+(p->door.mode?11:0)+NG_OutputPropertyCount(p));NG_String("tce_node");NG_String(va("%d",p->id));
   if(release){NG_String("tce_ascent_release");NG_String("1");}
   if(alignment){NG_String("tce_ascent_alignment");NG_String("1");}
   if(dropTarget>=0){NG_String("tce_walk_drop");NG_String("1");NG_String("tce_walk_drop_target");NG_String(va("%u",ngNodes[dropTarget].outputUID));}
   if(standTarget>=0){NG_String("tce_stand_jump");NG_String("1");NG_String("tce_stand_jump_target");NG_String(va("%u",ngNodes[standTarget].outputUID));}
   if(freeTarget>=0){NG_String("tce_free_climb");NG_String("1");NG_String("tce_free_climb_target");NG_String(va("%u",ngNodes[freeTarget].outputUID));}
   if(p->door.mode){const ngDoorRecipe *r=&p->door;
    int target=NG_Find(r->target),use=NG_Find(r->useSource),entry=NG_Find(r->entry);
    if(!r->validated||use<0||entry<0||!ngNodes[use].accepted||!ngNodes[entry].accepted||
       (r->role!=3&&(target<0||!ngNodes[target].accepted))){trap_FS_FCloseFile(ngFile);return qfalse;}
    NG_String("tce_door_entity");NG_String(va("%d",r->entity));NG_String("tce_door_use_entity");NG_String(va("%d",r->useEntity));
    NG_String("tce_door_target");NG_String(va("%u",target>=0?ngNodes[target].outputUID:0));NG_String("tce_door_mode");NG_String(va("%d",r->mode));
    NG_String("tce_door_team");NG_String(va("%d",r->team));NG_String("tce_door_role");NG_String(va("%d",r->role));
    NG_String("tce_door_model");NG_String(va("%d",r->model));NG_String("tce_door_use_model");NG_String(va("%d",r->useModel));
    NG_String("tce_door_use_nav");NG_String(va("%u",ngNodes[use].outputUID));NG_String("tce_door_entry");NG_String(va("%u",ngNodes[entry].outputUID));
    NG_String("tce_door_aim");NG_String(va("%.4f %.4f %.4f",r->aim[0],r->aim[1],r->aim[2]));
   }
   if(NG_OutputPropertyCount(p)) {
    int property;
    for(property=p->propertyHead;property;property=ngCustomProperties[property-1].next) {
     const ngCustomProperty *custom=&ngCustomProperties[property-1];
     NG_String(ngCustomPropertyText+custom->key);NG_String(ngCustomPropertyText+custom->value);
    }
   }
  }
  n=0;for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].from==i&&ngEdges[k].accepted)n++;
  NG_Pack(n);for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].from==i&&ngEdges[k].accepted){delta=ngNodes[ngEdges[k].to].index-p->index;NG_Pack(delta>=0?delta*2:(-delta*2)-1);}
 }
 trap_FS_FCloseFile(ngFile);return !ngIoFailed;
}
static void NG_Text(const char *s) { NG_Write(s,(int)strlen(s)); }
static void NG_ArrivalReport(void) {
 int i,count=0;
 NG_Text(va("\"arrival_regions\":{\"model\":\"native_local_arrival_region_v1\",\"default_radius\":35,\"min_ordinary_radius\":10,\"max_ordinary_radius\":300,\"overlap_allowed\":true,\"runtime_gate\":\"can_walk_to_next_ordinary_native_step_capsule\",\"trace_budget\":%d,\"traces\":%d,\"budget_exhausted\":%s,\"action_exemptions\":[",ngArrivalBudget,ngArrivalTraces,ngArrivalBudgetExhausted?"true":"false"));
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted&&ngArrivalProofs[i].role) {
  NG_Text(va("%s{\"id\":%d,\"role\":\"%s\",\"radius\":%.4f}",count++?",":"",ngNodes[i].id,ngArrivalProofs[i].role,ngNodes[i].radius));
 }
 NG_Text("]},");
}
static void NG_ArrivalReportNode(int index) {
 const ngArrivalProof *proof=&ngArrivalProofs[index];ngArrivalNeighbor neighbors[NG_ARRIVAL_NEIGHBORS];int i,count;
 if(!ngNodes[index].accepted)return;
 NG_Text(va(",\"arrival\":{\"policy\":\"%s\",\"requested_source\":\"%s\",\"requested_radius\":%.4f,\"effective_radius\":%.4f,\"limit_reason\":\"%s\",\"precision_required\":%s,\"region_verified\":%s,\"region_radius\":%.4f,\"hull_checks\":%d,\"floor_samples\":%d,\"transition_checks\":%d,\"transition_neighbors\":[",
  proof->policy?proof->policy:"unfinalized",ngNodes[index].requestedRadius>0.f?"manual_draft":"automatic_default",proof->requested,ngNodes[index].radius,proof->limitReason?proof->limitReason:"unfinalized",proof->precision?"true":"false",proof->verified?"true":"false",proof->regionRadius,proof->hullChecks,proof->floorSamples,proof->transitionChecks));
 count=NG_ArrivalNeighbors(index,neighbors);
 for(i=0;i<count;i++)NG_Text(va("%s{\"id\":%d,\"incoming\":%s,\"outgoing\":%s,\"crouch\":%s}",i?",":"",ngNodes[neighbors[i].node].id,neighbors[i].incoming?"true":"false",neighbors[i].outgoing?"true":"false",neighbors[i].crouch?"true":"false"));
 NG_Text("],\"attempts\":[");
 for(i=0;i<proof->attemptCount;i++)NG_Text(va("%s{\"radius\":%.4f,\"reason\":\"%s\",\"traces\":%d}",i?",":"",proof->attempts[i].radius,proof->attempts[i].reason,proof->attempts[i].traces));
 NG_Text("]}");
}
static void NG_TrReport(void) {
 int i,k;
 NG_Text(va("\"traversal_physics\":{\"gravity\":%.9g,\"jump_impulse\":%.4f,\"ladder_exit_upward_velocity\":115,\"model\":\"tce_ladder_forward_or_strafe_then_air_heavy_run_v3\",\"step_seconds\":0.008,\"ladder_strafe_friction\":14,\"ladder_strafe_acceleration\":10,\"ladder_strafe_speed_scale\":0.5,\"ladder_forward_vertical_scale\":0.9,\"landing_support_radius\":%.3f,\"landing_support_samples\":9,\"ground_continuation_step\":18.25},",NG_TrGravity(),NG_TrJumpImpulse(),ngWidth));
 NG_Text(va("\"ladder_landing_search\":{\"probes\":%d,\"grounded\":%d,\"capsule_supported\":%d,\"hull_arcs\":%d,\"walk_connected\":%d,\"direct_transfer_tests\":%d,\"direct_transfer_successes\":%d},",ngLandingProbes,ngLandingGrounded,ngLandingSupported,ngLandingArcs,ngLandingWalks,ngLowerTransferTests,ngLowerTransferSuccesses));
 NG_Text(va("\"ladder_exit_failures\":{\"physics\":%d,\"target_support\":%d,\"no_tangent\":%d,\"solid_or_budget\":%d,\"collision\":%d,\"touchdown_ground\":%d,\"ground_continuation\":%d,\"touchdown_hull\":%d,\"hazard_or_fall\":%d,\"time_limit\":%d},",ngExitFailures[0],ngExitFailures[1],ngExitFailures[2],ngExitFailures[3],ngExitFailures[4],ngExitFailures[5],ngExitFailures[6],ngExitFailures[7],ngExitFailures[8],ngExitFailures[9]));
 NG_Text(va("\"traversal_budgets\":{\"bsp_ladder_traces\":%d,\"other_ladder_traces\":%d,\"other_ladder_trace_budget\":40000,\"jump_traces\":%d,\"jump_trace_budget\":140000},",ngBspTraversalTraces,ngGenericLadderTraces,ngJumpTraversalTraces));
 NG_Text(va("\"jump_front_search\":{\"model\":\"local_ground_jump_and_walk_drop_round_robin_v4\",\"action_quantum\":%d,\"candidate_order\":\"strip_round_robin_jump_and_upper_walk_cost\",\"trace_budget\":%d,\"traces\":%d,\"front_budget\":%d,\"arc_budget\":%d,\"arc_budget_scope\":\"attempts_with_native_collision_queries\",\"no_trace_rejections\":%d,\"arc_checks\":%d,\"paired_routes\":%d,\"return_damage_rejected\":%d,\"return_landing_rejected\":%d,\"return_crashland_limit\":38,\"stop_reason\":\"%s\",\"live_verified\":false,\"cases\":[",NG_FRONT_JUMP_QUANTUM,NG_FRONT_JUMP_TRACES,ngFrontJumpTraces,NG_FRONT_JUMP_CASES,NG_FRONT_JUMP_ARCS,ngFrontJumpNoTraceRejected,ngFrontJumpArcChecks,ngFrontJumpConnected,ngFrontReturnDamageRejected,ngFrontReturnLandingRejected,ngFrontJumpStop?ngFrontJumpStop:"not_started"));
 for(i=0;i<ngFrontJumpCaseCount;i++) {
  const ngFrontJumpCase *entry=&ngFrontJumpCases[i];
  NG_Text(va("%s{\"low\":%d,\"high\":%d,\"ground_samples\":%d,\"bidirectional_walk_samples\":%d,\"arc_checks\":%d,\"traces\":%d,\"reason\":\"%s\",\"low_samples\":%d,\"high_samples\":%d,\"separated_low_launches\":%d,\"separated_high_launches\":%d,\"up_checks\":%d,\"return_checks\":%d,\"up_accepted\":%d,\"arc_outcomes\":[",i?",":"",ngNodes[entry->low].id,ngNodes[entry->high].id,entry->samples,entry->walkSamples,entry->arcChecks,entry->traces,entry->reason,entry->lowSamples,entry->highSamples,entry->lowLaunches,entry->highLaunches,entry->upChecks,entry->returnChecks,entry->upAccepted));
  for(k=0;k<NG_JUMP_FAILURES;k++)NG_Text(va("%s{\"reason\":\"%s\",\"up\":%d,\"return\":%d}",k?",":"",ngJumpFailureNames[k],entry->upFailures[k],entry->returnFailures[k]));
  NG_Text("]");
  NG_Text(va(",\"portal_rank\":{\"capsule_clear\":%s,\"lane_index\":%d,\"running_first_pass\":%s,\"acceptance_evidence\":false}",entry->portalClear?"true":"false",entry->portalLane,entry->portalFirst?"true":"false"));
  NG_Text(",\"sample_coordinates\":\"player_origin\",\"low_points\":[");
  for(k=0;k<entry->lowSamples;k++)NG_Text(va("%s[%.4f,%.4f,%.4f]",k?",":"",entry->lowPoints[k][0],entry->lowPoints[k][1],entry->lowPoints[k][2]+ngOrigin));
  NG_Text("],\"high_points\":[");
  for(k=0;k<entry->highSamples;k++)NG_Text(va("%s[%.4f,%.4f,%.4f]",k?",":"",entry->highPoints[k][0],entry->highPoints[k][1],entry->highPoints[k][2]+ngOrigin));
  NG_Text("]");
  if(entry->returnExampleFailure)NG_Text(va(",\"return_example\":{\"reason\":\"%s\",\"coordinates\":\"player_origin\",\"launch\":[%.4f,%.4f,%.4f],\"landing\":[%.4f,%.4f,%.4f],\"hit\":[%.4f,%.4f,%.4f]}",ngJumpFailureNames[entry->returnExampleFailure],entry->returnExampleFrom[0],entry->returnExampleFrom[1],entry->returnExampleFrom[2]+ngOrigin,entry->returnExampleTo[0],entry->returnExampleTo[1],entry->returnExampleTo[2]+ngOrigin,entry->returnExampleHit[0],entry->returnExampleHit[1],entry->returnExampleHit[2]+ngOrigin));
  NG_Text(va(",\"walk_drop_checks\":%d,\"walk_drop_accepted\":%d,\"walk_drop_reason\":\"%s\",\"walk_drop_outcomes\":[",entry->dropChecks,entry->dropAccepted,entry->dropReason?entry->dropReason:"not_tested"));
  for(k=0;k<NG_DROP_FAILURES;k++)NG_Text(va("%s{\"reason\":\"%s\",\"count\":%d}",k?",":"",ngDropFailureNames[k],entry->dropFailures[k]));
  NG_Text("]");
  if(entry->dropExampleFailure)NG_Text(va(",\"walk_drop_example\":{\"reason\":\"%s\",\"coordinates\":\"player_origin\",\"launch\":[%.4f,%.4f,%.4f],\"landing\":[%.4f,%.4f,%.4f],\"hit\":[%.4f,%.4f,%.4f]}",ngDropFailureNames[entry->dropExampleFailure],entry->dropExampleFrom[0],entry->dropExampleFrom[1],entry->dropExampleFrom[2]+ngOrigin,entry->dropExampleTo[0],entry->dropExampleTo[1],entry->dropExampleTo[2]+ngOrigin,entry->dropExampleHit[0],entry->dropExampleHit[1],entry->dropExampleHit[2]+ngOrigin));
  NG_Text("}");
 }
 NG_Text("]},");
 NG_Text(va("\"jump_selection\":{\"order\":\"bounded_low_step_fronts_then_spawn_connected_then_bridges_then_islands\",\"negative_torso_ray_checks\":%d,\"negative_torso_ray_rejected\":%d,\"full_arc_candidates\":%d,\"full_arc_budget\":1024,\"negative_ray_is_traversal_proof\":false},\"traversal_routes\":[",ngJumpPrefilterChecks,ngJumpPrefilterRejected,ngJumpCandidates));
 for(i=0;i<ngLadderProofCount;i++){
  const ngLadderProof *p=&ngLadderProofs[i];qboolean exported=ngNodes[p->low].accepted&&ngNodes[p->high].accepted;
  for(k=0;k<4;k++)if((k!=2||p->reverse)&&!ngNodes[p->node[k]].accepted)exported=qfalse;
  if(p->ascentRelease>=0&&!ngNodes[p->ascentRelease].accepted)exported=qfalse;
  if(p->ascentAlignment>=0&&!ngNodes[p->ascentAlignment].accepted)exported=qfalse;
  NG_Text(va("%s{\"kind\":\"ladder\",\"low\":%d,\"high\":%d,\"guides\":[%d,%d,%d,%d],\"inside_guide\":%d,\"reverse\":%s,\"exported\":%s,\"exit_method\":\"%s\",\"lifted_exit\":%s,\"launch\":[%.4f,%.4f,%.4f],\"exit_velocity_z\":%.1f,\"exit_speed_xy\":%.4f,\"exit_duration_seconds\":%.4f,\"live_verified\":false,\"overlap_pairs\":[",
   i?",":"",ngNodes[p->low].id,ngNodes[p->high].id,ngNodes[p->node[0]].id,ngNodes[p->node[1]].id,ngNodes[p->node[2]].id,ngNodes[p->node[3]].id,ngNodes[p->node[1]].id,p->reverse?"true":"false",exported?"true":"false",p->ballistic?"ballistic":"supported",p->liftedExit?"true":"false",p->launch[0],p->launch[1],p->launch[2]+ngOrigin,p->ballistic?115.f:0.f,p->speed,p->duration));
  NG_Text(va("{\"first\":%d,\"second\":%d,\"reason\":\"ladder_contact_guidance\"},{\"first\":%d,\"second\":%d,\"reason\":\"ladder_mount_contact\"},{\"first\":%d,\"second\":%d,\"reason\":\"ladder_inside_approach_guidance\"},{\"first\":%d,\"second\":%d,\"reason\":\"ladder_exit_floor_alias\"}],\"ascent_release\":%d,\"ascent_alignment\":%d,\"exit_initial_speed_xy\":0,\"exit_initial_wishspeed\":%.4f,\"dismount_touchdown\":[%.4f,%.4f,%.4f],\"dismount_ground_continuation\":%.4f,\"ground_continuation_model\":\"%s\",\"ladder_contact_seconds\":%.4f,\"exit_control\":\"%s\"}",
   ngNodes[p->node[0]].id,ngNodes[p->node[1]].id,ngNodes[p->low].id,ngNodes[p->node[0]].id,ngNodes[p->low].id,ngNodes[p->node[1]].id,ngNodes[p->high].id,ngNodes[p->node[3]].id,p->ascentRelease>=0?ngNodes[p->ascentRelease].id:-1,p->ascentAlignment>=0?ngNodes[p->ascentAlignment].id:-1,p->wishspeed,
   p->ballistic?p->touchdown[0]:p->landing[0],p->ballistic?p->touchdown[1]:p->landing[1],(p->ballistic?p->touchdown[2]:p->landing[2])+ngOrigin,p->groundContinuation,p->forwardExit?"native_walk_capsule_support_v1":"native_walk_center_support_v2",p->contactDuration,p->ballistic?(p->forwardExit?"forward_climb_until_contact_loss":"tangent_strafe_until_contact_loss"):"supported_walk"));
 }
 for(i=0;i<ngJumpProofCount;i++){
  const ngJumpProof *p=&ngJumpProofs[i];
  float impulse=p->standing?TCE_NavStandingImpulse(g_newbbox.integer):NG_TrJumpImpulse();
  float impactSquared=impulse*impulse-2.f*NG_TrGravity()*(ngNodes[p->landing].feet[2]-ngNodes[p->launch].feet[2]);
  if(impactSquared<0.f)impactSquared=0.f;
  NG_Text(va("%s{\"kind\":\"jump\",\"approach\":%d,\"launch\":%d,\"landing\":%d,\"exported\":%s,\"impulse_z\":%.4f,\"arc_model\":\"%s\",\"validated_return\":%s,\"landing_speed_z\":%.4f,\"crashland_delta\":%.4f,\"live_verified\":false",
   ngLadderProofCount||i?",":"",ngNodes[p->approach].id,ngNodes[p->launch].id,ngNodes[p->landing].id,
   ngNodes[p->launch].accepted&&ngNodes[p->landing].accepted?"true":"false",impulse,
   p->standing?"low_velocity_forward_jump_capsule_samples_v1":(p->validatedReturn?"paired_running_jump_crashland_return_v1":"running_jump_hull_arc_v1"),p->validatedReturn?"true":"false",
   p->standing?p->impact:sqrt(impactSquared),p->standing?p->crash:impactSquared*(g_newbbox.integer?.64f:1.f)*.0002f));
  if(p->standing)NG_Text(va(",\"standing_launch\":true,\"stop_offset\":8,\"prime_wishspeed\":40,\"launch_speed_samples\":[36,40,44],\"wishspeed_samples\":[114,160,204],\"launch_offset_samples\":[0,-2,2],\"sample_count\":27,\"step_seconds\":0.008,\"launch_radius\":2,\"duration_seconds\":%.4f,\"touchdown\":[%.4f,%.4f,%.4f],\"ground_continuation\":%.4f",p->duration,p->touchdown[0],p->touchdown[1],p->touchdown[2]+ngOrigin,p->continuation));
  NG_Text("}");
 }
 for(i=0;i<ngDropProofCount;i++) {
  const ngDropProof *p=&ngDropProofs[i];
  NG_Text(va("%s{\"kind\":\"walk_drop\",\"approach\":%d,\"launch\":%d,\"landing\":%d,\"paired_jump_launch\":%d,\"exported\":%s,\"model\":\"tce_ground_then_fall_speed_samples_v3\",\"launch_support_model\":\"stationary_capsule_ground_trace_025\",\"wishspeed_samples\":[114,160,204],\"sample_count\":3,\"step_seconds\":0.008,\"initial_velocity\":[0,0,0],\"impulse_z\":0,\"max_horizontal\":160,\"max_endpoint_horizontal\":160,\"max_airborne_overshoot\":48,\"maxtravel\":208,\"max_drop\":%.4f,\"gravity\":%.4f,\"crashland_scale\":%.4f,\"crashland_limit\":38,\"duration_seconds\":%.4f,\"contact_seconds\":%.4f,\"touchdown\":[%.4f,%.4f,%.4f],\"ground_continuation\":%.4f,\"landing_speed_z\":%.4f,\"crashland_delta\":%.4f,\"launch_radius\":2,\"landing_radius\":6,\"landing_support_samples\":9,\"touchdown_support_model\":\"native_walk_center_support_v2\",\"intermediate_contacts\":%d,\"intermediate_contact_limit\":8,\"live_verified\":false}",
   ngLadderProofCount||ngJumpProofCount||i?",":"",ngNodes[p->approach].id,ngNodes[p->launch].id,ngNodes[p->landing].id,ngNodes[p->pairedJumpLaunch].id,NG_TrDropTarget(p->launch)>=0?"true":"false",
   NG_TrSafeDropHeight(),NG_TrGravity(),g_newbbox.integer?.64f:1.f,p->duration,p->contactDuration,p->touchdown[0],p->touchdown[1],p->touchdown[2]+ngOrigin,p->groundContinuation,p->impact,p->crashDelta,p->intermediateContacts));
 }
 for(i=0;i<ngFreeClimbProofCount;i++) {
  const ngFreeClimbProof *p=&ngFreeClimbProofs[i];
  NG_Text(va("%s{\"kind\":\"free_climb\",\"approach\":%d,\"launch\":%d,\"landing\":%d,\"exported\":%s,\"model\":\"tce_free_climb_capsule_speed_samples_v1\",\"endpoint_rule\":\"measured_lip_and_landing_floor_v1\",\"contact_min_ledge\":63,\"minimum_endpoint_rise\":18.25,\"wishspeed_samples\":[114,160,204],\"sample_count\":3,\"step_seconds\":0.008,\"initial_stamina\":2500,\"initial_impulse_z\":%.4f,\"max_ledge\":%.4f,\"launch_radius\":2,\"duration_seconds\":%.4f,\"climb_seconds\":%.4f,\"stamina_used\":%.4f,\"touchdown\":[%.4f,%.4f,%.4f],\"direction\":[%.4f,%.4f,%.4f],\"landing_speed_z\":%.4f,\"crashland_delta\":%.4f,\"live_verified\":false}",
   ngLadderProofCount||ngJumpProofCount||ngDropProofCount||i?",":"",ngNodes[p->approach].id,ngNodes[p->launch].id,ngNodes[p->landing].id,NG_TrFreeTarget(p->launch)>=0?"true":"false",TCE_NavStandingImpulse(g_newbbox.integer),101.f*(g_newbbox.integer?1.25f:1.f),p->duration,p->climbSeconds,p->stamina,p->touchdown[0],p->touchdown[1],p->touchdown[2]+ngOrigin,p->direction[0],p->direction[1],p->direction[2],p->impact,p->crash));
 }
 NG_Text("],");
}
static void NG_ExternalShortcutReport(void);
static void NG_ExternalCoverageReport(void);
static void NG_ExternalInputNodeReport(int index);
static void NG_PathRepairReport(void);
static void NG_SpatialSparseReport(void) {
 int i;
 NG_Text(va("\"spatial_only\":%s,\"spatial_removed_chords\":%d,\"checks\":%d,\"check_limit\":%d,\"removal_limit\":%d,\"witnesses\":[",
  ngExternalSpatialImport&&!ngExternalManualDraft?"true":"false",ngSpatialChordCount,ngSpatialChordChecks,
  NG_SPATIAL_CHORD_CHECK_LIMIT,NG_SPATIAL_CHORD_LIMIT));
 for(i=0;i<ngSpatialChordCount;i++) {
  const ngSpatialChord *p=&ngSpatialChords[i];const ngEdge *e=&ngEdges[p->edge];
  NG_Text(va("%s{\"from\":%d,\"to\":%d,\"via\":%d,\"direct_length\":%.4f,\"witness_length\":%.4f,\"final_legs_accepted\":%s}",
   i?",":"",ngNodes[e->from].id,ngNodes[e->to].id,ngNodes[ngEdges[p->first].to].id,
   ngSparseLength[p->edge],ngSparseLength[p->first]+ngSparseLength[p->second],
   ngEdges[p->first].accepted&&ngEdges[p->second].accepted?"true":"false"));
 }
 NG_Text("]");
}
static qboolean NG_Report(const char *path,const char *source,const char *map,qboolean complete) {
 int i,conditional=0,exportedLadderLinks=0,exportedJumpLinks=0;
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted&&ngEdges[i].reason) {
  const ngNode *from=&ngNodes[ngEdges[i].from],*to=&ngNodes[ngEdges[i].to];
  qboolean manualLadder=(from->authored||to->authored)&&
   ((from->navFlags|to->navFlags)&NG_NAV_CLIMB)&&!((from->editRole|to->editRole)&128);
  qboolean manualJump=from->authored&&
   (((from->editRole&64)&&from->editDropId==to->id)||
    (!(from->editRole&64)&&(from->navFlags&(NG_NAV_JUMP|(1u<<20)|(1u<<21)))));
  if(!Q_stricmpn(ngEdges[i].reason,"ladder_",7)||manualLadder)exportedLadderLinks++;
  if(!Q_stricmpn(ngEdges[i].reason,"jump_",5)||manualJump)exportedJumpLinks++;
 }
 trap_FS_FOpenFile(path,&ngFile,FS_WRITE);if(!ngFile)return qfalse;ngIoFailed=qfalse;
 NG_Text(va("{\"quarantined_unreachable_nodes\":%d,\"sparsification\":{\"removed_chords\":%d,\"max_stretch\":1.25,",ngQuarantinedCount,ngSparseRemoved));
 NG_SpatialSparseReport();NG_Text("},");
 NG_Text(va("\"schema\":1,\"generation_complete\":%s,\"map\":\"%s\",\"source\":\"%s\",\"traces\":%d,\"trace_budget\":%d,\"bbox\":{\"half_width\":%.1f,\"stand_height\":%.1f,\"crouch_height\":%.1f,\"origin_above_feet\":%.1f},\"limitations\":\"Bounded map-floor sampling and conservative traversal candidates; full-map coverage and live traversal unverified. Binary doors require genuine runtime activation and fresh passage checks; timed returns remain live pending. Grounded wading uses posture-specific PM_SetWaterLevel samples; swimming, general moving platforms and unproved script actions unsupported. Optional WPS metadata preserved.\",",complete?"true":"false",map,source,ngTraces,NG_TRACE_BUDGET,ngWidth,ngStand,ngDuck,ngOrigin));
 NG_Text(va("\"water_policy\":{\"model\":\"grounded_pm_middle_sample_wading_v2\",\"foot_hazard_probe_height\":0,\"pm_first_sample_height\":1,\"standing_middle_sample_height\":%.0f,\"crouching_middle_sample_height\":%.0f,\"integer_sample_truncation\":true,\"wet_middle_sample_rejected\":true,\"swimming_supported\":false,\"lava_slime_hurt_rejected\":true},",TCE_NavWaterMiddleSample(ngOrigin==30.f,0),TCE_NavWaterMiddleSample(ngOrigin==30.f,1)));
 NG_Text("\"conditional_ground\":{\"model\":\"stationary_destructible_support_rechecked_each_step_v1\",\"immutable_world_proof\":false,\"runtime_support_revalidation_required\":true,\"surfaces\":[");
 for(i=MAX_CLIENTS;i<level.num_entities;i++)if(ngConditionalGroundHits[i]) {
  gentity_t *ent=&g_entities[i];
  NG_Text(va("%s{\"entity\":%d,\"modelindex\":%d,\"classname\":\"func_explosive\",\"health\":%d,\"linked\":%s,\"contents\":%d,\"pos_trtype\":%d,\"apos_trtype\":%d,\"support_checks\":%u,\"origin\":[%.4f,%.4f,%.4f],\"angles\":[%.4f,%.4f,%.4f]}",
   conditional++?",":"",i,ent->s.modelindex,ent->health,ent->r.linked?"true":"false",ent->r.contents,ent->s.pos.trType,ent->s.apos.trType,ngConditionalGroundHits[i],
   ent->r.currentOrigin[0],ent->r.currentOrigin[1],ent->r.currentOrigin[2],ent->r.currentAngles[0],ent->r.currentAngles[1],ent->r.currentAngles[2]));
 }
 NG_Text(va("],\"surface_count\":%d},",conditional));
 NG_Text(va("\"generation_mode\":\"%s\",\"preserve_spatial_samples\":%s,",ngExternalSpatialImport?"spatial_surface_samples":(ngExternalManualDraft?"manual_draft":"legacy_automatic"),ngExternalSpatialImport?"true":"false"));
 NG_Text(va("\"candidate_kind\":\"%s\",\"candidate_mode\":\"%s\",\"graph_policy\":\"%s\",",ngExternalImport?"external_bsp_recast":"native_floor_sampling",ngExternalImport?(ngExternalManualDraft?"manual_draft":(ngExternalSpatialImport?"automatic_spatial":"automatic")):"automatic",ngExternalManualDraft?"preserve_manual_directions":(ngExternalSpatialImport?"preserve_spatial_samples":"legacy_sparse_cleanup")));
 NG_Text(va("\"waypoint_metadata_contract\":\"TCE_NODE_METADATA_1\",\"metadata_input_count\":%d,",ngMetadataInputCount));
 NG_Text(va("\"custom_property_contract\":\"TCE_NODE_PROPERTIES_1\",\"custom_property_input_count\":%d,",ngCustomPropertyCount));
 if(ngExternalImport){NG_ExternalShortcutReport();NG_ExternalCoverageReport();NG_PathRepairReport();}
 NG_TrReport();NG_DoorReport();NG_ArrivalReport();
 NG_Text(va("\"detour_simplification\":{\"search_points\":%d,\"exported_points\":%d},\"coverage\":{\"method\":\"%s\",\"trace_ceiling\":%d,\"passes\":%d,\"spacing\":%d,\"minimum_spacing\":%d,\"full_map_verified\":false,\"samples\":%d,\"rejected\":%d,\"added_nodes\":%d,\"expanded_nodes\":%d,\"pending_nodes\":%d,\"spawn_connected_pending_nodes\":%d,\"stop_reason\":\"%s\",\"node_budget\":%d,\"edge_budget\":%d},\"camp_candidates\":%d,\"playability\":{\"spawn_reachability_available\":%s,\"spawn_reachable_nodes\":%d,\"isolated_geometry_nodes\":%d,\"islands_exported_for_review\":false},",ngDetourRaw,ngDetourKept,ngExternalImport?"external_candidate_native_collision_gate":"absolute_lattice_floor_frontier",ngCoverageTraceCeiling,ngCoveragePasses,ngExternalImport?0:64,ngExternalImport?0:48,ngCoverageSamples,ngCoverageRejected,ngCoverageAdded,ngCoverageExpanded,ngCoverageRemaining,ngCoverageSpawnPending,ngCoverageStop,NG_NODES,NG_EDGES,ngCampCount,ngSpawnSeeds?"true":"false",ngReachableCount,ngIsolatedCount));
 NG_Text(va("\"doorway_search\":{\"candidates\":%d,\"connected\":%d,\"turnpoints\":%d,\"traces\":%d,\"stop_reason\":\"%s\"},",ngPortalCandidates,ngPortalConnected,ngPortalAdded,ngPortalTraces,ngPortalStop));
 NG_Text(va("\"simplification\":{\"contracted\":%d,\"orphans_excluded\":%d,\"links_added\":%d,\"candidates\":%d,\"topology_rejected\":%d,\"new_link_limit\":256,\"stop_reason\":\"%s\"},",ngSimplifyRemoved,ngSimplifyOrphans,ngSimplifyLinks,ngSimplifyCandidates,ngSimpleTopologyRejected,ngSimplifyStop));
 NG_Text(va("\"map_geometry\":{\"bounds_valid\":%s,\"scan_finished\":%s,\"spawn_seeds\":%d,\"floor_seeds\":%d,\"samples\":%d,\"xy_spacing\":%d,\"z_spacing\":%d,\"sample_budget\":%d},\"traversal\":{\"bsp_ladder_surfaces\":%d,\"bsp_ladder_pairs\":%d,\"jump_candidates\":%d,\"jump_links\":%d,\"jump_constructed_links\":%d,\"ladder_candidates\":%d,\"ladder_links\":%d,\"ladder_constructed_links\":%d,\"counts_refer_to_exported_edges\":true,\"live_verified\":false},\"nodes\":[",ngGeometryBoundsValid?"true":"false",ngGeometryScanComplete?"true":"false",ngSpawnSeeds,ngGeometrySeeds,ngGeometrySamples,ngExternalImport?0:192,ngExternalImport?0:128,ngExternalImport?0:50000,ngBspLadderSurfaces,ngBspLadderPairCount,ngJumpCandidates,exportedJumpLinks,ngJumpLinks,ngLadderCandidates,exportedLadderLinks,ngLadderLinks));
 for(i=0;i<ngCount;i++){
  ngNode *n=&ngNodes[i];
  NG_Text(va("%s{\"id\":%d,\"index\":%d,\"position\":[%.4f,%.4f,%.4f],\"raw_position\":[%.4f,%.4f,%.4f],\"accepted\":%s,\"crouch\":%s,\"topology_protected\":%s,\"flags\":%d,\"packed\":%d,\"spawn_reachable\":%s,\"nav_flags\":%llu,\"radius\":%.4f,\"name\":",i?",":"",n->id,n->index,n->feet[0],n->feet[1],n->feet[2]+ngOrigin,n->raw[0],n->raw[1],n->raw[2],n->accepted?"true":"false",n->crouch?"true":"false",n->topologyProtected?"true":"false",n->flags,n->packed,ngSpawnReachable[i]?"true":"false",NG_OutputFlags(n),n->radius>0.f?n->radius:8.f));
  NG_ExternalJsonString(n->name);
  NG_Text(va(",\"way_guid\":%u,\"import_uid\":%u,\"metadata_supplied\":%s,\"facing\":[%.4f,%.4f,%.4f],\"reason\":\"%s\"",n->accepted?n->outputUID:0,NG_ImportUID(n),n->metadataSupplied&&n->id==n->editSourceId?"true":"false",n->facing[0],n->facing[1],n->facing[2],n->reason));
  NG_Text(",\"custom_properties\":{");
  if(NG_OutputPropertyCount(n)) {
   int property,emitted=0;
   for(property=n->propertyHead;property;property=ngCustomProperties[property-1].next) {
    const ngCustomProperty *custom=&ngCustomProperties[property-1];
    if(emitted++)NG_Text(",");NG_ExternalJsonString(ngCustomPropertyText+custom->key);
    NG_Text(":");NG_ExternalJsonString(ngCustomPropertyText+custom->value);
   }
  }
  NG_Text("}");
  NG_ArrivalReportNode(i);NG_ExternalInputNodeReport(i);NG_DoorNodeReport(i);NG_Text("}");
 }
 NG_Text("],\"edges\":[");for(i=0;i<ngEdgeCount;i++){ngEdge *e=&ngEdges[i];NG_Text(va("%s{\"from\":%d,\"to\":%d,\"accepted\":%s,\"crouch\":%s,\"added\":%s,\"reason\":\"%s\"}",i?",":"",ngNodes[e->from].id,e->to<0?-1:ngNodes[e->to].id,e->accepted?"true":"false",e->crouch?"true":"false",e->added?"true":"false",e->reason));}NG_Text("]}\n");trap_FS_FCloseFile(ngFile);return !ngIoFailed;
}
#include "tce_navgen_external.inc"

const char *TCE_NavgenValidateSpawn(const vec3_t origin,vec3_t feet,int *traces) {
 int n,savedTraces=ngTraces,savedBudget=ngTraceBudget;vec3_t from;const char *why;
 float savedWidth=ngWidth,savedOrigin=ngOrigin,savedStand=ngStand;qboolean savedProbe=ngProbe;
 for(n=0;n<3;n++)if(!NG_Finite(origin[n])||fabsf(origin[n])>131072.f)return "invalid_coordinates";
 ngWidth=g_newbbox.integer?16.f:14.f;ngOrigin=g_newbbox.integer?30.f:24.f;ngStand=g_newbbox.integer?87.f:70.f;
 ngTraces=0;ngTraceBudget=NG_TRACE_BASE_BUDGET;ngProbe=qfalse;
 VectorCopy(origin,from);from[2]-=ngOrigin;VectorCopy(from,feet);
 why=NG_Ground(from,feet,ngStand);
 /* Authoring may not silently project a spawn onto another step/storey. */
 if(!why&&fabsf(feet[2]-from[2])>1.f)why="spawn_not_on_selected_floor";
 if(traces)*traces=ngTraces;
 ngTraces=savedTraces;ngTraceBudget=savedBudget;ngWidth=savedWidth;ngOrigin=savedOrigin;ngStand=savedStand;ngProbe=savedProbe;
 return why;
}

/* Read-only identity/physics receipt in the same response as the probe.
 * Clients need no separate map/gametype RCON round trips per body query. */
static void NG_ProbeContext(void) {
 const unsigned char *p=(const unsigned char *)level.rawmapname;
 static const char hex[]="0123456789abcdef";
 char escaped[6*MAX_QPATH+1];int used=0;
 for(;*p&&used<(int)sizeof(escaped)-6;p++) {
  if(*p=='"'||*p=='\\'){escaped[used++]='\\';escaped[used++]=(char)*p;}
  else if(*p<32){escaped[used++]='\\';escaped[used++]='u';escaped[used++]='0';escaped[used++]='0';escaped[used++]=hex[*p>>4];escaped[used++]=hex[*p&15];}
  else escaped[used++]=(char)*p;
 }
 escaped[used]=0;
 G_Printf("navgen probe context: {\"schema\":1,\"map\":\"%s\",\"gametype\":%d,\"g_newbbox\":%d,\"bbox\":{\"half_width\":%.0f,\"stand_height\":%.0f,\"crouch_height\":%.0f,\"origin_above_feet\":%.0f},\"gravity\":%.9g}\n",
  escaped,g_gametype.integer,g_newbbox.integer,ngWidth,ngStand,ngDuck,ngOrigin,NG_TrGravity());
}

qboolean TCE_NavgenCommand(void) {
 char cmd[64],source[MAX_QPATH],output[MAX_QPATH],report[MAX_QPATH],expected[MAX_QPATH];const char *basename;int i,j,k,count=0,accepted=0,start; qboolean ok;
 trap_Argv(0,cmd,sizeof(cmd));
 if(!Q_stricmp(cmd,"tce_navgen_authoring")){
  char mode[16];trap_Argv(1,mode,sizeof(mode));
  if(!Q_stricmp(mode,"off")){ngAuthoring=qfalse;G_Printf("NAVSTUDIO_AUTHORING_OFF\n");return qtrue;}
  if(!Q_stricmp(mode,"on")){
   if(!trap_Cvar_VariableIntegerValue("sv_cheats") || trap_Cvar_VariableIntegerValue("dedicated")){
    G_Printf("NAVSTUDIO_AUTHORING_REFUSED: local devmap listen game required\n");return qtrue;
   }
   ngAuthoring=qtrue;ngAuthoringStarted=(unsigned int)trap_Milliseconds();
   G_Printf("NAVSTUDIO_AUTHORING_ON: warmup and round transitions held for up to 600 seconds\n");return qtrue;
  }
  G_Printf("NAVSTUDIO_AUTHORING active=%d\n",TCE_NavgenAuthoringActive());return qtrue;
 }
 if(!Q_stricmp(cmd,"tce_navgen_camera")){
  char arg[40];vec3_t from,to,mins,maxs;trace_t trace;int n;float height;
  if(trap_Argc()!=5){G_Printf("usage: tce_navgen_camera <player-origin x y z> <height>\n");return qtrue;}
  for(n=0;n<3;n++){trap_Argv(n+1,arg,sizeof(arg));from[n]=(float)atof(arg);}
  trap_Argv(4,arg,sizeof(arg));height=(float)atof(arg);
  for(n=0;n<3;n++)if(!NG_Finite(from[n])||fabsf(from[n])>131072.f){G_Printf("navgen camera: {\"accepted\":false,\"reason\":\"invalid_coordinates\"}\n");return qtrue;}
  if(!NG_Finite(height)||height<16.f||height>512.f){G_Printf("navgen camera: {\"accepted\":false,\"reason\":\"invalid_height\"}\n");return qtrue;}
  VectorCopy(from,to);to[2]+=height;VectorSet(mins,-4,-4,-4);VectorSet(maxs,4,4,4);
  trap_Trace(&trace,from,mins,maxs,to,ENTITYNUM_NONE,NG_MASK);
  if(trace.fraction<1.f){trace.endpos[2]-=4.f;if(trace.endpos[2]<from[2])trace.endpos[2]=from[2];}
  G_Printf("navgen camera: {\"accepted\":%s,\"map\":\"%s\",\"position\":[%.4f,%.4f,%.4f],\"clamped\":%s}\n",
   trace.startsolid||trace.allsolid?"false":"true",level.rawmapname,trace.endpos[0],trace.endpos[1],trace.endpos[2],trace.fraction<1.f?"true":"false");
  return qtrue;
 }
 if(!Q_stricmp(cmd,"tce_navgen_entities"))return NG_ExternalEntities();
 if(!Q_stricmp(cmd,"tce_navgen_import"))return NG_ExternalImport();
 if(!Q_stricmp(cmd,"tce_navgen_spawncheck")){
  char arg[40];vec3_t origin,feet;const char *why;int n,traces=0;
  if(trap_Argc()!=4){G_Printf("usage: tce_navgen_spawncheck <x y z> (player origin)\n");return qtrue;}
  for(n=0;n<3;n++){trap_Argv(1+n,arg,sizeof(arg));origin[n]=(float)atof(arg);}
  for(n=0;n<3;n++)if(!NG_Finite(origin[n])||fabsf(origin[n])>131072.f){G_Printf("navgen spawncheck: {\"accepted\":false,\"reason\":\"invalid_coordinates\"}\n");return qtrue;}
  why=TCE_NavgenValidateSpawn(origin,feet,&traces);
  G_Printf("navgen spawncheck: {\"accepted\":%s,\"reason\":\"%s\",\"map\":\"%s\",\"origin\":[%.5f,%.5f,%.5f],\"feet\":[%.5f,%.5f,%.5f],\"origin_above_feet\":%.0f,\"traces\":%d}\n",
   why?"false":"true",why?why:"native_standing_floor_validated",level.rawmapname,origin[0],origin[1],origin[2],feet[0],feet[1],feet[2],g_newbbox.integer?30.f:24.f,traces);return qtrue;
 }
 if(!Q_stricmp(cmd,"tce_navgen_exitprobe")){
  char arg[32];vec3_t from,to,normal,landed;float duration=0,speed=0;int n;qboolean result;
  if(trap_Argc()!=9){G_Printf("usage: tce_navgen_exitprobe <x y z> <target-x y z> <outward-normal-x y> (player origins)\n");return qtrue;}
  ngWidth=g_newbbox.integer?16.f:14.f;ngOrigin=g_newbbox.integer?30.f:24.f;ngStand=g_newbbox.integer?87.f:70.f;
  for(n=0;n<3;n++){trap_Argv(1+n,arg,sizeof(arg));from[n]=(float)atof(arg);trap_Argv(4+n,arg,sizeof(arg));to[n]=(float)atof(arg);}
  for(n=0;n<2;n++){trap_Argv(7+n,arg,sizeof(arg));normal[n]=(float)atof(arg);}normal[2]=0;
  if(!NG_Finite(from[0])||!NG_Finite(from[1])||!NG_Finite(from[2])||!NG_Finite(to[0])||!NG_Finite(to[1])||!NG_Finite(to[2])||!NG_Finite(normal[0])||!NG_Finite(normal[1])||VectorNormalize(normal)<.9f){G_Printf("navgen exitprobe: invalid coordinates/normal\n");return qtrue;}
  from[2]-=ngOrigin;to[2]-=ngOrigin;ngTraces=0;ngExitDebug=qtrue;
  result=NG_TrLadderDismount(from,to,normal,landed,&duration,&speed);ngExitDebug=qfalse;
  G_Printf("navgen exitprobe: %s contact%.4f total%.4f speed%.4f traces%d\n",result?"accepted":"rejected",ngLadderProofWorking.contactDuration,duration,speed,ngTraces);return qtrue;
 }
 if(!Q_stricmp(cmd,"tce_navgen_probe")||!Q_stricmp(cmd,"tce_navgen_jumpprobe")||!Q_stricmp(cmd,"tce_navgen_climbprobe")){
  char arg[32];int a,b,savedTraces=ngTraces,savedDiagnostics=ngStepDiagnostics,savedDropTests=ngDropTests,savedDropAccepted=ngDropAccepted;
  const char *stand,*duck,*savedDropFailure=ngDropFailure;ngNode from,to;qboolean savedProbe=ngProbe;int savedBudget=ngTraceBudget,n;
  qboolean jumpOnly=!Q_stricmp(cmd,"tce_navgen_jumpprobe");
  qboolean climbOnly=!Q_stricmp(cmd,"tce_navgen_climbprobe");
  float savedWidth=ngWidth,savedStand=ngStand,savedDuck=ngDuck,savedOrigin=ngOrigin;vec3_t savedDropFailurePoint;
  unsigned int savedConditionalGroundHits[MAX_GENTITIES];
  trap_Argv(1,arg,sizeof(arg));
  if(trap_Argc()==8&&!Q_stricmp(arg,"feet")) {
   /* Literal feet coordinates keep diagnostics usable after a normal round
    * initialization clears the generator cache. Never project/change them. */
   memset(&from,0,sizeof(from));memset(&to,0,sizeof(to));from.id=to.id=-1;from.accepted=to.accepted=qtrue;
   for(n=0;n<6;n++){float value;char *end;
    trap_Argv(n+2,arg,sizeof(arg));value=(float)strtod(arg,&end);
    if(end==arg||*end||!NG_Finite(value)||fabs(value)>131072.f){G_Printf("navgen probe: invalid literal feet coordinate\n");return qtrue;}
    if(n<3)from.feet[n]=value;else to.feet[n-3]=value;
   }
  } else {
   if(trap_Argc()!=3||!ngCount){G_Printf("usage: tce_navgen_probe <source-id> <target-id> after tce_navgen; or tce_navgen_probe feet <x y z> <x y z> (feet coordinates)\n");return qtrue;}
   trap_Argv(1,arg,sizeof(arg));a=NG_Find(atoi(arg));trap_Argv(2,arg,sizeof(arg));b=NG_Find(atoi(arg));
   if(a<0||b<0||
      (!ngNodes[a].accepted&&!(ngExternalImport&&a<ngExternalSuppliedCount&&ngExternalFloorAccepted[a]))||
      (!ngNodes[b].accepted&&!(ngExternalImport&&b<ngExternalSuppliedCount&&ngExternalFloorAccepted[b]))) {
    G_Printf("navgen probe: unknown endpoint or rejected native floor\n");return qtrue;
   }
   from=ngNodes[a];to=ngNodes[b];
  }
  VectorCopy(ngDropFailurePoint,savedDropFailurePoint);memcpy(savedConditionalGroundHits,ngConditionalGroundHits,sizeof(savedConditionalGroundHits));
  ngWidth=g_newbbox.integer?16.f:14.f;ngOrigin=g_newbbox.integer?30.f:24.f;ngStand=g_newbbox.integer?87.f:70.f;ngDuck=g_newbbox.integer?61.f:49.f;
  NG_ProbeContext();
  ngTraceBudget=16000;
  /* A missing spawn route is not a geometric rejection. Diagnose copies of
   * floor-validated input points without unquarantining them or editing links. */
  ngTraces=ngStepDiagnostics=0;ngProbe=qtrue;
  if(climbOnly)NG_TrProbeFreeClimb(&from,&to,16000);
  else if(jumpOnly)NG_TrProbeJumps(&from,&to,16000);
  else {
  stand=NG_Walk(&from,&to,ngStand);duck=NG_Walk(&from,&to,ngDuck);
  G_Printf("navgen probe %d->%d feet=(%.2f %.2f %.2f)->(%.2f %.2f %.2f) stand=%s crouch=%s traces=%d quarantined=%d/%d (diagnostic only; graph unchanged)\n",from.id,to.id,from.feet[0],from.feet[1],from.feet[2],to.feet[0],to.feet[1],to.feet[2],stand?stand:"accepted",duck?duck:"accepted",ngTraces,!from.accepted,!to.accepted);
  NG_ProbeImportOrdinary(&from,&to);
  NG_ProbeImportDropLaunch(&from,&to,stand);
  if(to.feet[2]<from.feet[2]-18.25f) {
   ngDropProof proof;qboolean accepted=NG_TrWalkDrop(&from,&to,16000,&proof);
   G_Printf("navgen probe drop %d->%d accepted=%d reason=%s crash_limit=38 max_drop=%.4f delta=%.4f landing=(%.4f %.4f %.4f) (readonly samples)\n",from.id,to.id,accepted,ngDropFailure?ngDropFailure:"unknown",NG_TrSafeDropHeight(),accepted?proof.crashDelta:0.f,accepted?proof.touchdown[0]:ngDropFailurePoint[0],accepted?proof.touchdown[1]:ngDropFailurePoint[1],(accepted?proof.touchdown[2]:ngDropFailurePoint[2])+ngOrigin);
  } else if(to.feet[2]-from.feet[2]>=63.f) {
   vec3_t d,end;trace_t hit;ngNode launch=from;ngFreeClimbProof proof;qboolean accepted=qfalse;float length;
   VectorSubtract(to.feet,from.feet,d);d[2]=0;length=VectorNormalize(d);VectorMA(from.feet,length,d,end);
   if(NG_Trace(&hit,from.feet,end,ngStand)&&!hit.startsolid&&!hit.allsolid&&hit.fraction<1.f&&hit.entityNum==ENTITYNUM_WORLD&&fabs(hit.plane.normal[2])<.001f) {
    VectorMA(hit.endpos,.125f,hit.plane.normal,end);
    if(!NG_Ground(end,launch.feet,ngStand)&&fabs(launch.feet[2]-from.feet[2])<=1.f&&!NG_Walk(&from,&launch,ngStand))accepted=NG_TrFreeClimb(&launch,&to,16000,&proof);
   }
   G_Printf("navgen probe freeclimb %d->%d accepted=%d launch=(%.4f %.4f %.4f) max_ledge=%.4f duration=%.4f stamina_used=%.4f (readonly3speed samples; graph unchanged)\n",from.id,to.id,accepted,launch.feet[0],launch.feet[1],launch.feet[2]+ngOrigin,101.f*(g_newbbox.integer?1.25f:1.f),accepted?proof.duration:0.f,accepted?proof.stamina:0.f);
  } else if(to.feet[2]-from.feet[2]>18.25f) {
   NG_TrProbeJumps(&from,&to,16000);
  }
  }
  ngDropTests=savedDropTests;ngDropAccepted=savedDropAccepted;
  ngDropFailure=savedDropFailure;VectorCopy(savedDropFailurePoint,ngDropFailurePoint);memcpy(ngConditionalGroundHits,savedConditionalGroundHits,sizeof(savedConditionalGroundHits));
  ngTraces=savedTraces;ngStepDiagnostics=savedDiagnostics;ngProbe=savedProbe;ngTraceBudget=savedBudget;
  ngWidth=savedWidth;ngStand=savedStand;ngDuck=savedDuck;ngOrigin=savedOrigin;return qtrue;
 }
 if(Q_stricmp(cmd,"tce_navgen"))return qfalse;
 if(trap_Argc()>2){G_Printf("usage: tce_navgen [optional_botroutes/currentmap.wps]\n");return qtrue;}
 if(!NG_SafePath(level.rawmapname)||strchr(level.rawmapname,'/')){G_Printf("navgen: invalid loaded map\n");return qtrue;}
 Q_strncpyz(source,"map_geometry",sizeof(source));
 ngTraceBudget=NG_TRACE_BASE_BUDGET;
 ngExternalImport=ngExternalManualDraft=ngExternalSpatialImport=qfalse;ngMetadataInputCount=ngCustomPropertyCount=ngCustomPropertyBytes=0;
 memset(ngConditionalGroundHits,0,sizeof(ngConditionalGroundHits));
 ngDetourRaw=ngDetourKept=ngCoverageSamples=ngCoverageRejected=ngCoverageAdded=ngCoverageExpanded=ngCoverageRemaining=0;ngCoverageStop="not_started";
 ngGeometrySamples=ngGeometrySeeds=ngSpawnSeeds=0;ngGeometryBoundsValid=ngGeometryScanComplete=qfalse;ngCoveragePasses=ngCoverageTraceCeiling=0;
 start=trap_Milliseconds();NG_TrCacheReset();ngTraces=ngEdgeCount=ngStepDiagnostics=0;ngWidth=g_newbbox.integer?16.f:14.f;ngOrigin=g_newbbox.integer?30.f:24.f;ngStand=g_newbbox.integer?87.f:70.f;ngDuck=g_newbbox.integer?61.f:49.f;
 if(trap_Argc()==2){
  trap_Argv(1,source,sizeof(source));Com_sprintf(expected,sizeof(expected),"%s.wps",level.rawmapname);
  basename=strrchr(source,'/');basename=basename?basename+1:source;
  if(Q_stricmp(basename,expected)){G_Printf("navgen: optional WPS basename must match loaded map %s\n",expected);return qtrue;}
  if(!NG_SafePath(source)||!NG_Read(source)){G_Printf("navgen: invalid/unavailable optional WPS v2 input %s\n",source);return qtrue;}
  for(i=0;i<ngCount;i++){NG_Node(&ngNodes[i]);if(ngNodes[i].accepted)ngNodes[i].index=count++;}
 }else NG_MapSeeds();
 for(i=0;i<ngCount;i++)for(j=0;j<ngNodes[i].count;j++)NG_Edge(i,NG_Find(ngNodes[i].targets[j]),ngNodes[i].linkFlags[j],qfalse);
 /* At most four closest extra candidates per node, 256 unit radius. Never
  * restore a rejected original link as an unflagged generated connection. */
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted){int best[4]={-1,-1,-1,-1};float dist[4]={65536,65536,65536,65536};
  for(j=0;j<ngCount;j++)if(j!=i&&ngNodes[j].accepted){vec3_t d;float ds;qboolean existing=qfalse;
   for(k=0;k<ngNodes[i].count;k++)if(ngNodes[i].targets[k]==ngNodes[j].id)existing=qtrue;if(existing)continue;
   VectorSubtract(ngNodes[i].feet,ngNodes[j].feet,d);ds=DotProduct(d,d);for(k=0;k<4;k++)if(ds<dist[k]){int t;for(t=3;t>k;t--){dist[t]=dist[t-1];best[t]=best[t-1];}dist[k]=ds;best[k]=j;break;}
  }for(k=0;k<4;k++)if(best[k]>=0)NG_Edge(i,best[k],0,qtrue);
 }
 if(trap_Argc()==2)NG_Repair();
 NG_Coverage(qfalse);
 NG_Portals();
 NG_Traversal();
 NG_Doors();
 NG_SpawnReachability();
 /* One bounded continuation explores only newly reachable, unvisited floors.
  * Existing work and counts are retained; no recursive traversal generation. */
 if(ngCoverageSpawnPending&&ngTraces<NG_TRACE_BUDGET-10000&&ngCount<NG_NODES-128)NG_Coverage(qtrue);
 NG_SpawnReachability();
 NG_Camps();
 NG_SpawnReachability();
 NG_QuarantineUnreachable();
 NG_TrPruneIncomplete();
 NG_Simplify();
 NG_Sparsify();
 NG_TrPruneSeedSpokes();
 NG_FinalizeArrivalRadii();
 NG_SpawnReachability();
 count=0;for(i=0;i<ngCount;i++)if(ngNodes[i].accepted)ngNodes[i].index=count++;
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted)accepted++;
 Com_sprintf(output,sizeof(output),"navgen/%s_auto.way",level.rawmapname);Com_sprintf(report,sizeof(report),"navgen/%s_auto.json",level.rawmapname);
 ok=count&&accepted&&!ngTrCacheFailed&&ngTraces<NG_TRACE_BUDGET&&NG_TrSeedSpokesValid(qtrue);
 if(ok)ok=NG_Save(output,count);
 ok=NG_Report(report,source,level.rawmapname,ok)&&ok;
 G_Printf("navgen: %s nodes=%d/%d edges=%d/%d traces=%d time=%dms output=%s report=%s (STAGED, not installed)\n",ok?"complete":"FAILED",count,ngCount,accepted,ngEdgeCount,ngTraces,trap_Milliseconds()-start,output,report);
 return qtrue;
}
