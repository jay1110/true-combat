"""Real Studio catalog/activation use the Engine cohort before source leasing."""
import hashlib
from pathlib import Path
import tempfile
from unittest import TestCase,main
from unittest.mock import Mock,patch
import zipfile

from projects import Projects
import server


class CatalogServerProjectionTests(TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.workspace=self.root/'workspace';self.game=self.root/'game'
        self.library=self.workspace/'etmain/all_maps';self.base=self.game/'etmain';self.mod=self.game/'tce2'
        for p in(self.library,self.workspace/'tce2',self.base,self.mod,self.game/'omnibot'):p.mkdir(parents=True)
        self.studio=server.Studio.__new__(server.Studio)
        self.studio.workspace=self.workspace;self.studio.test=self.game;self.studio.mod=self.mod;self.studio.game_mode='tce2'
        self.studio.maps_dirs=[self.library];self.studio.active_project=None
        self.studio.projects=Projects(self.game/'projects',self.base,[self.library])
        self.studio.rcon=Mock(side_effect=AssertionError('No Engine/RCON in catalog projection'))

    def archive(self,directory,name,files):
        path=directory/name
        with zipfile.ZipFile(path,'w')as z:
            for key,value in files.items():z.writestr(key,value)
        return path

    def assets(self):
        # This broad preview cohort is intentional: project_state must not
        # mistake workspace/library scripts for the actual Engine's VFS.
        self.studio.assets=server.Assets([self.workspace/'etmain',self.workspace/'tce2',self.library,self.base,self.mod])

    def files(self):return {str(p.relative_to(self.root)):p.read_bytes()for p in self.root.rglob('*')if p.is_file()}

    def row(self,state,path,name='obj_test'):
        return next(row for row in state['catalog']if row['source_path']==str(path.resolve())and row['map']==name)

    def test_actual_project_state_reports_selected_gt5_only_before_lease_without_workspace_or_library_bleed(self):
        self.archive(self.workspace/'etmain','workspace.pk3',{'maps/obj_test.bsp':b'workspace map',
            'maps/obj_test_gt2.script':'unrelated workspace GT2'})
        self.archive(self.workspace/'tce2','workspace-mod.pk3',{'maps/obj_test_gt7.script':'unrelated workspace mod GT7'})
        other=self.archive(self.library,'obj_variant_011.pk3',{'maps/obj_test.bsp':b'other version',
            'maps/obj_test_gt2.script':'other unleased library GT2'})
        selected=self.archive(self.library,'obj_variant_012.pk3',{'maps/obj_test.bsp':b'selected version',
            'maps/obj_test_gt5.script':'selected GT5'})
        self.assets();before=self.files();folders={str(p.relative_to(self.root))for p in self.root.rglob('*')if p.is_dir()}
        with patch.object(self.studio.projects,'acquire',side_effect=AssertionError('No lease from project_state')):
            result=self.studio.project_state()
        picked=self.row(result,selected);self.assertEqual(picked['gametype_availability']['available'],[5])
        self.assertEqual(self.row(result,other)['gametype_availability']['available'],[2])
        source=picked['gametype_availability']['gametypes']['5']['resolved']['selected']['sources'][0]
        self.assertEqual(source['path'],str(selected.resolve()));self.assertEqual(source['member'],'maps/obj_test_gt5.script')
        self.assertEqual(picked['gametype_availability']['catalog_projection']['target_path'],str(self.base/selected.name))
        self.assertEqual(result['active'],None);self.assertEqual(result['projects'],[]);self.assertEqual(result['recovery'],[])
        self.assertEqual(self.files(),before);self.assertEqual({str(p.relative_to(self.root))for p in self.root.rglob('*')if p.is_dir()},folders)
        self.assertFalse((self.base/selected.name).exists());self.assertTrue(selected.is_file());self.studio.rcon.assert_not_called()

    def test_actual_project_state_retains_real_base_and_later_mod_overrides(self):
        selected=self.archive(self.library,'m_selected.pk3',{'maps/obj_test.bsp':b'library BSP','maps/obj_test_gt5.script':'selected'})
        self.archive(self.base,'z_installed.pk3',{'maps/obj_test_gt2.script':'installed GT2','maps/obj_test_gt5.script':'later base GT5'})
        mod=self.archive(self.mod,'a_mod.pk3',{'Maps/Obj_Test_GT5.script':'mod override'})
        self.assets();before=self.files();picked=self.row(self.studio.project_state(),selected)
        self.assertEqual(picked['gametype_availability']['available'],[2,5])
        resolved=picked['gametype_availability']['gametypes']['5']['resolved']['selected']
        self.assertEqual(resolved['sources'][0]['path'],str(mod.resolve()))
        self.assertEqual(resolved['sha256'],hashlib.sha256(b'mod override').hexdigest())
        (self.mod/'mapscripts').mkdir();custom=self.mod/'mapscripts/obj_test.script';custom.write_text('generic custom override')
        picked=self.row(self.studio.project_state(),selected)
        self.assertEqual(picked['gametype_availability']['available'],[])
        self.assertIn('mapscripts/obj_test.script',picked['gametype_availability']['gametypes']['5']['reasons'][0])
        after=self.files();after.pop(str(custom.relative_to(self.root)));self.assertEqual(after,before)
        self.studio.rcon.assert_not_called()

    def test_et_catalog_rows_remain_unchanged(self):
        et=self.archive(self.library,'goldrush.pk3',{'maps/goldrush.bsp':b'ET BSP','maps/goldrush.script':'ET script'})
        self.archive(self.library,'obj_test.pk3',{'maps/obj_test.bsp':b'TC BSP','maps/obj_test_gt5.script':'GT5'})
        self.assets();plain,_=self.studio.projects.catalog(self.studio.assets.maps)
        original=next(row for row in plain if row['source_path']==str(et.resolve()))
        actual=self.row(self.studio.project_state(),et,'goldrush')
        self.assertEqual(actual,original);self.assertNotIn('gametype_availability',actual)

    def test_actual_activate_project_uses_base_then_active_mod_without_workspace_or_library_roots(self):
        self.archive(self.workspace/'etmain','workspace.pk3',{'maps/obj_test_gt2.script':'workspace only'})
        self.archive(self.library,'library.pk3',{'maps/obj_test_gt2.script':'library only'})
        self.archive(self.base,'m_leased.pk3',{'maps/obj_test.bsp':b'leased BSP','maps/obj_test_gt5.script':'base GT5'})
        mod=self.archive(self.mod,'a_mod.pk3',{'maps/obj_test_gt5.script':'active TCE mod'})
        self.archive(self.game/'omnibot','et.pk3',{'maps/obj_test_gt7.script':'must not leak across mods'})
        active=dict(id='fixture',map='obj_test',mod='tce2',gametype=5,
            paths={key:str(self.game/'projects/fixture'/key)for key in('native','preview','work','drafts')})
        self.studio.native_cached={'old':1};self.studio.hash_cached={'old':1};self.studio.game_state_cache='stale';self.studio.jobs={'old':1}
        before=self.files()
        with patch('server.Assets',wraps=server.Assets)as assets:
            self.studio.activate_project(active)
        assets.assert_called_once_with([self.base,self.mod])
        self.assertEqual(self.studio.assets.roots,[self.base.resolve(),self.mod.resolve()])
        available=self.studio.available_game_types('obj_test')
        self.assertEqual(available['available'],[5])
        self.assertEqual(available['gametypes']['5']['resolved']['selected']['sources'][0]['path'],str(mod.resolve()))
        self.assertEqual(self.studio.native_cached,{});self.assertEqual(self.studio.hash_cached,{})
        self.assertEqual(self.studio.jobs,{});self.assertIsNone(self.studio.game_state_cache)
        self.assertEqual(self.files(),before);self.studio.rcon.assert_not_called()


if __name__=='__main__':main()
