/* Native acceptance gate for an external BSP/Recast candidate. The external
 * graph is a proposal: every exported floor, hull and directed walking edge
 * still has to pass collision checks in this loaded server map. No global
 * floor raster or unvalidated proximity/reverse links are added. Bounded
 * component repair may add native-proven bridges and necessary portal turns.
 * Contraction keeps native-proven routes and a <=224-unit floor witness. */
#define NG_EXTERNAL_INPUT (5 * 1024 * 1024)
#define NG_EXTERNAL_NODE_RESERVE 512
#define NG_EXTERNAL_NODE_LIMIT (NG_NODES - NG_EXTERNAL_NODE_RESERVE)
#define NG_EXTERNAL_LEGACY_NODE_LIMIT 4096
#define NG_EXTERNAL_LEGACY_EDGE_LIMIT 65536
#define NG_EXTERNAL_EDGE_LIMIT (NG_EDGES - NG_EXTERNAL_NODE_RESERVE * 16)
static char ngExternalText[NG_EXTERNAL_INPUT + 1];
static unsigned char ngExternalSpawn[NG_NODES];
static char *ngExternalCursor;
static int ngExternalLine;
static const char *ngExternalError;
static int ngExternalSuppliedCount,ngExternalSuppliedEdges;
static int ngExternalVersion;
static int ngExternalInputTraces;
static qboolean ngExternalInputComplete;
static unsigned char ngExternalFloorAccepted[NG_NODES];
static const char *ngExternalFloorReason[NG_NODES];
static const char *ngExternalEdgeReason[NG_EDGES];
static unsigned char ngExternalEdgeAccepted[NG_EDGES];
static int ngExternalDirectAttempts[NG_NODES],ngExternalDetourAttempts[NG_NODES];
static int ngExternalMissingRegion[NG_NODES],ngExternalMissingRegionCount;
#define NG_EXTERNAL_DETOUR_REPORT 96
typedef struct {
 int seed,target,componentNodes,previousAttempts,spawnRank,traces;
 float distance,score;
 qboolean afterActions,connected;
} ngExternalDetourCase;
static ngExternalDetourCase ngExternalDetourCases[NG_EXTERNAL_DETOUR_REPORT];
static int ngExternalDetourCaseCount;
static void NG_ExternalShortcutsReset(void);
static void NG_ExternalJunctionsReset(void);
static void NG_ExternalJunctionReport(void);
static void NG_PathRepairReset(void);

static qboolean NG_ExternalToken(char *out,int size) {
 int n=0;char *p=ngExternalCursor;
 while(*p&&*p<=' '){if(*p=='\n')ngExternalLine++;p++;}
 if(!*p){ngExternalError="unexpected_end_of_input";return qfalse;}
 while(*p&&*p>' ') {
  if(n>=size-1){ngExternalError="oversized_token";return qfalse;}
  out[n++]=*p++;
 }
 out[n]=0;ngExternalCursor=p;return qtrue;
}
static qboolean NG_ExternalFlagWord(unsigned int *value) {
 char token[32];unsigned long long result=0;int i;
 if(!NG_ExternalToken(token,sizeof(token)))return qfalse;
 for(i=0;token[i];i++) {
  int d=token[i]-'0';
  if(d<0||d>9||result>(0xffffffffULL-(unsigned int)d)/10){ngExternalError="invalid_flag_word";return qfalse;}
  result=result*10+(unsigned int)d;
 }
 *value=(unsigned int)result;return qtrue;
}
static qboolean NG_ExternalInt(int *value,int maximum) {
 char token[32];int i,result=0;
 if(!NG_ExternalToken(token,sizeof(token)))return qfalse;
 for(i=0;token[i];i++) {
  int digit=token[i]-'0';
  if(digit<0||digit>9||result>(maximum-digit)/10||digit>maximum) {
   ngExternalError="invalid_or_out_of_range_integer";return qfalse;
  }
  result=result*10+digit;
 }
 *value=result;return qtrue;
}
static qboolean NG_ExternalFloat(float *value) {
 char token[64],*end;double number;
 if(!NG_ExternalToken(token,sizeof(token)))return qfalse;
 number=strtod(token,&end);
 if(end==token||*end||number!=number||number<=-1000000.0||number>=1000000.0) {
  ngExternalError="invalid_or_nonfinite_coordinate";return qfalse;
 }
 *value=(float)number;return qtrue;
}
static unsigned char ngExternalMetadataSeen[NG_NODES];
static int NG_ExternalHexDigit(unsigned char c) {
 if(c>='0'&&c<='9')return c-'0';
 if(c>='a'&&c<='f')return c-'a'+10;
 if(c>='A'&&c<='F')return c-'A'+10;
 return -1;
}
static qboolean NG_ExternalTextToken(char *name,int maximum) {
 char token[2048];unsigned char *bytes=(unsigned char *)name;int length,i,n=0;
 if(!NG_ExternalToken(token,sizeof(token)))return qfalse;
 if(!strcmp(token,"-")){name[0]=0;return qtrue;}
 length=(int)strlen(token);
 if(!length||(length&1)||length>maximum*2){ngExternalError="invalid_metadata_name_hex_or_length";return qfalse;}
 for(i=0;i<length;i+=2) {
  int a=NG_ExternalHexDigit((unsigned char)token[i]),b=NG_ExternalHexDigit((unsigned char)token[i+1]);
  if(a<0||b<0){ngExternalError="invalid_metadata_name_hex_or_length";return qfalse;}
  bytes[n++]=(unsigned char)((a<<4)|b);
 }
 for(i=0;i<n;) {
  unsigned int c=bytes[i++],code,minimum;int rest,j;
  if(c<128){if(c<32){ngExternalError="metadata_name_control_byte";return qfalse;}continue;}
  if(c>=0xc2&&c<=0xdf){code=c&31u;rest=1;minimum=0x80u;}
  else if(c>=0xe0&&c<=0xef){code=c&15u;rest=2;minimum=0x800u;}
  else if(c>=0xf0&&c<=0xf4){code=c&7u;rest=3;minimum=0x10000u;}
  else {ngExternalError="invalid_metadata_name_utf8";return qfalse;}
  if(i+rest>n){ngExternalError="invalid_metadata_name_utf8";return qfalse;}
  for(j=0;j<rest;j++) {
   unsigned int next=bytes[i++];
   if((next&0xc0u)!=0x80u){ngExternalError="invalid_metadata_name_utf8";return qfalse;}
   code=(code<<6)|(next&63u);
  }
  if(code<minimum||code>0x10ffffu||(code>=0xd800u&&code<=0xdfffu)) {
   ngExternalError="invalid_metadata_name_utf8";return qfalse;
  }
 }
 name[n]=0;return qtrue;
}
static qboolean NG_ExternalName(char *name) {return NG_ExternalTextToken(name,255);}
static qboolean NG_ExternalPropertiesRead(qboolean magicRead) {
 char token[64],key[128],value[1024];int version,count,i,id,index,k,size;
 if(!magicRead) {
  if(!NG_ExternalToken(token,sizeof(token)))return qfalse;
  if(strcmp(token,"TCE_NODE_PROPERTIES")){ngExternalError="trailing_metadata_data";return qfalse;}
 }
 if(!NG_ExternalInt(&version,1)||version!=1){ngExternalError="unsupported_property_version";return qfalse;}
 if(!NG_ExternalInt(&count,NG_PROPERTY_PAIRS))return qfalse;
 for(i=0;i<count;i++) {
  ngNode *node;ngCustomProperty *property;
  if(!NG_ExternalInt(&id,0x7fffffff))return qfalse;
  index=NG_Find(id);
  if(index<0){ngExternalError="unknown_property_source_id";return qfalse;}
  node=&ngNodes[index];
  if(node->propertyCount>=NG_PROPERTY_PER_NODE){ngExternalError="custom_properties_exceed_32_per_node";return qfalse;}
  if(!NG_ExternalTextToken(key,127)||!NG_ExternalTextToken(value,1023))return qfalse;
  if(!key[0]){ngExternalError="empty_custom_property_key";return qfalse;}
  if(!strncmp(key,"tce_",4)){ngExternalError="reserved_tce_property_requires_native_recipe";return qfalse;}
  for(k=node->propertyHead;k;k=ngCustomProperties[k-1].next)
   if(!strcmp(key,ngCustomPropertyText+ngCustomProperties[k-1].key)){ngExternalError="duplicate_custom_property_key";return qfalse;}
  size=(int)strlen(key)+(int)strlen(value)+2;
  if(size>NG_PROPERTY_BYTES-ngCustomPropertyBytes){ngExternalError="custom_property_storage_exceeds_1mib";return qfalse;}
  property=&ngCustomProperties[ngCustomPropertyCount];property->owner=index;property->next=node->propertyHead;
  property->key=ngCustomPropertyBytes;strcpy(ngCustomPropertyText+ngCustomPropertyBytes,key);ngCustomPropertyBytes+=(int)strlen(key)+1;
  property->value=ngCustomPropertyBytes;strcpy(ngCustomPropertyText+ngCustomPropertyBytes,value);ngCustomPropertyBytes+=(int)strlen(value)+1;
  node->propertyHead=++ngCustomPropertyCount;node->propertyCount++;
 }
 while(*ngExternalCursor&&*ngExternalCursor<=' ')ngExternalCursor++;
 if(*ngExternalCursor){ngExternalError="trailing_property_data";return qfalse;}
 return qtrue;
}
/* Optional Format5/6 tail. Names, facing and imported identity have no ability
 * to alter the already parsed physical recipe, radius, flags or adjacency. */
