import io
from pathlib import Path
import tempfile
import unittest
import zipfile
from types import SimpleNamespace
from navbundle import NavBundles
import mapscript

class GametypeTest(unittest.TestCase):
    def test_shared_way_separate_goals(self):
        with tempfile.TemporaryDirectory() as d:
            a,b=NavBundles(d,2),NavBundles(d,5)
            self.assertEqual(a.path('obj_test','way'),b.path('obj_test','way'))
            self.assertNotEqual(a.path('obj_test','goals'),b.path('obj_test','goals'))
            self.assertEqual(a.path('obj_test','script').name,'obj_test_gt2.gm')
    def test_copy_pair_keeps_legacy_and_other_modes(self):
        with tempfile.TemporaryDirectory() as d:
            legacy=NavBundles(d)
            for k in ('script','goals'):legacy.path('obj_test',k).write_bytes(k.encode())
            a=NavBundles(d,5);r=a.create_pair('obj_test')
            self.assertEqual(r['bundle']['pair_status'],'complete')
            self.assertEqual(a.path('obj_test','goals').read_bytes(),b'goals')
            self.assertEqual(legacy.path('obj_test','script').read_bytes(),b'script')
            self.assertFalse(NavBundles(d,2).path('obj_test','script').exists())
            self.assertEqual(a.names(),['obj_test'])
            with self.assertRaises(ValueError):a.create_pair('obj_test')
    def test_incomplete_pair_not_overwritten(self):
        with tempfile.TemporaryDirectory() as d:
            a=NavBundles(d,7);a.path('obj_test','goals').write_bytes(b'keep')
            self.assertEqual(a.read('obj_test')['pair_status'],'incomplete')
            a.create_pair('obj_test')
            self.assertEqual(a.path('obj_test','goals').read_bytes(),b'keep')
    def test_archive_has_shared_way_and_selected_pair(self):
        with tempfile.TemporaryDirectory() as d:
            a=NavBundles(d,5);a.create_pair('obj_test');a.path('obj_test','way').write_bytes(b'way')
            with zipfile.ZipFile(io.BytesIO(a.archive('obj_test'))) as z:
                self.assertEqual(set(z.namelist()),{'obj_test.way','obj_test_gt5.gm','obj_test_gt5_goals.gm'})
    def test_pk3_case_preserved_and_trigger_crossreference(self):
        with tempfile.TemporaryDirectory() as d:
            with zipfile.ZipFile(Path(d)/'map.pk3','w') as z:z.writestr('maps/Goldrush-GA.script','wm_announce "Tank ready!"')
            r=mapscript.read(SimpleNamespace(roots=[Path(d)]),'goldrush-ga','OnTrigger("Tank ready!", Map.Ready); OnTrigger("native_event", Map.Event);')
            self.assertEqual(r['name'],'maps/Goldrush-GA.script')
            self.assertEqual(r['matches'],[{'message':'Tank ready!','callback':'Map.Ready'}])
            self.assertEqual(r['unmatched'][0]['message'],'native_event')

    def test_pk3_gametype_precedes_generic(self):
        with tempfile.TemporaryDirectory() as d:
            with zipfile.ZipFile(Path(d)/'map.pk3','w') as z:
                z.writestr('maps/obj_delta.script','generic')
                z.writestr('maps/obj_delta_gt7.script','seven')
            assets=SimpleNamespace(roots=[Path(d)])
            self.assertEqual(mapscript.read(assets,'obj_delta',gametype=7)['text'],'seven')
            self.assertEqual(mapscript.read(assets,'obj_delta',gametype=5)['text'],'generic')

    def test_spawns_preserve_script_and_idempotent_block(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d)
            with zipfile.ZipFile(root/'map.pk3','w') as z:z.writestr('maps/obj_test_gt7.script','game_manager { spawn { wm_camo 3 } }')
            nodes=[dict(id=i,position=[i*512,0,24],accepted=True,spawn_reachable=True,crouch=False,nav_flags=0) for i in range(6)]
            edges=[dict(**{'from':i,'to':(i+1)%6},accepted=True) for i in range(6)]
            studio=SimpleNamespace(game_mode='tce2',mod=root,assets=SimpleNamespace(roots=[root]),require_map=lambda n:n,proof_status=lambda n:{'valid':True},native=lambda n:dict(nodes=nodes,edges=edges))
            a=mapscript.generate_spawns(studio,'obj_test');path=Path(a['path']);text=path.read_text();self.assertIn('wm_camo 3',text);self.assertEqual(a['count'],6)
            b=mapscript.generate_spawns(studio,'obj_test');self.assertEqual(path.read_text().count('tce_dmspawns 1'),1)
            self.assertTrue(list((path.parent/'.navstudio-backups').glob('*')))
            studio.proof_status=lambda n:{'valid':False}
            with self.assertRaises(ValueError):mapscript.generate_spawns(studio,'obj_test')

    def test_custom_editor_conflicts_and_backups(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);(root/'mapscripts').mkdir();p=root/'mapscripts/obj_test_gt7.script';p.write_text('tce_dmspawns 1')
            studio=SimpleNamespace(mod=root,game_mode='tce2',require_map=lambda n:n)
            loaded=mapscript.custom(studio,'obj_test',7)
            with self.assertRaises(RuntimeError):mapscript.custom(studio,'obj_test',7,'tce_dmspawns 0','stale')
            mapscript.custom(studio,'obj_test',7,'tce_dmspawns 0',loaded['sha256'])
            self.assertEqual(p.read_text(),'tce_dmspawns 0')
            self.assertEqual(len(list((root/'mapscripts/.navstudio-backups').glob('*'))),1)

if __name__=='__main__':unittest.main()
