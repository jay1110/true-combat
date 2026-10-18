/* Included after traversal helpers. Contraction preserves every directed
 * route through the removed vertex via one collision-proven representative.
 * It does not certify unsampled rooms or discard disconnected components. */
static int ngSimplifyRemoved,ngSimplifyOrphans,ngSimplifyLinks,ngSimplifyCandidates;
static const char *ngSimplifyStop;
static int ngSimpleTraceEnd;
static int ngSimpleOwner[NG_NODES];
static int ngSimpleTopologyRejected;
static int ngSparseRemoved;
static float ngSparseLength[NG_EDGES];
static int ngSparseOrder[NG_EDGES], ngSparseNext[NG_EDGES], ngSparseHead[NG_NODES];
#define NG_SPATIAL_CHORD_LIMIT 512
#define NG_SPATIAL_CHORD_CHECK_LIMIT 1000000
typedef struct {int edge,first,second;} ngSpatialChord;
static ngSpatialChord ngSpatialChords[NG_SPATIAL_CHORD_LIMIT];
static int ngSpatialChordCount,ngSpatialChordChecks;
static unsigned char ngSpatialActionNode[NG_NODES];
static void NG_SpatialSparseReset(void) {
 ngSpatialChordCount=ngSpatialChordChecks=0;
 memset(ngSpatialChords,0,sizeof(ngSpatialChords));
 memset(ngSpatialActionNode,0,sizeof(ngSpatialActionNode));
}
static int QDECL NG_SparseCompare(const void *pa,const void *pb) {
 int a=*(const int *)pa,b=*(const int *)pb;
 if(ngSparseLength[a]<ngSparseLength[b])return -1;
 if(ngSparseLength[a]>ngSparseLength[b])return 1;
 return a-b;
}
static qboolean NG_SparseOrdinary(int k) {
 ngEdge *e=&ngEdges[k];
 return e->accepted && e->from>=0 && e->to>=0 &&
  ngNodes[e->from].accepted && ngNodes[e->to].accepted &&
  !ngNodes[e->from].navFlags && !ngNodes[e->to].navFlags;
}
static qboolean NG_SparseDuck(int k) {
 return ngEdges[k].crouch || ngNodes[ngEdges[k].from].crouch || ngNodes[ngEdges[k].to].crouch;
}
/* Remove dense-room chords only with a persistent, already collision-checked
 * directed two-edge alternative. Shorter edges are processed first, so the
 * two strictly shorter witnesses cannot be removed later. Every deleted edge
 * has a final alternative at most 25% longer. Bridges and special traversals
 * survive; no new path is invented and no room vertex is deleted. */
static void NG_Sparsify(void) {
 int i,k,a,b,n=0;vec3_t delta;
 NG_SpatialSparseReset();
 ngSparseRemoved=0;
 for(i=0;i<ngCount;i++)ngSparseHead[i]=-1;
 for(k=0;k<ngEdgeCount;k++)if(NG_SparseOrdinary(k)) {
  VectorSubtract(ngNodes[ngEdges[k].from].feet,ngNodes[ngEdges[k].to].feet,delta);
  ngSparseLength[k]=VectorLength(delta);
  ngSparseNext[k]=ngSparseHead[ngEdges[k].from];ngSparseHead[ngEdges[k].from]=k;
  ngSparseOrder[n++]=k;
 }
 qsort(ngSparseOrder,n,sizeof(ngSparseOrder[0]),NG_SparseCompare);
 for(i=0;i<n;i++) {
  k=ngSparseOrder[i];
  for(a=ngSparseHead[ngEdges[k].from];a>=0;a=ngSparseNext[a]) {
   if(!ngEdges[a].accepted || ngSparseLength[a]>=ngSparseLength[k] ||
      (!NG_SparseDuck(k) && NG_SparseDuck(a)))continue;
   for(b=ngSparseHead[ngEdges[a].to];b>=0;b=ngSparseNext[b]) {
    if(!ngEdges[b].accepted || ngEdges[b].to!=ngEdges[k].to ||
       ngSparseLength[b]>=ngSparseLength[k] || (!NG_SparseDuck(k) && NG_SparseDuck(b)))continue;
    if(ngSparseLength[a]+ngSparseLength[b]<=ngSparseLength[k]*1.25f) {
     ngEdges[k].accepted=qfalse;
     ngEdges[k].reason="redundant_chord_shorter_directed_witness";
     ngSparseRemoved++;break;
    }
   }
   if(!ngEdges[k].accepted)break;
  }
 }
 G_Printf("navgen sparse: %d redundant directed chords removed; all vertices retained\n",ngSparseRemoved);
}
/* Automatic Format4 keeps every spatial floor sample. Only a redundant
 * ordinary directed chord can be removed, with two already native-accepted
 * strictly shorter walking legs. Source and action metadata are immutable;
 * this pass does not contract vertices or turn a traversal into a walk. */
