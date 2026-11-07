"""Finite ET 2.60b font ABI, atlas closure and real classic TGA regressions."""
import copy
import hashlib
import json
import math
from pathlib import Path
import re
import struct
import unittest

import classic_assets as c
import generate_assets as g
from verify_package import canonical_ui


class ClassicAssets(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.assets = canonical_ui.collect('game')
        cls.meta = json.loads(cls.assets['fonts/tce2_ui/PORTABLE.json'])

    def test_native_bgra_bottom_rows_and_exact_rgba_roundtrip(self):
        # Asymmetric corner colours detect the classic ET loader's fixed row
        # direction, including the cursor hotspot and letters' up/down direction.
        pixels = bytes((255,0,0,255, 0,255,0,80, 0,0,255,0, 19,21,23,255))
        data = g.tga(2,2,pixels)
        self.assertEqual(data[17],8)
        self.assertEqual(data[18:26],bytes((255,0,0,0,23,21,19,255)))
        self.assertEqual(c.tga_pixels(data),(2,2,pixels))
        opaque = bytes((255,0,0,255, 0,255,0,255, 0,0,255,255, 19,21,23,255))
        self.assertEqual(c.tga_pixels(g.tga(2,2,opaque,True)),(2,2,opaque))

    def test_npot_resize_keeps_uv_corners_and_premultiplied_alpha(self):
        original = bytes((255,255,255,255, 0,0,0,0, 255,0,0,255))
        w,h,pixels = g.power_of_two_image(3,1,original)
        self.assertEqual((w,h),(4,1))
        self.assertEqual(pixels[:4],original[:4])
        self.assertEqual(pixels[-4:],original[-4:])
        self.assertEqual(pixels[4:7],b'\xff\xff\xff')
        self.assertTrue(0 < pixels[7] < 255)
        self.assertEqual(g.power_of_two_image(4,1,pixels),(4,1,pixels))

    def test_real_six_font_tables_all_atlases_and_latin_pixels(self):
        report = c.verify(self.assets)
        self.assertEqual(report['font_sets'],6)
        self.assertEqual(report['glyphs'],1536)
        self.assertEqual(report['atlas_images'],6)
        self.assertEqual(report['format_bytes'],20548)
        for variant in self.meta['variants']:
            rows,scale,name = c.font_table(self.assets[variant['dat']])
            self.assertEqual(name,variant['dat'])
            self.assertAlmostEqual(scale*variant['point_size'],48,places=5)
            self.assertLessEqual(abs(rows[ord('I')][3]),4)
            self.assertGreater(rows[ord('W')][4],rows[ord('I')][4])

    def test_generation_provenance_bound_to_current_source_and_ttf(self):
        root = Path(__file__).resolve().parents[2]
        for source,digest in ((self.meta['generator'],self.meta['generator_sha256']),
                              (self.meta['raster_writer'],self.meta['raster_writer_sha256'])):
            self.assertEqual(hashlib.sha256((root/source).read_bytes()).hexdigest(),digest)
        self.assertEqual(self.meta['freetype'],'2.13.2')
        self.assertEqual(self.meta['freetype_py'],'2.5.1')
        self.assertEqual(self.meta['dpi'],72)
        self.assertEqual(self.meta['raster_mode'],'FT_LOAD_FORCE_AUTOHINT | FT_LOAD_TARGET_NORMAL')
        self.assertEqual(self.meta['outline_embolden_26_6'],24)
        # Check literal registration sites in both global assets and native UI/CG.
        requested = set()
        for path in (root/'pak').rglob('*.menu'):
            requested.update((name,int(size)) for name,size in re.findall(
                r'font\s+\w+\s+"tce2_ui/([^"\n]+)"\s+(\d+)',path.read_text(encoding='latin1')))
        for path in (root/'src/ui/tce_modern_ui.c',root/'src/cgame/cg_modern_limbopanel.inc'):
            requested.update((name,int(size)) for name,size in re.findall(
                r'trap_R_RegisterFont\("tce2_ui/([^"\n]+)",\s*(\d+)',path.read_text()))
        self.assertEqual(requested,c.FONT_SETS)

    def test_reject_wrong_size_endian_scale_missing_atlas_and_empty_pixels(self):
        path = self.meta['variants'][0]['dat']
        atlas = self.meta['variants'][0]['atlases'][0]
        for change in ('truncated','endian','scale','atlas','empty','unterminated','uv','bearing'):
            with self.subTest(change=change):
                files = dict(self.assets)
                data = bytearray(files[path])
                if change == 'truncated': data = data[:-1]
                elif change == 'endian': data[:4] = data[:4][::-1]
                elif change == 'scale': struct.pack_into('<f',data,20480,float('nan'))
                elif change == 'atlas': del files[atlas]
                elif change == 'empty':
                    old = files[atlas];w,h = struct.unpack_from('<HH',old,12)
                    files[atlas] = old[:18]+bytes(w*h*4)+old[18+w*h*4:]
                elif change == 'unterminated': data[48:80] = b'x'*32
                elif change == 'uv': struct.pack_into('<f',data,28,-.1)
                elif change == 'bearing': struct.pack_into('<i',data,12,100000)
                files[path] = bytes(data)
                # Keep honest hashes so the failure exercises ABI/raster checks.
                meta = copy.deepcopy(self.meta)
                for row in meta['files']:
                    if row['path'] in files:
                        row.update(bytes=len(files[row['path']]),sha256=c.sha(files[row['path']]))
                files['fonts/tce2_ui/PORTABLE.json'] = json.dumps(meta).encode()
                with self.assertRaises(ValueError): c.verify(files)

    def test_reject_top_origin_npot_and_missing_font_variant(self):
        for change in ('top_origin','npot','variant','old_density','orphan'):
            with self.subTest(change=change):
                files = dict(self.assets)
                if change in ('variant','old_density'):
                    meta = copy.deepcopy(self.meta)
                    if change == 'variant':meta['variants'].pop()
                    else:meta['dpi']=144
                    files['fonts/tce2_ui/PORTABLE.json'] = json.dumps(meta).encode()
                elif change == 'orphan':
                    files['fonts/tce2_ui/orphan.tga']=files[self.meta['variants'][0]['atlases'][0]]
                else:
                    data = bytearray(files['tce2_ui/slider_thumb.tga'])
                    if change == 'top_origin':data[17] |= 32
                    else:struct.pack_into('<H',data,14,48)
                    files['tce2_ui/slider_thumb.tga'] = bytes(data)
                with self.assertRaises(ValueError): c.verify(files)

    def test_native_widget_aliases_are_identical_portable_images(self):
        art = json.loads(self.assets['tce2_ui/SOURCE.json'])
        for target,source in art['native_aliases'].items():
            self.assertTrue(target.startswith('ui/assets/'))
            c.tga_pixels(self.assets[source])
        self.assertEqual(art['native_aliases']['ui/assets/sliderbutt_1.tga'],
                         'tce2_ui/slider_thumb.tga')

    def test_small_bilinear_stems_do_not_vanish_at_quarter_pixel_phases(self):
        # Actual fallback DAT/TGA, independent GL_LINEAR-style sampling. The
        # former144DPI/LCD bake lost8/32I/l samples at640/.20 completely.
        glyphs,scale,_=c.font_table(self.assets['fonts/tce2_ui/Barlow-Regular_21.dat'])
        for screen_width in (640,800):
            for textscale in (.20,.24):
                use=scale*textscale*screen_width/852
                for letter in 'Il':
                    glyph=glyphs[ord(letter)]
                    atlas=glyph[12].split(b'\0',1)[0].decode()+'.tga'
                    w,h,pixels=c.tga_pixels(self.assets[atlas])
                    for dx in (0,.25,.5,.75):
                        for dy in (0,.25,.5,.75):
                            left=10+dx+glyph[3]*use;top=22+dy-glyph[1]*use;peak=0
                            for py in range(math.ceil(top-.5),math.ceil(top+glyph[6]*use-.5)):
                                for px in range(math.ceil(left-.5),math.ceil(left+glyph[5]*use-.5)):
                                    x=glyph[7]*w+(px+.5-left)/use-.5
                                    y=glyph[8]*h+(py+.5-top)/use-.5
                                    a,b=math.floor(x),math.floor(y);fx,fy=x-a,y-b
                                    value=sum(pixels[(yy*w+xx)*4+3]*weight for xx,yy,weight in
                                              ((a,b,(1-fx)*(1-fy)),(a+1,b,fx*(1-fy)),
                                               (a,b+1,(1-fx)*fy),(a+1,b+1,fx*fy)))
                                    peak=max(peak,value/255)
                            self.assertGreater(peak,.25,(screen_width,textscale,letter,dx,dy))


if __name__ == '__main__':
    unittest.main()
