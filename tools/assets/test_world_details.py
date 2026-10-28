"""Finite native detail material/asset contracts; no engine/game execution."""
import hashlib
import json
from pathlib import Path
import re
import struct
import unittest
import generate_world_details as g


class WorldDetailsTests(unittest.TestCase):
    def test_reproducible_assets_and_neutral_blend(self):
        source = json.loads((g.DEST / "SOURCE.json").read_text())
        self.assertEqual(source["generator_sha256"], hashlib.sha256(Path(g.__file__).read_bytes()).hexdigest())
        for row in source["files"]:
            with self.subTest(asset=row["asset"]):
                data = (g.DEST / row["asset"]).read_bytes()
                self.assertEqual(hashlib.sha256(data).hexdigest(), row["sha256"])
                self.assertEqual(struct.unpack_from("<HHBB", data, 12), (128, 128, 32, 0x28))
                pixels = data[18:18 + 128 * 128 * 4]
                self.assertEqual(len(pixels), 65536)
                self.assertEqual(set(pixels[3::4]), {255})
                self.assertEqual(pixels[0::4], pixels[1::4])
                self.assertEqual(pixels[1::4], pixels[2::4])
                self.assertEqual(sum(pixels[0::4]) / 16384, 127.5)
                self.assertTrue(80 <= min(pixels[0::4]) < max(pixels[0::4]) <= 175)
                # Constant128 destination remains127.5 on average; no gamma preset.
                self.assertAlmostEqual(sum(2 * v * 128 / 255 for v in pixels[0::4]) / 16384, 128, places=9)

    def test_continuous_tile_seams(self):
        for kind in g.KINDS:
            for x, y in ((0, 0), (.125, 127.875), (54.75, 20.25), (-1.5, 99.25)):
                with self.subTest(kind=kind, x=x, y=y):
                    self.assertAlmostEqual(g.sample(kind, x, y), g.sample(kind, x + 128, y), places=8)
                    self.assertAlmostEqual(g.sample(kind, x, y), g.sample(kind, x, y + 128), places=8)

    def test_explicit_dynamic_materials_and_existing_base_stages(self):
        source = json.loads((g.DEST / "SOURCE.json").read_text())
        self.assertEqual(len(source["materials"]), 5)
        self.assertFalse((g.ROOT / "src/assets/scripts/tce2_materials.shader").exists())
        for material, info in zip(g.MATERIALS, source["materials"]):
            original, name, diffuse, kind, scale, surface = material
            path = g.MATERIAL_DEST / (name + ".shader")
            with self.subTest(material=name):
                shader = path.read_text()
                self.assertEqual(shader, g.shader_text(material))
                self.assertEqual(info["original_name"], original)
                self.assertTrue(info["path"].startswith("materials/tce2/world/"))
                self.assertEqual(info["sha256"], hashlib.sha256(path.read_bytes()).hexdigest())
                self.assertEqual(info["bytes"], len(path.read_bytes()))
                self.assertRegex(shader, r"(?m)^" + re.escape(original) + r"\s*$")
                self.assertEqual(shader.count("map $lightmap"), 1)
                self.assertEqual(shader.count("blendFunc GL_DST_COLOR GL_SRC_COLOR"), 1)
                self.assertEqual(shader.count("blendFunc GL_DST_COLOR GL_ZERO"), 1)
                self.assertEqual(shader.count("\n\t\tdetail\n"), 1)
                self.assertNotRegex(shader, r"(?im)^\s*(?:normalMap|specularMap|bumpMap|reflectionMap)\b")
                self.assertNotIn("surfaceparm trans", shader)
                self.assertNotIn("surfaceparm nolightmap", shader)
                self.assertIn("map " + diffuse, shader)
                self.assertIn("map textures/tce2/details/" + kind + ".tga", shader)
                self.assertEqual(shader.count("surfaceparm"), int(bool(surface)))
                self.assertLess(len(path.read_bytes()), 2048)


if __name__ == "__main__":
    unittest.main()
