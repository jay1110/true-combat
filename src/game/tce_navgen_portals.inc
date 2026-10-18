/* Fine doorway discovery between sampled, spawn-reachable floors and nearby
 * disconnected walkable floor components. This is bounded static walking search, not portal
 * inference from visibility and not permission to cross solid geometry. */
#define NG_PORTAL_SEARCH 2048
static int ngPortalCandidates,ngPortalConnected,ngPortalAdded,ngPortalTraces;
static const char *ngPortalStop;
typedef struct { ngNode node; int parent; float cost,score; qboolean closed; } ngPortalSearch;
static ngPortalSearch ngPortalSearchNodes[NG_PORTAL_SEARCH];

static float NG_PortalDistance(const vec3_t point,int *nearest) {
 int i;float best=384.f*384.f;*nearest=-1;
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted&&ngSpawnReachable[i]&&!ngNodes[i].navFlags&&!ngNodes[i].flags){
  vec3_t delta;float dist;VectorSubtract(ngNodes[i].feet,point,delta);
  if(fabs(delta[2])>(ngExternalSpatialImport?128.f:64.f))continue;
  dist=DotProduct(delta,delta);if(dist<best){best=dist;*nearest=i;}
 }
 return sqrt(best);
}
static qboolean NG_PortalSearchPath(int seed,int limit) {
 int used=1,found=-1,goal=-1,i,j,dx,dy,stride;
 memset(ngPortalSearchNodes,0,sizeof(ngPortalSearchNodes));ngPortalSearchNodes[0].node=ngNodes[seed];ngPortalSearchNodes[0].parent=-1;
 ngPortalSearchNodes[0].score=NG_PortalDistance(ngNodes[seed].feet,&i);
 while(used<NG_PORTAL_SEARCH&&ngTraces<limit-2500){
  int current=-1;float score=1e20f;ngPortalSearch *cur;
  for(i=0;i<used;i++)if(!ngPortalSearchNodes[i].closed&&ngPortalSearchNodes[i].score<score){score=ngPortalSearchNodes[i].score;current=i;}
  if(current<0)break;cur=&ngPortalSearchNodes[current];cur->closed=qtrue;
  for(i=0;i<ngCount&&ngTraces<limit-2500;i++)if(ngNodes[i].accepted&&ngSpawnReachable[i]&&!ngNodes[i].navFlags&&!ngNodes[i].flags){
   vec3_t delta;ngNode target=ngNodes[i];VectorSubtract(cur->node.feet,target.feet,delta);
   /* Preserve a rejected/special original pair; a genuinely different walking
    * path needs at least one independently validated intermediate point. */
   if(!current&&(NG_HasEdge(seed,i)||NG_HasEdge(i,seed)))continue;
   if(DotProduct(delta,delta)<=48.f*48.f&&NG_BothWays(&cur->node,&target)){found=current;goal=i;break;}
  }
  if(found>=0)break;
  /* Fine doorway offsets are searched only here, not exported as a grid.
   * Repeated closed cells are skipped before spending more trace budget. */
  /* Spatial imports use coarse, fully hull-tested advances plus the same
   * fine doorway lattice. A 32-unit move still tests every 8-unit floor
   * segment; it cannot bridge a hole or bypass a wall. */
  for(stride=ngExternalSpatialImport?32:8;stride>=8;stride/=4)for(dx=-1;dx<=1;dx++)for(dy=-1;dy<=1;dy++)if(dx||dy){
   ngNode candidate;vec3_t trial;const char *why;int previous=-1,nearest;float cost;
   if(used>=NG_PORTAL_SEARCH||ngTraces>=limit-2500)break;
   memset(&candidate,0,sizeof(candidate));VectorCopy(cur->node.feet,trial);trial[0]+=dx*stride;trial[1]+=dy*stride;
   if(fabs(trial[0]-ngNodes[seed].feet[0])>384.f||fabs(trial[1]-ngNodes[seed].feet[1])>384.f||fabs(trial[2]-ngNodes[seed].feet[2])>(ngExternalSpatialImport?128.f:64.f))continue;
   /* Do not repeat ground/hull traces for an already expanded lattice point
    * on this floor. Other heights remain eligible for stairs/stacked floors. */
   for(j=0;j<used;j++)if(ngPortalSearchNodes[j].closed&&fabs(ngPortalSearchNodes[j].node.feet[0]-trial[0])<.5f&&fabs(ngPortalSearchNodes[j].node.feet[1]-trial[1])<.5f&&fabs(ngPortalSearchNodes[j].node.feet[2]-trial[2])<1.f)break;
   if(j<used)continue;
   why=NG_Ground(trial,candidate.feet,ngStand);if(why){why=NG_Ground(trial,candidate.feet,ngDuck);candidate.crouch=qtrue;}if(why)continue;
   if(ngExternalSpatialImport&&fabs(candidate.feet[2]-ngNodes[seed].feet[2])>128.f)continue;
   for(j=0;j<used;j++)if(fabs(ngPortalSearchNodes[j].node.feet[0]-candidate.feet[0])<.5f&&fabs(ngPortalSearchNodes[j].node.feet[1]-candidate.feet[1])<.5f&&fabs(ngPortalSearchNodes[j].node.feet[2]-candidate.feet[2])<1.f){previous=j;break;}
   cost=cur->cost+stride*(dx&&dy?1.414214f:1.f);
   if(previous>=0&&(ngPortalSearchNodes[previous].closed||ngPortalSearchNodes[previous].cost<=cost))continue;
   if(!NG_BothWays(&cur->node,&candidate))continue;
   if(previous<0)previous=used++;
   ngPortalSearchNodes[previous].node=candidate;ngPortalSearchNodes[previous].parent=current;ngPortalSearchNodes[previous].cost=cost;
   ngPortalSearchNodes[previous].score=cost+1.25f*NG_PortalDistance(candidate.feet,&nearest);
  }
 }
 if(found>=0){int route[NG_PORTAL_SEARCH],length=0,last=seed,maxid=0,added=0;
  for(i=found;i>0;i=ngPortalSearchNodes[i].parent)route[length++]=i;
  if(ngCount+length>=NG_NODES-128||ngEdgeCount+2*(length+1)>=NG_EDGES-512)return qfalse;
  for(i=0;i<ngCount;i++)if(ngNodes[i].id>maxid)maxid=ngNodes[i].id;
  for(i=length-1;i>=0;){int chosen=i,index;ngNode start=ngNodes[last],target=ngNodes[goal];
   if(ngTraces<limit-1000&&!NG_HasEdge(last,goal)&&!NG_HasEdge(goal,last)&&NG_BothWays(&start,&target)){ngNodes[last].crouch|=start.crouch;ngNodes[goal].crouch|=target.crouch;break;}
   /* Export turns rather than the 8-unit search lattice. Every shortcut is
    * rechecked. When the budget ends retain the proven adjacent segment. */
   for(j=0;j<i&&ngTraces<limit-1000;j++){target=ngPortalSearchNodes[route[j]].node;if(NG_BothWays(&start,&target)){chosen=j;break;}}
   index=ngCount++;ngNodes[index]=ngPortalSearchNodes[route[chosen]].node;ngNodes[index].id=++maxid;
   ngNodes[index].accepted=qtrue;ngNodes[index].reason="generated_doorway_detour";ngNodes[index].index=-1;
   VectorCopy(ngNodes[index].feet,ngNodes[index].raw);ngNodes[index].raw[2]+=ngOrigin;
   NG_VerifiedWalkEdge(last,index,start.crouch||ngNodes[index].crouch);NG_VerifiedWalkEdge(index,last,start.crouch||ngNodes[index].crouch);
   last=index;i=chosen-1;added++;
  }
  NG_VerifiedWalkEdge(last,goal,ngNodes[last].crouch||ngNodes[goal].crouch||ngPortalSearchNodes[found].node.crouch);NG_VerifiedWalkEdge(goal,last,ngNodes[last].crouch||ngNodes[goal].crouch||ngPortalSearchNodes[found].node.crouch);
  ngPortalAdded+=added;NG_SpawnReachability();
  return ngSpawnReachable[seed]!=0;
 }
 return qfalse;
}
/* Weak walking components are scheduling groups only, never reachability
 * evidence. Only NG_SpawnReachability after accepted edges grants access. */
