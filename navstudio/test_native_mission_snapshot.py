"""Native exporter read-only regression and matching-GT mission provenance."""
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase,main,skipUnless
from unittest.mock import patch
from goalgen import mission_sources,make_plan
from test_traversal_reset_native import function


ROOT=(Path(__file__).resolve().parents[1] / "src")


class MissionSnapshotTests(TestCase):
    def snapshot(self):
        return dict(map='obj_test',gametype=2,objective_entities=[
            dict(entity=200,classname='team_CTF_redflag',native_goal_kind='take_flag',scriptname='dynamic_flag',
                 position=[128,0,30],dropped=False),
            dict(entity=201,classname='trigger_objective_info',native_goal_kind='capture',track='dynamic_capture',
                 spawnflags=1,position=[256,0,30],bounds=[[230,-32,0],[280,32,80]],contact_top=80,
                 target=dict(classname='trigger_flagonly_multiple',targetname='capture',
                    spawnflags=5,position=[256,0,30],bounds=[[230,-32,0],[280,32,80]]))])

    def test_matching_native_snapshot_adds_real_dynamic_flag_capture_entities(self):
        source,mode=mission_sources(None,'obj_test',2,self.snapshot())
        self.assertEqual(mode,'matching_native_runtime_snapshot')
        self.assertEqual([x['operation']for x in source],['take_flag','capture'])
        self.assertEqual([x['entity_id']for x in source],[200,201])
        self.assertEqual(source[1]['capture_teams'],4)
        self.assertEqual(source[1]['capture_team_source'],'native_flagonly_spawnflags')
        nodes=[dict(id=i,position=[i*128,0,30],accepted=True,spawn_reachable=True,nav_flags=0)for i in range(4)]
        edges=[dict(**{'from':i,'to':j},accepted=True)for i in range(4)for j in range(4)if abs(i-j)==1]
        plan=make_plan(dict(generation_complete=True,bbox=dict(origin_above_feet=30),nodes=nodes,edges=edges),source,2)
        self.assertTrue(plan['mission_geometry_complete'])
        self.assertEqual(plan['planning_status'],'mission_geometry_complete_live_pending')
        self.assertFalse(plan['mission_behavior_complete'])

    def test_gt2_snapshot_cannot_override_gt5_bsp_with_missing_bombsites(self):
        expected=[dict(tag='actual_gt5_bsp_site')]
        with patch('goalgen.objectives',return_value=expected)as bsp:
            source,mode=mission_sources(None,'obj_test',5,self.snapshot())
        self.assertIs(source,expected)
        self.assertEqual(mode,'bsp_proposal_runtime_entities_unconfirmed')
        bsp.assert_called_once_with(None,'obj_test',5)

    def test_no_native_ctf_entities_is_incomplete_not_tactical_mission_success(self):
        snapshot=self.snapshot();snapshot['objective_entities']=[]
        source,mode=mission_sources(None,'obj_test',2,snapshot)
        nodes=[dict(id=i,position=[i*128,0,30],accepted=True,spawn_reachable=True,nav_flags=0)for i in range(2)]
        edges=[dict(**{'from':0,'to':1},accepted=True),dict(**{'from':1,'to':0},accepted=True)]
        plan=make_plan(dict(generation_complete=True,nodes=nodes,edges=edges),source,2)
        self.assertEqual(plan['mission_entities_detected'],0)
        self.assertGreater(plan['tactical_goal_count'],0)
        self.assertFalse(plan['mission_geometry_complete'])
        self.assertEqual(plan['planning_status'],'mission_geometry_incomplete')

    def test_legacy_capture_target_without_colour_bits_is_not_guessed_from_oid(self):
        snapshot=self.snapshot();snapshot['objective_entities'][1]['target'].pop('spawnflags')
        source,mode=mission_sources(None,'obj_test',2,snapshot)
        self.assertEqual(source[1]['capture_teams'],0)
        self.assertEqual(source[1]['capture_team_source'],'native_flagonly_spawnflags_missing')
        nodes=[dict(id=i,position=[i*128,0,30],accepted=True,spawn_reachable=True,nav_flags=0)for i in range(4)]
        edges=[dict(**{'from':i,'to':j},accepted=True)for i in range(4)for j in range(4)if abs(i-j)==1]
        plan=make_plan(dict(generation_complete=True,nodes=nodes,edges=edges),source,2)
        self.assertFalse(any(g['type']=='CapPoint'for g in plan['goals']))
        self.assertIn('missing_flagonly_capture_flag_mask',[u['reason']for u in plan['unresolved']])

    def test_legacy_snapshot_and_wrong_map_remain_bsp_proposals(self):
        with patch('goalgen.objectives',return_value=['bsp']):
            for snapshot in (dict(map='obj_test',models=[]),dict(self.snapshot(),map='obj_other')):
                self.assertEqual(mission_sources(None,'obj_test',2,snapshot)[0],['bsp'])


