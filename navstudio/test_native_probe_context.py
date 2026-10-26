"""Native same-response context and unchanged small/large-hull drop proofs."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
from test_traversal_reset_native import function
from test_walk_drop_native import program as drop_program

ROOT=(Path(__file__).resolve().parents[1] / "src")


def compile_native(test,program,stem):
    temporary=tempfile.TemporaryDirectory(prefix='tce-probe-context-',dir=ROOT.parent/'build')
    test.addCleanup(temporary.cleanup);directory=Path(temporary.name)
    (directory/'main.c').write_text(program)
    (directory/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(Context LANGUAGES C)\nadd_executable('+stem+' main.c)\ntarget_compile_definitions('+stem+' PRIVATE GAMEDLL)\n')
    options=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
    for command in(['cmake','-S',str(directory),'-B',str(directory/'build'),'-A','Win32'],
                   ['cmake','--build',str(directory/'build'),'--config','Release']):
        result=subprocess.run(command,**options);test.assertEqual(result.returncode,0,result.stdout+result.stderr)
    return directory/'build/Release'/ (stem+'.exe'),options


@skipUnless(os.name=='nt'and shutil.which('cmake'),'Windows native compiler required')
class NativeProbeContextTests(TestCase):
    def test_actual_context_matches_map_gt_body_and_effective_gravity_without_state_changes(self):
        core=(ROOT/'game/tce_navgen.c').read_text();traversal=(ROOT/'game/tce_navgen_traversal.inc').read_text()
        program=r'''
#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "@HEADER@"
#undef vsnprintf
level_locals_t level;vmCvar_t g_gametype,g_newbbox,g_developer,g_gravity;
static float ngWidth,ngStand,ngDuck,ngOrigin;
static level_locals_t before;static vmCvar_t vars[4];
void QDECL G_Printf(const char *format,...) {
 char text[1024];va_list args;va_start(args,format);vsnprintf(text,sizeof(text),format,args);va_end(args);
 assert(strlen(text)<sizeof(text)-1);fputs(text,stdout);
}
'''.replace('@HEADER@',(ROOT/'game/g_local.h').as_posix())+function(traversal,'NG_TrGravity')+'\n'+function(core,'NG_ProbeContext')+r'''
int main(int argc,char **argv) {
 assert(argc==6);assert(strlen(argv[1])<sizeof(level.rawmapname));strcpy(level.rawmapname,argv[1]);
 level.time=12345;g_gametype.integer=atoi(argv[2]);g_newbbox.integer=atoi(argv[3]);
 g_developer.integer=atoi(argv[4]);g_gravity.value=(float)atof(argv[5]);
 ngWidth=g_newbbox.integer?16:14;ngStand=g_newbbox.integer?87:70;ngDuck=g_newbbox.integer?61:49;ngOrigin=g_newbbox.integer?30:24;
 before=level;vars[0]=g_gametype;vars[1]=g_newbbox;vars[2]=g_developer;vars[3]=g_gravity;
 NG_ProbeContext();
 assert(!memcmp(&before,&level,sizeof(level))&&!memcmp(&vars[0],&g_gametype,sizeof(g_gametype)));
 assert(!memcmp(&vars[1],&g_newbbox,sizeof(g_newbbox))&&!memcmp(&vars[2],&g_developer,sizeof(g_developer))&&!memcmp(&vars[3],&g_gravity,sizeof(g_gravity)));
 assert(ngWidth==(g_newbbox.integer?16:14)&&ngStand==(g_newbbox.integer?87:70)&&ngDuck==(g_newbbox.integer?61:49)&&ngOrigin==(g_newbbox.integer?30:24));
 return 0;
}
'''
        executable,options=compile_native(self,program,'context')
        maps=('obj_rain','obj_bloc','obj_fracture_v2','x'*63,'quoted"map\\name\n')
        for name in maps:
            for gt in(2,5,7):
                for profile in(0,1):
                    for developer,gravity in((0,333.75),(1,333.75),(1,800),(1,1600)):
                        with self.subTest(map=name,gt=gt,profile=profile,developer=developer,gravity=gravity):
                            result=subprocess.run([str(executable),name,str(gt),str(profile),str(developer),str(gravity)],**options)
                            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                            lines=result.stdout.splitlines();self.assertEqual(len(lines),1,result.stdout)
                            self.assertTrue(lines[0].startswith('navgen probe context: '))
                            self.assertLess(len((lines[0]+'\n').encode()),1024)
                            value=json.loads(lines[0].split('navgen probe context: ',1)[1])
                            gravity=int(gravity)if developer else 800
                            expected_gravity=int(gravity*1.25)if profile else gravity
                            self.assertEqual(value,dict(schema=1,map=name,gametype=gt,g_newbbox=profile,
                                bbox=dict(half_width=16 if profile else 14,stand_height=87 if profile else 70,
                                          crouch_height=61 if profile else 49,origin_above_feet=30 if profile else 24),gravity=expected_gravity))
        command=core[core.index(' if(!Q_stricmp(cmd,"tce_navgen_probe")'):core.index(' if(Q_stricmp(cmd,"tce_navgen"))')]
        self.assertEqual(command.count('NG_ProbeContext();'),1)
        self.assertLess(command.index('invalid literal feet coordinate'),command.index('NG_ProbeContext();'))
        self.assertLess(command.index('unknown endpoint or rejected native floor'),command.index('NG_ProbeContext();'))
        self.assertLess(command.index('NG_ProbeContext();'),command.index('NG_Walk(&from,&to,ngStand)'))
        self.assertLess(command.index('NG_ProbeContext();'),command.index('if(climbOnly)'))
        self.assertIn('\\"traversal_physics\\":{\\"gravity\\":%.9g',core)

    def test_actual_freefall_integrator_retains_both_body_profiles_and_damage_limit(self):
        program=drop_program().split('int main(int argc',1)[0]+r'''
int main(int argc,char **argv) {
 ngNode from,to;ngDropProof proof;int accepted,profile;float bound,height;
 assert(argc==3);profile=atoi(argv[1]);height=(float)atof(argv[2]);
 scenario=0;g_newbbox.integer=profile;g_developer.integer=0;g_gravity.value=800;
 ngWidth=profile?16:14;ngStand=profile?87:70;ngDuck=profile?61:49;ngOrigin=profile?30:24;
 ngTraceBudget=2000000;target_height=height;platform_edge=24;
 memset(&from,0,sizeof(from));memset(&to,0,sizeof(to));
 from.accepted=to.accepted=qtrue;from.authored=qtrue;from.id=1;to.id=2;
 VectorSet(from.feet,0,0,height+.125f);VectorSet(to.feet,128,0,.125f);
 g_entities[100].inuse=1;g_entities[100].classname="trigger_hurt";
 accepted=NG_TrWalkDrop(&from,&to,2000000,&proof);bound=profile?148.4375f:118.75f;
 REQUIRE(fabsf(NG_TrSafeDropHeight()-bound)<.001f,"original gravity/body-specific damage bound retained");
 if(height==102) {
  REQUIRE(accepted,"actual unchanged three-speed freefall remains harmless");
  REQUIRE(proof.crashDelta<=38&&proof.impact>0,"actual touchdown retains strict damage limit");
 } else REQUIRE(!accepted,"damaging unsupported fall must remain rejected");
 printf("PASS profile%d height%.0f accepted%d bound%.4f\n",profile,height,accepted,bound);return 0;
}
'''
        # The existing fixture uses REQUIRE, not the standard assert macro.
        program=program.replace('assert(argc==3);','REQUIRE(argc==3,"fixture arguments");')
        executable,options=compile_native(self,program,'drop')
        for profile in(0,1):
            for height in(102,300):
                with self.subTest(profile=profile,height=height):
                    result=subprocess.run([str(executable),str(profile),str(height)],**options)
                    self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                    self.assertIn('PASS profile'+str(profile),result.stdout)


if __name__=='__main__':main()
