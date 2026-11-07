"""Stock ET / ETL TGA compatibility of the actual navigation artwork."""
import hashlib
import json
from pathlib import Path
import struct
import unittest

import generate_fireteam_icons as fireteam
import generate_nav_icons as nav


def decode(data, honors_top_origin):
    """ET LoadTGA type2/32 writes input row0 to last row. ETL then honors bit5.

    Contracts: ET-GPL renderer/tr_image.c LoadTGA and ETL
    renderercommon/tr_image_tga.c LoadTGA. No engine source is imported.
    """
    assert data[:3] == b'\0\0\2' and data[16] == 32
    width, height = struct.unpack_from('<HH', data, 12)
    rows = [bytes(channel for i in range(18+y*width*4, 18+(y+1)*width*4, 4)
                  for channel in (data[i+2], data[i+1], data[i], data[i+3]))
            for y in range(height)]
    rows.reverse()
    if honors_top_origin and data[17] & 0x20:
        rows.reverse()
    return b''.join(rows)


class NavigationTGA(unittest.TestCase):
    def test_asymmetric_pixels_and_old_failure(self):
        pixels = bytes([255, 0, 0, 255, 0, 255, 0, 128,
                        0, 0, 255, 255, 20, 40, 60, 0])
        actual = nav.tga(2, pixels)
        self.assertEqual(actual[17], 8)
        self.assertEqual(decode(actual, False), pixels)
        self.assertEqual(decode(actual, True), pixels)
        # The former encoding was correct only in ETL, exactly reproducing the
        # reported Classic arrow reversal; this is not a renderer-angle issue.
        old = bytearray(actual); old[17] = 0x28
        old[18:34] = actual[26:34] + actual[18:26]
        self.assertEqual(decode(old, True), pixels)
        self.assertNotEqual(decode(old, False), pixels)

    def test_all_generated_art_and_manifest_pins(self):
        for generator, art in [(nav, nav.ART), (fireteam, fireteam.ART)]:
            manifest = json.loads((generator.DEST/'SOURCE.json').read_text())
            self.assertEqual(manifest['generator_sha256'], hashlib.sha256(Path(generator.__file__).read_bytes()).hexdigest())
            records = {entry['asset']: entry for entry in manifest['files']}
            for name, spec in art.items():
                size, draw = spec if isinstance(spec, tuple) else (64, spec)
                canvas = nav.Canvas(size); draw(canvas); expected = canvas.finish()
                data = (generator.DEST/(name+'.tga')).read_bytes()
                with self.subTest(asset=name):
                    self.assertEqual(data, nav.tga(size, expected))
                    self.assertEqual(decode(data, False), expected)
                    self.assertEqual(decode(data, True), expected)
                    self.assertEqual(records[name+'.tga']['sha256'], hashlib.sha256(data).hexdigest())
                    self.assertIn('bottom-left', records[name+'.tga']['channels'])

    def test_legacy_radar_assets_already_compatible(self):
        for name in ('radar_arrow.tga', 'radar_dot.tga'):
            data = (nav.ROOT/'src/assets/gfx/misc'/name).read_bytes()
            self.assertEqual(data[17] & 0x30, 0)
            self.assertEqual(decode(data, False), decode(data, True))


if __name__ == '__main__':
    unittest.main()