static int NG_PortalRoot(int *parents,int node) {
 while(parents[node]!=node){parents[node]=parents[parents[node]];node=parents[node];}
 return node;
}
static void NG_Portals(void) {
 unsigned char attempted[NG_NODES]={0};
 int parents[NG_NODES],attempts[NG_NODES]={0},first[NG_NODES];
 int begin=ngTraces,limit=begin+160000,attempt,i,groups=0;
 ngPortalCandidates=ngPortalConnected=ngPortalAdded=ngPortalTraces=0;ngPortalStop="candidate_frontier_exhausted";
 if(limit>NG_TRACE_BUDGET-150000)limit=NG_TRACE_BUDGET-150000;
 if(!ngSpawnSeeds){ngPortalStop="no_spawn_seeds";return;}
 NG_SpawnReachability();
 for(i=0;i<ngCount;i++){parents[i]=i;first[i]=-1;}
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted){
  int a=ngEdges[i].from,b=ngEdges[i].to;
  if(a<0||b<0||a>=ngCount||b>=ngCount)continue;
  if(!ngNodes[a].accepted||!ngNodes[b].accepted||ngNodes[a].navFlags||ngNodes[b].navFlags||ngNodes[a].flags||ngNodes[b].flags)continue;
  a=NG_PortalRoot(parents,a);b=NG_PortalRoot(parents,b);if(a!=b)parents[b]=a;
 }
 for(attempt=0;attempt<16;attempt++){
  int seed=-1,nearest,chosenGroup=-1,least=3;float best=384.f;
  if(ngTraces>=limit-3000){ngPortalStop="trace_budget";break;}
  if(ngCount>=NG_NODES-128||ngEdgeCount>=NG_EDGES-512){ngPortalStop="graph_budget";break;}
  for(i=0;i<ngCount;i++)if(ngNodes[i].accepted&&!ngSpawnReachable[i]&&!attempted[i]&&!ngNodes[i].navFlags&&!ngNodes[i].flags){
   int group=NG_PortalRoot(parents,i);float distance;vec3_t delta;
   /* No origin/reason filter: generated frontiers can be better entrances
    * than the original coarse floor seeds. Give other islands a first try
    * before a second approach to the same one. Never retry adjacent starts. */
   if(attempts[group]>=2||attempts[group]>least)continue;
   if(first[group]>=0){VectorSubtract(ngNodes[i].feet,ngNodes[first[group]].feet,delta);if(DotProduct(delta,delta)<96.f*96.f)continue;}
   distance=NG_PortalDistance(ngNodes[i].feet,&nearest);
   if(nearest>=0&&distance<384.f&&(attempts[group]<least||distance<best)){
    best=distance;seed=i;chosenGroup=group;least=attempts[group];
   }
  }
  if(seed<0)break;
  attempted[seed]=1;if(!attempts[chosenGroup]){first[chosenGroup]=seed;groups++;}attempts[chosenGroup]++;ngPortalCandidates++;
  {int localLimit=ngTraces+12000,oldCount=ngCount;if(localLimit>limit)localLimit=limit;
   if(NG_PortalSearchPath(seed,localLimit))ngPortalConnected++;
   /* New accepted detour nodes are reachable, but keep the scheduler arrays
    * valid independently of that invariant. */
   for(i=oldCount;i<ngCount;i++){parents[i]=i;first[i]=-1;}
  }
  if(attempt==15)ngPortalStop="candidate_budget";
 }
 ngPortalTraces=ngTraces-begin;
 G_Printf("navgen doorways: candidates=%d components=%d connected=%d turnpoints=%d traces=%d stop=%s (bounded static walking; not full room coverage)\n",ngPortalCandidates,groups,ngPortalConnected,ngPortalAdded,ngPortalTraces,ngPortalStop);
}