static qboolean NG_ExternalMetadataRead(void) {
 char token[64];int version,count,i,j,id,index;unsigned int uid;
 if(ngExternalVersion<5){ngExternalError="metadata_requires_authored_format5_or_6";return qfalse;}
 if(!NG_ExternalToken(token,sizeof(token)))return qfalse;
 if(!strcmp(token,"TCE_NODE_PROPERTIES"))return NG_ExternalPropertiesRead(qtrue);
 if(strcmp(token,"TCE_NODE_METADATA")){ngExternalError="trailing_candidate_data";return qfalse;}
 if(!NG_ExternalInt(&version,1)||version!=1){ngExternalError="unsupported_metadata_version";return qfalse;}
 if(!NG_ExternalInt(&count,ngCount))return qfalse;
 memset(ngExternalMetadataSeen,0,sizeof(ngExternalMetadataSeen));memset(ngUIDSet,0,sizeof(ngUIDSet));
 for(i=0;i<count;i++) {
  ngNode *node;
  if(!NG_ExternalInt(&id,0x7fffffff))return qfalse;
  index=NG_Find(id);
  if(index<0){ngExternalError="unknown_metadata_source_id";return qfalse;}
  if(ngExternalMetadataSeen[index]){ngExternalError="duplicate_metadata_source_id";return qfalse;}
  ngExternalMetadataSeen[index]=1;node=&ngNodes[index];
  if(!NG_ExternalFlagWord(&uid))return qfalse;
  /* Omni stores a uint32 but GetUID, path IDs and action target parsers are
   * signed int. Preserve precisely the usable nonnegative identity range. */
  if(uid>0x7fffffffu){ngExternalError="metadata_uid_exceeds_omnibot_signed_target_range";return qfalse;}
  if(uid&&!NG_UIDInsert(uid)){ngExternalError="duplicate_metadata_uid";return qfalse;}
  node->importUID=uid;
  for(j=0;j<3;j++)if(!NG_ExternalFloat(&node->facing[j]))return qfalse;
  if((double)node->facing[0]*node->facing[0]+(double)node->facing[1]*node->facing[1]+(double)node->facing[2]*node->facing[2]>1.0201002) {
   ngExternalError="metadata_facing_is_not_a_direction_vector";return qfalse;
  }
  if(!NG_ExternalName(node->name))return qfalse;
  node->metadataSupplied=qtrue;
 }
 ngMetadataInputCount=count;
 while(*ngExternalCursor&&*ngExternalCursor<=' ')ngExternalCursor++;
 if(*ngExternalCursor&&!NG_ExternalPropertiesRead(qfalse))return qfalse;
 return qtrue;
}
static void NG_ExternalReset(void) {
 int i;
 ngTraceBudget=NG_TRACE_BASE_BUDGET;
 ngCount=ngEdgeCount=ngTraces=ngStepDiagnostics=0;ngProbe=qfalse;
 ngDoorCaseCount=ngDoorCandidates=ngDoorLinks=ngDoorTraceStart=ngDoorProofTraces=0;ngDoorStop="manual_recipe_validation";
 memset(ngDoorCases,0,sizeof(ngDoorCases));
 /* Format5 deliberately skips discovery. Clear every prior import's action
  * receipt before parsing, so edited/deleted IDs cannot inherit old proofs. */
 ngDropProofCount=ngJumpProofCount=ngLadderProofCount=ngFreeClimbProofCount=0;ngSimpleTopologyRejected=0;
 ngJumpCandidates=ngJumpLinks=ngLadderCandidates=ngLadderLinks=0;
 ngJumpPrefilterChecks=ngJumpPrefilterRejected=0;
 ngBspTraversalTraces=ngGenericLadderTraces=ngJumpTraversalTraces=0;
 ngFrontJumpCaseCount=ngFrontJumpTraces=ngFrontJumpConnected=ngFrontJumpArcChecks=0;
 ngFrontJumpNoTraceRejected=ngFrontReturnDamageRejected=ngFrontReturnLandingRejected=0;
 ngFrontJumpStop="authored_graph_no_rediscovery";
 ngLandingProbes=ngLandingGrounded=ngLandingSupported=ngLandingArcs=ngLandingWalks=0;
 ngLowerTransferTests=ngLowerTransferSuccesses=0;ngDropTests=ngDropAccepted=0;
 memset(ngLadderFailure,0,sizeof(ngLadderFailure));memset(ngExitFailures,0,sizeof(ngExitFailures));
 memset(ngJumpProofs,0,sizeof(ngJumpProofs));memset(ngLadderProofs,0,sizeof(ngLadderProofs));
 memset(ngDropProofs,0,sizeof(ngDropProofs));memset(ngFrontJumpCases,0,sizeof(ngFrontJumpCases));
 memset(ngFreeClimbProofs,0,sizeof(ngFreeClimbProofs));
 memset(ngNodes,0,sizeof(ngNodes));memset(ngEdges,0,sizeof(ngEdges));
 memset(ngExternalSpawn,0,sizeof(ngExternalSpawn));
 memset(ngSpawnReachable,0,sizeof(ngSpawnReachable));
 memset(ngCoverageVisited,0,sizeof(ngCoverageVisited));
 memset(ngConditionalGroundHits,0,sizeof(ngConditionalGroundHits));
 NG_TrCacheReset();
 ngWidth=g_newbbox.integer?16.f:14.f;ngOrigin=g_newbbox.integer?30.f:24.f;
 ngStand=g_newbbox.integer?87.f:70.f;ngDuck=g_newbbox.integer?61.f:49.f;
 ngExternalImport=qtrue;ngExternalManualDraft=ngExternalSpatialImport=qfalse;ngDetourRaw=ngDetourKept=0;
 ngExternalSuppliedCount=ngExternalSuppliedEdges=ngExternalMissingRegionCount=0;
 ngExternalVersion=0;ngMetadataInputCount=ngCustomPropertyCount=ngCustomPropertyBytes=0;
 ngExternalInputTraces=0;ngExternalInputComplete=qfalse;
 ngExternalDetourCaseCount=0;
 memset(ngExternalFloorAccepted,0,sizeof(ngExternalFloorAccepted));
 memset(ngExternalEdgeAccepted,0,sizeof(ngExternalEdgeAccepted));
 for(i=0;i<NG_NODES;i++)ngExternalFloorReason[i]=NULL;
 for(i=0;i<NG_EDGES;i++)ngExternalEdgeReason[i]=NULL;
 memset(ngExternalDirectAttempts,0,sizeof(ngExternalDirectAttempts));memset(ngExternalDetourAttempts,0,sizeof(ngExternalDetourAttempts));
 memset(ngExternalMissingRegion,-1,sizeof(ngExternalMissingRegion));
 NG_ExternalShortcutsReset();
 NG_ExternalJunctionsReset();
 NG_SpatialSparseReset();
 ngDoorRepairRun=qfalse;
 NG_PathRepairReset();
 ngCoverageSamples=ngCoverageRejected=ngCoverageAdded=ngCoverageExpanded=ngCoverageRemaining=0;
 ngCoverageSpawnPending=ngCoveragePasses=ngCoverageTraceCeiling=0;
 ngCoverageStop="external_graph_no_floor_raster";
 ngCampCount=ngQuarantinedCount=ngGeometrySamples=ngGeometrySeeds=ngSpawnSeeds=0;
 ngReachableCount=ngIsolatedCount=0;
 ngGeometryBoundsValid=NG_WorldBounds();ngGeometryScanComplete=qfalse;
 ngPortalCandidates=ngPortalConnected=ngPortalAdded=ngPortalTraces=0;
 ngPortalStop="external_graph_no_portal_search";
 ngJumpCandidates=ngJumpLinks=ngLadderCandidates=ngLadderLinks=0;
 ngBspLadderSurfaces=ngBspLadderPairCount=0;
 ngSimplifyRemoved=ngSimplifyOrphans=ngSimplifyLinks=ngSimplifyCandidates=ngSparseRemoved=0;
 ngSimplifyStop="external_sparse_graph_not_contracted";
}
static qboolean NG_ExternalRead(const char *path) {
 fileHandle_t file;char token[MAX_QPATH];int size,version,count,edges,i,j,value;
 size=trap_FS_FOpenFile(path,&file,FS_READ);
 if(!file){ngExternalError="candidate_file_unavailable";return qfalse;}
 if(size<=0||size>NG_EXTERNAL_INPUT) {
  trap_FS_FCloseFile(file);ngExternalError="candidate_file_size_out_of_range";return qfalse;
 }
 trap_FS_Read(ngExternalText,size,file);trap_FS_FCloseFile(file);ngExternalText[size]=0;
 if(memchr(ngExternalText,0,size)){ngExternalError="embedded_zero_byte";return qfalse;}
 ngExternalCursor=ngExternalText;ngExternalLine=1;
 if(!NG_ExternalToken(token,sizeof(token)))return qfalse;
 if(strcmp(token,"TCE_NAVMESH")){ngExternalError="bad_magic";return qfalse;}
 if(!NG_ExternalInt(&version,6))return qfalse;
 if(version<1||version>6){ngExternalError="unsupported_version";return qfalse;}
 ngExternalVersion=version;
 ngExternalManualDraft=version==3||version>=5;
 ngExternalSpatialImport=version==4;
 if(!NG_ExternalToken(token,sizeof(token)))return qfalse;
 if(Q_stricmp(token,level.rawmapname)){ngExternalError="loaded_map_mismatch";return qfalse;}
 if(!NG_ExternalInt(&count,0x7fffffff)||!NG_ExternalInt(&edges,0x7fffffff))return qfalse;
 /* Leave explicit room for spawn, portal and action nodes. Never truncate
  * a spatial graph when its raster and narrow-entry samples need this room. */
 if(count>(version>=3?NG_EXTERNAL_NODE_LIMIT:NG_EXTERNAL_LEGACY_NODE_LIMIT)) {
  ngExternalError=version>=3?"input_nodes_exceed_15872_with_action_reserve":"legacy_input_nodes_exceed_4096";return qfalse;
 }
 if(edges>(version>=3?NG_EXTERNAL_EDGE_LIMIT:NG_EXTERNAL_LEGACY_EDGE_LIMIT)) {
  ngExternalError=version>=3?"input_links_exceed_253952_with_action_reserve":"legacy_input_links_exceed_65536";return qfalse;
 }
 if(!count||!edges){ngExternalError="empty_candidate_graph";return qfalse;}
 for(i=0;i<count;i++) {
  ngNode *node=&ngNodes[i];
  if(!NG_ExternalInt(&node->id,0x7fffffff))return qfalse;
  if(NG_Find(node->id)>=0){ngExternalError="duplicate_node_id";return qfalse;}
  for(j=0;j<3;j++)if(!NG_ExternalFloat(&node->raw[j]))return qfalse;
  /* Text coordinates are feet, whereas NG_Node and the report raw_position
   * use player origins. Keep the original feet in feet until validation. */
  VectorCopy(node->raw,node->feet);node->raw[2]+=ngOrigin;node->index=-1;
  if(!NG_ExternalInt(&value,1))return qfalse;node->crouch=value?qtrue:qfalse;
  if(!NG_ExternalInt(&value,1))return qfalse;ngExternalSpawn[i]=(unsigned char)value;
  if(version>=2){if(!NG_ExternalInt(&value,1))return qfalse;node->topologyProtected=value?qtrue:qfalse;}
  if(version==3) {
   if(!NG_ExternalInt(&value,NG_ORDINARY_RADIUS_MAX))return qfalse;
   if(value<10){ngExternalError="manual_arrival_radius_below_10";return qfalse;}
   node->requestedRadius=(float)value;
   if(!NG_ExternalInt(&value,64))return qfalse;
   if(value!=(node->crouch?64:0)){ngExternalError="manual_flags_must_match_crouch_0_or_64";return qfalse;}
   /* Crouch is ordinary stance, not an action. NG_Save supplies flag 64. */
  }
  if(version>=5) {
   unsigned int low,high;
   if(!NG_ExternalFloat(&node->requestedRadius)||node->requestedRadius<1.f||node->requestedRadius>NG_ORDINARY_RADIUS_MAX){ngExternalError="invalid_arrival_radius";return qfalse;}
   if(!NG_ExternalFlagWord(&low)||!NG_ExternalFlagWord(&high))return qfalse;
   node->editFlags=((unsigned long long)high<<32)|low;node->authored=qtrue;node->editSourceId=node->id;
   if(!NG_ExternalInt(&node->editRole,255)||!NG_ExternalInt(&value,0x7fffffff))return qfalse;
   if((node->editRole&128)&&((node->editRole&(2|4|8|16|32|64))||!(low&NG_NAV_CLIMB)||
      (low&((1u<<6)|(1u<<20)|(1u<<27)|(1u<<18)|(1u<<8))))) {
    ngExternalError="invalid_or_conflicting_free_climb_recipe";return qfalse;
   }
   node->editDropId=value-1;node->radius=node->requestedRadius;
   if((node->editRole&64)&&((node->editRole&(2|4|8|16|32))||(low&((1u<<6)|(1u<<9)|(1u<<20))))) {
    ngExternalError="conflicting_standing_jump_recipe_or_stance";return qfalse;
   }
   if(node->crouch!=((low&64)!=0)){ngExternalError="crouch_flag_mismatch";return qfalse;}
   /* Classify physical transitions separately from tactical/team flags. */
   node->navFlags=low&((1u<<9)|(1u<<27)|(1u<<18)|(1u<<8));
  }
  if(version==6){
   ngDoorRecipe *r=&node->door;int entity,use,target,useSource,entry;
   if(!NG_ExternalInt(&entity,ENTITYNUM_WORLD)||!NG_ExternalInt(&use,ENTITYNUM_WORLD)||
      !NG_ExternalInt(&target,0x7fffffff)||!NG_ExternalInt(&r->mode,3)||!NG_ExternalInt(&r->team,3)||
      !NG_ExternalInt(&r->role,3)||!NG_ExternalInt(&r->model,MAX_MODELS-1)||!NG_ExternalInt(&r->useModel,MAX_MODELS-1)||
      !NG_ExternalInt(&useSource,0x7fffffff)||!NG_ExternalInt(&entry,0x7fffffff))return qfalse;
   for(j=0;j<3;j++)if(!NG_ExternalFloat(&r->aim[j]))return qfalse;
   if(r->mode){
    if(entity<=MAX_CLIENTS||use<=MAX_CLIENTS||(!target&&r->role!=3)||!useSource||!entry||!r->team||!r->role||!r->model||
       !(node->editFlags&NG_DOOR_FLAG)||node->navFlags||node->editRole){ngExternalError="invalid_or_conflicting_door_recipe";return qfalse;}
    r->entity=entity-1;r->useEntity=use-1;r->target=target-1;r->useSource=useSource-1;r->entry=entry-1;
    node->navFlags|=NG_DOOR_FLAG;
   }else if(entity||use||target||r->team||r->role||r->model||r->useModel||useSource||entry||
             VectorLengthSquared(r->aim)>0.f){ngExternalError="door_metadata_without_mode";return qfalse;}
  }
  if(version>=5) {
   /* A larger ordinary arrival request is not permission to widen a physical
    * ladder/jump/drop/door recipe. Classify only after all Format6 fields. */
   qboolean action=node->navFlags||node->editRole||node->door.mode;
   if(action&&node->requestedRadius>NG_ACTION_RADIUS_MAX){ngExternalError="action_arrival_radius_above_100";return qfalse;}
   if(!action&&node->requestedRadius<10.f){ngExternalError="ordinary_arrival_radius_below_10";return qfalse;}
  }
  node->reason="external_candidate_not_validated";ngCount++;
 }
 for(i=0;i<edges;i++) {
  ngEdge *edge=&ngEdges[i];int id;
  if(!NG_ExternalInt(&id,0x7fffffff))return qfalse;edge->from=NG_Find(id);
  if(!NG_ExternalInt(&id,0x7fffffff))return qfalse;edge->to=NG_Find(id);
  if(edge->from<0||edge->to<0||edge->from==edge->to) {
   ngExternalError="unknown_or_identical_edge_endpoint";return qfalse;
  }
  edge->reason="external_candidate_not_validated";ngEdgeCount++;
 }
 ngExternalSuppliedCount=ngCount;ngExternalSuppliedEdges=ngEdgeCount;
 while(*ngExternalCursor&&*ngExternalCursor<=' ')ngExternalCursor++;
 if(*ngExternalCursor&&!NG_ExternalMetadataRead())return qfalse;
 return qtrue;
}
static qboolean NG_ExternalSpawnEntity(const char *name) {
 if(!name)return qfalse;
 return (!Q_stricmpn(name,"info_player_",12)&&Q_stricmp(name,"info_player_intermission"))||
  !Q_stricmp(name,"team_CTF_redspawn")||!Q_stricmp(name,"team_CTF_bluespawn")||
  !Q_stricmp(name,"team_CTF_redplayer")||!Q_stricmp(name,"team_CTF_blueplayer");
}
static qboolean NG_ExternalSpawnAnchor(ngNode *node) {
 int i;
 for(i=MAX_CLIENTS;i<level.num_entities&&ngTraces<NG_TRACE_BUDGET;i++) {
  gentity_t *ent=&g_entities[i];ngNode spawn;vec3_t delta;
  if(!ent->inuse||!NG_ExternalSpawnEntity(ent->classname))continue;
  VectorSubtract(ent->r.currentOrigin,node->raw,delta);
  if(DotProduct(delta,delta)>256.f*256.f)continue;
  memset(&spawn,0,sizeof(spawn));spawn.id=0;VectorCopy(ent->r.currentOrigin,spawn.raw);
  NG_Node(&spawn);if(!spawn.accepted)continue;
  if(!NG_Walk(&spawn,node,ngStand))return qtrue;
  if(!NG_Walk(&spawn,node,ngDuck)){node->crouch=qtrue;return qtrue;}
 }
 return qfalse;
}
typedef struct {int from,to,groupFrom,groupTo,front;float distance;} ngExternalPair;
#define NG_EXTERNAL_PAIRS 32768
static ngExternalPair ngExternalPairs[NG_EXTERNAL_PAIRS];
static int ngExternalComponent[NG_NODES],ngExternalComponentSize[NG_NODES];
static int ngExternalHead[NG_NODES],ngExternalReverseHead[NG_NODES];
static int ngExternalNext[NG_EDGES],ngExternalReverseNext[NG_EDGES];
static unsigned char ngExternalSearchAttempted[NG_NODES];
static qboolean NG_ExternalOrdinary(int index) {
 ngNode *node=&ngNodes[index];
 return node->accepted&&!node->flags&&!node->packed&&!node->navFlags&&NG_TrDropTarget(index)<0;
}
/* Exact strongly connected components with bounded iterative traversals.
 * A union of spawn-reachable areas is insufficient: opposing teams may
 * each have a perfectly valid but mutually disconnected spawn component. */
static qboolean NG_ExternalConnectivityNode(int index,qboolean actions) {
 ngNode *node=&ngNodes[index];
 return node->accepted&&!node->flags&&!node->packed&&(actions||(!node->navFlags&&NG_TrDropTarget(index)<0));
}
static int NG_ExternalBuildComponents(unsigned char *main,qboolean actions) {
 unsigned char seen[NG_NODES]={0},hasSpawn[NG_NODES]={0};
 int stack[NG_NODES],cursor[NG_NODES],order[NG_NODES],size=0,groups=0;
 int i,k,top,best=-1;
 memset(main,0,NG_NODES);
 for(i=0;i<ngCount;i++) {
  ngExternalHead[i]=ngExternalReverseHead[i]=ngExternalComponent[i]=-1;
  ngExternalComponentSize[i]=0;main[i]=0;
 }
 for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted) {
  int from=ngEdges[k].from,to=ngEdges[k].to;
  if(from<0||to<0||from>=ngCount||to>=ngCount||
     !NG_ExternalConnectivityNode(from,actions)||!NG_ExternalConnectivityNode(to,actions))continue;
  ngExternalNext[k]=ngExternalHead[from];ngExternalHead[from]=k;
  ngExternalReverseNext[k]=ngExternalReverseHead[to];ngExternalReverseHead[to]=k;
 }
 for(i=0;i<ngCount;i++)if(NG_ExternalConnectivityNode(i,actions)&&!seen[i]) {
  top=0;stack[0]=i;cursor[0]=ngExternalHead[i];seen[i]=1;
  while(top>=0) {
   k=cursor[top];
   while(k>=0&&seen[ngEdges[k].to])k=ngExternalNext[k];
   if(k<0){order[size++]=stack[top--];continue;}
   cursor[top]=ngExternalNext[k];
   {int to=ngEdges[k].to;seen[to]=1;stack[++top]=to;cursor[top]=ngExternalHead[to];}
  }
 }
 for(i=size-1;i>=0;i--)if(ngExternalComponent[order[i]]<0) {
  int head=0,tail=0;stack[tail++]=order[i];ngExternalComponent[order[i]]=groups;
  while(head<tail) {
   int node=stack[head++];ngExternalComponentSize[groups]++;
   if(ngSpawnReachable[node])hasSpawn[groups]=1;
   for(k=ngExternalReverseHead[node];k>=0;k=ngExternalReverseNext[k]) {
    int from=ngEdges[k].from;
    if(ngExternalComponent[from]<0){ngExternalComponent[from]=groups;stack[tail++]=from;}
   }
  }
  if(hasSpawn[groups]&&(best<0||ngExternalComponentSize[groups]>ngExternalComponentSize[best]))best=groups;
  groups++;
 }
 if(best>=0)for(i=0;i<ngCount;i++)main[i]=(unsigned char)(ngExternalComponent[i]==best);
 return best;
}
static int NG_ExternalComponents(unsigned char *main) {
 return NG_ExternalBuildComponents(main,qfalse);
}
static int QDECL NG_ExternalPairCompare(const void *left,const void *right) {
 const ngExternalPair *a=(const ngExternalPair *)left,*b=(const ngExternalPair *)right;
 int rankA=(ngSpawnReachable[a->from]||ngSpawnReachable[a->to])?0:1;
 int rankB=(ngSpawnReachable[b->from]||ngSpawnReachable[b->to])?0:1;
 if(rankA!=rankB)return rankA-rankB;
 if(a->distance<b->distance)return -1;
 if(a->distance>b->distance)return 1;
 if(a->from!=b->from)return a->from-b->from;
 return a->to-b->to;
}
static int QDECL NG_ExternalFrontCompare(const void *left,const void *right) {
 const ngExternalPair *a=(const ngExternalPair *)left,*b=(const ngExternalPair *)right;
 if(a->groupFrom!=b->groupFrom)return a->groupFrom-b->groupFrom;
 if(a->groupTo!=b->groupTo)return a->groupTo-b->groupTo;
 return NG_ExternalPairCompare(left,right);
}
static void NG_ExternalBridge(int from,int to,qboolean crouch) {
 int k;NG_TrLink(from,to,"external_native_component_bridge");
 if(crouch) {
  ngNodes[from].crouch=ngNodes[to].crouch=qtrue;
  for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted&&ngEdges[k].from==from&&ngEdges[k].to==to)ngEdges[k].crouch=qtrue;
 }
}
/* Repair is scheduled per component frontier, not per densely sampled point.
 * Each pass keeps its own caps; aggregate report counters never disable the
 * second pass. Only ordinary endpoints enter the native walking proof. */
