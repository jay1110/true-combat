"""Compile native metadata parser, identity allocator, WAY and JSON writers.

These are serialization/contract tests, not collision or playable-map proofs.
An independent ladder brush additionally checks native facing validation.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from unittest import TestCase, main, skipUnless
from test_doors_native import ROOT, FIXTURE

sys.path.insert(0,str(ROOT/'tools'))
from tce_navgen import read_way,audit_waypoint_metadata


BASE='''TCE_NAVMESH 5 obj_test 3 4
2001 0 0 0 0 1 1 2 134217728 0 65 2003
2002 64 0 32 0 0 1 6 0 0 1 0
2003 -64 0 0 0 0 0 35 0 0 0 0
2001 2002
2002 2003
2003 2001
2001 2003
'''
NAME='Deck "Ä" \\'


def tail(records):
    return 'TCE_NODE_METADATA 1 %d\n'%len(records)+''.join(
        '%s %s %s %s %s %s\n'%record for record in records)


def program():
    source=(ROOT/'game/tce_navgen.c').read_text()
    fixture=FIXTURE.split('int main(',1)[0]
    fixture=fixture.replace('if(scenario==3){',
        '''if(scenario==20){VectorSet(lo,12,-64,0);VectorSet(hi,16,64,160);
 brush(tr,a,mi,ma,b,lo,hi,ENTITYNUM_WORLD);if(tr->fraction<1&&fabs(tr->plane.normal[0])>.9f)tr->surfaceFlags|=SURF_LADDER;}
 if(scenario==3){''')
    return source+fixture+r'''
int main(int argc,char **argv){
 static char document[4096];FILE *file;int i,okay,guide;char path[1024];
 file=fopen(argv[1],"rb");REQUIRE(file!=NULL,"fixture input available");
 i=(int)fread(document,1,sizeof(document)-1,file);fclose(file);document[i]=0;
 inputDocument=document;strcpy(level.rawmapname,"obj_test");g_newbbox.integer=0;
 NG_ExternalReset();okay=NG_ExternalRead("candidate");
 if(!strcmp(argv[3],"reject")){REQUIRE(!okay,"invalid metadata must fail closed");
  REQUIRE(ngExternalError!=NULL,"explicit metadata diagnostic");puts(ngExternalError);return 0;}
 REQUIRE(okay,"native authored metadata parser");
 REQUIRE(ngCount==3&&ngEdgeCount==4,"metadata never changes graph dimensions");
 REQUIRE(ngNodes[0].id==2001&&ngNodes[0].editRole==65&&ngNodes[0].editDropId==2002&&
  ngNodes[0].editFlags==134217728ULL&&ngNodes[0].radius==2,"action recipe remains exact");
 REQUIRE(ngNodes[1].feet[0]==64&&ngNodes[1].feet[2]==32&&ngNodes[1].radius==6&&ngNodes[1].editFlags==0,
  "tail cannot override physical endpoint or arrival radius");
 REQUIRE(ngEdges[0].from==0&&ngEdges[0].to==1&&ngEdges[3].from==0&&ngEdges[3].to==2,"edge directions retained");
 for(i=0;i<ngCount;i++){ngNodes[i].accepted=qtrue;ngNodes[i].index=i;
  ngExternalFloorAccepted[i]=1;ngExternalFloorReason[i]="independent_serialization_fixture";}
 for(i=0;i<ngEdgeCount;i++){ngEdges[i].accepted=qtrue;ngEdges[i].reason="manual_native_transition_live_pending";}
 if(!strcmp(argv[3],"valid")){
  REQUIRE(ngMetadataInputCount==3&&ngNodes[0].importUID==2147483647u&&ngNodes[1].importUID==4,
   "usable high signed import identity retained");
  REQUIRE(!strcmp(ngNodes[0].name,"Deck \"\303\204\" \\"),"strict UTF8 decoded without quote/backslash loss");
  REQUIRE(ngNodes[2].facing[1]==1&&ngNodes[2].name[0],"ordinary author facing/name retained");
  // Native guides often copy a source struct. They must receive new UIDs,
  // including when index+1 is already reserved by an imported waypoint.
  guide=ngCount++;ngNodes[guide]=ngNodes[0];ngNodes[guide].id=2004;ngNodes[guide].index=guide;
  ngNodes[guide].authored=qfalse;ngNodes[guide].name[0]=0;VectorClear(ngNodes[guide].facing);
  REQUIRE(NG_AssignOutputUIDs()&&ngNodes[guide].outputUID==1&&NG_ImportUID(&ngNodes[guide])==0,
   "copied guide identity cannot alias source or imported index UID");
  // A rejected input still reserves its original identity for later repair.
  ngNodes[1].accepted=qfalse;ngNodes[1].importUID=3;ngNodes[2].index=1;ngNodes[guide].index=2;
  REQUIRE(NG_AssignOutputUIDs()&&ngNodes[guide].outputUID!=3&&ngNodes[1].outputUID==0,
   "rejected input UID remains reserved without a false export identity");
  printf("NATIVE_UID_PLAN_REJECTED %u %u %u %u\n",ngNodes[0].outputUID,ngNodes[1].outputUID,ngNodes[2].outputUID,ngNodes[guide].outputUID);
  ngNodes[1].accepted=qtrue;ngNodes[1].importUID=4;ngNodes[2].index=2;ngNodes[guide].index=3;
 }
 // A final-graph failure skips NG_Save in the actual import command. Emit
 // that same incomplete report with no assigned/exported WAY identities.
 for(i=0;i<ngCount;i++)ngNodes[i].outputUID=0;
 REQUIRE(NG_Report("partial.json","candidate.txt","obj_test",qfalse),"actual incomplete pre-Save report");
 snprintf(path,sizeof(path),"%s.partial.json",argv[2]);file=fopen(path,"wb");REQUIRE(file!=NULL,"partial report output");
 fwrite(output,1,outputLength,file);fclose(file);
 REQUIRE(NG_Save("memory.way",ngCount),"actual WAY writer");
 snprintf(path,sizeof(path),"%s.way",argv[2]);file=fopen(path,"wb");REQUIRE(file!=NULL,"WAY fixture output");
 fwrite(output,1,outputLength,file);fclose(file);
 REQUIRE(NG_Report("memory.json","candidate.txt","obj_test",qtrue),"actual escaped JSON writer");
 snprintf(path,sizeof(path),"%s.json",argv[2]);file=fopen(path,"wb");REQUIRE(file!=NULL,"JSON fixture output");
 fwrite(output,1,outputLength,file);fclose(file);
 // Actual contact normals come from the independent vertical ladder brush.
 {ngNode a,b,original;scenario=20;a=ngNodes[2];b=a;
  a.id=a.editSourceId=3001;b.id=b.editSourceId=3002;a.metadataSupplied=b.metadataSupplied=qtrue;
  a.navFlags=b.navFlags=NG_NAV_CLIMB;a.editRole=b.editRole=1;
  VectorSet(a.feet,-2,0,.25f);VectorSet(b.feet,-2,0,64.25f);VectorSet(a.facing,1,0,0);VectorCopy(a.facing,b.facing);
  original=a;REQUIRE(NG_EditorLadderFacing(&a,&b),"metadata facing matches current native ladder surface");
  REQUIRE(!memcmp(&a,&original,sizeof(a)),"native facing validation never overwrites authored metadata");
  VectorSet(a.facing,-1,0,0);REQUIRE(!NG_EditorLadderFacing(&a,&b),"antiparallel ladder metadata rejected");
  a.navFlags=0;REQUIRE(NG_EditorLadderFacing(&a,&b),"ordinary facing remains free of physical ladder restriction");
 }
 puts("native metadata parser, UID allocation, recipe targets, JSON and ladder facing verified");return 0;
}
'''


@skipUnless(os.name=='nt' and shutil.which('cmake'),'Windows CMake/MSVC required')
class WaypointMetadataNativeTests(TestCase):
    def test_actual_native_roundtrip_and_negative_input_gates(self):
        with tempfile.TemporaryDirectory(prefix='tce-metadata-native-') as folder:
            d=Path(folder);(d/'main.c').write_text(program(),encoding='utf-8')
            sources=' '.join('"'+(ROOT/'game'/name).as_posix()+'"' for name in ('q_shared.c','q_math.c','tce_movement.c'))
            (d/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(Metadata LANGUAGES C)\n'
                'add_executable(metadata main.c '+sources+')\ntarget_include_directories(metadata PRIVATE "'+(ROOT/'game').as_posix()+'")\n'
                'target_compile_options(metadata PRIVATE /Gy)\ntarget_link_options(metadata PRIVATE /OPT:REF)\n')
            options=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake','-S',folder,'-B',str(d/'build'),'-A','Win32'],
                            ['cmake','--build',str(d/'build'),'--config','Release']):
                r=subprocess.run(command,**options);self.assertEqual(r.returncode,0,r.stdout+r.stderr)
            exe=d/'build/Release/metadata.exe';candidate=d/'candidate.txt'
            records=[(2001,2147483647,0,0,0,NAME.encode().hex()),(2002,4,0,0,0,'5570706572'),
                     (2003,0,0,1,0,'4f7264696e617279')]
            candidate.write_text(BASE+tail(records),encoding='ascii')
            r=subprocess.run([str(exe),str(candidate),str(d/'roundtrip'),'valid'],**options)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
            report=json.loads((d/'roundtrip.json').read_text(encoding='utf-8'));nodes,edges=read_way(d/'roundtrip.way')
            partial=json.loads((d/'roundtrip.partial.json').read_text(encoding='utf8'))
            import copy
            from edit import candidate_recipe_nodes,native_way_uid
            original=copy.deepcopy(partial)
            planned=candidate_recipe_nodes(partial)
            self.assertFalse(partial['generation_complete'])
            self.assertTrue(all(n['way_guid']==0 for n in partial['nodes']))
            self.assertEqual([native_way_uid(planned[n['id']]) for n in partial['nodes'] if n['accepted']],
                             [n['uid'] for n in nodes])
            self.assertEqual(partial,original)
            rejected=copy.deepcopy(partial);rejected['nodes'][1].update(accepted=False,index=-1,import_uid=3)
            rejected['nodes'][2]['index']=1;rejected['nodes'][3]['index']=2
            rejected_plan=candidate_recipe_nodes(rejected)
            actual_rejected=[int(v) for v in next(line for line in r.stdout.splitlines() if line.startswith('NATIVE_UID_PLAN_REJECTED ')).split()[1:]]
            self.assertEqual([native_way_uid(rejected_plan[n['id']]) if n['accepted'] else 0 for n in rejected['nodes']],actual_rejected)
            self.assertEqual([n['uid'] for n in nodes],[2147483647,4,3,1])
            self.assertEqual(nodes[0]['name'],NAME);self.assertEqual(nodes[2]['facing'],(0,1,0))
            self.assertEqual(nodes[0]['properties']['tce_stand_jump_target'],'4')
            self.assertEqual(report['nodes'][0]['name'],NAME)
            self.assertEqual(report['nodes'][0]['way_guid'],2147483647)
            self.assertEqual(report['nodes'][3]['import_uid'],0)
            self.assertFalse(report['nodes'][3]['metadata_supplied'])
            self.assertEqual(edges,{(0,1),(1,2),(2,0),(0,2)})
            self.assertTrue(audit_waypoint_metadata(report,d/'roundtrip.json',nodes)['source_bound'])
            for field,value in [('name','lost name'),('way_guid',9),('import_uid',9),('facing',[1,0,0])]:
                import copy
                changed=copy.deepcopy(report);changed['nodes'][0][field]=value
                with self.subTest(binding=field):
                    with self.assertRaises(ValueError):audit_waypoint_metadata(changed,d/'roundtrip.json',nodes)
            # Format6 appends thirteen physical-door columns before the same
            # optional tail. No metadata can create a door recipe from zeros.
            rows=BASE.splitlines();rows[0]=rows[0].replace(' 5 ',' 6 ')
            for i in range(1,4):rows[i]+=' '+' '.join(['0']*13)
            candidate.write_text('\n'.join(rows)+'\n'+tail(records),encoding='ascii')
            r=subprocess.run([str(exe),str(candidate),str(d/'format6'),'valid'],**options)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
            self.assertEqual(read_way(d/'format6.way')[0][0]['properties']['tce_stand_jump_target'],'4')
            # Maximum byte-length survives the actual native WAY/string writer.
            longest=list(records);longest[2]=(2003,0,0,1,0,('x'*255).encode().hex())
            candidate.write_text(BASE+tail(longest),encoding='ascii')
            r=subprocess.run([str(exe),str(candidate),str(d/'longname'),'valid'],**options)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
            self.assertEqual(read_way(d/'longname.way')[0][2]['name'],'x'*255)
            candidate.write_text(BASE,encoding='ascii')
            r=subprocess.run([str(exe),str(candidate),str(d/'legacy'),'legacy'],**options)
            self.assertEqual(r.returncode,0,r.stdout+r.stderr)
            self.assertEqual([n['uid'] for n in read_way(d/'legacy.way')[0]],[1,2,3])
            bad=[tail([(2001,1,0,0,0,'-'),(2001,2,0,0,0,'-')]),
                tail([(2001,1,0,0,0,'-'),(2002,1,0,0,0,'-')]),tail([(9999,1,0,0,0,'-')]),
                tail([(2001,2147483648,0,0,0,'-')]),tail([(2001,4294967296,0,0,0,'-')]),
                tail([(2001,0,2,0,0,'-')]),tail([(2001,0,'nan',0,0,'-')]),
                tail([(2001,0,0,0,0,'00')]),tail([(2001,0,0,0,0,'0a')]),
                tail([(2001,0,0,0,0,'c080')]),tail([(2001,0,0,0,0,'eda080')]),
                tail([(2001,0,0,0,0,'f4908080')]),tail([(2001,0,0,0,0,'c3')]),
                tail([(2001,0,0,0,0,'xyz')]),tail([(2001,0,0,0,0,'61'*256)]),
                'TCE_NODE_METADATA 2 0\n','TCE_NODE_METADATA 1 4\n',tail(records)+'garbage\n']
            for i,value in enumerate(bad):
                with self.subTest(rejection=i):
                    candidate.write_text(BASE+value,encoding='ascii')
                    r=subprocess.run([str(exe),str(candidate),str(d/'bad'),'reject'],**options)
                    self.assertEqual(r.returncode,0,r.stdout+r.stderr)


if __name__=='__main__':main()
