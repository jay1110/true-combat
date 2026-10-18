import hashlib
import io
from pathlib import Path
import tempfile
from unittest import TestCase, main
import zipfile
from navbundle import NavBundles, goal_overlay
from server import Studio


class BundleTests(TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.bundle = NavBundles(self.root)
        self.original = {'goldrush-ga.way': b'\x00\xffbinary',
                         'goldrush-ga.gm': b'// \xe4\r\nglobal Map = {};\r\n',
                         'goldrush-ga_goals.gm': b'global _MG = {};\r\n'}
        for name, content in self.original.items(): (self.root / name).write_bytes(content)

    def tearDown(self): self.temp.cleanup()

    def test_serialized_goal_overlay_retains_real_type_fields_vectors_and_name(self):
        data=goal_overlay('''// native goal_save serializer
global _MG = {
 Version = 1,
 ["SWITCH_frontdoor"] = {
  GoalType = "SWITCH", Position = Vec3(-1.25,2e2,.125), Radius = 25,
  TagName = "Front Door Switch", Stance = "stand", Timeout = 1500, Use = 1,
  TeamAvailability = 6, CreateOnLoad = 0, AimVectors = {[0] = Vec3(1,0,0),},
 },
};''')
        self.assertNotIn('error',data);goal=data['goals'][0]
        self.assertEqual(goal['id'],'SWITCH_frontdoor');self.assertEqual(goal['position'],[-1.25,200,.125])
        self.assertEqual(goal['properties']['Timeout'],1500);self.assertEqual(goal['properties']['Use'],1)
        self.assertEqual(goal['properties']['AimVectors']['0'],[1,0,0]);self.assertFalse(goal['live'])
        self.assertNotIn('Active',goal['properties'])

    def test_goal_overlay_never_executes_or_guesses_scripted_expression(self):
        for text in ['global _MG={X={GoalType="CAMP",Position=ReadFile("x"),},};',
                     'global _MG={}; ExecuteCommand("quit");',
                     'global _MG={CAMP_x={Position=Vec3(1,2,3),AimVectors={ReadFile("x"),},},};',
                     'global _MG={CAMP_x={Position=Vec3(1,2,3),AimVectors={Vec3(1,0,0)+Vec3(0,1,0),},},};',
                     'global _MG={X={GoalType="CAMP",Position=Vec3(1e999,0,0),},};']:
            data=goal_overlay(text);self.assertEqual(data['goals'],[]);self.assertIn('error',data)

    def test_native_saved_stadtrand_all_29_goals_and_switch_values_are_lossless(self):
        # Exact output of the native goal_save editor proof, not hand-normalized.
        fixture=Path(__file__).with_name('fixtures')/'native_stadtrand_gt2_saved_goals.gm'
        raw=fixture.read_bytes()
        self.assertEqual(hashlib.sha256(raw).hexdigest(),'d20f377b58aaec6a305c16945252c97bd65407f97e498f82dc2bd1d96a45b6f0')
        path=NavBundles(self.root,2).path('obj_stadtrand','goals');path.write_bytes(raw)
        result=NavBundles(self.root,2).read('obj_stadtrand');overlay=result['goal_overlay']
        self.assertNotIn('error',overlay);self.assertEqual(overlay['skipped'],[])
        goals={goal['name']:goal for goal in overlay['goals']}
        self.assertEqual(len(goals),29)
        self.assertEqual({kind:sum(goal['type']==kind for goal in goals.values())
                          for kind in ('ATTACK','CAMP','CAPPOINT','DEFEND','FLAG','SWITCH')},
                         {'ATTACK':2,'CAMP':20,'CAPPOINT':2,'DEFEND':2,'FLAG':2,'SWITCH':1})
        attack=goals['ATTACK_to_capture_point_A']
        self.assertEqual(attack['properties']['AimVectors'],{'0':[.736,.676,.039]})
        self.assertNotIn('GoalType',attack['properties'])
        switch=goals['SWITCH_NavStudio_EditorProof_20261014']
        self.assertEqual(switch['type'],'SWITCH');self.assertEqual(switch['schema_defaults'],{})
        self.assertEqual({key:switch['effective_properties'][key] for key in ('Stance','Timeout','Use')},
                         {'Stance':'crouch','Timeout':2500,'Use':0})
        self.assertFalse(switch['live']);self.assertNotIn('Active',switch['effective_properties'])
        self.assertEqual(path.read_bytes(),raw);self.assertEqual(fixture.read_bytes(),raw)
        self.assertEqual(overlay['sha256'],hashlib.sha256(raw).hexdigest())

    def test_unkeyed_native_values_use_zero_based_indices_independent_of_explicit_keys(self):
        data=goal_overlay('''global _MG={CAMP_array={Position=Vec3(1,2,3),
 AimVectors={Vec3(1,0,0), [8]=Vec3(8,0,0), Vec3(0,1,0),},
 Roles={"ATTACKER", "DEFENDER",}, Nested={{Vec3(0,0,1),}, true, null,},},};''')
        self.assertNotIn('error',data);properties=data['goals'][0]['properties']
        self.assertEqual(properties['AimVectors'],{'0':[1,0,0],'8':[8,0,0],'1':[0,1,0]})
        self.assertEqual(properties['Roles'],{'0':'ATTACKER','1':'DEFENDER'})
        self.assertEqual(properties['Nested'],{'0':{'0':[0,0,1]},'1':True,'2':None})

    def test_native_name_prefix_fallback_and_artillery_type_match_loader(self):
        data=goal_overlay('''global _MG={
 Alias={Name="SWITCH_button",Position=Vec3(1,2,3),},
 ARTILLERY_S_spot={Position=Vec3(1,2,3),},
 ARTILLERY_D_target={Position=Vec3(1,2,3),},
 CAMP_explicit={GoalType="DEFEND",Position=Vec3(1,2,3),},
 CAMP_nullname={Name=null,Position=Vec3(1,2,3),},
 NoPrefix={Position=Vec3(1,2,3),},
 };''')
        self.assertNotIn('error',data);goals={goal['id']:goal for goal in data['goals']}
        self.assertEqual(data['skipped'],['NoPrefix'])
        self.assertEqual(goals['Alias']['name'],'SWITCH_button');self.assertEqual(goals['Alias']['type'],'SWITCH')
        self.assertEqual(goals['Alias']['schema_defaults'],{'Stance':'stand','Timeout':1500,'Use':1})
        self.assertEqual(goals['ARTILLERY_S_spot']['type'],'ARTILLERY_S')
        self.assertEqual(goals['ARTILLERY_D_target']['type'],'ARTILLERY_D')
        self.assertEqual(goals['CAMP_explicit']['type'],'DEFEND')
        self.assertEqual(goals['CAMP_nullname']['name'],'CAMP_nullname')

    def test_switch_save_omits_defaults_but_preview_labels_native_origin_without_rewriting(self):
        path=self.bundle.path('obj_default','goals')
        raw=b'global _MG={Version=1,SWITCH_default={GoalType="SWITCH",Position=Vec3(1,2,3),Radius=35,Version=1,},};'
        path.write_bytes(raw)
        result=self.bundle.read('obj_default');goal=result['goal_overlay']['goals'][0]
        expected={'Stance':'stand','Timeout':1500,'Use':1}
        self.assertEqual(goal['schema_defaults'],expected)
        for key,value in expected.items():
            self.assertNotIn(key,goal['properties'])
            self.assertEqual(goal['effective_properties'][key],value)
        self.assertEqual(goal['schema_default_source'],'mapgoal_switch.gm · SWITCH v1')
        self.assertNotIn('Active',goal['effective_properties']);self.assertFalse(goal['live'])
        self.assertEqual(path.read_bytes(),raw)
        self.assertEqual(result['goal_overlay']['sha256'],hashlib.sha256(raw).hexdigest())

    def test_switch_explicit_zero_and_custom_values_are_never_replaced_by_defaults(self):
        goal=goal_overlay('global _MG={SWITCH_custom={GoalType="SWITCH",Position=Vec3(1,2,3),Stance="crouch",Timeout=0,Use=0,},};')['goals'][0]
        self.assertEqual(goal['schema_defaults'],{})
        self.assertIsNone(goal['schema_default_source'])
        self.assertEqual(goal['effective_properties'],goal['properties'])
        other=goal_overlay('global _MG={CAMP_example={GoalType="CAMP",Position=Vec3(1,2,3),},};')['goals'][0]
        self.assertEqual(other['schema_defaults'],{})
        self.assertNotIn('Timeout',other['effective_properties'])

    def test_goal_overlay_uses_exact_gametype_and_hash_without_rewriting(self):
        pair=NavBundles(self.root,5);path=pair.path('obj_test','goals')
        raw=b'global _MG={CAMP_real={GoalType="CAMP",Position=Vec3(1,2,3),Radius=80,},};'
        path.write_bytes(raw);result=pair.read('obj_test')
        self.assertEqual(result['goal_overlay']['sha256'],hashlib.sha256(raw).hexdigest())
        self.assertEqual(result['goal_overlay']['goals'][0]['radius'],80)
        self.assertEqual(NavBundles(self.root,2).read('obj_test')['goal_overlay']['goals'],[])
        self.assertEqual(path.read_bytes(),raw)

    def test_goal_overlay_legacy_fallback_requires_both_files_and_no_partial_gt_pair(self):
        raw='global _MG={CAMP_legacy={GoalType="CAMP",Position=Vec3(1,2,3),Radius=35,},};'
        (self.root/'obj_shared_goals.gm').write_text(raw)
        selected=NavBundles(self.root,5)
        self.assertFalse(selected.read('obj_shared')['goal_overlay']['fallback'])
        (self.root/'obj_shared.gm').write_text('global Map={};')
        overlay=selected.read('obj_shared')['goal_overlay']
        self.assertTrue(overlay['fallback']);self.assertEqual(overlay['goals'][0]['name'],'CAMP_legacy')
        self.assertTrue(overlay['file'].endswith('obj_shared_goals.gm'))
        selected.path('obj_shared','script').write_text('global Map={};')
        overlay=selected.read('obj_shared')['goal_overlay']
        self.assertFalse(overlay['fallback']);self.assertEqual(overlay['goals'],[])

    def test_exact_stem_and_goal_suffix(self):
        self.assertEqual(self.bundle.names(), ['goldrush-ga'])
        self.assertTrue(self.bundle.read('goldrush-ga')['complete'])
        self.assertFalse(any(f['exists'] for f in self.bundle.read('goldrush')['files'].values()))

    def test_read_is_nonexecuting_and_keeps_bytes(self):
        source = self.bundle.read('goldrush-ga')['files']['script']
        self.assertEqual(source['encoding'], 'cp1252')
        self.assertIn('ä', source['text'])
        for name, content in self.original.items(): self.assertEqual((self.root/name).read_bytes(), content)

    def test_archive_contains_original_three_files(self):
        with zipfile.ZipFile(io.BytesIO(self.bundle.archive('goldrush-ga'))) as archive:
            self.assertEqual(set(archive.namelist()), set(self.original))
            for name, content in self.original.items(): self.assertEqual(archive.read(name), content)

    def test_save_backs_up_exact_bytes_preserves_encoding_and_other_files(self):
        old = self.bundle.read('goldrush-ga')['files']['script']
        result = self.bundle.save('goldrush-ga','script','// ö\nglobal Map = {};\n',old['sha256'])
        self.assertEqual(Path(result['backup']).read_bytes(), self.original['goldrush-ga.gm'])
        self.assertEqual((self.root/'goldrush-ga.gm').read_bytes(), b'// \xf6\r\nglobal Map = {};\r\n')
        for name in ('goldrush-ga.way','goldrush-ga_goals.gm'):
            self.assertEqual((self.root/name).read_bytes(), self.original[name])

    def test_external_change_rejected_without_overwrite(self):
        old = self.bundle.read('goldrush-ga')['files']['goals']
        path = self.root/'goldrush-ga_goals.gm'; path.write_bytes(b'new external goals')
        with self.assertRaises(RuntimeError): self.bundle.save('goldrush-ga','goals','replacement',old['sha256'])
        self.assertEqual(path.read_bytes(), b'new external goals')

    def test_missing_companions_not_invented(self):
        self.assertFalse(self.bundle.read('missing')['complete'])
        with self.assertRaises(ValueError): self.bundle.save('missing','script','global Map={};',None)

    def test_noop_creates_no_backup(self):
        old = self.bundle.read('goldrush-ga')['files']['script']
        self.assertFalse(self.bundle.save('goldrush-ga','script',old['text'],old['sha256'])['saved'])
        self.assertFalse((self.root/'.navstudio-backups').exists())

    def test_path_and_binary_edit_rejected(self):
        for name in ('../goldrush', 'C:/goldrush', 'goldrush/ga', ''):
            with self.assertRaises(ValueError): self.bundle.read(name)
        with self.assertRaises(ValueError): self.bundle.save('goldrush-ga','way','data',None)

    def test_studio_selects_original_et_folder_separately(self):
        studio = Studio.__new__(Studio)
        studio.test = self.root
        studio.game_mode = 'et'; studio.mod = self.root/'omnibot'
        self.assertEqual(studio.nav_bundles().root, self.root/'omnibot/et/nav')
        studio.game_mode = 'tce2'; studio.mod = self.root/'tce2'
        self.assertEqual(studio.nav_bundles().root, self.root/'omni-bot/et/nav')


if __name__ == '__main__': main()