#define NG_EXTERNAL_DIRECT_CHECKS 256
#define NG_EXTERNAL_LOCAL_SEARCHES 48
static unsigned char ngExternalFrontTries[NG_EXTERNAL_PAIRS];
static void NG_ExternalRepair(qboolean afterActions) {
 unsigned char main[NG_NODES],targets[NG_NODES];int groupTries[NG_NODES];
 int triedFrom[NG_EXTERNAL_DIRECT_CHECKS],triedTo[NG_EXTERNAL_DIRECT_CHECKS];
 int triedFront[NG_EXTERNAL_DIRECT_CHECKS];
 int begin=ngTraces,limit=begin+(ngExternalSpatialImport&&afterActions?400000:300000),directLimit=begin+100000;
 int reserve=afterActions?200000:550000;
 int i,j,k,pairs=0,fronts=0,direct=0,directChecks=0,searched=0,mainGroup;
 int round,storageSkipped=0,localIteration=0,connectedBefore=ngPortalConnected;
 ngPortalStop="external_component_frontier_exhausted";
 if(!ngSpawnSeeds){ngPortalStop="no_spawn_seeds";return;}
 if(limit>NG_TRACE_BUDGET-reserve)limit=NG_TRACE_BUDGET-reserve;
 if(directLimit>limit)directLimit=limit;
 if(ngTraces>=limit-4000){ngPortalStop="external_component_trace_reserve";return;}
 NG_SpawnReachability();mainGroup=NG_ExternalBuildComponents(main,afterActions);
 if(mainGroup<0){ngPortalStop="no_valid_spawn_component";return;}
 /* Include currently detached pairs too: one side can become reachable after
  * an earlier bridge. This does not admit detached roofs; reachability is
  * checked again immediately before each proof. */
 for(i=0;i<ngCount;i++)if(NG_ExternalOrdinary(i))
  for(j=i+1;j<ngCount;j++)if(NG_ExternalOrdinary(j)&&
      ngExternalComponent[i]!=ngExternalComponent[j]) {
   vec3_t delta;float distance;int ga,gb;
   VectorSubtract(ngNodes[i].feet,ngNodes[j].feet,delta);distance=DotProduct(delta,delta);
   if(distance>256.f*256.f)continue;
   if(pairs>=NG_EXTERNAL_PAIRS){storageSkipped++;continue;}
   ga=ngExternalComponent[i];gb=ngExternalComponent[j];
   ngExternalPairs[pairs].from=ga<gb?i:j;ngExternalPairs[pairs].to=ga<gb?j:i;
   ngExternalPairs[pairs].groupFrom=ga<gb?ga:gb;
   ngExternalPairs[pairs].groupTo=ga<gb?gb:ga;
   ngExternalPairs[pairs++].distance=distance;
  }
 /* Assign stable frontier IDs before sorting by distance. Round zero gets
  * one attempt per frontier; later rounds use distinct approaches at least
  * 64 units apart, so a dense door cannot consume every direct test. */
 qsort(ngExternalPairs,pairs,sizeof(ngExternalPairs[0]),NG_ExternalFrontCompare);
 for(k=0;k<pairs;k++) {
  if(!k||ngExternalPairs[k].groupFrom!=ngExternalPairs[k-1].groupFrom||
      ngExternalPairs[k].groupTo!=ngExternalPairs[k-1].groupTo)fronts++;
  ngExternalPairs[k].front=fronts-1;
 }
 memset(ngExternalFrontTries,0,sizeof(ngExternalFrontTries));
 qsort(ngExternalPairs,pairs,sizeof(ngExternalPairs[0]),NG_ExternalPairCompare);
 for(round=0;round<4&&directChecks<NG_EXTERNAL_DIRECT_CHECKS&&ngTraces<directLimit-2000;round++)
  for(k=0;k<pairs&&directChecks<NG_EXTERNAL_DIRECT_CHECKS&&ngTraces<directLimit-2000;k++) {
   ngExternalPair *pair=&ngExternalPairs[k];ngNode a,b;qboolean separated=qtrue;
   int at;i=pair->from;j=pair->to;
   /* A spawn in each SCC is not a return path. Current directed
    * components already include accepted action cycles after traversal. */
   if(ngExternalFrontTries[pair->front]>round||
      ngExternalComponent[i]==ngExternalComponent[j])continue;
   for(at=0;at<directChecks;at++)if(triedFront[at]==pair->front) {
    vec3_t da,db;
    VectorSubtract(ngNodes[i].feet,ngNodes[triedFrom[at]].feet,da);
    VectorSubtract(ngNodes[j].feet,ngNodes[triedTo[at]].feet,db);
    if(DotProduct(da,da)<64.f*64.f&&DotProduct(db,db)<64.f*64.f){separated=qfalse;break;}
   }
   if(!separated)continue;
   if(ngEdgeCount>=NG_EDGES-512){ngPortalStop="graph_budget";break;}
   triedFrom[directChecks]=i;triedTo[directChecks]=j;triedFront[directChecks]=pair->front;
   directChecks++;ngExternalFrontTries[pair->front]++;ngPortalCandidates++;
   ngExternalDirectAttempts[i]++;ngExternalDirectAttempts[j]++;
   a=ngNodes[i];b=ngNodes[j];if(!NG_BothWays(&a,&b))continue;
   NG_ExternalBridge(i,j,a.crouch||b.crouch);NG_ExternalBridge(j,i,a.crouch||b.crouch);
   NG_SpawnReachability();mainGroup=NG_ExternalBuildComponents(main,afterActions);
   direct++;ngPortalConnected++;
  }
 memset(ngExternalSearchAttempted,0,sizeof(ngExternalSearchAttempted));
 for(localIteration=0;localIteration<NG_EXTERNAL_LOCAL_SEARCHES&&ngTraces<limit-4000;localIteration++) {
  int seed=-1,least=4,priority=2,target=-1;float best=384.f,bestScore=1e30f;
  if(ngCount>=NG_NODES-128||ngEdgeCount>=NG_EDGES-512){ngPortalStop="graph_budget";break;}
  NG_SpawnReachability();mainGroup=NG_ExternalBuildComponents(main,afterActions);
  if(mainGroup<0)break;
  memset(groupTries,0,sizeof(groupTries));
  for(i=0;i<ngCount;i++)if(ngExternalSearchAttempted[i]&&ngExternalComponent[i]>=0)groupTries[ngExternalComponent[i]]++;
  /* main already includes full directed action cycles after traversal.
   * Keep checking the other SCCs even if they contain their own spawns or
   * an incoming-only jump. Only ordinary endpoints enter NG_BothWays; no
   * existing action recipe is replaced or reinterpreted as a walk. */
  memcpy(targets,main,sizeof(targets));
  memcpy(ngSpawnReachable,targets,sizeof(targets));
  for(i=0;i<ngCount;i++)if(NG_ExternalOrdinary(i)&&!targets[i]&&!ngExternalSearchAttempted[i]) {
   int group=ngExternalComponent[i],nearest,rank;float distance,score;qboolean separated=qtrue,better;
   if(group<0||groupTries[group]>=3||(!ngExternalSpatialImport&&groupTries[group]>least))continue;
   for(j=0;j<ngCount;j++)if(ngExternalSearchAttempted[j]&&ngExternalComponent[j]==group) {
    vec3_t delta;VectorSubtract(ngNodes[i].feet,ngNodes[j].feet,delta);
    if(DotProduct(delta,delta)<64.f*64.f){separated=qfalse;break;}
   }
   if(!separated)continue;
   rank=1;for(j=0;j<ngCount;j++)if(ngExternalComponent[j]==group&&
      !strcmp(ngNodes[j].reason,"map_entity_spawn_seed")){rank=0;break;}
   distance=NG_PortalDistance(ngNodes[i].feet,&nearest);
   if(nearest<0||distance>=384.f)continue;
   score=distance;
   if(ngExternalSpatialImport) {
    /* Spatial samples represent coverage. Give a large detached room a fair
     * attempt before nearby one-point fragments exhaust the bounded search.
     * This orders proofs only: size is not evidence of a traversable route. */
    int gain=ngExternalComponentSize[group];
    if(gain>128)gain=128;if(gain<1)gain=1;
    score=(distance+64.f)/(float)sqrt((double)gain);
    better=rank<priority||(rank==priority&&(groupTries[group]<least||
      (groupTries[group]==least&&(score<bestScore||(score==bestScore&&distance<best)))));
   }else better=groupTries[group]<least||rank<priority||(rank==priority&&distance<best);
   if(better) {
    seed=i;target=nearest;least=groupTries[group];priority=rank;best=distance;bestScore=score;
   }
  }
  if(seed<0){NG_SpawnReachability();break;}
  ngExternalSearchAttempted[seed]=1;ngPortalCandidates++;searched++;ngExternalDetourAttempts[seed]++;
  {int localLimit=ngTraces+24000,oldMain=-1,caseIndex=-1,searchBegin=ngTraces;qboolean connected;
   if(ngExternalSpatialImport&&ngExternalDetourCaseCount<NG_EXTERNAL_DETOUR_REPORT) {
    ngExternalDetourCase *entry=&ngExternalDetourCases[ngExternalDetourCaseCount];
    caseIndex=ngExternalDetourCaseCount++;
    entry->seed=ngNodes[seed].id;entry->target=ngNodes[target].id;
    entry->componentNodes=ngExternalComponentSize[ngExternalComponent[seed]];
    entry->previousAttempts=least;entry->spawnRank=priority;
    entry->distance=best;entry->score=bestScore;entry->afterActions=afterActions;
   }
   if(localLimit>limit)localLimit=limit;
   for(i=0;i<ngCount;i++)if(targets[i]){oldMain=i;break;}
   NG_PortalSearchPath(seed,localLimit);
   NG_SpawnReachability();mainGroup=NG_ExternalBuildComponents(main,afterActions);
   connected=oldMain>=0&&ngExternalComponent[seed]>=0&&
      ngExternalComponent[seed]==ngExternalComponent[oldMain];
   if(connected)ngPortalConnected++;
   if(caseIndex>=0) {
    ngExternalDetourCases[caseIndex].connected=connected;
    ngExternalDetourCases[caseIndex].traces=ngTraces-searchBegin;
   }
  }
 }
 NG_SpawnReachability();ngPortalTraces+=ngTraces-begin;
 if(ngTraces>=limit-4000)ngPortalStop="external_component_trace_budget";
 else if(localIteration==NG_EXTERNAL_LOCAL_SEARCHES)ngPortalStop="external_component_candidate_budget";
 else if(storageSkipped)ngPortalStop="external_component_storage_budget";
 G_Printf("navgen external repair: pass=%s fronts=%d direct_checks=%d direct_bridges=%d local_searches=%d connected=%d turnpoints=%d traces=%d storage_skipped=%d stop=%s\n",
  afterActions?"after_actions":"before_actions",fronts,directChecks,direct,searched,
  ngPortalConnected-connectedBefore,ngPortalAdded,ngTraces-begin,storageSkipped,ngPortalStop);
}
#define NG_LOCAL_MAX_CHECKS 48
#define NG_LOCAL_MAX_PREFILTERS 2048
#define NG_LOCAL_PREFILTER_REPORT 24
#define NG_LOCAL_TRACE_BUDGET 100000
#define NG_LOCAL_INFINITY 1e20f
typedef struct {
 int from,to;float direct,forward,reverse;
 qboolean tried,initialCandidate;
} ngLocalPair;
typedef struct {
 int from,to,traces;float direct,before[2],after[2],final[2];
 const char *stand[2],*duck[2],*reason;
 qboolean accepted,crouch;
} ngLocalCase;
static ngLocalPair ngLocalPairs[NG_EXTERNAL_PAIRS];
static ngLocalCase ngLocalCases[NG_LOCAL_MAX_CHECKS];
static int ngLocalPairCount,ngLocalInitial,ngLocalChecked,ngLocalAdded,ngLocalRejected,ngLocalSubsumed,ngLocalTraces;
static int ngLocalPrefilterChecked,ngLocalPrefilterRejected;
static int ngLocalPrefilterPairs[NG_LOCAL_PREFILTER_REPORT];
static int ngLocalHeap[NG_NODES],ngLocalHeapPosition[NG_NODES];
static float ngLocalEdgeLength[NG_EDGES],ngLocalDistances[4][NG_NODES];
static const char *ngLocalStop;
static qboolean ngLocalGraphValid,ngLocalFinalized;
static void NG_ExternalShortcutsReset(void) {
 ngLocalPairCount=ngLocalInitial=ngLocalChecked=ngLocalAdded=ngLocalRejected=ngLocalSubsumed=ngLocalTraces=0;
 ngLocalPrefilterChecked=ngLocalPrefilterRejected=0;
 ngLocalStop="not_started";
 ngLocalGraphValid=qtrue;ngLocalFinalized=qfalse;
 memset(ngLocalPairs,0,sizeof(ngLocalPairs));memset(ngLocalCases,0,sizeof(ngLocalCases));
}
static qboolean NG_LocalNode(int index) {
 ngNode *node=&ngNodes[index];
 if(!NG_ExternalOrdinary(index)||node->ent[0]||node->ent[1]||node->ent[2]||node->count)return qfalse;
 if(node->name[0]&&Q_stricmpn(node->name,"tce2_camp_",10))return qfalse;
 return node->reason&&!strstr(node->reason,"ladder")&&!strstr(node->reason,"jump");
}
static void NG_LocalLinks(void) {
 int i,k;
 for(i=0;i<ngCount;i++)ngExternalHead[i]=ngExternalReverseHead[i]=-1;
 for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted) {
  int from=ngEdges[k].from,to=ngEdges[k].to;vec3_t delta;
  /* Candidate endpoints exclude action anchors, but an ordinary, validated
   * floor leg through a landing anchor is still part of the walk graph.
   * Use the same graph as sparsification when measuring its final witnesses;
   * CLIMB/JUMP transitions remain excluded by their endpoint nav flags. */
  if(from<0||to<0||from>=ngCount||to>=ngCount||!NG_SparseOrdinary(k))continue;
  ngExternalNext[k]=ngExternalHead[from];ngExternalHead[from]=k;
  ngExternalReverseNext[k]=ngExternalReverseHead[to];ngExternalReverseHead[to]=k;
  VectorSubtract(ngNodes[from].feet,ngNodes[to].feet,delta);ngLocalEdgeLength[k]=VectorLength(delta);
 }
}
/* Exact directed geometric path lengths, with bounded decrease-key storage.
 * No all-pairs matrix is allocated in the game module. */
static void NG_LocalDistances(int source,qboolean reverse,float *distance) {
 int i,used=1;
 for(i=0;i<ngCount;i++){distance[i]=NG_LOCAL_INFINITY;ngLocalHeapPosition[i]=-1;}
 distance[source]=0.f;ngLocalHeap[0]=source;ngLocalHeapPosition[source]=0;
 while(used) {
  int node=ngLocalHeap[0],tail=ngLocalHeap[--used],at=0,k;
  ngLocalHeapPosition[node]=-2;
  if(used) {
   ngLocalHeap[0]=tail;ngLocalHeapPosition[tail]=0;
   while(at*2+1<used) {
    int child=at*2+1;
    if(child+1<used&&distance[ngLocalHeap[child+1]]<distance[ngLocalHeap[child]])child++;
    if(distance[tail]<=distance[ngLocalHeap[child]])break;
    ngLocalHeap[at]=ngLocalHeap[child];ngLocalHeapPosition[ngLocalHeap[at]]=at;at=child;
   }
   ngLocalHeap[at]=tail;ngLocalHeapPosition[tail]=at;
  }
  for(k=reverse?ngExternalReverseHead[node]:ngExternalHead[node];k>=0;
      k=reverse?ngExternalReverseNext[k]:ngExternalNext[k]) {
   int to=reverse?ngEdges[k].from:ngEdges[k].to;float next=distance[node]+ngLocalEdgeLength[k];
   if(ngLocalHeapPosition[to]==-2||next>=distance[to])continue;
   distance[to]=next;at=ngLocalHeapPosition[to];
   if(at<0){at=used++;ngLocalHeap[at]=to;ngLocalHeapPosition[to]=at;}
   while(at>0) {
    int parent=(at-1)/2;
    if(distance[ngLocalHeap[parent]]<=next)break;
    ngLocalHeap[at]=ngLocalHeap[parent];ngLocalHeapPosition[ngLocalHeap[at]]=at;at=parent;
   }
   ngLocalHeap[at]=to;ngLocalHeapPosition[to]=at;
  }
 }
}
static qboolean NG_LocalUseful(const ngLocalPair *pair) {
 return pair->forward<NG_LOCAL_INFINITY&&pair->reverse<NG_LOCAL_INFINITY&&
  pair->forward>pair->direct*1.75f&&pair->reverse>pair->direct*1.75f&&
  pair->forward-pair->direct>128.f&&pair->reverse-pair->direct>128.f;
}
/* One newly admitted bidirectional bridge can shorten any existing path.
 * Four directed distance fields update every pair exactly after the edit;
 * subsequent selection never relies on the original, now obsolete detour. */
static void NG_LocalUpdateDistances(int from,int to,float direct) {
 int i;
 NG_LocalLinks();
 NG_LocalDistances(from,qfalse,ngLocalDistances[0]);
 NG_LocalDistances(from,qtrue,ngLocalDistances[1]);
 NG_LocalDistances(to,qfalse,ngLocalDistances[2]);
 NG_LocalDistances(to,qtrue,ngLocalDistances[3]);
 for(i=0;i<ngLocalPairCount;i++) {
  ngLocalPair *pair=&ngLocalPairs[i];float route;
  route=ngLocalDistances[1][pair->from]+direct+ngLocalDistances[2][pair->to];
  if(route<pair->forward)pair->forward=route;
  route=ngLocalDistances[3][pair->from]+direct+ngLocalDistances[0][pair->to];
  if(route<pair->forward)pair->forward=route;
  route=ngLocalDistances[1][pair->to]+direct+ngLocalDistances[2][pair->from];
  if(route<pair->reverse)pair->reverse=route;
  route=ngLocalDistances[3][pair->to]+direct+ngLocalDistances[0][pair->from];
  if(route<pair->reverse)pair->reverse=route;
 }
}
static qboolean NG_LocalAdmit(int from,int to,qboolean crouch) {
 int k;
 NG_TrLink(from,to,"external_native_local_shortcut");
 for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted&&ngEdges[k].from==from&&ngEdges[k].to==to) {
  ngEdges[k].crouch=crouch;ngEdges[k].added=qtrue;
  ngEdges[k].reason="external_native_local_shortcut";return qtrue;
 }
 return qfalse;
}
/* Negative scheduling filter only: a thin line at crouched torso height
 * quickly screens pairs separated by walls. It is not a player hull, floor
 * proof or declaration that all other possible routes are blocked. Every
 * surviving pair still gets both directed NG_Walk checks below. */
