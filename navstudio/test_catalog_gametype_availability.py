"""Pre-lease GT picker uses each source in the real base/mod VFS; no Engine."""
import copy
import hashlib
from pathlib import Path
import tempfile
from types import SimpleNamespace
from unittest import TestCase,main
from unittest.mock import patch
import zipfile

import mapscript


class CatalogAvailabilityTests(TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name);self.base=self.root/'etmain';self.mod=self.root/'tce2';self.library=self.root/'all_maps'
        for p in(self.base,self.mod,self.library):p.mkdir()
        self.assets=SimpleNamespace(roots=[self.base,self.mod],catalog_base_root=self.base)

    def archive(self,directory,name,files):
        path=directory/name
        with zipfile.ZipFile(path,'w')as z:
            for member,data in files.items():z.writestr(member,data)
        return path

    def row(self,path,name='obj_test',mod='tce2'):
        return dict(source_id=hashlib.sha256(str(path).encode()).hexdigest(),map=name,mod=mod,filename=path.name,
                    source_path=str(path),location='all_maps'if path.parent==self.library else'installed',
                    size=path.stat().st_size,stamp=[path.stat().st_mtime_ns,path.stat().st_size])

    def call(self,rows,directory='mapscripts'):
        return mapscript.catalog_availability(self.assets,rows,directory)

    def picked(self,row,gt):return row['gametype_availability']['gametypes'][str(gt)]['resolved']['selected']

    def test_same_bsp_name_different_library_pk3s_have_distinct_actual_gt_sets_without_mutations(self):
        one=self.archive(self.library,'obj_variant_011.pk3',{'maps/obj_test_gt2.script':b'first GT2'})
        two=self.archive(self.library,'obj_variant_012.pk3',{'maps/obj_test_gt5.script':b'second GT5'})
        et=dict(self.row(one),map='goldrush',mod='et');rows=[self.row(one),self.row(two),et];before=copy.deepcopy(rows)
        files={p:p.read_bytes()for p in self.root.rglob('*')if p.is_file()}
        values=self.call(rows)
        self.assertEqual(rows,before);self.assertEqual(values[2],et)
        self.assertEqual([r['gametype_availability']['available']for r in values[:2]],[[2],[5]])
        self.assertEqual(self.picked(values[0],2)['sources'][0]['path'],str(one))
        self.assertEqual(self.picked(values[1],5)['sources'][0]['path'],str(two))
        self.assertTrue(values[0]['gametype_availability']['gametypes']['5']['reasons'])
        projection=values[0]['gametype_availability']['catalog_projection']
        self.assertEqual(projection['target_path'],str(self.base/one.name));self.assertFalse(projection['physical_files_changed'])
        self.assertEqual({p:p.read_bytes()for p in self.root.rglob('*')if p.is_file()},files)
        self.assertEqual(list(self.base.iterdir()),[])

    def test_selected_original_basename_replaces_same_virtual_target_but_keeps_other_installed_packages(self):
        old=self.archive(self.base,'m_map.pk3',{'maps/obj_test_gt2.script':'old target only GT2'})
        selected=self.archive(self.library,'m_map.pk3',{'maps/obj_test_gt5.script':'selected GT5'})
        earlier=self.archive(self.base,'a_other.pk3',{'maps/obj_test_gt7.script':'installed other map GT7'})
        old_raw=old.read_bytes();value=self.call([self.row(selected)])[0]
        self.assertEqual(value['gametype_availability']['available'],[5,7])
        self.assertEqual(self.picked(value,5)['sources'][0]['path'],str(selected))
        self.assertEqual(self.picked(value,7)['sources'][0]['path'],str(earlier))
        self.assertEqual(old.read_bytes(),old_raw)

    def test_installed_archives_are_alphabetical_and_later_mod_root_remains_highest(self):
        selected=self.archive(self.library,'m_map.pk3',{'maps/obj_test_gt2.script':'selected'})
        self.archive(self.base,'a_base.pk3',{'maps/obj_test_gt2.script':'earlier'})
        later=self.archive(self.base,'z_base.pk3',{'maps/obj_test_gt2.script':'later base'})
        value=self.call([self.row(selected)])[0]
        self.assertEqual(self.picked(value,2)['sources'][0]['path'],str(later))
        (self.base/'Maps').mkdir();loose=self.base/'Maps/Obj_Test_GT2.script';loose.write_text('loose base')
        value=self.call([self.row(selected)])[0]
        self.assertEqual(self.picked(value,2)['sources'][0]['path'],str(loose));self.assertIsNone(self.picked(value,2)['sources'][0]['member'])
        mod=self.archive(self.mod,'a_mod.pk3',{'MAPS/OBJ_TEST_GT2.SCRIPT':'mod beats all base'})
        value=self.call([self.row(selected)])[0]
        self.assertEqual(self.picked(value,2)['sources'][0]['path'],str(mod))
        self.assertEqual(self.picked(value,2)['sources'][0]['member'],'MAPS/OBJ_TEST_GT2.SCRIPT')
        self.assertEqual(self.picked(value,2)['sha256'],hashlib.sha256(b'mod beats all base').hexdigest())

    def test_custom_generic_override_does_not_manufacture_gts_and_empty_custom_falls_through(self):
        selected=self.archive(self.library,'map.pk3',{'maps/obj_test_gt2.script':'two','maps/obj_test_gt5.script':'five'})
        (self.mod/'Custom').mkdir();generic=self.mod/'Custom/Obj_Test.script';generic.write_text('generic custom')
        seven=self.mod/'Custom/Obj_Test_GT7.script';seven.write_text('exact seven')
        value=self.call([self.row(selected)],'CUSTOM/')[0]
        self.assertEqual(value['gametype_availability']['available'],[7])
        self.assertEqual(self.picked(value,2)['relative'],'custom/obj_test.script')
        self.assertIn('Aufgelöst wird custom/obj_test.script',value['gametype_availability']['gametypes']['2']['reasons'][0])
        generic.write_bytes(b'');value=self.call([self.row(selected)],'Custom')[0]
        self.assertEqual(value['gametype_availability']['available'],[2,5,7])

    def test_missing_or_unknown_source_and_vfs_binding_never_defaults_to_all_gametypes(self):
        selected=self.archive(self.library,'map.pk3',{'maps/obj_test_gt2.script':'two'});valid=self.row(selected)
        for row in (dict(valid,source_path=str(self.library/'missing.pk3')),dict(valid,filename='other.pk3'),
                    {k:v for k,v in valid.items()if k!='source_path'}):
            with self.subTest(row=row):
                value=self.call([row])[0]['gametype_availability'];self.assertEqual(value['available'],[])
                self.assertTrue(all(item['reasons']for item in value['gametypes'].values()))
        for assets in(object(),SimpleNamespace(roots=[self.base,self.mod]),SimpleNamespace(roots=[self.mod],catalog_base_root=self.base)):
            with self.subTest(assets=assets):
                value=mapscript.catalog_availability(assets,[valid],'mapscripts')[0]
                self.assertEqual(value['gametype_availability']['available'],[])

    def test_unreadable_or_oversized_selected_source_does_not_break_other_rows(self):
        good=self.archive(self.library,'good.pk3',{'maps/obj_good_gt5.script':'five'})
        broken=self.library/'broken.pk3';broken.write_bytes(b'not a zip')
        huge=self.archive(self.library,'huge.pk3',{'maps/obj_huge_gt2.script':b'x'*(1024*1024+1)})
        values=self.call([self.row(broken,'obj_bad'),self.row(huge,'obj_huge'),self.row(good,'obj_good')])
        self.assertEqual([r['gametype_availability']['available']for r in values],[[],[],[5]])
        self.assertIn('1 MiB',values[1]['gametype_availability']['gametypes']['2']['reasons'][0])

    def test_existing_installed_source_is_indexed_once_and_uses_no_shadow_library(self):
        installed=self.archive(self.base,'map.pk3',{'maps/obj_test_gt2.script':'installed'})
        self.archive(self.library,'z_other.pk3',{'maps/obj_test_gt5.script':'unselected library'})
        with patch('mapscript._catalog_archive',wraps=mapscript._catalog_archive)as index:
            value=self.call([self.row(installed),self.row(installed)])[0]
        self.assertEqual(value['gametype_availability']['available'],[2]);self.assertEqual(index.call_count,1)

    def test_three_hundred_sources_share_one_base_mod_scan_and_one_read_per_unique_archive(self):
        self.archive(self.base,'base.pk3',{'maps/unrelated.script':'base'})
        self.archive(self.mod,'mod.pk3',{'maps/unrelated.script':'mod'})
        rows=[]
        for i in range(300):
            name='obj_map_'+str(i);gt=(2,5,7)[i%3]
            archive=self.archive(self.library,'map_'+str(i)+'.pk3',{'maps/'+name+'_gt'+str(gt)+'.script':str(i)})
            rows.append(self.row(archive,name))
        with patch('mapscript._catalog_archive',wraps=mapscript._catalog_archive)as index,\
             patch('mapscript._catalog_loose',wraps=mapscript._catalog_loose)as loose,\
             patch('mapscript._raw',side_effect=AssertionError('Do not reopen indexed script archives')):
            values=self.call(rows)
        self.assertEqual(index.call_count,302);self.assertEqual(loose.call_count,2)
        self.assertEqual(len({call.args[0]for call in index.call_args_list}),302)
        self.assertEqual([r['gametype_availability']['available']for r in values],[[gt]for gt in(2,5,7)*100])

    def test_new_call_rechecks_source_bytes_without_a_stale_stamp_cache(self):
        selected=self.archive(self.library,'map.pk3',{'maps/obj_test_gt2.script':'first'})
        row=self.row(selected);first=self.call([row])[0]
        self.archive(self.library,'map.pk3',{'maps/obj_test_gt2.script':'other'})
        second=self.call([row])[0]
        self.assertNotEqual(self.picked(first,2)['sha256'],self.picked(second,2)['sha256'])


if __name__=='__main__':main()
