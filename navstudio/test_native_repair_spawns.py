"""Execute the actual post-action controller and full native walking gates.
Two ordinary regions each have their own spawn. A flat floor can connect them;
a real gap cannot. No engine process, invented accepted link or RCON is used.
"""
import os
from pathlib import Path
import shutil,subprocess,tempfile
from unittest import TestCase,main,skipUnless
ROOT=(Path(__file__).resolve().parents[1] / "src")

@skipUnless(os.name=='nt' and shutil.which('cmake'),'requires native C compiler')
class NativeRepairSpawns(TestCase):
 def test_post_action_same_spawn_union_does_not_skip_physical_repair(self):
  source=(ROOT/'game/tce_navgen.c').read_text()
  fixture=r"""
#include <stdarg.h>
#undef _vsnprintf
#undef vsnprintf
int Q_vsnprintf(char *d,int n,const char *f,va_list a){return _vsnprintf(d,n,f,a);}
level_locals_t level;gentity_t g_entities[MAX_GENTITIES];
vmCvar_t g_newbbox,g_developer,g_gravity,g_speed,g_realism,g_gametype,g_movespeed;
void Use_BinaryMover(gentity_t *e,gentity_t *o,gentity_t *a){exit(108);}
void target_relay_use(gentity_t *e,gentity_t *o,gentity_t *a){exit(111);}
void use_invisible_user(gentity_t *e,gentity_t *o,gentity_t *a){exit(112);}
void trap_LinkEntity(gentity_t *e){exit(109);}
void trap_UnlinkEntity(gentity_t *e){exit(110);}
qboolean trap_EntityContactCapsule(const vec3_t a,const vec3_t b,const gentity_t *e){exit(113);return qfalse;}
static int hole;
int trap_Milliseconds(void){return 0;}
int trap_Argc(void){exit(101);return 0;}
void trap_Argv(int n,char *b,int l){exit(102);}
int trap_FS_FOpenFile(const char *p,fileHandle_t *f,fsMode_t m){if(m!=FS_READ||strcmp(p,"maps/.bsp"))exit(103);*f=0;return -1;}
void trap_FS_Read(void *b,int n,fileHandle_t f){exit(104);}
int trap_FS_Write(const void *b,int n,fileHandle_t f){exit(105);return 0;}
void trap_FS_FCloseFile(fileHandle_t f){exit(106);}
int trap_Cvar_VariableIntegerValue(const char *n){exit(107);return 0;}
void QDECL G_Printf(const char *f,...){ }
void QDECL Com_Printf(const char *f,...){ }
void QDECL Com_Error(int e,const char *f,...){exit(100);}
int trap_EntitiesInBox(const vec3_t a,const vec3_t b,int *list,int max){return 0;}
int trap_PointContents(const vec3_t p,int i){return 0;}
static void flat(trace_t *tr,const vec3_t a,const vec3_t mins,const vec3_t maxs,const vec3_t b){
 float low=mins?mins[2]:0;memset(tr,0,sizeof(*tr));tr->fraction=1;tr->entityNum=ENTITYNUM_WORLD;VectorCopy(b,tr->endpos);
 if(hole&&a[0]>-24.f&&a[0]<24.f)return;
 if(a[2]+low<.125f){tr->startsolid=tr->allsolid=qtrue;tr->fraction=0;VectorCopy(a,tr->endpos);return;}
 if(b[2]+low<.125f){tr->fraction=(a[2]+low-.125f)/(a[2]-b[2]);VectorSubtract(b,a,tr->endpos);VectorMA(a,tr->fraction,tr->endpos,tr->endpos);tr->plane.normal[2]=1.f;}
}
void trap_Trace(trace_t *t,const vec3_t a,const vec3_t mi,const vec3_t ma,const vec3_t b,int i,int m){flat(t,a,mi,ma,b);}
void trap_TraceCapsule(trace_t *t,const vec3_t a,const vec3_t mi,const vec3_t ma,const vec3_t b,int i,int m){flat(t,a,mi,ma,b);}
int main(void){int scenario,i;
 for(scenario=0;scenario<2;scenario++){
  hole=scenario;g_newbbox.integer=1;g_gametype.integer=2;NG_ExternalReset();ngExternalSpatialImport=qtrue;
  ngSpawnSeeds=2;ngCount=2;
  for(i=0;i<2;i++){ngNodes[i].id=i;ngNodes[i].accepted=qtrue;ngNodes[i].reason="map_entity_spawn_seed";ngNodes[i].index=-1;VectorSet(ngNodes[i].feet,i?64.f:-64.f,0,.25f);}
  NG_SpawnReachability();if(!ngSpawnReachable[0]||!ngSpawnReachable[1])return 1;
  NG_ExternalRepair(qtrue);
  if(!scenario){if(!NG_HasEdge(0,1)||!NG_HasEdge(1,0)||ngPortalConnected!=1)return 2;}
  else {if(NG_HasEdge(0,1)||NG_HasEdge(1,0)||ngPortalConnected)return 3;}
 }
 puts("actual post-action controller/native walk two-spawn + gap controls passed");return 0;
}
"""
  with tempfile.TemporaryDirectory(prefix='tce-native-repair-spawns-') as directory:
   root=Path(directory);(root/'main.c').write_text(source+fixture)
   old=(ROOT/'game/tce_navgen_external.inc').read_text().replace('ngExternalComponent[i]==ngExternalComponent[j])continue;',
       'ngExternalComponent[i]==ngExternalComponent[j]||(afterActions&&ngSpawnReachable[i]&&ngSpawnReachable[j]))continue;',1).replace(
       'memcpy(targets,main,sizeof(targets));','memcpy(targets,afterActions?ngSpawnReachable:main,sizeof(targets));',1)
   (root/'external-control.inc').write_text(old)
   (root/'control.c').write_text(source.replace('#include "tce_navgen_external.inc"','#include "external-control.inc"')+fixture)
   cmake='cmake_minimum_required(VERSION 3.15)\nproject(RepairSpawns LANGUAGES C)\nadd_executable(repair main.c "'+(ROOT/'game/q_shared.c').as_posix()+'" "'+(ROOT/'game/q_math.c').as_posix()+'" "'+(ROOT/'game/tce_movement.c').as_posix()+'")\ntarget_include_directories(repair PRIVATE "'+(ROOT/'game').as_posix()+'")\ntarget_compile_options(repair PRIVATE /Gy)\ntarget_link_options(repair PRIVATE /OPT:REF)\n'
   cmake+='add_executable(control control.c "'+(ROOT/'game/q_shared.c').as_posix()+'" "'+(ROOT/'game/q_math.c').as_posix()+'" "'+(ROOT/'game/tce_movement.c').as_posix()+'")\ntarget_include_directories(control PRIVATE "'+(ROOT/'game').as_posix()+'")\ntarget_compile_options(control PRIVATE /Gy)\ntarget_link_options(control PRIVATE /OPT:REF)\n'
   (root/'CMakeLists.txt').write_text(cmake)
   for command in (['cmake','-S',directory,'-B',str(root/'build'),'-A','Win32'],['cmake','--build',str(root/'build'),'--config','Release'],[str(root/'build/Release/repair.exe')]):
    r=subprocess.run(command,capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
    self.assertEqual(r.returncode,0,r.stdout+r.stderr)
   control=subprocess.run([str(root/'build/Release/control.exe')],capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
   self.assertEqual(control.returncode,2,"Old post-action spawn-union skip must fail identical flat-floor fixture.")
if __name__=='__main__':main()
