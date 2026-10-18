/* Explicit, append-only repair of a complete authored graph. Search state is
 * private; no node/edge is appended until every segment of a bounded chain
 * has a fresh native walking proof. Existing rejected pairs stay rejected.
 * A jump launch is a terminal for incoming walking only, never a new exit. */
#define NG_PATH_REPAIR_CHAINS 32
#define NG_PATH_REPAIR_POINTS 32
#define NG_PATH_REPAIR_SEARCHES 16
#define NG_PATH_REPAIR_TRACES 300000
typedef struct { int count,indices[NG_PATH_REPAIR_POINTS];qboolean jump,crouch[NG_PATH_REPAIR_POINTS-1]; } ngPathRepairChain;
static ngPathRepairChain ngPathRepairChains[NG_PATH_REPAIR_CHAINS];
static int ngPathRepairChainCount,ngPathRepairInputNodes,ngPathRepairInputEdges;
static int ngPathRepairMinimum,ngPathRepairBegin,ngPathRepairTraces,ngPathRepairLimit;
static qboolean ngPathRepairRun,ngPathRepairPreserved,ngPathRepairBudgetComplete;
static const char *ngPathRepairStop;
static unsigned char ngPathRepairMain[NG_NODES],ngPathRepairTried[NG_NODES];

static void NG_PathRepairReset(void) {
 ngPathRepairRun=ngPathRepairPreserved=ngPathRepairBudgetComplete=qfalse;
 ngPathRepairChainCount=ngPathRepairInputNodes=ngPathRepairInputEdges=0;
 ngPathRepairMinimum=ngPathRepairBegin=ngPathRepairTraces=ngPathRepairLimit=0;
 ngPathRepairStop="not_requested";
 memset(ngPathRepairChains,0,sizeof(ngPathRepairChains));
}
/* The append-only implementation never writes an old record. This guard
 * catches accidental changes; the caller also compares all original
 * serialized metadata and pairs before merging this proposal. */
