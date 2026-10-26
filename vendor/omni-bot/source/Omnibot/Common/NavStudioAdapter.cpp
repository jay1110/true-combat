#include "PrecompCommon.h"
#include "NavStudioAdapter.h"
#include "EngineFuncs.h"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <sstream>

namespace NavStudioAdapter {
namespace {
constexpr int MaxNodes = 15872, MaxEdges = 253952, TraceBudget = 2000000;
constexpr float Width = 18, Stand = 72, Duck = 48, Origin = 24, Step = 18;
constexpr uint64_t Crouch = 64, Climb = 256, Jump = 512, JumpLow = 1ull << 18, JumpGap = 1ull << 27;
constexpr int Mask = TR_MASK_SOLID | TR_MASK_PLAYERCLIP;
struct Node {
 int id=0,index=-1,role=0,drop=0,incoming=0,outgoing=0,rejected=0;
 Vector3f raw=Vector3f::ZERO,feet=Vector3f::ZERO;
 uint64_t flags=0;
 float radius=35;
 bool crouch=false,spawn=false,floor=false,reachable=false;
 String reason="pending";
};
struct Edge { int from=0,to=0; bool accepted=false,crouch=false; String reason="pending"; };
struct State {
 String map,source,error;
 fs::path directory;
 int version=4,traces=0;
 size_t nextNode=0,nextEdge=0;
 std::vector<Node> nodes;
 std::vector<Edge> edges;
 std::map<int,size_t> ids;
};
std::unique_ptr<State> pending;

bool Name(const String &value) {
 if(value.empty()||value.size()>160) return false;
 for(char c:value) if(!(std::isalnum(static_cast<unsigned char>(c))||c=='_'||c=='-'||c=='.')) return false;
 return value.find("..")==String::npos;
}
String Quote(const String &value) {
 std::ostringstream out;out<<'"';
 for(unsigned char c:value) {
  if(c=='"'||c=='\\') out<<'\\'<<c;
  else if(c<32||c>=127) out<<"\\u00"<<std::hex<<std::setw(2)<<std::setfill('0')<<static_cast<int>(c)<<std::dec;
  else out<<c;
 }
 out<<'"';return out.str();
}
void Vec(std::ostream &out,const Vector3f &p) { out<<'['<<p.x<<','<<p.y<<','<<p.z<<']'; }
void Vec(std::ostream &out,const float p[3]) { out<<'['<<p[0]<<','<<p[1]<<','<<p[2]<<']'; }
bool Finite(const Vector3f &p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z)&&p.SquaredLength()<3.f*131072.f*131072.f; }
fs::path OutputRoot() {
 const char *home=g_EngineFuncs->GetLogPath();
 if(!home||!*home) throw std::runtime_error("original ET engine returned no game log/home directory");
 auto root=fs::canonical(fs::path(home))/"navgen";
 fs::create_directories(root);return root;
}
void Atomic(const fs::path &path,const String &bytes) {
 auto temp=path;temp+=".navstudio.tmp";
 { std::ofstream stream(temp.string().c_str(),std::ios::binary|std::ios::trunc);
  stream.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));stream.flush();
  if(!stream) throw std::runtime_error("unable to write NavStudio output"); }
 if(fs::exists(path)) fs::remove(path);
 fs::rename(temp,path);
}
AABB Hull(float height) {
 AABB box;box.m_Mins[0]=box.m_Mins[1]=-Width;box.m_Mins[2]=-Origin;
 box.m_Maxs[0]=box.m_Maxs[1]=Width;box.m_Maxs[2]=height-Origin;return box;
}
bool Trace(State &s,obTraceResult &result,const Vector3f &a,const Vector3f &b,const AABB *hull) {
 if(s.traces++>=TraceBudget) {s.error="native_trace_budget_exhausted";return false;}
 return EngineFuncs::TraceLine(result,a,b,hull,Mask,-1,False);
}
bool Clear(State &s,const Vector3f &a,const Vector3f &b,float height) {
 AABB hull=Hull(height);obTraceResult result;
 return Trace(s,result,a,b,&hull)&&!result.m_StartSolid&&result.m_Fraction>=.9999f;
}
bool WorldSupport(const obTraceResult &result) {
 return !result.m_HitEntity.IsValid()||result.m_HitEntity.GetIndex()==1022;
}
bool Ground(State &s,const Vector3f &feet,Vector3f &found,float height,String &reason) {
 // Probe only the feet before testing the full stationary posture. Raising a
 // full player hull by one step would falsely reject low crouch-only passages.
 AABB hull=Hull(2);obTraceResult result;
 Vector3f origin=feet+Vector3f(0,0,Origin);
 if(!Trace(s,result,origin+Vector3f(0,0,Step),origin-Vector3f(0,0,64),&hull)) {reason="engine_trace_failed";return false;}
 if(result.m_StartSolid||result.m_Fraction>=1.f) {reason="no_clear_supported_floor";return false;}
 if(result.m_Normal[2]<.7f) {reason="floor_slope_exceeds_et_walk_limit";return false;}
 if(!WorldSupport(result)) {reason="entity_ground_requires_dynamic_recipe";return false;}
 found=Vector3f(result.m_Endpos[0],result.m_Endpos[1],result.m_Endpos[2]-Origin+.25f);
 Vector3f wading=found+Vector3f(0,0,18);
 if(g_EngineFuncs->GetPointContents(static_cast<const float*>(wading))&(CONT_WATER|CONT_SLIME|CONT_LAVA)) {reason="deep_water_or_hazard";return false;}
 if(!Clear(s,found+Vector3f(0,0,Origin),found+Vector3f(0,0,Origin),height)) {reason="standing_hull_occupied";return false;}
 reason="native_et_floor_validated";return true;
}
bool Walk(State &s,Node &a,Node &b,float height,String &reason) {
 Vector3f delta=b.feet-a.feet;
 float horizontal=std::sqrt(delta.x*delta.x+delta.y*delta.y);
 if(horizontal>2048) {reason="ordinary_link_too_long";return false;}
 int steps=std::max(1,static_cast<int>(std::ceil(horizontal/16.f)));
 Vector3f current=a.feet;
 for(int i=1;i<=steps;i++) {
  Vector3f sample=a.feet+delta*(static_cast<float>(i)/steps);sample.z=current.z;
  Vector3f next;
  if(!Ground(s,sample,next,height,reason)) return false;
  if(next.z-current.z>Step+.5f||next.z-current.z<-Step-.5f) {reason="step_or_drop_exceeds_et_limit";return false;}
  Vector3f start=current+Vector3f(0,0,Origin),end=next+Vector3f(0,0,Origin);
  if(!Clear(s,start,end,height)) {
   Vector3f up(0,0,Step);
   if(!Clear(s,start,start+up,height)||!Clear(s,start+up,end+up,height)||!Clear(s,end+up,end,height)) {reason="player_hull_blocked";return false;}
  }
  current=next;
 }
 if((current-b.feet).Length()>2.f) {reason="different_floor_at_target";return false;}
 reason="external_native_validated_walk";return true;
}
bool Arc(State &s,const Node &a,const Node &b,String &reason) {
 constexpr float velocity=270,gravity=800,speed=320;
 float dz=b.feet.z-a.feet.z,disc=velocity*velocity-2*gravity*dz;
 if(disc<0) {reason="jump_height_exceeds_et_impulse";return false;}
 float duration=(velocity+std::sqrt(disc))/gravity;
 Vector3f horizontal=b.feet-a.feet;horizontal.z=0;
 if(duration<=0||duration>1.25f||horizontal.Length()>speed*duration) {reason="jump_gap_exceeds_et_speed";return false;}
 int steps=std::max(2,static_cast<int>(std::ceil(duration/.02f)));
 Vector3f previous=a.feet+Vector3f(0,0,Origin);
 for(int i=1;i<=steps;i++) {
  float t=duration*i/steps;
  Vector3f next=a.feet+horizontal*(t/duration)+Vector3f(0,0,Origin+velocity*t-.5f*gravity*t*t);
  if(!Clear(s,previous,next,Stand)) {reason="et_ballistic_hull_blocked";return false;}
  previous=next;
 }
 reason="et_swept_ballistic_jump_live_pending";return true;
}
bool LadderContact(State &s,const Vector3f &origin) {
 if(g_EngineFuncs->GetPointContents(static_cast<const float*>(origin))&CONT_LADDER) return true;
 for(int i=0;i<8;i++) {
  float angle=i*3.14159265f/4;
  obTraceResult result;
  if(Trace(s,result,origin,origin+Vector3f(std::cos(angle)*36,std::sin(angle)*36,0),nullptr)&&
     !result.m_StartSolid&&result.m_Fraction<1.f&&(result.m_Surface&SURFACE_LADDER)) return true;
 }
 return false;
}
bool Ladder(State &s,const Node &a,const Node &b,String &reason) {
 Vector3f delta=b.feet-a.feet;
 if(std::abs(delta.z)<18||std::abs(delta.z)>2048||delta.x*delta.x+delta.y*delta.y>48*48) {reason="ladder_alignment_outside_checked_column";return false;}
 int steps=std::max(2,static_cast<int>(std::ceil(std::abs(delta.z)/16)));
 Vector3f previous=a.feet+Vector3f(0,0,Origin);
 for(int i=1;i<=steps;i++) {
  Vector3f next=a.feet+delta*(static_cast<float>(i)/steps)+Vector3f(0,0,Origin);
  if(!Clear(s,previous,next,Duck)||!LadderContact(s,next)) {reason="ladder_surface_or_hull_not_proven";return false;}
  previous=next;
 }
 reason="et_ladder_surface_sweep_live_pending";return true;
}
// Omni v9 sets bit 7 on the LAST byte (the inverse of LEB128).
void Packed(String &out,uint64_t value) {do {unsigned char byte=value&127;value>>=7;out+=static_cast<char>(byte|(value?0:128));}while(value);}
void Bytes(String &out,const void *value,size_t size) {out.append(static_cast<const char*>(value),size);}
void Text(String &out,const String &value) {Packed(out,value.size());out+=value;}
String Way(State &s) {
 String out;unsigned char version=9;uint32_t count=0;
 for(auto &node:s.nodes) if(node.reachable) node.index=count++;
 Bytes(out,&version,1);Bytes(out,&count,4);out.append(264,'\0');
 AABB bounds;g_EngineFuncs->GetMapExtents(bounds);Bytes(out,bounds.m_Mins,12);Bytes(out,bounds.m_Maxs,12);
 std::vector<std::vector<int>> connections(s.nodes.size());
 for(auto &edge:s.edges) if(edge.accepted&&s.nodes[edge.from].reachable&&s.nodes[edge.to].reachable) connections[edge.from].push_back(s.nodes[edge.to].index);
 for(size_t i=0;i<s.nodes.size();i++) {
  auto &node=s.nodes[i];if(!node.reachable) continue;
  unsigned char mask=4|16|(node.flags?1:0);Bytes(out,&mask,1);
  Vector3f position=node.feet+Vector3f(0,0,Origin);Bytes(out,static_cast<const float*>(position),12);
  if(node.flags) Packed(out,node.flags);
  Bytes(out,&node.radius,4);Packed(out,1);Text(out,"tce_node");Text(out,std::to_string(node.id));
  Packed(out,connections[i].size());
  for(int target:connections[i]) {int delta=target-node.index;Packed(out,delta<0?(static_cast<uint64_t>(-delta-1)<<1)|1:static_cast<uint64_t>(delta)<<1);}
 }
 return out;
}
void Finish(State &s) {
 std::vector<std::vector<size_t>> outgoing(s.nodes.size());
 for(auto &edge:s.edges) if(edge.accepted) outgoing[edge.from].push_back(edge.to);
 std::queue<size_t> queue;
 for(size_t i=0;i<s.nodes.size();i++) if(s.nodes[i].floor&&s.nodes[i].spawn) {s.nodes[i].reachable=true;queue.push(i);}
 if(queue.empty()) {
  std::vector<AutoNavFeature> features(4096);
  int count=g_EngineFuncs->GetAutoNavFeatures(features.data(),static_cast<int>(features.size()));
  for(int j=0;j<std::min(count,static_cast<int>(features.size()));j++) {
   int type=features[j].m_Type;
   if(type!=ENT_CLASS_GENERIC_PLAYERSTART&&type!=ENT_CLASS_GENERIC_PLAYERSTART_TEAM1&&type!=ENT_CLASS_GENERIC_PLAYERSTART_TEAM2) continue;
   Vector3f point(features[j].m_Position[0],features[j].m_Position[1],features[j].m_Position[2]-Origin);
   float best=192*192;size_t chosen=s.nodes.size();
   for(size_t i=0;i<s.nodes.size();i++) if(s.nodes[i].floor) {float distance=(s.nodes[i].feet-point).SquaredLength();if(distance<best){best=distance;chosen=i;}}
   if(chosen<s.nodes.size()&&!s.nodes[chosen].reachable){s.nodes[chosen].reachable=true;queue.push(chosen);}
  }
 }
 while(!queue.empty()){size_t node=queue.front();queue.pop();for(size_t target:outgoing[node])if(!s.nodes[target].reachable){s.nodes[target].reachable=true;queue.push(target);}}
 bool complete=s.error.empty();int accepted=0,links=0,floorCount=0,initialLinks=0;std::vector<int> isolated,missing;
 for(auto &edge:s.edges) if(edge.accepted) {initialLinks++;s.nodes[edge.from].outgoing++;s.nodes[edge.to].incoming++;}
 for(auto &node:s.nodes) {
  if(node.floor) floorCount++;
  if(node.floor&&!node.reachable) {node.reason="missing_spawn_route";missing.push_back(node.id);}
  if(node.reachable){node.index=accepted++;if(node.outgoing==0){isolated.push_back(node.id);complete=false;}}
 }
 for(auto &edge:s.edges) {if(edge.accepted&&s.nodes[edge.from].reachable&&s.nodes[edge.to].reachable) links++;else if(edge.accepted){edge.accepted=false;edge.reason="outside_spawn_route";}}
 if(accepted==0||links==0) complete=false;
 if(s.version==5) for(auto &node:s.nodes) if(!node.floor||node.rejected) complete=false;
 if(complete) Atomic(s.directory/(s.map+"_navmesh.way"),Way(s));
 std::ostringstream out;out<<std::setprecision(9);
 out<<"{\"schema\":1,\"adapter\":\"original_et_engine_interface_v1\",\"map\":"<<Quote(s.map)<<",\"source\":"<<Quote(s.source)
    <<",\"generation_complete\":"<<(complete?"true":"false")<<",\"candidate_mode\":"<<Quote(s.version==5?"manual_draft":"automatic_spatial")
    <<",\"candidate_kind\":\"external_bsp_recast\",\"preserve_spatial_samples\":true,\"generation_mode\":"<<Quote(s.version==5?"manual_graph_native_validation":"spatial_surface_samples")<<",\"traces\":"<<s.traces
    <<",\"trace_budget\":"<<TraceBudget<<",\"error\":"<<Quote(s.error)<<",\"bbox\":{\"half_width\":18,\"stand_height\":72,\"crouch_height\":48,\"origin_above_feet\":24},"
    <<"\"limitations\":\"Actual original-ET AABB/body and world-support traces; stationary entity grounds, swimming and live jump/ladder traversal remain separate. Input coverage is not whole-map completion.\",\"traversal_routes\":[],\"nodes\":[";
 for(size_t i=0;i<s.nodes.size();i++) {
  auto &n=s.nodes[i];if(i)out<<',';
  out<<"{\"id\":"<<n.id<<",\"index\":"<<n.index<<",\"accepted\":"<<(n.reachable?"true":"false")<<",\"spawn_reachable\":"<<(n.reachable?"true":"false")
     <<",\"crouch\":"<<(n.crouch?"true":"false")<<",\"radius\":"<<n.radius<<",\"nav_flags\":"<<n.flags<<",\"reason\":"<<Quote(n.reason)<<",\"position\":";
  Vec(out,n.feet+Vector3f(0,0,Origin));out<<",\"raw_position\":";Vec(out,n.raw+Vector3f(0,0,Origin));
  out<<",\"native_input\":{\"floor_accepted\":"<<(n.floor?"true":"false")<<",\"floor_reason\":"<<Quote(n.floor?"native_et_floor_validated":n.reason)
     <<",\"initial_incoming\":"<<n.incoming<<",\"initial_outgoing\":"<<n.outgoing<<",\"initial_rejected_incident_links\":"<<n.rejected
     <<",\"status\":"<<Quote(n.reachable?"spawn_reachable":n.floor?"missing_spawn_route":"geometry_rejected")<<"}}";
 }
 out<<"],\"edges\":[";
 for(size_t i=0;i<s.edges.size();i++){auto &e=s.edges[i];if(i)out<<',';out<<"{\"from\":"<<s.nodes[e.from].id<<",\"to\":"<<s.nodes[e.to].id<<",\"accepted\":"<<(e.accepted?"true":"false")<<",\"crouch\":"<<(e.crouch?"true":"false")<<",\"reason\":"<<Quote(e.reason)<<'}';}
 out<<"],\"native_input_coverage\":{\"input_nodes\":"<<s.nodes.size()<<",\"input_ids\":[";
 for(size_t i=0;i<s.nodes.size();i++){if(i)out<<',';out<<s.nodes[i].id;}
 out<<"],\"initial_directed_links\":"<<s.edges.size()<<",\"initial_accepted_links\":"<<initialLinks<<",\"initial_rejected_links\":"<<s.edges.size()-initialLinks
    <<",\"native_floor_accepted\":"<<floorCount<<",\"native_floor_rejected\":"<<s.nodes.size()-floorCount<<",\"exported_input_nodes\":"<<accepted
    <<",\"spawn_reachable_input_nodes\":"<<accepted<<",\"connectivity_missing_input_nodes\":"<<missing.size()<<",\"contracted_input_nodes\":0,\"ordinary_without_exit\":[";
 for(size_t i=0;i<isolated.size();i++){if(i)out<<',';out<<isolated[i];}
 out<<"],\"missing_regions\":[";
 if(!missing.empty()){out<<"{\"node_ids\":[";for(size_t i=0;i<missing.size();i++){if(i)out<<',';out<<missing[i];}out<<"]}";}
 out<<"],\"geometry_rejections\":[";std::map<String,int> reasons;
 for(auto &node:s.nodes)if(!node.floor)reasons[node.reason]++;
 bool comma=false;for(auto &reason:reasons){if(comma)out<<',';comma=true;out<<"{\"reason\":"<<Quote(reason.first)<<",\"count\":"<<reason.second<<'}';}
 out<<"],\"capacity\":{\"input_validation_complete\":true,\"input_truncated\":false},\"full_map_verified\":false},\"quarantined_unreachable_nodes\":"<<missing.size()
    <<",\"coverage\":{\"pending_nodes\":0,\"spawn_connected_pending_nodes\":0,\"stop_reason\":\"external_candidate_validation_complete\"}}";
 Atomic(s.directory/(s.map+"_navmesh.json"),out.str());
 EngineFuncs::ConsoleMessage(va("NavStudio ET import %s: %d nodes, %d links, complete=%d, traces=%d",s.map.c_str(),accepted,links,complete?1:0,s.traces));
}
}