static qboolean NG_LocalPrefilter(const ngLocalPair *pair) {
 vec3_t from,to;trace_t trace;
 VectorCopy(ngNodes[pair->from].feet,from);VectorCopy(ngNodes[pair->to].feet,to);
 from[2]+=ngDuck*.5f;to[2]+=ngDuck*.5f;
 if(ngTraces>=NG_TRACE_BUDGET)return qfalse;
 trap_Trace(&trace,from,NULL,NULL,to,ENTITYNUM_NONE,NG_MASK);ngTraces++;
 return !trace.startsolid&&!trace.allsolid&&trace.fraction==1.f;
}
static void NG_ExternalLocalShortcuts(void) {
 unsigned char main[NG_NODES];int i,j,start=ngTraces,limit=start+NG_LOCAL_TRACE_BUDGET;
 NG_ExternalShortcutsReset();ngLocalStop="no_remaining_material_detour";
 if(limit>NG_TRACE_BUDGET-2000)limit=NG_TRACE_BUDGET-2000;
 NG_SpawnReachability();NG_ExternalComponents(main);NG_LocalLinks();
 for(i=0;i<ngCount;i++)if(NG_LocalNode(i)&&ngSpawnReachable[i])
  for(j=i+1;j<ngCount;j++)if(NG_LocalNode(j)&&ngSpawnReachable[j]&&
      ngExternalComponent[i]>=0&&ngExternalComponent[i]==ngExternalComponent[j]) {
   vec3_t delta;float length;ngLocalPair *pair;
   VectorSubtract(ngNodes[i].feet,ngNodes[j].feet,delta);length=VectorLength(delta);
   if(length<1.f||length>320.f||fabs(delta[2])>18.25f)continue;
   if(ngLocalPairCount==NG_EXTERNAL_PAIRS){ngLocalStop="candidate_storage_budget";continue;}
   pair=&ngLocalPairs[ngLocalPairCount++];pair->from=i;pair->to=j;pair->direct=length;
   pair->forward=pair->reverse=NG_LOCAL_INFINITY;
  }
 for(i=0;i<ngCount;i++)if(NG_LocalNode(i)&&ngSpawnReachable[i]) {
  NG_LocalDistances(i,qfalse,ngLocalDistances[0]);
  for(j=0;j<ngLocalPairCount;j++) {
   ngLocalPair *pair=&ngLocalPairs[j];
   if(pair->from==i)pair->forward=ngLocalDistances[0][pair->to];
   if(pair->to==i)pair->reverse=ngLocalDistances[0][pair->from];
  }
 }
 for(i=0;i<ngLocalPairCount;i++)if(NG_LocalUseful(&ngLocalPairs[i])) {
  ngLocalPairs[i].initialCandidate=qtrue;ngLocalInitial++;
 }
 while(ngLocalChecked<NG_LOCAL_MAX_CHECKS) {
  int selected=-1,begin,direction;float score=0.f;ngLocalPair *pair;ngLocalCase *result;
  if(ngTraces>=limit-2000){ngLocalStop="trace_budget";break;}
  if(ngLocalPrefilterChecked>=NG_LOCAL_MAX_PREFILTERS){ngLocalStop="prefilter_budget";break;}
  if(ngEdgeCount>=NG_EDGES-2){ngLocalStop="edge_budget";break;}
  for(i=0;i<ngLocalPairCount;i++)if(!ngLocalPairs[i].tried&&NG_LocalUseful(&ngLocalPairs[i])) {
   float saving=ngLocalPairs[i].forward+ngLocalPairs[i].reverse-2.f*ngLocalPairs[i].direct;
   if(saving>score){score=saving;selected=i;}
  }
  if(selected<0)break;
  pair=&ngLocalPairs[selected];pair->tried=qtrue;ngLocalPrefilterChecked++;
  if(!NG_LocalPrefilter(pair)) {
   if(ngLocalPrefilterRejected<NG_LOCAL_PREFILTER_REPORT)ngLocalPrefilterPairs[ngLocalPrefilterRejected]=selected;
   ngLocalPrefilterRejected++;continue;
  }
  result=&ngLocalCases[ngLocalChecked++];
  result->from=pair->from;result->to=pair->to;result->direct=pair->direct;
  result->before[0]=pair->forward;result->before[1]=pair->reverse;
  result->reason="native_both_directions_accepted";result->accepted=qtrue;begin=ngTraces;
  for(direction=0;direction<2;direction++) {
   int from=direction?pair->to:pair->from,to=direction?pair->from:pair->to;
   const char *why=NG_Walk(&ngNodes[from],&ngNodes[to],ngStand);
   result->stand[direction]=why?why:"accepted";result->duck[direction]="not_required";
   if(why) {
    why=NG_Walk(&ngNodes[from],&ngNodes[to],ngDuck);
    result->duck[direction]=why?why:"accepted";
    if(!why)result->crouch=qtrue;
   }
   if(why){result->accepted=qfalse;result->reason=why;}
  }
  result->traces=ngTraces-begin;
  /* Do not recreate a crossing diagonal after preserving the room/portal
   * graph. A rejected shortcut leaves the existing directed route intact. */
  if(result->accepted){
   int edge;
   for(edge=0;edge<ngEdgeCount;edge++)if(ngEdges[edge].accepted){
    int a=ngEdges[edge].from,b=ngEdges[edge].to;
    if(a>=0&&b>=0&&a!=b&&!ngNodes[a].navFlags&&!ngNodes[b].navFlags&&
       NG_SimpleCross(pair->from,pair->to,a,b)){
     result->accepted=qfalse;result->reason="shortcut_would_cross_room_route";break;
    }
   }
  }
  if(result->accepted&&ngTraces<limit) {
   if(result->crouch)ngNodes[pair->from].crouch=ngNodes[pair->to].crouch=qtrue;
   if(NG_LocalAdmit(pair->from,pair->to,result->crouch)&&NG_LocalAdmit(pair->to,pair->from,result->crouch)) {
    NG_LocalUpdateDistances(pair->from,pair->to,pair->direct);ngLocalAdded++;
   }else {
    result->accepted=qfalse;result->reason="edge_install_failed";
    ngLocalGraphValid=qfalse;ngLocalRejected++;
   }
  }else {
   if(result->accepted){result->accepted=qfalse;result->reason="trace_budget";}
   ngLocalRejected++;
  }
  result->after[0]=pair->forward;result->after[1]=pair->reverse;
  if(!ngLocalGraphValid){ngLocalStop="edge_install_failed";break;}
 }
 if(ngLocalChecked==NG_LOCAL_MAX_CHECKS)ngLocalStop="native_pair_budget";
 for(i=0;i<ngLocalPairCount;i++)if(ngLocalPairs[i].initialCandidate&&!ngLocalPairs[i].tried&&
     !NG_LocalUseful(&ngLocalPairs[i]))ngLocalSubsumed++;
 ngLocalTraces=ngTraces-start;
 G_Printf("navgen local shortcuts: candidates=%d prefilter=%d/%d checked=%d added_pairs=%d rejected=%d subsumed=%d traces=%d stop=%s\n",
  ngLocalInitial,ngLocalPrefilterRejected,ngLocalPrefilterChecked,ngLocalChecked,ngLocalAdded,ngLocalRejected,ngLocalSubsumed,ngLocalTraces,ngLocalStop);
}
static qboolean NG_LocalFinalize(void) {
 int i;
 if(ngLocalFinalized)return ngLocalGraphValid;
 ngLocalFinalized=qtrue;
 if(!ngLocalGraphValid)G_Printf("navgen final graph: local shortcut phase already invalid, stop=%s\n",ngLocalStop);
 NG_LocalLinks();
 for(i=0;i<ngLocalChecked;i++) {
  ngLocalCase *result=&ngLocalCases[i];
  if(result->from<0||result->to<0||result->from>=ngCount||result->to>=ngCount) {
   result->final[0]=result->final[1]=NG_LOCAL_INFINITY;
   G_Printf("navgen final graph: shortcut case=%d has invalid endpoints %d -> %d, nodes=%d\n",
    i,result->from,result->to,ngCount);
   ngLocalGraphValid=qfalse;continue;
  }
  NG_LocalDistances(result->from,qfalse,ngLocalDistances[0]);
  result->final[0]=ngLocalDistances[0][result->to];
  NG_LocalDistances(result->to,qfalse,ngLocalDistances[0]);
  result->final[1]=ngLocalDistances[0][result->from];
  /* Sparsification may retain the direct edge or its certified <=1.25x
   * two-hop witness. Refuse publication if either promised route is lost. */
  if(result->accepted&&(result->final[0]>result->direct*1.25f+.01f||
     result->final[1]>result->direct*1.25f+.01f)) {
   G_Printf("navgen final graph: shortcut case=%d nodes=%d -> %d direct=%.3f limit=%.3f final=[%.3f,%.3f]\n",
    i,ngNodes[result->from].id,ngNodes[result->to].id,result->direct,
    result->direct*1.25f+.01f,result->final[0],result->final[1]);
   ngLocalGraphValid=qfalse;
  }
 }
 return ngLocalGraphValid;
}
static void NG_ExternalShortcutReport(void) {
 int i;float totalBefore=0.f,totalAfter=0.f,totalFinal=0.f;
 NG_LocalFinalize();
 for(i=0;i<ngLocalChecked;i++) {
  ngLocalCase *result=&ngLocalCases[i];
  totalBefore+=result->before[0]+result->before[1];totalAfter+=result->after[0]+result->after[1];
  totalFinal+=result->final[0]+result->final[1];
 }
 NG_Text(va("\"local_shortcuts\":{\"near_pairs\":%d,\"detour_candidates\":%d,\"checked\":%d,\"added_pairs\":%d,\"rejected\":%d,\"subsumed\":%d,\"traces\":%d,\"trace_budget\":%d,\"pair_budget\":%d,\"max_distance\":320,\"max_height_difference\":18.25,\"minimum_ratio_both_directions\":1.75,\"minimum_extra_distance_both_directions\":128,\"stop_reason\":\"%s\",\"directed_distance_totals\":{\"before\":%.3f,\"after_each_addition\":%.3f,\"final\":%.3f},\"cases\":[",
  ngLocalPairCount,ngLocalInitial,ngLocalChecked,ngLocalAdded,ngLocalRejected,ngLocalSubsumed,ngLocalTraces,
  NG_LOCAL_TRACE_BUDGET,NG_LOCAL_MAX_CHECKS,ngLocalStop,totalBefore,totalAfter,totalFinal));
 for(i=0;i<ngLocalChecked;i++) {
  ngLocalCase *r=&ngLocalCases[i];ngNode *a=&ngNodes[r->from],*b=&ngNodes[r->to];
  NG_Text(va("%s{\"from\":%d,\"to\":%d,\"from_position\":[%.4f,%.4f,%.4f],\"to_position\":[%.4f,%.4f,%.4f],\"direct\":%.3f,\"detour_before\":[%.3f,%.3f],\"detour_after\":[%.3f,%.3f],\"detour_final\":[%.3f,%.3f],\"accepted\":%s,\"crouch\":%s,\"traces\":%d,\"reason\":\"%s\",\"native_stand\":[\"%s\",\"%s\"],\"native_duck\":[\"%s\",\"%s\"]}",
   i?",":"",a->id,b->id,a->feet[0],a->feet[1],a->feet[2]+ngOrigin,b->feet[0],b->feet[1],b->feet[2]+ngOrigin,
   r->direct,r->before[0],r->before[1],r->after[0],r->after[1],r->final[0],r->final[1],
   r->accepted?"true":"false",r->crouch?"true":"false",r->traces,r->reason,r->stand[0],r->stand[1],r->duck[0],r->duck[1]));
 }
 NG_Text(va("],\"final_routes_valid\":%s,\"prefilter\":{\"method\":\"negative_torso_height_point_trace\",\"checked\":%d,\"rejected\":%d,\"budget\":%d,\"native_walk_proof\":false,\"cases_truncated\":%s,\"cases\":[",
  ngLocalGraphValid?"true":"false",ngLocalPrefilterChecked,ngLocalPrefilterRejected,NG_LOCAL_MAX_PREFILTERS,
  ngLocalPrefilterRejected>NG_LOCAL_PREFILTER_REPORT?"true":"false"));
 for(i=0;i<ngLocalPrefilterRejected&&i<NG_LOCAL_PREFILTER_REPORT;i++) {
  ngLocalPair *pair=&ngLocalPairs[ngLocalPrefilterPairs[i]];
  ngNode *a=&ngNodes[pair->from],*b=&ngNodes[pair->to];
  NG_Text(va("%s{\"from\":%d,\"to\":%d,\"from_position\":[%.4f,%.4f,%.4f],\"to_position\":[%.4f,%.4f,%.4f],\"reason\":\"blocked_centerline_prefilter\",\"full_walk_checked\":false}",
   i?",":"",a->id,b->id,a->feet[0],a->feet[1],a->feet[2]+ngOrigin,
   b->feet[0],b->feet[1],b->feet[2]+ngOrigin));
 }
 NG_Text("]}},");
 NG_ExternalJunctionReport();
}
#define NG_JUNCTION_MAX 16
#define NG_JUNCTION_CASES 128
#define NG_JUNCTION_TRACE_BUDGET 20000
typedef struct {int edges[2],nodes[4],junction;vec3_t point;float ratio;qboolean accepted,crouch,reused;const char *reason;} ngJunctionCase;
static ngJunctionCase ngJunctionCases[NG_JUNCTION_CASES];
static int ngJunctionCount,ngJunctionChecked,ngJunctionTraces;
static const char *ngJunctionStop;
static void NG_ExternalJunctionsReset(void) {
 ngJunctionCount=ngJunctionChecked=ngJunctionTraces=0;ngJunctionStop="not_started";
 memset(ngJunctionCases,0,sizeof(ngJunctionCases));
}
static qboolean NG_ExternalJunctionEdge(int edge) {
 ngEdge *e=&ngEdges[edge];
 if(e->from<0||e->to<0||e->from>=ngCount||e->to>=ngCount)return qfalse;
 if(!NG_SparseOrdinary(edge)||ngNodes[e->from].flags||ngNodes[e->to].flags||
    ngNodes[e->from].packed||ngNodes[e->to].packed)return qfalse;
 /* An ordinary floor anchor may participate, but the dedicated action
  * recipe and its explicitly proven floor-continuation leg stay intact. */
 return e->reason&&!strstr(e->reason,"ladder")&&!strstr(e->reason,"jump");
}
static qboolean NG_ExternalJunctionCross(int first,int second,vec3_t point) {
 const ngEdge *a=&ngEdges[first],*b=&ngEdges[second];
 const float *p=ngNodes[a->from].feet,*q=ngNodes[a->to].feet;
 const float *r=ngNodes[b->from].feet,*s=ngNodes[b->to].feet;
 float ux=q[0]-p[0],uy=q[1]-p[1],vx=s[0]-r[0],vy=s[1]-r[1],den=ux*vy-uy*vx,t,w,z1,z2;
 if(a->from==b->from||a->from==b->to||a->to==b->from||a->to==b->to||fabs(den)<.01f)return qfalse;
 t=((r[0]-p[0])*vy-(r[1]-p[1])*vx)/den;w=((r[0]-p[0])*uy-(r[1]-p[1])*ux)/den;
 if(t<=.00001f||t>=.99999f||w<=.00001f||w>=.99999f)return qfalse;
 z1=p[2]+t*(q[2]-p[2]);z2=r[2]+w*(s[2]-r[2]);
 /* Overlapping projections on different storeys or stair elevations are
  * not junctions. A later native floor check must agree with this height. */
 if(fabs(z1-z2)>1.f)return qfalse;
 VectorSet(point,p[0]+t*ux,p[1]+t*uy,(z1+z2)*.5f);return qtrue;
}
static qboolean NG_ExternalJunctionPromises(void) {
 int i;
 for(i=0;i<ngLocalChecked;i++)if(ngLocalCases[i].accepted) {
  const ngLocalCase *r=&ngLocalCases[i];float limit=r->direct*1.25f+.01f;
  NG_LocalDistances(r->from,qfalse,ngLocalDistances[0]);
  if(ngLocalDistances[0][r->to]>limit)return qfalse;
  NG_LocalDistances(r->to,qfalse,ngLocalDistances[0]);
  if(ngLocalDistances[0][r->from]>limit)return qfalse;
 }
 return NG_TrSeedSpokesValid(qfalse);
}
/* Validate all legs before touching the graph, then verify every existing
 * shortcut promise on the proposed graph. A failed transaction restores
 * every edge and node count. Existing vertices/owner proofs never move. */
