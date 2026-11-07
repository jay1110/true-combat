"""Actual background generator/inventory and native orientation checks."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import background_assets as b
import generate_assets as art
import classic_assets


class MapBackgrounds(unittest.TestCase):
    def source(self,root):
        root.mkdir()
        for index,phase in b.PHASES.items():
            path=root/('%02d_obj_map_%s.png'%(index,b.TOKENS[phase]))
            art.png_write(path,3,2,bytes([255,0,0,255]*3+[0,0,255,255]*3))
        return root

    def test_actual_portable_render_is_upright_and_keeps_both_edges(self):
        with tempfile.TemporaryDirectory() as temp:
            source=self.source(Path(temp)/'pictures');out=Path(temp)/'assets'
            manifest=b.build(source,out)
            self.assertEqual(len(manifest['files']),16)
            for row in manifest['files']:
                data=(out/row['path']).read_bytes();w,h,pixels=classic_assets.tga_pixels(data)
                self.assertEqual((w,h),(4,2));self.assertEqual(data[17],0)
                self.assertEqual(pixels[:4],bytes([255,0,0,255]))
                self.assertEqual(pixels[-4:],bytes([0,0,255,255]))
                self.assertLess(len(row['shader']),64)
                self.assertEqual(row['phase'],b.PHASES[row['id']])
            self.assertEqual(b.build(source,out,True),manifest)
            self.assertEqual(b.validate(out),manifest)
            self.assertFalse((out/'tce2_ui/background.tga').exists())

    def test_missing_duplicate_wrong_phase_and_output_source_overlap(self):
        with tempfile.TemporaryDirectory() as temp:
            source=self.source(Path(temp)/'pictures');first=next(source.glob('01_*.png'))
            duplicate=source/first.name.replace('obj_map','obj_other');duplicate.write_bytes(first.read_bytes())
            with self.assertRaises(ValueError):b.inventory(source)
            duplicate.unlink();first.unlink()
            with self.assertRaises(ValueError):b.inventory(source)
            with self.assertRaises(ValueError):b.build(source,source/'out')

    def test_frozen_manifest_detects_changed_source_and_payload(self):
        with tempfile.TemporaryDirectory() as temp:
            source=self.source(Path(temp)/'pictures');out=Path(temp)/'assets';m=b.build(source,out)
            p=source/m['files'][0]['source'];p.write_bytes(p.read_bytes()+b'changed')
            with self.assertRaises(ValueError):b.build(source,out,True)

    def test_frozen_validation_needs_no_external_pngs(self):
        with tempfile.TemporaryDirectory() as temp:
            source=self.source(Path(temp)/'pictures');out=Path(temp)/'assets';manifest=b.build(source,out)
            for p in source.glob('*.png'):p.unlink()
            with patch.object(b,'inventory',side_effect=AssertionError('PNG inventory was read')), \
                 patch.object(b,'render',side_effect=AssertionError('PNG was resampled')):
                self.assertEqual(b.validate(out),manifest)
            with self.assertRaises(ValueError):b.build(source,out,True)

    def test_frozen_validation_rejects_modified_payload_shader_and_provenance(self):
        with tempfile.TemporaryDirectory() as temp:
            out=Path(temp)/'assets';manifest=b.build(self.source(Path(temp)/'pictures'),out)
            for name in (manifest['files'][0]['path'],b.SHADERS,'tce2_ui/backgrounds/runtime.cfg'):
                p=out/name;original=p.read_bytes();p.write_bytes(original+b'changed')
                with self.subTest(path=name),self.assertRaises(ValueError):b.validate(out)
                p.write_bytes(original)
            catalog=out/b.CATALOG;original=catalog.read_bytes()
            for field,value in [('generator_sha256','0'*64),('resampler_sha256','0'*64)]:
                bad=json.loads(original);bad[field]=value;catalog.write_text(json.dumps(bad))
                with self.subTest(field=field),self.assertRaises(ValueError):b.validate(out)
            for field,value in [('id',2),('path','../../outside.tga'),('source','../image.png')]:
                bad=json.loads(original);bad['files'][0][field]=value;catalog.write_text(json.dumps(bad))
                with self.subTest(field=field),self.assertRaises(ValueError):b.validate(out)
            catalog.write_bytes(original);(out/'tce2_ui/backgrounds/stale.tga').write_bytes(b'stale')
            with self.assertRaises(ValueError):b.validate(out)

    def test_cli_frozen_check_and_explicit_generation(self):
        with tempfile.TemporaryDirectory() as temp:
            source=self.source(Path(temp)/'pictures');out=Path(temp)/'assets';b.build(source,out)
            with patch('sys.argv',['background_assets','--check','--out',str(out)]):b.main()
            with patch('sys.argv',['background_assets','--source',str(source),'--out',str(out)]):b.main()
            with patch.object(b,'ROOT',Path(temp)), \
                 patch('sys.argv',['background_assets','--source',str(source)]):b.main()
            self.assertEqual(b.validate(Path(temp)/'build/ui-backgrounds')['files'],b.validate(out)['files'])
            self.assertFalse((Path(temp)/'pak').exists())
            with patch('sys.argv',['background_assets']),self.assertRaises(SystemExit):b.main()

    def test_production_catalog_matches_frozen_runtime_assets(self):
        p=b.ROOT/'pak'/b.CATALOG
        expected=json.loads(p.read_text());self.assertEqual(b.validate(b.ROOT/'pak'),expected)


if __name__=='__main__':unittest.main()