void Import(const StringVector &args) {
 try {
  if(pending) throw std::runtime_error("NavStudio ET import already running");
  if(args.size()!=2||args[1].compare(0,7,"navgen/")!=0||!Name(args[1].substr(7))||args[1].size()<12||args[1].substr(args[1].size()-4)!=".txt") throw std::runtime_error("expected navgen/<candidate.txt>");
  std::unique_ptr<State> state(new State);state->directory=OutputRoot();state->map=g_EngineFuncs->GetMapName();state->source=args[1];
  if(!Name(state->map)) throw std::runtime_error("invalid current map name");
  auto input=state->directory/args[1].substr(7);
  if(fs::file_size(input)>32*1024*1024) throw std::runtime_error("candidate exceeds 32 MiB");
  std::ifstream stream(input.string().c_str());String line,magic,map;int count=0,links=0;
  std::getline(stream,line);std::istringstream header(line);header>>magic>>state->version>>map>>count>>links;
  if(!header||magic!="TCE_NAVMESH"||(state->version!=4&&state->version!=5)||map!=state->map||count<1||count>MaxNodes||links<1||links>MaxEdges) throw std::runtime_error("invalid candidate header/map/count/format (requires 4 or 5)");
  for(int i=0;i<count;i++) {
   std::getline(stream,line);std::istringstream row(line);Node node;int duck=0,spawn=0,protectedNode=0;uint32_t low=0,high=0;
   row>>node.id>>node.raw.x>>node.raw.y>>node.raw.z>>duck>>spawn>>protectedNode;
   if(state->version==5){row>>node.radius>>low>>high>>node.role>>node.drop;node.flags=(static_cast<uint64_t>(high)<<32)|low;}
   node.crouch=duck!=0;node.spawn=spawn!=0;node.feet=node.raw;if(node.crouch)node.flags|=Crouch;
   String extra;if(!row||row>>extra||node.id<0||!Finite(node.raw)||node.radius<1||node.radius>100||duck<0||duck>1||spawn<0||spawn>1||protectedNode<0||protectedNode>1||state->ids.count(node.id)) throw std::runtime_error("invalid/duplicate candidate node");
   state->ids[node.id]=state->nodes.size();state->nodes.push_back(node);
  }
  std::set<std::pair<int,int>> pairs;
  for(int i=0;i<links;i++) {
   std::getline(stream,line);std::istringstream row(line);int a=-1,b=-1;row>>a>>b;String extra;
   if(!row||row>>extra||a==b||!state->ids.count(a)||!state->ids.count(b)||!pairs.insert({a,b}).second) throw std::runtime_error("invalid/duplicate candidate edge");
   Edge edge;edge.from=static_cast<int>(state->ids[a]);edge.to=static_cast<int>(state->ids[b]);state->edges.push_back(edge);
  }
  while(std::getline(stream,line)) if(line.find_first_not_of(" \t\r\n")!=String::npos) throw std::runtime_error("unexpected candidate trailing data");
  pending=std::move(state);EngineFuncs::ConsoleMessage("NavStudio ET native import started; checked output will be written without changing the active graph.");
 } catch(const std::exception &error) {EngineFuncs::ConsoleError(va("NavStudio ET import error: %s",error.what()));}
}