static const char *NG_ExternalJunctionTry(ngJunctionCase *result,int reuse,int limit,int *maxid) {
 int originals[4],oldCount=0,legFrom[8],legTo[8],legIndex[8],legCount=0;
 int savedIndex[12],savedCount=0,previousCount=ngCount,previousEdges=ngEdgeCount;
 ngEdge saved[12];ngNode junction;int j=reuse>=0?reuse:ngCount,i,k,n,needed=0;
 qboolean duck=result->crouch;float height=duck?ngDuck:ngStand,ratio=1.f;
 if(reuse<0&&(ngCount>=NG_NODES||*maxid==0x7fffffff))return "junction_node_storage";
 if(ngTraces+5>limit)return "junction_trace_budget";
 memset(&junction,0,sizeof(junction));junction.id=reuse>=0?ngNodes[reuse].id:*maxid+1;
 junction.index=-1;junction.crouch=duck;
 if(reuse>=0)VectorCopy(ngNodes[reuse].feet,junction.raw);else VectorCopy(result->point,junction.raw);
 junction.raw[2]+=ngOrigin;NG_Node(&junction);
 if(!junction.accepted||(!duck&&junction.crouch))return "junction_floor_or_clearance_rejected";
 if(fabs(junction.feet[2]-result->point[2])>1.f)return "junction_native_floor_height_mismatch";
 if(reuse>=0) {
  vec3_t gap;VectorSubtract(junction.feet,ngNodes[reuse].feet,gap);
  if(DotProduct(gap,gap)>.0625f)return "junction_reused_floor_changed";
  junction=ngNodes[reuse];
 }else {
  for(i=0;i<ngCount;i++)if(ngNodes[i].accepted) {
   vec3_t gap;float minimum=ngNodes[i].navFlags?(ngNodes[i].radius>0.f?ngNodes[i].radius:8.f)+1.25f:2.25f;
   VectorSubtract(junction.feet,ngNodes[i].feet,gap);
   if(DotProduct(gap,gap)<minimum*minimum)return "junction_too_close_to_existing_arrival";
  }
  junction.radius=4.f;junction.topologyProtected=qtrue;junction.reason="external_native_crossing_junction";
 }
 for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted) {
  ngEdge *e=&ngEdges[k];qboolean relevant=qfalse;
  for(i=0;i<2;i++) {
   const ngEdge *original=&ngEdges[result->edges[i]];
   if((e->from==original->from&&e->to==original->to)||(e->from==original->to&&e->to==original->from))relevant=qtrue;
  }
  if(!relevant)continue;
  if(oldCount>=4)return "junction_duplicate_original_edge";
  if(!NG_ExternalJunctionEdge(k)||NG_SparseDuck(k)!=duck)return "junction_reverse_action_or_stance_mismatch";
  originals[oldCount++]=k;
 }
 for(i=0;i<oldCount;i++) {
  const ngEdge *e=&ngEdges[originals[i]];vec3_t d;float originalLength,splitLength;int ends[3];
  VectorSubtract(ngNodes[e->from].feet,ngNodes[e->to].feet,d);originalLength=VectorLength(d);
  VectorSubtract(ngNodes[e->from].feet,junction.feet,d);splitLength=VectorLength(d);
  VectorSubtract(junction.feet,ngNodes[e->to].feet,d);splitLength+=VectorLength(d);
  if(originalLength<.01f||splitLength>originalLength*1.01f+.01f)return "junction_replacement_length_exceeded";
  if(splitLength/originalLength>ratio)ratio=splitLength/originalLength;
  ends[0]=e->from;ends[1]=j;ends[2]=e->to;
  for(k=0;k<2;k++)if(ends[k]!=ends[k+1]) {
   for(n=0;n<legCount;n++)if(legFrom[n]==ends[k]&&legTo[n]==ends[k+1])break;
   if(n<legCount)continue;
   legFrom[legCount]=ends[k];legTo[legCount]=ends[k+1];legIndex[legCount++]=-1;
  }
 }
 for(i=0;i<legCount;i++) {
  const ngNode *a=legFrom[i]==j?&junction:&ngNodes[legFrom[i]],*b=legTo[i]==j?&junction:&ngNodes[legTo[i]];
  vec3_t delta;int worstTraces;VectorSubtract(a->feet,b->feet,delta);
  /* NG_Walk uses at most six traces per eight-unit step. Reserve the whole
   * call before entering it so the optional pass cannot exceed its budget. */
  worstTraces=6*(int)ceil(VectorLength(delta)/8.f);if(worstTraces<6)worstTraces=6;
  if(ngTraces+worstTraces>limit)return "junction_trace_budget";
  if(NG_Walk(a,b,height))return "junction_directed_hull_walk_rejected";
  for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].from==legFrom[i]&&ngEdges[k].to==legTo[i]) {
   if((ngEdges[k].reason&&!strcmp(ngEdges[k].reason,"special_link_requires_review"))||
      (ngEdges[k].accepted&&(!NG_ExternalJunctionEdge(k)||NG_SparseDuck(k)!=duck)))return "junction_existing_leg_action_or_stance_mismatch";
   legIndex[i]=k;break;
  }
  if(legIndex[i]<0)needed++;
 }
 if(ngEdgeCount+needed>NG_EDGES)return "junction_edge_storage";
 for(i=0;i<oldCount+legCount;i++) {
  int index=i<oldCount?originals[i]:legIndex[i-oldCount];
  if(index<0)continue;
  for(k=0;k<savedCount&&savedIndex[k]!=index;k++);
  if(k==savedCount){savedIndex[savedCount]=index;saved[savedCount++]=ngEdges[index];}
 }
 if(reuse<0){ngNodes[j]=junction;ngCount++;}
 for(i=0;i<oldCount;i++) {
  ngEdge *e=&ngEdges[originals[i]];
  e->accepted=qfalse;e->reason="external_crossing_replaced_by_native_junction";
  NG_TrSetAccepted(e->from,e->to,qfalse);
 }
 for(i=0;i<legCount;i++) {
  ngEdge *e;
  if(legIndex[i]<0){legIndex[i]=ngEdgeCount++;e=&ngEdges[legIndex[i]];memset(e,0,sizeof(*e));e->added=qtrue;}
  else e=&ngEdges[legIndex[i]];
  e->from=legFrom[i];e->to=legTo[i];e->accepted=qtrue;e->crouch=duck;e->reason="external_native_junction_walk";
  NG_TrSetAccepted(e->from,e->to,qtrue);
 }
 NG_LocalLinks();
 if(!NG_ExternalJunctionPromises()) {
  for(i=0;i<legCount;i++)NG_TrSetAccepted(legFrom[i],legTo[i],qfalse);
  for(i=0;i<savedCount;i++) {
   ngEdge *e=&ngEdges[savedIndex[i]];*e=saved[i];
   if(e->accepted)NG_TrSetAccepted(e->from,e->to,qtrue);
  }
  ngCount=previousCount;ngEdgeCount=previousEdges;
  NG_LocalLinks();return "junction_shortcut_promise_rollback";
 }
 if(reuse<0){(*maxid)++;ngGeometrySeeds++;}
 ngLocalFinalized=qfalse;result->junction=j;result->reused=reuse>=0;result->ratio=ratio;
 return NULL;
}
static void NG_ExternalJunctions(void) {
 int i,k,maxid=0,start=ngTraces,limit=ngTraces+NG_JUNCTION_TRACE_BUDGET;qboolean changed;
 NG_ExternalJunctionsReset();ngJunctionStop="no_remaining_proven_crossing";
 if(limit>NG_TRACE_BUDGET-4000)limit=NG_TRACE_BUDGET-4000;
 for(i=0;i<ngCount;i++)if(ngNodes[i].id>maxid)maxid=ngNodes[i].id;
 do {
  changed=qfalse;
  for(i=0;i<ngEdgeCount&&!changed;i++)if(NG_ExternalJunctionEdge(i))
   for(k=i+1;k<ngEdgeCount&&!changed;k++)if(NG_ExternalJunctionEdge(k)) {
    vec3_t point;ngJunctionCase *result;const char *why;int n,reuse=-1;float best=64.f;
    if(!NG_ExternalJunctionCross(i,k,point))continue;
    for(n=0;n<ngJunctionChecked;n++)if(ngJunctionCases[n].edges[0]==i&&ngJunctionCases[n].edges[1]==k)break;
    if(n<ngJunctionChecked)continue;
    if(ngJunctionCount>=NG_JUNCTION_MAX){ngJunctionStop="junction_count_limit";goto junction_done;}
    if(ngJunctionChecked>=NG_JUNCTION_CASES){ngJunctionStop="junction_candidate_limit";goto junction_done;}
    if(ngTraces+5>limit){ngJunctionStop="junction_trace_budget";goto junction_done;}
    result=&ngJunctionCases[ngJunctionChecked++];result->edges[0]=i;result->edges[1]=k;result->junction=-1;
    result->nodes[0]=ngEdges[i].from;result->nodes[1]=ngEdges[i].to;result->nodes[2]=ngEdges[k].from;result->nodes[3]=ngEdges[k].to;
    VectorCopy(point,result->point);result->crouch=NG_SparseDuck(i);
    if(result->crouch!=NG_SparseDuck(k)){result->reason="junction_stance_mismatch";continue;}
    for(n=0;n<ngCount;n++)if(NG_LocalNode(n)&&ngNodes[n].crouch==result->crouch) {
     vec3_t delta;float distance;VectorSubtract(ngNodes[n].feet,point,delta);distance=DotProduct(delta,delta);
     if(distance<best){best=distance;reuse=n;}
    }
    why=reuse>=0?NG_ExternalJunctionTry(result,reuse,limit,&maxid):"no_nearby_reuse";
    if(why)why=NG_ExternalJunctionTry(result,-1,limit,&maxid);
    result->accepted=why==NULL;result->reason=why?why:"native_floor_and_all_directed_legs_accepted";
    if(!why){ngJunctionCount++;changed=qtrue;}
    G_Printf("navgen junction: %d->%d x %d->%d at (%.3f %.3f %.3f) %s junction=%d reused=%d\n",
     ngNodes[result->nodes[0]].id,ngNodes[result->nodes[1]].id,ngNodes[result->nodes[2]].id,ngNodes[result->nodes[3]].id,
     point[0],point[1],point[2]+ngOrigin,result->reason,result->junction>=0?ngNodes[result->junction].id:-1,result->reused);
    if(why&&!strcmp(why,"junction_trace_budget")){ngJunctionStop=why;goto junction_done;}
   }
 }while(changed);
junction_done:
 ngJunctionTraces=ngTraces-start;
 G_Printf("navgen junctions: added=%d checked=%d traces=%d/%d stop=%s\n",ngJunctionCount,ngJunctionChecked,ngJunctionTraces,NG_JUNCTION_TRACE_BUDGET,ngJunctionStop);
}
static void NG_ExternalJunctionReport(void) {
 int i;
 NG_Text(va("\"native_junctions\":{\"accepted\":%d,\"checked\":%d,\"traces\":%d,\"trace_budget\":%d,\"junction_limit\":%d,\"max_height_difference\":1,\"max_replacement_ratio\":1.01,\"stop_reason\":\"%s\",\"cases\":[",
  ngJunctionCount,ngJunctionChecked,ngJunctionTraces,NG_JUNCTION_TRACE_BUDGET,NG_JUNCTION_MAX,ngJunctionStop));
 for(i=0;i<ngJunctionChecked;i++) {
  const ngJunctionCase *r=&ngJunctionCases[i];
  NG_Text(va("%s{\"nodes\":[%d,%d,%d,%d],\"position\":[%.4f,%.4f,%.4f],\"accepted\":%s,\"crouch\":%s,\"junction\":%d,\"reused\":%s,\"replacement_ratio\":%.6f,\"reason\":\"%s\"}",
   i?",":"",ngNodes[r->nodes[0]].id,ngNodes[r->nodes[1]].id,ngNodes[r->nodes[2]].id,ngNodes[r->nodes[3]].id,
   r->point[0],r->point[1],r->point[2]+ngOrigin,r->accepted?"true":"false",r->crouch?"true":"false",
   r->junction>=0?ngNodes[r->junction].id:-1,r->reused?"true":"false",r->ratio,r->reason));
 }
 NG_Text("]},");
}
/* Contraction may stop early when its optional phase budget is exhausted;
 * that only leaves extra vertices. Never publish after exhausting the hard
 * native trace budget or losing a retained owner's directed walk proofs. */
static qboolean NG_ExternalFinalGraph(void) {
 unsigned char hasExit[NG_NODES]={0};int i,missingExit=0;
 if(ngTrCacheFailed) {
  G_Printf("navgen final graph: exact pair cache capacity exhausted; WAY withheld\n");return qfalse;
 }
 for(i=0;i<ngExternalSuppliedCount;i++)if(ngNodes[i].authored) {
  ngNode *n=&ngNodes[i];
  if(!n->accepted||n->crouch!=((n->editFlags&64)!=0)) {
   G_Printf("navgen editor: point %d rejected or needs CROUCH; no edited WAY published\n",n->id);return qfalse;
  }
 }
 for(i=0;i<ngExternalSuppliedEdges;i++)if(ngNodes[ngEdges[i].from].authored&&!ngEdges[i].accepted) {
  G_Printf("navgen editor: connection %d -> %d rejected: %s; no edited WAY published\n",ngNodes[ngEdges[i].from].id,ngNodes[ngEdges[i].to].id,ngEdges[i].reason);return qfalse;
 }
 if(!ngExternalInputComplete) {
  G_Printf("navgen final graph: supplied input validation incomplete; WAY withheld\n");return qfalse;
 }
 if(ngTraces>=NG_TRACE_BUDGET) {
  G_Printf("navgen final graph: hard trace budget exhausted (%d/%d)\n",ngTraces,NG_TRACE_BUDGET);
  return qfalse;
 }
 if(!NG_LocalFinalize()) {
  G_Printf("navgen final graph: local shortcut preservation failed\n");return qfalse;
 }
 if(!NG_TrSeedSpokesValid(qtrue))return qfalse;
 for(i=0;i<ngCount;i++)if(!strcmp(ngNodes[i].reason,"contracted_to_validated_walk_neighbor")) {
  int owner=ngSimpleOwner[i];vec3_t delta;
  if(owner<0||owner>=ngCount||!ngNodes[owner].accepted) {
   G_Printf("navgen final graph: contracted node=%d has invalid owner index=%d, nodes=%d\n",
    ngNodes[i].id,owner,ngCount);return qfalse;
  }
  VectorSubtract(ngNodes[i].feet,ngNodes[owner].feet,delta);
  if(DotProduct(delta,delta)>224.f*224.f+.01f||
     NG_SimpleCached(i,owner)!=1||NG_SimpleCached(owner,i)!=1) {
   G_Printf("navgen final graph: contracted node=%d owner=%d distance=%.3f limit=224 proofs=[%d,%d]\n",
    ngNodes[i].id,ngNodes[owner].id,VectorLength(delta),NG_SimpleCached(i,owner),NG_SimpleCached(owner,i));
   return qfalse;
  }
 }
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted) {
  int from=ngEdges[i].from,to=ngEdges[i].to;
  if(from<0||to<0||from>=ngCount||to>=ngCount||from==to||
     !ngNodes[from].accepted||!ngNodes[to].accepted) {
   G_Printf("navgen final graph: accepted edge=%d has invalid endpoints indices=%d -> %d, nodes=%d\n",
    i,from,to,ngCount);return qfalse;
  }
  hasExit[from]=1;
 }
 /* A standing sample can be a valid dead end with its return edge. It is
  * not a usable bot destination if no accepted exit exists at all. Keep
  * the source and detailed report for repair instead of deleting coverage
  * merely to make the published graph pass. */
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted&&!ngNodes[i].navFlags&&!hasExit[i]) {
  if(missingExit++<16)G_Printf("navgen final graph: ordinary node=%d has no validated outgoing route; WAY withheld, sample retained for repair\n",ngNodes[i].id);
 }
 if(missingExit){G_Printf("navgen final graph: %d ordinary samples without an exit route\n",missingExit);return qfalse;}
 return qtrue;
}
/* A Recast stair ramp can differ from the actual supporting tread. A larger
 * projection is not generally safe (it could be another floor). Recover only
 * the already NG_Node-validated tread when two distinct supplied ordinary
 * neighbors independently prove complete bidirectional native step routes. */
