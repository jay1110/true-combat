"""Real Client walk against independent slope/ceiling collision planes.

The fixture uses analytic expanded floor, ceiling and wall half-spaces. An
adversarial missing capsule-support observation separately checks that a foot
box is never sufficient. No gameplay or whole-map result is claimed.
"""
import os
import shutil
from unittest import TestCase, main, skipUnless
from test_runtime_walk_native import SHIM, CLIENT, ROOT, extract
from test_door_runtime_native import compile_run


def program():
    source = SHIM
    start = source.index('bool TraceLine(')
    end = source.index('\n}\n}\nclass Client', start) + 2
    trace = r'''
bool TraceLine(obTraceResult&r,const Vector3f&a,const Vector3f&b,const AABB*h,int mask,int,int){
 ++queries;const bool capsule=(mask&TR_MASK_NAVCAPSULE)!=0;
 if(capsule)++bodyqueries;else ++boxqueries;
 r=obTraceResult{};
 const float slope=.1f,normal=1.f/std::sqrt(1.f+slope*slope);
 // Bottom hemisphere of the real 16x87 hull versus the flat 32u foot box.
 const float support=capsule ? 16.f/normal+14.f : 30.f+16.f*slope;
 auto clip=[&](Vector3f n,float d){
  const float da=a.Dot(n)-d,db=b.Dot(n)-d;
  if(da<-.0001f){r.m_StartSolid=1;r.m_Fraction=0;}
  if(db<0&&db<da){const float t=da/(da-db);if(t<r.m_Fraction){
   r.m_Fraction=std::fmax(0.f,t);for(int j=0;j<3;j++)r.m_Normal[j]=(&n.x)[j];}}
 };
 // Scenario2 supplies a valid broad footprint but no actual body support:
 // this deliberately adversarial observation must never bridge a gap.
 if(!(scenario==2&&capsule&&b.x>=32&&b.x<=64))
  clip({-slope*normal,0,normal},support*normal);
 // 100u ceiling permits the real slope and native foot-anchor route, but
 // not the artificial full 18u rise used by the old horizontal-step proof.
 clip({0,0,-1},-((scenario==1?91.f:100.f)-h->m_Maxs[2]));
 if(scenario==3)clip({-1,0,0},-48.f); // Actual vertical obstruction.
 for(int j=0;j<3;j++)r.m_Endpos[j]=(&a.x)[j]+r.m_Fraction*((&b.x)[j]-(&a.x)[j]);
 return true;
}
'''
    source = source[:start] + trace + source[end:]
    client = CLIENT.read_text()
    helpers = '\n'.join(extract(client, name) for name in
        ('NavigationGroundContentsSafe', 'NavigationGroundSurfaceSafe',
         'TraceWalkTo', 'CanWalkTo', 'ResolveWalkDestination'))
    return source + (ROOT / 'game/tce_nav_water.h').read_text() + helpers + r'''
int main(){
 Client c;c.position={0,0,16.f*std::sqrt(1.01f)+14.f+.125f};
 const Vector3f goal(96,0,30.f+1.6f+9.6f+.125f);
 scenario=0;require(c.CanWalkTo(goal),"real sloping floor beneath low ceiling is reachable");
 require(boxqueries>0&&bodyqueries>boxqueries,"foot proposal and complete body support both queried");
 require(queries<=12*12+4,"one bounded alternative per eight-unit step");
 for(scenario=1;scenario<=3;++scenario){
  queries=bodyqueries=boxqueries=0;
  require(!c.CanWalkTo(goal),scenario==1?"actual low headroom rejects":
   scenario==2?"footbox without real capsule support rejects":"actual wall rejects");
  require(bodyqueries>0,"negative also uses complete body");
 }
 printf("actual Client grounded slope: safe low ceiling positive; true headroom, footprint-only unsupported gap and wall negatives passed\n");
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class GroundFirstWalkTests(TestCase):
    def test_full_body_supported_slope_avoids_artificial_ceiling_collision(self):
        compile_run(self, program())


if __name__ == '__main__':
    main()