void Update() {
 if(!pending) return;
 try {
  auto &s=*pending;
  if(s.map!=g_EngineFuncs->GetMapName()){pending.reset();EngineFuncs::ConsoleError("NavStudio ET import cancelled: map changed");return;}
  auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(8);
  while(std::chrono::steady_clock::now()<deadline&&s.error.empty()) {
   if(s.nextNode<s.nodes.size()) {
    auto &node=s.nodes[s.nextNode++];String reason;
    node.floor=Ground(s,node.raw,node.feet,node.crouch?Duck:Stand,reason);
    if(!node.floor&&!node.crouch&&s.version==4) {node.floor=Ground(s,node.raw,node.feet,Duck,reason);if(node.floor){node.crouch=true;node.flags|=Crouch;}}
    if(node.role&~1){node.floor=false;reason="authored_action_role_requires_et_recipe";}
    node.reason=reason;continue;
   }
   if(s.nextEdge<s.edges.size()) {
    auto &edge=s.edges[s.nextEdge++];auto &a=s.nodes[edge.from];auto &b=s.nodes[edge.to];
    if(!a.floor||!b.floor) edge.reason="endpoint_floor_rejected";
    else {
     edge.crouch=a.crouch||b.crouch;
     if(a.flags&(Jump|JumpLow|JumpGap)) edge.accepted=Arc(s,a,b,edge.reason);
     else edge.accepted=Walk(s,a,b,edge.crouch?Duck:Stand,edge.reason);
     if(!edge.accepted&&((a.flags|b.flags)&Climb)) edge.accepted=Ladder(s,a,b,edge.reason);
     if(!edge.accepted&&s.version==4&&!edge.crouch&&!(a.flags&(Jump|JumpLow|JumpGap))) {edge.accepted=Walk(s,a,b,Duck,edge.reason);if(edge.accepted){edge.crouch=true;a.crouch=b.crouch=true;a.flags|=Crouch;b.flags|=Crouch;}}
    }
    if(!edge.accepted){a.rejected++;b.rejected++;}continue;
   }
   Finish(s);pending.reset();return;
  }
  if(!s.error.empty()){Finish(s);pending.reset();}
 } catch(const std::exception &error){EngineFuncs::ConsoleError(va("NavStudio ET import failure: %s",error.what()));pending.reset();}
}