static void NG_ExternalStairFloors(void) {
 int i,k,begin=ngTraces,limit=ngTraces+50000,repaired=0;
 if(limit>NG_TRACE_BUDGET-550000)limit=NG_TRACE_BUDGET-550000;
 for(i=0;i<ngCount&&ngTraces<limit-8000;i++) {
  ngNode *node=&ngNodes[i];unsigned char tried[NG_NODES]={0};
  int neighbors[2],found=0; qboolean neighborCrouch[2],crouch=node->crouch;
  float delta=node->feet[2]+ngOrigin-node->raw[2];
  if(node->accepted||strcmp(node->reason,"external_floor_height_mismatch")||fabs(delta)>32.f)continue;
  for(k=0;k<ngEdgeCount&&found<2&&ngTraces<limit-8000;k++) {
   int other=-1;ngNode a,b;
   if(ngEdges[k].from==i)other=ngEdges[k].to;
   else if(ngEdges[k].to==i)other=ngEdges[k].from;
   if(other<0||other==i||tried[other]||!NG_ExternalOrdinary(other))continue;
   tried[other]=1;
   if(found) {
    vec3_t separation;VectorSubtract(ngNodes[other].feet,ngNodes[neighbors[0]].feet,separation);
    if(DotProduct(separation,separation)<1.f)continue;
   }
   a=*node;b=ngNodes[other];
   if(!NG_BothWays(&a,&b))continue;
   neighbors[found]=other;neighborCrouch[found]=b.crouch;found++;crouch|=a.crouch;
  }
  if(found!=2)continue;
  node->accepted=qtrue;node->crouch=crouch;node->reason="external_stair_floor_reprojected";
  /* Preserve this measured tread through simplification; raw retains the
   * original Recast sample so the vertical correction remains auditable. */
  node->topologyProtected=qtrue;ngGeometrySeeds++;repaired++;
  for(k=0;k<2;k++)ngNodes[neighbors[k]].crouch|=neighborCrouch[k];
  G_Printf("navgen stair floor: node=%d delta_z=%.4f origin=(%.4f %.4f %.4f) neighbors=%d,%d native_bothways=2\n",
   node->id,delta,node->feet[0],node->feet[1],node->feet[2]+ngOrigin,
   ngNodes[neighbors[0]].id,ngNodes[neighbors[1]].id);
 }
 if(repaired)G_Printf("navgen stair floors: repaired=%d traces=%d\n",repaired,ngTraces-begin);
}
/* The action scanner samples extra floor anchors while searching. A manual
 * draft must not inherit ordinary fill-in vertices from failed searches. */
static void NG_ExternalDiscardUnusedActionSamples(int suppliedCount) {
 unsigned char keep[NG_NODES];int i,k;
 memset(keep,0,sizeof(keep));
 for(i=0;i<ngLadderProofCount;i++) {
  const ngLadderProof *p=&ngLadderProofs[i];
  keep[p->low]=keep[p->high]=1;
  for(k=0;k<4;k++)keep[p->node[k]]=1;
  if(p->ascentRelease>=0)keep[p->ascentRelease]=1;
  if(p->ascentAlignment>=0)keep[p->ascentAlignment]=1;
 }
 for(i=0;i<ngJumpProofCount;i++) {
  const ngJumpProof *p=&ngJumpProofs[i];keep[p->approach]=keep[p->launch]=keep[p->landing]=1;
 }
 for(i=0;i<ngDropProofCount;i++) {
  const ngDropProof *p=&ngDropProofs[i];keep[p->approach]=keep[p->launch]=keep[p->landing]=keep[p->pairedJumpLaunch]=1;
 }
 for(i=0;i<ngFreeClimbProofCount;i++) {
  const ngFreeClimbProof *p=&ngFreeClimbProofs[i];keep[p->approach]=keep[p->launch]=keep[p->landing]=1;
 }
 for(i=0;i<ngCount;i++)if(ngNodes[i].door.mode&&ngNodes[i].door.validated){
  int target=NG_Find(ngNodes[i].door.target),use=NG_Find(ngNodes[i].door.useSource),entry=NG_Find(ngNodes[i].door.entry);
  keep[i]=1;if(target>=0)keep[target]=1;if(use>=0)keep[use]=1;if(entry>=0)keep[entry]=1;
 }
 /* Only explicitly bound, fully checked door approach chains survive the
  * temporary action sampler. A failed discovery must leave no floor islands. */
 for(i=suppliedCount;i<ngCount;i++)if(NG_DoorApproachGuide(i))keep[i]=1;
 for(i=suppliedCount;i<ngCount;i++)if(ngNodes[i].accepted&&!keep[i]) {
  ngNodes[i].accepted=qfalse;ngNodes[i].index=-1;ngNodes[i].reason="unused_action_search_floor";
 }
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted&&
     (!ngNodes[ngEdges[i].from].accepted||!ngNodes[ngEdges[i].to].accepted)) {
  ngEdges[i].accepted=qfalse;ngEdges[i].reason="unused_action_search_endpoint";
 }
}
/* Capture native-valid but disconnected regions before quarantine changes
 * acceptance. Weak grouping is diagnostic only, never a route proof. */
static void NG_ExternalMissingRegions(void) {
 int parent[NG_NODES],number[NG_NODES],i;
 for(i=0;i<ngCount;i++){parent[i]=i;number[i]=-1;ngExternalMissingRegion[i]=-1;}
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted) {
  int a=ngEdges[i].from,b=ngEdges[i].to,ra,rb;
  if(a<0||b<0||!ngNodes[a].accepted||!ngNodes[b].accepted||ngSpawnReachable[a]||ngSpawnReachable[b])continue;
  ra=NG_PortalRoot(parent,a);rb=NG_PortalRoot(parent,b);if(ra!=rb)parent[rb]=ra;
 }
 ngExternalMissingRegionCount=0;
 for(i=0;i<ngExternalSuppliedCount;i++)if(ngExternalFloorAccepted[i]&&ngNodes[i].accepted&&!ngSpawnReachable[i]) {
  int root=NG_PortalRoot(parent,i);
  if(number[root]<0)number[root]=ngExternalMissingRegionCount++;
 }
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted&&!ngSpawnReachable[i])ngExternalMissingRegion[i]=number[NG_PortalRoot(parent,i)];
}
static void NG_ExternalInputNodeReport(int index) {
 int incoming=0,outgoing=0,rejected=0,finalIncoming=0,finalOutgoing=0,k;const char *status;
 if(!ngExternalImport||index>=ngExternalSuppliedCount)return;
 if(ngNodes[index].authored)NG_Text(va(",\"editor_role\":%d,\"editor_drop_target\":%d",ngNodes[index].editRole,ngNodes[index].editDropId));
 for(k=0;k<ngExternalSuppliedEdges;k++) {
  const ngEdge *e=&ngEdges[k];if(e->from!=index&&e->to!=index)continue;
  if(!ngExternalEdgeAccepted[k]){rejected++;continue;}
  if(e->from==index)outgoing++;if(e->to==index)incoming++;
 }
 for(k=0;k<ngEdgeCount;k++)if(ngEdges[k].accepted){if(ngEdges[k].from==index)finalOutgoing++;if(ngEdges[k].to==index)finalIncoming++;}
 if(!ngExternalFloorAccepted[index])status="native_floor_rejected";
 else if(ngNodes[index].accepted&&ngSpawnReachable[index])status="spawn_reachable";
 else if(ngExternalMissingRegion[index]>=0)status="missing_spawn_route";
 else if(!strcmp(ngNodes[index].reason,"contracted_to_validated_walk_neighbor"))status="coverage_witness_contracted";
 else status="excluded_after_native_floor_validation";
 NG_Text(va(",\"native_input\":{\"floor_accepted\":%s,\"floor_reason\":\"%s\",\"initial_incoming\":%d,\"initial_outgoing\":%d,\"initial_rejected_incident_links\":%d,\"final_incoming\":%d,\"final_outgoing\":%d,\"direct_endpoint_attempts\":%d,\"detour_searches\":%d,\"missing_region\":%d,\"status\":\"%s\"}",
  ngExternalFloorAccepted[index]?"true":"false",ngExternalFloorReason[index]?ngExternalFloorReason[index]:"not_evaluated",incoming,outgoing,rejected,finalIncoming,finalOutgoing,ngExternalDirectAttempts[index],ngExternalDetourAttempts[index],ngExternalMissingRegion[index],status));
}
static void NG_ExternalCoverageReport(void) {
 unsigned char hasExit[NG_NODES]={0};
 const char *reasons[64];int counts[64],reasonCount=0;
 int i,k,g,valid=0,exported=0,reachable=0,missing=0,contracted=0,links=0,direct=0,searches=0;
 NG_Text(va("\"trace_budget_policy\":{\"model\":\"external_input_count_plus_postprocessing_v1\",\"base\":%d,\"hard_cap\":%d,\"per_input_node\":%d,\"per_directed_input_link\":%d,\"input_estimate\":%d,\"post_input_allowance\":%d,\"remaining_after_input\":%d,\"phase_limits_scaled\":false,\"estimate_is_validation_proof\":false},",
  NG_TRACE_BASE_BUDGET,NG_TRACE_MAX_BUDGET,NG_TRACE_NODE_ALLOWANCE,NG_TRACE_EDGE_ALLOWANCE,
  ngExternalSuppliedCount*NG_TRACE_NODE_ALLOWANCE+ngExternalSuppliedEdges*NG_TRACE_EDGE_ALLOWANCE,
  NG_TRACE_BASE_BUDGET,NG_TRACE_BUDGET-ngExternalInputTraces));
 for(i=0;i<ngExternalSuppliedCount;i++) {
  const char *reason=ngExternalFloorReason[i]?ngExternalFloorReason[i]:"not_evaluated";
  valid+=ngExternalFloorAccepted[i]!=0;exported+=ngNodes[i].accepted!=0;
  reachable+=ngNodes[i].accepted&&ngSpawnReachable[i];missing+=ngExternalMissingRegion[i]>=0;
  contracted+=!strcmp(ngNodes[i].reason,"contracted_to_validated_walk_neighbor");
  if(ngExternalFloorAccepted[i])continue;
  for(k=0;k<reasonCount&&strcmp(reasons[k],reason);k++);
  if(k==reasonCount&&reasonCount<64){reasons[k]=reason;counts[k]=0;reasonCount++;}
  if(k<64)counts[k]++;
 }
 for(i=0;i<ngExternalSuppliedEdges;i++)links+=ngExternalEdgeAccepted[i]!=0;
 for(i=0;i<ngCount;i++){direct+=ngExternalDirectAttempts[i];searches+=ngExternalDetourAttempts[i];}
 NG_Text(va("\"native_input_coverage\":{\"full_map_verified\":false,\"scope\":\"supplied_spatial_samples_only\",\"input_nodes\":%d,\"native_floor_accepted\":%d,\"native_floor_rejected\":%d,\"exported_input_nodes\":%d,\"spawn_reachable_input_nodes\":%d,\"connectivity_missing_input_nodes\":%d,\"contracted_input_nodes\":%d,\"initial_directed_links\":%d,\"initial_accepted_links\":%d,\"initial_rejected_links\":%d,\"geometry_rejections\":[",ngExternalSuppliedCount,valid,ngExternalSuppliedCount-valid,exported,reachable,missing,contracted,ngExternalSuppliedEdges,links,ngExternalSuppliedEdges-links));
 for(k=0;k<reasonCount;k++)NG_Text(va("%s{\"reason\":\"%s\",\"count\":%d}",k?",":"",reasons[k],counts[k]));
 reasonCount=0;
 for(i=0;i<ngExternalSuppliedEdges;i++)if(!ngExternalEdgeAccepted[i]) {
  const char *reason=ngExternalEdgeReason[i]?ngExternalEdgeReason[i]:"not_evaluated";
  for(k=0;k<reasonCount&&strcmp(reasons[k],reason);k++);
  if(k==reasonCount&&reasonCount<64){reasons[k]=reason;counts[k]=0;reasonCount++;}
  if(k<64)counts[k]++;
 }
 NG_Text(va("],\"capacity\":{\"native_nodes\":%d,\"native_directed_links\":%d,\"input_node_limit\":%d,\"input_link_limit\":%d,\"input_bytes_limit\":%d,\"input_validation_traces\":%d,\"input_validation_complete\":%s,\"input_truncated\":false},\"link_rejections\":[",NG_NODES,NG_EDGES,ngExternalManualDraft||ngExternalSpatialImport?NG_EXTERNAL_NODE_LIMIT:NG_EXTERNAL_LEGACY_NODE_LIMIT,ngExternalManualDraft||ngExternalSpatialImport?NG_EXTERNAL_EDGE_LIMIT:NG_EXTERNAL_LEGACY_EDGE_LIMIT,NG_EXTERNAL_INPUT,ngExternalInputTraces,ngExternalInputComplete?"true":"false"));
 for(k=0;k<reasonCount;k++)NG_Text(va("%s{\"reason\":\"%s\",\"count\":%d}",k?",":"",reasons[k],counts[k]));
 NG_Text(va("],\"repair\":{\"direct_endpoint_attempts\":%d,\"detour_searches\":%d,\"stop_reason\":\"%s\",\"search_xy_extent\":384,\"search_z_extent\":%d,\"coarse_stride\":%d,\"fine_stride\":8,\"selection_model\":\"%s\",\"selection_is_route_proof\":false,\"detour_selection\":[",direct,searches,ngPortalStop,ngExternalSpatialImport?128:64,ngExternalSpatialImport?32:8,ngExternalSpatialImport?"spawn_then_attempts_then_distance64_over_sqrt_component128_v1":"legacy_component_frontier"));
 for(i=0;i<ngExternalDetourCaseCount;i++) {
  ngExternalDetourCase *entry=&ngExternalDetourCases[i];
  NG_Text(va("%s{\"seed\":%d,\"target\":%d,\"component_nodes\":%d,\"previous_attempts\":%d,\"spawn_rank\":%d,\"distance\":%.4f,\"score\":%.4f,\"after_actions\":%s,\"connected\":%s,\"traces\":%d}",i?",":"",entry->seed,entry->target,entry->componentNodes,entry->previousAttempts,entry->spawnRank,entry->distance,entry->score,entry->afterActions?"true":"false",entry->connected?"true":"false",entry->traces));
 }
 NG_Text("]},\"missing_regions\":[");
 for(g=0;g<ngExternalMissingRegionCount;g++) {
  vec3_t lo,hi;float nearestDistance=1e30f;int count=0,nearest=-1,seed=-1,da=0,sa=0;
  VectorSet(lo,1e30f,1e30f,1e30f);VectorSet(hi,-1e30f,-1e30f,-1e30f);
  for(i=0;i<ngCount;i++)if(ngExternalMissingRegion[i]==g) {
   da+=ngExternalDirectAttempts[i];sa+=ngExternalDetourAttempts[i];
   if(i>=ngExternalSuppliedCount)continue;
   count++;
   for(k=0;k<3;k++){float value=ngNodes[i].feet[k]+(k==2?ngOrigin:0);if(value<lo[k])lo[k]=value;if(value>hi[k])hi[k]=value;}
   for(k=0;k<ngCount;k++)if(ngNodes[k].accepted&&ngSpawnReachable[k]) {
    vec3_t delta;float distance;VectorSubtract(ngNodes[i].feet,ngNodes[k].feet,delta);distance=DotProduct(delta,delta);
    if(distance<nearestDistance){nearestDistance=distance;nearest=k;seed=i;}
   }
  }
  NG_Text(va("%s{\"id\":%d,\"grouping\":\"weak_native_links_before_quarantine\",\"input_nodes\":%d,\"bounds\":[[%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f]],\"direct_endpoint_attempts\":%d,\"detour_searches\":%d,\"nearest_missing_node\":%d,\"nearest_spawn_reachable_node\":%d,\"nearest_distance\":%.4f,\"nearest_is_route_proof\":false,\"node_ids\":[",g?",":"",g,count,lo[0],lo[1],lo[2],hi[0],hi[1],hi[2],da,sa,seed>=0?ngNodes[seed].id:-1,nearest>=0?ngNodes[nearest].id:-1,nearest>=0?sqrt(nearestDistance):-1.f));
  count=0;for(i=0;i<ngExternalSuppliedCount;i++)if(ngExternalMissingRegion[i]==g)NG_Text(va("%s%d",count++?",":"",ngNodes[i].id));
  NG_Text("]}");
 }
 NG_Text("],\"input_ids\":[");
 for(i=0;i<ngExternalSuppliedCount;i++)NG_Text(va("%s%d",i?",":"",ngNodes[i].id));
 NG_Text("],\"ordinary_without_exit\":[");
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted)hasExit[ngEdges[i].from]=1;
 k=0;for(i=0;i<ngCount;i++)if(ngNodes[i].accepted&&!ngNodes[i].navFlags&&!hasExit[i])NG_Text(va("%s%d",k++?",":"",ngNodes[i].id));
 NG_Text("]},");
}
/* Manual transitions: do not downgrade an action into a walking proof.
 * Ladder contact uses the same native surface test as automatic discovery. */
static qboolean NG_EditorLadderNormal(const vec3_t feet,vec3_t normal) {
 int axis,sign;
 for(axis=0;axis<2;axis++)for(sign=-1;sign<=1;sign+=2) {
  VectorClear(normal);normal[axis]=(float)sign;
  if(NG_TrAirborneLadderContact(feet,normal))return qtrue;
 }
 return qfalse;
}
/* Interior guidance is a target sphere, not a standable capsule centre.
 * Resolve it to an actual ladder-contact capsule within that same sphere. */
