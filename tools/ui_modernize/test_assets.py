"""Finite native UI art, alpha, shader, font coverage and provenance gates."""
import hashlib
import json
from pathlib import Path
import re
import struct
import unittest
import generate_assets as g
import theme_assets


def u16(data,at):return struct.unpack_from(">H",data,at)[0]
def u32(data,at):return struct.unpack_from(">I",data,at)[0]


def has_glyph(data,code):
    tables={data[12+i*16:16+i*16]:u32(data,12+i*16+8) for i in range(u16(data,4))}
    base=tables[b"cmap"]
    for index in range(u16(data,base+2)):
        record=base+4+index*8;platform,encoding=struct.unpack_from(">HH",data,record)
        if platform not in (0,3):continue
        table=base+u32(data,record+4)
        if u16(data,table)!=4:continue
        count=u16(data,table+6)//2;ends=table+14;starts=ends+count*2+2;deltas=starts+count*2;ranges=deltas+count*2
        for segment in range(count):
            end,start=u16(data,ends+segment*2),u16(data,starts+segment*2)
            if start<=code<=end:
                delta=u16(data,deltas+segment*2);offset=u16(data,ranges+segment*2)
                if not offset:return ((code+delta)&65535)!=0
                value=u16(data,ranges+segment*2+offset+2*(code-start))
                return value!=0 and ((value+delta)&65535)!=0
    return False


class NativeUIAssetTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.meta=json.loads((g.DEST/"tce2_ui/SOURCE.json").read_text())

    def test_frozen_art_and_native_tga(self):
        self.assertEqual(self.meta["generator_sha256"],hashlib.sha256(Path(g.__file__).read_bytes()).hexdigest())
        self.assertEqual(self.meta["background_sha256"],hashlib.sha256(g.SOURCE.read_bytes()).hexdigest())
        self.assertEqual(len(self.meta["files"]),155)
        original=[row for row in self.meta['files'] if not row['path'].startswith('tce2_ui/theme/')]
        self.assertEqual(len(original),53)
        self.assertEqual(self.meta['theme_layers']['generator_sha256'],hashlib.sha256(Path(theme_assets.__file__).read_bytes()).hexdigest())
        for row in self.meta["files"]:
            with self.subTest(asset=row["path"]):
                path=g.DEST/row["path"];data=path.read_bytes()
                self.assertEqual(hashlib.sha256(data).hexdigest(),row["sha256"])
                self.assertEqual(len(data),row["bytes"])
                self.assertEqual(data[2],2)
                self.assertEqual(struct.unpack_from("<HHB",data,12),(row["width"],row["height"],row["channels"]))
                self.assertEqual(data[17],0x20 if row["channels"]==24 else 0x28)
                self.assertTrue(row["path"].startswith("tce2_ui/"))

    def test_unique_native_vertex_alpha_shaders(self):
        shader=(g.DEST/"scripts/tce2_ui.shader").read_text()
        names=re.findall(r"(?m)^tce2_ui/[^\s{}]+$",shader)
        self.assertEqual(len(names),155);self.assertEqual(len(set(names)),155)
        self.assertEqual(shader.count("blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA"),155)
        self.assertEqual(shader.count("rgbGen vertex"),155)
        self.assertEqual(shader.count("alphaGen vertex"),155)
        self.assertNotRegex(shader,r"(?im)^\s*(normalMap|specularMap|bumpMap|reflectionMap)\b")
        self.assertEqual(shader.count("nomipmaps"),155)

    def test_palette_layers_are_neutral_and_preserve_every_original_alpha_pixel(self):
        rows={row['path']:row for row in self.meta['files']}
        sources=[path for path in rows if not path.startswith('tce2_ui/theme/') and path not in ('tce2_ui/background.tga','tce2_ui/cursor.tga')]
        self.assertEqual(len(sources),51)
        for path in sources:
            with self.subTest(path=path):
                original=(g.DEST/path).read_bytes();key=path[len('tce2_ui/'):-4]
                width,height=struct.unpack_from('<HH',original,12);end=18+width*height*4
                layers=[(g.DEST/('tce2_ui/theme/'+key+'_'+kind+'.tga')).read_bytes() for kind in ('body','accent')]
                for layer in layers:
                    self.assertEqual(layer[:18],original[:18])
                    self.assertEqual(layer[18:end:4],layer[19:end:4])
                    self.assertEqual(layer[19:end:4],layer[20:end:4])
                body,accent=layers
                for before,a,b in zip(original[21:end:4],body[21:end:4],accent[21:end:4]):
                    self.assertFalse(a and b,'Masks must not double-blend one source pixel')
                    self.assertEqual(a+b,before,'Opacity mask must preserve source coverage')
                if key.startswith('icons/'):
                    self.assertEqual(set(body[21:end:4]),{0})
                    self.assertEqual(accent[21:end:4],original[21:end:4])
        for forbidden in ('background','cursor'):
            self.assertNotIn('tce2_ui/theme/'+forbidden+'_body.tga',rows)

    def test_transparent_controls_and_distinct_states(self):
        for key in ("check_off","check_on","radio_off","radio_on","focus"):
            data=(g.DEST/("tce2_ui/"+key+".tga")).read_bytes();alpha=data[18:18+64*64*4][3::4] if key!="focus" else data[18:18+256*64*4][3::4]
            self.assertIn(0,alpha);self.assertIn(255,alpha)
        button=[(g.DEST/("tce2_ui/"+key+".tga")).read_bytes() for key in ("button","button_hover","button_pressed","button_disabled")]
        self.assertEqual(len(set(button)),4)
        self.assertEqual(len(self.meta["native_aliases"]),11)
        for old,new in self.meta["native_aliases"].items():
            self.assertTrue(old.startswith("ui/assets/"));self.assertTrue(new.startswith("tce2_ui/"));self.assertTrue((g.DEST/new).is_file())

    def test_cursor_native_hotspot(self):
        info=self.meta["cursor_hotspot"]
        self.assertEqual((info["x"],info["y"]),(0,0))
        self.assertEqual(info["shader"],"tce2_ui/cursor")
        data=(g.DEST/info["asset"]).read_bytes();rgba=data[18:18+64*64*4]
        self.assertGreater(rgba[3],0)  # upper-left visible outline owns native hotspot
        self.assertEqual(rgba[(63*64+63)*4+3],0)
        self.assertIn("clampmap "+info["asset"],(g.DEST/"scripts/tce2_ui.shader").read_text())

    def test_official_licensed_font_payload_and_latin_coverage(self):
        source=json.loads((g.DEST/"fonts/tce2_ui/SOURCE.json").read_text())
        for row in source["files"]:
            with self.subTest(font=row["path"]):
                data=(g.DEST/row["path"]).read_bytes()
                self.assertEqual(hashlib.sha256(data).hexdigest(),row["sha256"])
                self.assertIn(source["revision"],row["source"])
                self.assertTrue(row["source"].startswith("https://raw.githubusercontent.com/google/fonts/"))
                if row["path"].endswith('.ttf'):
                    self.assertEqual(data[:4],b"\0\1\0\0")
                    for letter in "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789Ã„Ã–ÃœÃ¤Ã¶Ã¼ÃŸÃ©":self.assertTrue(has_glyph(data,ord(letter)),letter)
                else:self.assertIn(b"SIL OPEN FONT LICENSE",data)


if __name__=="__main__":unittest.main()
