"""Actual engine door WAY -> Python draft -> actual native Format6 parser."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from unittest import TestCase,main,skipUnless

from edit import Drafts,door_role
from test_doors_native import FIXTURE,ROOT
sys.path.insert(0,str(ROOT/'tools'))
from tce_navgen import read_way

MAIN=r'''
int main(int argc,char **argv){int i,k;const char *bad;FILE *f;static char document[262144];setup(0);
 if(argc==2){NG_Doors();for(i=0;i<ngCount;i++)ngNodes[i].index=i;
  REQUIRE(NG_Save("navgen/test.way",ngCount),"engine WAY export");f=fopen(argv[1],"wb");REQUIRE(f!=NULL,"WAY file");fwrite(output,1,outputLength,f);fclose(f);return 0;}
 f=fopen(argv[2],"rb");REQUIRE(f!=NULL,"Python candidate");i=(int)fread(document,1,sizeof(document)-1,f);fclose(f);document[i]=0;
 inputDocument=document;strcpy(level.rawmapname,"obj_test");NG_ExternalReset();
 REQUIRE(NG_ExternalRead("candidate"),"Python Format6 must parse without conflicting traversal guide roles");
 for(i=0;i<ngCount;i++){NG_Node(&ngNodes[i]);REQUIRE(ngNodes[i].accepted,"Python node real floor");}
 for(k=0;k<ngEdgeCount;k++){bad=NG_EditorEdge(&ngEdges[k]);REQUIRE(bad==NULL,"Python directed action native proof");ngEdges[k].accepted=qtrue;}
 REQUIRE(NG_DoorManualFinalize(),"Python door references and physical revalidation");return 0;
}
'''


@skipUnless(os.name=='nt' and shutil.which('cmake'),'Windows CMake/MSVC required')
class PythonNativeDoorRoundtrip(TestCase):
    def test_generated_guide_identity_survives_real_full_serialization_chain(self):
        with tempfile.TemporaryDirectory(prefix='tce-python-door-') as temp:
            target=Path(temp)
            source=(ROOT/'game/tce_navgen.c').read_text()+FIXTURE.split('int main(int argc,char **argv)')[0]+MAIN
            (target/'main.c').write_text(source)
            cmake='cmake_minimum_required(VERSION 3.15)\nproject(Doors LANGUAGES C)\n'
            cmake+='add_executable(doors main.c '+' '.join('"'+(ROOT/'game'/name).as_posix()+'"' for name in ('q_shared.c','q_math.c','tce_movement.c'))+')\n'
            cmake+='target_include_directories(doors PRIVATE "'+(ROOT/'game').as_posix()+'")\ntarget_compile_options(doors PRIVATE /Gy)\ntarget_link_options(doors PRIVATE /OPT:REF)\n'
            (target/'CMakeLists.txt').write_text(cmake)
            executable=target/'build/Release/doors.exe';way=target/'door.way'
            def run(command):
                value=subprocess.run(command,capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
                self.assertEqual(value.returncode,0,value.stdout+value.stderr)
            run(['cmake','-S',temp,'-B',str(target/'build'),'-A','Win32'])
            run(['cmake','--build',str(target/'build'),'--config','Release'])
            run([str(executable),str(way)])
            binary,pairs=read_way(way)
            ids={n['uid']:int(n['properties']['tce_node']) for n in binary}
            nodes=[];guides=[]
            for node in binary:
                identity=ids[node['uid']];props=node['properties'];role=door_role(props,ids)
                if role:guides.append(identity)
                p=list(node['position']);p[2]-=30
                nodes.append(dict(id=identity,position=p,radius=node['radius'],nav_flags=node['flags'],
                    name=node['name'],facing=list(node['facing']),way_guid=node['uid'],
                    action_properties=props,action_roles=[role] if role else []))
            edges=[dict(zip(('from','to'),(ids[binary[a]['uid']],ids[binary[b]['uid']])))for a,b in pairs]
            store=Drafts(target/'drafts');doc=store.start('obj_test',dict(nodes=nodes,edges=edges),
                dict(environment={},generated_guide_ids=guides))
            self.assertTrue(all(n['generated_guide']for n in doc['nodes']if n['id']in guides))
            store.save('obj_test',0,{})
            candidate=store.export_candidate('obj_test',0,{},target/'preview')
            run([str(executable),str(way),str(candidate)])


if __name__=='__main__':main()
