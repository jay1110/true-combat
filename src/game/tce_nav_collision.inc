/* Shared physical spawn/generator checks. The including translation unit
 * supplies its private trace budget, actor dimensions and ground accounting. */
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
#ifndef TCE_NAV_RUNTIME
  if(ngProbe)G_Printf("navgen ground stage=foot solid=%d/%d entity=%d start=(%.4f %.4f %.4f) end=(%.4f %.4f %.4f)\n",tr.startsolid,tr.allsolid,tr.entityNum,a[0],a[1],a[2],b[0],b[1],b[2]);
#endif
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
#ifndef TCE_NAV_RUNTIME
  if(ngProbe)G_Printf("navgen ground stage=body solid=%d/%d entity=%d height=%.2f start=end=(%.4f %.4f %.4f)\n",tr.startsolid,tr.allsolid,tr.entityNum,height,feet[0],feet[1],feet[2]);
  NG_ProbeBodyRepresentations(feet,height);
#endif
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