static qboolean NG_EditorContact(const ngNode *node,ngNode *physical,vec3_t normal) {
 int axis,sign,offset;*physical=*node;
 if(NG_EditorLadderNormal(node->feet,normal)&&NG_TrClear(node->feet,node->feet))return qtrue;
 if(!(node->editRole&16))return qfalse;
 for(axis=0;axis<2;axis++)for(sign=-1;sign<=1;sign+=2)for(offset=1;offset<=node->radius;offset++) {
  VectorCopy(node->feet,physical->feet);physical->feet[axis]+=(float)(sign*offset);
  if(NG_EditorLadderNormal(physical->feet,normal)&&NG_TrClear(physical->feet,physical->feet))return qtrue;
 }
 return qfalse;
}
static qboolean NG_EditorFacingMatchesContact(const ngNode *node,const vec3_t normal) {
 float length;
 if(!node->metadataSupplied||node->id!=node->editSourceId||!(node->navFlags&NG_NAV_CLIMB)||
    (node->editRole&128))return qtrue;
 length=VectorLength(node->facing);
 /* A null facing carries no authored directional override. A supplied
  * direction must point into the same real surface as PM_LadderMove. */
 return length<.000001f||DotProduct(node->facing,normal)<=-.9999f*length;
}
static qboolean NG_EditorLadderFacing(const ngNode *a,const ngNode *b) {
 ngNode pa,pb;vec3_t normal,other;qboolean ca,cb;
 if((!a->metadataSupplied||VectorLengthSquared(a->facing)<.000001f)&&
    (!b->metadataSupplied||VectorLengthSquared(b->facing)<.000001f))return qtrue;
 ca=NG_EditorContact(a,&pa,normal);cb=NG_EditorContact(b,&pb,other);
 if(ca&&!NG_EditorFacingMatchesContact(a,normal))return qfalse;
 if(cb&&!NG_EditorFacingMatchesContact(b,other))return qfalse;
 if(!ca&&cb&&!NG_EditorFacingMatchesContact(a,other))return qfalse;
 if(!cb&&ca&&!NG_EditorFacingMatchesContact(b,normal))return qfalse;
 return qtrue;
}
static qboolean NG_EditorLadderSegment(const ngNode *a,const ngNode *b) {
 ngNode pa,pb;vec3_t normal,other,delta,p;int steps,i;qboolean ca,cb;
 ca=NG_EditorContact(a,&pa,normal);cb=NG_EditorContact(b,&pb,other);
 if(!ca&&!cb)return qfalse;
 if(!ca)VectorCopy(other,normal);
 if(!NG_TrClear(pa.feet,pb.feet))return qfalse;
 VectorSubtract(pb.feet,pa.feet,delta);steps=(int)ceil(VectorLength(delta)/4.f);if(steps<1)steps=1;
 for(i=0;i<=steps;i++) {
  float distanceA=VectorLength(delta)*(float)i/steps,distanceB=VectorLength(delta)-distanceA;
  VectorMA(pa.feet,(float)i/steps,delta,p);
  if(NG_TrAirborneLadderContact(p,normal))continue;
  /* A raised top turning point clears the lip; only its bounded terminal
   * segment may leave contact, and must stay capsule-clear. */
  if((a->editRole&32)&&distanceA<=ngOrigin+18.f)continue;
  if((b->editRole&32)&&distanceB<=ngOrigin+18.f)continue;
  return qfalse;
 }
 return qtrue;
}
/* Keep the existing flat/lip import for jumps and drops. Only an explicit
 * new DROP guide may additionally retain its real gentle-slope body pose. */
static qboolean NG_EditorJumpLaunchFloor(ngNode *node) {
 vec3_t feet,down;trace_t tr;qboolean drop;float difference;
 drop=(node->editRole&8)&&!(node->editRole&~(1|8))&&
      !node->navFlags&&node->editDropId>=0;
 if(!node->authored||(!(node->editRole&1)&&!drop)||
    (!(node->navFlags&((1u<<27)|(1u<<18)|(1u<<8)))&&!drop)||node->crouch)return qfalse;
 if(NG_Ground(node->feet,feet,ngStand))return qfalse;
 difference=feet[2]-node->feet[2];
 if(fabs(difference)>.25f&&(!drop||node->editRole!=9||difference<0.f||difference>2.f||
    !NG_TrPhysicalGroundAlias(node->feet,feet)))return qfalse;
 VectorCopy(node->feet,down);down[2]-=.25f;
 if(!NG_Trace(&tr,node->feet,down,ngStand)||tr.startsolid||tr.allsolid||tr.fraction==1.f||
    !NG_StaticGround(tr.entityNum)||!NG_OrdinarySurface(tr.surfaceFlags,tr.plane.normal[2]))return qfalse;
 node->accepted=qtrue;node->reason=drop?"manual_drop_launch_capsule_ground":"manual_jump_launch_capsule_ground";return qtrue;
}
static const char *NG_EditorDropLaunchWalk(const ngNode *from,const ngNode *to,int endpoint,const char *why) {
 ngNode typed=endpoint?*to:*from;
 if(typed.editRole!=9||typed.navFlags||!NG_EditorJumpLaunchFloor(&typed))return "typed_drop_launch_rejected";
 /* A source physical pose already follows NG_Walk's real-body state. Only
  * the final export-height comparison needs a typed target coordinate. */
 return endpoint?NG_TrStandingPhysicalWalk(from,to,why):why;
}
/* Read-only typed alternatives to the ordinary import records. Proposed
 * feet are full-capsule support plus skin, never a rewrite of graph nodes.
 * The caller must query that literal pose again before using exact=true. */
static void NG_ProbeImportDropLaunch(const ngNode *from,const ngNode *to,const char *standWhy) {
 int endpoint;qboolean savedProbe=ngProbe;ngProbe=qfalse;
 for(endpoint=0;endpoint<2;endpoint++) {
  const ngNode *source=endpoint?to:from;ngNode candidate,literal;
  vec3_t down,error;trace_t tr;qboolean accepted=qfalse,exact=qfalse;
  const char *reason="capsule_no_support",*walkWhy;
  memset(&literal,0,sizeof(literal));literal.id=1;literal.authored=qtrue;
  literal.editRole=9;literal.editDropId=0;
  VectorCopy(source->feet,literal.feet);VectorCopy(source->feet,literal.raw);literal.raw[2]+=ngOrigin;
  candidate=literal;VectorCopy(source->feet,down);down[2]-=18.5f;
  if(!NG_Trace(&tr,source->feet,down,ngStand))reason="trace_budget";
  else if(tr.startsolid||tr.allsolid)reason="capsule_startsolid";
  else if(tr.fraction<1.f&&NG_StaticGround(tr.entityNum)&&NG_OrdinarySurface(tr.surfaceFlags,tr.plane.normal[2])) {
   VectorCopy(tr.endpos,candidate.feet);candidate.feet[2]+=.125f;
   VectorCopy(candidate.feet,candidate.raw);candidate.raw[2]+=ngOrigin;
   accepted=NG_EditorJumpLaunchFloor(&candidate);reason=accepted?"accepted":"typed_drop_launch_rejected";
   VectorSubtract(candidate.feet,source->feet,error);exact=accepted&&DotProduct(error,error)<=.0001f;
  }
  G_Printf("navgen probe action_import kind=drop_launch endpoint=%d accepted=%d exact=%d reason=%s feet=(%.5f %.5f %.5f)\n",
   endpoint,accepted,exact,reason,candidate.feet[0],candidate.feet[1],candidate.feet[2]);
  walkWhy=(!accepted||!exact)?"typed_drop_launch_not_exact":
   endpoint?NG_EditorDropLaunchWalk(from,&literal,1,standWhy):NG_EditorDropLaunchWalk(&literal,to,0,standWhy);
  G_Printf("navgen probe action_walk kind=drop_launch endpoint=%d accepted=%d reason=%s\n",
   endpoint,walkWhy==NULL,walkWhy?walkWhy:"accepted");
 }
 ngProbe=savedProbe;
}
static const char *NG_EditorEdge(ngEdge *edge) {
 ngNode *a=&ngNodes[edge->from],*b=&ngNodes[edge->to];vec3_t normal,landed;float duration,speed;
 const unsigned int jump=(1u<<27)|(1u<<18)|(1u<<8);
 if(NG_DoorConditionalEdge(edge->from,edge->to))return NG_DoorEditorEdge(edge);
 if(a->door.mode||b->door.mode){ngNode *recipe=a->door.mode?a:b;ngDoorPose pose;const char *why=NG_DoorRecipeIdentity(recipe,&pose);
  if(why)return why;
  return NG_DoorAnchorWalk(&pose,a,b)?NULL:"door_ordinary_anchor_blocked_in_closed_or_open_pose";
 }
 if((a->editRole&128)&&b->id==a->editDropId) {ngFreeClimbProof proof;
  if(!NG_TrFreeClimb(a,b,ngTraces+8000,&proof))return "manual_free_climb_recipe_rejected";
  return NG_TrCommitManualFreeClimb(edge,&proof)?NULL:"manual_free_climb_recipe_conflict_or_limit";
 }
 if((a->editRole&64)&&b->id==a->editDropId) {ngJumpProof proof;
  if(!NG_TrStandingJump(a,b,ngTraces+16000,&proof))return "manual_standing_jump_recipe_rejected";
  return NG_TrCommitManualStandingJump(edge,&proof)?NULL:"manual_standing_jump_recipe_conflict_or_budget";
 }
 if((a->navFlags&jump)&&!(a->editRole&64))return NG_TrJump(a,b)?NULL:"manual_jump_arc_rejected";
 if((a->editRole&8)&&b->id==a->editDropId) {ngDropProof proof;memset(&proof,0,sizeof(proof));
  if(!NG_TrWalkDrop(a,b,ngTraces+16000,&proof))return "manual_drop_arc_rejected";
  return NG_TrCommitManualDrop(edge,&proof)?NULL:"manual_drop_recipe_conflict_or_budget";
 }
 if((a->navFlags|b->navFlags)&NG_NAV_CLIMB) {
  if(!NG_EditorLadderFacing(a,b))return "manual_ladder_facing_conflicts_native_contact";
  if(NG_EditorLadderSegment(a,b))return NULL;
  if((b->editRole&32)&&NG_TrStableLanding(a->feet)) {
   vec3_t raised,d;int k;VectorSubtract(b->feet,a->feet,d);
   VectorCopy(a->feet,raised);raised[2]=b->feet[2];
   if(d[2]>=-.25f&&d[2]<=18.25f&&d[0]*d[0]+d[1]*d[1]<=96.f*96.f&&
      NG_TrClear(a->feet,raised)&&NG_TrClear(raised,b->feet))
    for(k=0;k<ngExternalSuppliedEdges;k++)if(ngEdges[k].from==edge->to&&
       ngNodes[ngEdges[k].to].feet[2]<b->feet[2]-32.f&&
       NG_EditorLadderSegment(b,&ngNodes[ngEdges[k].to]))return NULL;
  }
  if(NG_EditorLadderNormal(a->feet,normal)) {
   if(NG_TrContactSegment(a->feet,b->feet,normal))return NULL;
   if(NG_TrLadderDismount(a->feet,b->feet,normal,landed,&duration,&speed)) {
    vec3_t d;VectorSubtract(landed,b->feet,d);if(VectorLength(d)<=18.f)return NULL;
   }
  }
  if(NG_EditorLadderNormal(b->feet,normal)&&NG_TrClear(a->feet,b->feet)&&NG_TrStableLanding(a->feet))return NULL;
  /* Grounded approach/exit guides may legitimately just walk. */
 }
 {const char *why=NG_Walk(a,b,ngStand);
  if(why&&!strcmp(why,"different_floor")&&b->editRole==9&&!b->navFlags)
   why=NG_EditorDropLaunchWalk(a,b,1,why);
  if(why&&!a->navFlags&&!b->navFlags&&!(a->editRole&~(1|8))&&
     !(b->editRole&~1)&&b->feet[2]<a->feet[2]-18.25f) {
   ngDropProof proof;memset(&proof,0,sizeof(proof));
   /* Direction alone is not a falling proof. Preserve the authored edge,
    * but upgrade it only after all command-speed capsule/support/impact
    * samples pass and its directed WAY target can be recorded exactly. */
   if(NG_TrWalkDrop(a,b,ngTraces+16000,&proof))
    return NG_TrCommitManualDrop(edge,&proof)?NULL:"manual_drop_recipe_conflict_or_budget";
  }
  if(why){why=NG_Walk(a,b,ngDuck);edge->crouch=qtrue;}
  return why;
 }
}
#include "tce_navgen_pathrepair.inc"
static qboolean NG_ExternalImport(void) {
 char source[MAX_QPATH],output[MAX_QPATH],report[MAX_QPATH];
 int i,start,suppliedCount,actionBaseCount,count=0,accepted=0,minimumDoorId=0; qboolean ok,repairDoors=qfalse,repairPaths=qfalse;
 if(trap_Argc()==4) {
  char option[32],identity[32],*end;double number;
  trap_Argv(2,option,sizeof(option));trap_Argv(3,identity,sizeof(identity));
  number=strtod(identity,&end);
  if(end!=identity&&!*end&&NG_Finite((float)number)&&number>=0&&number<2147467263.&&floor(number)==number) {
   repairDoors=!Q_stricmp(option,"repair_doors");repairPaths=!Q_stricmp(option,"repair_paths");
   if(repairDoors||repairPaths)minimumDoorId=(int)number;
  }
 }
 if(trap_Argc()!=2&&!repairDoors&&!repairPaths) {
  G_Printf("usage: tce_navgen_import navgen/<map>_candidate.txt [repair_doors|repair_paths <next_source_id>]\n");return qtrue;
 }
 trap_Argv(1,source,sizeof(source));
 if(!NG_SafePath(source)||Q_stricmpn(source,"navgen/",7)||
    !NG_SafePath(level.rawmapname)||strchr(level.rawmapname,'/')) {
  G_Printf("navgen import: invalid map or candidate path\n");return qtrue;
 }
 start=trap_Milliseconds();NG_ExternalReset();ngExternalError="unknown";ngExternalLine=1;
 if(!NG_ExternalRead(source)) {
  G_Printf("navgen import: rejected %s at line %d: %s; no WAY written\n",source,ngExternalLine,ngExternalError);
  ngCount=ngEdgeCount=0;return qtrue;
 }
 if(repairDoors&&ngExternalVersion<5) {
  G_Printf("navgen import: repair_doors requires the current complete authored graph; no WAY written\n");return qtrue;
 }
 if(repairPaths&&ngExternalVersion<5) {
  G_Printf("navgen import: repair_paths requires a complete Format5/6 authored graph; no WAY written\n");return qtrue;
 }
 ngTraceBudget=NG_ExternalTraceBudget(ngExternalSuppliedCount,ngExternalSuppliedEdges);
 G_Printf("navgen import budget: nodes=%d links=%d total=%d cap=%d post_input_allowance=%d (input-count estimate; collision and phase limits unchanged)\n",
  ngExternalSuppliedCount,ngExternalSuppliedEdges,NG_TRACE_BUDGET,NG_TRACE_MAX_BUDGET,NG_TRACE_BASE_BUDGET);
 suppliedCount=ngCount;
 for(i=0;i<ngCount;i++) {
  ngNode *node=&ngNodes[i];float candidateZ=node->feet[2];
  if(node->authored&&(node->navFlags&NG_NAV_CLIMB)) {
   vec3_t normal;ngNode physical;
   if(NG_EditorContact(node,&physical,normal)||((node->editRole&32)&&NG_TrClear(node->feet,node->feet))) {
    node->accepted=qtrue;node->reason="manual_ladder_contact";
   } else NG_Node(node);
  } else if(!NG_EditorJumpLaunchFloor(node))NG_Node(node);
  if(node->accepted&&fabs(node->feet[2]-candidateZ)>18.f) {
   node->accepted=qfalse;node->reason="external_floor_height_mismatch";
  }
  if(node->accepted) {
   node->reason="external_native_validated_floor";ngGeometrySeeds++;
   if(ngExternalSpawn[i]&&NG_ExternalSpawnAnchor(node)) {
    node->reason="map_entity_spawn_seed";ngSpawnSeeds++;
   }
  }
 }
 if(!ngExternalManualDraft)NG_ExternalStairFloors();
 for(i=0;i<suppliedCount;i++){ngExternalFloorAccepted[i]=(unsigned char)ngNodes[i].accepted;ngExternalFloorReason[i]=ngNodes[i].reason;}
 for(i=0;i<ngEdgeCount;i++) {
  ngEdge *edge=&ngEdges[i];const char *why;
  edge->reason="rejected_endpoint";
  if(!ngNodes[edge->from].accepted||!ngNodes[edge->to].accepted)continue;
  if(NG_TrSeen(edge->from,edge->to)){edge->reason="duplicate_edge";continue;}
  NG_TrMarkSeen(edge->from,edge->to);
  why=ngNodes[edge->from].authored?NG_EditorEdge(edge):NG_Walk(&ngNodes[edge->from],&ngNodes[edge->to],ngStand);
  if(why&&!ngNodes[edge->from].authored){why=NG_Walk(&ngNodes[edge->from],&ngNodes[edge->to],ngDuck);edge->crouch=qtrue;}
  if(why){edge->reason=why;continue;}
  edge->accepted=qtrue;edge->reason=(ngNodes[edge->from].authored&&((ngNodes[edge->from].navFlags|ngNodes[edge->to].navFlags)||ngNodes[edge->from].editRole&(8|64)))?"manual_native_transition_live_pending":"external_native_validated_walk";
  NG_TrSetAccepted(edge->from,edge->to,qtrue);
  if(edge->crouch)ngNodes[edge->from].crouch=ngNodes[edge->to].crouch=qtrue;
 }
 for(i=0;i<ngExternalSuppliedEdges;i++){ngExternalEdgeAccepted[i]=(unsigned char)ngEdges[i].accepted;ngExternalEdgeReason[i]=ngEdges[i].reason;}
 ngExternalInputTraces=ngTraces;ngExternalInputComplete=ngTraces<NG_TRACE_BUDGET;
 /* A stationary body query needs two traces and can refuse at limit-1.
  * Test the explicit reason as well as the counter; otherwise that last
  * unused trace could make partially checked input appear complete. */
 for(i=0;i<suppliedCount;i++)if(ngNodes[i].reason&&!strcmp(ngNodes[i].reason,"trace_budget"))ngExternalInputComplete=qfalse;
 for(i=0;i<ngExternalSuppliedEdges;i++)if(ngEdges[i].reason&&!strcmp(ngEdges[i].reason,"trace_budget"))ngExternalInputComplete=qfalse;
 /* Mark every supplied node as evaluated, but never claim this checked all
  * space in the map. Disconnected geometry remains visible in the report. */
 memset(ngCoverageVisited,1,sizeof(ngCoverageVisited));
 if(repairPaths) {
  /* Proposal only. Keep all supplied records and rejected pairs in the
   * report; do not quarantine, simplify, reset action proofs, or publish a
   * partial WAY. The merged authored graph requires a separate full check. */
  if(!NG_DoorManualFinalize())ngExternalInputComplete=qfalse;
  NG_PathRepair(minimumDoorId);NG_ExternalMissingRegions();
  /* No arrival-region proof is performed by this proposal pass. Do not
   * attach a previous run's cached radius receipt to its unchanged nodes. */
  memset(ngArrivalProofs,0,sizeof(ngArrivalProofs));
  ngArrivalTraceStart=ngArrivalTraceLimit=ngArrivalTraces=ngArrivalBudget=0;
  ngArrivalBudgetExhausted=qfalse;
  for(i=0;i<ngCount;i++)if(ngNodes[i].accepted)ngNodes[i].index=count++;
  Com_sprintf(report,sizeof(report),"navgen/%s_navmesh.json",level.rawmapname);
  ok=NG_Report(report,source,level.rawmapname,qfalse);
  G_Printf("navgen path repair: %s new_nodes=%d chains=%d traces=%d stop=%s report=%s (PROPOSAL ONLY; no WAY written)\n",
   ok&&ngPathRepairPreserved&&ngPathRepairBudgetComplete?"complete":"FAILED",ngCount-suppliedCount,
   ngPathRepairChainCount,ngPathRepairTraces,ngPathRepairStop,report);
  return qtrue;
 }
 if(!ngExternalManualDraft)NG_ExternalRepair(qfalse);
 /* Recast proposes floor connectivity only. Discover action transitions
  * against native collision before rejecting disconnected upper floors.
  * Dedicated CLIMB/JUMP nodes survive ordinary contraction/sparsification. */
 actionBaseCount=ngCount;
 /* Format5 contains the complete authored graph, including action guides.
  * Re-discovery here restores deliberately deleted points/links and can
  * reuse their IDs. Its transitions have already passed NG_EditorEdge.
  * Automatic graphs and legacy Format3 still need action discovery. */
 if(ngExternalVersion<5){NG_Traversal();NG_Doors();}
 else {
  if(!NG_DoorManualFinalize())ngExternalInputComplete=qfalse;
  /* A normal check never restores deliberately removed action points.
   * Only the explicit repair operation may append newly proved doors. */
  if(repairDoors&&ngExternalInputComplete&&!NG_DoorRepair(minimumDoorId))ngExternalInputComplete=qfalse;
 }
 if(ngExternalManualDraft||ngExternalSpatialImport)NG_ExternalDiscardUnusedActionSamples(actionBaseCount);
 if(!ngExternalManualDraft)NG_ExternalRepair(qtrue);
 NG_SpawnReachability();NG_ExternalMissingRegions();NG_QuarantineUnreachable();NG_TrPruneIncomplete();
 /* Camp names/facing are derived metadata, including after manual edits.
  * Regenerate them without adding or restoring any walking connection. */
 NG_Camps();
 if(!ngExternalManualDraft&&!ngExternalSpatialImport) {
  NG_Simplify();NG_ExternalLocalShortcuts();NG_Sparsify();NG_TrPruneSeedSpokes();NG_ExternalJunctions();
 } else {
  /* Format3/5 preserve manual edits. Format4 preserves every room sample
   * and all actions; only redundant ordinary chords with a final directed
   * native two-hop witness may be removed. */
  if(ngExternalSpatialImport&&!ngExternalManualDraft)NG_SpatialSparseWalks();
  for(i=0;i<ngCount;i++)ngSimpleOwner[i]=i;
  ngSimpleTopologyRejected=0;ngSimplifyStop=ngExternalManualDraft?"manual_draft_preserve_ordinary_topology":"spatial_samples_preserve_local_alternatives";
 }
 NG_FinalizeArrivalRadii();NG_SpawnReachability();
 for(i=0;i<ngCount;i++)if(ngNodes[i].accepted)ngNodes[i].index=count++;
 for(i=0;i<ngEdgeCount;i++)if(ngEdges[i].accepted)accepted++;
 Com_sprintf(output,sizeof(output),"navgen/%s_navmesh.way",level.rawmapname);
 Com_sprintf(report,sizeof(report),"navgen/%s_navmesh.json",level.rawmapname);
 ok=count&&accepted&&ngSpawnSeeds&&NG_ExternalFinalGraph();
 if(!ok)G_Printf("navgen import: final native proof/budget/usable-graph check failed; no WAY written\n");
 if(ok)ok=NG_Save(output,count);
 ok=NG_Report(report,source,level.rawmapname,ok)&&ok;
 G_Printf("navgen import: %s nodes=%d/%d edges=%d/%d spawn_anchors=%d traces=%d time=%dms output=%s report=%s (STAGED, not installed)\n",
  ok?"complete":"FAILED",count,ngCount,accepted,ngEdgeCount,ngSpawnSeeds,ngTraces,
  trap_Milliseconds()-start,output,report);
 return qtrue;
}
/* JSON string emission is shared only by this runtime snapshot. Entity
 * class/model names are map data, so never interpolate them unescaped. */
