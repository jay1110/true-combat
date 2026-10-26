"""In-memory lossless package contract, no server/game/project writes."""
import copy
import hashlib
import io
import json
from unittest import TestCase, main
import zipfile

from navbundle import NavBundles


class ProjectPackageTests(TestCase):
    def setUp(self):
        self.name = 'obj_rain'
        self.way = b'\x00\xffWAY-preserved\r\n'
        self.goals = {'obj_rain_gt2.gm': b'// \xe4\r\nglobal Map={};\r\n',
                      'obj_rain_gt2_goals.gm': b'global _MG={};\r\n',
                      'obj_rain_gt5.gm': b'global Map={};\n',
                      'obj_rain_gt5_goals.gm': b'global _MG={};\n'}
        self.scripts = {'obj_rain_gt2.script': b'game_manager { spawn { } }\r\n',
                        'obj_rain_gt5.script': b'game_manager { spawn { } }\n',
                        'obj_rain_gt7.script': b'game_manager { spawn { tce_dmspawns 1 } }\n'}
        self.metadata = dict(mod='tce2', map=self.name, revision=4, receipt=dict(current=True))

    def package(self, **changes):
        args = dict(name=self.name, way_bytes=self.way, goal_files=self.goals,
                    map_scripts=self.scripts, metadata=self.metadata)
        args.update(changes)
        return NavBundles.archive_project(**args)

    def rewrite(self, data, mutate):
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            files = {name: archive.read(name) for name in archive.namelist()}
        mutate(files)
        output = io.BytesIO()
        with zipfile.ZipFile(output, 'w') as archive:
            for name, raw in files.items(): archive.writestr(name, raw)
        return output.getvalue()

    def test_complete_archive_preserves_bytes_and_binds_manifest(self):
        before = copy.deepcopy((self.goals,self.scripts,self.metadata))
        data = self.package()
        manifest = NavBundles.verify_project_archive(data)
        self.assertEqual(manifest['gametypes'],[2,5,7])
        self.assertEqual(manifest['goal_gametypes'],[2,5])
        self.assertEqual(manifest['mapscript_gametypes'],[2,5,7])
        payload = dict({'nav/obj_rain.way':self.way}, **{'nav/'+p:r for p,r in self.goals.items()},
                       **{'mapscripts/'+p:r for p,r in self.scripts.items()})
        with zipfile.ZipFile(io.BytesIO(data)) as archive:
            self.assertEqual(set(archive.namelist()),set(payload)|{'README.txt','manifest.json'})
            for path, raw in payload.items():
                self.assertEqual(archive.read(path),raw)
                self.assertEqual(manifest['files'][path],dict(bytes=len(raw),sha256=hashlib.sha256(raw).hexdigest()))
            self.assertIn(b'g_mapScriptDirectory "mapscripts"',archive.read('README.txt'))
            self.assertIn(b'tce_dmspawns 0',archive.read('README.txt'))
        self.assertEqual((self.goals,self.scripts,self.metadata),before)

    def test_equal_inputs_and_reordered_dicts_produce_identical_zip(self):
        self.assertEqual(self.package(),self.package(goal_files=dict(reversed(list(self.goals.items()))),
                                                    map_scripts=dict(reversed(list(self.scripts.items())))))

    def test_et_legacy_pair_and_optional_mapscript_remain_supported(self):
        args=dict(name='goldrush-ga',goal_files={'goldrush-ga.gm':b'global Map={};','goldrush-ga_goals.gm':b'global _MG={};'},
                  map_scripts={'goldrush-ga.script':b'game_manager {}'},metadata=dict(mod='et'))
        manifest=NavBundles.verify_project_archive(self.package(**args))
        self.assertEqual(manifest['mod'],'et');self.assertEqual(manifest['gametypes'],[2])
        args['map_scripts']={}
        self.assertEqual(NavBundles.verify_project_archive(self.package(**args))['mapscript_gametypes'],[])

    def test_explicit_gt7_only_spawn_package_has_no_invented_goals(self):
        data=self.package(goal_files={},map_scripts={'obj_rain_gt7.script':self.scripts['obj_rain_gt7.script']},
                          metadata=dict(mod='tce2',gametypes=[7]))
        manifest=NavBundles.verify_project_archive(data)
        self.assertEqual(manifest['gametypes'],[7])
        self.assertEqual(manifest['goal_gametypes'],[])
        self.assertEqual(manifest['mapscript_gametypes'],[7])
        self.assertFalse(any(path.endswith('.gm')for path in manifest['files']))
        for metadata in ({'mod':'tce2'},{'mod':'tce2','gametypes':[2,7]}):
            with self.assertRaises(ValueError):self.package(goal_files={},map_scripts={'obj_rain_gt7.script':b'code'},metadata=metadata)

    def test_unsafe_wrong_map_invalid_gt_and_case_aliases_rejected(self):
        for name in ('../obj_rain','C:/obj_rain','obj_rain/', '', 'a'*65, 'CON', 'LPT1'):
            with self.subTest(name=name), self.assertRaises(ValueError):self.package(name=name)
        for filename in ('../obj_rain_gt2.gm','obj_other_gt2.gm','obj_rain_gt3.gm','obj_rain_gt2.gm:secret','obj_rain_gt2.gm\\x'):
            with self.subTest(filename=filename),self.assertRaises(ValueError):self.package(goal_files={filename:b'code'})
        duplicate=dict(self.goals,OBJ_RAIN_GT2_Goals_gm=b'bad')
        duplicate['OBJ_RAIN_GT2.GM']=b'code'
        with self.assertRaises(ValueError):self.package(goal_files=duplicate)
        with self.assertRaises(ValueError):self.package(map_scripts={'../obj_rain_gt7.script':b'code'})

    def test_incomplete_pairs_missing_mapscripts_and_nonempty_limits(self):
        missing=dict(self.goals);missing.pop('obj_rain_gt5_goals.gm')
        with self.assertRaises(ValueError):self.package(goal_files=missing)
        missing=dict(self.scripts);missing.pop('obj_rain_gt5.script')
        with self.assertRaises(ValueError):self.package(map_scripts=missing)
        with self.assertRaises(ValueError):self.package(way_bytes=b'')
        with self.assertRaises(ValueError):self.package(goal_files={})
        with self.assertRaises(ValueError):self.package(goal_files=dict(self.goals,**{'obj_rain_gt2.gm':b' \n'}))
        with self.assertRaises(ValueError):self.package(way_bytes=b'x'*(16*1024*1024+1))
        with self.assertRaises(ValueError):self.package(metadata={'mod':'tce2','bad':float('nan')})

    def test_payload_tampering_and_missing_payload_rejected(self):
        data=self.package()
        for mutate in (lambda files:files.update({'nav/obj_rain.way':b'changed'}),
                       lambda files:files.pop('mapscripts/obj_rain_gt5.script'),
                       lambda files:files.update({'nav/obj_unlisted.gm':b'extra'})):
            with self.assertRaises(ValueError):NavBundles.verify_project_archive(self.rewrite(data,mutate))

    def test_manifest_and_traversal_tampering_rejected_without_extraction(self):
        def fake(files):
            manifest=json.loads(files['manifest.json']);manifest['gametypes']=[2,5]
            files['manifest.json']=json.dumps(manifest).encode()
        with self.assertRaises(ValueError):NavBundles.verify_project_archive(self.rewrite(self.package(),fake))
        with self.assertRaises(ValueError):NavBundles.verify_project_archive(self.rewrite(self.package(),lambda files:files.update({'../escape':b'bad'})))
        with self.assertRaises(ValueError):NavBundles.verify_project_archive(b'not a zip')


if __name__ == '__main__':main()
