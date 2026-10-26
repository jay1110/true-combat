"""Run the real read-only jump diagnostic and prove state restoration."""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
try:
    from .test_standing_jump_native import program as standing_program
    from .test_traversal_reset_native import function, ROOT
except ImportError:
    from test_standing_jump_native import program as standing_program
    from test_traversal_reset_native import function, ROOT


def program():
    source = (ROOT / 'game/tce_navgen_traversal.inc').read_text()
    core = (ROOT / 'game/tce_navgen.c').read_text()
    text = standing_program().split('int main(int argc', 1)[0]
    quiet = 'static void G_Printf(const char *format,...){(void)format;}'
    assert quiet in text
    text = text.replace(quiet, 'static void G_Printf(const char *format,...){char output[240];va_list args;va_start(args,format);vsnprintf(output,sizeof(output),format,args);va_end(args);fputs(output,stdout);}')
    body = function(source, 'NG_TrProbeJumps')
    needed = set(re.findall(r'\bng\w+\b', body))
    declarations = [line for line in (core + '\n' + source).splitlines()
                    if re.match(r'^static [^;()]+;', line) and line not in text
                    and needed.intersection(re.findall(r'\bng\w+\b', line))]
    return text + '\n'.join(declarations) + body + r'''
int main(int argc,char **argv){
 ngNode from,to,original[2];vec3_t expectedPoint;int expectedTraces=9,expectedSteps=0;
 scenario=argc>1?atoi(argv[1]):0;g_newbbox.integer=1;g_gravity.value=800;
 ngWidth=16;ngStand=87;ngDuck=61;ngOrigin=30;ngTraceBudget=2000000;
 memset(&from,0,sizeof(from));memset(&to,0,sizeof(to));from.id=1;to.id=2;
 VectorSet(from.feet,0,0,.125f);VectorSet(to.feet,64,0,32.125f);
 if(scenario==7){VectorSet(from.feet,-131072,131072,.125f);VectorSet(to.feet,-131008,131072,32.125f);}
 if(scenario==8){VectorSet(from.feet,-131072,-131072,-131072);VectorSet(to.feet,-131008,-131072,-131040);}
 from.accepted=to.accepted=1;ngCount=2;ngNodes[0]=from;ngNodes[1]=to;
 memcpy(original,ngNodes,sizeof(original));ngConditionalGroundHits[2]=7;
 g_entities[100].inuse=1;g_entities[100].classname="trigger_hurt";
 ngTraces=expectedTraces;ngStepDiagnostics=expectedSteps;ngJumpArcFailure=12;ngProbe=qtrue;
 VectorSet(ngJumpArcFailurePoint,4,5,6);VectorCopy(ngJumpArcFailurePoint,expectedPoint);
 NG_TrProbeJumps(&from,&to,2000000);
 REQUIRE(ngTraces==expectedTraces&&ngStepDiagnostics==expectedSteps,"query and diagnostic accounting restored");
 REQUIRE(ngProbe==qtrue,"caller diagnostic-print mode restored");
 REQUIRE(ngJumpArcFailure==12&&VectorCompare(ngJumpArcFailurePoint,expectedPoint),"previous failure code and point restored");
 REQUIRE(ngConditionalGroundHits[2]==7,"conditional ground accounting restored");
 REQUIRE(ngCount==2&&!memcmp(original,ngNodes,sizeof(original))&&ngEdgeCount==0&&ngJumpProofCount==0,"no graph, edge or proof mutation");
 puts("jump probe preserved current graph and diagnostic state");return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class JumpProbeNativeTests(TestCase):
    def test_actual_current_world_probe_reports_physics_and_restores_state(self):
        with tempfile.TemporaryDirectory(prefix='tce-jump-probe-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(JumpProbe LANGUAGES C)\n'
                'add_executable(jump-probe main.c)\n')
            options = dict(capture_output=True, text=True, timeout=90,
                           creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for scenario in (0, 1, 3, 4, 7, 8):
                with self.subTest(scenario=scenario):
                    result = subprocess.run([str(directory / 'build/Release/jump-probe.exe'), str(scenario)], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    lines = [line[len('navgen probe jump '):] for line in result.stdout.splitlines()
                             if line.startswith('navgen probe jump ')]
                    self.assertEqual(len(lines), 4 if scenario == 0 else 3, result.stdout)
                    self.assertEqual(len(result.stdout.splitlines()), len(lines) + 1, result.stdout)
                    for line in lines:
                        self.assertLess(len(('navgen probe jump ' + line + '\n').encode('utf-8')), 210,
                                        'each complete JSON record must fit the actual per-G_Printf limit')
                    records = [json.loads(line) for line in lines]
                    report = {record['part']: record for record in records}
                    self.assertEqual(len(report), len(records))
                    self.assertTrue(report['points']['readonly'])
                    self.assertFalse(report['points']['live_verified'])
                    self.assertEqual(report['points']['coordinates'], 'feet')
                    self.assertEqual(report['standing']['accepted'], scenario == 0)
                    self.assertGreater(report['standing']['queries'], 0)
                    if scenario == 0:
                        self.assertFalse(report['running']['accepted'])
                        self.assertEqual(report['standing_proof']['samples'], 27)
                        self.assertLessEqual(report['standing_proof']['crash'], 38)
                    if scenario == 3:
                        self.assertEqual(report['standing']['reason'], 'hurt_trigger')
                    if scenario == 4:
                        self.assertEqual(report['standing']['reason'], 'liquid_hazard')


if __name__ == '__main__':
    main()
