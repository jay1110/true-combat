"""Actual native portal-first jump discovery on independent wagon brushes.

The measured Railhouse access supplies only this fixture's dimensions. No map
coordinates, accepted actions or physics results are injected into production.
The native floor walks, running/standing arcs, three-speed drop, SCC selection
and commit all execute unchanged. This is not an ET-engine collision substitute.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
try:
    from .test_front_scheduler_native import program as scheduler_program
except ImportError:
    from test_front_scheduler_native import program as scheduler_program


def program():
    text = scheduler_program().split('int main(int argc', 1)[0]
    marker = 'if(p[2]+low<support(p[0],p[1],width)-.000001f)'
    # Low platform ends at0; raised wagon floor starts at9 and rises20. The
    # actual doorway spans120 and is113.75 above the upper floor. Horizontal
    # axes are rotated/translated from the game so no hardcoded source search
    # can satisfy this fixture accidentally.
    brushes = r'''
 if((scenario==9||scenario==10)&&p[0]+width>9.f&&p[0]-width<19.f&&
    p[2]+top>20.f&&p[2]+low<134.f&&
    (scenario==10||p[1]-width<-52.f||p[1]+width>68.f)){
    VectorSet(normal,-1,0,0);return 3;}
 if((scenario==9||scenario==10)&&p[0]+width>9.f&&p[0]-width<19.f&&
    p[2]+top>134.f&&p[2]+low<138.f){VectorSet(normal,0,0,-1);return 2;}
 '''
    assert marker in text
    text = text.replace(marker, brushes + marker, 1)
    return text + r'''
int main(int argc,char **argv){
 int i,maxid=4,mode=argc>1?atoi(argv[1]):9;ngNode explicitLow,explicitHigh;
 scenario=mode;target_height=20;g_newbbox.integer=1;
 ngWidth=16;ngStand=87;ngDuck=61;ngOrigin=30;g_gravity.value=800;
 ngTraceBudget=2000000;ngCount=4;ngExternalSpatialImport=1;
 VectorSet(ngNodes[0].feet,-76,-144,.125f);VectorSet(ngNodes[1].feet,42,-140,20.125f);
 VectorSet(ngNodes[2].feet,-104.165f,-16,.125f);VectorSet(ngNodes[3].feet,117.5376f,-16,20.125f);
 for(i=0;i<4;i++){
  ngNodes[i].id=i+1;ngNodes[i].accepted=1;ngNodes[i].radius=20;
  ngNodes[i].reason=i%2?"external_native_validated_floor":"map_entity_spawn_seed";
 }
 ngNodes[1].topologyProtected=1;
 REQUIRE(!NG_Walk(&ngNodes[1],&ngNodes[3],ngStand)&&!NG_Walk(&ngNodes[3],&ngNodes[1],ngStand),"actual ordinary upper walks join mission contact and central entrance");
 REQUIRE(!NG_Walk(&ngNodes[0],&ngNodes[2],ngStand)&&!NG_Walk(&ngNodes[2],&ngNodes[0],ngStand),"actual connected lower approach floor");
 NG_TrLink(0,2,"external_native_validated_walk");NG_TrLink(2,0,"external_native_validated_walk");
 NG_TrLink(1,3,"external_native_validated_walk");NG_TrLink(3,1,"external_native_validated_walk");
 NG_SpawnReachability();
 explicitLow=ngNodes[2];explicitHigh=ngNodes[3];
 VectorSet(explicitLow.feet,-36,0,.125f);VectorSet(explicitHigh.feet,36,0,20.125f);
 if(mode==9){
  ngJumpProof standing;ngDropProof drop;
  REQUIRE(NG_TrJump(&explicitLow,&explicitHigh),"independent exact running arc through middle doorway");
  REQUIRE(!NG_TrStandingJump(&explicitLow,&explicitHigh,24000,&standing),"higher stopped arc genuinely collides with lintel");
  REQUIRE(NG_TrWalkDrop(&explicitHigh,&explicitLow,24000,&drop)&&drop.crashDelta<=38,"independently proved three-speed harmless return");
 }
 ngTraces=0;NG_TrFrontJumps(4,&maxid);
 printf("portal mode %d cases %d queries %d arcs %d paired %d\n",mode,ngFrontJumpCaseCount,ngFrontJumpTraces,ngFrontJumpArcChecks,ngFrontJumpConnected);
 for(i=0;i<ngFrontJumpCaseCount;i++)printf("front %d low %d high %d clear %d lane %d first %d accepted %d samples %d/%d reason %s\n",i,ngFrontJumpCases[i].low,ngFrontJumpCases[i].high,ngFrontJumpCases[i].portalClear,ngFrontJumpCases[i].portalLane,ngFrontJumpCases[i].portalFirst,ngFrontJumpCases[i].upAccepted,ngFrontJumpCases[i].lowSamples,ngFrontJumpCases[i].highSamples,ngFrontJumpCases[i].reason);
 REQUIRE(ngFrontJumpTraces<24000&&ngFrontJumpArcChecks<=256&&ngFrontJumpCaseCount<=16,"original global budgets unchanged");
 if(mode==9){
  REQUIRE(ngSpawnReachable[1]&&ngSpawnReachable[3],"ordinary doorway connects protected mission SCC");
  REQUIRE(ngFrontJumpConnected>0&&ngJumpProofCount>0&&ngDropProofCount>0,"actual paired native receipts and graph commit");
  REQUIRE(!ngJumpProofs[0].standing,"generator selects running jump under real low lintel");
  REQUIRE(ngFrontJumpCases[0].portalClear&&ngFrontJumpCases[0].portalFirst,"open portal precedes closer solid west wall");
  REQUIRE(ngFrontJumpTraces<12000,"free lane found before expensive full blocked-front scans");
 }else{
  REQUIRE(!ngSpawnReachable[1]&&!ngSpawnReachable[3]&&!ngFrontJumpConnected,"sealed doorway ranking cannot waive actual native wall");
  REQUIRE(!ngJumpProofCount&&!ngDropProofCount,"no unproved action receipt or graph mutation");
 }
 return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class FrontPortalNativeTests(TestCase):
    def test_actual_running_portal_and_sealed_wall_under_original_budgets(self):
        with tempfile.TemporaryDirectory(prefix='tce-front-portal-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(FrontPortal LANGUAGES C)\n'
                'add_executable(front-portal main.c)\n')
            options = dict(capture_output=True, text=True, timeout=90,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for mode in (9, 10):
                with self.subTest(mode=mode):
                    result = subprocess.run([str(directory / 'build/Release/front-portal.exe'), str(mode)], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    main()
