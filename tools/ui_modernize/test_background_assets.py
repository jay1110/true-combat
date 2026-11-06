"""Actual background generator/inventory and native orientation checks."""
import json
from pathlib import Path
import tempfile
import unittest
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

    def test_production_catalog_matches_sources_when_generated(self):
        p=b.ROOT/'ui-modern/assets/tce2_ui/backgrounds/catalog.json'
        if not p.exists():self.skipTest('Production catalog not generated yet')
        expected=json.loads(p.read_text());self.assertEqual(b.build(b.ROOT/'map_pics',b.ROOT/'ui-modern/assets',True),expected)


if __name__=='__main__':unittest.main()