static unsigned long long NG_PathRepairInputHash(void) {
 unsigned long long hash=1469598103934665603ULL;int i,j;
 for(i=0;i<ngPathRepairInputNodes;i++) {
  const unsigned char *p=(const unsigned char *)&ngNodes[i];
  for(j=0;j<(int)sizeof(ngNode);j++){hash^=p[j];hash*=1099511628211ULL;}
 }
 for(i=0;i<ngPathRepairInputEdges;i++) {
  const unsigned char *p=(const unsigned char *)&ngEdges[i];
  for(j=0;j<(int)sizeof(ngEdge);j++){hash^=p[j];hash*=1099511628211ULL;}
 }
 return hash;
}
static qboolean NG_PathRepairOrdinary(int index,qboolean crouch) {
 const ngNode *n=&ngNodes[index];unsigned long long flags=NG_OutputFlags(n);
 return n->accepted&&!n->flags&&!n->packed&&!n->navFlags&&!n->editRole&&!n->door.mode&&
  flags==(crouch?64ULL:0ULL)&&n->crouch==crouch;
}
static qboolean NG_PathRepairGoal(int index,qboolean crouch) {
 return NG_PathRepairOrdinary(index,crouch)||(crouch&&NG_PathRepairOrdinary(index,qfalse));
}
static qboolean NG_PathRepairReimport(ngNode *candidate,qboolean crouch) {
 ngNode imported;qboolean exact;vec3_t literal;char number[64];int axis;
 /* Reports carry origins to four decimals; the editor subtracts ngOrigin.
  * Check that literal representation, not an unexported high precision
  * search pose. A different floor/stance is rejected, never snapped. */
 for(axis=0;axis<3;axis++) {
  Com_sprintf(number,sizeof(number),"%.4f",candidate->feet[axis]+(axis==2?ngOrigin:0.f));
  literal[axis]=(float)strtod(number,NULL)-(axis==2?ngOrigin:0.f);
 }
 if(!NG_ImportOrdinaryCopy(literal,crouch,&imported,&exact)||!exact)return qfalse;
 VectorCopy(imported.feet,candidate->feet);candidate->crouch=crouch;
 return qtrue;
}
static qboolean NG_PathRepairJump(int index) {
 const ngNode *n=&ngNodes[index];ngNode copy;int k,outgoing=0;
 unsigned long long flags=NG_OutputFlags(n),allowed=(1ULL<<27)|(1ULL<<18)|(1ULL<<8);
 if(!n->accepted||n->crouch||n->flags||n->packed||n->door.mode||!flags||(flags&~allowed)||
    (n->editRole&~(1|64)))return qfalse;
 for(k=0;k<ngPathRepairInputEdges;k++)if(ngEdges[k].from==index) {
  if(!ngEdges[k].accepted)return qfalse;
  outgoing++;
 }
 if(outgoing!=1)return qfalse;
 copy=*n;return NG_EditorJumpLaunchFloor(&copy);
}
static float NG_PathRepairDistance(const vec3_t point,qboolean crouch) {
 int i;float best=384.f*384.f;
 for(i=0;i<ngPathRepairInputNodes;i++)if(ngPathRepairMain[i]&&NG_PathRepairGoal(i,crouch)) {
  vec3_t delta;float squared;VectorSubtract(point,ngNodes[i].feet,delta);
  squared=DotProduct(delta,delta);if(squared<best)best=squared;
 }
 return sqrt(best);
}
static qboolean NG_PathRepairWalk(const ngNode *from,const ngNode *to,qboolean jump,qboolean crouch,int limit) {
 vec3_t delta;float height=crouch?ngDuck:ngStand;int reserve;
 VectorSubtract(to->feet,from->feet,delta);
 /* Reserve the worst-case full-capsule/step/skin checks for both walks
  * before invoking them. Optional searches never raise the hard budget. */
 reserve=((int)ceil(VectorLength(delta)/8.f)+2)*32*(jump?1:2);
 if(ngTraces>=limit-reserve)return qfalse;
 /* Search runs outward from the terminal. For JUMP only the reverse
  * direction is a proposed connection; the action's exit is untouched. */
 if(NG_Walk(to,from,height))return qfalse;
 return jump||!NG_Walk(from,to,height);
}
static qboolean NG_PathRepairCommit(int seed,int goal,int found,qboolean jump,qboolean crouch,int limit,int *nextId) {
 int route[NG_PORTAL_SEARCH],chosen[NG_PATH_REPAIR_POINTS],indices[NG_PATH_REPAIR_POINTS];
 int length=0,count=1,at,current=0,i,j,k,newCount;ngPathRepairChain *chain;
 qboolean terminalCrouch=crouch&&NG_PathRepairOrdinary(goal,qtrue);
 const ngNode *from;
 for(i=found;i>0;i=ngPortalSearchNodes[i].parent) {
  if(length>=NG_PORTAL_SEARCH)return qfalse;route[length++]=i;
 }
 /* Existing rejected endpoint pairs may be repaired via bends, but never
  * by upgrading that pair. A bend is mandatory in every proposal. */
 if(!length)return qfalse;
 chosen[0]=0;
 while(current<length) {
  int pick=-1;
  from=&ngPortalSearchNodes[chosen[count-1]].node;
  for(at=length-1;at>=current;at--) {
   int candidate=route[length-1-at];
   if(NG_PathRepairWalk(from,&ngPortalSearchNodes[candidate].node,jump,crouch,limit)){pick=at;break;}
  }
  if(pick<0||count>=NG_PATH_REPAIR_POINTS-1)return qfalse;
  chosen[count++]=route[length-1-pick];current=pick+1;
 }
 /* Prune the final search samples only with a fresh direct proof. Keep
  * at least one new bend, even if an old endpoint pair could now pass. */
 for(i=1;i<count;i++)if(NG_PathRepairWalk(&ngPortalSearchNodes[chosen[i]].node,&ngNodes[goal],jump,terminalCrouch,limit)) {count=i+1;break;}
 if(i==count)return qfalse;
 newCount=count-1;
 if(ngCount+newCount>=NG_NODES||ngEdgeCount+(jump?1:2)*count>=NG_EDGES||
    ngPathRepairChainCount+(jump?1:2)>NG_PATH_REPAIR_CHAINS||
    *nextId<ngPathRepairMinimum||*nextId>0x7fffffff-newCount)return qfalse;
 /* Reimport every retained new bend once more before the final movement
  * proofs. Nothing is allocated if the normal editor pipeline rejects it. */
 for(i=1;i<count;i++)if(!NG_PathRepairReimport(&ngPortalSearchNodes[chosen[i]].node,crouch))return qfalse;
 /* Final proof covers every exact retained segment before any allocation. */
 for(i=0;i<count;i++) {
  const ngNode *a=i?&ngPortalSearchNodes[chosen[i]].node:&ngNodes[seed];
  const ngNode *b=i+1<count?&ngPortalSearchNodes[chosen[i+1]].node:&ngNodes[goal];
  if(!NG_PathRepairWalk(a,b,jump,i+1==count?terminalCrouch:crouch,limit))return qfalse;
 }
 indices[0]=seed;
 for(i=1;i<count;i++) {
  ngNode *n=&ngNodes[ngCount];indices[i]=ngCount++;
  memset(n,0,sizeof(*n));n->id=(*nextId)++;n->index=-1;n->accepted=qtrue;n->crouch=crouch;
  n->radius=n->requestedRadius=10.f;n->topologyProtected=qtrue;
  n->reason="generated_native_path_repair_bend";
  VectorCopy(ngPortalSearchNodes[chosen[i]].node.feet,n->feet);
  VectorCopy(n->feet,n->raw);n->raw[2]+=ngOrigin;
 }
 indices[count]=goal;
 for(k=0;k<(jump?1:2);k++) {
  chain=&ngPathRepairChains[ngPathRepairChainCount++];chain->count=count+1;chain->jump=jump;
  for(i=0;i<=count;i++)chain->indices[i]=indices[k||jump?count-i:i];
  for(i=0;i<count;i++) {
   int sourceLeg=k||jump?count-1-i:i;
   ngEdge *e=&ngEdges[ngEdgeCount++];memset(e,0,sizeof(*e));
   chain->crouch[i]=sourceLeg==count-1?terminalCrouch:crouch;
   j=chain->indices[i];e->from=j;e->to=chain->indices[i+1];e->accepted=e->added=qtrue;e->crouch=chain->crouch[i];
   e->reason=jump?"native_path_repair_jump_approach":"native_path_repair_walk";
  }
 }
 return qtrue;
}
static qboolean NG_PathRepairSearch(int seed,qboolean jump,qboolean crouch,int limit,int *nextId) {
 int used=1,i,j,dx,dy;
 memset(ngPortalSearchNodes,0,sizeof(ngPortalSearchNodes));
 ngPortalSearchNodes[0].node=ngNodes[seed];ngPortalSearchNodes[0].parent=-1;
 ngPortalSearchNodes[0].score=NG_PathRepairDistance(ngNodes[seed].feet,crouch);
 while(used<NG_PORTAL_SEARCH&&ngTraces<limit-6000) {
  int current=-1;float score=1e20f;ngPortalSearch *cur;
  for(i=0;i<used;i++)if(!ngPortalSearchNodes[i].closed&&ngPortalSearchNodes[i].score<score) {score=ngPortalSearchNodes[i].score;current=i;}
  if(current<0)break;cur=&ngPortalSearchNodes[current];cur->closed=qtrue;
  if(current)for(i=0;i<ngPathRepairInputNodes&&ngTraces<limit-6000;i++)
   if(ngPathRepairMain[i]&&NG_PathRepairGoal(i,crouch)) {
    vec3_t d;VectorSubtract(cur->node.feet,ngNodes[i].feet,d);
    if(DotProduct(d,d)<=48.f*48.f&&NG_PathRepairWalk(&cur->node,&ngNodes[i],jump,crouch&&NG_PathRepairOrdinary(i,qtrue),limit)&&
       NG_PathRepairCommit(seed,i,current,jump,crouch,limit,nextId))return qtrue;
   }
  for(dx=-1;dx<=1;dx++)for(dy=-1;dy<=1;dy++)if(dx||dy) {
   ngNode candidate;vec3_t trial;int previous=-1;float cost;
   if(used>=NG_PORTAL_SEARCH||ngTraces>=limit-6000)break;
   memset(&candidate,0,sizeof(candidate));VectorCopy(cur->node.feet,trial);trial[0]+=dx*8;trial[1]+=dy*8;
   if(fabs(trial[0]-ngNodes[seed].feet[0])>384.f||fabs(trial[1]-ngNodes[seed].feet[1])>384.f)continue;
   for(j=0;j<used;j++)if(ngPortalSearchNodes[j].closed&&fabs(ngPortalSearchNodes[j].node.feet[0]-trial[0])<.5f&&
      fabs(ngPortalSearchNodes[j].node.feet[1]-trial[1])<.5f&&fabs(ngPortalSearchNodes[j].node.feet[2]-trial[2])<1.f)break;
   if(j<used||NG_Ground(trial,candidate.feet,crouch?ngDuck:ngStand)||fabs(candidate.feet[2]-ngNodes[seed].feet[2])>64.f)continue;
   candidate.crouch=crouch;if(!NG_PathRepairReimport(&candidate,crouch))continue;
   for(j=0;j<used;j++)if(fabs(ngPortalSearchNodes[j].node.feet[0]-candidate.feet[0])<.5f&&
      fabs(ngPortalSearchNodes[j].node.feet[1]-candidate.feet[1])<.5f&&fabs(ngPortalSearchNodes[j].node.feet[2]-candidate.feet[2])<1.f){previous=j;break;}
   cost=cur->cost+(dx&&dy?11.313709f:8.f);
   if(previous>=0&&(ngPortalSearchNodes[previous].closed||ngPortalSearchNodes[previous].cost<=cost))continue;
   if(!NG_PathRepairWalk(&cur->node,&candidate,jump,crouch,limit))continue;
   if(previous<0)previous=used++;
   ngPortalSearchNodes[previous].node=candidate;ngPortalSearchNodes[previous].parent=current;
   ngPortalSearchNodes[previous].cost=cost;
   ngPortalSearchNodes[previous].score=cost+1.25f*NG_PathRepairDistance(candidate.feet,crouch);
  }
 }
 return qfalse;
}
static void NG_PathRepair(int minimumId) {
 int i,attempt,nextId=minimumId;unsigned long long before;
 ngPathRepairRun=qtrue;ngPathRepairMinimum=minimumId;
 ngPathRepairInputNodes=ngCount;ngPathRepairInputEdges=ngEdgeCount;ngPathRepairBegin=ngTraces;
 ngPathRepairLimit=ngTraces+NG_PATH_REPAIR_TRACES;
 if(ngPathRepairLimit>NG_TRACE_BUDGET-4000)ngPathRepairLimit=NG_TRACE_BUDGET-4000;
 ngPathRepairStop="input_validation_incomplete";
 if(!ngExternalInputComplete)return;
 for(i=0;i<ngCount;i++) {
  if(ngNodes[i].id>=minimumId){ngPathRepairStop="minimum_source_id_not_above_input";return;}
 }
 before=NG_PathRepairInputHash();memset(ngPathRepairTried,0,sizeof(ngPathRepairTried));
 ngPathRepairStop="bounded_frontier_exhausted";
 for(attempt=0;attempt<NG_PATH_REPAIR_SEARCHES&&ngTraces<ngPathRepairLimit-8000;attempt++) {
  int seed=-1,localLimit;float distance=384.f;qboolean jump=qfalse,crouch=qfalse;
  NG_SpawnReachability();
  if(NG_ExternalBuildComponents(ngPathRepairMain,qtrue)<0){ngPathRepairStop="no_spawn_component";break;}
  for(i=0;i<ngPathRepairInputNodes&&ngTraces<ngPathRepairLimit-6000;i++)if(!ngPathRepairMain[i]&&!ngPathRepairTried[i]&&ngNodes[i].accepted) {
   qboolean duck=(NG_OutputFlags(&ngNodes[i])&64)!=0;
   qboolean action=!NG_PathRepairOrdinary(i,duck);float candidate;
   if(action&&!NG_PathRepairJump(i))continue;
   if(action)duck=qfalse;
   candidate=NG_PathRepairDistance(ngNodes[i].feet,duck);
   if(candidate<distance){seed=i;distance=candidate;jump=action;crouch=duck;}
  }
  if(seed<0)break;ngPathRepairTried[seed]=1;
  localLimit=ngTraces+24000;if(localLimit>ngPathRepairLimit)localLimit=ngPathRepairLimit;
  NG_PathRepairSearch(seed,jump,crouch,localLimit,&nextId);
 }
 ngPathRepairTraces=ngTraces-ngPathRepairBegin;
 ngPathRepairPreserved=before==NG_PathRepairInputHash();
 ngPathRepairBudgetComplete=ngTraces<NG_TRACE_BUDGET&&!ngTrCacheFailed;
 if(!ngPathRepairPreserved)ngPathRepairStop="input_preservation_failed";
 else if(!ngPathRepairBudgetComplete)ngPathRepairStop="hard_trace_or_pair_budget";
 else if(attempt==NG_PATH_REPAIR_SEARCHES)ngPathRepairStop="bounded_search_limit";
 else if(ngTraces>=ngPathRepairLimit-8000)ngPathRepairStop="bounded_phase_limit";
 NG_SpawnReachability();
}
static void NG_PathRepairReport(void) {
 int i,j;
 if(!ngPathRepairRun)return;
 NG_Text(va("\"path_repair\":{\"model\":\"native_append_paths_v1\",\"explicit_repair\":true,\"minimum_new_source_id\":%d,\"input_validation_complete\":%s,\"input_nodes\":%d,\"input_edges\":%d,\"preserved_input\":%s,\"traces\":%d,\"trace_budget\":%d,\"budget_complete\":%s,\"stop_reason\":\"%s\",\"added_node_ids\":[",ngPathRepairMinimum,ngExternalInputComplete?"true":"false",ngPathRepairInputNodes,ngPathRepairInputEdges,ngPathRepairPreserved?"true":"false",ngPathRepairTraces,NG_PATH_REPAIR_TRACES,ngPathRepairBudgetComplete?"true":"false",ngPathRepairStop));
 for(i=ngPathRepairInputNodes;i<ngCount;i++)NG_Text(va("%s%d",i==ngPathRepairInputNodes?"":",",ngNodes[i].id));
 NG_Text("],\"chains\":[");
 for(i=0;i<ngPathRepairChainCount;i++) {
  ngPathRepairChain *c=&ngPathRepairChains[i];
  NG_Text(va("%s{\"kind\":\"%s\",\"accepted\":true,\"source_ids\":[",i?",":"",c->jump?"jump_launch_approach":"ordinary"));
  for(j=0;j<c->count;j++)NG_Text(va("%s%d",j?",":"",ngNodes[c->indices[j]].id));
  NG_Text("],\"crouch\":[");for(j=1;j<c->count;j++)NG_Text(va("%s%s",j==1?"":",",c->crouch[j-1]?"true":"false"));NG_Text("]}");
 }
 NG_Text("]},");
}
