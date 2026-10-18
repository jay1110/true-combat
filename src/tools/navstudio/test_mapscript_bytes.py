"""Raw map-script export/render is lossless; no API, Engine or real files."""
import copy
from pathlib import Path
import tempfile
from types import SimpleNamespace
from unittest import TestCase,main
from unittest.mock import patch
import zipfile

import mapscript
import spawns


class MapScriptByteTests(TestCase):
    def fixture(self,raw,member='Maps/Obj_Test_GT5.script'):
        temp=tempfile.TemporaryDirectory();self.addCleanup(temp.cleanup)
        root=Path(temp.name);path=root/'map.pk3'
        with zipfile.ZipFile(path,'w')as z:z.writestr(member,raw)
        return root,SimpleNamespace(roots=[root])

    def document(self):
        return dict(enabled=True,nodes=[dict(position=[256.,64.,30.],angle=90.),dict(position=[512.,-64.,30.],angle=270.)])

    def test_pk3_raw_utf8_bom_and_crlf_preserved_with_exact_binding(self):
        raw=b'\xef\xbb\xbf'+('// Tür 日本語\r\ngame_manager { spawn { wm_announce "Grüße" } }\r\n').encode('utf8')
        root,assets=self.fixture(raw);before=(root/'map.pk3').read_bytes()
        resolution=mapscript.binding(assets,'obj_test',5)
        self.assertEqual(mapscript.read_raw(assets,'obj_test',5,expected=resolution),raw)
        self.assertEqual((root/'map.pk3').read_bytes(),before)
        viewed=mapscript.read(assets,'obj_test',gametype=5)
        self.assertEqual(viewed['codec'],'utf-8-sig');self.assertEqual(viewed['newline'],'\r\n')

    def test_custom_loose_override_returns_its_literal_bytes_not_pk3_fallback(self):
        _,assets=self.fixture(b'packed')
        root=assets.roots[0];(root/'Mapscripts').mkdir();path=root/'Mapscripts/Obj_Test_GT5.Script'
        raw=b'// caf\xe9\r\ngame_manager { spawn { } }\r\n';path.write_bytes(raw)
        bound=mapscript.binding(assets,'obj_test',5,'mapscripts')
        self.assertEqual(mapscript.read_raw(assets,'obj_test',5,'mapscripts',bound),raw)
        self.assertIsNone(bound['selected']['sources'][0]['member'])

    def test_changed_expected_selection_hash_or_size_cannot_return_bytes(self):
        _,assets=self.fixture(b'original')
        expected=mapscript.binding(assets,'obj_test',5)
        for key,value in(('sha256','a'*64),('bytes',1),('relative','maps/other_gt5.script')):
            changed=copy.deepcopy(expected);changed['selected'][key]=value
            with self.subTest(field=key),self.assertRaisesRegex(RuntimeError,'Quellbindung geändert'):
                mapscript.read_raw(assets,'obj_test',5,expected=changed)
        root=assets.roots[0];(root/'maps').mkdir();(root/'maps/obj_test_gt5.script').write_bytes(b'new loose override')
        with self.assertRaisesRegex(RuntimeError,'Quellbindung geändert'):mapscript.read_raw(assets,'obj_test',5,expected=expected)

    def test_source_changed_between_binding_and_byte_read_fails_closed(self):
        _,assets=self.fixture(b'packed')
        root=assets.roots[0];(root/'maps').mkdir();path=root/'maps/obj_test_gt5.script';path.write_bytes(b'first')
        original=mapscript._raw;calls=[]
        def changed(source):
            value=original(source);calls.append(source)
            if len(calls)==1:path.write_bytes(b'second')
            return value
        with patch('mapscript._raw',side_effect=changed),self.assertRaisesRegex(RuntimeError,'SHA-/Größenbindung'):
            mapscript.read_raw(assets,'obj_test',5)

    def test_missing_raw_source_raises_instead_of_exporting_an_empty_script(self):
        _,assets=self.fixture(b'five')
        with self.assertRaises(FileNotFoundError):mapscript.read_raw(assets,'obj_missing',2)

    def test_detected_codecs_roundtrip_bom_nonascii_and_undefined_legacy_bytes(self):
        for raw in(b'\xef\xbb\xbf// Gr\xc3\xbc\xc3\x9fe\r\n',b'// caf\xe9\r\n\x81', '// 日本語\n'.encode('utf8'),b'// ascii\r\n'):
            source=mapscript.decode_script(raw)
            with self.subTest(raw=raw):self.assertEqual(mapscript.encode_script(source['text'],source['codec']),raw)

    def test_gt7_new_block_preserves_original_codec_bom_and_unmanaged_bytes(self):
        for codec,bom in(('utf-8',b''),('utf-8',b'\xef\xbb\xbf'),('cp1252',b'')):
            prefix='// Grüße\r\ngame_manager\r\n{\r\n spawn\r\n {';suffix='\r\n  wm_announce "Türe"\r\n }\r\n}\r\n'
            raw=bom+(prefix+suffix).encode(codec);rendered=mapscript.render_gt7(raw,self.document())
            with self.subTest(codec=codec,bom=bom):
                self.assertTrue(rendered.startswith(bom+prefix.encode(codec)))
                self.assertTrue(rendered.endswith(suffix.encode(codec)))
                decoded=mapscript.decode_script(rendered);text=decoded['text'];start=text.index(spawns.START);end=text.index(spawns.END)+len(spawns.END)
                self.assertNotIn('\n',text[start:end].replace('\r\n',''))
                self.assertEqual(rendered.count(b'tce_dmspawn '),2)
                self.assertEqual(mapscript.render_gt7(rendered,self.document()),rendered)

    def test_gt7_replaces_existing_block_and_preserves_mixed_original_newlines_elsewhere(self):
        prefix='// Ä\r\ngame_manager\n{\r\n spawn {\r\n\t\t'
        suffix='\n\t\twm_announce "Keep ü"\r\n }\n}\r\n'
        raw=(prefix+spawns.START+'\r\nold generated block\r\n'+spawns.END+suffix).encode('cp1252')
        result=mapscript.render_gt7(raw,self.document())
        self.assertTrue(result.startswith(prefix.encode('cp1252')));self.assertTrue(result.endswith(suffix.encode('cp1252')))
        self.assertNotIn(b'old generated block',result);self.assertEqual(result.count(spawns.START.encode()),1)

    def test_invalid_text_or_oversize_encoding_never_replaces_source_codec(self):
        with self.assertRaises(ValueError):mapscript.encode_script('text','latin1')
        with self.assertRaises(ValueError):mapscript.encode_script('x'*(1024*1024+1),'utf-8')
        with self.assertRaises(TypeError):mapscript.decode_script('already converted text')

    def test_legacy_generate_spawns_preserves_source_and_existing_override_codec_and_crlf(self):
        for codec,bom,label in(('utf-8',b'\xef\xbb\xbf','日本語'),('utf-8',b'','Grüße'),('cp1252',b'','Grüße')):
            prefix=('// '+label+'\r\ngame_manager\r\n{\r\n spawn {').encode(codec)
            suffix=('\r\n wm_announce "Keep '+label+'"\r\n }\r\n}\r\n').encode(codec)
            raw=bom+prefix+suffix;root,assets=self.fixture(raw,member='Maps/Obj_Test_GT7.script')
            nodes=[dict(id=i,position=[i*512.,0.,30.],accepted=True,spawn_reachable=True,crouch=False,nav_flags=0)for i in range(6)]
            report=dict(nodes=nodes,edges=[dict(**{'from':i,'to':(i+1)%6},accepted=True)for i in range(6)])
            studio=SimpleNamespace(require_map=lambda n:n,game_mode='tce2',assets=assets,mod=root,
                proof_status=lambda n:dict(valid=True),native=lambda n:report)
            first=mapscript.generate_spawns(studio,'obj_test');path=Path(first['path']);content=path.read_bytes()
            with self.subTest(codec=codec,bom=bom):
                self.assertTrue(content.startswith(bom+prefix));self.assertTrue(content.endswith(suffix))
                mapscript.generate_spawns(studio,'obj_test');self.assertEqual(path.read_bytes(),content)
                self.assertEqual(mapscript.decode_script(content)['codec'],'utf-8-sig'if bom else codec)
                self.assertEqual(content.count(spawns.START.encode()),1)

    def test_custom_edit_keeps_utf8_bom_or_legacy_codec_and_returns_bom_free_text(self):
        for codec,bom,label in(('utf-8',b'\xef\xbb\xbf','日本語'),('utf-8',b'','日本語'),('cp1252',b'','Grüße')):
            root,_=self.fixture(b'packed');(root/'mapscripts').mkdir();path=root/'mapscripts/obj_test_gt7.script'
            raw=bom+('// '+label+'\r\ngame_manager { spawn { } }\r\n').encode(codec);path.write_bytes(raw)
            studio=SimpleNamespace(require_map=lambda n:n,game_mode='tce2',mod=root)
            loaded=mapscript.custom(studio,'obj_test',7)
            with self.subTest(codec=codec,bom=bom):
                self.assertFalse(loaded['text'].startswith('\ufeff'));self.assertIn(label,loaded['text'])
                text=loaded['text']+'// edited '+label+'\r\n'
                saved=mapscript.custom(studio,'obj_test',7,text,loaded['sha256'])
                self.assertEqual(path.read_bytes(),bom+text.encode(codec));self.assertEqual(saved['text'],text)
                self.assertEqual(saved['encoding'],'utf-8-sig'if bom else codec)
                backups=list((root/'mapscripts/.navstudio-backups').iterdir())
                self.assertEqual(len(backups),1);self.assertEqual(backups[0].read_bytes(),raw)


if __name__=='__main__':main()
