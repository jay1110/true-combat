/* Production planning and delivery methods; deterministic navigation services.
 * This exercises shared-planner client ownership, not game movement physics. */
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
typedef unsigned obuint32;
typedef unsigned long long NavFlags;
enum { NAV_FOUNDGOAL=1, WAYPOINT_ISDONE=2, SKIP_NO_CONNECTIONS=1,
       ENT_FLAG_ONLADDER=1, ENT_FLAG_ONGROUND=2 };
enum { F_NAV_CLOSED=1, F_NAV_TEAMONLY=2, F_NAV_CLIMB=4, F_NAV_JUMP=8,
       F_NAV_JUMPLOW=16, F_NAV_JUMPGAP=32, F_NAV_TELEPORT=64,
       F_NAV_ELEVATOR=128, F_NAV_SCRIPT=256, F_NAV_DOOR=512 };
#define NAVSTUDIO_DIAGNOSTIC(x) ((void)0)
struct Vector3f {
 float x,y,z;
 Vector3f(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
 Vector3f operator-(const Vector3f&v)const{return Vector3f(x-v.x,y-v.y,z-v.z);}
 float Length()const{return std::sqrt(x*x+y*y+z*z);}
 Vector3f AddZ(float a)const{return Vector3f(x,y,z+a);}
};
static float Length(const Vector3f&a,const Vector3f&b){return (a-b).Length();}
static float SquaredLength(const Vector3f&a,const Vector3f&b){const float n=Length(a,b);return n*n;}
struct PropertyMap {std::map<std::string,std::string> p;std::string GetProperty(const char*k)const{auto i=p.find(k);return i==p.end()?std::string():i->second;}};
struct Waypoint {
 Vector3f pos;float radius=35;NavFlags flags=0;obuint32 uid=1;
 int m_Mark=0,m_GoalIndex=0;std::vector<int>m_Connections;PropertyMap props;
 bool IsFlagOn(NavFlags f)const{return (flags&f)!=0;}
 Vector3f GetPosition()const{return pos;}float GetRadius()const{return radius;}
 NavFlags GetNavigationFlags()const{return flags;}obuint32 GetUID()const{return uid;}
 int OnPathThrough()const{return 0;}int OnPathThroughParam()const{return 0;}
 const PropertyMap&GetPropertyMap()const{return props;}
};
struct Client {
 mutable int calls=0;bool validated=true,poisoned=false;
 virtual ~Client(){}
 virtual bool UseValidatedWalkEntries()const{if(poisoned)throw std::runtime_error("removed client consulted");++calls;return validated;}
 bool HasEntityFlag(int)const{return false;}
 bool CanWalkTo(const Vector3f&,NavFlags)const{return true;}
 int GetGameID()const{return 3;}int GetTeam()const{return 1;}
 Vector3f GetPosition()const{return Vector3f();}
};
struct Destination {Vector3f m_Position;float m_Radius;Destination(Vector3f p,float r):m_Position(p),m_Radius(r){}};
typedef std::vector<Destination>DestinationVector;
struct Flags {unsigned bits=0;void SetFlag(int n){bits|=n;}void ClearFlag(int n){bits&=~n;}bool CheckFlag(int n)const{return (bits&n)!=0;}};
struct ClosestLink {
 enum{NumWps=1};Waypoint*m_Wp[1];ClosestLink(Waypoint*w=NULL){m_Wp[0]=w;}
 bool IsValid()const{return m_Wp[0]!=NULL;}
};
struct Path {
 struct TceDoorRecipe {int role=0,mode=0,target=0,entry=0;bool Valid()const{return false;}bool SameDoor(const TceDoorRecipe&)const{return false;}};
 struct Point {
  Point&Flags(NavFlags){return *this;}Point&NavId(obuint32){return *this;}
  Point&TceAscentRelease(bool){return *this;}Point&TceAscentAlignment(bool){return *this;}
  Point&TceWalkDrop(bool){return *this;}Point&TceStandJump(bool){return *this;}
  Point&TceFreeClimb(bool){return *this;}Point&TceDoor(TceDoorRecipe){return *this;}
  Point&OnPathThrough(int){return *this;}Point&OnPathThroughParam(int){return *this;}
 }point;
 int count=0;Point&AddPt(const Vector3f&,float){++count;return point;}
};
struct TceRouteReservation {int owner,team,created,expires;std::vector<obuint32>nodes;};
static std::vector<TceRouteReservation>tceRoutes;
static int prepareCalls;
static void TcePrepareRoutes(Client*c){++prepareCalls;if(c)c->UseValidatedWalkEntries();}
struct IGame {static int GetTime(){return 1000;}};
namespace Utils {static float ClosestPtOnLine(const Vector3f&,const Vector3f&,const Vector3f&,Vector3f&v){v=Vector3f();return 0;}}
static float g_fBottomWaypointOffset=-24,g_fTopWaypointOffset=40,g_fPathLevelOffset=.5f;
struct PathPlannerWaypoint {
 Client*m_Client=NULL;std::vector<Waypoint*>m_Solution,m_WaypointList,m_OpenList;
 Flags m_PlannerFlags;ClosestLink m_Start;int m_GoalIndex=0,m_WaypointMark=0;
 int PlanPathToNearestWithEntryPolicy(Client*,const Vector3f&,const DestinationVector&,const NavFlags&,bool);
 void GetPath(Path&,int=0);
 Waypoint*_GetClosestWaypoint(const Vector3f&,NavFlags,int,void*){return NULL;}
 void _RunDijkstra(NavFlags){}bool FoundGoal()const{return m_PlannerFlags.CheckFlag(NAV_FOUNDGOAL);}
 Path::TceDoorRecipe ReadDoorRecipe(Waypoint*)const{return Path::TceDoorRecipe();}
};
/* ACTUAL_PLAN */
/* ACTUAL_GET_PATH */
int main(){
 try {
  PathPlannerWaypoint planner;Client active,old;old.poisoned=true;Waypoint stale;
  DestinationVector goals(1,Destination(Vector3f(10,0,0),10));Path path;
  /* A shared planner still remembers the removed bot, while another bot is
   * already inside its destination. The old implementation calls old here. */
  planner.m_Client=&old;planner.m_Solution.push_back(&stale);
  assert(planner.PlanPathToNearestWithEntryPolicy(&active,Vector3f(),goals,0,false)==0);
  assert(planner.FoundGoal()&&planner.m_Solution.empty());
  planner.GetPath(path);assert(path.count==0);
  assert(planner.m_Client==&active&&active.calls==1);
  /* Real deletion must also be safe under ASan, rather than just tolerating
   * a NULL previous client. No deleted object is dereferenced by this test. */
  Client*removed=new Client();planner.m_Client=removed;delete removed;
  planner.PlanPathToNearestWithEntryPolicy(&active,Vector3f(10,0,0),goals,0,false);
  planner.GetPath(path);assert(planner.m_Client==&active&&path.count==0);
  planner.m_Client=&old;
  planner.PlanPathToNearestWithEntryPolicy(NULL,Vector3f(),goals,0,false);
  planner.GetPath(path);assert(planner.m_Client==NULL);
  /* The ordinary-entry contract still rejects a volume-only shortcut. */
  planner.m_Client=&old;prepareCalls=0;
  planner.PlanPathToNearestWithEntryPolicy(&active,Vector3f(),goals,0,true);
  assert(prepareCalls==1&&!planner.FoundGoal()&&planner.m_Client==&active);
  /* Outside radius and zero/multiple destinations continue through search. */
  for(int n=0;n<3;++n){
   DestinationVector ds=n==0?DestinationVector():goals;
   if(n==2)ds.push_back(goals[0]);
   prepareCalls=0;planner.m_Client=&old;
   planner.PlanPathToNearestWithEntryPolicy(&active,Vector3f(-1,0,0),ds,0,false);
   assert(prepareCalls==1&&planner.m_Client==&active&&!planner.FoundGoal());
  }
  /* A nonvalidated engine client and zero radius retain the same ownership. */
  active.validated=false;planner.m_Client=&old;
  goals[0].m_Radius=0;
  planner.PlanPathToNearestWithEntryPolicy(&active,goals[0].m_Position,goals,0,false);
  planner.GetPath(path);assert(planner.m_Client==&active&&path.count==0);
  std::puts("PASS: actual planner fastpath/GetPath; poisoned, deleted, NULL, ordinary and outside-goal clients");return 0;
 }catch(const std::exception&e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 7;}
}
