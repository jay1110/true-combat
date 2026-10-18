"""Only literal resolved GT scripts authorize goals; no Engine or API."""
import hashlib
import json
from pathlib import Path
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import zipfile

import mapscript
from goalgen import generate,apply
import test_goalgen as goal_fixtures


class AvailabilityTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.base=self.root/'etmain';self.mod=self.root/'tce2'
        self.base.mkdir();self.mod.mkdir();self.assets=SimpleNamespace(roots=[self.base,self.mod])

    def archive(self,root,name,files):
        with zipfile.ZipFile(root/name,'w')as z:
            for member,raw in files.items():z.writestr(member,raw)

    def test_exact_gt_filename_required_generic_fallback_never_proves_another_gt(self):
        self.archive(self.mod,'map.pk3',{'maps/obj_test.script':'generic','maps/OBJ_TEST_GT2.script':'two'})
        value=mapscript.availability(self.assets,'obj_test')
        self.assertEqual(value['available'],[2]);self.assertFalse(value['gametypes']['5']['available'])
        self.assertEqual(mapscript.read(self.assets,'obj_test',gametype=5)['text'],'generic')
        self.assertEqual(value['gametypes']['5']['resolved']['selected']['relative'],'maps/obj_test.script')

    def test_actual_loose_overrides_last_archive_and_mod_root_with_preserved_case(self):
        self.archive(self.base,'z.pk3',{'maps/obj_test_gt2.script':'base'})
        self.archive(self.mod,'a.pk3',{'maps/obj_test_gt2.script':'earlier'})
        self.archive(self.mod,'z.pk3',{'Maps/Obj_Test_GT2.script':'later'})
        self.assertEqual(mapscript.read(self.assets,'obj_test',gametype=2)['text'],'later')
        (self.mod/'Maps').mkdir();loose=self.mod/'Maps/Obj_Test_GT2.Script';loose.write_bytes(b'loose')
        result=mapscript.read(self.assets,'obj_test',gametype=2)
        self.assertEqual(result['text'],'loose');self.assertEqual(result['name'],'Maps/Obj_Test_GT2.Script')
        self.assertEqual(result['sha256'],hashlib.sha256(b'loose').hexdigest())

    def test_exact_custom_override_can_add_gt_but_generic_override_cannot_prove_gt(self):
        self.archive(self.mod,'map.pk3',{'maps/obj_test_gt2.script':'two','maps/obj_test_gt5.script':'five',
            'Custom/obj_test.script':'custom generic','Custom/obj_test_gt7.script':'seven'})
        value=mapscript.availability(self.assets,'obj_test','CUSTOM/')
        self.assertEqual(value['available'],[7])
        self.assertEqual(value['gametypes']['2']['resolved']['selected']['relative'],'custom/obj_test.script')

    def test_empty_custom_override_falls_through_but_empty_exact_map_script_is_unavailable(self):
        self.archive(self.mod,'map.pk3',{'maps/obj_test_gt2.script':'two','maps/obj_test_gt5.script':'',
            'mapscripts/obj_test_gt2.script':'','mapscripts/obj_test.script':''})
        self.assertEqual(mapscript.availability(self.assets,'obj_test','mapscripts')['available'],[2])

    def test_invalid_directory_matches_engine_maps_fallback_and_ignores_other_gt_names(self):
        self.archive(self.mod,'map.pk3',{'maps/obj_test_gt5.script':'five','maps/obj_test_gt3.script':'three'})
        value=mapscript.availability(self.assets,'obj_test','../outside')
        self.assertEqual(value['available'],[5]);self.assertFalse(value['gametypes']['5']['resolved']['directory_valid'])

    def test_no_scripts_or_missing_vfs_provenance_never_invents_a_gametype(self):
        self.assertEqual(mapscript.availability(self.assets,'obj_test')['available'],[])
        with self.assertRaisesRegex(ValueError,'VFS roots'):mapscript.availability(object(),'obj_test')

    def test_adding_removing_and_editing_loose_script_rechecks_actual_bytes_without_cached_assets(self):
        (self.mod/'maps').mkdir();path=self.mod/'maps/obj_test_gt5.script';path.write_bytes(b'first')
        first=mapscript.availability(self.assets,'obj_test');path.write_bytes(b'second')
        self.assertNotEqual(first,mapscript.availability(self.assets,'obj_test'))
        path.unlink();self.assertEqual(mapscript.availability(self.assets,'obj_test')['available'],[])

    def test_bulk_picker_uses_one_vfs_scan_and_keeps_each_maps_actual_gametypes(self):
        self.archive(self.mod,'map.pk3',{'maps/obj_one_gt2.script':'two','maps/obj_two_gt5.script':'five'})
        with patch('mapscript._sources',wraps=mapscript._sources)as scan:
            rows=mapscript.availability_many(self.assets,['obj_one','obj_two','obj_one'])
        self.assertEqual(scan.call_count,1);self.assertEqual(rows['obj_one']['available'],[2]);self.assertEqual(rows['obj_two']['available'],[5])

    def test_unreadable_pk3_is_excluded_like_assets_instead_of_masking_a_valid_script(self):
        self.archive(self.mod,'map.pk3',{'maps/obj_test_gt2.script':'two'})
        (self.mod/'zbroken.pk3').write_bytes(b'not an archive')
        self.assertEqual(mapscript.availability(self.assets,'obj_test')['available'],[2])

    def test_gt7_spawn_generation_cannot_promote_a_generic_script(self):
        self.archive(self.mod,'map.pk3',{'maps/obj_test.script':'game_manager { spawn { } }'})
        nodes=[dict(id=i,position=[i*512,0,30],accepted=True,spawn_reachable=True,crouch=False,nav_flags=0)for i in range(6)]
        studio=SimpleNamespace(require_map=lambda n:n,game_mode='tce2',mod=self.mod,assets=self.assets,
            proof_status=lambda n:dict(valid=True),native=lambda n:dict(nodes=nodes,edges=[dict(**{'from':i,'to':(i+1)%6},accepted=True)for i in range(6)]))
        with self.assertRaisesRegex(ValueError,'Exaktes GT7-Mapscript fehlt'):mapscript.generate_spawns(studio,'obj_test')
        self.assertFalse((self.mod/'mapscripts').exists())