void Snapshot(const StringVector &args) {
 try {
  if(args.size()!=1) throw std::runtime_error("navstudio_snapshot accepts no arguments");
  String map=g_EngineFuncs->GetMapName();if(!Name(map))throw std::runtime_error("invalid current map");
  std::ostringstream out;out<<std::setprecision(9)<<"{\"schema\":1,\"map\":"<<Quote(map)<<",\"adapter\":\"original_et_engine_interface_v1\",\"models\":[],\"engine_entities\":[";
  bool comma=false;
  for(int id=64;id<1022;id++) {
   GameEntity ent=g_EngineFuncs->EntityFromID(id);if(!ent.IsValid())continue;
   Vector3f position;AABB local,world;float forward[3],right[3],up[3];
   if(!EngineFuncs::EntityPosition(ent,position)||!EngineFuncs::EntityLocalAABB(ent,local)||!EngineFuncs::EntityWorldAABB(ent,world)||!EngineFuncs::EntityOrientation(ent,forward,right,up))continue;
   bool collision=false;
   for(int axis=0;axis<3;axis++){
    Vector3f a=position,b=position;a[axis]=world.m_Mins[axis]-32;b[axis]=world.m_Maxs[axis]+32;obTraceResult trace;
    if(EngineFuncs::TraceLine(trace,a,b,nullptr,Mask,-1,False)&&trace.m_HitEntity==ent&&trace.m_Fraction<1){collision=true;break;}
   }
   Vector3f origin=position;
   for(int axis=0;axis<3;axis++){
    float center=(local.m_Mins[axis]+local.m_Maxs[axis])*.5f;
    for(int j=0;j<3;j++)origin[j]-=center*(axis==0?forward[j]:axis==1?-right[j]:up[j]);
   }
   Vector3f angles(std::atan2(-forward[2],std::sqrt(forward[0]*forward[0]+forward[1]*forward[1]))*57.2957795f,std::atan2(forward[1],forward[0])*57.2957795f,std::atan2(-right[2],up[2])*57.2957795f);
   if(comma)out<<',';comma=true;out<<"{\"entity\":"<<id<<",\"name\":"<<Quote(EngineFuncs::EntityName(ent))<<",\"entity_class\":"<<g_EngineFuncs->GetEntityClass(ent)<<",\"collision_observed\":"<<(collision?"true":"false")<<",\"position\":";Vec(out,position);
   out<<",\"origin\":";Vec(out,origin);out<<",\"angles\":";Vec(out,angles);out<<",\"local_mins\":";Vec(out,local.m_Mins);out<<",\"local_maxs\":";Vec(out,local.m_Maxs);out<<",\"mins\":";Vec(out,world.m_Mins);out<<",\"maxs\":";Vec(out,world.m_Maxs);out<<'}';
  }
  out<<"],\"autonav_features\":[";std::vector<AutoNavFeature> features(4096);int count=g_EngineFuncs->GetAutoNavFeatures(features.data(),static_cast<int>(features.size()));
  for(int i=0;i<std::min(count,static_cast<int>(features.size()));i++) {
   auto &f=features[i];if(i)out<<',';out<<"{\"type\":"<<f.m_Type<<",\"team\":"<<f.m_Team<<",\"position\":";Vec(out,f.m_Position);out<<",\"target_position\":";Vec(out,f.m_TargetPosition);out<<",\"mins\":";Vec(out,f.m_Bounds.m_Mins);out<<",\"maxs\":";Vec(out,f.m_Bounds.m_Maxs);out<<",\"obstacle_entity\":"<<(f.m_ObstacleEntity?"true":"false")<<",\"bidirectional\":"<<(f.m_BiDirectional?"true":"false")<<'}';
  }
  out<<"],\"limits\":\"Original ET API has no raw modelindex/classname; match queried local bounds/name/pose to BSP before exporting collision models.\"}";
  Atomic(OutputRoot()/(map+"_entities.json"),out.str());EngineFuncs::ConsoleMessage("NavStudio ET snapshot written from actual original engine entity queries.");
 } catch(const std::exception &error){EngineFuncs::ConsoleError(va("NavStudio ET snapshot error: %s",error.what()));}
}
}
