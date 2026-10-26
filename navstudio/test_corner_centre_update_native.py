"""Actual FollowPath::Update: bounded centring after a blocked corner sweep.

Uses the live Delta coordinates, with independently supplied collision/ground
observations and actor positions. This proves controller decisions across real
Update frames; it does not claim to simulate ET movement or map collision.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_door_runtime_native import compile_run
from test_door_update_native import program as door_program


def program():
    source = door_program().split('int main(){', 1)[0]
    old = 'bool CanWalkTo(const Vector3f&p,NavFlags=0){if(approachBlocked&&p.x==0)return false;if(egressBlocked&&p.x==256)return false;return p.x<100||p.y>100||passable;}'
    assert old in source
    source = source.replace(old, r'''
 Vector3f velocity;bool centreClear=true,nextClear=false,requireExactCentre=false;
 int centreChecks=0,nextChecks=0;
 bool CanWalkTo(const Vector3f&p,NavFlags=0){
  if(p.x==1486.f){++centreChecks;return centreClear;}
  ++nextChecks;
  return nextClear||(requireExactCentre&&(position-Vector3f(1486,1686,12.25f)).Flatten().SquaredLength()<=.125f*.125f);
 }
''')
    # This controller fixture supplies walk-to-centre/next observations, not
    # collisions. Current grounded support is an independent observation and
    # must not be confused with its intentionally rejected outgoing walk.
    source = source.replace('bool ResolveWalkDestination(const Vector3f&p,float,NavFlags f,Vector3f&out){out=p;return CanWalkTo(p,f);}',
        'bool ResolveWalkDestination(const Vector3f&p,float r,NavFlags f,Vector3f&out){out=p;if(r==.25f&&(p-position).SquaredLength()==0)return true;return CanWalkTo(p,f);}')
    source = source.replace('Vector3f GetVelocity(){return{};}', 'Vector3f GetVelocity(){return velocity;}')
    source = source.replace('struct Steering {Vector3f target;',
                            'struct Steering {Vector3f target;float radius=0,input=1;')
    source = source.replace('void SetTarget(const Vector3f&p,float=0,int=0,bool=false,float=1){target=p;}',
                            'void SetTarget(const Vector3f&p,float r=0,int=0,bool=false,float s=1){target=p;radius=r;input=s;}')
    return source + r'''
void setupCorner(FollowPath&f,User&u){
 f.client.position={1486.155f,1684.558f,12.125f};
 f.m_CurrentPath.AddPt({1486,1686,12.25f},60).PlayerOrigin().NavId(439);
 f.m_CurrentPath.AddPt({1408,1773.62f,12.25f},2).PlayerOrigin().NavId(82);
 f.m_Query.m_User=&u;
}
bool logContains(const char*s){for(const auto&line:logs)if(line.find(s)!=String::npos)return true;return false;}
int main(){
 // Real reported 1.4503u distance must not be called an exact-centre failure.
 static FollowPath reachable;User owner;setupCorner(reachable,owner);
 reachable.client.requireExactCentre=true;
 frame(reachable);
 assert(reachable.failures==0&&reachable.m_CurrentPath.GetCurrentPtIndex()==0);
 assert(reachable.client.centreChecks==1&&reachable.client.nextChecks==1);
 assert(reachable.client.steer.radius==0.f&&reachable.client.steer.input<.125f);
 assert(reachable.client.steer.target.x==1486&&reachable.client.steer.target.y==1686);
 reachable.client.position={1486.1f,1685.1f,12.125f};frame(reachable);
 reachable.client.position={1486.04f,1685.7f,12.125f};frame(reachable);
 assert(reachable.failures==0&&reachable.m_CurrentPath.GetCurrentPtIndex()==0);
 reachable.client.position={1486,1686,12.125f};frame(reachable);
 assert(reachable.m_CurrentPath.GetCurrentPtIndex()==1&&reachable.failures==0&&owner.successes==0);
 assert(reachable.client.nextChecks==4&&reachable.client.centreChecks==3);
 assert(reachable.m_TceCentreNavId==0&&logContains("CENTRE_CLEAR"));
 reachable.client.position={1408,1773.62f,12.25f};frame(reachable);
 assert(owner.successes==1&&reachable.failures==0);

 // No unchecked move even at 1.45u when current centre itself is unreachable.
 static FollowPath blocked;User blockedOwner;setupCorner(blocked,blockedOwner);
 blocked.client.centreClear=false;frame(blocked);
 assert(blocked.failures==1&&blocked.m_PathStatus==FollowPath::PathFinished);
 assert(blocked.m_CurrentPath.GetCurrentPtIndex()==0&&blockedOwner.successes==0);
 assert(blocked.client.steer.target.y==blocked.client.position.y);
 assert(logContains("reason=centre_unreachable"));

 // A floor disappearing during the approach fails on the next actual query.
 static FollowPath lostFloor;User lostOwner;setupCorner(lostFloor,lostOwner);frame(lostFloor);
 lostFloor.client.position={1486.1f,1685.1f,12.125f};lostFloor.client.centreClear=false;frame(lostFloor);
 assert(lostFloor.failures==1&&lostOwner.successes==0&&lostFloor.client.nextChecks==2);

 // A persistent missing next floor is not bypassed after reaching the centre.
 static FollowPath missing;User missingOwner;setupCorner(missing,missingOwner);frame(missing);
 missing.client.position={1486,1686,12.125f};frame(missing);
 assert(missing.failures==0);frame(missing);assert(missing.failures==0);frame(missing);
 assert(missing.failures==1&&missingOwner.successes==0&&missing.m_CurrentPath.GetCurrentPtIndex()==0);
 assert(logContains("reason=centre_settled"));

 // A valid centre that the actor never reaches has a fixed wall-clock bound.
 static FollowPath stationary;User stationaryOwner;setupCorner(stationary,stationaryOwner);
 for(int i=0;i<30&&stationary.failures==0;++i)frame(stationary);
 assert(stationary.failures==1&&stationary.client.nextChecks<=27&&stationaryOwner.successes==0);
 assert(stationary.client.resets==0&&logContains("reason=centre_timeout"));

 // High frame rates also have a hard attempt budget; moving residual velocity
 // cannot suppress failure by repeatedly resetting the true-centre still timer.
 static FollowPath repeated;User repeatedOwner;setupCorner(repeated,repeatedOwner);
 for(int i=0;i<70&&repeated.failures==0;++i){++IGame::time;repeated.Update(.001f);}
 assert(repeated.failures==1&&repeated.client.nextChecks==64&&repeatedOwner.successes==0);

 // Snow's independent observed trajectory: opposite momentum first moves
 // from .645u to 5.8u, then the slow input converges over 2.85s. Every frame
 // must recheck both walks. The next leg opens at .114u, never by timeout.
 static FollowPath snow;User snowOwner;setupCorner(snow,snowOwner);
 snow.client.requireExactCentre=true;snow.client.position={1486.645f,1686,12.125f};frame(snow);
 const float braking[]={3.55f,5.22f,5.8f,5.38f,4.97f};
 for(float d:braking){snow.client.position.x=1486+d;frame(snow);assert(snow.failures==0);}
 for(int i=1;i<=46;++i){
  const float d=4.97f-(4.97f-.270f)*i/46.f;
  snow.client.position.x=1486+d;frame(snow);assert(snow.failures==0&&snowOwner.successes==0);
 }
 for(float d:{.230f,.200f,.170f,.140f,.114f}){
  snow.client.position.x=1486+d;frame(snow);assert(snow.failures==0);
 }
 assert(snow.m_CurrentPath.GetCurrentPtIndex()==1&&snow.client.resets==1&&snowOwner.successes==0);
 assert(snow.client.nextChecks==57&&snow.client.centreChecks==56);

 // Alternating offsets do not repeatedly refresh a decreasing watermark.
 static FollowPath oscillating;User oscillatingOwner;setupCorner(oscillating,oscillatingOwner);frame(oscillating);
 for(int i=0;i<45&&oscillating.failures==0;++i){
  oscillating.client.position={1486+(i%2?.8f:1.2f),1686,12.125f};frame(oscillating);
 }
 assert(oscillating.failures==1&&oscillatingOwner.successes==0&&oscillating.client.resets==0);

 // Even genuine gradual improvement cannot extend a single centre episode
 // beyond three seconds when the next floor remains unavailable.
 static FollowPath slow;User slowOwner;setupCorner(slow,slowOwner);frame(slow);
 for(int i=0;i<65&&slow.failures==0;++i){
  slow.client.position={1486+(i<5?4.f:4.f-(i-5)*.03f),1686,12.125f};frame(slow);
 }
 assert(slow.failures==1&&slowOwner.successes==0&&slow.client.nextChecks<=61&&slow.client.resets==0);

 // If the next native sweep clears before full centring, consume only after
 // that fresh success, never require a fabricated exact-position teleport.
 static FollowPath clears;User clearsOwner;setupCorner(clears,clearsOwner);frame(clears);
 clears.client.nextClear=true;frame(clears);
 assert(clears.failures==0&&clears.m_CurrentPath.GetCurrentPtIndex()==1&&clears.m_TceCentreNavId==0);
 printf("actual complete Update: Delta 1.4503u corner, fresh centre/next guards, lost floor, true-centre failure, time and attempt bounds passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class CornerCentreUpdateTests(TestCase):
    def test_actual_update_centres_only_on_a_valid_segment_and_never_bypasses_next_sweep(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