static qboolean NG_SpatialSparseNode(int i) {
 const ngNode *n=&ngNodes[i];
 return n->accepted&&!ngSpatialActionNode[i]&&!n->authored&&!n->flags&&!n->packed&&!n->navFlags&&
  !n->editFlags&&!n->editRole&&!n->door.mode&&!n->count&&!n->ent[0]&&!n->ent[1]&&!n->ent[2]&&
  (!n->reason||(!strstr(n->reason,"jump")&&!strstr(n->reason,"drop")&&!strstr(n->reason,"free_climb")));
}
static qboolean NG_SpatialSparseEdge(int k) {
 const ngEdge *e=&ngEdges[k];
 return e->accepted&&e->from>=0&&e->to>=0&&e->from<ngCount&&e->to<ngCount&&e->from!=e->to&&
  NG_SpatialSparseNode(e->from)&&NG_SpatialSparseNode(e->to)&&e->reason&&
  (!strcmp(e->reason,"accepted")||!strcmp(e->reason,"external_native_validated_walk"));
}
static void NG_SpatialSparseActions(void) {
 int i,k;
 memset(ngSpatialActionNode,0,sizeof(ngSpatialActionNode));
 for(i=0;i<ngLadderProofCount;i++) {
  const ngLadderProof *p=&ngLadderProofs[i];
  /* The ordinary low/high floor aliases are not the directed ladder
   * targets. Keep their vertices and all ladder-labelled edges intact. */
  for(k=0;k<4;k++)if(p->node[k]>=0&&p->node[k]<ngCount)ngSpatialActionNode[p->node[k]]=1;
  if(p->ascentRelease>=0&&p->ascentRelease<ngCount)ngSpatialActionNode[p->ascentRelease]=1;
  if(p->ascentAlignment>=0&&p->ascentAlignment<ngCount)ngSpatialActionNode[p->ascentAlignment]=1;
 }
 for(i=0;i<ngJumpProofCount;i++) {
  const ngJumpProof *p=&ngJumpProofs[i];
  if(p->approach>=0&&p->approach<ngCount)ngSpatialActionNode[p->approach]=1;
  if(p->launch>=0&&p->launch<ngCount)ngSpatialActionNode[p->launch]=1;
  if(p->landing>=0&&p->landing<ngCount)ngSpatialActionNode[p->landing]=1;
 }
 for(i=0;i<ngDropProofCount;i++) {
  const ngDropProof *p=&ngDropProofs[i];
  if(p->approach>=0&&p->approach<ngCount)ngSpatialActionNode[p->approach]=1;
  if(p->launch>=0&&p->launch<ngCount)ngSpatialActionNode[p->launch]=1;
  if(p->landing>=0&&p->landing<ngCount)ngSpatialActionNode[p->landing]=1;
  if(p->pairedJumpLaunch>=0&&p->pairedJumpLaunch<ngCount)ngSpatialActionNode[p->pairedJumpLaunch]=1;
 }
 for(i=0;i<ngFreeClimbProofCount;i++) {
  const ngFreeClimbProof *p=&ngFreeClimbProofs[i];
  if(p->approach>=0&&p->approach<ngCount)ngSpatialActionNode[p->approach]=1;
  if(p->launch>=0&&p->launch<ngCount)ngSpatialActionNode[p->launch]=1;
  if(p->landing>=0&&p->landing<ngCount)ngSpatialActionNode[p->landing]=1;
 }
}
static void NG_SpatialSparseWalks(void) {
 int i,k,a,b,n=0;vec3_t delta;
 if(!ngExternalSpatialImport||ngExternalManualDraft)return;
 NG_SpatialSparseActions();
 for(i=0;i<ngCount;i++)ngSparseHead[i]=-1;
 for(k=0;k<ngEdgeCount;k++)if(NG_SpatialSparseEdge(k)) {
  VectorSubtract(ngNodes[ngEdges[k].from].feet,ngNodes[ngEdges[k].to].feet,delta);
  ngSparseLength[k]=VectorLength(delta);
  ngSparseNext[k]=ngSparseHead[ngEdges[k].from];ngSparseHead[ngEdges[k].from]=k;
  ngSparseOrder[n++]=k;
 }
 qsort(ngSparseOrder,n,sizeof(ngSparseOrder[0]),NG_SparseCompare);
 for(i=0;i<n&&ngSpatialChordCount<NG_SPATIAL_CHORD_LIMIT;i++) {
  k=ngSparseOrder[i];
  for(a=ngSparseHead[ngEdges[k].from];a>=0;a=ngSparseNext[a]) {
   if(!ngEdges[a].accepted||ngSparseLength[a]>=ngSparseLength[k]||
      (!NG_SparseDuck(k)&&NG_SparseDuck(a)))continue;
   for(b=ngSparseHead[ngEdges[a].to];b>=0;b=ngSparseNext[b]) {
    if(ngSpatialChordChecks>=NG_SPATIAL_CHORD_CHECK_LIMIT)goto spatial_sparse_done;
    ngSpatialChordChecks++;
    if(!ngEdges[b].accepted||ngEdges[b].to!=ngEdges[k].to||ngSparseLength[b]>=ngSparseLength[k]||
       (!NG_SparseDuck(k)&&NG_SparseDuck(b)))continue;
    if(ngSparseLength[a]+ngSparseLength[b]>ngSparseLength[k]*1.25f)continue;
    ngSpatialChords[ngSpatialChordCount].edge=k;
    ngSpatialChords[ngSpatialChordCount].first=a;
    ngSpatialChords[ngSpatialChordCount++].second=b;
    ngEdges[k].accepted=qfalse;ngEdges[k].reason="redundant_chord_shorter_directed_witness";
    NG_TrSetAccepted(ngEdges[k].from,ngEdges[k].to,qfalse);ngSparseRemoved++;break;
   }
   if(!ngEdges[k].accepted)break;
  }
 }
spatial_sparse_done:
 G_Printf("navgen spatial chords: removed=%d checks=%d/%d cap=%d directed_walk_detour_limit=1.25 vertices/actions unchanged\n",
  ngSpatialChordCount,ngSpatialChordChecks,NG_SPATIAL_CHORD_CHECK_LIMIT,NG_SPATIAL_CHORD_LIMIT);
}
static unsigned int ngSimpleWalkCache[NG_PAIR_CACHE_SLOTS];
static qboolean ngSimpleCacheFull;
static int NG_SimpleCached(int a,int b){return NG_PairGet(ngSimpleWalkCache,a,b);}
static qboolean NG_SimpleCache(int a,int b,int result) {
 if(NG_PairSet(ngSimpleWalkCache,a,b,result))return qtrue;
 ngSimpleCacheFull=qtrue;return qfalse;
}
static void NG_SimpleOrphans(void) {
 unsigned char linked[NG_NODES];int i;memset(linked,0,sizeof(linked));
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted&&ngEdges[i].from>=0&&ngEdges[i].to>=0){
  linked[ngEdges[i].from]=linked[ngEdges[i].to]=1;
 }
 for(i=0;i<ngCount;i++){
  ngNode *n=&ngNodes[i];
  if(!n->accepted||linked[i])continue;
  n->accepted=qfalse;n->index=-1;
  if(n->reason&&strstr(n->reason,"spawn"))n->reason="unconnected_spawn_not_exported";
  else if(n->reason&&strstr(n->reason,"objective"))n->reason="unconnected_objective_not_exported";
  else if(n->reason&&strstr(n->reason,"ladder"))n->reason="unconnected_ladder_endpoint_not_exported";
  else if(n->navFlags||n->name[0]||n->flags||n->packed||n->count)n->reason="unconnected_special_node_not_exported";
  else n->reason="unconnected_ordinary_node_not_exported";
  ngSimplifyOrphans++;
 }
}
static qboolean NG_SimpleRepresentative(int i) {
 ngNode *n=&ngNodes[i];
 if(!n->accepted||n->navFlags||n->name[0]||n->flags||n->packed||
    n->ent[0]||n->ent[1]||n->ent[2]||n->count)return qfalse;
 if(!n->reason||strstr(n->reason,"spawn")||strstr(n->reason,"objective")||
    strstr(n->reason,"ladder")||strstr(n->reason,"jump"))return qfalse;
 return qtrue;
}
static qboolean NG_SimpleNode(int i) {
 /* A protected portal may own nearby floor witnesses without being moved
  * or removed itself. Only the point being contracted needs to be mutable. */
 return !ngNodes[i].topologyProtected && NG_SimpleRepresentative(i);
}
static qboolean NG_SimpleWalk(int from,int to) {
 vec3_t d;int k;if(from==to)return qtrue;
 if(NG_SimpleCached(from,to))return NG_SimpleCached(from,to)==1;
 if(NG_TrSpecialInput(from,to))return qfalse;
 for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].from==from&&ngEdges[k].to==to){
  if(ngEdges[k].reason&&!strcmp(ngEdges[k].reason,"special_link_requires_review"))return qfalse;
  if(ngEdges[k].accepted&&(!ngEdges[k].crouch||ngNodes[from].crouch)&&!ngNodes[from].navFlags&&!ngNodes[to].navFlags){return NG_SimpleCache(from,to,1);}
 }
 VectorSubtract(ngNodes[from].feet,ngNodes[to].feet,d);
 /* The sampling edge limit must not prevent a longer collision-proven
  * shortcut across an open region after contraction. */
 if(DotProduct(d,d)>256.f*256.f)return qfalse;
 if(ngTraces>=ngSimpleTraceEnd)return qfalse;
 NG_SimpleCache(from,to,NG_Walk(&ngNodes[from],&ngNodes[to],ngNodes[from].crouch?ngDuck:ngStand)==NULL?1:2);
 return NG_SimpleCached(from,to)==1;
}
static void NG_SimpleLink(int from,int to) {
 int k;NG_TrLink(from,to,"simplified_directed_walk_validated");
 if(ngNodes[from].crouch&&ngNodes[to].crouch)
  for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted&&ngEdges[k].from==from&&ngEdges[k].to==to)ngEdges[k].crouch=qtrue;
}
/* Keep the geometry of the room/portal graph, not merely reachability. A
 * collision-clear contraction can otherwise turn a chain into a diagonal
 * fan through a whole room. Directed edges are collapsed to geometric pairs
 * only for this quality check; all native directed proofs remain mandatory. */