static void NG_ExternalJsonString(const char *value) {
 const unsigned char *p=(const unsigned char *)(value?value:"");
 NG_Text("\"");
 for(;*p;p++) {
  if(*p=='"'||*p=='\\'){NG_Byte('\\');NG_Byte(*p);}
  else if(*p<32)NG_Text(va("\\u%04x",(unsigned int)*p));
  else NG_Byte(*p);
 }
 NG_Text("\"");
}
static void NG_ExternalMissionEntity(gentity_t *ent,int identity) {
 const char *kind="unsupported";
 if(ent->s.eType==ET_ITEM&&(!strcmp(ent->classname,"team_CTF_redflag")||!strcmp(ent->classname,"team_CTF_blueflag")))kind="take_flag";
 else if(ent->s.eType==ET_OID_TRIGGER&&(ent->spawnflags&3)&&ent->target_ent&&ent->target_ent->inuse) {
  gentity_t *target=ent->target_ent;
  if(g_gametype.integer==2&&target->s.eType==65)kind="activate_capture";
  else if(g_gametype.integer==2&&target->classname&&
          (!strcmp(target->classname,"trigger_flagonly_multiple")||!strcmp(target->classname,"trigger_flagonly")))kind="capture";
  else if((g_gametype.integer==2||g_gametype.integer==5)&&target->s.eType==ET_EXPLOSIVE&&target->constructibleStats.weaponclass==2)kind="plant_defuse";
 }
 NG_Text(va("{\"entity\":%d,\"classname\":",identity));NG_ExternalJsonString(ent->classname);
 NG_Text(",\"native_goal_kind\":");NG_ExternalJsonString(kind);
 NG_Text(",\"track\":");NG_ExternalJsonString(ent->track);
 NG_Text(",\"scriptname\":");NG_ExternalJsonString(ent->scriptName);
 NG_Text(",\"targetname\":");NG_ExternalJsonString(ent->targetname);
 NG_Text(va(",\"spawnflags\":%d,\"linked\":%s,\"contents\":%d,\"state\":%d,\"dropped\":%s,",
  ent->spawnflags,ent->r.linked?"true":"false",ent->r.contents,ent->entstate,
  (ent->flags&FL_DROPPED_ITEM)?"true":"false"));
 NG_Text(va("\"position\":[%.4f,%.4f,%.4f],\"bounds\":[[%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f]],\"contact_top\":%.4f,\"target\":",
  (ent->r.absmin[0]+ent->r.absmax[0])*.5f,(ent->r.absmin[1]+ent->r.absmax[1])*.5f,(ent->r.absmin[2]+ent->r.absmax[2])*.5f,
  ent->r.absmin[0],ent->r.absmin[1],ent->r.absmin[2],ent->r.absmax[0],ent->r.absmax[1],ent->r.absmax[2],
  ent->r.maxs[2]+ent->s.pos.trBase[2]));
 if(ent->target_ent&&ent->target_ent->inuse) {
  gentity_t *target=ent->target_ent;
  NG_Text(va("{\"entity\":%d,\"classname\":",(int)(target-g_entities)));NG_ExternalJsonString(target->classname);
  NG_Text(",\"targetname\":");NG_ExternalJsonString(target->targetname);
  NG_Text(",\"scriptname\":");NG_ExternalJsonString(target->scriptName);
  NG_Text(va(",\"spawnflags\":%d,\"weaponclass\":%d,\"state\":%d,\"position\":[%.4f,%.4f,%.4f],\"bounds\":[[%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f]]}",
   target->spawnflags,target->constructibleStats.weaponclass,target->entstate,
   (target->r.absmin[0]+target->r.absmax[0])*.5f,(target->r.absmin[1]+target->r.absmax[1])*.5f,(target->r.absmin[2]+target->r.absmax[2])*.5f,
   target->r.absmin[0],target->r.absmin[1],target->r.absmin[2],target->r.absmax[0],target->r.absmax[1],target->r.absmax[2]));
 }else NG_Text("null");
 NG_Text("}");
}
static qboolean NG_ExternalEntities(void) {
 char path[MAX_QPATH];int i,count=0,missions=0;
 if(trap_Argc()!=1||!NG_SafePath(level.rawmapname)||strchr(level.rawmapname,'/')) {
  G_Printf("usage: tce_navgen_entities (loaded map required)\n");return qtrue;
 }
 Com_sprintf(path,sizeof(path),"navgen/%s_entities.json",level.rawmapname);
 trap_FS_FOpenFile(path,&ngFile,FS_WRITE);
 if(!ngFile){G_Printf("navgen entities: cannot write %s\n",path);return qtrue;}
 ngIoFailed=qfalse;
 NG_Text(va("{\"schema\":1,\"map\":\"%s\",\"gametype\":%d,\"time\":%d,\"models\":[",level.rawmapname,g_gametype.integer,level.time));
 for(i=MAX_CLIENTS;i<level.num_entities;i++) {
  gentity_t *ent=&g_entities[i];
  if(!ent->inuse||!ent->r.bmodel)continue;
  NG_Text(va("%s{\"entity\":%d,\"modelindex\":%d,\"model\":",count?",":"",i,ent->s.modelindex));
  NG_ExternalJsonString(ent->model);NG_Text(",\"classname\":");NG_ExternalJsonString(ent->classname);
  NG_Text(va(",\"linked\":%s,\"contents\":%d,\"eType\":%d,\"stationary\":%s,\"pos_trtype\":%d,\"apos_trtype\":%d,\"takedamage\":%s,\"health\":%d,\"navigation_ground_kind\":%d,",
   ent->r.linked?"true":"false",ent->r.contents,ent->s.eType,
   ent->s.pos.trType==TR_STATIONARY&&ent->s.apos.trType==TR_STATIONARY?"true":"false",
   ent->s.pos.trType,ent->s.apos.trType,ent->takedamage?"true":"false",ent->health,TCE_NavGroundKind(i)));
  NG_Text(va("\"origin\":[%.4f,%.4f,%.4f],\"angles\":[%.4f,%.4f,%.4f],\"mins\":[%.4f,%.4f,%.4f],\"maxs\":[%.4f,%.4f,%.4f]}",
   ent->r.currentOrigin[0],ent->r.currentOrigin[1],ent->r.currentOrigin[2],
   ent->r.currentAngles[0],ent->r.currentAngles[1],ent->r.currentAngles[2],
   ent->r.absmin[0],ent->r.absmin[1],ent->r.absmin[2],ent->r.absmax[0],ent->r.absmax[1],ent->r.absmax[2]));
  count++;
 }
 NG_Text("],\"objective_entities\":[");
 for(i=MAX_CLIENTS;i<level.num_entities;i++) {
  gentity_t *ent=&g_entities[i];
  if(!ent->inuse||!ent->classname||
     (strcmp(ent->classname,"trigger_objective_info")&&strcmp(ent->classname,"team_CTF_redflag")&&
      strcmp(ent->classname,"team_CTF_blueflag")))continue;
  if(missions)NG_Text(",");NG_ExternalMissionEntity(ent,i);missions++;
 }
 NG_Text("],\"door_entities\":[");
 {int doors=0;
  for(i=MAX_CLIENTS;i<level.num_entities;i++) {
   gentity_t *ent=&g_entities[i];ngDoorPose pose;int team=0,k;const char *why=NULL;qboolean door=NG_DoorClass(ent);
   if(!ent->inuse||!ent->classname||(!door&&Q_stricmp(ent->classname,"func_button")&&
      Q_stricmp(ent->classname,"func_invisible_user")&&Q_stricmp(ent->classname,"target_relay")))continue;
   if(doors++)NG_Text(",");NG_Text(va("{\"entity\":%d,\"modelindex\":%d,\"classname\":",i,ent->s.modelindex));NG_ExternalJsonString(ent->classname);
   NG_Text(",\"target\":");NG_ExternalJsonString(ent->target);NG_Text(",\"targetname\":");NG_ExternalJsonString(ent->targetname);
   NG_Text(",\"scriptname\":");NG_ExternalJsonString(ent->scriptName);
   NG_Text(va(",\"key\":%d,\"allowteams\":%d,\"spawnflags\":%d,\"mover_state\":%d,\"linked\":%s,\"contents\":%d,\"origin\":[%.4f,%.4f,%.4f],\"bounds\":[[%.4f,%.4f,%.4f],[%.4f,%.4f,%.4f]]",
    ent->key,ent->allowteams,ent->spawnflags,ent->moverState,ent->r.linked?"true":"false",ent->r.contents,
    ent->r.currentOrigin[0],ent->r.currentOrigin[1],ent->r.currentOrigin[2],ent->r.absmin[0],ent->r.absmin[1],ent->r.absmin[2],ent->r.absmax[0],ent->r.absmax[1],ent->r.absmax[2]));
   if(door){why=NG_DoorGroup(i,&pose,&team);
    NG_Text(va(",\"master\":%d,\"team\":%d,\"eligible\":%s,\"reason\":",NG_DoorMaster(i),team,why?"false":"true"));NG_ExternalJsonString(why?why:"binary_pose_requires_native_passage_and_runtime_activation");
    {const float *closed=Q_stricmp(ent->classname,"func_door_rotating")?ent->pos1:ent->r.currentOrigin;
     const float *open=Q_stricmp(ent->classname,"func_door_rotating")?ent->pos2:ent->r.currentOrigin;
     NG_Text(va(",\"closed_origin\":[%.4f,%.4f,%.4f],\"open_origin\":[%.4f,%.4f,%.4f],\"rotation_axis\":[%.4f,%.4f,%.4f],\"rotation_degrees\":%.4f,\"parts\":[",
      closed[0],closed[1],closed[2],open[0],open[1],open[2],ent->rotate[0],ent->rotate[1],ent->rotate[2],ent->angle));}
    for(k=0;k<pose.count;k++)NG_Text(va("%s%d",k?",":"",pose.part[k].entity));NG_Text("]");
   }
   NG_Text("}");
  }
 }
 NG_Text("]}\n");trap_FS_FCloseFile(ngFile);
 G_Printf("navgen entities: %s %d inline model entities / %d mission entities -> %s\n",ngIoFailed?"FAILED":"wrote",count,missions,path);
 return qtrue;
}
