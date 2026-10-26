"""Execute automatic anchor reuse with the real hull/ledge integrators.

One vertical platform is approached from two nearby floors. Both routes must
use the same directed launch, while distinct destinations, authored IDs and
flag variants remain independent. No game process or map fixture is created.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless

try:
    from .test_free_climb_native import program as collision_program
    from .test_traversal_reset_native import function
except ImportError:
    from test_free_climb_native import program as collision_program
    from test_traversal_reset_native import function

ROOT = (Path(__file__).resolve().parents[1] / "src")


def program():
    source = '\n'.join((ROOT / f'game/{name}').read_text() for name in
                       ('tce_navgen.c', 'tce_navgen_traversal.inc', 'tce_navgen_freeclimb.inc'))
    prefix = collision_program().split('int main(', 1)[0]
    names = ('NG_PairKey', 'NG_PairHash', 'NG_PairGet', 'NG_PairSet',
             'NG_TrMarkSeen', 'NG_TrSetAccepted', 'NG_TrAccepted', 'NG_TrLink',
             'NG_TrReuseOrdinaryFloor', 'NG_TrFrontNode', 'NG_TrReuseFreeClimb', 'NG_TrFreeClimbs')
    functions = [function(source, name) for name in names]
    needed = set(re.findall(r'\bng\w+\b', '\n'.join(functions)))
    declarations = [line for line in source.splitlines()
                    if re.match(r'^static [^;()]+;', line)
                    and needed.intersection(re.findall(r'\bng\w+\b', line))
                    and line not in prefix]
    edge = re.search(r'typedef struct\s*\{[^}]+\}\s*ngEdge;', source).group()
    return prefix + edge + '\n' + '\n'.join(declarations + functions) + r'''
int main(void) {
 int maxid=3,index,count,proofs;ngNode point,original[3];ngFreeClimbProof saved;
 g_newbbox.integer=1;ngWidth=16;ngStand=87;ngDuck=61;ngOrigin=30;
 g_gravity.value=800;ngTraceBudget=2000000;target_height=102;scenario=0;
 memset(ngNodes,0,sizeof(ngNodes));memset(ngEdges,0,sizeof(ngEdges));
 ngCount=3;ngEdgeCount=ngFreeClimbProofCount=0;
 VectorSet(ngNodes[0].feet,0,0,.125f);VectorSet(ngNodes[1].feet,-1,0,.125f);
 VectorSet(ngNodes[2].feet,64,0,102.125f);
 for(index=0;index<3;index++){ngNodes[index].id=index+1;ngNodes[index].accepted=1;ngSpawnReachable[index]=index<2;original[index]=ngNodes[index];}
 NG_TrFreeClimbs(3,&maxid);
 REQUIRE(ngFreeClimbProofCount==1,"two approaches must not duplicate one directed action");
 REQUIRE(ngCount==4&&ngEdgeCount==5,"one launch plus both approach/retreat routes");
 REQUIRE(NG_TrAccepted(0,3)&&NG_TrAccepted(3,0)&&NG_TrAccepted(1,3)&&NG_TrAccepted(3,1)&&NG_TrAccepted(3,2),"all directed routes preserved");
 REQUIRE(ngFreeClimbProofs[0].climbSeconds>0&&ngFreeClimbProofs[0].stamina<2490,"same recipe retains real native body/stamina receipt");
 REQUIRE(body_calls>100,"full capsule collision integrator executed");
 for(index=0;index<3;index++)REQUIRE(VectorCompare(original[index].feet,ngNodes[index].feet)&&original[index].id==ngNodes[index].id,"source coordinates and IDs unchanged");
 saved=ngFreeClimbProofs[0];point=ngNodes[3];count=ngCount;proofs=ngFreeClimbProofCount;
 REQUIRE(NG_TrReuseFreeClimb(&point,2)==3,"exact directed target reused");
 REQUIRE(NG_TrReuseFreeClimb(&point,1)==-1,"different target does not overwrite recipe");
 point.feet[1]+=.01f;REQUIRE(NG_TrReuseFreeClimb(&point,2)==-1,"nearby does not mean identical launch");
 point=ngNodes[3];ngNodes[3].authored=1;REQUIRE(NG_TrReuseFreeClimb(&point,2)==-1,"authored SourceID protected");ngNodes[3].authored=0;
 point.editFlags=1;REQUIRE(NG_TrReuseFreeClimb(&point,2)==-1,"candidate tactical flag variant protected");point.editFlags=0;
 ngNodes[3].editFlags=1;REQUIRE(NG_TrReuseFreeClimb(&point,2)==-1,"tactical flag variant protected");ngNodes[3].editFlags=0;
 ngNodes[3].accepted=0;REQUIRE(NG_TrReuseFreeClimb(&point,2)==-1,"rejected guide cannot be reused");ngNodes[3].accepted=1;
 REQUIRE(ngCount==count&&ngFreeClimbProofCount==proofs&&!memcmp(&saved,&ngFreeClimbProofs[0],sizeof(saved)),"failed reuse candidates do not mutate graph or proof");
 point=ngNodes[0];point.topologyProtected=0;
 REQUIRE(NG_TrReuseOrdinaryFloor(&point)==0,"grounded automatic floor reused with real body checks");
 REQUIRE(NG_TrFrontNode(&point,1,0,&maxid)==0&&ngCount==count,"front landing does not allocate duplicate ordinary point");
 ngNodes[0].authored=1;REQUIRE(NG_TrReuseOrdinaryFloor(&point)==-1,"authored ordinary SourceID protected");ngNodes[0].authored=0;
 ngNodes[0].navFlags=NG_NAV_CLIMB;REQUIRE(NG_TrReuseOrdinaryFloor(&point)==-1,"ordinary and ladder exit flag variants stay separate");ngNodes[0].navFlags=0;
 ngNodes[0].door.mode=1;REQUIRE(NG_TrReuseOrdinaryFloor(&point)==-1,"door recipe not merged");ngNodes[0].door.mode=0;
 point.feet[0]=64;point.feet[2]=102.125f;ngNodes[2].feet[2]=point.feet[2];
 scenario=2;REQUIRE(NG_TrReuseOrdinaryFloor(&point)==-1,"identical but ceiling-blocked floor still rejected");
 puts("automatic exact ordinary/action reuse, true ledge contacts, authored/flags/target/ceiling guards passed");return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class ActionAnchorReuseNativeTests(TestCase):
    def test_real_ledge_scanner_reuses_exact_action_and_ordinary_anchors(self):
        with tempfile.TemporaryDirectory(prefix='tce-action-reuse-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(ActionReuse LANGUAGES C)\n'
                'add_executable(action-reuse main.c)\n')
            options = dict(capture_output=True, text=True, timeout=90,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release'],
                            [str(directory / 'build/Release/action-reuse.exe')]):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    main()