static qboolean NG_SimpleCross(int a,int b,int c,int d) {
 const float *p=ngNodes[a].feet,*q=ngNodes[b].feet;
 const float *r=ngNodes[c].feet,*s=ngNodes[d].feet;
 float ux=q[0]-p[0],uy=q[1]-p[1],vx=s[0]-r[0],vy=s[1]-r[1];
 float den=ux*vy-uy*vx,t,w;
 if(a==c||a==d||b==c||b==d||fabs(den)<.01f)return qfalse;
 t=((r[0]-p[0])*vy-(r[1]-p[1])*vx)/den;
 w=((r[0]-p[0])*uy-(r[1]-p[1])*ux)/den;
 if(t<=.01f||t>=.99f||w<=.01f||w>=.99f)return qfalse;
 return fabs((p[2]+t*(q[2]-p[2]))-(r[2]+w*(s[2]-r[2])))<=18.f;
}
static int NG_SimpleFans(int center,const int *neighbors,int count) {
 int i,j,total=0;vec3_t a,b;float aa,bb,dot;
 for(i=0;i<count;i++)for(j=i+1;j<count;j++){
  VectorSubtract(ngNodes[neighbors[i]].feet,ngNodes[center].feet,a);
  VectorSubtract(ngNodes[neighbors[j]].feet,ngNodes[center].feet,b);
  if(fabs(a[2])>18.f||fabs(b[2])>18.f)continue;
  aa=a[0]*a[0]+a[1]*a[1];bb=b[0]*b[0]+b[1]*b[1];dot=a[0]*b[0]+a[1]*b[1];
  if(aa>48.f*48.f&&bb>48.f*48.f&&dot>0&&dot*dot>.9567727f*aa*bb)total++;
 }
 return total;
}
static qboolean NG_SimpleTopology(int removed,int rep) {
 int oldA[64],oldB[64],merged[128],na=0,nb=0,nm=0,k,j,t,before=0,after=0;
 for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted){
  ngEdge *e=&ngEdges[k];int n=-1,*list=NULL,*count=NULL;
  if(e->from==removed||e->to==removed){n=e->from==removed?e->to:e->from;list=oldA;count=&na;}
  if(list){for(j=0;j<*count&&list[j]!=n;j++){}if(j==*count){if(*count>=64)return qfalse;list[(*count)++]=n;}}
  if(e->from==rep||e->to==rep){n=e->from==rep?e->to:e->from;for(j=0;j<nb&&oldB[j]!=n;j++){}if(j==nb){if(nb>=64)return qfalse;oldB[nb++]=n;}}
 }
 for(t=0;t<na+nb;t++){
  int n=t<na?oldA[t]:oldB[t-na];if(n==removed||n==rep)continue;
  for(j=0;j<nm&&merged[j]!=n;j++){}if(j==nm)merged[nm++]=n;
 }
 if(nm>(nb>4?nb:4))return qfalse;
 if(NG_SimpleFans(rep,merged,nm)>NG_SimpleFans(removed,oldA,na)+NG_SimpleFans(rep,oldB,nb))return qfalse;
 for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted){
  int c=ngEdges[k].from,d=ngEdges[k].to;
  if(c<0||d<0||c==d||c==removed||d==removed||c==rep||d==rep||ngNodes[c].navFlags||ngNodes[d].navFlags)continue;
  for(j=0;j<na;j++)if(NG_SimpleCross(removed,oldA[j],c,d))before++;
  for(j=0;j<nb;j++)if(oldB[j]!=removed&&NG_SimpleCross(rep,oldB[j],c,d))before++;
  for(j=0;j<nm;j++)if(NG_SimpleCross(rep,merged[j],c,d))after++;
 }
 return after<=before;
}
static void NG_Simplify(void) {
 int pass,i,k,j,degree,rep,start=ngTraces,in[32],out[32],ni,no,order[NG_NODES],at,before;
 float nearest[NG_NODES];
 for(i=0;i<ngCount;i++)ngSimpleOwner[i]=i;
 ngSimplifyRemoved=ngSimplifyOrphans=ngSimplifyLinks=ngSimplifyCandidates=0;
 ngSimpleTopologyRejected=0;
 ngSimpleTraceEnd=start+500000;if(ngSimpleTraceEnd>NG_TRACE_BUDGET-2000)ngSimpleTraceEnd=NG_TRACE_BUDGET-2000;
 memset(ngSimpleWalkCache,0,sizeof(ngSimpleWalkCache));ngSimpleCacheFull=qfalse;NG_SimpleOrphans();
 ngSimplifyStop="no_more_safe_contractions";
 for(pass=0;pass<ngCount;pass++){
  before=ngSimplifyRemoved;
  for(i=0;i<ngCount;i++){nearest[i]=1e20f;order[i]=i;}
  for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted){
   int a=ngEdges[k].from,b=ngEdges[k].to;vec3_t delta;float ds;
   if(a<0||b<0||!NG_SimpleRepresentative(a)||!NG_SimpleRepresentative(b))continue;
   VectorSubtract(ngNodes[a].feet,ngNodes[b].feet,delta);ds=DotProduct(delta,delta);
   if(ds<nearest[a])nearest[a]=ds;if(ds<nearest[b])nearest[b]=ds;
  }
  /* Process actual overlaps first, independent of map node numbering. */
  for(i=1;i<ngCount;i++){int value=order[i];j=i;while(j>0&&nearest[order[j-1]]>nearest[value]){order[j]=order[j-1];j--;}order[j]=value;}
  for(at=0;at<ngCount;at++){i=order[at];if(NG_SimpleNode(i)){
  int reps[64],nr=0,r,t;float distances[64];qboolean protectedEdge=qfalse,ok=qfalse;
  if(ngTraces>=ngSimpleTraceEnd){ngSimplifyStop="trace_budget";goto done;}
  ni=no=degree=0;rep=-1;
  for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted&&(ngEdges[k].from==i||ngEdges[k].to==i)){
   int neighbor=ngEdges[k].from==i?ngEdges[k].to:ngEdges[k].from;
   if(neighbor<0||neighbor>=ngCount||!ngNodes[neighbor].accepted||
      ngNodes[neighbor].navFlags){protectedEdge=qtrue;break;}
   if(ngEdges[k].from==i){if(no==32){protectedEdge=qtrue;break;}out[no++]=neighbor;}
   else {if(ni==32){protectedEdge=qtrue;break;}in[ni++]=neighbor;}
   degree++;
   if(NG_SimpleRepresentative(neighbor)&&ngNodes[neighbor].crouch==ngNodes[i].crouch){
    vec3_t delta;float ds;VectorSubtract(ngNodes[i].feet,ngNodes[neighbor].feet,delta);ds=DotProduct(delta,delta);
    if(ds<224.f*224.f){
     for(r=0;r<nr&&reps[r]!=neighbor;r++);
     if(r==nr&&nr<64){
      r=nr++;while(r>0&&distances[r-1]>ds){reps[r]=reps[r-1];distances[r]=distances[r-1];r--;}
      reps[r]=neighbor;distances[r]=ds;
     }
    }
   }
  }
  if(protectedEdge)continue;
  if(!degree){ngNodes[i].accepted=qfalse;ngNodes[i].index=-1;ngNodes[i].reason="unconnected_ordinary_node_not_exported";ngSimplifyOrphans++;continue;}
  /* Long dead ends remain room/extent witnesses. A redundant ordinary leaf
   * <=32 units from its sole bidirectional neighbour may be absorbed, but
   * only on already accepted native walks in both directions and with the
   * identical stance. The owner/proof checks below still apply unchanged. */
  if(ni==1&&no==1&&in[0]==out[0]) {
   vec3_t delta;float distance;int proofs=0; qboolean otherLink=qfalse;
   ngNode *target;rep=in[0];target=&ngNodes[rep];
   VectorSubtract(ngNodes[i].feet,target->feet,delta);distance=DotProduct(delta,delta);
   if(distance>32.f*32.f||target->crouch!=ngNodes[i].crouch||target->navFlags||
      target->flags||target->packed||target->ent[0]||target->ent[1]||target->ent[2]||target->count)continue;
   /* Existing spawn/objective/camp anchors may remain as representatives:
    * this contraction leaves their position, ID, name, facing, radius and metadata intact.
    * Unknown action-bearing targets remain excluded from this exception. */
   if(!target->reason)continue;
   if(!NG_SimpleRepresentative(rep)&&strcmp(target->reason,"map_entity_spawn_seed")&&
      strcmp(target->reason,"map_objective_seed")&&Q_stricmpn(target->name,"tce2_camp_",10))continue;
   for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted) {
    ngEdge *edge=&ngEdges[k];
    if(!edge->crouch||ngNodes[i].crouch) {
     if(edge->from==i&&edge->to==rep)proofs|=1;
     if(edge->from==rep&&edge->to==i)proofs|=2;
    }
    if((edge->from==rep&&edge->to!=i)||(edge->to==rep&&edge->from!=i))otherLink=qtrue;
   }
   /* Do not collapse an entire two-node component into an orphan. */
   if(proofs!=3||!otherLink)continue;
   reps[0]=rep;distances[0]=distance;nr=1;
  }
  if(!nr||!ni||!no||ngEdgeCount+ni+no>=NG_EDGES)continue;
  /* Try alternative representatives instead of retaining a dense cluster
   * when the nearest one lies on the wrong side of a corner. Keep every
   * original floor sample within 224 units of its final representative,
   * with a hull-proven walk both ways: repeated contractions cannot slowly
   * erase a room or move its coverage witness around an obstacle. */
  for(r=0;r<nr;r++) {
   rep=reps[r];ok=qtrue;ngSimplifyCandidates++;
   if(!NG_SimpleTopology(i,rep)){ngSimpleTopologyRejected++;ok=qfalse;continue;}
   for(t=0;t<ngCount&&ok;t++)if(ngSimpleOwner[t]==i||ngSimpleOwner[t]==rep){
    vec3_t delta;VectorSubtract(ngNodes[t].feet,ngNodes[rep].feet,delta);
    if(DotProduct(delta,delta)>224.f*224.f)ok=qfalse;
   }
   for(j=0;j<ni&&ok;j++)ok=NG_SimpleWalk(in[j],rep);
   for(j=0;j<no&&ok;j++)ok=NG_SimpleWalk(rep,out[j]);
   for(t=0;t<ngCount&&ok;t++)if(ngSimpleOwner[t]==i)
    ok=NG_SimpleWalk(t,rep)&&NG_SimpleWalk(rep,t);
   if(ngSimpleCacheFull){ngSimplifyStop="walk_proof_cache_capacity";goto done;}
   if(ok)break;
  }
  if(!ok)continue;
  for(t=0;t<ngCount;t++)if(ngSimpleOwner[t]==i)ngSimpleOwner[t]=rep;
  for(j=0;j<ni;j++)if(in[j]!=rep){if(!NG_TrAccepted(in[j],rep))ngSimplifyLinks++;NG_SimpleLink(in[j],rep);}
  for(j=0;j<no;j++)if(out[j]!=rep){if(!NG_TrAccepted(rep,out[j]))ngSimplifyLinks++;NG_SimpleLink(rep,out[j]);}
  for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted&&(ngEdges[k].from==i||ngEdges[k].to==i)){
   ngEdges[k].accepted=qfalse;ngEdges[k].reason="replaced_by_validated_walk_contraction";
  }
  ngNodes[i].accepted=qfalse;ngNodes[i].index=-1;ngNodes[i].reason="contracted_to_validated_walk_neighbor";ngSimplifyRemoved++;
 }}
  if(ngSimplifyRemoved==before)break;
 }
done:
 NG_SimpleOrphans();
 G_Printf("navgen simplify: %d contracted, %d unconnected suppressed, %d candidates, %s; coverage remains unverified\n",ngSimplifyRemoved,ngSimplifyOrphans,ngSimplifyCandidates,ngSimplifyStop);
}
