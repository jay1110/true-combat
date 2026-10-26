"""Actual compact climb-only world probe, with bounded formatter replies."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
try:
    from .test_free_climb_height_native import program as height_program
    from .test_traversal_reset_native import function, ROOT
except ImportError:
    from test_free_climb_height_native import program as height_program
    from test_traversal_reset_native import function, ROOT


def program():
    text = height_program().split('int main(void)', 1)[0]
    quiet = 'static void G_Printf(const char *format,...){(void)format;}'
    assert quiet in text
    text = text.replace(quiet, 'static void G_Printf(const char *format,...){char output[240];va_list args;va_start(args,format);vsnprintf(output,sizeof(output),format,args);va_end(args);fputs(output,stdout);}')
    body = function((ROOT / 'game/tce_navgen_freeclimb.inc').read_text(), 'NG_TrProbeFreeClimb')
    return text + body + r'''
int main(int argc,char **argv){
 ngNode from,to,original[2];int initialTraces=11,initialSteps=0;
 scenario=argc>1?atoi(argv[1]):0;rim_height=scenario==1?60:scenario==2?170:64;
 g_newbbox.integer=1;ngWidth=16;ngStand=87;ngDuck=61;ngOrigin=30;g_gravity.value=800;ngTraceBudget=2000000;
 memset(&from,0,sizeof(from));memset(&to,0,sizeof(to));from.accepted=to.accepted=1;
 VectorSet(from.feet,-64,0,.125f);VectorSet(to.feet,64,0,60.125f);
 if(scenario==7){VectorSet(from.feet,-131072,-131072,-131072);VectorSet(to.feet,-131008,-131072,-131012);}
 original[0]=from;original[1]=to;ngCount=2;ngEdgeCount=0;
 ngTraces=initialTraces;ngStepDiagnostics=initialSteps;ngProbe=qtrue;ngConditionalGroundHits[2]=7;
 g_entities[100].inuse=1;g_entities[100].classname="trigger_hurt";
 NG_TrProbeFreeClimb(&from,&to,2000000);
 REQUIRE(ngTraces==initialTraces&&ngStepDiagnostics==initialSteps&&ngProbe==qtrue,"all caller accounting and print state restored");
 REQUIRE(ngConditionalGroundHits[2]==7&&ngCount==2&&ngEdgeCount==0,"world support accounting and graph unchanged");
 REQUIRE(!memcmp(&from,&original[0],sizeof(from))&&!memcmp(&to,&original[1],sizeof(to)),"input nodes unchanged");
 puts("climb probe preserved current state");return 0;
}
'''


@skipUnless(os.name == 'nt' and shutil.which('cmake'), 'Windows CMake/MSVC required')
class FreeClimbProbeNativeTests(TestCase):
    def test_actual_projected_launch_proof_and_short_json_preserve_state(self):
        with tempfile.TemporaryDirectory(prefix='tce-climb-probe-') as folder:
            directory = Path(folder)
            (directory / 'main.c').write_text(program())
            (directory / 'CMakeLists.txt').write_text(
                'cmake_minimum_required(VERSION 3.15)\nproject(ClimbProbe LANGUAGES C)\nadd_executable(climb-probe main.c)\n')
            options = dict(capture_output=True, text=True, timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake', '-S', folder, '-B', str(directory / 'build'), '-A', 'Win32'],
                            ['cmake', '--build', str(directory / 'build'), '--config', 'Release']):
                result = subprocess.run(command, **options)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            for scenario in (0, 1, 2, 3, 4, 7):
                with self.subTest(scenario=scenario):
                    result = subprocess.run([str(directory / 'build/Release/climb-probe.exe'), str(scenario)], **options)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    lines = [line for line in result.stdout.splitlines() if line.startswith('navgen probe climb ')]
                    self.assertEqual(len(lines), 4 if scenario == 0 else 3, result.stdout)
                    self.assertEqual(len(result.stdout.splitlines()), len(lines)+1, result.stdout)
                    for line in lines:
                        self.assertLess(len((line+'\n').encode()), 210)
                    report = {record['part']: record for record in
                              (json.loads(line[len('navgen probe climb '):]) for line in lines)}
                    self.assertTrue(report['points']['readonly'])
                    self.assertEqual(report['points']['coordinates'], 'feet')
                    self.assertEqual(report['climb']['accepted'], scenario == 0)
                    self.assertFalse(report['climb']['live_verified'])
                    self.assertGreater(report['climb']['queries'], 0)
                    if scenario == 0:
                        self.assertTrue(report['launch']['world_face'])
                        self.assertTrue(report['launch']['walk'])
                        self.assertTrue(report['launch']['lip_gate'])
                        self.assertAlmostEqual(report['launch']['position'][0], 3.875, places=3)
                        self.assertEqual(report['proof']['samples'], 3)
                        self.assertLessEqual(report['proof']['crash'], 38)
                    if scenario == 1:
                        self.assertFalse(report['launch']['lip_gate'])
                        self.assertEqual(report['climb']['reason'], 'native_lip_gate_rejected')


if __name__ == '__main__':
    main()
