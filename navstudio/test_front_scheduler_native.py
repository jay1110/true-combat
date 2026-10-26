"""Exercise the full actual native front scheduler on independent geometry.

The first eligible raised floor has an impassable tall wall; a second lateral
front has a real standing jump and independent safe drop. All collision,
physics, connectivity and graph commit functions are production C bodies.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
try:
    from .test_front_jump_native import program as front_program
    from .test_traversal_reset_native import function, ROOT
except ImportError:
    from test_front_jump_native import program as front_program
    from test_traversal_reset_native import function, ROOT


def program(greedy_control=False):
    core = (ROOT / 'game/tce_navgen.c').read_text()
    traversal = (ROOT / 'game/tce_navgen_traversal.inc').read_text()
    source = core + '\n' + traversal
    text = front_program().split('int main(int argc', 1)[0]
    if greedy_control:
        assert '#define NG_FRONT_JUMP_QUANTUM 8' in text
        text = text.replace('#define NG_FRONT_JUMP_QUANTUM 8', '#define NG_FRONT_JUMP_QUANTUM 441')
    old = 'if(p[2]+low<support(p[0],p[1],width)-.000001f)'
    wall = '''if(scenario==8&&p[0]+width>0.f&&p[0]-width<9.f&&
    p[1]+width>-48.f&&p[1]-width<48.f&&p[2]+top>0.f&&p[2]+low<200.f){
    VectorSet(normal,-1,0,0);return 3;}\n '''
    assert old in text
    text = text.replace(old, wall + old, 1)
    names = ('NG_PairKey', 'NG_PairHash', 'NG_PairGet', 'NG_PairSet',
             'NG_TrMarkSeen', 'NG_TrSetAccepted', 'NG_TrAccepted', 'NG_TrSpecialInput',
             'NG_TrLink', 'NG_TrReuseOrdinaryFloor', 'NG_TrFrontNode',
             'NG_TrStrongComponents', 'NG_SpawnReachability', 'NG_TrFrontAccountArc',
             'NG_TrFrontAdvance', 'NG_TrFrontExpand', 'NG_TrFrontCommit', 'NG_TrFrontJumps')
    functions = [function(source, name) for name in names]
    needed = set(re.findall(r'\bng\w+\b', '\n'.join(functions)))
    declarations = [line for line in source.splitlines()
                    if re.match(r'^static [^;()]+;', line) and line not in text
                    and needed.intersection(re.findall(r'\bng\w+\b', line))]
    structures = [re.search(r'typedef struct\s*\{[^}]+\}\s*ngFrontState;', source).group()]
    text += (ROOT / 'game/tce_nav_graph.h').read_text()
    return text + '\n'.join(structures + declarations + functions) + r'''
int main(int argc,char **argv){
 int i,maxid=4;scenario=8;target_height=20;
 g_newbbox.integer=argc>1?atoi(argv[1]):0;
 ngWidth=g_newbbox.integer?16.f:14.f;ngStand=g_newbbox.integer?87.f:70.f;
 ngDuck=g_newbbox.integer?61.f:49.f;ngOrigin=g_newbbox.integer?30.f:24.f;g_gravity.value=800;
 ngTraceBudget=2000000;ngCount=4;ngExternalSpatialImport=1;
 VectorSet(ngNodes[0].feet,-76,0,.125f);VectorSet(ngNodes[1].feet,42,0,20.125f);
 VectorSet(ngNodes[2].feet,-76,-128,.125f);VectorSet(ngNodes[3].feet,42,-128,20.125f);
 for(i=0;i<4;i++){
  ngNodes[i].id=i+1;ngNodes[i].accepted=1;ngNodes[i].radius=20;
  ngNodes[i].reason=i%2?"external_native_validated_floor":"map_entity_spawn_seed";
  ngNodes[i].topologyProtected=i%2;
 }
 NG_SpawnReachability();NG_TrFrontJumps(4,&maxid);
 printf("scheduler cases %d arcs %d queries %d connected %d stop %s\n",ngFrontJumpCaseCount,ngFrontJumpArcChecks,ngFrontJumpTraces,ngFrontJumpConnected,ngFrontJumpStop);
 for(i=0;i<ngFrontJumpCaseCount;i++)printf("case %d low %d high %d up %d accepted %d drop %d reason %s\n",i,ngFrontJumpCases[i].low,ngFrontJumpCases[i].high,ngFrontJumpCases[i].upChecks,ngFrontJumpCases[i].upAccepted,ngFrontJumpCases[i].dropChecks,ngFrontJumpCases[i].reason);
 REQUIRE(ngFrontJumpCaseCount>=2,"second distinct front is scheduled");
 REQUIRE(ngFrontJumpCases[0].low==2&&ngFrontJumpCases[0].high==3&&ngFrontJumpCases[0].portalClear,"body-clear lateral front ranked before blocked closest front");
 for(i=0;i<ngFrontJumpCaseCount;i++)if(ngFrontJumpCases[i].low==0&&ngFrontJumpCases[i].high==1)
  REQUIRE(ngFrontJumpCases[i].upAccepted==0,"retained tall-wall fallback never becomes an accepted jump");
 REQUIRE(ngSpawnReachable[3]&&ngFrontJumpConnected>=1,"lateral front reached under same global budget");
 REQUIRE(ngFrontJumpTraces<24000&&ngFrontJumpArcChecks<=256,"unchanged global collision and action bounds");
 REQUIRE(ngJumpProofCount>0&&ngDropProofCount>0,"no one-sided pending proof promoted to graph");
 REQUIRE((!ngJumpProofs[0].standing||ngJumpProofs[0].crash<=38)&&ngDropProofs[0].crashDelta<=38,"actual native jump and independent harmless drop receipts");
 if(!ngJumpProofs[0].standing)REQUIRE(NG_TrJump(&ngNodes[ngJumpProofs[0].launch],&ngNodes[ngJumpProofs[0].landing]),"generated running receipt independently repeats actual hull arc");
 REQUIRE(sizeof(ngFrontState)*NG_FRONT_JUMP_CASES<600000,"bounded transient scheduler storage");
 {
  ngFrontState state;ngFrontJumpCase entry;vec3_t forward;qboolean lowLaunch[21];int iteration;
  memset(&state,0,sizeof(state));memset(&entry,0,sizeof(entry));state.pendingArc=-1;scenario=0;
  ngTraces=ngFrontJumpArcChecks=ngJumpProofCount=ngDropProofCount=ngEdgeCount=0;ngCount=2;
  VectorSet(ngNodes[0].feet,-76,0,.125f);VectorSet(ngNodes[1].feet,42,0,20.125f);
  VectorSet(forward,1,0,0);state.lc=NG_TrFrontSamples(0,forward,state.low,24000,&entry);
  VectorSet(forward,-1,0,0);state.hc=NG_TrFrontSamples(1,forward,state.high,24000,&entry);
  for(i=0;i<state.lc;i++)lowLaunch[i]=NG_TrLaunchSeparated(&state.low[i]);
  for(i=0;i<state.hc;i++)state.highLaunch[i]=NG_TrLaunchSeparated(&state.high[i]);
  state.arcCount=NG_TrFrontPairs(state.low,state.lc,state.high,state.hc,lowLaunch,&ngNodes[0],&ngNodes[1],state.arcs);
  REQUIRE(!NG_TrFrontAdvance(&state,&entry,24000,1)&&state.pendingArc>=0&&entry.upAccepted==1&&entry.dropChecks==0,"accepted ascent paused exactly at quantum");
  REQUIRE(ngCount==2&&ngEdgeCount==0&&ngJumpProofCount==0&&ngDropProofCount==0,"partial upward receipt never changes graph");
  for(iteration=0;iteration<256&&!state.done;iteration++)NG_TrFrontAdvance(&state,&entry,24000,1);
  REQUIRE(state.found&&entry.dropAccepted==1&&state.jump.standing&&state.drop.duration>0,"pending exact standing receipt survives resume until independent drop accepted");
  REQUIRE(ngCount==2&&ngEdgeCount==0&&ngJumpProofCount==0&&ngDropProofCount==0,"advance alone does not fabricate graph commit");
 }
 return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class FrontSchedulerNativeTests(TestCase):
    def test_full_native_scheduler_falls_back_from_blocked_first_front(self):
        with tempfile.TemporaryDirectory(prefix='tce-front-schedule-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'greedy.c').write_text(program(greedy_control=True))
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(FrontScheduler LANGUAGES C)\n'
                'add_executable(front-scheduler main.c)\nadd_executable(greedy-control greedy.c)\n')
            options = dict(capture_output=True, text=True, timeout=90,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for profile in (0, 1):
                with self.subTest(profile=profile):
                    result = subprocess.run([str(directory / 'build/Release/front-scheduler.exe'), str(profile)], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(directory / 'build/Release/greedy-control.exe')], **options)
            # Portal ordering now protects the legal front even from the old
            # greedy quantum: both controls must still obey actual geometry.
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    main()
