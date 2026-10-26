"""Actual native PropertyMap tail parser and WAY/report serialization."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
from unittest import TestCase, main, skipUnless
from test_waypoint_metadata_native import ROOT,BASE,NAME,tail,program,read_way,audit_waypoint_metadata
from tce_navgen import audit_custom_properties


def properties(records):
    return 'TCE_NODE_PROPERTIES 1 %d\n'%len(records)+''.join('%d %s %s\n'%(
        identity,key.encode('utf8').hex(),value.encode('utf8').hex() or '-') for identity,key,value in records)


@skipUnless(os.name=='nt' and shutil.which('cmake'),'Windows CMake/MSVC required')
class WaypointPropertiesNativeTests(TestCase):
    def test_actual_native_properties_are_literals_and_do_not_override_recipes(self):
        with tempfile.TemporaryDirectory(prefix='tce-properties-native-') as folder:
            d=Path(folder)
            (d/'main.c').write_text(program().replace('static char document[4096]','static char document[NG_EXTERNAL_INPUT+1]'),encoding='utf8')
            sources=' '.join('"'+(ROOT/'game'/name).as_posix()+'"' for name in ('q_shared.c','q_math.c','tce_movement.c'))
            (d/'CMakeLists.txt').write_text('cmake_minimum_required(VERSION 3.15)\nproject(Properties LANGUAGES C)\n'
                'add_executable(properties main.c '+sources+')\ntarget_include_directories(properties PRIVATE "'+(ROOT/'game').as_posix()+'")\n'
                'target_compile_options(properties PRIVATE /Gy)\ntarget_link_options(properties PRIVATE /OPT:REF)\n')
            options=dict(capture_output=True,text=True,timeout=90,creationflags=subprocess.CREATE_NO_WINDOW)
            for command in (['cmake','-S',folder,'-B',str(d/'build'),'-A','Win32'],['cmake','--build',str(d/'build'),'--config','Release']):
                result=subprocess.run(command,**options);self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            exe=d/'build/Release/properties.exe';candidate=d/'candidate.txt'
            metadata=tail([(2001,2147483647,0,0,0,NAME.encode().hex()),(2002,4,0,0,0,'5570706572'),
                           (2003,0,0,1,0,'4f7264696e617279')])
            pairs=[(2001,'paththrough','door_route'),(2001,'paththroughparam','A "Ä" \\ B'),
                   (2002,'straße','literal {{ example }}'),(2003,'empty','')]
            for mode in ('legacy','valid'):
                candidate.write_text(BASE+(metadata if mode=='valid' else '')+properties(pairs),encoding='ascii')
                result=subprocess.run([str(exe),str(candidate),str(d/mode),mode],**options)
                self.assertEqual(result.returncode,0,result.stdout+result.stderr)
                report=json.loads((d/(mode+'.json')).read_text(encoding='utf8'));nodes,edges=read_way(d/(mode+'.way'))
                expected={identity:{key:value for owner,key,value in pairs if owner==identity} for identity in (2001,2002,2003)}
                self.assertEqual(report['custom_property_input_count'],4)
                for index,node in enumerate(report['nodes'][:3]):
                    self.assertEqual(node['custom_properties'],expected[node['id']])
                    self.assertEqual({k:v for k,v in nodes[index]['properties'].items() if not k.startswith('tce_')},expected[node['id']])
                self.assertEqual(nodes[0]['properties']['tce_stand_jump_target'],'4' if mode=='valid' else '2')
                if mode=='valid':
                    self.assertEqual(report['nodes'][3]['custom_properties'],{})
                    self.assertEqual(nodes[3]['properties'],{'tce_node':'2004'})
                    self.assertTrue(audit_waypoint_metadata(report,d/(mode+'.json'),nodes)['source_bound'])
                self.assertTrue(audit_custom_properties(report,d/(mode+'.json'),nodes)['source_bound'])
            longest=[(2001,'k'*127,'v'*1023)]
            candidate.write_text(BASE+properties(longest),encoding='ascii')
            result=subprocess.run([str(exe),str(candidate),str(d/'longest'),'legacy'],**options)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertEqual(read_way(d/'longest.way')[0][0]['properties']['k'*127],'v'*1023)
            bad=[properties([(2001,'same','one'),(2001,'same','two')]),properties([(2001,'tce_node','spoof')]),
                properties([(2001,'tce_unknown','spoof')]),properties([(9999,'x','y')]),
                'TCE_NODE_PROPERTIES 1 1\n2001 - -\n','TCE_NODE_PROPERTIES 1 1\n2001 78 00\n',
                'TCE_NODE_PROPERTIES 1 1\n2001 78 c080\n',properties([(2001,'k'*128,'v')]),
                properties([(2001,'k','v'*1024)]),properties([(2001,'p%d'%i,'v') for i in range(33)]),
                'TCE_NODE_PROPERTIES 1 4097\n','TCE_NODE_PROPERTIES 2 0\n',properties(pairs)+'trailing\n',
                properties(pairs)+metadata]
            for i,value in enumerate(bad):
                with self.subTest(rejection=i):
                    candidate.write_text(BASE+value,encoding='ascii')
                    result=subprocess.run([str(exe),str(candidate),str(d/'reject'),'reject'],**options)
                    self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            rows=['TCE_NAVMESH 5 obj_test 128 2']
            rows+=['%d %d 0 0 0 0 0 35 0 0 0 0'%(1000+i,i*64) for i in range(128)]
            rows+=['1000 1001','1001 1000']
            huge=properties([(1000+i//32,'p%d'%(i%32),'v'*1023) for i in range(1100)])
            candidate.write_text('\n'.join(rows)+'\n'+huge,encoding='ascii')
            result=subprocess.run([str(exe),str(candidate),str(d/'budget'),'reject'],**options)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('custom_property_storage_exceeds_1mib',result.stdout)


if __name__=='__main__':main()
