"""Typed physical DROP import/approach against the actual C slope geometry.

The ordinary node import and walk remain strict. Only an explicit new DROP
pose gets the separately proved coordinate adapter; no game is started.
"""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless

from test_standing_slope_native import program as slope_program
from test_traversal_reset_native import function

ROOT = Path(__file__).resolve().parents[2]


def program():
    code = slope_program().split('int main(', 1)[0]
    code = code.replace(function(code, 'G_Printf'), r'''
static char captured[8192];
static void G_Printf(const char *format,...) {
 va_list args;size_t length=strlen(captured);va_start(args,format);
 vsnprintf(captured+length,sizeof(captured)-length,format,args);va_end(args);
}
''')
    core = (ROOT / 'game/tce_navgen.c').read_text()
    external = (ROOT / 'game/tce_navgen_external.inc').read_text()
    code += '\n' + '\n'.join(function(core, name) for name in ('NG_Node', 'NG_ImportOrdinaryCopy'))
    code += '\n' + '\n'.join(function(external, name) for name in
                            ('NG_EditorJumpLaunchFloor', 'NG_EditorDropLaunchWalk', 'NG_ProbeImportDropLaunch'))
    return code + r'''
int main(int argc,char **argv) {
 ngNode node,original,exported,imported,source;vec3_t actual,anchor;const char *why;
 qboolean exact,accepted;float scale,offset;
 scenario=argc>1?atoi(argv[1]):0;
 ngWidth=14;ngStand=70;ngDuck=49;ngOrigin=24;ngTraceBudget=2000000;
 g_newbbox.integer=0;g_gravity.value=800;
 g_entities[100].inuse=1;g_entities[100].classname="trigger_hurt";
 if(scenario==1||scenario==17||scenario==28)slope_x=slope_y=0;
 if(scenario==2){slope_x=.2f;slope_y=0;}
 scale=sqrtf(1.f+slope_x*slope_x+slope_y*slope_y);
 VectorSet(actual,0,0,ngWidth*(scale-1.f)+.125f*scale+.125f);
 VectorSet(anchor,0,0,ngWidth*(fabsf(slope_x)+fabsf(slope_y))+.125f*scale+.125f);
 memset(&node,0,sizeof(node));node.id=42;node.authored=qtrue;node.editRole=9;node.editDropId=17;
 VectorCopy(actual,node.feet);VectorCopy(actual,node.raw);node.raw[2]+=ngOrigin;
 if(scenario==8)node.feet[2]-=.25f;
 if(scenario==17)node.feet[2]=1.5f;
 if(scenario==21)node.editRole=8;
 if(scenario==22){node.editRole=1;node.navFlags=1u<<27;}
 if(scenario==23)node.editDropId=-1;
 if(scenario==24)node.crouch=qtrue;
 if(scenario==25)node.feet[2]+=.5f;
 if(scenario==26)node.authored=qfalse;
 if(scenario==15)ngTraces=NG_TRACE_BUDGET;
 original=node;accepted=NG_EditorJumpLaunchFloor(&node);
 printf("drop capsule fixture%d accepted%d traces%d\n",scenario,accepted,ngTraces);
 if(scenario==0||scenario==1||scenario==18||scenario==27||scenario==28) {
  REQUIRE(accepted,"explicit physical DROP import accepted");
  REQUIRE(!memcmp(node.feet,original.feet,sizeof(vec3_t))&&node.editRole==9&&node.navFlags==0,
          "typed import preserves literal pose and action metadata");
 }else {
  REQUIRE(!accepted,"invalid or untyped physical launch must reject");
  return 0;
 }
 if(scenario==1)return 0;
 if(scenario==28) {
  source=node;source.feet[2]=.125f;captured[0]=0;
  REQUIRE(!NG_Walk(&source,&node,ngStand),"ordinary flat skin-height walk is valid");
  NG_ProbeImportDropLaunch(&source,&node,NULL);
  REQUIRE(strstr(captured,"action_import kind=drop_launch endpoint=0 accepted=1 exact=0 reason=accepted"),"flat source has distinct canonical physical pose");
  REQUIRE(strstr(captured,"action_walk kind=drop_launch endpoint=0 accepted=0 reason=typed_drop_launch_not_exact"),"typed walk cannot contradict nonexact same-reply import");
  REQUIRE(strstr(captured,"action_import kind=drop_launch endpoint=1 accepted=1 exact=1 reason=accepted"),"canonical flat target remains exact");
  REQUIRE(strstr(captured,"action_walk kind=drop_launch endpoint=1 accepted=1 reason=accepted"),"canonical target keeps checked body walk");
  return 0;
 }
 offset=anchor[2]-actual[2];REQUIRE(offset>1.8f&&offset<2.f,"real physical versus ordinary footbox offset");
 REQUIRE(NG_ImportOrdinaryCopy(actual,qfalse,&imported,&exact)&&!exact,"ordinary import still projects and is not exact");
 REQUIRE(!NG_Ground(actual,anchor,ngStand),"ordinary anchor obtained without changing stored action");
 exported=node;VectorCopy(anchor,exported.feet);
 why=NG_Walk(&exported,&node,ngStand);
 REQUIRE(why&&!strcmp(why,"different_floor"),"ordinary target comparison remains unchanged");
 REQUIRE(!NG_EditorDropLaunchWalk(&exported,&node,1,why),"explicit DROP target body path accepts physical alias");
 REQUIRE(NG_TrStandingLanding(actual),"same physical plane supports typed standing arrival");
 source=exported;source.feet[0]=-8;source.feet[2]-=8*slope_x;
 REQUIRE(NG_TrStandingExitContinuation(source.feet,actual),"standing physical target retains full supported continuation");
 node.editRole=1;
 REQUIRE(NG_EditorDropLaunchWalk(&exported,&node,1,why)!=NULL,"ordinary guide cannot borrow DROP adapter");
 node=original;
 REQUIRE(NG_EditorDropLaunchWalk(&exported,&node,1,"capsule_walk_sweep")!=NULL,"only different_floor can adapt");
 if(scenario==18) {
  scenario=20;why=NG_Walk(&source,&node,ngStand);
  REQUIRE(why&&NG_EditorDropLaunchWalk(&source,&node,1,why),"actual obstructed body path still rejects");
  return 0;
 }
 if(scenario==27) {
  ngNodes[0]=original;ngCount=1;ngProbe=qtrue;captured[0]=0;
  NG_ProbeImportDropLaunch(&exported,&original,"different_floor");
  REQUIRE(!memcmp(&ngNodes[0],&original,sizeof(original))&&ngCount==1&&ngProbe,"probe preserves graph and diagnostic mode");
  REQUIRE(strstr(captured,"action_import kind=drop_launch endpoint=0 accepted=1 exact=0 reason=accepted"),"footbox input suggests physical pose without exact claim");
  REQUIRE(strstr(captured,"action_import kind=drop_launch endpoint=1 accepted=1 exact=1 reason=accepted"),"fresh literal physical input has exact typed import");
  REQUIRE(strstr(captured,"action_walk kind=drop_launch endpoint=0 accepted=0"),"unphysical source cannot borrow the type");
  REQUIRE(strstr(captured,"action_walk kind=drop_launch endpoint=1 accepted=1 reason=accepted"),"only explicit target adapter certifies this directed query");
  printf("%s",captured);
 }
 return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class DropCapsuleLaunchNativeTests(TestCase):
    def test_actual_typed_import_adapter_and_readonly_records(self):
        scenarios = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                     16, 17, 18, 19, 21, 22, 23, 24, 25, 26, 27, 28]
        with tempfile.TemporaryDirectory(prefix='tce-drop-capsule-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\n'
                'project(DropCapsuleRegression LANGUAGES C)\nadd_executable(drop-capsule main.c)\n')
            options = dict(capture_output=True, text=True, timeout=90,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            executable = directory / 'build/Release/drop-capsule.exe'
            for scenario in scenarios:
                with self.subTest(scenario=scenario):
                    result = subprocess.run([str(executable), str(scenario)], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    main()
