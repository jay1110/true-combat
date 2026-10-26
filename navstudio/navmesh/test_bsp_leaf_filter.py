"""Offline negative-filter controls: valid leaves and crouch space are retained."""
import hashlib
import json
from pathlib import Path
import struct
import tempfile
import unittest
import zipfile

from waygraph import BspLeafFilter, Mesh, build


class LeafFilterTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def geometry(self, plane=(1., 0., 0., 0.), clusters=(0, -1), children=(-1, -2), zipped=False):
        data = bytearray(b"IBSP" + struct.pack("<i", 47) + bytes(17*8))
        lumps = {2: struct.pack("<4f", *plane),
                 3: struct.pack("<9i", 0, *children, -1000, -1000, -1000, 1000, 1000, 1000),
                 4: b"".join(struct.pack("<12i", cluster, 0, -1000, -1000, -1000,
                                        1000, 1000, 1000, 0, 0, 0, 0) for cluster in clusters)}
        for index, content in lumps.items():
            struct.pack_into("<ii", data, 8+index*8, len(data), len(content))
            data.extend(content)
        path = self.root / ("map.pk3" if zipped else "map.bsp")
        if zipped:
            with zipfile.ZipFile(path, "w") as archive:
                archive.writestr("maps/sample.bsp", data)
        else:
            path.write_bytes(data)
        source = {"path": str(path), "bsp_sha256": hashlib.sha256(data).hexdigest()}
        if zipped:
            source["archive_member"] = "maps/sample.bsp"
        return {"map": "sample", "input": source, "spawns": []}

    def test_reject_outside_and_retain_inside_or_boundary(self):
        filt = BspLeafFilter(self.geometry(zipped=True))
        self.assertFalse(filt.allows([-20, 0, 0], 0, "lattice"))
        self.assertTrue(filt.allows([20, 0, 0], 1, "lattice"))
        self.assertTrue(filt.allows([0, 0, 0], 1, "lattice"))
        self.assertEqual(filt.report()["excluded_unique_positions"], 1)

    def test_any_crouch_body_sample_in_valid_leaf_retains_point(self):
        filt = BspLeafFilter(self.geometry(plane=(0., 0., 1., 40.)))
        self.assertTrue(filt.allows([0, 0, 0]))  # z+48 is valid, z+8/+24 are outside.
        self.assertFalse(filt.allows([0, 0, -20]))
        self.assertLess(max(filt.OFFSETS), 61)

    def test_no_cluster_bsp_disables_exclusion(self):
        filt = BspLeafFilter(self.geometry(clusters=(-1, -1)))
        self.assertFalse(filt.enabled)
        self.assertEqual(filt.disabled_reason, "no_usable_visibility_clusters")
        self.assertTrue(filt.allows([-20, 0, 0]))

    def test_missing_provenance_retains_every_sample(self):
        filt = BspLeafFilter({})
        self.assertFalse(filt.enabled)
        self.assertTrue(filt.allows([-999, 0, 0]))

    def test_hash_mismatch_fails_instead_of_filtering_different_bsp(self):
        geometry = self.geometry()
        geometry["input"]["bsp_sha256"] = "0"*64
        with self.assertRaisesRegex(ValueError, "hash differs"):
            BspLeafFilter(geometry)

    def test_invalid_child_fails(self):
        with self.assertRaisesRegex(ValueError, "child index"):
            BspLeafFilter(self.geometry(children=(-1, -3)))

    def test_cyclic_tree_fails_bounded_lookup(self):
        filt = BspLeafFilter(self.geometry(children=(-1, 0)))
        with self.assertRaisesRegex(ValueError, "cyclic"):
            filt.allows([-20, 0, 0])

    def test_build_records_excluded_centers_without_losing_valid_surface(self):
        geometry = self.geometry()
        data = {"coordinate_system": "TCE_Z_up", "profile": {"radius": 16, "height": 61, "step": 18}, "vertices": [], "polygons": []}
        for origin in (-320, 192):
            first = len(data["vertices"])
            data["vertices"] += [[origin, 0, 0], [origin+128, 0, 0], [origin+128, 128, 0], [origin, 128, 0]]
            data["polygons"].append({"vertices": list(range(first, first+4)), "neighbors": [-1]*4})
        path = self.root / "mesh.json"
        path.write_text(json.dumps(data))
        mesh = Mesh(path)
        stand = Mesh(path)
        stand.profile = dict(mesh.profile, height=87)
        nodes, edges, report = build(stand, mesh, geometry, 320, 128)
        self.assertTrue(nodes)
        self.assertTrue(all(node["point"][0] >= 0 for node in nodes))
        self.assertEqual(report["coverage"]["excluded_outside_bsp_polygon_centers"], [0])
        self.assertEqual(report["coverage"]["witness_count"], 1)
        self.assertEqual(report["coverage"]["unrepresented_polygons"], [])
        self.assertTrue(report["coverage"]["surface_witnesses"])
        self.assertTrue(report["bsp_leaf_filter"]["excluded_samples"])


if __name__ == "__main__":
    unittest.main()