@skipUnless(os.name=='nt'and shutil.which('cmake'),'Windows CMake/MSVC required')
class NativeMissionExporterTests(TestCase):
    def test_actual_exporter_includes_point_flags_dynamic_trigger_and_does_not_mutate_entities(self):
        source=(ROOT/'game/tce_navgen_external.inc').read_text()
        doors=(ROOT/'game/tce_navgen_doors.inc').read_text()
        core=(ROOT/'game/tce_navgen.c').read_text()
        bodies=[function(core,'NG_Finite')]
        bodies += [function(doors,n)for n in ('NG_DoorClass','NG_DoorMaster','NG_DoorTeam','NG_DoorGroup')]
        bodies += [function(source,n)for n in ('NG_ExternalJsonString','NG_ExternalMissionEntity','NG_ExternalEntities')]
        door_types=[item for item in re.findall(r'typedef struct\s*\{[^}]+\}\s*\w+;',doors,re.S)
                    if item.rsplit('}',1)[1].strip(' ;') in ('ngDoorSaved','ngDoorPose')]
        program=r'''
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
typedef int qboolean,fileHandle_t;typedef float vec3_t[3];
enum {MAX_CLIENTS=4,MAX_GENTITIES=16,MAX_MODELS=256,MAX_QPATH=64,FS_WRITE=1,TR_STATIONARY=0,FL_DROPPED_ITEM=8,qtrue=1,qfalse=0,ET_ITEM=2,ET_OID_TRIGGER=12,ET_EXPLOSIVE=14,
 MOVER_POS1=0,MOVER_POS2=1,MOVER_POS1ROTATE=7,MOVER_POS2ROTATE=8};
#define NG_DOOR_GROUP 32
#define NG_MASK (1|0x10000)
#define Q_stricmp _stricmp
#define VectorSubtract(a,b,c) do{int axis;for(axis=0;axis<3;axis++)(c)[axis]=(a)[axis]-(b)[axis];}while(0)
#define VectorLengthSquared(a) ((a)[0]*(a)[0]+(a)[1]*(a)[1]+(a)[2]*(a)[2])
#define VectorLength(a) sqrt(VectorLengthSquared(a))
typedef struct {int bmodel,linked,contents;vec3_t currentOrigin,currentAngles,mins,maxs,absmin,absmax;} entityShared_t;
typedef struct Entity {
 int inuse,spawnflags,entstate,flags,takedamage,health,key,allowteams,moverState;char *model,*classname,*track,*scriptName,*targetname,*target;
 entityShared_t r;vec3_t pos1,pos2,rotate;float angle;
 struct {int modelindex,eType;struct {int trType;vec3_t trBase;}pos,apos;}s;
 struct {int weaponclass;}constructibleStats;struct Entity *target_ent,*teammaster,*teamchain;
 void (*use)(struct Entity*,struct Entity*,struct Entity*);
}gentity_t;
static void Use_BinaryMover(gentity_t *ent,gentity_t *other,gentity_t *activator){(void)ent;(void)other;(void)activator;exit(12);}
static gentity_t g_entities[MAX_GENTITIES];static struct{char rawmapname[MAX_QPATH];int time,num_entities;}level;
static struct{int integer;}g_gametype;static fileHandle_t ngFile;static qboolean ngIoFailed;
static char output[32000];static size_t cursor;
static void NG_Text(const char *s){size_t n=strlen(s);if(cursor+n>=sizeof(output))exit(4);memcpy(output+cursor,s,n+1);cursor+=n;}
static void NG_Byte(int c){char s[2]={(char)c,0};NG_Text(s);}
static const char*va(const char *format,...){static char result[4096];va_list a;va_start(a,format);vsnprintf(result,sizeof(result),format,a);va_end(a);return result;}
static int trap_Argc(void){return 1;}static int NG_SafePath(const char*p){(void)p;return 1;}
static void Com_sprintf(char*out,size_t size,const char*format,...){va_list a;va_start(a,format);vsnprintf(out,size,format,a);va_end(a);}
static void trap_FS_FOpenFile(const char*p,fileHandle_t *file,int mode){(void)p;(void)mode;*file=1;}
static void trap_FS_FCloseFile(fileHandle_t file){(void)file;}
static int TCE_NavGroundKind(int id){(void)id;return 0;}
static void G_Printf(const char*p,...){(void)p;}
'''+'\n'.join(door_types+bodies)+r'''
int main(int argc,char **argv){gentity_t before[MAX_GENTITIES];int i;
 strcpy(level.rawmapname,"obj_test");level.time=500;level.num_entities=14;g_gametype.integer=argc>1?atoi(argv[1]):2;
 for(i=4;i<14;i++)g_entities[i].inuse=1;
 g_entities[4].classname="team_CTF_redflag";g_entities[4].scriptName="dynamic_flag\"quoted";
 g_entities[4].s.eType=ET_ITEM;
 g_entities[4].r.absmin[0]=120;g_entities[4].r.absmax[0]=136;
 g_entities[5].classname="trigger_objective_info";g_entities[5].track="dynamic_capture";g_entities[5].spawnflags=1;
 g_entities[5].s.eType=ET_OID_TRIGGER;
 g_entities[5].r.linked=1;g_entities[5].r.contents=16;g_entities[5].r.bmodel=1;g_entities[5].model="*1";
 g_entities[5].r.maxs[2]=80;g_entities[5].s.pos.trBase[2]=4;g_entities[5].target_ent=&g_entities[6];
 g_entities[6].classname="trigger_flagonly_multiple";g_entities[6].targetname="capture";
 g_entities[6].spawnflags=5;
 g_entities[7].classname="team_CTF_blueflag";g_entities[7].flags=FL_DROPPED_ITEM;
 g_entities[8].classname="func_door";g_entities[8].model="*2";g_entities[8].s.modelindex=2;
 g_entities[8].r.bmodel=g_entities[8].r.linked=1;g_entities[8].r.contents=1;
 g_entities[8].use=Use_BinaryMover;g_entities[8].pos2[1]=160;g_entities[8].allowteams=1;
 g_entities[8].targetname="door_target";
 g_entities[9]=g_entities[8];g_entities[9].key=-1;g_entities[9].s.modelindex=3;g_entities[9].model="*3";g_entities[9].targetname="locked_door";
 g_entities[10].classname="func_button";g_entities[10].model="*4";g_entities[10].s.modelindex=4;
 g_entities[10].r.bmodel=g_entities[10].r.linked=1;g_entities[10].r.contents=1;
 g_entities[10].target="relay_chain";
 g_entities[11].classname="target_relay";g_entities[11].targetname="relay_chain";g_entities[11].target="door_target";
 g_entities[12].classname="trigger_objective_info";g_entities[12].track="native_sabotage";g_entities[12].spawnflags=2;
 g_entities[12].s.eType=ET_OID_TRIGGER;g_entities[12].target_ent=&g_entities[13];
 g_entities[13].classname="func_explosive";g_entities[13].s.eType=ET_EXPLOSIVE;g_entities[13].constructibleStats.weaponclass=2;
 memcpy(before,g_entities,sizeof(before));NG_ExternalEntities();
 if(memcmp(before,g_entities,sizeof(before))||level.time!=500)return 9;
 puts(output);return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='tce-objective-snapshot-')as folder:
            directory=Path(folder);(directory/'main.c').write_text(program)
            (directory/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(MissionSnapshot LANGUAGES C)\nadd_executable(mission-snapshot main.c)\n')
            options=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake','-S',folder,'-B',str(directory/'build'),'-A','Win32'],
                            ['cmake','--build',str(directory/'build'),'--config','Release']):
                run=subprocess.run(command,**options);self.assertEqual(run.returncode,0,run.stdout+run.stderr)
            run=subprocess.run([str(directory/'build/Release/mission-snapshot.exe')],**options)
            self.assertEqual(run.returncode,0,run.stdout+run.stderr);snapshot=json.loads(run.stdout)
            for gametype in(5,7):
                run=subprocess.run([str(directory/'build/Release/mission-snapshot.exe'),str(gametype)],**options)
                self.assertEqual(run.returncode,0,run.stdout+run.stderr)
                by_id={item['entity']:item for item in json.loads(run.stdout)['objective_entities']}
                self.assertEqual(by_id[12]['native_goal_kind'],'plant_defuse'if gametype==5 else'unsupported')
        self.assertEqual(snapshot['gametype'],2)
        self.assertEqual(len(snapshot['models']),4)
        self.assertEqual(len(snapshot['objective_entities']),4)
        self.assertEqual(snapshot['objective_entities'][0]['scriptname'],'dynamic_flag"quoted')
        self.assertEqual(snapshot['objective_entities'][1]['target']['classname'],'trigger_flagonly_multiple')
        self.assertEqual(snapshot['objective_entities'][1]['target']['spawnflags'],5)
        self.assertEqual(snapshot['objective_entities'][1]['contact_top'],84)
        self.assertEqual(snapshot['objective_entities'][1]['native_goal_kind'],'capture')
        self.assertTrue(snapshot['objective_entities'][2]['dropped'])
        self.assertEqual(snapshot['objective_entities'][3]['native_goal_kind'],'plant_defuse')
        door_data={item['entity']:item for item in snapshot['door_entities']}
        self.assertEqual(set(door_data),{8,9,10,11})
        self.assertTrue(door_data[8]['eligible'])
        self.assertEqual(door_data[8]['parts'],[8])
        self.assertEqual(door_data[8]['team'],1)
        self.assertEqual(door_data[8]['closed_origin'],[0,0,0])
        self.assertEqual(door_data[8]['open_origin'],[0,160,0])
        self.assertFalse(door_data[9]['eligible'])
        self.assertEqual(door_data[9]['reason'],'door_locked_or_key_required')
        self.assertEqual(door_data[10]['target'],'relay_chain')
        self.assertEqual(door_data[11]['target'],'door_target')


if __name__=='__main__':main()
