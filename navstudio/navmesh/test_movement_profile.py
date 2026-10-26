"""Map-dependent TCE hull and origin conversions; no engine or authored WAY."""
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from waygraph import (Mesh, movement_profile, validate_native_profile, build,
                     build_visibility_legacy, add_native_feedback, relax_ordinary_chains)


class MovementProfileTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def meshes(self, small=False, floor=0):
        radius, stand_height, duck_height = (14,70,49) if small else (16,87,61)
        paths=[]
        for name,height in [('stand',stand_height),('duck',duck_height)]:
            p=self.root/(name+'.json')
            p.write_text(json.dumps({'coordinate_system':'TCE_Z_up',
                'profile':{'radius':radius,'height':height,'step':18,'cell':4,'cell_height':2},
                'vertices':[[0,0,floor],[256,0,floor],[256,256,floor],[0,256,floor]],
                'polygons':[{'vertices':[0,1,2,3],'neighbors':[-1]*4}]}))
            paths.append(p)
        return Mesh(paths[0]),Mesh(paths[1])

    def native(self, profile, nodes):
        return {'map':'test','generation_complete':True,
            'bbox':{'half_width':profile['radius'],'stand_height':profile['standing_height'],
                    'crouch_height':profile['crouching_height'],'origin_above_feet':profile['origin_above_feet']},
            'nodes':[dict(id=i,raw_position=[*n['point'][:2],n['point'][2]+profile['origin_above_feet']],
                          accepted=True,reason='external_native_validated_floor',crouch=False,nav_flags=0)
                     for i,n in enumerate(nodes)],'edges':[]}

    def test_both_exact_profiles_and_duck_only_legacy_helper(self):
        for small,expected in [(False,(16,87,61,30)),(True,(14,70,49,24))]:
            with self.subTest(small=small):
                stand,duck=self.meshes(small)
                p=movement_profile(stand,duck)
                self.assertEqual(tuple(p[k] for k in ['radius','standing_height','crouching_height','origin_above_feet']),expected)
                self.assertEqual(movement_profile(None,duck),p)

    def test_mixed_invalid_or_missing_profiles_fail(self):
        for field,value in [('radius',14),('height',70),('step',19),('cell',8),('cell_height',4)]:
            with self.subTest(field=field):
                stand,duck=self.meshes();stand.profile[field]=value
                with self.assertRaisesRegex(ValueError,'profiles differ'):movement_profile(stand,duck)
        for field,value in [('radius',15),('height',49),('step',19),('radius',float('nan'))]:
            stand,duck=self.meshes();duck.profile[field]=value
            with self.assertRaises(ValueError):movement_profile(stand,duck)
        with self.assertRaises(ValueError):movement_profile(SimpleNamespace(profile={}),SimpleNamespace(profile={}))

    def test_spawn_projection_uses_map_origin_in_both_algorithms(self):
        for small,origin in [(False,30),(True,24)]:
            for algorithm in [build,build_visibility_legacy]:
                with self.subTest(small=small,algorithm=algorithm.__name__):
                    stand,duck=self.meshes(small,floor=18)
                    geometry={'map':'test','spawns':[{'entity':3,'classname':'team_CTF_redspawn','origin_bsp':[100,100,origin]}]}
                    with patch.object(duck,'closest',wraps=duck.closest) as closest:
                        nodes,edges,report=algorithm(stand,duck,geometry,320)
                    self.assertEqual(closest.call_args[0][0],[100,100,0])
                    self.assertFalse(report['spawn_failures'])
                    self.assertTrue(any(n['spawn'] for n in nodes))
                    self.assertEqual(report['movement_profile']['origin_above_feet'],origin)

    def test_feedback_coordinates_and_bbox_both_profiles(self):
        for small in [False,True]:
            stand,duck=self.meshes(small);profile=movement_profile(stand,duck)
            nodes=[dict(point=[50,50,0],polygon=0,component=0,crouch=False,spawn=True,kind='spatial_lattice_sample')]
            report={'map':'test','graph_policy':'preserve_spatial_samples','spatial_sampling':{'spacing':128}}
            native=self.native(profile,nodes);path=self.root/'native.json';path.write_text(json.dumps(native))
            self.assertEqual(add_native_feedback(nodes,[],report,stand,duck,path),[])
            self.assertEqual(report['movement_profile'],profile)
            relax_ordinary_chains(nodes,[],report,duck,native=native,stand=stand)
            native['nodes'][0]['raw_position'][2]+=6;path.write_text(json.dumps(native))
            with self.assertRaisesRegex(ValueError,'coordinate mismatch'):add_native_feedback(nodes,[],report,stand,duck,path)
            with self.assertRaisesRegex(ValueError,'coordinate mismatch'):relax_ordinary_chains(nodes,[],report,duck,native=native,stand=stand)

    def test_wrong_native_bbox_rejected_before_coordinate_validation(self):
        stand,duck=self.meshes(True);profile=movement_profile(stand,duck)
        for key in ['half_width','stand_height','crouch_height','origin_above_feet']:
            n=self.native(profile,[]);n['bbox'][key]+=1
            with self.assertRaisesRegex(ValueError,'movement profile differs'):validate_native_profile(profile,n)
        with self.assertRaisesRegex(ValueError,'movement profile differs'):validate_native_profile(profile,{})

    def test_cli_emits_profile_and_refuses_mismatched_mesh_without_output(self):
        stand,duck=self.meshes(True)
        geo=self.root/'geometry.json';geo.write_text(json.dumps({'map':'test','spawns':[]}));out=self.root/'candidate.txt'
        cmd=[sys.executable,str(Path(__file__).with_name('waygraph.py')),'--stand',str(stand.path),'--duck',str(duck.path),'--geometry',str(geo),'--out',str(out)]
        result=subprocess.run(cmd,capture_output=True,text=True)
        self.assertEqual(result.returncode,0,result.stderr)
        report=json.loads(out.with_suffix('.json').read_text());self.assertEqual(report['movement_profile'],movement_profile(stand,duck))
        mismatch=json.loads(stand.path.read_text());mismatch['profile']['radius']=16;stand.path.write_text(json.dumps(mismatch))
        fresh=self.root/'must-not-exist.txt';cmd[-1]=str(fresh)
        result=subprocess.run(cmd,capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0);self.assertIn('profiles differ',result.stderr)
        self.assertFalse(fresh.exists());self.assertFalse(fresh.with_suffix('.json').exists())


if __name__=='__main__':unittest.main()