class AvailableGoalFilesTests(unittest.TestCase):
    setUp=goal_fixtures.GoalFilesTests.setUp
    report=goal_fixtures.GoalPlanTests.report
    def test_only_gt5_script_generates_only_gt5_pair_and_reports_skipped_gt2(self):
        (self.root/'assets/maps/obj_test_gt2.script').unlink()
        with patch('goalgen.objectives',return_value=[]):result=generate(self.studio,'obj_test')
        self.assertEqual(result['generated_gametypes'],[5]);self.assertEqual(result['skipped_unavailable_gametypes'],[2])
        self.assertEqual(sorted(p.name for p in(self.root/'bundles').iterdir()),['obj_test_gt5.gm','obj_test_gt5_goals.gm'])
        manifest=json.loads(Path(result['manifest']).read_bytes())
        self.assertEqual(manifest['plans'][0]['map_script_source']['selected']['relative'],'maps/obj_test_gt5.script')

    def test_missing_all_exact_scripts_refuses_before_any_gm_write(self):
        for path in(self.root/'assets/maps').iterdir():path.unlink()
        (self.root/'assets/maps/obj_test.script').write_text('generic fallback')
        with self.assertRaisesRegex(ValueError,'exaktes GT-Mapscript'):generate(self.studio,'obj_test')
        self.assertFalse(list((self.root/'bundles').iterdir()))

    def test_previously_owned_unavailable_phantom_pair_removed_but_edited_file_preserved(self):
        with patch('goalgen.objectives',return_value=[]):generate(self.studio,'obj_test')
        edited=self.root/'bundles/obj_test_gt5.gm';edited.write_bytes(b'// author changes')
        (self.root/'assets/maps/obj_test_gt5.script').unlink()
        with patch('goalgen.objectives',return_value=[]):result=generate(self.studio,'obj_test')
        self.assertEqual(edited.read_bytes(),b'// author changes')
        self.assertFalse((self.root/'bundles/obj_test_gt5_goals.gm').exists())
        self.assertEqual(result['pruned_owned_unavailable_files'],['obj_test_gt5_goals.gm'])
        self.assertEqual(result['generated_gametypes'],[2])

    def test_script_change_during_generation_cannot_write_new_files(self):
        path=self.root/'assets/maps/obj_test_gt5.script'
        def change(*args):path.write_bytes(b'changed while planning');return []
        with patch('goalgen.objectives',side_effect=change),self.assertRaisesRegex(RuntimeError,'GT-Mapscripts/Overrides'):
            generate(self.studio,'obj_test')
        self.assertFalse(list((self.root/'bundles').iterdir()))

    def test_transaction_failure_restores_pruned_phantom_files_and_existing_manifest(self):
        with patch('goalgen.objectives',return_value=[]):generate(self.studio,'obj_test')
        paths=[*list((self.root/'bundles').iterdir()),self.root/'work/generated-goals.json'];before={p:p.read_bytes()for p in paths}
        (self.root/'assets/maps/obj_test_gt5.script').unlink()
        with patch('goalgen.objectives',return_value=[]),patch('projects.write_json',side_effect=OSError('write refused')),self.assertRaisesRegex(OSError,'write refused'):
            generate(self.studio,'obj_test')
        self.assertEqual({p:p.read_bytes()for p in paths},before)

    def test_apply_missing_gametype_refuses_before_game_stop_or_any_copy(self):
        (self.root/'assets/maps/obj_test_gt5.script').unlink()
        self.studio.stop_owned_test=unittest.mock.Mock()
        self.studio.own_test_process=unittest.mock.Mock(side_effect=AssertionError('No Engine/process access'))
        with self.assertRaisesRegex(ValueError,'GT5.*nicht vorhanden'):apply(self.studio,dict(map='obj_test',gametype=5))
        self.studio.own_test_process.assert_not_called();self.studio.stop_owned_test.assert_not_called()
        self.assertFalse(list((self.root/'bundles').iterdir()))


if __name__=='__main__':unittest.main()
