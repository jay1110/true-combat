"""WORLD-only native-foot liquid exclusion controls, no engine required."""
import hashlib
from pathlib import Path
import struct
import re
import json
import subprocess
import sys
import tempfile
import unittest

from waygraph import BspLeafFilter, BspWorldLiquidFilter, SpatialGraph, validate_candidate_capacity, SPATIAL_NODE_LIMIT, SPATIAL_DIRECTED_LINK_LIMIT


class WorldLiquidTests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)

    def make(self, contents=0x20, world=True, bad_plane=False, water_top=100.):
        raw = bytearray(b"IBSP"+struct.pack("<i", 47)+bytes(17*8))
        planes = [(1., 0., 0., 0.), (1., 0., 0., 100.), (-1., 0., 0., 100.),
                  (0., 1., 0., 100.), (0., -1., 0., 100.), (0., 0., 1., water_top), (0., 0., -1., 100.)]
        leaves = b"".join(struct.pack("<12i", 0, 0, -1000, -1000, -1000, 1000, 1000, 1000, 0, 0, 0, 1) for _ in range(2))
        lumps = {1: struct.pack("<64sii", b"test_liquid", 0, contents),
                 2: b"".join(struct.pack("<4f", *p) for p in planes),
                 3: struct.pack("<9i", 0, -1, -2, -1000, -1000, -1000, 1000, 1000, 1000),
                 4: leaves, 6: struct.pack("<i", 0),
                 7: struct.pack("<6f4i", -1000, -1000, -1000, 1000, 1000, 1000, 0, 0, 0, int(world)),
                 8: struct.pack("<3i", 0, 6, 0),
                 9: b"".join(struct.pack("<2i", 99 if bad_plane else i, 0) for i in range(1, 7))}
        for i, data in lumps.items():
            struct.pack_into("<2i", raw, 8+i*8, len(raw), len(data));raw.extend(data)
        path = Path(self.folder.name)/"test.bsp";path.write_bytes(raw)
        tree = BspLeafFilter({"input": {"path": str(path), "bsp_sha256": hashlib.sha256(raw).hexdigest()}})
        return BspWorldLiquidFilter(tree)

    def test_deep_water_rejected_with_brush_ledger(self):
        filt = self.make()
        self.assertFalse(filt.allows([0, 0, 0], 2, "sample"))
        self.assertFalse(filt.allows([0, 0, 0], 2, "center"))
        report = filt.report()
        self.assertEqual(report["excluded_unique_positions"], 1)
        self.assertEqual(report["excluded_samples"][0]["kinds"], ["sample", "center"])
        self.assertEqual(report["excluded_samples"][0]["contents"], 0x20)
        self.assertEqual(report["excluded_samples"][0]["brush"], 0)
        self.assertEqual(len(report["bsp_sha256"]), 64)

    def test_water_boundary_and_projection_uncertainty_retained(self):
        filt = self.make()
        for p in ([100, 0, 0], [99.9, 0, 0], [0, 0, 62], [0, 0, 80], [0, 0, 99.9], [0, 0, -80]):
            self.assertTrue(filt.allows(p), p)
        self.assertFalse(filt.allows([0, 0, 38]))

    def test_higher_native_reprojection_can_make_water_wadeable(self):
        filt = self.make(water_top=112.)
        # Raw90's old range70..110 was wholly wet. Native feet106 and its
        # dry probe124 make this only6 units of water, which is supported.
        self.assertTrue(filt.allows([0, 0, 90]))
        self.assertTrue(filt.allows([0, 0, 51]))  # highest middle sample112 at the boundary
        self.assertFalse(filt.allows([0, 0, 50])) # every possible middle sample wet
        report = filt.report()
        self.assertEqual(report['water_exclusion_vertical_range'], [-20., 61.])
        self.assertEqual(report['hazard_exclusion_vertical_range'], [-20., 20.])

    def test_posture_middle_sample_keeps_legitimate32_unit_wading(self):
        filt=self.make(water_top=32.)
        # Even without any native floor reprojection, a standing enlarged
        # TC body samples middle at41. The previous18-unit veto was wrong.
        self.assertTrue(filt.allows([0,0,0]))
        filt=self.make(water_top=60.)
        # Native upward reprojection20 could still make the sample dry.
        self.assertTrue(filt.allows([0,0,0]))
        filt=self.make(water_top=62.)
        self.assertFalse(filt.allows([0,0,0]))

    def test_hazards_and_mixed_water_hazards_keep_original_range(self):
        for contents in (0x8, 0x10, 0x28, 0x30):
            filt = self.make(contents, water_top=112.)
            self.assertFalse(filt.allows([0, 0, 90]))
            self.assertTrue(filt.allows([0, 0, 92]))

    def test_dry_narrow_ledge_above_or_beside_water_retained(self):
        filt = self.make()
        for p in ([0, 0, 101], [101, 0, 0], [-101, 0, 0]):
            self.assertTrue(filt.allows(p), p)

    def test_lava_and_slime_share_native_hazard_mask(self):
        for contents in (0x8, 0x10):
            self.assertFalse(self.make(contents).allows([0, 0, 0]))

    def test_solid_does_not_become_liquid_exclusion(self):
        self.assertTrue(self.make(1).allows([0, 0, 0]))

    def test_inline_liquid_is_not_treated_as_world_brush(self):
        filt = self.make(world=False)
        self.assertTrue(filt.allows([0, 0, 0]))
        self.assertEqual(filt.report()["world_liquid_brushes"], 0)

    def test_bad_brush_plane_rejected(self):
        with self.assertRaisesRegex(ValueError, "planes"):
            self.make(bad_plane=True)

    def test_no_provenance_retains_samples(self):
        filt = BspWorldLiquidFilter(BspLeafFilter({}))
        self.assertTrue(filt.allows([0, 0, 0]))
        self.assertFalse(filt.enabled)

    def test_mask_and_projection_agree_with_native_contract(self):
        flags = (Path(__file__).resolve().parents[2]/"src/game/surfaceflags.h").read_text()
        values = [int(re.search(r"#define\s+"+name+r"\s+(0x[0-9a-fA-F]+)", flags)[1], 16)
                  for name in ("CONTENTS_LAVA", "CONTENTS_SLIME", "CONTENTS_WATER")]
        self.assertEqual(BspWorldLiquidFilter.CONTENTS_MASK, values[0] | values[1] | values[2])
        self.assertGreater(BspWorldLiquidFilter.PROJECTION, 18.5+.125)
        native = (Path(__file__).resolve().parents[2]/'src/game/tce_navgen.c').read_text()
        self.assertIn('TCE_NavWaterMiddleSample(ngOrigin==30.f,height<=ngDuck)',native)
        # The broad-phase veto must use the maximum supported standing
        # middle sample. Per-posture acceptance remains an actual native test.
        self.assertGreaterEqual(BspWorldLiquidFilter.WADE_DRY_PROBE,41.)

    def test_native_capacity_and_legacy_boundaries(self):
        source = (Path(__file__).resolve().parents[2]/"src/game/tce_navgen.c").read_text()
        native = int(re.search(r"#define\s+NG_NODES\s+(\d+)", source)[1])
        self.assertEqual(SPATIAL_NODE_LIMIT, native-512)
        self.assertEqual(SPATIAL_DIRECTED_LINK_LIMIT, native*16-8192)
        validate_candidate_capacity(15872, 253952, True)
        validate_candidate_capacity(4096, 65536, False)
        for nodes, links, spatial in ((15873, 0, True), (1, 253953, True),
                                      (4097, 0, False), (1, 65537, False), (0, 0, True)):
            with self.assertRaisesRegex(ValueError, "capacity"):
                validate_candidate_capacity(nodes, links, spatial)

    def test_add_preserves_repair_reserve(self):
        graph = object.__new__(SpatialGraph)
        graph.nodes = [None]*SPATIAL_NODE_LIMIT
        with self.assertRaisesRegex(ValueError, "15872"):
            graph.add({"point": [0, 0, 0]})

    def test_export_hash_is_exact_written_bytes_and_header_counts(self):
        filt = self.make();root = Path(self.folder.name)
        bsp = root/"test.bsp";geo = root/"geometry.json";mesh = root/"mesh.json";out = root/"candidate.txt"
        geo.write_text(json.dumps({"map": "test", "spawns": [], "input": {
            "path": str(bsp), "bsp_sha256": hashlib.sha256(bsp.read_bytes()).hexdigest()}}))
        mesh.write_text(json.dumps({"coordinate_system": "TCE_Z_up", "profile": {"radius": 16, "height": 87, "step": 18},
            "vertices": [[200, 0, 0], [456, 0, 0], [456, 256, 0], [200, 256, 0]],
            "polygons": [{"vertices": [0, 1, 2, 3], "neighbors": [-1]*4}]}))
        duck = root/"duck.json"
        duck_data = json.loads(mesh.read_text())
        duck_data["profile"]["height"] = 61
        duck.write_text(json.dumps(duck_data))
        result = subprocess.run([sys.executable, str(Path(__file__).with_name("waygraph.py")),
            "--stand", str(mesh), "--duck", str(duck), "--geometry", str(geo), "--out", str(out)],
            capture_output=True, text=True)
        self.assertEqual(result.returncode, 0, result.stderr)
        data = out.read_bytes();report = json.loads(out.with_suffix(".json").read_text())
        self.assertEqual(hashlib.sha256(data).hexdigest(), report["candidate_sha256"])
        self.assertNotIn(b"\r\n", data)
        header = data.splitlines()[0].split()
        self.assertEqual(int(header[3]), report["counts"]["nodes"])
        self.assertEqual(int(header[4]), report["counts"]["directed_edges"])


if __name__ == "__main__":
    unittest.main()
