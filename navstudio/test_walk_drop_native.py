"""Actual production drop/lip validation with independent swept floor fixtures.

These are native C regressions, not ET capsule parity or live map certification.
The source functions execute unchanged; fixtures provide only collision planes.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
from test_standing_jump_native import SHIM
from test_traversal_reset_native import function

ROOT = (Path(__file__).resolve().parents[1] / "src")


def program():
    source = '\n'.join((ROOT / ('game/' + name)).read_text() for name in
                       ('tce_navgen.c', 'tce_navgen_traversal.inc', 'tce_navgen_external.inc'))
    names = ('NG_Trace', 'NG_Hurt', 'NG_StaticGround', 'NG_ProbeBodyRepresentations',
             'NG_OrdinarySurface', 'NG_Ground', 'NG_Walk', 'NG_TrClear',
             'NG_TrGravity', 'NG_TrStableLanding', 'NG_TrGentlePlaneSupport', 'NG_TrPhysicalGroundAlias', 'NG_TrCentreSupported',
             'NG_TrExitContinuation', 'NG_TrSafeDropHeight', 'NG_TrDropGroundClip', 'NG_TrWalkDropSpeed',
             'NG_TrWalkDrop', 'NG_TrCommitManualDrop', 'NG_EditorJumpLaunchFloor')
    functions = [function(source, name) for name in names]
    needed = set(re.findall(r'\bng\w+\b', '\n'.join(functions))) | {'ngTraceBudget'}
    declarations = [line for line in source.splitlines()
                    if re.match(r'^static [^;()]+;', line)
                    and needed.intersection(re.findall(r'\bng\w+\b', line))]
    structures = [text for text in re.findall(r'typedef struct\s*\{[^}]+\}\s*\w+;', source, re.S)
                  if text.rsplit('}', 1)[1].strip(' ;') in ('ngDoorRecipe', 'ngNode', 'ngEdge', 'ngDropProof')]
    defines = '\n'.join(line for line in source.splitlines() if re.match(r'^#define NG_\w+\s', line))
    header = (ROOT / 'game/tce_nav_water.h').read_text()
    shim = SHIM.replace('if(x+halfwidth>=platform_edge)return target_height;',
                        'if(x-halfwidth<platform_edge)return target_height;')
    return shim + defines + '\n' + header + '\n' + '\n'.join(structures + declarations + functions) + r'''
int main(int argc,char **argv){
 ngNode a,b;ngDropProof proof;ngEdge edge;int accepted;
 scenario=argc>1?atoi(argv[1]):0;
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));memset(&edge,0,sizeof(edge));
 g_newbbox.integer=1;ngWidth=16;ngStand=87;ngDuck=61;ngOrigin=30;
 g_gravity.value=800;g_developer.integer=0;ngTraceBudget=2000000;
 target_height=102.f;platform_edge=24.f;
 g_entities[100].inuse=1;g_entities[100].classname="trigger_hurt";
 VectorSet(a.feet,0,0,102.125f);VectorSet(b.feet,128,0,.125f);
 a.accepted=b.accepted=qtrue;a.authored=qtrue;a.id=6841;b.id=1521;
 if(scenario==7){target_height=160;VectorSet(a.feet,0,0,160.125f);}
 if(scenario==5)platform_edge=-100;
 if(scenario==6){
  // A capsule/foot hull rests on the roof lip, while its centre is over air.
  a.editRole=1;a.navFlags=1u<<27;VectorSet(a.feet,30,0,102.125f);
  REQUIRE(NG_EditorJumpLaunchFloor(&a),"unchanged generated jump lip is capsule grounded");
  a.editRole=9;a.navFlags=0;a.editDropId=1521;
  REQUIRE(NG_EditorJumpLaunchFloor(&a),"explicit generated drop lip uses the exact same PM capsule support");
  REQUIRE(!strcmp(a.reason,"manual_drop_launch_capsule_ground"),"drop launch remains a distinct directed action");
  a.editRole=8;
  REQUIRE(NG_EditorJumpLaunchFloor(&a),"existing authored drop uses the same exact support without guide regeneration");
  a.editRole=1;
  REQUIRE(!NG_EditorJumpLaunchFloor(&a),"ordinary new guide has no action-ground exception");
  a.editRole=9;
  a.editDropId=-1;
  REQUIRE(!NG_EditorJumpLaunchFloor(&a),"drop lip without explicit landing target rejected");
  a.editDropId=1521;a.navFlags=512;
  REQUIRE(!NG_EditorJumpLaunchFloor(&a),"conflicting ladder/drop guide cannot obtain support exemption");
  a.navFlags=0;
  VectorSet(a.feet,45,0,102.125f);
  REQUIRE(!NG_EditorJumpLaunchFloor(&a),"airborne guide cannot claim lip support");
  a.editRole=1;a.navFlags=1u<<27;
  REQUIRE(!NG_EditorJumpLaunchFloor(&a),"airborne generated jump still rejected");
  printf("scenario6 actual capsule-supported lip and airborne rejection passed\n");return 0;
 }
 accepted=NG_TrWalkDrop(&a,&b,2000000,&proof);
 printf("scenario%d drop%d reason%s traces%d\n",scenario,accepted,ngDropFailure,ngTraces);
 if(!scenario){
  REQUIRE(accepted,"102-unit no-jump drop below actual damage threshold");
  REQUIRE(proof.crashDelta<=38&&proof.impact>0,"three-speed actual harmless contacts");
  REQUIRE(fabs(NG_TrSafeDropHeight()-148.4375f)<.001,"actual gravity/body damage bound");
  ngNodes[0]=a;ngNodes[1]=b;ngNodes[0].radius=ngNodes[1].radius=35;edge.from=0;edge.to=1;
  REQUIRE(NG_TrCommitManualDrop(&edge,&proof),"commit only exact directed drop target");
  REQUIRE(ngNodes[0].editRole&8,"directed native drop role");
  REQUIRE(ngNodes[0].editDropId==1521&&ngNodes[0].navFlags==0,"no invented reverse or JUMP flags");
  REQUIRE(ngDropProofCount==1&&ngDropProofs[0].launch==0&&ngDropProofs[0].landing==1,"numeric proof retained");
  edge.to=2;ngNodes[2]=b;ngNodes[2].id=99;
  REQUIRE(!NG_TrCommitManualDrop(&edge,&proof),"conflicting second landing fails closed");
 }else REQUIRE(!accepted,"wall/ceiling/hurt/water/unsupported/damaging drop must reject");
 return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class WalkDropNativeTests(TestCase):
    def test_actual_integrator_harmless_drop_hazards_and_generated_lip_reimport(self):
        with tempfile.TemporaryDirectory(prefix='tce-drop-regression-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\n'
                                                    'project(DropRegression LANGUAGES C)\nadd_executable(drop main.c)\n')
            options = dict(capture_output=True, text=True, timeout=90, creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake','-S',folder,'-B',str(directory/'build'),'-A','Win32'],
                            ['cmake','--build',str(directory/'build'),'--config','Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for scenario in range(8):
                with self.subTest(scenario=scenario):
                    result = subprocess.run([str(directory/'build/Release/drop.exe'),str(scenario)], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__': main()
